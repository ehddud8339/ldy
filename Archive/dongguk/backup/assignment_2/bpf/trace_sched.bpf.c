#include "../include/common.h"
#include "../include/vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_core_read.h>

char LICENSE[] SEC("license") = "GPL";

/* ==== Maps ==== */

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 65536);
    __type(key, __u32);   // pid
    __type(value, __u64); // wake ts (ns)
} wake_ts SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
    __uint(max_entries, 1);
    __type(key, __u32);
    __type(value, __u64);
} switch_cnt SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 1 << 24); // 16 MiB
} ringbuf SEC(".maps");

/* ==== Helpers ==== */

static __always_inline __u32 get_cpu_id(void)
{
    return (__u32)bpf_get_smp_processor_id();
}

static __always_inline void inc_switch_count(void)
{
    __u32 idx = 0;
    __u64 *val = bpf_map_lookup_elem(&switch_cnt, &idx);
    if (val)
        __sync_fetch_and_add(val, 1);
}

/* Emit a sched wake→run latency event to ringbuf */
static __always_inline void emit_sched_event(__u32 pid, const char *next_comm, __u64 latency_ns)
{
    struct event *e = bpf_ringbuf_reserve(&ringbuf, sizeof(*e), 0);
    if (!e)
        return;

    /* header */
    e->h.ts_ns  = bpf_ktime_get_ns();
    e->h.cpu    = get_cpu_id();
    e->h.type   = EVENT_SCHED_WAKE_RUN;

    /* payload */
    e->d.sched.pid        = pid;
    e->d.sched.tgid       = 0;  /* Not available from sched_switch tracepoint; left as 0. */
    e->d.sched.latency_ns = latency_ns;

    /* copy comm (best-effort) */
    if (next_comm) {
        /* next_comm is part of the tracepoint payload; safe copy */
        bpf_probe_read_kernel_str(e->d.sched.comm, COMM_LEN, next_comm);
    } else {
        e->d.sched.comm[0] = '\0';
    }

    bpf_ringbuf_submit(e, 0);
}

/* ==== Tracepoints ==== */

SEC("raw_tracepoint/sched_wakeup")
int on_sched_wakeup(struct bpf_raw_tracepoint_args *ctx)
{
    struct task_struct *p = (void *)ctx->args[0];
    __u32 pid = (__u32)BPF_CORE_READ(p, pid);
    __u64 now = bpf_ktime_get_ns();
    bpf_map_update_elem(&wake_ts, &pid, &now, BPF_ANY);
    return 0;
}

SEC("raw_tracepoint/sched_switch")
int on_sched_switch(struct bpf_raw_tracepoint_args *ctx)
{
    /* bool preempt = (bool)ctx->args[0]; */
    struct task_struct *prev = (void *)ctx->args[1];
    struct task_struct *next = (void *)ctx->args[2];
    (void)prev;

    __u32 next_pid = (__u32)BPF_CORE_READ(next, pid);
    inc_switch_count();

    __u64 *tsp = bpf_map_lookup_elem(&wake_ts, &next_pid);
    if (tsp) {
        __u64 now = bpf_ktime_get_ns();
        __u64 delta = now - *tsp;

        char comm[COMM_LEN];
        if (bpf_core_read_str(&comm, sizeof(comm), &next->comm) < 0)
            comm[0] = '\0';

        emit_sched_event(next_pid, comm, delta);
        bpf_map_delete_elem(&wake_ts, &next_pid);
    }
    return 0;
}
