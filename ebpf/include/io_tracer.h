#ifndef IO_TRACER_H
#define IO_TRACER_H

/* 공용 헤더는 BPF/유저 공간 양쪽에서 사용됨
 * - BPF 쪽: vmlinux.h 또는 linux/types.h를 먼저 include
 * - 유저 쪽: 필요하다면 아래 typedef를 활성화해서 사용
 */
/*
#ifndef __BPF_TYPES_DEFINED
#define __BPF_TYPES_DEFINED
#include <stdint.h>
typedef uint8_t  __u8;
typedef uint16_t __u16;
typedef uint32_t __u32;
typedef uint64_t __u64;
typedef int32_t  __s32;
typedef int64_t  __s64;
#endif
*/

//
// ========== 공통 상수 / enum ==========
//

/* 이벤트 타입
 * - 같은 struct를 재사용하더라도 evt_type으로 어떤 단계인지 구분
 */
enum io_evt_type {
  // FS 계층 (syscall + VFS + ext4)
  IO_EVT_FS_SYS_ENTER = 1,    // sys_enter_write
  IO_EVT_FS_SYS_EXIT,   // sys_exit_write
  IO_EVT_FS_VFS_WRITE,    // vfs_write
  IO_EVT_FS_WRITE_ITER,   // ext4_file_write_iter (또는 범용 write_iter)
  IO_EVT_EXT4_WRITE_BEGIN,    // ext4_write_begin
  IO_EVT_EXT4_WRITE_END,    // ext4_write_end

  // JBD2 / 저널링
  IO_EVT_JBD2_WRITE_SUPER,    // jbd2_write_superblock

  // Block 계층
  IO_EVT_BLK_RQ_INSERT,   // block_rq_insert
  IO_EVT_BLK_RQ_ISSUE,    // block_rq_issue
  IO_EVT_BLK_RQ_COMPLETE,   // block_rq_complete

  // NVMe 드라이버 계층
  IO_EVT_NVME_QUEUE_RQ,   // nvme_queue_rq
  IO_EVT_NVME_COMPLETE_RQ,    // nvme_complete_rq
};

//
// ========== 키 / 식별자 구조체 ==========
//

/* FS 계층에서 사용하는 syscall 단위 correlation 키
 * - (tgid, pid, seq) 조합
 * - seq는 per-(tgid,pid) 모노토닉 증가값
 */
struct fs_key {
  __u32 tgid; // 프로세스 ID
  __u32 pid;  // 스레드 ID
  __u64 seq;  // 해당 태스크 내 write() 시퀀스 번호
};

/* Block/NVMe/IRQ 계층에서 사용하는 request 키
 * - struct request * 포인터 값을 그대로 사용
 */
struct rq_key {
  __u64 ptr;  // (unsigned long)(struct request *)
};

/* 파일 식별자
 * - FS/Block 계층에서 공통으로 사용할 수 있는 최소 식별 정보
 * - dev: 하위 비트 중심으로 쓸 것이므로 32bit로 충분 (분석 목적)
 */
struct file_id {
  __u32 dev;  // (u32)inode->i_sb->s_dev 또는 bio->bi_bdev->bd_dev
  __u32 pad;  // 정렬용 패딩
  __u64 ino;  // inode 번호
};

//
// ========== 공통 이벤트 헤더 ==========
//

/* 모든 이벤트에서 공통으로 사용하는 헤더
 * - ts_ns: 이벤트 발생 시점 (monotonic clock 기준)
 * - evt_type: enum io_evt_type
 * - cpu: BPF가 실행된 CPU 번호
 * - tgid/pid: current->tgid/pid (해당 시점의 태스크 문맥)
 */
struct io_evt_hdr {
  __u64 ts_ns;    // 이벤트 타임스탬프 (ns)
  __u32 evt_type; // enum io_evt_type
  __u32 cpu;    // 현재 CPU

  __u32 tgid;   // current->tgid
  __u32 pid;    // current->pid
};

//
// ========== FS 계층 이벤트 페이로드 ==========
//

/* sys_enter_write 시점
 * - FS correlation을 위한 시작 정보
 * - fd, count 등 syscall 인자 중심
 */
struct evt_fs_sys_enter {
  struct fs_key key;  // FS correlation 키

  __s32 fd;     // write(fd, ...)
  __u32 pad0;

  __u64 count;    // 요청한 바이트 수
  __u64 buf;    // 사용자 버퍼 포인터 (분석 목적, 보안상 필요 없으면 생략 가능)
};

/* sys_exit_write 시점
 * - syscall 결과/지연시간 계산에 사용
 * - ret: 성공 시 쓰여진 바이트 수, 실패 시 음수 에러
 */
struct evt_fs_sys_exit {
  struct fs_key key;

  __s64 ret;  // write() 반환값
  __u64 pad0; // 정렬용
};

/* vfs_write 시점
 * - file/inode와 offset 정보를 얻을 수 있는 가장 첫 지점
 */
struct evt_fs_vfs_write {
  struct fs_key key;
  struct file_id fid; // 대상 파일

  __u64 pos;    // 파일 오프셋 (바이트)
  __u64 count;    // 쓰기 바이트 수
};

/* write_iter (ext4_file_write_iter) 시점
 * - 실제로 FS가 보는 write 길이, 플래그 등을 기록
 * - direct I/O / sync 여부 등도 플래그로 인코딩 가능
 */
struct evt_fs_write_iter {
  struct fs_key key;
  struct file_id fid;

  __u64 pos;    // iocb->ki_pos (바이트)
  __u64 bytes;    // iov_iter에서 계산된 총 바이트 수

