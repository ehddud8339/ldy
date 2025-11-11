#include "../include/vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>
#include "../include/common.h"

char LICENSE[] SEC("license") = "Dual BSD/GPL";

/* ===== Keys ===== */
struct irq_key {
    __u32 cpu;
    __u32 irq;
};

struct sirq_key {
    __u32 cpu;
    __u32 vec;
};

/* ===== Maps ===== */

/* Wakeup -> Switch-in 지연 측정 */
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 131072);
    __type(key, __u32);      // pid
    __type(value, __u64);    // wakeup ts (ns)
} wakeup_ts_map SEC(".maps");

/* Hard IRQ entry 저장 */
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 8192);
    __type(key, struct irq_key);
    __type(value, __u64);
} irq_entry_ts SEC(".maps");

/* SoftIRQ entry/exit 저장 */ 
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 8192);
    __type(key, struct sirq_key);
    __type(value, __u64);
} sirq_raise_ts SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 8192);
    __type(key, struct sirq_key);
    __type(value, __u64);
} sirq_entry_ts SEC(".maps");

/* 유저 전달용 버퍼 */
struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 1 << 24); /* 16 MiB */
} events SEC(".maps");

/* ===== Helpers ===== */
static __always_inline __u32 get_cpu(void) {
    return bpf_get_smp_processor_id();
}

static __always_inline void push_event(void *data, __u64 size)
{
    void *dst = bpf_ringbuf_reserve(&events, size, 0);
    if (!dst) return;
    __builtin_memcpy(dst, data, size);
    bpf_ringbuf_submit(dst, 0);
}

/* ===== Hook points ===== */

SEC("raw_tracepoint/sched_wakeup")
int on_sched_wakeup(struct bpf_raw_tracepoint_args *ctx)
{
    struct task_struct *p = (void *)ctx->args[0];
    __u32 pid = (__u32)BPF_CORE_READ(p, pid);
    __u64 now = bpf_ktime_get_ns();
    bpf_map_update_elem(&wakeup_ts_map, &pid, &now, BPF_ANY);
    return 0;
}

SEC("raw_tracepoint/sched_switch")
int on_sched_switch(struct bpf_raw_tracepoint_args *ctx)
{
    /* bool preempt = (bool)ctx->args[0]; */
    struct task_struct *prev = (void *)ctx->args[1];
    struct task_struct *next = (void *)ctx->args[2];

    __u64 now = bpf_ktime_get_ns();
    __u32 cpu = get_cpu();

    /* 1) 스케줄 지연: wakeup -> switch-in */
    __u32 next_pid  = (__u32)BPF_CORE_READ(next, pid);
    __u32 next_prio = (__u32)BPF_CORE_READ(next, prio);
    __u64 *wts = bpf_map_lookup_elem(&wakeup_ts_map, &next_pid);
    if (wts) {
        struct event e = {};
        e.h.ts   = now;
        e.h.cpu  = cpu;
        e.h.type = EVT_SCHED_LAT;
        e.u.slat.pid        = next_pid;
        e.u.slat.prio       = next_prio;
        e.u.slat.target_cpu = cpu;              /* 실제 인스케줄된 CPU 기록 */
        e.u.slat.delta_ns   = now - *wts;
        push_event(&e, sizeof(e));
        bpf_map_delete_elem(&wakeup_ts_map, &next_pid);
    }

    /* 2) 컨텍스트 스위치 메타 */
    {
        struct event e = {};
        __u32 prev_pid  = (__u32)BPF_CORE_READ(prev, pid);
        __u32 prev_prio = (__u32)BPF_CORE_READ(prev, prio);

        e.h.ts   = now;
        e.h.cpu  = cpu;
        e.h.type = EVT_CTXSW;
        e.u.cs.prev_pid   = prev_pid;
        e.u.cs.next_pid   = next_pid;
        e.u.cs.prev_prio  = prev_prio;
        e.u.cs.next_prio  = next_prio;
        push_event(&e, sizeof(e));
    }
    return 0;
}

SEC("raw_tracepoint/irq_handler_entry")
int on_irq_entry(struct bpf_raw_tracepoint_args *ctx)
{
    int irq = (int)ctx->args[0];
    __u64 now = bpf_ktime_get_ns();
    struct irq_key key = { .cpu = get_cpu(), .irq = (__u32)irq };
    bpf_map_update_elem(&irq_entry_ts, &key, &now, BPF_ANY);
    return 0;
}

SEC("raw_tracepoint/irq_handler_exit")
int on_irq_exit(struct bpf_raw_tracepoint_args *ctx)
{
    int irq = (int)ctx->args[0];
    int ret = (int)ctx->args[2];
    __u64 now = bpf_ktime_get_ns();
    struct irq_key key = { .cpu = get_cpu(), .irq = (__u32)irq };

    __u64 *ets = bpf_map_lookup_elem(&irq_entry_ts, &key);
    if (ets) {
        struct event e = {};
        e.h.ts   = now;
        e.h.cpu  = key.cpu;
        e.h.type = EVT_IRQ_H;
        e.u.idur.irq    = key.irq;
        e.u.idur.ret    = (__u32)ret;
        e.u.idur.dur_ns = now - *ets;
        push_event(&e, sizeof(e));
        bpf_map_delete_elem(&irq_entry_ts, &key);
    }
    return 0;
}

SEC("raw_tracepoint/softirq_raise")
int on_sirq_raise(struct bpf_raw_tracepoint_args *ctx)
{
    unsigned int vec = (unsigned int)ctx->args[0];
    __u64 now = bpf_ktime_get_ns();
    struct sirq_key key = { .cpu = get_cpu(), .vec = (__u32)vec };
    bpf_map_update_elem(&sirq_raise_ts, &key, &now, BPF_ANY);
    return 0;
}

SEC("raw_tracepoint/softirq_entry")
int on_sirq_entry(struct bpf_raw_tracepoint_args *ctx)
{
    unsigned int vec = (unsigned int)ctx->args[0];
    __u64 now = bpf_ktime_get_ns();
    struct sirq_key key = { .cpu = get_cpu(), .vec = (__u32)vec };

    /* raise -> entry 지연 */
    __u64 *rts = bpf_map_lookup_elem(&sirq_raise_ts, &key);
    if (rts) {
        struct event e = {};
        e.h.ts   = now;
        e.h.cpu  = key.cpu;
        e.h.type = EVT_SIRQ_LAT;
        e.u.silat.vec    = key.vec;
        e.u.silat.lat_ns = now - *rts;
        push_event(&e, sizeof(e));
        bpf_map_delete_elem(&sirq_raise_ts, &key);
    }

    /* entry ts 저장 (entry -> exit 처리시간 계산) */
    bpf_map_update_elem(&sirq_entry_ts, &key, &now, BPF_ANY);
    return 0;
}

SEC("raw_tracepoint/softirq_exit")
int on_sirq_exit(struct bpf_raw_tracepoint_args *ctx)
{
    unsigned int vec = (unsigned int)ctx->args[0];
    __u64 now = bpf_ktime_get_ns();
    struct sirq_key key = { .cpu = get_cpu(), .vec = (__u32)vec };

    __u64 *ets = bpf_map_lookup_elem(&sirq_entry_ts, &key);
    if (ets) {
        struct event e = {};
        e.h.ts   = now;
        e.h.cpu  = key.cpu;
        e.h.type = EVT_SIRQ_DUR;
        e.u.sidur.vec    = key.vec;
        e.u.sidur.dur_ns = now - *ets;
        push_event(&e, sizeof(e));
        bpf_map_delete_elem(&sirq_entry_ts, &key);
    }
    return 0;
}

