// bpf/fuse_trace.bpf.c

#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

#include "fuse_trace_common.h"

char LICENSE[] SEC("license") = "GPL";

/* =========================
 * Config
 * =========================
 * DAEMON_COMM은 빌드 시 -DDAEMON_COMM="\"StackFS_ll\"" 형태로 바꿀 수 있음.
 */
#ifndef DAEMON_COMM
#define DAEMON_COMM "StackFS_ll"
#endif
const volatile char g_daemon_comm[TASK_COMM_LEN] SEC(".rodata") = DAEMON_COMM;

static __always_inline int comm_eq_16(const char comm[TASK_COMM_LEN])
{
    /* "StackFS_ll" (길이 9) + '\0' */
    if (comm[0] != 'S') return 0;
    if (comm[1] != 't') return 0;
    if (comm[2] != 'a') return 0;
    if (comm[3] != 'c') return 0;
    if (comm[4] != 'k') return 0;
    if (comm[5] != 'F') return 0;
    if (comm[6] != 'S') return 0;
    if (comm[7] != '_') return 0;
    if (comm[8] != 'l') return 0;
    if (comm[9] != 'l') return 0;

    /* 정확히 같은 comm만 받으려면 여기서 NUL 종료 확인 */
    if (comm[10] != '\0') return 0;

    return 1;
}

/* =========================
 * tracepoint ctx (minimal)
 *  - vmlinux.h에 trace_event_raw_* 정의가 없을 때를 대비해 직접 정의
 * ========================= */
struct tp_sched_wakeup_args {
    __u16 common_type;
    __u8  common_flags;
    __u8  common_preempt_count;
    __s32 common_pid;

    char  comm[TASK_COMM_LEN];
    __s32 pid;
    __s32 prio;
    __s32 success;
};

struct tp_sched_switch_args {
    __u16 common_type;
    __u8  common_flags;
    __u8  common_preempt_count;
    __s32 common_pid;

    char  prev_comm[TASK_COMM_LEN];
    __s32 prev_pid;
    __s32 prev_prio;
    __s64 prev_state;

    char  next_comm[TASK_COMM_LEN];
    __s32 next_pid;
    __s32 next_prio;
};

/* =========================
 * ringbuf (최종 1회 emit)
 * ========================= */
struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 1 << 24);
} events SEC(".maps");

/* =========================
 * per-unique state (누적)
 *  - FIX: LRU_HASH 사용 안함(측정 state eviction 방지)
 * ========================= */
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 262144);
    __type(key, __u64);                 /* unique */
    __type(value, struct fuse_req_state);
} req_state SEC(".maps");

/* =========================
 * Scheduler tracking (daemon only, comm filter)
 *  - wakeup_ts[tid]  : sched_wakeup 시각
 *  - wake2run_ns[tid]: wakeup -> switch-in
 * ========================= */
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 131072);
    __type(key, __u32);   /* tid */
    __type(value, __u64); /* ts_ns */
} wakeup_ts SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 131072);
    __type(key, __u32);   /* tid */
    __type(value, __u64); /* delta_ns */
} wake2run_ns SEC(".maps");

/* =========================
 * Helpers
 * ========================= */
static __always_inline struct fuse_req_state *get_or_init_state(__u64 unique)
{
    struct fuse_req_state *st = bpf_map_lookup_elem(&req_state, &unique);
    if (st)
        return st;

    struct fuse_req_state init = {};
    init.unique = unique;
    bpf_map_update_elem(&req_state, &unique, &init, BPF_ANY);
    return bpf_map_lookup_elem(&req_state, &unique);
}

