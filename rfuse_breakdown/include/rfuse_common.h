// include/rfuse_common.h
#ifndef __RFUSE_COMMON_H
#define __RFUSE_COMMON_H

// =====================
// RFUSE module struct
// =====================
struct fuse_in_header {
	uint32_t	len;
	uint32_t	opcode;
	uint64_t	unique;
	uint64_t	nodeid;
	uint32_t	uid;
	uint32_t	gid;
	uint32_t	pid;
	uint32_t	padding;
};

struct fuse_out_header {
	uint32_t	len;
	int32_t		error;
	uint64_t	unique;
};

struct rfuse_req{
	/** Request input header **/
	struct{
		uint64_t    unique;
		uint64_t    nodeid;
		uint32_t    opcode;
		uint32_t    uid;
		uint32_t    gid;
		uint32_t    pid;
		uint32_t	arg[2];	    // Location of in operation-specific argument
		uint32_t	arglen[2];	// Size of in operation-specific argument
	}in; // 48 

	/** Request output header **/
	struct{
		int32_t     error;
		uint32_t	arg;	// Location of out operation-specific argument
		uint32_t	arglen;	// Size of out operation-specific argument
		uint32_t	padding;	
	}out; // 16

	/** request buffer index **/
	uint32_t index; // 4
	int32_t riq_id;
	/** Request flags, updated with test/set/clear_bit() **/
	unsigned long flags; // 8
};

struct rfuse_iqueue{
	int riq_id;
};

/* =========================
 * FUSE opcode enum & helper
 * ========================= */
enum fuse_opcode {
    FUSE_LOOKUP             = 1,
    FUSE_FORGET             = 2,
    FUSE_GETATTR            = 3,
    FUSE_SETATTR            = 4,
    FUSE_READLINK           = 5,
    FUSE_SYMLINK            = 6,
    FUSE_MKNOD              = 8,
    FUSE_MKDIR              = 9,
    FUSE_UNLINK             = 10,
    FUSE_RMDIR              = 11,
    FUSE_RENAME             = 12,
    FUSE_LINK               = 13,
    FUSE_OPEN               = 14,
    FUSE_READ               = 15,
    FUSE_WRITE              = 16,
    FUSE_STATFS             = 17,
    FUSE_RELEASE            = 18,
    FUSE_FSYNC              = 20,
    FUSE_SETXATTR           = 21,
    FUSE_GETXATTR           = 22,
    FUSE_LISTXATTR          = 23,
    FUSE_REMOVEXATTR        = 24,
    FUSE_FLUSH              = 25,
    FUSE_INIT               = 26,
    FUSE_OPENDIR            = 27,
    FUSE_READDIR            = 28,
    FUSE_RELEASEDIR         = 29,
    FUSE_FSYNCDIR           = 30,
    FUSE_GETLK              = 31,
    FUSE_SETLK              = 32,
    FUSE_SETLKW             = 33,
    FUSE_ACCESS             = 34,
    FUSE_CREATE             = 35,
    FUSE_INTERRUPT          = 36,
    FUSE_BMAP               = 37,
    FUSE_DESTROY            = 38,
    FUSE_IOCTL              = 39,
    FUSE_POLL               = 40,
    FUSE_NOTIFY_REPLY       = 41,
    FUSE_BATCH_FORGET       = 42,
    FUSE_FALLOCATE          = 43,
    FUSE_READDIRPLUS        = 44,
    FUSE_RENAME2            = 45,
    FUSE_LSEEK              = 46,
    FUSE_COPY_FILE_RANGE    = 47,

    /* CUSE specific */
    CUSE_INIT               = 4096,
};

/*
 * 유저 프로그램에서만 사용되는 helper.
 * BPF 쪽은 문자열 필요 없으니까 __BPF_TRACING__ 일 때는 빼버린다.
 */
