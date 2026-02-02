#include "../include/vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>
#include "../include/common.h"

char LICENSE[] SEC("license") = "Dual BSD/GPL";

/* ===== Key & Value ===== */

struct io_key {
    dev_t   dev;        // tracepoint dev 필드(그대로)
    sector_t sector;    // LBA
    __u32   nr_sector;  // 섹터 개수
};

/* ===== Maps ===== */ 

/* Key는 생성할 signature */
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 262144);
    __type(key, struct io_key);
    __type(value, __u64);
} insert_ts_map SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 262144);
    __type(key, struct io_key);
    __type(value, __u64);
} issue_ts_map SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_PERCPU_HASH);
    __uint(max_entries, 1024);
    __type(key, __u64);
    __type(value, __s32);
} qdepth_pc_map SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 1 << 24); /* 16MiB */
} ring SEC(".maps");

/* ===== Helper funcs ===== */

static __always_inline void unpack_devt(dev_t devt, struct dev_id *out)
{
    out->major = (__u32)((devt >> 20) & 0xFFF);
    out->minor = (__u32)(devt & ((1U << 20) - 1));
}

static __always_inline void fill_hdr(struct evt_hdr *h, __u32 type)
{
    __u64 pidtgid = bpf_get_current_pid_tgid();
    h->ts   = bpf_ktime_get_ns();
    h->cpu  = bpf_get_smp_processor_id();
    h->type = type;
    h->pid  = (__u32)pidtgid;
    h->tgid = (__u32)(pidtgid >> 32);
}

static __always_inline __s32 get_qd_cur(__u64 devkey)
{
    __s32 *pd = bpf_map_lookup_elem(&qdepth_pc_map, &devkey);
    return pd ? *pd : 0;
}

/* ===== TPs Ctxs ===== */
struct tp_common {
    unsigned short common_type;       /* +0  size=2 */
    unsigned char  common_flags;      /* +2  size=1 */
    unsigned char  common_preempt_count;/* +3 size=1 */
    int            common_pid;        /* +4  size=4 */
};                                    /* 합계 8바이트 */

struct tp_block_rq_insert {
    struct tp_common _c;              /* +0..+7  */
    dev_t       dev;                  /* +8  size=4 */
    sector_t    sector;               /* +16 size=8 */
    unsigned int nr_sector;           /* +24 size=4 */
    unsigned int bytes;               /* +28 size=4 */
    char        rwbs[8];              /* 이후 사용 안함 */
    char        comm[16];
    int         __data_loc_cmd;
};

struct tp_block_rq_issue {
    struct tp_common _c;
    dev_t       dev;                  /* +8  */
    sector_t    sector;               /* +16 */
    unsigned int nr_sector;           /* +24 */
    unsigned int bytes;               /* +28 */
    char        rwbs[8];
    char        comm[16];
    int         __data_loc_cmd;
};

struct tp_block_rq_complete {
    struct tp_common _c;
    dev_t       dev;                  /* +8  */
    sector_t    sector;               /* +16 */
    unsigned int nr_sector;           /* +24 */
    int         error;                /* +28 size=4 */
    char        rwbs[8];
    int         __data_loc_cmd;
};

/* ===== Tracepoint Programs ===== */

SEC("tracepoint/block/block_rq_insert")
int on_rq_insert(const struct tp_block_rq_insert *ctx)
{
    struct io_key key = {};
    key.dev        = ctx->dev;               /* off=+8  */
    key.sector     = ctx->sector;            /* off=+16 */
    key.nr_sector  = (__u32)ctx->nr_sector;  /* off=+24 */

    __u64 now = bpf_ktime_get_ns();
    bpf_map_update_elem(&insert_ts_map, &key, &now, BPF_ANY);

    struct dev_id did = {};
    unpack_devt(key.dev, &did);
    __u64 devkey = ((__u64)did.major << 32) | did.minor;

    /* INSERT 시점: 증감 없이 현재 QD를 읽어 기록 */
    __s32 qd_cur = get_qd_cur(devkey);

    /* 이벤트 방출 */
    struct event *ev = bpf_ringbuf_reserve(&ring, sizeof(*ev), 0);
    if (!ev) return 0;

    fill_hdr(&ev->h, EVT_RQ_INSERT);
    ev->u.ins.dev        = did;
    ev->u.ins.sector     = key.sector;
    ev->u.ins.nr_sector  = key.nr_sector;
    ev->u.ins.bytes      = (__u32)ctx->bytes;  /* off=+28 */
    ev->u.ins.qd_cur     = qd_cur;             /* ★ 추가 */

    bpf_ringbuf_submit(ev, 0);
    return 0;
}

