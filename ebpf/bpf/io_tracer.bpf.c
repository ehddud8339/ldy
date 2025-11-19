// bpf/io_tracer.bpf.c

#include "../include/vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

#include "../include/common.h"

char LICENSE[] SEC("license") = "GPL";

// ==============================
// 필터 설정 map
// ==============================

struct {
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__uint(max_entries, 1);
	__type(key, __u32);
	__type(value, struct io_filter_conf);
} filter_conf SEC(".maps");

// ==============================
// Correlation ID & 매핑용 map
// ==============================

/* 전역 req_id 시퀀스
 * - key=0 하나에 64bit counter를 저장
 * - sys_enter_write에서 __sync_fetch_and_add로 증가시키며 사용
 */
struct {
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__uint(max_entries, 1);
	__type(key, __u32);
	__type(value, io_req_id_t);
} global_req_seq SEC(".maps");

/* per-task 현재 req_id 매핑
 * - key: pid (스레드 ID, lower 32 bits)
 * - value: io_req_id_t
 * - sys_enter_write에서 세팅, sys_exit_write에서 삭제
 * - vfs/ext4/block/nvme hook들은 여기에서 현재 req_id를 lookup해서 사용
 */
struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 16384);
	__type(key, __u32);
	__type(value, io_req_id_t);
} task_req_map SEC(".maps");

/* (예비) request 포인터 → req_id 매핑
 * - 추후 blk_mq_start_request / nvme_queue_rq 등에서 채워넣을 예정
 * - 지금 단계에서는 아직 사용하지 않음
 */
struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 65536);
	__type(key, __u64);		 // (unsigned long)struct request *
	__type(value, io_req_id_t);
} rq_req_map SEC(".maps");

// ==============================
// 공통 유틸
// ==============================

static __always_inline void get_ids(__u32 *tgid, __u32 *pid)
{
	__u64 id = bpf_get_current_pid_tgid();
	if (tgid)
		*tgid = id >> 32;
	if (pid)
		*pid = id & 0xffffffff;
}

static __always_inline bool pass_filter(void)
{
	__u32 key = 0;
	struct io_filter_conf *conf;
	__u64 pid_tgid = bpf_get_current_pid_tgid();
	__u32 tgid = pid_tgid >> 32;
	char comm[TASK_COMM_LEN];

	conf = bpf_map_lookup_elem(&filter_conf, &key);
	if (!conf || !conf->enabled)
		return true; // 필터 끄면 모두 허용

	if (conf->use_tgid && conf->tgid != tgid)
		return false;

	if (conf->use_comm) {
		if (bpf_get_current_comm(&comm, sizeof(comm)) == 0) {
			int i;
			bool match = true;
#pragma unroll
			for (i = 0; i < TASK_COMM_LEN; i++) {
				if (conf->comm[i] == '\0' && comm[i] == '\0')
					break;
				if (conf->comm[i] != comm[i]) {
					match = false;
					break;
				}
			}
			if (!match)
				return false;
		}
	}

	return true;
}

static __always_inline io_req_id_t next_req_id(void)
{
	__u32 key = 0;
	io_req_id_t *seq = bpf_map_lookup_elem(&global_req_seq, &key);
	io_req_id_t old;

	if (!seq)
		return 0;

	/* __sync_fetch_and_add는 BPF에서 원자 연산으로 lowering 됨 */
	old = __sync_fetch_and_add(seq, 1);

	/* req_id == 0 을 "no correlation"으로 쓰고 싶다면,
	 * 여기서 0이면 한 번 더 증가시키는 식으로 조정할 수 있음.
	 * 지금은 0도 유효한 ID로 허용.
	 */
	return old;
}

/* 현재 task(pid 기준)에 매핑된 req_id 조회 */
static __always_inline io_req_id_t get_current_req_id(void)
{
	__u32 pid;
	io_req_id_t *reqp;

	get_ids(NULL, &pid);
	reqp = bpf_map_lookup_elem(&task_req_map, &pid);
	if (!reqp)
		return 0;
	return *reqp;
}