#ifndef __BPF_TRACING__
static inline const char *rfuse_opcode_to_str(uint32_t opcode)
{
    switch (opcode) {
    case FUSE_LOOKUP:          return "LOOKUP";
    case FUSE_FORGET:          return "FORGET";
    case FUSE_GETATTR:         return "GETATTR";
    case FUSE_SETATTR:         return "SETATTR";
    case FUSE_READLINK:        return "READLINK";
    case FUSE_SYMLINK:         return "SYMLINK";
    case FUSE_MKNOD:           return "MKNOD";
    case FUSE_MKDIR:           return "MKDIR";
    case FUSE_UNLINK:          return "UNLINK";
    case FUSE_RMDIR:           return "RMDIR";
    case FUSE_RENAME:          return "RENAME";
    case FUSE_LINK:            return "LINK";
    case FUSE_OPEN:            return "OPEN";
    case FUSE_READ:            return "READ";
    case FUSE_WRITE:           return "WRITE";
    case FUSE_STATFS:          return "STATFS";
    case FUSE_RELEASE:         return "RELEASE";
    case FUSE_FSYNC:           return "FSYNC";
    case FUSE_SETXATTR:        return "SETXATTR";
    case FUSE_GETXATTR:        return "GETXATTR";
    case FUSE_LISTXATTR:       return "LISTXATTR";
    case FUSE_REMOVEXATTR:     return "REMOVEXATTR";
    case FUSE_FLUSH:           return "FLUSH";
    case FUSE_INIT:            return "INIT";
    case FUSE_OPENDIR:         return "OPENDIR";
    case FUSE_READDIR:         return "READDIR";
    case FUSE_RELEASEDIR:      return "RELEASEDIR";
    case FUSE_FSYNCDIR:        return "FSYNCDIR";
    case FUSE_GETLK:           return "GETLK";
    case FUSE_SETLK:           return "SETLK";
    case FUSE_SETLKW:          return "SETLKW";
    case FUSE_ACCESS:          return "ACCESS";
    case FUSE_CREATE:          return "CREATE";
    case FUSE_INTERRUPT:       return "INTERRUPT";
    case FUSE_BMAP:            return "BMAP";
    case FUSE_DESTROY:         return "DESTROY";
    case FUSE_IOCTL:           return "IOCTL";
    case FUSE_POLL:            return "POLL";
    case FUSE_NOTIFY_REPLY:    return "NOTIFY_REPLY";
    case FUSE_BATCH_FORGET:    return "BATCH_FORGET";
    case FUSE_FALLOCATE:       return "FALLOCATE";
    case FUSE_READDIRPLUS:     return "READDIRPLUS";
    case FUSE_RENAME2:         return "RENAME2";
    case FUSE_LSEEK:           return "LSEEK";
    case FUSE_COPY_FILE_RANGE: return "COPY_FILE_RANGE";
    case CUSE_INIT:            return "CUSE_INIT";
    default:                   return "UNKNOWN";
    }
}
#endif /* !__BPF_TRACING__ */

/* 맵 키 구조체 */
struct rfuse_req_key {
    int riq_id;
    uint64_t unique;
};

/* [수정] 요청 상태 추적용 구조체 */
struct rfuse_req_state {
    uint64_t unique;
    uint32_t opcode;
    uint32_t flags;
    uint64_t ts_queued_ns;
    uint64_t ts_dequeued_ns;
    uint64_t ts_daemon_done_ns;
    uint64_t ts_end_ns;
    uint64_t copy_from_latency_ns;
    uint64_t copy_to_latency_ns;
    uint64_t alloc_delay_ns; /* [추가] Alloc & Block 지연 시간 */
};

/* [수정] User Space로 보낼 이벤트 구조체 */
struct rfuse_req_event {
    uint64_t ts_ns;
    int      riq_id;
    uint32_t req_index; /* ring index if needed */
    uint64_t unique;
    uint32_t opcode;
    uint32_t pid;
    char     comm[16];

    uint64_t queue_delay_ns;
    uint64_t daemon_delay_ns;
    uint64_t response_delay_ns;
    uint64_t copy_from_latency_ns;
    uint64_t copy_to_latency_ns;
    uint64_t alloc_delay_ns; /* [추가] */
};

#endif /* __RFUSE_COMMON_H */
