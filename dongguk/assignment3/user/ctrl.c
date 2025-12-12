#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <errno.h>
#include <string.h>         // strerror
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/resource.h>   // setrlimit, RLIMIT_MEMLOCK
#include <limits.h>         // ULLONG_MAX
#include <bpf/libbpf.h>

#include "../include/common.h"
#include "../bpf/trace_blk_sched.skel.h"

static volatile sig_atomic_t stop;
static inline unsigned long long now_monotonic_ns(void);
static void fmt_bw(double bytes_per_sec, char *out, size_t outsz);

/* ===== 집계 ===== */
struct stats64 {
    unsigned long long cnt;
    unsigned long long total;  // ns
    unsigned long long min;    // ns
    unsigned long long max;    // ns
};

struct app_ctx {
    FILE *out_fp;                // NDJSON 파일 (옵션)
    const char *out_path;        // 파일 경로
    unsigned long print_every;   // 1/N 샘플링 (stdout)
    unsigned long long seen;     // 처리 이벤트 수

    // 누적 통계 (I/O 전용)
    struct stats64 st_lat;       // device latency (issue -> complete)
    struct stats64 st_qlat;      // queuing latency (insert -> issue)
    unsigned long long iops;     // complete 건수(초간단 IOPS 근사용)
    unsigned long long bytes;    // complete 바이트 합계(BW 근사)
    
    unsigned long long t0_ns;
};

static void stats_init(struct stats64 *s) {
    s->cnt = 0;
    s->total = 0;
    s->min = ULLONG_MAX;
    s->max = 0;
}

static inline void stats_add(struct stats64 *s, unsigned long long x_ns) {
    s->cnt++;
    s->total += x_ns;
    if (x_ns < s->min) s->min = x_ns;
    if (x_ns > s->max) s->max = x_ns;
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
    print_stats("lat    (issue->complete)", &app->st_lat);
    print_stats("qlat   (insert->issue)  ", &app->st_qlat);
    printf("iops (count of completes): %llu\n", app->iops);
    printf("bytes(completed total)  : %llu\n", app->bytes);

    // ===== IOPS 계산 추가 =====
    if (app->t0_ns > 0) {
        unsigned long long now = now_monotonic_ns();
        double elapsed = (double)(now - app->t0_ns) / 1e9;
        if (elapsed > 0) {
            double iops = (double)app->iops / elapsed;
            double bw_Bps = (double)app->bytes / elapsed;

            char bwbuf[64];
            fmt_bw(bw_Bps, bwbuf, sizeof(bwbuf));

            printf("\n[throughput]\n");
            printf("elapsed time : %.3f sec\n", elapsed);
            printf("IOPS         : %.0f\n", iops);
            printf("BW           : %s\n", bwbuf);
        }
    }
}
static void on_sigint(int signo) { (void)signo; stop = 1; }

static void bump_memlock_rlimit(void)
{
    struct rlimit r = { RLIM_INFINITY, RLIM_INFINITY };
    if (setrlimit(RLIMIT_MEMLOCK, &r) != 0)
        perror("setrlimit(RLIMIT_MEMLOCK)");
}

static void usage(const char *prog)
{
    fprintf(stderr,
        "Usage: sudo %s [-o output.ndjson] [-n N]\n"
        "  -o FILE   Save ALL events to FILE in NDJSON (1 JSON per line)\n"
        "  -n N      Print only every 1/N events to stdout (default: 1 = print all)\n",
        prog);
}

