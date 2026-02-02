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
#ifndef __s64
typedef long long __s64;
#endif

/* ----- constants ----- */
#define EVENT_VERSION 1

/* ----- event types (for mq-deadline / block layer) ----- */
enum event_type {
    EVENT_RQ_INSERT   = 1,  /* request inserted to scheduler queue */
    EVENT_RQ_ISSUE    = 2,  /* request issued to device/driver */
    EVENT_RQ_COMPLETE = 3,  /* request completed (may carry latencies) */
    EVENT_QDEPTH_SNAP = 4   /* queue-depth snapshot/update per device */
};

/* ----- common event header ----- */
struct event_hdr {
    __u64 ts_ns;     /* monotonic timestamp (ns) */
    __u32 cpu;       /* smp processor id */
    __u16 type;      /* enum event_type */
    __u16 version;   /* EVENT_VERSION */
};

/* ----- keys / map-shared structs ----- */
/* Per-request map key (request pointer cast to u64) */
struct rq_key {
    __u64 rq_ptr;
};

/* Per-request inflight state (stored in BPF map) */
struct inflight {
    __u64 insert_ns;    /* time at block_rq_insert */
    __u64 issue_ns;     /* time at block_rq_issue (0 if not yet) */
    __u64 complete_ns;  /* optional: set at final completion */
    __u64 size_bytes;   /* original request size in bytes */
    __u64 done_bytes;   /* accumulated completed bytes (partial completes) */
    __u64 dev;          /* device id (e.g., (major<<20)|minor or raw dev_t) */
    __u32 tgid;         /* process id (thread group) */
    __u32 pid;          /* thread id */
    __u32 flags;        /* request flags/cmd_flags (opaque bitset) */
    __u32 _pad;         /* align/future use */
};

/* Per-device queue-depth snapshot (optional shared map/event payload) */
struct qdepth {
    __u64 dev;    /* device id key */
    __s64 depth;  /* instantaneous queue depth (can be negative if bug) */
};

/* ----- payloads (emitted via ringbuf/perf from BPF) ----- */
struct rq_insert_payload {
    __u64 rq_ptr;
    __u64 dev;
    __u64 size_bytes;
    __u32 tgid;
    __u32 pid;
    __u32 flags;     /* rw/sync/flush bits as captured */
    __u32 _pad;
};

struct rq_issue_payload {
    __u64 rq_ptr;
    __u64 dev;
    __u64 size_bytes;
    __u32 tgid;
    __u32 pid;
    __u32 flags;
    __u32 _pad;
};

struct rq_complete_payload {
    __u64 rq_ptr;
    __u64 dev;
    __u64 bytes_done;     /* bytes reported with this completion event */
    __u32 status;         /* blk_status_t cast to __u32 */
    __u32 _pad;

    /* Optional precomputed latencies (ns); 0 if not computed in BPF */
    __u64 lat_queue_ns;   /* issue_ns - insert_ns */
    __u64 lat_dev_ns;     /* complete_ns - issue_ns */
    __u64 lat_total_ns;   /* complete_ns - insert_ns */
};

struct qdepth_payload {
    __u64 dev;
    __s64 depth;
    __u64 interval_ns;    /* measurement window if sampled, else 0 */
};

/* ----- unified event ----- */
struct event {
    struct event_hdr h;
    union {
        struct rq_insert_payload   rq_insert;
        struct rq_issue_payload    rq_issue;
        struct rq_complete_payload rq_complete;
        struct qdepth_payload      qdepth;
    } d;
};

#endif /* COMMON_H */