static __always_inline void emit_req_end_and_cleanup(__u64 unique,
                                                     __u32 opcode,
                                                     __s32 err,
                                                     __u64 ts_end_ns)
{
    struct fuse_req_state *st = bpf_map_lookup_elem(&req_state, &unique);
    if (!st)
        return;

    /* end 정보 갱신 */
    if (opcode)
        st->opcode = opcode;
    st->err       = err;
    st->ts_end_ns = ts_end_ns;
    st->flags    |= FUSE_F_SEEN_END;

    struct fuse_req_event_v1 *e =
        bpf_ringbuf_reserve(&events, sizeof(*e), 0);

    if (e) {
        __builtin_memset(e, 0, sizeof(*e));

        e->unique = st->unique;
        e->opcode = st->opcode;
        e->err    = st->err;

        e->d_tgid = st->d_tgid;
        e->d_tid  = st->d_tid;
        e->k_tid  = st->k_tid;
        e->flags  = st->flags;

        e->d_cpu  = st->d_cpu;
        e->k_cpu  = st->k_cpu;

        e->ts_queue_ns = st->ts_queue_ns;
        e->ts_recv_ns  = st->ts_recv_ns;
        e->ts_send_ns  = st->ts_send_ns;
        e->ts_end_ns   = st->ts_end_ns;

        /* derived */
        if ((st->flags & FUSE_F_SEEN_QUEUE) && (st->flags & FUSE_F_SEEN_RECV) &&
            st->ts_recv_ns >= st->ts_queue_ns) {
            e->queuing_ns = st->ts_recv_ns - st->ts_queue_ns;
        }

        e->sched_delay_ns = st->sched_delay_ns;

        if ((st->flags & FUSE_F_SEEN_RECV) && (st->flags & FUSE_F_SEEN_SEND) &&
            st->ts_send_ns >= st->ts_recv_ns) {
            e->daemon_ns = st->ts_send_ns - st->ts_recv_ns;
        }

        if ((st->flags & FUSE_F_SEEN_SEND) && (st->flags & FUSE_F_SEEN_END) &&
            st->ts_end_ns >= st->ts_send_ns) {
            e->response_ns = st->ts_end_ns - st->ts_send_ns;
        }

        __builtin_memcpy(e->d_comm, st->d_comm, TASK_COMM_LEN);
        __builtin_memcpy(e->k_comm, st->k_comm, TASK_COMM_LEN);

        bpf_ringbuf_submit(e, 0);
    }

    /* 완료 시 state 정리(누적 누수 방지) */
    bpf_map_delete_elem(&req_state, &unique);
}

/* =========================
 * Kernel probes
 * ========================= */

/* noinline void trace_fuse_queue_request(unsigned int opcode, u64 unique, u64 ts) */
SEC("kprobe/trace_fuse_queue_request")
int BPF_KPROBE(kp_trace_fuse_queue_request,
               unsigned int opcode, u64 unique, u64 ts)
{
    __u64 now = bpf_ktime_get_ns();
    __u32 pid_tgid = (__u32)bpf_get_current_pid_tgid();
    __u32 cpu = bpf_get_smp_processor_id();

    struct fuse_req_state *st = get_or_init_state((__u64)unique);
    if (!st)
        return 0;

    st->opcode = opcode;
    st->k_tid  = pid_tgid;
    st->k_cpu  = cpu;
    st->ts_queue_ns = now;
    st->flags |= FUSE_F_SEEN_QUEUE;
    bpf_get_current_comm(&st->k_comm, sizeof(st->k_comm));

    return 0;
}

/* noinline void trace_fuse_request_end(unsigned int opcode, u64 unique, u64 ts, int err) */
SEC("kprobe/trace_fuse_request_end")
int BPF_KPROBE(kp_trace_fuse_request_end,
               unsigned int opcode, u64 unique, u64 ts, int err)
{
    __u64 now = bpf_ktime_get_ns();
    emit_req_end_and_cleanup((__u64)unique, (__u32)opcode, (__s32)err, now);
    return 0;
}

/* =========================
 * User-space uprobes
 * ========================= */

