#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

#include "../include/common.h"

char LICENSE[] SEC("license") = "GPL";

/* ==== Struct ==== */
struct inner_info {
  uint64_t req_id;
  uint64_t io_inner_id;
};

struct bio_key {
  uint64_t bio_ptr;
};

/* ==== BPF Map ==== */
// Active request
struct {
  __uint(type, BPF_MAP_TYPE_HASH);
  __uint(max_entries, 16384);
  __type(key, u32);   // pid (task->pid)
  __type(value, u64); // req_id
} active_req SEC(".maps");

// Unique local sequence
struct {
  __uint(type, BPF_MAP_TYPE_HASH);
  __uint(max_entries, 16384);
  __type(key, u32);   // pid
  __type(value, u64); // seq counter
} req_seq SEC(".maps");

struct {
  __uint(type, BPF_MAP_TYPE_HASH);
  __uint(max_entries, 65536);
  __type(key, struct bio_key);
  __type(value, struct inner_info);
} bio_map SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 1);
    __type(key, u32);      // always 0
    __type(value, u64);    // monotonically increasing counter
} io_seq SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 1 << 24); // 16MB 예: 상황 맞게 조절
} events_rb SEC(".maps");

/* ==== Helper func ==== */

static __always_inline void get_ids(u32 *pid, u32 *tgid) {
  u64 pid_tgid = bpf_get_current_pid_tgid();
  *pid  = (u32)pid_tgid;
  *tgid = (u32)(pid_tgid >> 32);
}

static __always_inline u64 create_and_store_req_id(void) {
  u32 pid, tgid;
  u64 *seqp, seq = 0;
  u64 req_id;

  get_ids(&pid, &tgid);

  seqp = bpf_map_lookup_elem(&req_seq, &pid);
  if (seqp) {
    seq = *seqp + 1;
    bpf_map_update_elem(&req_seq, &pid, &seq, BPF_ANY);
  } else {
    seq = 1;
    bpf_map_update_elem(&req_seq, &pid, &seq, BPF_ANY);
  }

  req_id = ((u64)tgid << 32) | (seq & 0xffffffffULL);

  bpf_map_update_elem(&active_req, &pid, &req_id, BPF_ANY);

  return req_id;
}

static __always_inline u64 lookup_req_id(int *found) {
  u32 pid, tgid_unused;
  u64 *reqp;
  get_ids(&pid, &tgid_unused);

  reqp = bpf_map_lookup_elem(&active_req, &pid);
  if (!reqp) {
    if (found) *found = 0;
    return 0;
  }
  if (found) *found = 1;
  return *reqp;
}

static __always_inline void clear_req_id(void) {
  u32 pid, tgid_unused;
  get_ids(&pid, &tgid_unused);
  bpf_map_delete_elem(&active_req, &pid);
}

static __always_inline u64 next_io_inner_id(void) {
  u32 idx = 0;
  u64 *valp;
  u64 newv;

  valp = bpf_map_lookup_elem(&io_seq, &idx);
  if (!valp)
    return 0;

  newv = *valp + 1;
  bpf_map_update_elem(&io_seq, &idx, &newv, BPF_ANY);
  return newv;
}

static __always_inline struct event *reserve_evt(void) {
  struct event *e;
  e = bpf_ringbuf_reserve(&events_rb, sizeof(&e), 0);
  return 0;
}

static __always_inline void fill_hdr(struct event *e, u32 evt_id, u64 req_id) {
  u32 pid, tgid;
  get_ids(&pid, &tgid);

  e->hdr.ts_ns = bpf_ktime_get_ns();
  e->hdr.pid = pid;
  e->hdr.tgid = tgid;
  e->hdr.evt_id = evt_id;
  e->hdr.reserved = 0;
  e->hdr.req_id = req_id;
}

static __always_inline void submit_evt(struct event *e) {
  bpf_ringbuf_submit(e, 0);
}

/* ==== Hook funcs ==== */

SEC("kprobe/__x64_sys_read")
int BPF_KPROBE(handle_sys_read_enter, int fd, char __user *buf, size_t count) {
  u64 req_id;
  struct event *e;

  req_id = create_and_store_req_id();

  e = reserve_evt();
  if (!e) return 0;

  fill_hdr(e, EVT_VFS_READ_ENTER, req_id);

  __builtin_memset(&e->payload.vfs, 0, sizeof(e->payload.vfs));
  e->payload.vfs.len_req = (u32)count;
  e->payload.vfs.len_ret = 0;

  submit_evt(e);
  return 0;
}

SEC("kretprobe/__x64_sys_read")
int BPF_KRETPROBE(handle_sys_read_exit) {
  struct event *e;
  u64 req_id;
  int found = 0;
  long ret = PT_REGS_RC(ctx);

  req_id = lookup_req_id(&found);
  if (!found) return 0;

  e = reserve_evt();
  if (!e) goto out;

  fill_hdr(e, EVT_VFS_READ_EXIT, req_id);

  __builtin_memset(&e->payload.vfs, 0, sizeof(e->payload.vfs));
  if (ret > 0) e->payload.vfs.len_ret = (u32)ret;
  else e->payload.vfs.len_ret = 0;

  submit_evt(e);

out:
  clear_req_id();
  return 0;
}