// ==============================
// Syscall 계층: sys_enter_write / sys_exit_write
// ==============================

SEC("tracepoint/syscalls/sys_enter_write")
int tp_sys_enter_write(void *ctx)
{
	if (!pass_filter())
		return 0;

	__u32 tgid, pid;
	get_ids(&tgid, &pid);

	/* 새로운 correlation ID 생성 */
	io_req_id_t req_id = next_req_id();
	if (req_id) {
		/* per-task map에 현재 req_id 저장 */
		bpf_map_update_elem(&task_req_map, &pid, &req_id, BPF_ANY);
	}

	bpf_printk("[sys_enter_write] req_id=%llu tgid=%u pid=%u\n",
			   (unsigned long long)req_id, tgid, pid);
	return 0;
}

SEC("tracepoint/syscalls/sys_exit_write")
int tp_sys_exit_write(void *ctx)
{
	if (!pass_filter())
		return 0;

	__u32 tgid, pid;
	get_ids(&tgid, &pid);

	io_req_id_t req_id = 0;
	io_req_id_t *reqp = bpf_map_lookup_elem(&task_req_map, &pid);
	if (reqp)
		req_id = *reqp;

	bpf_printk("[sys_exit_write]  req_id=%llu tgid=%u pid=%u\n",
			   (unsigned long long)req_id, tgid, pid);

	/* write() 완료 시점이므로 per-task 매핑은 삭제 */
	if (reqp)
		bpf_map_delete_elem(&task_req_map, &pid);

	return 0;
}

// ==============================
// FS 계층: vfs_write / ext4_file_write_iter / ext4_write_begin/end
// ==============================

// vfs_write(struct file *file, const char __user *buf, size_t count, loff_t *pos)
SEC("kprobe/vfs_write")
int kprobe_vfs_write(struct pt_regs *ctx)
{
	if (!pass_filter())
		return 0;

	__u32 tgid, pid;
	get_ids(&tgid, &pid);

	io_req_id_t req_id = get_current_req_id();

	bpf_printk("[vfs_write]		   req_id=%llu tgid=%u pid=%u\n",
			   (unsigned long long)req_id, tgid, pid);
	return 0;
}

// ext4_file_write_iter(struct kiocb *iocb, struct iov_iter *from)
SEC("kprobe/ext4_file_write_iter")
int kprobe_ext4_file_write_iter(struct pt_regs *ctx)
{
	if (!pass_filter())
		return 0;

	__u32 tgid, pid;
	get_ids(&tgid, &pid);

	io_req_id_t req_id = get_current_req_id();

	bpf_printk("[ext4_write_iter]  req_id=%llu tgid=%u pid=%u\n",
			   (unsigned long long)req_id, tgid, pid);
	return 0;
}

// ext4_write_begin(struct file *file, struct address_space *mapping, loff_t pos,
//	  unsigned len, unsigned flags, struct page **pagep, void **fsdata)
SEC("kprobe/ext4_write_begin")
int kprobe_ext4_write_begin(struct pt_regs *ctx)
{
	if (!pass_filter())
		return 0;

	__u32 tgid, pid;
	get_ids(&tgid, &pid);

	io_req_id_t req_id = get_current_req_id();

	bpf_printk("[ext4_write_begin] req_id=%llu tgid=%u pid=%u\n",
			   (unsigned long long)req_id, tgid, pid);
	return 0;
}

// ext4_write_end(..., loff_t pos, unsigned len, unsigned copied, struct page *page, void *fsdata)
SEC("kprobe/ext4_write_end")
int kprobe_ext4_write_end(struct pt_regs *ctx)
{
	if (!pass_filter())
		return 0;

	__u32 tgid, pid;
	get_ids(&tgid, &pid);

	io_req_id_t req_id = get_current_req_id();

	bpf_printk("[ext4_write_end]   req_id=%llu tgid=%u pid=%u\n",
			   (unsigned long long)req_id, tgid, pid);
	return 0;
}

// ==============================
// JBD2 / 저널링: jbd2_write_superblock
// ==============================

