// bpf/rfuse_loop_trace.bpf.c
#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

#include "rfuse_common.h"

char LICENSE[] SEC("license") = "GPL";

struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 1 << 24);
} rfuse_loop_events SEC(".maps");

SEC("uprobe/rfuse_latency_probe")
int up_rfuse_latency_probe(struct pt_regs *ctx)
{
    struct rfuse_loop_event *e;

    int riq_id                  = (int)PT_REGS_PARM1(ctx);
    __u32 tid_arg               = (__u32)PT_REGS_PARM2(ctx);
    __u64 gap_ns                = (__u64)PT_REGS_PARM3(ctx);
    __u64 lock_wait_ns          = (__u64)PT_REGS_PARM4(ctx);
    __u64 hold_ns               = (__u64)PT_REGS_PARM5(ctx);
    __u64 ioctl_postunlock_ns   = (__u64)PT_REGS_PARM6(ctx);

    e = bpf_ringbuf_reserve(&rfuse_loop_events, sizeof(*e), 0);
    if (!e)
        return 0;

    e->ts_ns = bpf_ktime_get_ns();
    e->riq_id = riq_id;
    e->tid = tid_arg;
    e->gap_ns = gap_ns;
    e->lock_wait_ns = lock_wait_ns;
    e->hold_ns = hold_ns;
    e->ioctl_postunlock_ns = ioctl_postunlock_ns;

    bpf_ringbuf_submit(e, 0);
    return 0;
}

