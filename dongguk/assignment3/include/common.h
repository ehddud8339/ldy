#ifndef COMMON_H
#define COMMON_H

#if !defined(__BPF__)
#   include <linux/types.h>
#endif

/* ===== Event meta ===== */
enum event_type {
    EVT_RQ_INSERT   = 1,   /* block_rq_insert  */
    EVT_RQ_ISSUE    = 2,   /* block_rq_issue   */
    EVT_RQ_COMPLETE = 3,   /* block_rq_complete */
};

struct evt_hdr {
    __u64 ts;      /* timestamp (ns) */
    __u32 cpu;     /* CPU ID */
    __u32 type;    /* event_type */
    __u32 pid;     /* current PID */
    __u32 tgid;    /* current TGID */
};

/* ===== Device ===== */ 
struct dev_id {
    __u32 major;
    __u32 minor;
};

/* ===== Payload ===== */

struct rq_insert {
    struct dev_id dev;
    __u64 sector;
    __u32 nr_sector;
    __u32 bytes;
    __u64 sig;      /* derived signature key */
    __s32 qd_cur;
};

struct rq_issue {
    struct dev_id dev;
    __u64 sector;
    __u32 nr_sector;
    __u32 bytes;
    __u64 sig;
    __s32 qd_cur;
};

struct rq_complete {
    struct dev_id dev;
    __u64 sector;      /* 추가 */
    __u32 nr_sector;   /* 추가 */
    __u32 bytes;
    __u32 error;
    __u64 lat_ns;      /* issue -> complete */
    __u64 qlat_ns;     /* insert -> issue   */
    __u64 sig;
    __s32 qd_cur;
};

/* ===== Event ===== */

struct event {
    struct evt_hdr h;
    union {
        struct rq_insert   ins;
        struct rq_issue    iss;
        struct rq_complete cmp;
    } u;
};

#endif /* COMMON_H */
