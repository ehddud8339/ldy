#include "../include/common.h"
#include "../include/vmlinux.h"

#include <bpf/bpf_helpers.h>
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_tracing.h>



/* =======================
 * Maps
 * =======================*/
/* inflight map: key = rq_ptr (u64), value = struct inflight */
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 65536);
    __type(key, __u64);
    __type(value, struct inflight);
} inflight_map SEC(".maps");

/* per-device queue depth: key = dev (u64), value = s64 depth */
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 2048);
    __type(key, __u64);
    __type(value, __s64);
} qdepth_map SEC(".maps");

/* ring buffer for events */
struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 1 << 24); /* 16MB (튜닝 가능) */
} events_rb SEC(".maps");

/* =======================
 * Small helpers
 * =======================*/
static __inline int inc_qdepth(__u64 dev)
{
    if (!dev) return 0;
    __s64 *v = bpf_map_lookup_elem(&qdepth_map, &dev);
    if (v) {
        __s64 nv = *v + 1;
        bpf_map_update_elem(&qdepth_map, &dev, &nv, BPF_ANY);
    } else {
        __s64 one = 1;
        bpf_map_update_elem(&qdepth_map, &dev, &one, BPF_ANY);
    }
    return 0;
}

static __inline int dec_qdepth(__u64 dev)
{
    if (!dev) return 0;
    __s64 *v = bpf_map_lookup_elem(&qdepth_map, &dev);
    if (v) {
        __s64 nv = *v - 1;
        bpf_map_update_elem(&qdepth_map, &dev, &nv, BPF_ANY);
    } else {
        __s64 minus1 = -1;
        bpf_map_update_elem(&qdepth_map, &dev, &minus1, BPF_ANY);
    }
    return 0;
}

/* rq -> dev (dev_t or (major<<20|minor)) */
static __inline __u64 rq_dev(struct request *rq)
{
    struct gendisk *disk = NULL;
    struct request_queue *q = NULL;
    __u64 dev = 0;

    /* 1) rq->rq_disk (5.15에서 종종 존재) */
    BPF_CORE_READ_INTO(&disk, rq, rq_disk);
    if (!disk) {
        /* 2) rq->q->disk (신규 경로) */
        BPF_CORE_READ_INTO(&q, rq, q);
        if (!q) return 0;
        BPF_CORE_READ_INTO(&disk, q, disk);
        if (!disk) return 0;
    }

    /* 3) devt가 없으니 major/first_minor로 구성 */
    {
        unsigned int major = 0, first_minor = 0;
        BPF_CORE_READ_INTO(&major,       disk, major);
        BPF_CORE_READ_INTO(&first_minor, disk, first_minor);
        if (major || first_minor)
            dev = ((__u64)major << 20) | (__u64)first_minor;
    }
    return dev; /* 없으면 0 */
}

static __inline __u64 rq_size_bytes(struct request *rq)
{
    __u64 sz = 0;

    /* 1) __data_len (바이트 단위) */
    BPF_CORE_READ_INTO(&sz, rq, __data_len);
    if (sz) return sz;

    /* 2) 첫 bio의 bi_iter.bi_size */
    {
        struct bio *bio = NULL;
        BPF_CORE_READ_INTO(&bio, rq, bio);
        if (bio) {
            __u64 bsz = 0;
            BPF_CORE_READ_INTO(&bsz, bio, bi_iter.bi_size);
            if (bsz) return bsz;
        }
    }

    /* 3) 더 이상 안전한 대안 없음 → 0 */
    return 0;
}

/* =======================
 * KPROBES: insert / issue / complete
 * =======================*/

