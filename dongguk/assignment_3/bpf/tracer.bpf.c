// SPDX-License-Identifier: GPL-2.0
// bpf/trace_block.bpf.c
//
// Trace mq-deadline via block tracepoints (insert/issue/complete).
// Collect per-request timing and per-device queue depth.
// CO-RE friendly: requires include/vmlinux.h generated from kernel BTF.

#include "../include/vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_core_read.h>
#include "../include/common.h"

/* ===== Maps ===== */

/* runtime config: single slot [0] */
struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 1);
    __type(key, uint32_t);
    __type(value, struct cfg);
} g_cfg SEC(".maps");

/* inflight requests: key = rq pointer */
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 65536);
    __type(key, rq_key_t);
    __type(value, struct inflight);
} inflight_map SEC(".maps");

/* per-device queue depth (approx; hash map). key = dev_id_t */
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 4096);
    __type(key, dev_id_t);
    __type(value, uint64_t);
} qdepth_map SEC(".maps");

/* completion events -> userspace */
struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 1 << 24); /* 16MB ring */
} events_rb SEC(".maps");


/* ===== Helpers ===== */

static __always_inline uint32_t get_pid(void)   { return (uint32_t)(bpf_get_current_pid_tgid() & 0xffffffffULL); }
static __always_inline uint32_t get_tgid(void)  { return (uint32_t)(bpf_get_current_pid_tgid() >> 32); }

static __always_inline dev_id_t dev_from_dev_t(__u32 dev)
{
    /* Linux kdev_t.h: MINORBITS=20 by default on modern kernels */
    dev_id_t d;
    d.major = dev >> 20;
    d.minor = dev & ((1u << 20) - 1);
    return d;
}

/* Try to derive IO flags from tracepoint's rwbs string (e.g., "R", "W", "WS", etc.) */
static __always_inline uint32_t io_flag_from_rwbs(const void *rwbs_ptr)
{
    /* raw tracepoint structs usually have: char rwbs[8]; */
    char c0 = 0, c1 = 0, c2 = 0, c3 = 0;
    /* be robust: read first few chars (CO-RE safe) */
    bpf_core_read(&c0, sizeof(c0), rwbs_ptr);
    bpf_core_read(&c1, sizeof(c1), (const char *)rwbs_ptr + 1);
    bpf_core_read(&c2, sizeof(c2), (const char *)rwbs_ptr + 2);
    bpf_core_read(&c3, sizeof(c3), (const char *)rwbs_ptr + 3);

    uint32_t f = 0;
    if (c0 == 'R' || c0 == 'r') f |= IO_READ;
    if (c0 == 'W' || c0 == 'w') f |= IO_WRITE;
    if (c1 == 'S' || c2 == 'S' || c3 == 'S') f |= IO_SYNC;   /* "...S" indicates sync */
    if (c1 == 'F' || c2 == 'F' || c3 == 'F') f |= IO_FLUSH;  /* "...F" indicates flush */
    if (c1 == 'M' || c2 == 'M' || c3 == 'M') f |= IO_META;   /* meta */
    if (c1 == 'U' || c2 == 'U' || c3 == 'U') f |= IO_FUA;    /* FUA */
    return f;
}

/* Load cfg[0] if exists */
static __always_inline const struct cfg *load_cfg(void)
{
    uint32_t k = 0;
    return bpf_map_lookup_elem(&g_cfg, &k);
}

/* queue depth ++ and return previous depth (snapshot) */
static __always_inline uint32_t qdepth_inc(dev_id_t d)
{
    uint64_t *vp = bpf_map_lookup_elem(&qdepth_map, &d);
    uint64_t one = 1, old = 0;
    if (!vp) {
        /* initialize to 1 */
        bpf_map_update_elem(&qdepth_map, &d, &one, BPF_NOEXIST);
        return 0;
    }
    old = __sync_fetch_and_add(vp, 1);
    return (uint32_t)old;
}

/* queue depth -- and return depth before decrement (snapshot) */
static __always_inline uint32_t qdepth_dec(dev_id_t d)
{
    uint64_t *vp = bpf_map_lookup_elem(&qdepth_map, &d);
    if (!vp) return 0;
    uint64_t old = __sync_fetch_and_add(vp, (uint64_t)-1);
    return (uint32_t)old;
}


