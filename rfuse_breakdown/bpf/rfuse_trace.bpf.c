// bpf/rfuse_trace.bpf.c

#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_endian.h>

#include "rfuse_common.h"   // rfuse_req, rfuse_iqueue, rfuse_req_key, rfuse_req_state, rfuse_req_event

char LICENSE[] SEC("license") = "GPL";

/* ----------------------------------------------------
 *  공용 map 정의
 * -------------------------------------------------- */

// 요청 상태 (key = (riq_id, unique))
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 65536);
    __type(key, struct rfuse_req_key);
    __type(value, struct rfuse_req_state);
} rfuse_states SEC(".maps");

// 완료 이벤트 ringbuf
struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 1 << 24); // 16MB
} rfuse_events SEC(".maps");

// [추가] PID별 Alloc 시작 시간 저장 (Alloc & Block 측정용)
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 10240);
    __type(key, __u32); // PID
    __type(value, __u64); // Timestamp
} pid_alloc_map SEC(".maps");

/* ----------------------------------------------------
 *  공용 헬퍼
 * -------------------------------------------------- */

static __always_inline __u64 get_tid(void)
{
    return bpf_get_current_pid_tgid();
}

static __always_inline struct rfuse_req_state *
get_or_init_state(const struct rfuse_req_key *key)
{
    struct rfuse_req_state *st;

    st = bpf_map_lookup_elem(&rfuse_states, key);
    if (st)
        return st;

    struct rfuse_req_state zero = {};
    bpf_map_update_elem(&rfuse_states, key, &zero, BPF_ANY);
    return bpf_map_lookup_elem(&rfuse_states, key);
}

/* ----------------------------------------------------
 * 추가. Alloc Start (rfuse_get_req)
 * -------------------------------------------------- */

/* * struct rfuse_req *rfuse_get_req(struct fuse_mount *fm, ...)
 * - 이 함수 진입 시점이 요청 할당 및 블로킹 시작점입니다.
 * - 아직 req가 없으므로 PID를 키로 사용합니다.
 */
SEC("kprobe/rfuse_get_req")
int kp_rfuse_get_req(struct pt_regs *ctx)
{
    __u32 pid = bpf_get_current_pid_tgid() >> 32;
    __u64 ts = bpf_ktime_get_ns();
    bpf_map_update_elem(&pid_alloc_map, &pid, &ts, BPF_ANY);
    return 0;
}

/* ====================================================
 *  1) kernel: rfuse_queue_request(struct rfuse_req *r_req)
 *     - queuing point (커널에서 pending queue에 넣는 시점)
 *     - key = (riq_id, in.unique)
 * ==================================================== */

/* 새 버전: rfuse_submit_request(opcode, unique, riq_id, ts) 래퍼에 kprobe */
SEC("kprobe/rfuse_submit_request")
int kp_rfuse_submit_request(struct pt_regs *ctx)
{
    __u32 opcode     = (__u32)PT_REGS_PARM1(ctx);
    __u64 unique     = (__u64)PT_REGS_PARM2(ctx);
    int   riq_id     = (int)PT_REGS_PARM3(ctx);

    struct rfuse_req_key key = {};
    struct rfuse_req_state *st;
    __u64 *ts;
    __u32 pid = bpf_get_current_pid_tgid() >> 32;

    key.riq_id = riq_id;
    key.unique = unique;

    st = get_or_init_state(&key);
    if (!st)
        return 0;
    /* queue 시작 타임스탬프: 커널에서 넘긴 ts를 쓰거나, bpf_ktime_get_ns() 써도 됨 */
    if (!st->ts_queued_ns)
        st->ts_queued_ns = bpf_ktime_get_ns();         /* 또는 bpf_ktime_get_ns(); */

    st->opcode = opcode;
    st->unique = unique;
    ts = bpf_map_lookup_elem(&pid_alloc_map, &pid);
    if (ts) {
        if (st->ts_queued_ns > *ts) {
            st->alloc_delay_ns = st->ts_queued_ns - *ts;
        }
        bpf_map_delete_elem(&pid_alloc_map, &pid);
    } else {
        st->alloc_delay_ns = 0;
    }

    return 0;
}

