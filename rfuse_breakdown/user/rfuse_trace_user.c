// user/rfuse_trace_user.c

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

#include <bpf/libbpf.h>

#include "rfuse_common.h"
#include "rfuse_trace.skel.h"

static volatile sig_atomic_t exiting = 0;
static FILE *outf;
static uint64_t event_count;

static void handle_sigint(int sig)
{
    exiting = 1;
    if (outf)
        fflush(outf);
}

static int parse_u64_hex(const char *s, unsigned long long *out)
{
    char *end = NULL;
    errno = 0;
    unsigned long long v = strtoull(s, &end, 16);
    if (errno != 0 || end == s || *end != '\0')
        return -1;
    *out = v;
    return 0;
}

static int handle_event(void *ctx, void *data, size_t len)
{
    const struct rfuse_req_event *e = data;
    const char *opname = rfuse_opcode_to_str(e->opcode);

    if (!outf)
        return 0;

    /* ns → us 변환 */
    unsigned long long ts_us        = e->ts_ns / 1000;
    unsigned long long alloc_us     = e->alloc_delay_ns / 1000;
    unsigned long long q_us         = e->queue_delay_ns / 1000;
    unsigned long long d_us         = e->daemon_delay_ns / 1000;
    unsigned long long r_us         = e->response_delay_ns / 1000;
    unsigned long long copy_from_us = e->copy_from_latency_ns / 1000;
    unsigned long long copy_to_us   = e->copy_to_latency_ns / 1000;

    fprintf(outf,
            "%llu,%d,%u,%llu,%u,%s,%u,%s,%llu,%llu,%llu,%llu,%llu,%llu\n",
            ts_us,
            e->riq_id,
            e->req_index,
            (unsigned long long)e->unique,
            e->opcode,
            opname,
            e->pid,
            e->comm,
            alloc_us,
            q_us,
            d_us,
            r_us,
            copy_from_us,
            copy_to_us);

    /* 100개마다 flush */
    event_count++;
    if (event_count % 100 == 0)
        fflush(outf);

    return 0;
}

/*
 * func_name 우선으로 uprobe/uretprobe attach 시도,
 * 실패하면 addr_override(offset)로 재시도.
 */
static struct bpf_link *
attach_uprobe_with_fallback(struct bpf_program *prog,
                            const char *binary_path,
                            const char *func_name,
                            bool is_retprobe,
                            unsigned long long addr_override)
{
    struct bpf_link *link;
    int err;

    /* 1) 심볼 이름 기반 attach (옵션: func_name 있을 때만) */
    if (func_name && func_name[0]) {
        LIBBPF_OPTS(bpf_uprobe_opts, opts,
            .retprobe  = is_retprobe,
            .func_name = func_name,
        );

        link = bpf_program__attach_uprobe_opts(prog,
                                               -1,          /* pid = all */
                                               binary_path,
                                               0,           /* func_offset (func_name 사용) */
                                               &opts);
        err = libbpf_get_error(link);
        if (!err)
            return link;

        fprintf(stderr,
                "auto u%sprobe attach failed for %s (func %s): %s\n",
                is_retprobe ? "ret" : "",
                binary_path,
                func_name,
                strerror(-err));
    }

    /* 2) 주소 override 있으면 offset 기반 attach */
    if (addr_override) {
        link = bpf_program__attach_uprobe(prog,
                                          is_retprobe,
                                          -1,
                                          binary_path,
                                          (size_t)addr_override);
        err = libbpf_get_error(link);
        if (!err)
            return link;

        fprintf(stderr,
                "addr override u%sprobe attach failed for %s (0x%llx): %s\n",
                is_retprobe ? "ret" : "",
                binary_path,
                (unsigned long long)addr_override,
                strerror(-err));
    }

    return NULL;
}