/* INSERT: rq가 스케줄러 큐에 들어가는 시점 */
SEC("kprobe/blk_mq_sched_insert_request")
int BPF_KPROBE(kp_insert, struct request *rq)
{
    __u64 key = (unsigned long)rq;
    struct inflight in = {};
    __u64 now = bpf_ktime_get_ns();
    __u64 tgid_pid = bpf_get_current_pid_tgid();

    in.insert_ns   = now;
    in.issue_ns    = 0;
    in.complete_ns = 0;
    in.size_bytes  = rq_size_bytes(rq);
    in.done_bytes  = 0;
    in.dev         = rq_dev(rq);
    in.tgid        = (__u32)(tgid_pid >> 32);
    in.pid         = (__u32)(tgid_pid & 0xffffffffULL);
    in.flags       = 0; /* 필요 시 rq->cmd_flags를 CORE_READ 해서 채우세요. */

    bpf_map_update_elem(&inflight_map, &key, &in, BPF_ANY);

    inc_qdepth(in.dev);
    return 0;
}

/* ISSUE: rq가 드라이버로 넘겨지는 시점 */
SEC("kprobe/blk_mq_start_request")
int BPF_KPROBE(kp_issue, struct request *rq)
{
    __u64 key = (unsigned long)rq;
    struct inflight *p = bpf_map_lookup_elem(&inflight_map, &key);
    __u64 now = bpf_ktime_get_ns();

    if (p) {
        if (!p->issue_ns) p->issue_ns = now;
        if (!p->size_bytes) p->size_bytes = rq_size_bytes(rq);
        if (!p->dev) p->dev = rq_dev(rq);
    } else {
        /* insert 없이 issue 먼저 찍히는 드문 상황 보정 */
        struct inflight in = {};
        in.insert_ns   = 0;
        in.issue_ns    = now;
        in.complete_ns = 0;
        in.size_bytes  = rq_size_bytes(rq);
        in.done_bytes  = 0;
        in.dev         = rq_dev(rq);
        in.tgid        = 0;
        in.pid         = 0;
        in.flags       = 0;
        bpf_map_update_elem(&inflight_map, &key, &in, BPF_ANY);

        inc_qdepth(in.dev);
    }
    return 0;
}

/* COMPLETE: rq 완료 */
SEC("kprobe/blk_mq_end_request")
int BPF_KPROBE(kp_complete, struct request *rq, blk_status_t error)
{
    __u64 key = (unsigned long)rq;
    struct inflight *p = bpf_map_lookup_elem(&inflight_map, &key);
    __u64 now = bpf_ktime_get_ns();
    __u64 bytes_done = rq_size_bytes(rq);
    __u32 status = (__u32)error; /* blk_status_t는 보통 u8 */

    struct event *e;
    size_t ev_sz = sizeof(*e);

    e = bpf_ringbuf_reserve(&events_rb, ev_sz, 0);
    if (!e) goto out_cleanup;

    __builtin_memset(e, 0, ev_sz);

    /* event header */
    e->h.ts_ns   = now;
    e->h.cpu     = bpf_get_smp_processor_id();
    e->h.type    = EVENT_RQ_COMPLETE;
    e->h.version = EVENT_VERSION;

    /* payload */
    e->d.rq_complete.rq_ptr      = key;
    e->d.rq_complete.dev         = p ? p->dev : rq_dev(rq);
    e->d.rq_complete.bytes_done  = bytes_done;
    e->d.rq_complete.status      = status;

    if (p) {
        if (p->insert_ns && p->issue_ns)
            e->d.rq_complete.lat_queue_ns = (p->issue_ns - p->insert_ns);
        if (p->issue_ns)
            e->d.rq_complete.lat_dev_ns = (now - p->issue_ns);
        if (p->insert_ns)
            e->d.rq_complete.lat_total_ns = (now - p->insert_ns);
    }

    bpf_ringbuf_submit(e, 0);

out_cleanup:
    if (p && p->dev)
        dec_qdepth(p->dev);
    if (p)
        bpf_map_delete_elem(&inflight_map, &key);
    return 0;
}

char LICENSE[] SEC("license") = "GPL";

