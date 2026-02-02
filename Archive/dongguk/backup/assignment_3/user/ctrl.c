#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <errno.h>
#include <string.h>         // strerror
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/resource.h>   // setrlimit, RLIMIT_MEMLOCK
#include <linux/limits.h>
#include <limits.h>         // ULLONG_MAX
#include <bpf/libbpf.h>

#include "../include/common.h"
#include "../bpf/trace_block.skel.h"

#define PIN_ROOT "/sys/fs/bpf/ebpf_proj"
#define PIN_RING PIN_ROOT "/ringbuf"

static volatile sig_atomic_t stop;

/* ---- simple stats containers (형태만 유지) ---- */
struct stats64 {
    unsigned long long cnt;
    unsigned long long total;  // ns
    unsigned long long min;    // ns
    unsigned long long max;    // ns
};

struct qdepth_stats {
    long long last;
    long long min;
    long long max;
};

struct app_ctx {
    FILE *out_fp;                // NDJSON 파일 (옵션)
    const char *out_path;        // 파일 경로
    unsigned long print_every;   // 1/N 샘플링 (stdout)
    unsigned long long seen;     // 처리 이벤트 수

    // 누적 통계 (latency)
    struct stats64 st_q;     // queue latency
    struct stats64 st_dev;   // device latency
    struct stats64 st_tot;   // total latency

    // 큐깊이 개요
    struct qdepth_stats st_qd;
};

/* ---- stats helpers (형태 유지) ---- */
static void stats_init(struct stats64 *s) {
    s->cnt = 0;
    s->total = 0;
    s->min = ULLONG_MAX;
    s->max = 0;
}
static inline void stats_add(struct stats64 *s, unsigned long long v_ns) {
    s->cnt++;
    s->total += v_ns;
    if (v_ns < s->min) s->min = v_ns;
    if (v_ns > s->max) s->max = v_ns;
}
static void print_stats(const char *name, const struct stats64 *s) {
    unsigned long long avg = (s->cnt ? s->total / s->cnt : 0);
    unsigned long long minv = (s->cnt ? s->min : 0);
    unsigned long long maxv = (s->cnt ? s->max : 0);
    printf("%s: count=%llu, total_ns=%llu, min_ns=%llu, max_ns=%llu, avg_ns=%llu\n",
           name,
           (unsigned long long)s->cnt,
           (unsigned long long)s->total,
           (unsigned long long)minv,
           (unsigned long long)maxv,
           (unsigned long long)avg);
}

static void print_summary(const struct app_ctx *app) {
    puts("\n[summary]");
    print_stats("queue latency   (insert->issue)\n", &app->st_q);
    print_stats("device latency  (issue->complete)\n", &app->st_dev);
    print_stats("total  latency  (insert->complete)\n", &app->st_tot);
    printf("qdepth snapshot: last=%lld, min=%lld, max=%lld\n",
           (long long)app->st_qd.last,
           (long long)app->st_qd.min,
           (long long)app->st_qd.max);
}

/* ---- process/global setup (형태 유지) ---- */
static void on_sigint(int signo) { (void)signo; stop = 1; }

static int ensure_dir(const char *path)
{
    if (!path || !*path) return -EINVAL;
    if (!mkdir(path, 0755) || errno == EEXIST) return 0;
    return -errno;
}

/* bpffs mount hint */
static void hint_mount_bpffs(void)
{
    struct stat st;
    if (stat("/sys/fs/bpf", &st) == 0 && S_ISDIR(st.st_mode)) return;
    fprintf(stderr, "WARN: /sys/fs/bpf not found. Mount bpffs:\n");
    fprintf(stderr, "      sudo mkdir -p /sys/fs/bpf && sudo mount -t bpf bpf /sys/fs/bpf\n");
}

/* memlock bump */
static void bump_memlock_rlimit(void)
{
    struct rlimit r = { RLIM_INFINITY, RLIM_INFINITY };
    if (setrlimit(RLIMIT_MEMLOCK, &r) != 0) {
        perror("setrlimit(RLIMIT_MEMLOCK)");
    }
}

/* ---- usage (형태 유지) ---- */
static void usage(const char *prog)
{
    fprintf(stderr,
        "Usage: sudo %s [-o output.ndjson] [-n N]\n"
        "  -o FILE   Save ALL events to FILE in NDJSON (1 JSON per line)\n"
        "  -n N      Print only every 1/N events to stdout (default: 1 = print all)\n",
        prog);
}

