#ifndef COMMON_H
#define COMMON_H

/* ----- portable integer types ----- */
#ifndef __u8
typedef unsigned char __u8;
#endif
#ifndef __u16
typedef unsigned short __u16;
#endif
#ifndef __u32
typedef unsigned int __u32;
#endif
#ifndef __u64
typedef unsigned long long __u64;
#endif

/* ----- constants ----- */
#define COMM_LEN 16

/* ----- event types ----- */
enum event_type {
    EVENT_SCHED_WAKE_RUN = 1,   /* wake→run latency (ns) */
    EVENT_IRQ_LATENCY    = 2,   /* hard IRQ handler latency (ns) */
    EVENT_SOFTIRQ_LATENCY= 3    /* softirq latency (ns) */
};

/* softirq latency type */
enum softirq_phase {
    SOFTIRQ_RAISE_TO_ENTRY = 0, /* dispatch delay */
    SOFTIRQ_ENTRY_TO_EXIT  = 1  /* processing time */
};

/* ----- common event header ----- */
struct event_hdr {
    __u64 ts_ns;      /* timestamp (bpf_ktime_get_ns) */
    __u32 cpu;        /* smp processor id */
    __u16 type;       /* enum event_type */
    __u16 version;    /* EVENT_VERSION */
};

/* ----- payloads ----- */
struct sched_wake_run {
    __u32 pid;
    __u32 tgid;
    __u64 latency_ns;             /* wake→run */
    char  comm[COMM_LEN];
};

struct irq_latency {
    __u32 irq;
    __u32 _reserved;              /* align */
    __u64 latency_ns;             /* entry→exit */
};

struct softirq_latency {
    __u32 vec_nr;
    __u8  phase;                  /* enum softirq_phase */
    __u8  _pad[3];
    __u64 latency_ns;
};

/* ----- unified event ----- */
struct event {
    struct event_hdr h;
    union {
        struct sched_wake_run  sched;
        struct irq_latency     irq;
        struct softirq_latency softirq;
    } d;
};

#endif /* COMMON_H */