SEC("kprobe/ext4_file_read_iter")
int BPF_KPROBE(handle_ext4_read_enter, struct kiocb *iocb, struct iov_iter *to) {
  struct event *e;
  u64 req_id;
  int found = 0;

  req_id = lookup_req_id(&found);
  if (!found) return 0;

  e = reserve_evt();
  if (!e) return 0;

  fill_hdr(e, EVT_FS_READ_ENTER, req_id);

  __builtin_memset(&e->payload.fs, 0, sizeof(e->payload.fs));

  e->payload.fs.inode        = iocb->ki_filp->f_inode->i_ino;
  e->payload.fs.file_offset  = iocb->ki_pos;
  e->payload.fs.bytes_issued_to_cache = iov_iter_count(to);
  e->payload.fs.flags        = iocb->ki_flags;

  submit_evt(e);
  return 0;
}

SEC("kretprobe/ext4_file_read_iter")
int BPF_KRETPROBE(handle_ext4_read_exit)
{
  struct event *e;
  u64 req_id;
  int found = 0;

  req_id = lookup_req_id(&found);
  if (!found) return 0;

  e = reserve_evt();
  if (!e) return 0;

  fill_hdr(e, EVT_FS_READ_EXIT, req_id);

  __builtin_memset(&e->payload.fs, 0, sizeof(e->payload.fs));
    // 여기서는 return 값을 ctx에서 PT_REGS_RC(ctx)로 뽑아 FS 단계에서 실제로 준비된 bytes 등 기록 가능
  long ret = PT_REGS_RC(ctx);
  e->payload.fs.bytes_issued_to_cache = (ret > 0) ? (u32)ret : 0;

  submit_evt(e);
  return 0;
}

SEC("kprobe/submit_bio_noacct")
int BPF_KPROBE(handle_submit_bio, struct bio *bio) {
  struct event *e;
  struct inner_info info = {};
  struct bio_key key = {};
  __u64 req_id;
  int found = 0;

  req_id = lookup_req_id(&found);
  if (!found) return 0;

  info.req_id = req_id;
  info.io_inner_id = next_io_inner_id();

  key.bio_ptr = (unsigned long)bio;
  bpf_map_update_elem(&bio_map, &key, &info, BPF_ANY);

  e = reserve_evt();
  if (!e) return 0;

  fill_hdr(e, EVT_BLK_SUBMIT, req_id);

  __builtin_memset(&e->payload.blk, 0, sizeof(e->payload.blk));

  e->payload.blk.sector = BPF_CORE_READ(bio, bi_iter.bi_sector);
  siez_t size = BPF_CORE_READ(bio, bi_iter.bi_size);
  e->payload.blk.bytes = (__u32)size;
  e->payload.blk.rwbs = BPF_CORE_READ(bio, bi_opf);

  e->payload.blk.queue_id = 0;
  e->payload.blk.io_inner_id = info.io_inner_id;

  submit_evt(e);
  return 0;
}

SEC("kprobe/bio_endio")
int BPF_KPROBE(handle_bio_endio, struct bio *bio) {
  struct event *e;
  struct bio_key key = {};
  struct inner_info *infop;

  key.bio_ptr = (unsigned long)bio;
  infop = bpf_map_lookup_elem(&bio_map, &key);
  if (!infop) return 0;

  e = reserve_evt();
  if (e) {
    fill_hdr(e, EVT_BLK_COMPLETE, infop->req_id);

    __builtin_memset(&e->payload.blk, 0, sizeof(e->payload.blk));

    e->payload.blk.sector = BPF_CORE_READ(bio, bi_iter.bi_sector);
    siez_t size = BPF_CORE_READ(bio, bi_iter.bi_size);
    e->payload.blk.bytes = (__u32)size;
    e->payload.blk.rwbs = BPF_CORE_READ(bio, bi_opf);

    e->payload.blk.queue_id = 0;
    e->payload.blk.io_inner_id = infop->io_inner_id;

    submit_evt(e);
  }

  bpf_map_delete_elem(&bio_map, &key);
  return 0;
}

SEC("kprobe/nvme_queue_rq")
int BPF_KPROBE(handle_nvme_queue_rq,
               struct blk_mq_hw_ctx *hctx,
               const struct blk_mq_queue_data *bd) {
  struct event *e;
  struct request *rq;
  struct bio *bio;
  struct bio_key key = {};
  struct inner_info *infop;

  rq = (struct request *)BPF_CORE_READ(bd, rq);
  if (!rq) return 0;

  bio = (struct bio *)BPF_CORE_READ(rq, bio);
  if (!bio) return 0;

  key.bio_ptr = (unsigned long)bio;
  infop = bpf_map_lookup_elem(&bio_map, &key);
  if (!infop) return 0;

  e = reserve_evt();
  if (!e) return 0;
  
  // fill_hdr(e, EVT_DRIVER_ENTER, infop->req_id);
}