/* ---- NDJSON writer (형태만 유지, 이벤트 타입만 교체) ---- */
static void write_event_json(FILE *fp, const struct event *e)
{
    switch (e->h.type) {
    case EVENT_RQ_INSERT:
        fprintf(fp,
            "{\"type\":\"rq_insert\",\"ts_ns\":%llu,\"cpu\":%u,"
            "\"rq\":%llu,\"dev\":%llu,\"size\":%llu,"
            "\"pid\":%u,\"tgid\":%u,\"flags\":%u}\n",
            (unsigned long long)e->h.ts_ns,
            e->h.cpu,
            (unsigned long long)e->d.rq_insert.rq_ptr,
            (unsigned long long)e->d.rq_insert.dev,
            (unsigned long long)e->d.rq_insert.size_bytes,
            e->d.rq_insert.pid,
            e->d.rq_insert.tgid,
            e->d.rq_insert.flags);
        break;

    case EVENT_RQ_ISSUE:
        fprintf(fp,
            "{\"type\":\"rq_issue\",\"ts_ns\":%llu,\"cpu\":%u,"
            "\"rq\":%llu,\"dev\":%llu,\"size\":%llu,"
            "\"pid\":%u,\"tgid\":%u,\"flags\":%u}\n",
            (unsigned long long)e->h.ts_ns,
            e->h.cpu,
            (unsigned long long)e->d.rq_issue.rq_ptr,
            (unsigned long long)e->d.rq_issue.dev,
            (unsigned long long)e->d.rq_issue.size_bytes,
            e->d.rq_issue.pid,
            e->d.rq_issue.tgid,
            e->d.rq_issue.flags);
        break;

    case EVENT_RQ_COMPLETE:
        fprintf(fp,
            "{\"type\":\"rq_complete\",\"ts_ns\":%llu,\"cpu\":%u,"
            "\"rq\":%llu,\"dev\":%llu,\"bytes_done\":%llu,"
            "\"status\":%u,\"lat_q_ns\":%llu,\"lat_dev_ns\":%llu,\"lat_tot_ns\":%llu}\n",
            (unsigned long long)e->h.ts_ns,
            e->h.cpu,
            (unsigned long long)e->d.rq_complete.rq_ptr,
            (unsigned long long)e->d.rq_complete.dev,
            (unsigned long long)e->d.rq_complete.bytes_done,
            e->d.rq_complete.status,
            (unsigned long long)e->d.rq_complete.lat_queue_ns,
            (unsigned long long)e->d.rq_complete.lat_dev_ns,
            (unsigned long long)e->d.rq_complete.lat_total_ns);
        break;

    case EVENT_QDEPTH_SNAP:
        fprintf(fp,
            "{\"type\":\"qdepth\",\"ts_ns\":%llu,\"cpu\":%u,"
            "\"dev\":%llu,\"depth\":%lld,\"interval_ns\":%llu}\n",
            (unsigned long long)e->h.ts_ns,
            e->h.cpu,
            (unsigned long long)e->d.qdepth.dev,
            (long long)e->d.qdepth.depth,
            (unsigned long long)e->d.qdepth.interval_ns);
        break;

    default:
        /* unknown -> skip */
        break;
    }
}

/* ---- ringbuf handler (형태 유지: 파일 저장, stdout 샘플링, 통계 누적) ---- */
static int handle_event(void *ctx, void *data, size_t len)
{
    if (len < sizeof(struct event)) return 0;

    struct app_ctx *app = (struct app_ctx *)ctx;
    const struct event *e = (const struct event *)data;

    app->seen++;

    /* 통계 업데이트 (latency 중심: COMPLETE에만 누적) */
    if (e->h.type == EVENT_RQ_COMPLETE) {
        if (e->d.rq_complete.lat_queue_ns)
            stats_add(&app->st_q,   (unsigned long long)e->d.rq_complete.lat_queue_ns);
        if (e->d.rq_complete.lat_dev_ns)
            stats_add(&app->st_dev, (unsigned long long)e->d.rq_complete.lat_dev_ns);
        if (e->d.rq_complete.lat_total_ns)
            stats_add(&app->st_tot, (unsigned long long)e->d.rq_complete.lat_total_ns);
    } else if (e->h.type == EVENT_QDEPTH_SNAP) {
        long long d = (long long)e->d.qdepth.depth;
        app->st_qd.last = d;
        if (app->seen == 1) {
            app->st_qd.min = d;
            app->st_qd.max = d;
        } else {
            if (d < app->st_qd.min) app->st_qd.min = d;
            if (d > app->st_qd.max) app->st_qd.max = d;
        }
    }

    /* 1) 파일 저장 (전부) */
    if (app->out_fp) {
        write_event_json(app->out_fp, e);
        fflush(app->out_fp);
    }

    /* 2) 표준출력: 1/N 샘플링 */
    unsigned long N = (app->print_every == 0 ? 1 : app->print_every);
    if (app->seen % N == 0) {
        write_event_json(stdout, e);
        fflush(stdout);
    }

    return 0;
}

