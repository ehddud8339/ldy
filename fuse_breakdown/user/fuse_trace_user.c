// user/fuse_trace_user.c

#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <bpf/libbpf.h>

#include "fuse_trace.skel.h"
#include "fuse_trace_common.h"

/* ---------- CSV 파일 포인터 ---------- */
static FILE *csv_fp = NULL;

/* ---------- [수정] PID별 Alloc 시작 시간 저장용 맵 ---------- */
#define MAX_PIDS 4194304
static uint64_t pid_alloc_map[MAX_PIDS]; /* Key: PID, Value: Start Timestamp */

/* ---------- type / opcode 이름 ---------- */
static const char *type_name(uint32_t type)
{
    switch (type) {
    case EVT_ALLOC_START: return "AllocStart"; /* [추가] */
    case EVT_QUEUE: return "Queuing";
    case EVT_RECV:  return "Receive";
    case EVT_SEND:  return "Send";
    case EVT_END:   return "End req";
    default:        return "Unknown";
    }
}

static const char *opcode_name(uint32_t opcode)
{
    switch (opcode) {
    case 1:  return "LOOKUP";
    case 2:  return "FORGET";
    case 3:  return "GETATTR";
    case 4:  return "SETATTR";
    case 5:  return "READLINK";
    case 6:  return "SYMLINK";
    case 8:  return "MKNOD";
    case 9:  return "MKDIR";
    case 10: return "UNLINK";
    case 11: return "RMDIR";
    case 12: return "RENAME";
    case 13: return "LINK";
    case 14: return "OPEN";
    case 15: return "READ";
    case 16: return "WRITE";
    case 17: return "STATFS";
    case 18: return "RELEASE";
    case 20: return "FSYNC";
    case 21: return "SETXATTR";
    case 22: return "GETXATTR";
    case 23: return "LISTXATTR";
    case 24: return "REMOVEXATTR";
    case 25: return "FLUSH";
    case 26: return "INIT";
    case 27: return "OPENDIR";
    case 28: return "READDIR";
    case 29: return "RELEASEDIR";
    case 30: return "FSYNCDIR";
    case 31: return "GETLK";
    case 32: return "SETLK";
    case 33: return "SETLKW";
    case 34: return "ACCESS";
    case 35: return "CREATE";
    case 36: return "INTERRUPT";
    case 37: return "BMAP";
    case 38: return "DESTROY";
    case 39: return "IOCTL";
    case 40: return "POLL";
    case 41: return "NOTIFY_REPLY";
    case 42: return "BATCH_FORGET";
    case 43: return "FALLOCATE";
    case 44: return "READDIRPLUS";
    case 45: return "RENAME2";
    case 46: return "LSEEK";
    case 47: return "COPY_FILE_RANGE";
    case 4096: return "CUSE_INIT";
    default: return "UNKNOWN";
    }
}

/* ---------- unique 별 timestamp 저장 ---------- */

struct pending_ts {
    uint64_t unique;
    uint64_t alloc_delay_ns; /* [추가] Alloc & Block 지연 시간 */
    uint64_t queue_ts;
    uint64_t recv_ts;
    uint64_t send_ts;
    uint64_t end_ts;
    uint32_t opcode;    /* QUEUE에서 세팅, 나머지는 이걸 재사용 */
};

#define MAX_PENDING 4194304
static struct pending_ts pending[MAX_PENDING];

static struct pending_ts *get_slot(uint64_t unique)
{
    return &pending[unique % MAX_PENDING];
}

static volatile sig_atomic_t exiting = 0;
static void sig_handler(int sig) { exiting = 1; }

/* ---------- ringbuf 콜백 ---------- */