/* ====================================================
 *  2) user: rfuse_read_request(...)
 *
 *  __attribute__((noinline))
 *  void rfuse_read_request(unsigned int opcode,
 *                          unsigned long long unique,
 *                          int riq_id,
 *                          unsigned long long ts);
 *
 *  - dequeue 시점 (유저가 ring channel에서 요청 꺼내 처리 시작)
 *  - key = (riq_id, unique)
 * ==================================================== */

SEC("uprobe")
int up_rfuse_read_request(struct pt_regs *ctx)
{
    __u32 opcode = (__u32)PT_REGS_PARM1(ctx);
    __u64 unique = (__u64)PT_REGS_PARM2(ctx);
    int   riq_id = (int)PT_REGS_PARM3(ctx);
    // __u64 ts  = (__u64)PT_REGS_PARM4(ctx); // 필요하면 사용

    struct rfuse_req_key key = {};
    struct rfuse_req_state *st;

    key.riq_id = riq_id;
    key.unique = unique;

    st = get_or_init_state(&key);
    if (!st)
        return 0;

    if (!st->ts_dequeued_ns)
        st->ts_dequeued_ns = bpf_ktime_get_ns();
    // opcode/unique 재확인
    st->opcode = opcode;
    st->unique = unique;

    return 0;
}

/* ====================================================
 *  3) user: payload copy latency
 *
 *  __attribute__((noinline))
 *  void rfuse_copy_from_payload_begin_end(unsigned int opcode,
 *                                         unsigned long long unique,
 *                                         int riq_id,
 *                                         unsigned long long latency_ns);
 *
 *  __attribute__((noinline))
 *  void rfuse_copy_to_payload_begin_end(unsigned int opcode,
 *                                       unsigned long long unique,
 *                                       int riq_id,
 *                                       unsigned long long latency_ns);
 *
 *  - key = (riq_id, unique)
 * ==================================================== */

// from kernel → user copy latency (user가 pread() 후 payload 복사)
SEC("uprobe")
int up_rfuse_copy_from_payload_begin_end(struct pt_regs *ctx)
{
    __u32 opcode      = (__u32)PT_REGS_PARM1(ctx);
    __u64 unique      = (__u64)PT_REGS_PARM2(ctx);
    int   riq_id      = (int)PT_REGS_PARM3(ctx);
    __u64 latency_ns  = (__u64)PT_REGS_PARM4(ctx);

    struct rfuse_req_key key = {};
    struct rfuse_req_state *st;

    key.riq_id = riq_id;
    key.unique = unique;

    st = get_or_init_state(&key);
    if (!st)
        return 0;

    st->copy_from_latency_ns = latency_ns;
    st->opcode = opcode;
    st->unique = unique;

    return 0;
}

// from user → kernel copy latency (fuse_reply_buf() 에서 pwrite)
SEC("uprobe")
int up_rfuse_copy_to_payload_begin_end(struct pt_regs *ctx)
{
    __u32 opcode      = (__u32)PT_REGS_PARM1(ctx);
    __u64 unique      = (__u64)PT_REGS_PARM2(ctx);
    int   riq_id      = (int)PT_REGS_PARM3(ctx);
    __u64 latency_ns  = (__u64)PT_REGS_PARM4(ctx);

    struct rfuse_req_key key = {};
    struct rfuse_req_state *st;

    key.riq_id = riq_id;
    key.unique = unique;

    st = get_or_init_state(&key);
    if (!st)
        return 0;

    st->copy_to_latency_ns = latency_ns;
    st->opcode = opcode;
    st->unique = unique;

    return 0;
}