/* ---- main (형태 유지: 옵션 파싱 → skel open/load/attach → ring poll → summary) ---- */
int main(int argc, char **argv)
{
    int err = 0;
    struct app_ctx app;
    memset(&app, 0, sizeof(app));
    app.print_every = 1;
    stats_init(&app.st_q);
    stats_init(&app.st_dev);
    stats_init(&app.st_tot);
    app.st_qd.last = 0;
    app.st_qd.min  = 0;
    app.st_qd.max  = 0;

    /* 옵션 파싱 */
    int opt;
    while ((opt = getopt(argc, argv, "o:n:h")) != -1) {
        switch (opt) {
        case 'o':
            app.out_path = optarg;
            break;
        case 'n': {
            char *endp = NULL;
            long v = strtol(optarg, &endp, 10);
            if (!optarg[0] || (endp && *endp) || v <= 0) {
                fprintf(stderr, "Invalid -n value: %s\n", optarg);
                return 1;
            }
            app.print_every = (unsigned long)v;
            break;
        }
        case 'h':
        default:
            usage(argv[0]);
            return (opt == 'h') ? 0 : 1;
        }
    }

    /* NDJSON 파일 오픈 (선택) */
    if (app.out_path) {
        app.out_fp = fopen(app.out_path, "a"); // append
        if (!app.out_fp) {
            fprintf(stderr, "Failed to open output file %s: %s\n",
                    app.out_path, strerror(errno));
            return 1;
        }
        setvbuf(app.out_fp, NULL, _IOLBF, 0);  // line-buffering
    }

    libbpf_set_strict_mode(LIBBPF_STRICT_ALL);
    libbpf_set_print(NULL);

    struct sigaction sa = { .sa_handler = on_sigint };
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    bump_memlock_rlimit();
    hint_mount_bpffs();

    /* ensure /sys/fs/bpf exists */
    err = ensure_dir("/sys/fs/bpf");
    if (err) { fprintf(stderr, "bpffs missing? %s\n", strerror(-err)); goto out; }
    err = ensure_dir(PIN_ROOT);
    if (err) { fprintf(stderr, "mkdir %s: %s\n", PIN_ROOT, strerror(-err)); goto out; }

    /* open skel */
    struct trace_block_bpf *skel = NULL;
    skel = trace_block_bpf__open();
    if (!skel) {
        fprintf(stderr, "open skel failed\n");
        err = -1;
        goto out;
    }

    /* share ringbuf via pin (형태상 동일 경로 사용) */
    bpf_map__set_pin_path(skel->maps.events_rb, PIN_RING);

    /* load */
    err = trace_block_bpf__load(skel);
    if (err) { fprintf(stderr, "load skel: %d\n", err); goto out; }

    /* attach */
    err = trace_block_bpf__attach(skel);
    if (err) { fprintf(stderr, "attach skel: %d\n", err); goto out; }

    /* ring buffer */
    {
        int ring_fd = bpf_map__fd(skel->maps.events_rb);
        if (ring_fd < 0) { fprintf(stderr, "ringbuf fd: %d\n", ring_fd); goto out; }

        struct ring_buffer *rb = ring_buffer__new(ring_fd, handle_event, &app, NULL);
        if (!rb) { fprintf(stderr, "ring_buffer__new failed\n"); goto out; }

        while (!stop) {
            err = ring_buffer__poll(rb, 200);
            if (err == -EINTR) break;
            if (err < 0) {
                fprintf(stderr, "ring_buffer__poll: %d\n", err);
                break;
            }
        }
        ring_buffer__free(rb);
    }

    /* 종료 요약 */
    print_summary(&app);

out:
    if (skel) trace_block_bpf__destroy(skel);
    if (app.out_fp) fclose(app.out_fp);
    return (err < 0) ? 1 : 0;
}

