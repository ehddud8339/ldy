// fuse_req_lat_user.c
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <inttypes.h>

#include <bpf/libbpf.h>

#include "fuse_req_lat_user.h"
#include "fuse_opname.h"
#include "fuse_req_lat.bpf.skel.h"

static volatile sig_atomic_t exiting = 0;

static void handle_sigint(int sig)
{
    (void)sig;
    exiting = 1;
}

/* 원하는 필터: READ/WRITE 만 보고 싶으면 이 플래그로 제어 */
static int filter_read = 0;
static int filter_write = 0;

/* ringbuf 콜백 */
static int handle_event(void *ctx, void *data, size_t data_sz)
{
    (void)ctx;

    if (data_sz < sizeof(struct fuse_req_event)) {
        fprintf(stderr, "event size mismatch: got %zu, expect %zu\n",
                data_sz, sizeof(struct fuse_req_event));
        return 0;
    }

    struct fuse_req_event *e = data;

    if (filter_read && e->opcode != FUSE_READ)
        return 0;
    if (filter_write && e->opcode != FUSE_WRITE)
        return 0;

    const char *opname = fuse_opcode_name(e->opcode);

    printf("req[%8" PRIu64 "]: opcode=%" PRIu32 " (%s) len=%" PRIu32
           " unique=%" PRIu64 " err=%" PRId32 "\n",
           e->seq, e->opcode, opname, e->len, e->unique, e->err);

    printf("    enqueue_ts=%" PRIu64 " ns  "
           "dequeue_ts=%" PRIu64 " ns  "
           "done_ts=%" PRIu64 " ns\n",
           e->enqueue_ts_ns, e->dequeue_ts_ns, e->done_ts_ns);

    printf("    queueing_ns=%" PRIu64 " ns\n",
           e->queue_wait_ns);

    printf("    daemon_ns  =%" PRIu64 " ns\n",
           e->daemon_ns);

    /* flush 해두는 게 로그 정렬에 유리 */
    fflush(stdout);

    return 0;
}

static void usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s [--read] [--write]\n"
            "  --read   : READ(FUSE_READ) 요청만 출력\n"
            "  --write  : WRITE(FUSE_WRITE) 요청만 출력\n"
            "  (둘 다 없으면 모든 opcode 출력)\n",
            prog);
}

int main(int argc, char **argv)
{
    int err;
    struct fuse_req_lat_bpf *skel = NULL;
    struct ring_buffer *rb = NULL;

    /* 간단한 옵션 파싱 */
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--read"))
            filter_read = 1;
        else if (!strcmp(argv[i], "--write"))
            filter_write = 1;
        else {
            usage(argv[0]);
            return 1;
        }
    }

    libbpf_set_strict_mode(LIBBPF_STRICT_ALL);

    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);

    /* 1) skeleton 열기 */
    skel = fuse_req_lat_bpf__open();
    if (!skel) {
        fprintf(stderr, "failed to open BPF skeleton\n");
        return 1;
    }

    /* 필요하다면 로딩 전에 map 옵션 / attach 옵션 조정 가능 */

    /* 2) 로드 + attach */
    err = fuse_req_lat_bpf__load(skel);
    if (err) {
        fprintf(stderr, "failed to load BPF skeleton: %d\n", err);
        goto cleanup;
    }

    err = fuse_req_lat_bpf__attach(skel);
    if (err) {
        fprintf(stderr, "failed to attach BPF programs: %d\n", err);
        goto cleanup;
    }

    /* 3) ring buffer 생성 */
    rb = ring_buffer__new(bpf_map__fd(skel->maps.events),
                          handle_event, NULL, NULL);
    if (!rb) {
        fprintf(stderr, "failed to create ring buffer\n");
        goto cleanup;
    }

    printf("fuse_req_lat tracer is running.\n");
    printf("Press Ctrl-C to exit.\n");

    /* 4) poll loop */
    while (!exiting) {
        err = ring_buffer__poll(rb, 100 /* ms */);
        if (err == -EINTR) {
            /* 시그널로 깬 경우 */
            break;
        } else if (err < 0) {
            fprintf(stderr, "ring_buffer__poll() failed: %d\n", err);
            break;
        }
        /* err == 0 이면 timeout, 다시 loop */
    }

cleanup:
    ring_buffer__free(rb);
    fuse_req_lat_bpf__destroy(skel);
    return err < 0 ? -err : 0;
}

