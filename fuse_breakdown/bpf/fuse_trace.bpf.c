// bpf/fuse_trace.bpf.c

#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

#include "fuse_trace_common.h"

char LICENSE[] SEC("license") = "GPL";

/* ringbuf */
struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 1 << 24);
} events SEC(".maps");


static __always_inline void emit_event(__u32 type,
                                       __u32 opcode,
                                       __u64 unique,
                                       __s64 err)
{
    struct event *e;

    e = bpf_ringbuf_reserve(&events, sizeof(*e), 0);
    if (!e)
        return;

    e->ts_ns  = bpf_ktime_get_ns();
    e->type   = type;
    e->opcode = opcode;
    e->unique = unique;
    e->err    = err;
    e->pid    = (__u32)(bpf_get_current_pid_tgid() >> 32);
    bpf_get_current_comm(&e->comm, sizeof(e->comm));

    bpf_ringbuf_submit(e, 0);
}

/* -------- 커널 쪽 kprobe들 -------- */

SEC("kprobe/fuse_get_req")
int BPF_KPROBE(kp_fuse_get_req, struct fuse_mount *fm)
{
    /* opcode는 여기서 알 수 없으므로 0, unique도 0 */
    emit_event(EVT_ALLOC_START, 0, 0, 0);
    return 0;
}

/* [추가] args->force 일 때 호출되는 할당 함수도 추적 
SEC("kprobe/fuse_request_alloc")
int BPF_KPROBE(kp_fuse_request_alloc, struct fuse_mount *fm, gfp_t flags)
{
    emit_event(EVT_ALLOC_START, 0, 0, 0);
    return 0;
}
*/
/* noinline void trace_fuse_queue_request(unsigned int opcode,u64 unique,u64 ts) */
SEC("kprobe/trace_fuse_queue_request")
int BPF_KPROBE(kp_trace_fuse_queue_request,
               unsigned int opcode, u64 unique, u64 ts)
{
    emit_event(EVT_QUEUE, opcode, unique, 0);
    return 0;
}

/* noinline void trace_fuse_request_end(unsigned int opcode,u64 unique,u64 ts,int err) */
SEC("kprobe/trace_fuse_request_end")
int BPF_KPROBE(kp_trace_fuse_request_end,
               unsigned int opcode, u64 unique, u64 ts, int err)
{
    emit_event(EVT_END, opcode, unique, err);
    return 0;
}

/* -------- 유저쪽 uprobes -------- */

/* 유저: void receive_buf(unsigned int opcode, unsigned long long unique, struct timespec ts) */
SEC("uprobe/receive_buf")
int BPF_KPROBE(up_receive_buf,
               unsigned int opcode,
               unsigned long long unique,
               void *ts_unused)
{
    emit_event(EVT_RECV, opcode, unique, 0);
    return 0;
}

/* libfuse: static int fuse_send_msg(struct fuse_session *se, struct fuse_chan *ch,
 *                                   struct iovec *iov, int count)
 *   - iov[0].iov_base  → struct fuse_out_header*
 *   - 거기서 unique / error 추출
 */

struct fuse_out_header {
    __u32 len;
    __s32 error;
    __u64 unique;
};

/* iovec 최소 정의 (user 영역용) */
struct u_iovec {
    void *iov_base;
    __u64 iov_len;
};

SEC("uprobe/fuse_send_msg")
int BPF_KPROBE(up_fuse_send_msg,
               void *se, void *ch, struct u_iovec *iov, int count)
{
    struct u_iovec iov0 = {};
    struct fuse_out_header hdr = {};
    void *outp;

    /* iov[0] 읽기 */
    if (bpf_probe_read_user(&iov0, sizeof(iov0), &iov[0]) < 0)
        return 0;

    outp = iov0.iov_base;
    if (!outp)
        return 0;

    /* fuse_out_header 읽기 */
    if (bpf_probe_read_user(&hdr, sizeof(hdr), outp) < 0)
        return 0;

    /* opcode는 여기선 알기 어렵기 때문에 0으로 두고, user에서 QUEUE의 opcode를 사용 */
    emit_event(EVT_SEND, 0, hdr.unique, hdr.error);
    return 0;
}