static int handle_event(void *ctx, void *data, size_t len)
{
    const struct event *e = data;
    struct pending_ts *p = NULL;

    /* [추가] 1. 할당 시작 이벤트 (Unique ID 없음, PID 사용) */
    if (e->type == EVT_ALLOC_START) {
        if (e->pid < MAX_PIDS) {
            pid_alloc_map[e->pid] = e->ts_ns;
        }
        /* AllocStart는 화면 출력 없이 리턴 (너무 빈번할 수 있음) */
        return 0;
    }

    /* 2. 그 외 이벤트는 Unique ID 기반 처리 */
    p = get_slot(e->unique);

    switch (e->type) {
    case EVT_QUEUE:
        /* 새로운 요청 시작: 슬롯을 초기화하고 ID 등록 */
        memset(p, 0, sizeof(*p)); // 슬롯 깨끗이 비우기 (alloc_delay_ns도 0됨)
        p->unique = e->unique;    // ID 등록
        p->queue_ts = e->ts_ns;
        p->opcode   = e->opcode;

        /* [추가] PID 맵 확인하여 Alloc Latency 계산 */
        if (e->pid < MAX_PIDS && pid_alloc_map[e->pid] != 0) {
            uint64_t start_ts = pid_alloc_map[e->pid];
            if (e->ts_ns > start_ts) {
                p->alloc_delay_ns = e->ts_ns - start_ts;
            }
            pid_alloc_map[e->pid] = 0; /* 사용 후 초기화 */
        }
        break;

    case EVT_RECV:
        /* ID가 다르면(충돌 났거나 덮어쓰여졌으면) 무시 */
        if (p->unique != e->unique) return 0;
        p->recv_ts = e->ts_ns;
        break;
    case EVT_SEND:
        if (p->unique != e->unique) return 0;
        p->send_ts = e->ts_ns;
        break;
    case EVT_END:
        if (p->unique != e->unique) return 0;
        p->end_ts = e->ts_ns;
        break;
    default:
        break;
    }

    const char *opname = opcode_name(p->opcode ? p->opcode : e->opcode);
    printf("[%s] Op: %s, Unique: %llu, ts: %llu ns\n",
           type_name(e->type),
           opname,
           (unsigned long long)e->unique,
           (unsigned long long)e->ts_ns);

    /* 네 지점이 모두 채워졌으면 duration 계산 + 출력 + CSV 기록 */
    if (p->queue_ts && p->recv_ts && p->send_ts && p->end_ts) {
        /* [추가] 단위 변환 */
        uint64_t alloc_us = p->alloc_delay_ns / 1000;

        uint64_t q2r_ns = p->recv_ts - p->queue_ts; // queuing + copy
        uint64_t r2s_ns = p->send_ts - p->recv_ts;  // daemon latency
        uint64_t s2e_ns = p->end_ts  - p->send_ts;  // response latency

        uint64_t q2r_us = q2r_ns / 1000;
        uint64_t r2s_us = r2s_ns / 1000;
        uint64_t s2e_us = s2e_ns / 1000;

        printf("=================================\n");
        printf("Alloc & Block Delay  : %llu us\n", (unsigned long long)alloc_us);
        printf("Queuing + Copy Delay : %llu us\n", (unsigned long long)q2r_us);
        printf("Daemon Delay         : %llu us\n", (unsigned long long)r2s_us);
        printf("Response Delay       : %llu us\n", (unsigned long long)s2e_us);
        printf("=================================\n");

        /* ---- CSV 파일에 기록 ----
         * 형식: Unique,Op,Alloc(us),Queuing(us),Daemon(us),Response(us)
         */
        if (csv_fp) {
            fprintf(csv_fp, "%llu,%llu,%s,%llu,%llu,%llu,%llu\n",
                    (unsigned long long)p->queue_ts,
                    (unsigned long long)e->unique,
                    opname,
                    (unsigned long long)alloc_us, /* [추가] */
                    (unsigned long long)q2r_us,
                    (unsigned long long)r2s_us,
                    (unsigned long long)s2e_us);
            fflush(csv_fp);
        }

        /* slot 초기화 */
        memset(p, 0, sizeof(*p));
    }

    return 0;
}

/* ---------- main ---------- */

int main(int argc, char **argv)
{
    struct fuse_trace_bpf *skel = NULL;
    struct ring_buffer *rb = NULL;
    int err;

    if (argc < 4) {
        fprintf(stderr,
            "Usage: %s <target_so_or_bin> <recv_offset_hex> <send_offset_hex> [csv_path]\n",
            argv[0]);
        return 1;
    }

    const char *target   = argv[1];
    unsigned long long recv_off = strtoull(argv[2], NULL, 0);
    unsigned long long send_off = strtoull(argv[3], NULL, 0);

    /* 4번째 인자 (= CSV 경로) 가 있으면 사용하고,
       없으면 기본값 fuse_trace.csv */
    const char *csv_path = (argc >= 5) ? argv[4] : "fuse_trace.csv";

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    libbpf_set_strict_mode(LIBBPF_STRICT_ALL);

    /* ---- CSV 파일 오픈 ---- */
    csv_fp = fopen(csv_path, "w");
    if (!csv_fp) {
        perror("fopen csv_path");
    } else {
        /* [수정] 헤더에 alloc_us 추가 */
        fprintf(csv_fp, "ts,unique,op,alloc_us,queuing_us,daemon_us,response_us\n");
        fflush(csv_fp);
    }

    skel = fuse_trace_bpf__open();
    if (!skel) {
        fprintf(stderr, "fuse_trace_bpf__open failed\n");
        return 1;
    }

    err = fuse_trace_bpf__load(skel);
    if (err) {
        fprintf(stderr, "fuse_trace_bpf__load failed: %d\n", err);
        goto cleanup;
    }

    /* kprobes attach */
    err = fuse_trace_bpf__attach(skel);
    if (err) {
        fprintf(stderr, "auto attach failed: %d\n", err);
        goto cleanup;
    }

    /* uprobe: receive_buf */
    skel->links.up_receive_buf =
        bpf_program__attach_uprobe(
            skel->progs.up_receive_buf,
            false,
            -1,
            target,
            recv_off
        );
    if (!skel->links.up_receive_buf) {
        fprintf(stderr, "attach uprobe(receive_buf) failed (off=0x%llx)\n",
                recv_off);
        goto cleanup;
    }

    /* uprobe: fuse_send_msg */
    skel->links.up_fuse_send_msg =
        bpf_program__attach_uprobe(
            skel->progs.up_fuse_send_msg,
            false,
            -1,
            target,
            send_off
        );
    if (!skel->links.up_fuse_send_msg) {
        fprintf(stderr, "attach uprobe(fuse_send_msg) failed (off=0x%llx)\n",
                send_off);
        goto cleanup;
    }

    rb = ring_buffer__new(bpf_map__fd(skel->maps.events),
                          handle_event, NULL, NULL);
    if (!rb) {
        fprintf(stderr, "ring_buffer__new failed\n");
        goto cleanup;
    }

    printf("Tracing...\n");
    printf("  target: %s\n", target);
    printf("  receive_buf offset : 0x%llx\n", recv_off);
    printf("  fuse_send_msg offset: 0x%llx\n", send_off);
    printf("  CSV output: %s\n", csv_path);
    printf("Press Ctrl+C to stop.\n");

    while (!exiting) {
        err = ring_buffer__poll(rb, 100);
        if (err < 0 && errno != EINTR)
            break;
    }

cleanup:
    ring_buffer__free(rb);
    fuse_trace_bpf__destroy(skel);
    if (csv_fp)
        fclose(csv_fp);
    return 0;
}