/* 유저: void receive_buf(unsigned int opcode, unsigned long long unique, struct timespec ts) */
SEC("uprobe/receive_buf")
int BPF_KPROBE(up_receive_buf,
               unsigned int opcode,
               unsigned long long unique,
               void *ts_unused)
{
    __u64 now = bpf_ktime_get_ns();
    __u64 u   = (__u64)unique;

    __u64 pid_tgid = bpf_get_current_pid_tgid();
    __u32 tid  = (__u32)pid_tgid;
    __u32 tgid = (__u32)(pid_tgid >> 32);
    __u32 cpu  = bpf_get_smp_processor_id();

    struct fuse_req_state *st = get_or_init_state(u);
    if (!st)
        return 0;

    /* daemon 정보 갱신 */
    if (!st->opcode)
        st->opcode = opcode;

    st->d_tgid = tgid;
    st->d_tid  = tid;
    st->d_cpu  = cpu;
    st->ts_recv_ns = now;
    st->flags |= FUSE_F_SEEN_RECV;
    bpf_get_current_comm(&st->d_comm, sizeof(st->d_comm));

    /* daemon wake2run (sched delay) 첨부: one-shot 소비 */
    __u64 *dp = bpf_map_lookup_elem(&wake2run_ns, &tid);
    if (dp) {
        st->sched_delay_ns = *dp;
        st->flags |= FUSE_F_SEEN_SCHED;
        bpf_map_delete_elem(&wake2run_ns, &tid);
    }

    return 0;
}

/* libfuse: static int fuse_send_msg(..., struct iovec *iov, int count)
 *  - iov[0].iov_base -> struct fuse_out_header*
 */
struct fuse_out_header {
    __u32 len;
    __s32 error;
    __u64 unique;
};

struct u_iovec {
    void *iov_base;
    __u64 iov_len;
};

SEC("uprobe/fuse_send_msg")
int BPF_KPROBE(up_fuse_send_msg,
               void *se, void *ch, struct u_iovec *iov, int count)
{
    if (count <= 0 || !iov)
        return 0;

    struct u_iovec iov0 = {};
    if (bpf_probe_read_user(&iov0, sizeof(iov0), &iov[0]) < 0)
        return 0;

    void *outp = iov0.iov_base;
    if (!outp)
        return 0;

    struct fuse_out_header hdr = {};
    if (bpf_probe_read_user(&hdr, sizeof(hdr), outp) < 0)
        return 0;

    __u64 now = bpf_ktime_get_ns();
    __u64 u = hdr.unique;

    struct fuse_req_state *st = get_or_init_state(u);
    if (!st)
        return 0;

    st->ts_send_ns = now;
    st->flags |= FUSE_F_SEEN_SEND;

    /* end가 아직이면 임시로 error 기록(최종은 request_end 우선) */
    if (!(st->flags & FUSE_F_SEEN_END))
        st->err = hdr.error;

    return 0;
}

/* =========================
 * Scheduler tracepoints
 *  - FIX: comm 필터를 wakeup에서만 적용(저장 기준)
 *  - switch에서는 wakeup_ts 존재 여부로만 처리 (이미 필터된 tid만 비용 발생)
 * ========================= */
SEC("tracepoint/sched/sched_wakeup")
int tp_sched_wakeup(struct tp_sched_wakeup_args *ctx)
{
    if (!comm_eq_16(ctx->comm))
        return 0;

    __u32 tid = (__u32)ctx->pid;
    __u64 now = bpf_ktime_get_ns();
    bpf_map_update_elem(&wakeup_ts, &tid, &now, BPF_ANY);
    return 0;
}

SEC("tracepoint/sched/sched_wakeup_new")
int tp_sched_wakeup_new(struct tp_sched_wakeup_args *ctx)
{
    if (!comm_eq_16(ctx->comm))
        return 0;

    __u32 tid = (__u32)ctx->pid;
    __u64 now = bpf_ktime_get_ns();
    bpf_map_update_elem(&wakeup_ts, &tid, &now, BPF_ANY);
    return 0;
}

SEC("tracepoint/sched/sched_switch")
int tp_sched_switch(struct tp_sched_switch_args *ctx)
{
    __u32 next = (__u32)ctx->next_pid;

    __u64 *tsp = bpf_map_lookup_elem(&wakeup_ts, &next);
    if (!tsp)
        return 0;

    __u64 now = bpf_ktime_get_ns();
    __u64 delta = now - *tsp;

    bpf_map_update_elem(&wake2run_ns, &next, &delta, BPF_ANY);
    bpf_map_delete_elem(&wakeup_ts, &next);
    return 0;
}