SEC("kprobe/jbd2_write_superblock")
int kprobe_jbd2_write_superblock(struct pt_regs *ctx)
{
	if (!pass_filter())
		return 0;

	__u32 tgid, pid;
	get_ids(&tgid, &pid);

	/* flusher/kworker 문맥일 수도 있으므로 req_id가 0일 가능성 있음 */
	io_req_id_t req_id = get_current_req_id();

	bpf_printk("[jbd2_write_super] req_id=%llu tgid=%u pid=%u\n",
			   (unsigned long long)req_id, tgid, pid);
	return 0;
}

// ==============================
// Block 계층: block_rq_insert / issue / complete
// ==============================
//
// 지금 단계에서는 rq* 포인터에서 req_id 매핑을 아직 안 하고,
// "이 시점에 실행 중인 task 기준"의 req_id만 붙여서 찍는다.
// (sync write 의 경우 꽤 잘 맞고, async는 0일 가능성이 높음)
//
// 추후:
//	 - submit_bio / blk_mq_start_request / nvme_queue_rq 등에서
//	   bio/req 포인터에 req_id를 매핑하는 로직을 추가할 예정.
//

SEC("tracepoint/block/block_rq_insert")
int tp_block_rq_insert(void *ctx)
{
	if (!pass_filter())
		return 0;

	__u32 tgid, pid;
	get_ids(&tgid, &pid);

	io_req_id_t req_id = get_current_req_id();

	bpf_printk("[blk_rq_insert]   req_id=%llu tgid=%u pid=%u\n",
			   (unsigned long long)req_id, tgid, pid);
	return 0;
}

SEC("tracepoint/block/block_rq_issue")
int tp_block_rq_issue(void *ctx)
{
	if (!pass_filter())
		return 0;

	__u32 tgid, pid;
	get_ids(&tgid, &pid);

	io_req_id_t req_id = get_current_req_id();

	bpf_printk("[blk_rq_issue]	  req_id=%llu tgid=%u pid=%u\n",
			   (unsigned long long)req_id, tgid, pid);
	return 0;
}

SEC("tracepoint/block/block_rq_complete")
int tp_block_rq_complete(void *ctx)
{
	if (!pass_filter())
		return 0;

	__u32 tgid, pid;
	get_ids(&tgid, &pid);

	io_req_id_t req_id = get_current_req_id();

	bpf_printk("[blk_rq_complete] req_id=%llu tgid=%u pid=%u\n",
			   (unsigned long long)req_id, tgid, pid);
	return 0;
}

// ==============================
// NVMe 드라이버 계층: nvme_queue_rq / nvme_complete_rq
// ==============================
//
// 여기서도 아직은 rq 포인터를 보지 않고,
// current task 기준 req_id만 태깅하는 수준으로 둔다.
// 추후 rq_req_map을 이용해 request* ↔ req_id 매핑을 추가할 계획.
//

// nvme_queue_rq(struct blk_mq_hw_ctx *hctx, const struct blk_mq_queue_data *bd)
SEC("kprobe/nvme_queue_rq")
int kprobe_nvme_queue_rq(struct pt_regs *ctx)
{
	if (!pass_filter())
		return 0;

	__u32 tgid, pid;
	get_ids(&tgid, &pid);

	io_req_id_t req_id = get_current_req_id();

	bpf_printk("[nvme_queue_rq]   req_id=%llu tgid=%u pid=%u\n",
			   (unsigned long long)req_id, tgid, pid);
	return 0;
}

// nvme_complete_rq(struct request *req)
SEC("kprobe/nvme_complete_rq")
int kprobe_nvme_complete_rq(struct pt_regs *ctx)
{
	if (!pass_filter())
		return 0;

	__u32 tgid, pid;
	get_ids(&tgid, &pid);

	io_req_id_t req_id = get_current_req_id();

	bpf_printk("[nvme_complete_rq] req_id=%llu tgid=%u pid=%u\n",
			   (unsigned long long)req_id, tgid, pid);
	return 0;
}

