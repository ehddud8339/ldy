// bpf/fuse_req_lat.bpf.c

#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

#include "fuse_opname.h"   // opcode -> name (user-space 쪽에서 사용)

// =======================
// BPF 전용 struct 정의
//  - user/include/fuse_req_lat_user.h 의 fuse_req_event 와 레이아웃 동일
// =======================

struct fuse_req_event {
    __u64 unique;          // fuse_in_header.unique
    __u32 opcode;          // fuse_in_header.opcode
    __u32 len;             // fuse_in_header.len
    __s32 err;             // fuse_out_header.error
    __u32 _pad;

    __u64 enqueue_ts_ns;   // pending queue에 들어간 시각
    __u64 dequeue_ts_ns;   // daemon이 가져간 시각
    __u64 done_ts_ns;      // daemon이 완료해서 커널에 완료 알린 시각

    __u64 queue_wait_ns;   // dequeue - enqueue
    __u64 daemon_ns;       // done - dequeue

    __u64 seq;             // 몇 번째 요청인지 (BPF에서 증가시키는 카운터)
};

// =======================
// shadow struct 들
// =======================

// FUSE UAPI 헤더 레이아웃 (커널 struct fuse_in/out_header 앞부분만 복사)
struct fuse_in_header_shadow {
    __u32  len;
    __u32  opcode;
    __u64  unique;
    __u64  nodeid;
    __u32  uid;
    __u32  gid;
    __u32  pid;
    __u32  padding;
};

struct fuse_out_header_shadow {
    __u32  len;
    __s32  error;
    __u64  unique;
};

// 커널 struct fuse_req 의 앞부분만 shadow
// (list, intr_entry, in.h, out.h 정도만 필요)
struct fuse_req_kern {
    struct list_head list;
    struct list_head intr_entry;
    void *args;
    struct refcount_struct count;
    unsigned long flags;

    struct {
        struct fuse_in_header_shadow h;
    } in;

    struct {
        struct fuse_out_header_shadow h;
    } out;
    // 이후 필드는 생략
};

// fuse_copy_state 앞부분 shadow
struct fuse_copy_state_shadow {
    int write;
    struct fuse_req *req;
    // 이후 필드는 생략
};

// =======================
// 맵 정의
// =======================

// unique -> 진행중인 요청 상태
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 65536);
    __type(key, __u64);
    __type(value, struct fuse_req_event);
} req_state SEC(".maps");

// 순번 카운터
struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 1);
    __type(key, __u32);
    __type(value, __u64);
} seq_cnt SEC(".maps");

// ringbuf: 완료된 요청 이벤트 전달용
struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 1 << 24); // 16MB
} events SEC(".maps");

// =======================
// helper
// =======================

static __always_inline int read_req_headers(struct fuse_req *req_ptr,
                                            struct fuse_req_kern *out)
{
    if (!req_ptr)
        return -1;

    if (bpf_probe_read_kernel(out, sizeof(*out), req_ptr))
        return -1;

    return 0;
}

static __always_inline __u64 next_seq(void)
{
    __u32 key = 0;
    __u64 *p = bpf_map_lookup_elem(&seq_cnt, &key);
    __u64 v;

    if (!p) {
        v = 1;
        bpf_map_update_elem(&seq_cnt, &key, &v, BPF_ANY);
        return v;
    }

    v = *p + 1;
    bpf_map_update_elem(&seq_cnt, &key, &v, BPF_ANY);
    return v;
}

// =======================
// kprobe: queue_request_and_unlock
//   queue 에 들어갈 때 (enqueue_ts_ns)
//   queue_request_and_unlock(struct fuse_iqueue *fiq, struct fuse_req *req)
// =======================

