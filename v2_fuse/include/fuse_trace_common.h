#pragma once

#ifndef TASK_COMM_LEN
#define TASK_COMM_LEN 16
#endif

/* error 방지 용도 */
struct fuse_mount {};

/* 측정 플래그(누락/유실 대비) */
enum fuse_req_flags {
    FUSE_F_SEEN_QUEUE = 1u << 0,
    FUSE_F_SEEN_RECV  = 1u << 1,
    FUSE_F_SEEN_SEND  = 1u << 2,
    FUSE_F_SEEN_END   = 1u << 3,
    FUSE_F_SEEN_SCHED = 1u << 4,
};

/* per-unique 상태 (BPF map에 누적 저장) */
struct fuse_req_state {
    /* identity */
    uint64_t unique;
    uint32_t opcode;
    int32_t  err;

    /* process/thread identity */
    uint32_t d_tgid;   /* daemon process(tgid) */
    uint32_t d_tid;    /* daemon thread(tid) */
    uint32_t k_tid;    /* queue 시점의 current tid(커널 컨텍스트) */
    uint32_t flags;

    /* cpu */
    uint32_t d_cpu;    /* recv 시점 daemon cpu */
    uint32_t k_cpu;    /* queue 시점 cpu */
    uint32_t _pad0;
    uint32_t _pad1;

    /* timestamps (ns) */
    uint64_t ts_queue_ns;
    uint64_t ts_recv_ns;
    uint64_t ts_send_ns;
    uint64_t ts_end_ns;

    /* scheduler (v1 핵심: daemon wake2run) */
    uint64_t sched_delay_ns;

    /* metadata */
    char d_comm[TASK_COMM_LEN];
    char k_comm[TASK_COMM_LEN];
};

/* 최종 1회 전송 레코드 (trace_fuse_request_end에서 emit) */
struct fuse_req_event_v1 {
    /* identity */
    uint64_t unique;
    uint32_t opcode;
    int32_t  err;

    /* identities */
    uint32_t d_tgid;
    uint32_t d_tid;
    uint32_t k_tid;
    uint32_t flags;

    /* cpu */
    uint32_t d_cpu;
    uint32_t k_cpu;
    uint32_t _pad0;
    uint32_t _pad1;

    /* raw timestamps */
    uint64_t ts_queue_ns;
    uint64_t ts_recv_ns;
    uint64_t ts_send_ns;
    uint64_t ts_end_ns;

    /* derived durations (ns) */
    uint64_t queuing_ns;       /* recv - queue */
    uint64_t sched_delay_ns;   /* daemon wake2run (v1) */
    uint64_t daemon_ns;        /* send - recv */
    uint64_t response_ns;      /* end - send */

    char d_comm[TASK_COMM_LEN];
    char k_comm[TASK_COMM_LEN];
};

