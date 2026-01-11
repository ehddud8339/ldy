#pragma once

/* error 방지 용도 */
struct fuse_mount {
};

/* BPF <-> User 공용 이벤트 구조 */
enum event_type {
    EVT_QUEUE = 0,
    EVT_END   = 1,
    EVT_RECV  = 2,
    EVT_SEND  = 3,
    EVT_ALLOC_START = 4, /* [추가] 할당 시작 */
};

struct event {
    uint64_t ts_ns;     /* bpf_ktime_get_ns() */
    uint32_t type;      /* enum event_type */
    uint32_t opcode;
    uint64_t unique;
    int64_t  err;
    uint32_t pid;
    char     comm[16];
};

