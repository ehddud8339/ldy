#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>

#define TASK_COMM_LEN 16

#define PAGE_SHIFT 12
#define PAGE_SIZE  (1UL << PAGE_SHIFT)

char LICENSE[] SEC("license") = "Dual BSD/GPL";

// user랑 공유하는 이벤트 구조체
// 보통 common.h를 생성하여 공유
struct evt {
    __u64 ts;
    __u32 pid, tgid;
    __u64 vaddr;
    char comm[TASK_COMM_LEN];
};
// user와 이벤트를 주고 받을 링 버퍼
struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 1 << 24);
} rb SEC(".maps");

struct trace_event_raw_anon_fault_map {
    struct trace_entry  ent;
    void               *mm;
    unsigned long       addr;
    unsigned long       pfn;
    unsigned long       paddr;
    bool                is_zero;
    bool                writable;
};

/* ==== Probes ==== */
// uprobe/printf로는 추적이 안되서 찾아보니
// glibc에서 보안 용도로 printf() 호출을 __printf_chk()로 치환하는 경우가 존재
SEC("uprobe//lib/x86_64-linux-gnu/libc.so.6:__printf_chk")
int uprobe_printf(struct pt_regs *ctx) {
    // PID로 하는 것이 필터링 하는 것이 정석이지만
    // 현재는 어떤 프로그램을 추적할 지 명확하기 때문에
    // comm으로 필터링
    char comm[TASK_COMM_LEN];
    bpf_get_current_comm(comm, sizeof(comm));
    if (__builtin_memcmp(comm, "AOS\0", 4) != 0) return 0;
    // int __printf_chk(int flag, const char *format, ...);
    // 즉, 3번째 파라미터 부터 가변 인자
    __u64 vaddr = (unsigned long)PT_REGS_PARM3(ctx);
    __u64 id = bpf_get_current_pid_tgid();

    // 링 버퍼에서 이벤트 구조체를 할당 받고 값을 채워 제출
    struct evt *e = bpf_ringbuf_reserve(&rb, sizeof(*e), 0);
    if (!e)
        return 0;

    e->pid   = (__u32)id;
    e->tgid  = (__u32)(id >> 32);
    e->vaddr = vaddr;
    __builtin_memcpy(e->comm, comm, sizeof(e->comm));

    bpf_ringbuf_submit(e, 0);

    return 0;
};

SEC("kprobe/handle_mm_fault")
int BPF_KPROBE(struct pt_regs *ctx) {
    char comm[TASK_COMM_LEN];
    bpf_get_current_comm(comm, sizeof(comm));
    if (__builtin_memcmp(comm, "user_program\0", 13) != 0) return 0;

    __u64 ts = // 현재 타임 스탬프
}

/*
SEC("tracepoint/ldy/anon_fault_map")
int tp_anon_fault_map(struct trace_event_raw_anon_fault_map *ctx) {
    char comm[TASK_COMM_LEN];
    bpf_get_current_comm(comm, sizeof(comm));
    if (__builtin_memcmp(comm, "AOS\0", 4) != 0) return 0;

    unsigned long vaddr = ctx->addr;
    unsigned long pfn   = ctx->pfn;

    __u64 paddr = ((u64)pfn << PAGE_SHIFT) | ((__u64)vaddr & (PAGE_SIZE - 1));

    bpf_printk("comm=%s vaddr=0x%lx paddr=0x%lx",
                comm, vaddr, paddr);
    return 0;
};
*/