SEC("tracepoint/block/block_rq_issue")
int on_rq_issue(const struct tp_block_rq_issue *ctx)
{
    struct io_key key = {};
    key.dev        = ctx->dev;
    key.sector     = ctx->sector;
    key.nr_sector  = (__u32)ctx->nr_sector;

    __u64 now = bpf_ktime_get_ns();
    bpf_map_update_elem(&issue_ts_map, &key, &now, BPF_ANY);

    struct dev_id did = {};
    unpack_devt(key.dev, &did);
    __u64 devkey = ((__u64)did.major << 32) | did.minor;

    /* ISSUE 시점: in-flight ++ 후 현재 QD 기록 */
    __s32 *pd = bpf_map_lookup_elem(&qdepth_pc_map, &devkey);
    if (!pd) {
        __s32 zero = 0;
        bpf_map_update_elem(&qdepth_pc_map, &devkey, &zero, BPF_ANY);
        pd = bpf_map_lookup_elem(&qdepth_pc_map, &devkey);
    }
    if (pd) {
        (*pd)++;
    }
    __s32 qd_cur = get_qd_cur(devkey);

    /* 이벤트 방출 */
    struct event *ev = bpf_ringbuf_reserve(&ring, sizeof(*ev), 0);
    if (!ev) return 0;

    fill_hdr(&ev->h, EVT_RQ_ISSUE);
    ev->u.iss.dev        = did;
    ev->u.iss.sector     = key.sector;
    ev->u.iss.nr_sector  = key.nr_sector;
    ev->u.iss.bytes      = (__u32)ctx->bytes;  /* off=+28 */
    ev->u.iss.qd_cur     = qd_cur;             /* ★ 추가 */

    bpf_ringbuf_submit(ev, 0);
    return 0;
}

SEC("tracepoint/block/block_rq_complete")
int on_rq_complete(const struct tp_block_rq_complete *ctx)
{
    struct io_key key = {};
    key.dev        = ctx->dev;
    key.sector     = ctx->sector;
    key.nr_sector  = (__u32)ctx->nr_sector;

    int   error  = ctx->error;              /* off=+28 */
    __u32 bytes  = key.nr_sector * 512U;

    __u64 now = bpf_ktime_get_ns();

    /* device latency: issue -> complete */
    __u64 lat = 0;
    __u64 *p_issue = bpf_map_lookup_elem(&issue_ts_map, &key);
    if (p_issue) {
        lat = (now >= *p_issue) ? (now - *p_issue) : 0;
        bpf_map_delete_elem(&issue_ts_map, &key);
    }

    /* queuing latency: insert -> issue */
    __u64 qlat = 0;
    __u64 *p_insert = bpf_map_lookup_elem(&insert_ts_map, &key);
    if (p_insert && p_issue && *p_issue >= *p_insert) {
        qlat = *p_issue - *p_insert;
        bpf_map_delete_elem(&insert_ts_map, &key);
    } else if (p_insert) {
        bpf_map_delete_elem(&insert_ts_map, &key);
    }

    struct dev_id did = {};
    unpack_devt(key.dev, &did);
    __u64 devkey = ((__u64)did.major << 32) | did.minor;

    /* COMPLETE 시점: in-flight -- 후 현재 QD 기록(음수 방지) */
    __s32 *pd = bpf_map_lookup_elem(&qdepth_pc_map, &devkey);
    if (pd) {
        if (*pd > 0) (*pd)--;
    }
    __s32 qd_cur = get_qd_cur(devkey);

    /* 이벤트 방출 */
    struct event *ev = bpf_ringbuf_reserve(&ring, sizeof(*ev), 0);
    if (!ev) return 0;

    fill_hdr(&ev->h, EVT_RQ_COMPLETE);
    ev->u.cmp.dev       = did;
    ev->u.cmp.sector    = key.sector;        /* 이미 앞서 추가한 필드 */
    ev->u.cmp.nr_sector = key.nr_sector;     /* 이미 앞서 추가한 필드 */
    ev->u.cmp.bytes     = bytes;
    ev->u.cmp.error     = error;
    ev->u.cmp.lat_ns    = lat;
    ev->u.cmp.qlat_ns   = qlat;
    ev->u.cmp.qd_cur    = qd_cur;            /* ★ 추가 */

    bpf_ringbuf_submit(ev, 0);
    return 0;
}