SEC("kprobe/queue_request_and_unlock")
int kprobe_queue_request_and_unlock(struct pt_regs *ctx)
{
    struct fuse_req *req = (struct fuse_req *)PT_REGS_PARM2(ctx);
    struct fuse_req_kern kreq;
    struct fuse_req_event ev = {};
    __u64 unique;

    if (read_req_headers(req, &kreq))
        return 0;

    unique = kreq.in.h.unique;
    if (!unique)
        return 0;

    ev.unique        = unique;
    ev.opcode        = kreq.in.h.opcode;
    ev.len           = kreq.in.h.len;
    ev.err           = 0;
    ev.enqueue_ts_ns = bpf_ktime_get_ns();
    ev.dequeue_ts_ns = 0;
    ev.done_ts_ns    = 0;
    ev.queue_wait_ns = 0;
    ev.daemon_ns     = 0;
    ev.seq           = next_seq();

    bpf_map_update_elem(&req_state, &unique, &ev, BPF_ANY);
    return 0;
}

// =======================
// kprobe: fuse_copy_args
//   daemon이 /dev/fuse 에서 request를 실제로 복사 시작하는 시점
//   (cs->req 가 이미 세팅된 이후라 여기서 dequeue_ts_ns 를 찍음)
//   fuse_copy_args(struct fuse_copy_state *cs, ...)
// =======================

SEC("kprobe/fuse_copy_args")
int kprobe_fuse_copy_args(struct pt_regs *ctx)
{
    struct fuse_copy_state_shadow cs_local = {};
    struct fuse_copy_state_shadow *cs;
    struct fuse_req *req;
    struct fuse_req_kern kreq;
    __u64 unique;
    struct fuse_req_event *ev;

    cs = (struct fuse_copy_state_shadow *)PT_REGS_PARM1(ctx);
    if (!cs)
        return 0;

    if (bpf_probe_read_kernel(&cs_local, sizeof(cs_local), cs)) {
        bpf_printk("fuse_copy_args: read cs failed\n");
        return 0;
    }

    req = cs_local.req;
    if (!req) {
        // fuse_dev_do_read 진입 시점과 달리, 여기서는 보통 req != NULL 이어야 함
        bpf_printk("fuse_copy_args: cs->req == NULL\n");
        return 0;
    }

    if (read_req_headers(req, &kreq)) {
        bpf_printk("fuse_copy_args: read_req_headers failed\n");
        return 0;
    }

    unique = kreq.in.h.unique;
    if (!unique) {
        bpf_printk("fuse_copy_args: unique == 0\n");
        return 0;
    }

    ev = bpf_map_lookup_elem(&req_state, &unique);
    if (!ev) {
        bpf_printk("fuse_copy_args: no state for unique=%llu\n", unique);
        return 0;
    }

    if (ev->dequeue_ts_ns == 0) {
        ev->dequeue_ts_ns = bpf_ktime_get_ns();
        if (ev->enqueue_ts_ns)
            ev->queue_wait_ns = ev->dequeue_ts_ns - ev->enqueue_ts_ns;

        bpf_printk("dequeue: unique=%llu seq=%llu queue_wait_ns=%llu\n",
                   unique, ev->seq, ev->queue_wait_ns);
    }

    return 0;
}

// =======================
// kprobe: fuse_request_end
//   요청 완료 시점 (done_ts_ns, err)
//   fuse_request_end(struct fuse_req *req)
// =======================

SEC("kprobe/fuse_request_end")
int kprobe_fuse_request_end(struct pt_regs *ctx)
{
    struct fuse_req *req = (struct fuse_req *)PT_REGS_PARM1(ctx);
    struct fuse_req_kern kreq;
    __u64 unique;
    struct fuse_req_event *st;
    struct fuse_req_event *out;

    if (read_req_headers(req, &kreq))
        return 0;

    unique = kreq.in.h.unique;
    if (!unique)
        return 0;

    st = bpf_map_lookup_elem(&req_state, &unique);
    if (!st)
        return 0;

    st->done_ts_ns = bpf_ktime_get_ns();
    st->err        = kreq.out.h.error;
    if (st->dequeue_ts_ns)
        st->daemon_ns = st->done_ts_ns - st->dequeue_ts_ns;

    out = bpf_ringbuf_reserve(&events, sizeof(*out), 0);
    if (!out) {
        bpf_map_delete_elem(&req_state, &unique);
        return 0;
    }

    *out = *st;
    bpf_ringbuf_submit(out, 0);

    bpf_map_delete_elem(&req_state, &unique);
    return 0;
}

char LICENSE[] SEC("license") = "Dual BSD/GPL";