/* ====================================================
 *  4) user: rfuse_send_result(...)
 *
 *  __attribute__((noinline))
 *  void rfuse_send_result(unsigned int opcode,
 *                          unsigned long long unique,
 *                          int riq_id,
 *                          unsigned long long ts);
 *
 *  - daemon processing 끝 + response 시작 시점
 *  - key = (riq_id, unique)
 * ==================================================== */

SEC("uprobe")
int up_rfuse_send_result(struct pt_regs *ctx)
{
    __u32 opcode = (__u32)PT_REGS_PARM1(ctx);
    __u64 unique = (__u64)PT_REGS_PARM2(ctx);
    int   riq_id = (int)PT_REGS_PARM3(ctx);
    // __u64 ts  = (__u64)PT_REGS_PARM4(ctx); // 필요하면 사용

    struct rfuse_req_key key = {};
    struct rfuse_req_state *st;

    key.riq_id = riq_id;
    key.unique = unique;

    st = get_or_init_state(&key);
    if (!st)
        return 0;

    st->ts_daemon_done_ns = bpf_ktime_get_ns();
    st->opcode = opcode;
    st->unique = unique;

    return 0;
}

/* ====================================================
 *  5) kernel: rfuse_request_end(struct rfuse_req *r_req)
 *
 *  - 전체 요청 수명 종료
 *  - key = (riq_id, in.unique)
 *  - 여기서 queue/daemon/response + copy latency 묶어서 이벤트로 내보냄
 * ==================================================== */

SEC("kprobe/rfuse_request_end")
int kp_rfuse_request_end(struct pt_regs *ctx)
{
    struct rfuse_req *r_req = (void *)PT_REGS_PARM1(ctx);
    struct rfuse_req_key key = {};
    struct rfuse_req_state *st;
    struct rfuse_req_event *ev;
    __u32 riq_id = 0;
    __u64 unique = 0;
    __u64 now = bpf_ktime_get_ns();
    __u64 queue_delay = 0, daemon_delay = 0, resp_delay = 0;
    __u64 pid_tgid = bpf_get_current_pid_tgid();
    __u32 pid = pid_tgid >> 32;

    if (!r_req)
        return 0;

    bpf_probe_read_kernel(&riq_id, sizeof(riq_id), &r_req->riq_id);
    bpf_probe_read_kernel(&unique, sizeof(unique), &r_req->in.unique);

    key.riq_id = (int)riq_id;
    key.unique = unique;

    st = bpf_map_lookup_elem(&rfuse_states, &key);
    if (!st)
        return 0;
    st->ts_end_ns = now;
    if (st->ts_queued_ns && st->ts_dequeued_ns)
        queue_delay = st->ts_dequeued_ns - st->ts_queued_ns;

    if (st->ts_dequeued_ns && st->ts_daemon_done_ns)
        daemon_delay = st->ts_daemon_done_ns - st->ts_dequeued_ns;

    if (st->ts_daemon_done_ns && st->ts_end_ns)
        resp_delay = st->ts_end_ns - st->ts_daemon_done_ns;

    ev = bpf_ringbuf_reserve(&rfuse_events, sizeof(*ev), 0);
    if (!ev)
        goto out;

    ev->ts_ns  = now;
    ev->riq_id = key.riq_id;
    ev->req_index = 0;      // 이제 의미 없음, 그냥 0
    ev->unique = st->unique;
    ev->opcode = st->opcode;
    ev->pid    = pid;
    bpf_get_current_comm(ev->comm, sizeof(ev->comm));
    ev->alloc_delay_ns      = st->alloc_delay_ns;
    ev->queue_delay_ns      = queue_delay;
    ev->daemon_delay_ns     = daemon_delay;
    ev->response_delay_ns   = resp_delay;
    ev->copy_from_latency_ns = st->copy_from_latency_ns;
    ev->copy_to_latency_ns   = st->copy_to_latency_ns;

    bpf_ringbuf_submit(ev, 0);

out:
    bpf_map_delete_elem(&rfuse_states, &key);
    return 0;
}

