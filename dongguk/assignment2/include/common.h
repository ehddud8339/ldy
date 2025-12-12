#ifndef COMMON_H
#define COMMON_H

#if !defined(__BPF__)
#   include <linux/types.h>
#endif

/* ===== Event ===== */
enum event_type {
    EVT_SCHED_LAT = 1,   /* sched_wakeup -> sched_switch 지연 */
    EVT_CTXSW     = 2,   /* sched_switch 발생(컨텍스트 전환 카운트/메타) */
    EVT_IRQ_H     = 3,   /* hard IRQ 처리 시간 (entry->exit) */
    EVT_SIRQ_LAT  = 4,   /* softirq raise -> entry 지연 */
    EVT_SIRQ_DUR  = 5,   /* softirq entry -> exit 처리 시간 */
};

struct evt_hdr {
    __u64 ts;
    __u32 cpu;
    __u32 type;
};

/* ===== Payload ===== */

/* Wakeup -> Switch-in 지연 */
struct sched_lat {
    __u32 pid;
    __u32 target_cpu;   /* sched_wakeup의 target CPU */
    __u32 prio;         /* 깨운 태스크의 우선순위 */
    __u64 delta_ns;     /* wakeup_ts -> switch_ts */
};

/* 컨텍스트 스위치 메타 (카운트/분류용) */
struct ctxsw {
    __u32 prev_pid;
    __u32 next_pid;
    __u32 prev_prio;
    __u32 next_prio;
    __u64 prev_state;
};

/* Hard IRQ 처리 시간 */
struct irq_dur {
    __u32 irq;          /* IRQ 번호 */
    __u32 ret;
    __u64 dur_ns;       /* exit - entry */
};

/* SoftIRQ 지연/처리 시간 */
struct sirq_lat {
    __u32 vec;          /* softirq 벡터 ID (tracepoint vec 필드 그대로) */
    __u32 _pad;
    __u64 lat_ns;       /* entry - raise */
};

struct sirq_dur {
    __u32 vec;          /* softirq 벡터 ID */
    __u32 _pad;
    __u64 dur_ns;       /* exit - entry */
};

/* 링 버퍼 전달 이벤트 */
struct event {
    struct evt_hdr h;
    union {
        struct sched_lat slat;
        struct ctxsw     cs;
        struct irq_dur   idur;
        struct sirq_lat  silat;
        struct sirq_dur  sidur;
    } u;
};

#endif /* COMMON_H */
