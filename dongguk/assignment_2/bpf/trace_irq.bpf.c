#include "../include/common.h"
#include "../include/vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_core_read.h>

char LICENSE[] SEC("license") = "GPL";

/* ==== Keys ==== */
struct irq_key {
    __u32 cpu;
    __u32 irq;
};

/* ==== Maps ==== */

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 8192);
    __type(key, struct irq_key);
    __type(value, __u64);
} irq_entry_ts SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_PERCPU_HASH);
    __uint(max_entries, 2048);
    __type(key, __u32);   // irq
    __type(value, __u64); // count
} irq_count SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 1 << 24); // 16 MiB
} ringbuf SEC(".maps");

/* ==== Helpers ==== */

static __always_inline __u32 get_cpu_id(void)
{
    return (__u32)bpf_get_smp_processor_id();
}

static __always_inline void inc_irq_count(__u32 irq)
{
    __u64 init = 1, *p;

    p = bpf_map_lookup_elem(&irq_count, &irq);
    if (p) {
        /* per-CPU hash value: safe to update without atomic */
        *p += 1;
    } else {
        bpf_map_update_elem(&irq_count, &irq, &init, BPF_ANY);
    }
}

static __always_inline void emit_irq_event(__u32 irq, __u64 latency_ns)
{
    struct event *e = bpf_ringbuf_reserve(&ringbuf, sizeof(*e), 0);
    if (!e)
        return;

    /* header */
    e->h.ts_ns   = bpf_ktime_get_ns();
    e->h.cpu     = get_cpu_id();
    e->h.type    = EVENT_IRQ_LATENCY;

    /* payload */
    e->d.irq.irq        = irq;
    e->d.irq._reserved  = 0;
    e->d.irq.latency_ns = latency_ns;

    bpf_ringbuf_submit(e, 0);
}

/* ==== Tracepoints ==== */

SEC("raw_tracepoint/irq_handler_entry")
int on_irq_handler_entry(struct bpf_raw_tracepoint_args *ctx)
{
    int irq = (int)ctx->args[0];
    /* struct irqaction *action = (void *)ctx->args[1]; (unused) */
    struct irq_key key = { .cpu = get_cpu_id(), .irq = (__u32)irq };
    __u64 now = bpf_ktime_get_ns();

    bpf_map_update_elem(&irq_entry_ts, &key, &now, BPF_ANY);
    inc_irq_count((__u32)irq);
    return 0;
}

SEC("raw_tracepoint/irq_handler_exit")
int on_irq_handler_exit(struct bpf_raw_tracepoint_args *ctx)
{
    int irq = (int)ctx->args[0];
    /* struct irqaction *action = (void *)ctx->args[1]; int ret = (int)ctx->args[2]; */
    struct irq_key key = { .cpu = get_cpu_id(), .irq = (__u32)irq };
    __u64 *tsp = bpf_map_lookup_elem(&irq_entry_ts, &key);
    if (!tsp) return 0;

    __u64 delta = bpf_ktime_get_ns() - *tsp;
    emit_irq_event((__u32)irq, delta);
    bpf_map_delete_elem(&irq_entry_ts, &key);
    return 0;
}
