#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>

#define TASK_COMM_LEN 16

#define PAGE_MASK (~((1UL << 12) - 1))

char LICENSE[] SEC("license") = "Dual BSD/GPL";

struct trace_event_raw_anon_fault {
    struct trace_entry ent;
    unsigned long addr;
    unsigned long pfn;
};

// user랑 공유하는 이벤트 구조체
// 보통 common.h를 생성하여 공유
struct evt {
    __u64 ts;
    __u32 pid, tgid;
    __u64 vaddr;
    __u64 paddr;
    char comm[TASK_COMM_LEN];
};
// user와 이벤트를 주고 받을 링 버퍼
struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 1 << 24);
} rb SEC(".maps");

/*
SEC("kprobe/handle_mm_fault")
int BPF_KPROBE(handle_mm_fault) {
    char comm[TASK_COMM_LEN];
    bpf_get_current_comm(comm, sizeof(comm));
    if (__builtin_memcmp(comm, "user_program\0", 13) != 0) return 0;

    struct vm_area_struct *vma = (struct vm_area_struct *)PT_REGS_PARM1(ctx);
    struct file *vm_file = BPF_CORE_READ(vma, vm_file);
    if (vm_file != NULL) return 0;
    
    __u64 ts    = bpf_ktime_get_ns();
    __u64 pid   = bpf_get_current_pid_tgid();
    __u64 vaddr = (__u64)PT_REGS_PARM2(ctx) & PAGE_MASK;

    struct evt *e = bpf_ringbuf_reserve(&rb, sizeof(*e), 0);
    if (!e) return 0;

    e->ts       = ts;
    e->pid      = (__u32)pid;
    e->tgid     = (__u32)(pid >> 32);
    e->vaddr    = vaddr;
    e->paddr    = 0;
    __builtin_memcpy(e->comm, comm, sizeof(e->comm));

    bpf_ringbuf_submit(e, 0);
    return 0;
};
*/
SEC("tracepoint/ldy/anon_fault")
int tp_anon_fault(struct trace_event_raw_anon_fault *ctx) {
    char comm[TASK_COMM_LEN];
    bpf_get_current_comm(comm, sizeof(comm));
    if (__builtin_memcmp(comm, "user_program\0", 13) != 0) return 0;

    __u64 ts    = bpf_ktime_get_ns();
    __u64 paddr = ((__u64)ctx->pfn << 12);
    __u64 pid   = bpf_get_current_pid_tgid();

    struct evt *e = bpf_ringbuf_reserve(&rb, sizeof(*e), 0);
    if (!e) return 0;

    e->ts       = ts;
    e->pid      = (__u32)pid;
    e->tgid     = (__u32)(pid >> 32);
    e->vaddr    = (__u64)ctx->addr;
    e->paddr    = paddr;
    __builtin_memcpy(e->comm, comm, sizeof(e->comm));

    bpf_ringbuf_submit(e, 0);

    return 0;
};