/* ===== NDJSON 출력 ===== */
static void write_event_json(FILE *fp, const struct event *e)
{
    switch (e->h.type) {
    case EVT_RQ_INSERT:
        fprintf(fp,
            "{\"type\":\"insert\",\"ts_ns\":%llu,\"cpu\":%u,"
            "\"pid\":%u,\"tgid\":%u,"
            "\"dev_major\":%u,\"dev_minor\":%u,"
            "\"sector\":%llu,\"nr_sector\":%u,\"bytes\":%u,"
            "\"qd_cur\":%d}\n",
            (unsigned long long)e->h.ts, e->h.cpu, e->h.pid, e->h.tgid,
            e->u.ins.dev.major, e->u.ins.dev.minor,
            (unsigned long long)e->u.ins.sector, e->u.ins.nr_sector, e->u.ins.bytes,
            (int)e->u.ins.qd_cur);
        break;

    case EVT_RQ_ISSUE:
        fprintf(fp,
            "{\"type\":\"issue\",\"ts_ns\":%llu,\"cpu\":%u,"
            "\"pid\":%u,\"tgid\":%u,"
            "\"dev_major\":%u,\"dev_minor\":%u,"
            "\"sector\":%llu,\"nr_sector\":%u,\"bytes\":%u,"
            "\"qd_cur\":%d}\n",
            (unsigned long long)e->h.ts, e->h.cpu, e->h.pid, e->h.tgid,
            e->u.iss.dev.major, e->u.iss.dev.minor,
            (unsigned long long)e->u.iss.sector, e->u.iss.nr_sector, e->u.iss.bytes,
            (int)e->u.iss.qd_cur);
        break;

    case EVT_RQ_COMPLETE:
        fprintf(fp,
            "{\"type\":\"complete\",\"ts_ns\":%llu,\"cpu\":%u,"
            "\"pid\":%u,\"tgid\":%u,"
            "\"dev_major\":%u,\"dev_minor\":%u,"
            "\"sector\":%llu,\"nr_sector\":%u,"
            "\"bytes\":%u,\"error\":%d,"
            "\"lat_ns\":%llu,\"qlat_ns\":%llu,"
            "\"qd_cur\":%d}\n",
            (unsigned long long)e->h.ts, e->h.cpu, e->h.pid, e->h.tgid,
            e->u.cmp.dev.major, e->u.cmp.dev.minor,
            (unsigned long long)e->u.cmp.sector, e->u.cmp.nr_sector,
            e->u.cmp.bytes, e->u.cmp.error,
            (unsigned long long)e->u.cmp.lat_ns, (unsigned long long)e->u.cmp.qlat_ns,
            (int)e->u.cmp.qd_cur);
        break;

    default:
        break;
    }
}

/* ===== 출력 형식 ===== */
static void print_event_sample(const struct event *e)
{
    switch (e->h.type) {
    case EVT_RQ_INSERT:
        printf("[INSERT] ts=%llu cpu=%u pid=%u dev=%u:%u sector=%llu nsec=%u bytes=%u qd=%d\n",
               (unsigned long long)e->h.ts, e->h.cpu, e->h.pid,
               e->u.ins.dev.major, e->u.ins.dev.minor,
               (unsigned long long)e->u.ins.sector,
               e->u.ins.nr_sector, e->u.ins.bytes,
               (int)e->u.ins.qd_cur);
        break;
    case EVT_RQ_ISSUE:
        printf("[ISSUE ] ts=%llu cpu=%u pid=%u dev=%u:%u sector=%llu nsec=%u bytes=%u qd=%d\n",
               (unsigned long long)e->h.ts, e->h.cpu, e->h.pid,
               e->u.iss.dev.major, e->u.iss.dev.minor,
               (unsigned long long)e->u.iss.sector,
               e->u.iss.nr_sector, e->u.iss.bytes,
               (int)e->u.iss.qd_cur);
        break;
    case EVT_RQ_COMPLETE:
        printf("[COMP  ] ts=%llu cpu=%u pid=%u dev=%u:%u sector=%llu nsec=%u "
               "bytes=%u err=%d lat=%.3f ms qlat=%.3f ms qd=%d\n",
               (unsigned long long)e->h.ts, e->h.cpu, e->h.pid,
               e->u.cmp.dev.major, e->u.cmp.dev.minor,
               (unsigned long long)e->u.cmp.sector, e->u.cmp.nr_sector,
               e->u.cmp.bytes, e->u.cmp.error,
               (double)e->u.cmp.lat_ns / 1e6,
               (double)e->u.cmp.qlat_ns / 1e6,
               (int)e->u.cmp.qd_cur);
        break;
    default:
        break;
    }
}

/* ===== Helpers ===== */
static inline unsigned long long now_monotonic_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (unsigned long long)ts.tv_sec * 1000000000ULL + (unsigned long long)ts.tv_nsec;
}