  __u32 flags;    // 필요시 RWF_xxx, O_DIRECT 등 비트 인코딩
  __u32 pad0;
};

/* ext4_write_begin / ext4_write_end
 * - 페이지 캐시 / block mapping 관점에서의 경계
 * - writeback/flusher 문맥에서도 호출될 수 있으므로
 *   fs_key가 없는 경우도 발생할 수 있음 (fs_key의 seq==0 등으로 표현 가능)
 */
struct evt_ext4_write_begin {
  struct fs_key key;   // 유저 문맥이면 유효, flusher면 0 초기값일 수 있음
  struct file_id fid;

  __u64 pos;     // 시작 바이트 오프셋
  __u32 len;     // 처리하려는 바이트 수 (페이지 단위로 반올림된 값일 수 있음)
  __u32 flags;     // ext4 내부 플래그(옵션)
};

struct evt_ext4_write_end {
  struct fs_key key;
  struct file_id fid;

  __u64 pos;     // 동일 영역을 가리키는 오프셋
  __u32 copied;    // 실제로 쓰인 바이트 수
  __u32 err;     // 오류 발생 시 에러 코드 (0이면 성공)
};

//
// ========== JBD2 / 저널링 이벤트 페이로드 ==========
//

/* jbd2_write_superblock
 * - 저널 커밋 구간(트랜잭션) 단위 latency 파악용
 * - 특정 syscall과 직접 매칭하지 않고, 시스템 전체 write backpressure 지표로 사용
 */
struct evt_jbd2_write_super {
  __u32 dev;    // 저널이 올라간 블록 디바이스
  __u32 pad0;

  __u64 tid;    // journal transaction id (가능하면)
};

//
// ========== Block 계층 이벤트 페이로드 ==========
//

/* block_rq_* 공통 정보
 * - insert/issue/complete 모두 같은 구조 사용하고
 *   evt_type으로 어느 단계인지 구분한다.
 */
struct evt_blk_rq {
  struct rq_key rq;   // struct request * 포인터
  __u32 dev;    // (u32)rq->rq_disk->first_minor 등, 분석 용도
  __u32 op;     // req_op(rq) 값 (REQ_OP_READ/WRITE/FLUSH 등)

  __u64 sector;   // 시작 섹터
  __u32 nr_sectors;   // 섹터 개수
  __u32 pad0;

  // 필요시 여기 아래에 스케줄러 이름, priority 등 추가 가능
};

//
// ========== NVMe 드라이버 계층 이벤트 페이로드 ==========
//

/* nvme_queue_rq
 * - NVMe SQ에 커맨드를 제출하는 시점
 */
struct evt_nvme_queue_rq {
  struct rq_key rq;

  __u16 qid;    // Submission Queue ID
  __u8  opcode;   // NVMe 명령 opcode (NVME_CMD_READ/WRITE 등)
  __u8  pad0;

  __u32 nsid;   // Namespace ID

  // 필요시 slba, nblocks 등도 추가 가능 (추적 가능한 경우에 한해)
  __u64 slba;   // 시작 LBA (옵션)
  __u16 nblocks;  // 블록 수 (옵션)
  __u16 pad1;
  __u32 pad2;
};

/* nvme_complete_rq
 * - CQ에서 completion을 처리한 시점
 */
struct evt_nvme_complete_rq {
  struct rq_key rq;

  __u16 qid;    // Completion Queue ID (또는 동일 SQ ID)
  __u16 status;   // NVMe completion status (SC + SCT 등 인코딩)

  __u32 pad0;
};

//
// ========== 통합 이벤트 구조체 ==========
//

/* ring buffer로 내보낼 최종 이벤트
 * - hdr.evt_type으로 union 중 어떤 필드가 유효한지 구분한다.
 */
struct io_evt {
  struct io_evt_hdr hdr;

  union {
  // FS 계층
  struct evt_fs_sys_enter    fs_sys_enter;
  struct evt_fs_sys_exit     fs_sys_exit;
  struct evt_fs_vfs_write    fs_vfs;
  struct evt_fs_write_iter   fs_iter;
  struct evt_ext4_write_begin  ext4_begin;
  struct evt_ext4_write_end  ext4_end;

  // JBD2 / 저널링
  struct evt_jbd2_write_super  jbd2_super;

  // Block 계층
  struct evt_blk_rq    blk_rq;

  // NVMe 드라이버
  struct evt_nvme_queue_rq   nvme_q;
  struct evt_nvme_complete_rq  nvme_c;
  } data;
};

//
// ========== (선택) 런타임 설정 구조체 ==========
//

/* 트레이서 동작 제어용 설정
 * - BPF_ARRAY(g_cfg, struct io_cfg, 1) 형태로 두고
 *   유저 공간에서 0번 인덱스에 설정 값을 써 넣는 식으로 사용
 */
struct io_cfg {
  __u32 target_tgid;   // 특정 프로세스만 추적 (0이면 무시)
  __u32 target_dev;    // 특정 블록 디바이스만 추적 (0이면 무시)

  __u32 enable_fs;     // FS 계층 이벤트 on/off
  __u32 enable_jbd2;   // JBD2 이벤트 on/off

  __u32 enable_blk;    // Block 계층 이벤트 on/off
  __u32 enable_nvme;   // NVMe 계층 이벤트 on/off

  __u32 min_bytes;     // 이 크기 미만 I/O는 무시 (0이면 무제한)
  __u32 pad0;
};

#endif /* IO_TRACER_H */

