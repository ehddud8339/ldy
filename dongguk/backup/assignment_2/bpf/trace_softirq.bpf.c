#include "../include/common.h"
#include "../include/vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_core_read.h>

char LICENSE[] SEC("license") = "GPL";

/* ==== Keys ==== */
struct sirq_key {
    __u32 cpu;
    __u32 vec_nr;
};

/* ==== Maps ==== */

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 8192);
    __type(key, struct sirq_key);
    __type(value, __u64);
} softirq_raise_ts SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 8192);
    __type(key, struct sirq_key);
    __type(value, __u64);
} softirq_entry_ts SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_PERCPU_HASH);
    __uint(max_entries, 512);
    __type(key, __u32);   // vec_nr
    __type(value, __u64); // count
} softirq_count SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 1 << 24); // 16 MiB
} ringbuf SEC(".maps");

/* ==== Helpers ==== */

static __always_inline __u32 get_cpu_id(void)
{
    return (__u32)bpf_get_smp_processor_id();
}

static __always_inline void inc_softirq_count(__u32 vec)
{
    __u64 init = 1, *p = bpf_map_lookup_elem(&softirq_count, &vec);
    if (p) {
        *p += 1; /* per-CPU map: atomic 불필요 */
    } else {
        bpf_map_update_elem(&softirq_count, &vec, &init, BPF_ANY);
    }
}

static __always_inline void emit_softirq_event(__u32 vec_nr, __u8 phase, __u64 latency_ns)
{
    struct event *e = bpf_ringbuf_reserve(&ringbuf, sizeof(*e), 0);
    if (!e)
        return;

    /* header */
    e->h.ts_ns   = bpf_ktime_get_ns();
    e->h.cpu     = get_cpu_id();
    e->h.type    = EVENT_SOFTIRQ_LATENCY;

    /* payload */
    e->d.softirq.vec_nr     = vec_nr;
    e->d.softirq.phase      = phase;  /* enum softirq_phase */
    e->d.softirq._pad[0]    = 0;
    e->d.softirq._pad[1]    = 0;
    e->d.softirq._pad[2]    = 0;
    e->d.softirq.latency_ns = latency_ns;

    bpf_ringbuf_submit(e, 0);
}

/* ==== Tracepoints ==== */

SEC("raw_tracepoint/softirq_raise")
int on_softirq_raise(struct bpf_raw_tracepoint_args *ctx)
{
    unsigned int vec = (unsigned int)ctx->args[0];
    struct sirq_key key = { .cpu = get_cpu_id(), .vec_nr = (__u32)vec };
    __u64 now = bpf_ktime_get_ns();
    bpf_map_update_elem(&softirq_raise_ts, &key, &now, BPF_ANY);
    return 0;
}

SEC("raw_tracepoint/softirq_entry")
int on_softirq_entry(struct bpf_raw_tracepoint_args *ctx)
{
    unsigned int vec = (unsigned int)ctx->args[0];
    struct sirq_key key = { .cpu = get_cpu_id(), .vec_nr = (__u32)vec };
    __u64 now = bpf_ktime_get_ns();

    __u64 *tsp = bpf_map_lookup_elem(&softirq_raise_ts, &key);
    if (tsp) {
        __u64 delta = now - *tsp;
        emit_softirq_event((__u32)vec, SOFTIRQ_RAISE_TO_ENTRY, delta);
        bpf_map_delete_elem(&softirq_raise_ts, &key);
    }

    bpf_map_update_elem(&softirq_entry_ts, &key, &now, BPF_ANY);
    inc_softirq_count((__u32)vec);
    return 0;
}

SEC("raw_tracepoint/softirq_exit")
int on_softirq_exit(struct bpf_raw_tracepoint_args *ctx)
{
    unsigned int vec = (unsigned int)ctx->args[0];
    struct sirq_key key = { .cpu = get_cpu_id(), .vec_nr = (__u32)vec };
    __u64 *tsp = bpf_map_lookup_elem(&softirq_entry_ts, &key);
    if (!tsp) return 0;

    __u64 delta = bpf_ktime_get_ns() - *tsp;
    emit_softirq_event((__u32)vec, SOFTIRQ_ENTRY_TO_EXIT, delta);
    bpf_map_delete_elem(&softirq_entry_ts, &key);
    return 0;
}