static void fmt_bw(double bytes_per_sec, char *out, size_t outsz)
{
    const double KB = 1024.0, MB = 1024.0*KB, GB = 1024.0*MB;
    if (bytes_per_sec >= GB) snprintf(out, outsz, "%.2fGiB/s", bytes_per_sec/GB);
    else if (bytes_per_sec >= MB) snprintf(out, outsz, "%.2fMiB/s", bytes_per_sec/MB);
    else if (bytes_per_sec >= KB) snprintf(out, outsz, "%.2fKiB/s", bytes_per_sec/KB);
    else snprintf(out, outsz, "%.0fB/s", bytes_per_sec);
}

/* ===== 콜백 ===== */
static int handle_event(void *ctx, void *data, size_t len)
{
    if (len < sizeof(struct event)) return 0;

    struct app_ctx *app = (struct app_ctx *)ctx;
    const struct event *e = (const struct event *)data;

    app->seen++;

    /* 통계 업데이트 */
    if (e->h.type == EVT_RQ_COMPLETE) {
        stats_add(&app->st_lat,  (unsigned long long)e->u.cmp.lat_ns);
        stats_add(&app->st_qlat, (unsigned long long)e->u.cmp.qlat_ns);
        app->iops++;
        app->bytes += e->u.cmp.bytes;

        /* ← 첫 complete 시각을 기준으로 누적 시간 측정 시작 */
        if (app->t0_ns == 0)
            app->t0_ns = now_monotonic_ns();
    }

    /* 1) 파일 저장 (전부) */
    if (app->out_fp) {
        write_event_json(app->out_fp, e);
        fflush(app->out_fp);
    }

    /* 2) 표준출력: 1/N 샘플링 */
    unsigned long N = (app->print_every == 0 ? 1 : app->print_every);
    if (app->seen % N == 0) {
        print_event_sample(e);
        fflush(stdout);
    }

    return 0;
}

/* ===== Main ===== */
int main(int argc, char **argv)
{
    int err = 0;
    struct app_ctx app;
    memset(&app, 0, sizeof(app));
    app.print_every = 1;
    stats_init(&app.st_lat);
    stats_init(&app.st_qlat);

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

    struct sigaction sa = { .sa_handler = on_sigint };
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    bump_memlock_rlimit();

    struct trace_blk_sched_bpf *skel = NULL;

    /* open -> load -> attach */
    skel = trace_blk_sched_bpf__open();
    if (!skel) { fprintf(stderr, "open skel failed\n"); err = -1; goto out; }

    err = trace_blk_sched_bpf__load(skel);
    if (err) { fprintf(stderr, "load skel: %d\n", err); goto out; }

    err = trace_blk_sched_bpf__attach(skel);
    if (err) { fprintf(stderr, "attach skel: %d\n", err); goto out; }

    /* ringbuf */
    {
        int ring_fd = bpf_map__fd(skel->maps.ring);
        if (ring_fd < 0) { fprintf(stderr, "ringbuf fd: %d\n", ring_fd); err = -1; goto out; }

        struct ring_buffer *rb = ring_buffer__new(ring_fd, handle_event, &app, NULL);
        if (!rb) { fprintf(stderr, "ring_buffer__new failed\n"); err = -1; goto out; }

        printf("Running... (Ctrl-C to stop)\n");
        fflush(stdout);

        /* consume-loop: epoll 없이 즉시 리턴 -> Ctrl-C에 빠르게 반응 */
        const long ns_per_ms = 1000000L;
        const long sleep_ms = 50; // 50ms: CPU 낭비 줄이면서 반응성 확보
        while (!stop) {
            err = ring_buffer__consume(rb);
            if (err < 0 && errno != EINTR) {
                fprintf(stderr, "ring_buffer__consume: %d\n", err);
                break;
            }
            struct timespec req = { .tv_sec = 0, .tv_nsec = sleep_ms * ns_per_ms };
            nanosleep(&req, NULL);
        }
        ring_buffer__free(rb);
    }

    /* 종료 요약 */
    print_summary(&app);

out:
    if (skel) trace_blk_sched_bpf__destroy(skel);
    if (app.out_fp) fclose(app.out_fp);
    return (err < 0) ? 1 : 0;
}