/* ===== Tracepoint programs =====
 * Tracepoint arg layout reference: include/trace/events/block.h
 * We rely on CO-RE (BTF) field names commonly present:
 *   struct trace_event_raw_block_rq {
 *     dev_t dev;
 *     sector_t sector;
 *     unsigned int nr_bytes;
 *     /* ... */
 *     char rwbs[8];
 *     /* ... */
 *   }
 */

SEC("tracepoint/block/block_rq_insert")
int on_block_rq_insert(struct trace_event_raw_block_rq *ctx)
{
    const struct cfg *c = load_cfg();

    /* Extract fields from ctx */
    __u32 dev_raw = 0;
    __u32 nr_bytes = 0;
    dev_id_t dev; uint32_t pid = get_pid(), tgid = get_tgid();
    uint32_t flags = 0;

    bpf_core_read(&dev_raw, sizeof(dev_raw), &ctx->dev);
    dev = dev_from_dev_t(dev_raw);
    bpf_core_read(&nr_bytes, sizeof(nr_bytes), &ctx->nr_bytes);
    flags = io_flag_from_rwbs(&ctx->rwbs);

    /* filters */
    if (c) {
        if (!match_task(c, tgid, pid)) return 0;
        if (!match_dev(c, &dev)) return 0;
        if (!match_op(c, flags)) return 0;
        if (!pass_min_bytes(c, nr_bytes)) return 0;
    }

    /* inflight create/update */
    rq_key_t key = (rq_key_t)ctx->rq; /* rq pointer present in raw struct */
    struct inflight st = {};
    st.rq_ptr    = key;
    st.dev       = dev;
    st.bytes_req = nr_bytes;
    st.bytes_done= 0;
    st.io_flag   = flags;
    st.tgid      = tgid;
    st.pid       = pid;
    st.insert_ns = bpf_ktime_get_ns();

    /* snapshot: queue depth before increment */
    uint32_t before = qdepth_inc(dev);
    (void)before; /* if you later want to store this, add a field */

    bpf_map_update_elem(&inflight_map, &key, &st, BPF_ANY);
    return 0;
}

SEC("tracepoint/block/block_rq_issue")
int on_block_rq_issue(struct trace_event_raw_block_rq *ctx)
{
    rq_key_t key = (rq_key_t)ctx->rq;
    struct inflight *st = bpf_map_lookup_elem(&inflight_map, &key);
    if (!st) return 0;

    /* record issue timestamp once */
    if (st->issue_ns == 0)
        st->issue_ns = bpf_ktime_get_ns();

    return 0;
}

SEC("tracepoint/block/block_rq_complete")
int on_block_rq_complete(struct trace_event_raw_block_rq *ctx)
{
    __u32 dev_raw = 0;
    __u32 nr_bytes = 0;
    bpf_core_read(&dev_raw, sizeof(dev_raw), &ctx->dev);
    bpf_core_read(&nr_bytes, sizeof(nr_bytes), &ctx->nr_bytes);

    rq_key_t key = (rq_key_t)ctx->rq;
    struct inflight *st = bpf_map_lookup_elem(&inflight_map, &key);
    if (!st) {
        /* Unknown rq (e.g., filtered earlier). Nothing to do. */
        return 0;
    }

    /* partial-complete accounting */
    st->bytes_done += nr_bytes;
    int fully_done = (st->bytes_req == 0) ? 1 : (st->bytes_done >= st->bytes_req);

    if (!fully_done) {
        /* wait until done */
        return 0;
    }

    /* prepare event */
    struct io_event *e = bpf_ringbuf_reserve(&events_rb, sizeof(*e), 0);
    if (!e) {
        /* still maintain queue depth & cleanup even if drop */
        (void)qdepth_dec(st->dev);
        bpf_map_delete_elem(&inflight_map, &key);
        return 0;
    }

    e->rq_ptr      = st->rq_ptr;
    e->dev         = st->dev;
    e->tgid        = st->tgid;
    e->pid         = st->pid;
    e->io_flag     = st->io_flag;
    e->bytes_req   = st->bytes_req;
    e->bytes_done  = st->bytes_done;
    e->insert_ns   = st->insert_ns;
    e->issue_ns    = st->issue_ns;
    e->complete_ns = bpf_ktime_get_ns();

    bpf_ringbuf_submit(e, 0);

    /* queue depth decrement and cleanup */
    (void)qdepth_dec(st->dev);
    bpf_map_delete_elem(&inflight_map, &key);
    return 0;
}

char LICENSE[] SEC("license") = "GPL";

