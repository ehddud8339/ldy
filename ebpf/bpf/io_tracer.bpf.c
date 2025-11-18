// bpf/io_tracer.bpf.c

#include "../include/vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

#include "../include/io_tracer.h"

char LICENSE[] SEC("license") = "GPL";

//
// ========== 공통 유틸 ==========
//

static __always_inline void get_ids(__u32 *tgid, __u32 *pid)
{
  __u64 id = bpf_get_current_pid_tgid();
  if (tgid)
  *tgid = id >> 32;
  if (pid)
  *pid = id & 0xffffffff;
}

//
// ========== Syscall 계층: sys_enter_write / sys_exit_write ==========
//
// - 일단은 tracepoint로만 진입 여부와 pid/tgid 출력
// - 추후 fs_key 생성/관리 로직을 여기에 붙이면 됨
//

SEC("tracepoint/syscalls/sys_enter_write")
int tp_sys_enter_write(void *ctx)
{
  __u32 tgid, pid;
  get_ids(&tgid, &pid);

  bpf_printk("[sys_enter_write] tgid=%u pid=%u\n", tgid, pid);
  return 0;
}

SEC("tracepoint/syscalls/sys_exit_write")
int tp_sys_exit_write(void *ctx)
{
  __u32 tgid, pid;
  get_ids(&tgid, &pid);

  bpf_printk("[sys_exit_write] tgid=%u pid=%u\n", tgid, pid);
  return 0;
}

//
// ========== FS 계층: vfs_write / ext4_file_write_iter / ext4_write_begin/end ==========
//
// - kprobe 기반으로 진입 여부와 간단한 인자만 출력
// - 아직 io_tracer.h의 fs_key, file_id 등은 사용하지 않음
//

// vfs_write(struct file *file, const char __user *buf, size_t count, loff_t *pos)
SEC("kprobe/vfs_write")
int kprobe_vfs_write(struct pt_regs *ctx)
{
  __u32 tgid, pid;
  get_ids(&tgid, &pid);

  /* BTF 기반 BPF_KPROBE 시그니처를 쓰지 않고 pt_regs에서 바로 꺼내지 않음.
   * 일단은 호출 여부만 확인하는 용도로 pid/tgid만 출력.
   */
  bpf_printk("[vfs_write] tgid=%u pid=%u\n", tgid, pid);
  return 0;
}

// ext4_file_write_iter(struct kiocb *iocb, struct iov_iter *from)
SEC("kprobe/ext4_file_write_iter")
int kprobe_ext4_file_write_iter(struct pt_regs *ctx)
{
  __u32 tgid, pid;
  get_ids(&tgid, &pid);

  bpf_printk("[ext4_file_write_iter] tgid=%u pid=%u\n", tgid, pid);
  return 0;
}

// ext4_write_begin(struct file *file, struct address_space *mapping, loff_t pos,
//	  unsigned len, unsigned flags, struct page **pagep, void **fsdata)
SEC("kprobe/ext4_write_begin")
int kprobe_ext4_write_begin(struct pt_regs *ctx)
{
  __u32 tgid, pid;
  get_ids(&tgid, &pid);

  bpf_printk("[ext4_write_begin] tgid=%u pid=%u\n", tgid, pid);
  return 0;
}

// ext4_write_end(..., loff_t pos, unsigned len, unsigned copied, struct page *page, void *fsdata)
SEC("kprobe/ext4_write_end")
int kprobe_ext4_write_end(struct pt_regs *ctx)
{
  __u32 tgid, pid;
  get_ids(&tgid, &pid);

  bpf_printk("[ext4_write_end] tgid=%u pid=%u\n", tgid, pid);
  return 0;
}

//
// ========== JBD2 / 저널링: jbd2_write_superblock ==========
//
// - 저널 커밋 시점이 찍히는지만 확인
//

SEC("kprobe/jbd2_write_superblock")
int kprobe_jbd2_write_superblock(struct pt_regs *ctx)
{
  __u32 tgid, pid;
  get_ids(&tgid, &pid);

  bpf_printk("[jbd2_write_superblock] tgid=%u pid=%u\n", tgid, pid);
  return 0;
}

//
// ========== Block 계층: block_rq_insert / issue / complete ==========
//
// - tracepoint/block/* 은 ctx 타입을 신경 쓰지 않고 void *로만 받음
//	 (현재 단계에선 bpf_printk만 하기 때문에 문제 없음)
//

SEC("tracepoint/block/block_rq_insert")
int tp_block_rq_insert(void *ctx)
{
  __u32 tgid, pid;
  get_ids(&tgid, &pid);

  bpf_printk("[block_rq_insert] tgid=%u pid=%u\n", tgid, pid);
  return 0;
}

SEC("tracepoint/block/block_rq_issue")
int tp_block_rq_issue(void *ctx)
{
  __u32 tgid, pid;
  get_ids(&tgid, &pid);

  bpf_printk("[block_rq_issue] tgid=%u pid=%u\n", tgid, pid);
  return 0;
}

SEC("tracepoint/block/block_rq_complete")
int tp_block_rq_complete(void *ctx)
{
  __u32 tgid, pid;
  get_ids(&tgid, &pid);

  bpf_printk("[block_rq_complete] tgid=%u pid=%u\n", tgid, pid);
  return 0;
}

//
// ========== NVMe 드라이버 계층: nvme_queue_rq / nvme_complete_rq ==========
//
// - kprobe 기반
// - 일단 rq 포인터나 큐 ID 같은 건 나중에 BTF/BPF_CORE_READ로 뽑도록 두고
//	 여기선 단순히 호출 여부만 확인
//

// nvme_queue_rq(struct blk_mq_hw_ctx *hctx, const struct blk_mq_queue_data *bd)
SEC("kprobe/nvme_queue_rq")
int kprobe_nvme_queue_rq(struct pt_regs *ctx)
{
  __u32 tgid, pid;
  get_ids(&tgid, &pid);

  bpf_printk("[nvme_queue_rq] tgid=%u pid=%u\n", tgid, pid);
  return 0;
}

// nvme_complete_rq(struct request *req)
SEC("kprobe/nvme_complete_rq")
int kprobe_nvme_complete_rq(struct pt_regs *ctx)
{
  __u32 tgid, pid;
  get_ids(&tgid, &pid);

  bpf_printk("[nvme_complete_rq] tgid=%u pid=%u\n", tgid, pid);
  return 0;
}
