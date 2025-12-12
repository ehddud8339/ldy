#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>

char LICENSE[] SEC("license") = "Dual BSD/GPL";

// 이벤트 타입 정의
enum event_type {
    EVENT_ENTRY = 0,
    EVENT_EXIT = 1
};

// 유저 공간으로 보낼 공통 데이터 구조체
struct event_t {
    int type;           // 0: ENTRY, 1: EXIT
    u64 ts;             // 이벤트 발생 시각 (나노초)
    u32 pid;
    u32 tid;
    char comm[16];
    u64 len;            // 요청 크기
    unsigned int flags; 
    long ret;           // (EXIT 전용) 리턴 값
    u64 duration_ns;    // (EXIT 전용) 소요 시간
};

// Latency 계산을 위한 시작 시간 저장용 구조체 (Map에 저장)
struct start_data_t {
    u64 ts;
    u64 len;
    unsigned int flags;
    char comm[16];
};

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 10240);
    __type(key, u64);
    __type(value, struct start_data_t);
} start_map SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 256 * 1024);
} rb SEC(".maps");

// --- [진입점 추적] ---
SEC("kprobe/vfs_write")
int BPF_KPROBE(vfs_write_entry, struct file *file, const char *buf, size_t count, loff_t *pos)
{
    struct start_data_t data = {};
    struct event_t *e;
    char comm[16];
    u64 id = bpf_get_current_pid_tgid();

    // 1. comm 필터링
    bpf_get_current_comm(&comm, sizeof(comm));
    if (comm[0] != 'f' || comm[1] != 'i' || comm[2] != 'o' || comm[3] != '\0') {
        return 0;
    }

    // 2. 기본 데이터 준비
    data.ts = bpf_ktime_get_ns();
    data.len = count;
    data.flags = BPF_CORE_READ(file, f_flags);
    __builtin_memcpy(data.comm, comm, sizeof(data.comm));

    // 3. [진입 이벤트 전송] Ring Buffer에 ENTRY 이벤트 submit
    e = bpf_ringbuf_reserve(&rb, sizeof(*e), 0);
    if (e) {
        e->type = EVENT_ENTRY;
        e->ts = data.ts;
        e->pid = id >> 32;
        e->tid = (u32)id;
        e->len = data.len;
        e->flags = data.flags;
        e->ret = 0;          // 진입 시에는 의미 없음
        e->duration_ns = 0;  // 진입 시에는 의미 없음
        __builtin_memcpy(e->comm, comm, sizeof(e->comm));
        bpf_ringbuf_submit(e, 0);
    }

    // 4. Latency 계산을 위해 Map에 시작 정보 저장
    bpf_map_update_elem(&start_map, &id, &data, BPF_ANY);
    return 0;
}

// --- [종료점 추적] ---
SEC("kretprobe/vfs_write")
int BPF_KRETPROBE(vfs_write_exit, long ret)
{
    u64 id = bpf_get_current_pid_tgid();
    struct start_data_t *start;
    struct event_t *e;
    u64 end_ts;

    // 1. Map 조회
    start = bpf_map_lookup_elem(&start_map, &id);
    if (!start) return 0;

    end_ts = bpf_ktime_get_ns();

    // 2. [종료 이벤트 전송] Ring Buffer에 EXIT 이벤트 submit
    e = bpf_ringbuf_reserve(&rb, sizeof(*e), 0);
    if (e) {
        e->type = EVENT_EXIT;
        e->ts = end_ts;
        e->pid = id >> 32;
        e->tid = (u32)id;
        e->len = start->len;
        e->flags = start->flags;
        e->ret = ret;
        e->duration_ns = end_ts - start->ts; // 소요 시간 계산
        __builtin_memcpy(e->comm, start->comm, sizeof(e->comm));
        bpf_ringbuf_submit(e, 0);
    }

    // 3. Map 정리
    bpf_map_delete_elem(&start_map, &id);
    return 0;
}