int main(int argc, char **argv)
{
    const char *daemon_path;
    const char *out_path;
    struct rfuse_trace_bpf *skel;
    struct ring_buffer *rb = NULL;
    struct bpf_link *link_read = NULL;
    struct bpf_link *link_send = NULL;
    struct bpf_link *link_copy_from = NULL;
    struct bpf_link *link_copy_to = NULL;
    int err = 0;

    /* addr overrides (optional) */
    unsigned long long addr_read = 0;
    unsigned long long addr_send = 0;
    unsigned long long addr_copy_from = 0;
    unsigned long long addr_copy_to = 0;
    
    if (argc < 3) {
        fprintf(stderr,
                "Usage: %s /path/to/rfuse_daemon.so /path/to/output.csv "
                "[--addr-read=0x.. --addr-send=0x.. "
                "--addr-copy-from=0x.. --addr-copy-to=0x..]\n",
                argv[0]);
        return 1;
    }

    daemon_path = argv[1];
    out_path    = argv[2];

    outf = fopen(out_path, "w");
    if (!outf) {
        perror("fopen output csv");
        return 1;
    }

    /* CSV header 출력 */
    fprintf(outf,
            "ts_ns,riq_id,req_index,unique,opcode,opcode_name,pid,comm,"
            "alloc_block_us,queue_us,daemon_us,response_us,copy_from_us,copy_to_us\n");
    fflush(outf);

    /* 간단한 옵션 파서 */
    for (int i = 3; i < argc; i++) {
        const char *arg = argv[i];
        const char *p;

        if (strncmp(arg, "--addr-read=", 12) == 0) {
            p = arg + 12;
            if (parse_u64_hex(p, &addr_read)) {
                fprintf(stderr, "invalid --addr-read: %s\n", p);
                return 1;
            }
        } else if (strncmp(arg, "--addr-send=", 12) == 0) {
            p = arg + 12;
            if (parse_u64_hex(p, &addr_send)) {
                fprintf(stderr, "invalid --addr-send: %s\n", p);
                return 1;
            }
        } else if (strncmp(arg, "--addr-copy-from=", 17) == 0) {
            p = arg + 17;
            if (parse_u64_hex(p, &addr_copy_from)) {
                fprintf(stderr, "invalid --addr-copy-from: %s\n", p);
                return 1;
            }
        } else if (strncmp(arg, "--addr-copy-to=", 15) == 0) {
            p = arg + 15;
            if (parse_u64_hex(p, &addr_copy_to)) {
                fprintf(stderr, "invalid --addr-copy-to: %s\n", p);
                return 1;
            }
        } else {
            fprintf(stderr, "unknown option: %s\n", arg);
            return 1;
        }
    }

    libbpf_set_strict_mode(LIBBPF_STRICT_ALL);

    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);

    skel = rfuse_trace_bpf__open_and_load();
    if (!skel) {
        fprintf(stderr, "failed to open/load BPF skeleton\n");
        return 1;
    }

    /* kprobe만 수동 attach (uprobe는 아래에서 직접 붙임) */
    struct bpf_link *k_link_queue = NULL;
    struct bpf_link *k_link_end   = NULL;
    struct bpf_link *k_link_req   = NULL;

    /* 추가 kprobe/rfuse_get_req */
    k_link_req = bpf_program__attach(skel->progs.kp_rfuse_get_req);
    err = libbpf_get_error(k_link_req);
    if (err) {
        k_link_req = NULL;
        fprintf(stderr, "failed to attach kprobe rfuse_get_req: %s\n",
                strerror(-err));
        goto cleanup;
    }

    /* 1) kprobe/rfuse_queue_request */
    k_link_queue = bpf_program__attach(skel->progs.kp_rfuse_submit_request);
    err = libbpf_get_error(k_link_queue);
    if (err) {
        k_link_queue = NULL;
        fprintf(stderr, "failed to attach kprobe rfuse_submit_request: %s\n",
            strerror(-err));
        goto cleanup;
    }

    /* 2) kprobe/rfuse_request_end */
    k_link_end = bpf_program__attach(skel->progs.kp_rfuse_request_end);
    err = libbpf_get_error(k_link_end);
    if (err) {
        k_link_end = NULL;
        fprintf(stderr, "failed to attach kprobe rfuse_request_end: %s\n",
            strerror(-err));
        goto cleanup;
    }


    /* ========== UPROBES ========== */

    /* 1) rfuse_read_pending_head (entry) */
    link_read = attach_uprobe_with_fallback(
        skel->progs.up_rfuse_read_request,
        daemon_path,
        "rfuse_read_request",
        false,
        addr_read);
    if (!link_read) {
        fprintf(stderr, "failed to attach uprobe rfuse_read_request\n");
        err = -1;
        goto cleanup;
    }

    /* 3) rfuse_send_reply_iov_nofree(fuse_req_t u_req, int error) */
    link_send = attach_uprobe_with_fallback(
        skel->progs.up_rfuse_send_result,
        daemon_path,
        "rfuse_send_result",
        false,
        addr_send);
    if (!link_send) {
        fprintf(stderr, "failed to attach uprobe rfuse_send_result\n");
        err = -1;
        goto cleanup;
    }

    /* 4) rfuse_copy_from_payload_begin(opcode, unique, latency_ns) */
    link_copy_from = attach_uprobe_with_fallback(
        skel->progs.up_rfuse_copy_from_payload_begin_end,
        daemon_path,
        "rfuse_copy_from_payload_begin_end",
        false,
        addr_copy_from);
    if (!link_copy_from) {
        fprintf(stderr, "failed to attach uprobe rfuse_copy_from_payload_begin_end\n");
        err = -1;
        goto cleanup;
    }

    /* 5) rfuse_copy_to_payload_begin(opcode, unique, latency_ns) */
    link_copy_to = attach_uprobe_with_fallback(
        skel->progs.up_rfuse_copy_to_payload_begin_end,
        daemon_path,
        "rfuse_copy_to_payload_begin_end",
        false,
        addr_copy_to);
    if (!link_copy_to) {
        fprintf(stderr, "failed to attach uprobe rfuse_copy_to_payload_begin_end\n");
        err = -1;
        goto cleanup;
    }

    /* ========== RING BUFFER ========== */

    rb = ring_buffer__new(bpf_map__fd(skel->maps.rfuse_events),
                          handle_event, NULL, NULL);
    if (!rb) {
        fprintf(stderr, "failed to create ring buffer\n");
        err = -1;
        goto cleanup;
    }

    printf("ts_ns,riq_id,req_index,unique,opcode,pid,comm,"
           "queue_ns,daemon_ns,response_ns,copy_from_ns,copy_to_ns\n");

    while (!exiting) {
        err = ring_buffer__poll(rb, 100 /* ms */);
        if (err == -EINTR)
            break;
        if (err < 0) {
            fprintf(stderr, "ring_buffer__poll failed: %d\n", err);
            break;
        }
    }

cleanup:
    if (k_link_req)
        bpf_link__destroy(k_link_req);
    if (link_read)
        bpf_link__destroy(link_read);
    if (link_send)
        bpf_link__destroy(link_send);
    if (link_copy_from)
        bpf_link__destroy(link_copy_from);
    if (link_copy_to)
        bpf_link__destroy(link_copy_to);
    if (k_link_queue)
        bpf_link__destroy(k_link_queue);
    if (k_link_end)
        bpf_link__destroy(k_link_end);
    if (outf)
        fclose(outf);

    ring_buffer__free(rb);
    rfuse_trace_bpf__destroy(skel);
    return err != 0;
}

