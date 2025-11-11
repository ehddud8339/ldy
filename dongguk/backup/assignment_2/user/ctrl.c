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
#include "../bpf/trace_sched.skel.h"
#include "../bpf/trace_irq.skel.h"
#include "../bpf/trace_softirq.skel.h"

#define PIN_ROOT "/sys/fs/bpf/ebpf_proj"
#define PIN_RING PIN_ROOT "/ringbuf"

static volatile sig_atomic_t stop;

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

    // 누적 통계
    struct stats64 st_sched;
    struct stats64 st_irq;
    struct stats64 st_softirq;
};

static void stats_init(struct stats64 *s) {
    s->cnt = 0;
    s->total = 0;
    s->min = ULLONG_MAX;
    s->max = 0;
}

static inline void stats_add(struct stats64 *s, unsigned long long lat_ns) {
    s->cnt++;
    s->total += lat_ns;
    if (lat_ns < s->min) s->min = lat_ns;
    if (lat_ns > s->max) s->max = lat_ns;
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
    print_stats("sched   (wake->run)\n", &app->st_sched);
    print_stats("irq     (entry->exit)\n", &app->st_irq);
    print_stats("softirq (raise/entry/exit merged)\n", &app->st_softirq);
}

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

static void usage(const char *prog)
{
    fprintf(stderr,
        "Usage: sudo %s [-o output.ndjson] [-n N]\n"
        "  -o FILE   Save ALL events to FILE in NDJSON (1 JSON per line)\n"
        "  -n N      Print only every 1/N events to stdout (default: 1 = print all)\n",
        prog);
}

/* 공통: 한 이벤트를 주어진 FILE* 로 NDJSON 한 줄 출력 */
static void write_event_json(FILE *fp, const struct event *e)
{
    switch (e->h.type) {
    case EVENT_SCHED_WAKE_RUN:
        fprintf(fp,
                "{\"type\":\"sched\",\"ts_ns\":%llu,\"cpu\":%u,"
                "\"pid\":%u,\"tgid\":%u,\"comm\":\"%.*s\",\"lat_ns\":%llu}\n",
                (unsigned long long)e->h.ts_ns,
                e->h.cpu,
                e->d.sched.pid,
                e->d.sched.tgid,
                COMM_LEN, e->d.sched.comm,
                (unsigned long long)e->d.sched.latency_ns);
        break;

    case EVENT_IRQ_LATENCY:
        fprintf(fp,
                "{\"type\":\"irq\",\"ts_ns\":%llu,\"cpu\":%u,"
                "\"irq\":%u,\"lat_ns\":%llu}\n",
                (unsigned long long)e->h.ts_ns,
                e->h.cpu,
                e->d.irq.irq,
                (unsigned long long)e->d.irq.latency_ns);
        break;

    case EVENT_SOFTIRQ_LATENCY:
        fprintf(fp,
                "{\"type\":\"softirq\",\"ts_ns\":%llu,\"cpu\":%u,"
                "\"vec\":%u,\"phase\":\"%s\",\"lat_ns\":%llu}\n",
                (unsigned long long)e->h.ts_ns,
                e->h.cpu,
                e->d.softirq.vec_nr,
                (e->d.softirq.phase == SOFTIRQ_RAISE_TO_ENTRY) ? "raise_entry" : "entry_exit",
                (unsigned long long)e->d.softirq.latency_ns);
        break;

    default:
        /* unknown -> skip */
        break;
    }
}

/* ringbuf handler: 파일 저장(전체), stdout 샘플링(1/N), 통계 누적 */
static int handle_event(void *ctx, void *data, size_t len)
{
    if (len < sizeof(struct event)) return 0;

    struct app_ctx *app = (struct app_ctx *)ctx;
    struct event *e = (struct event *)data;

    app->seen++;

    /* 통계 업데이트 */
    switch (e->h.type) {
    case EVENT_SCHED_WAKE_RUN:
        stats_add(&app->st_sched, (unsigned long long)e->d.sched.latency_ns);
        break;
    case EVENT_IRQ_LATENCY:
        stats_add(&app->st_irq, (unsigned long long)e->d.irq.latency_ns);
        break;
    case EVENT_SOFTIRQ_LATENCY:
        // raise->entry / entry->exit 모두 softirq 통계에 합산
        stats_add(&app->st_softirq, (unsigned long long)e->d.softirq.latency_ns);
        break;
    default:
        break;
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

int main(int argc, char **argv)
{
    int err = 0;
    struct app_ctx app;
    memset(&app, 0, sizeof(app));
    app.print_every = 1;
    stats_init(&app.st_sched);
    stats_init(&app.st_irq);
    stats_init(&app.st_softirq);

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

    struct trace_sched_bpf   *sched = NULL;
    struct trace_irq_bpf     *irq   = NULL;
    struct trace_softirq_bpf *sirq  = NULL;

    /* open skels */
    sched = trace_sched_bpf__open();
    if (!sched) { fprintf(stderr, "open sched skel failed\n"); goto out; }
    irq = trace_irq_bpf__open();
    if (!irq)   { fprintf(stderr, "open irq skel failed\n"); goto out; }
    sirq = trace_softirq_bpf__open();
    if (!sirq)  { fprintf(stderr, "open softirq skel failed\n"); goto out; }

    /* share ringbuf via pin */
    bpf_map__set_pin_path(sched->maps.ringbuf, PIN_RING);
    bpf_map__set_pin_path(irq->maps.ringbuf,   PIN_RING);
    bpf_map__set_pin_path(sirq->maps.ringbuf,  PIN_RING);

    /* load */
    if ((err = trace_sched_bpf__load(sched))) { fprintf(stderr, "load sched: %d\n", err); goto out; }
    if ((err = trace_irq_bpf__load(irq)))     { fprintf(stderr, "load irq: %d\n", err);   goto out; }
    if ((err = trace_softirq_bpf__load(sirq))){ fprintf(stderr, "load softirq: %d\n", err); goto out; }

    /* attach */
    if ((err = trace_sched_bpf__attach(sched)))  { fprintf(stderr, "attach sched: %d\n", err); goto out; }
    if ((err = trace_irq_bpf__attach(irq)))      { fprintf(stderr, "attach irq: %d\n", err); goto out; }
    if ((err = trace_softirq_bpf__attach(sirq))) { fprintf(stderr, "attach softirq: %d\n", err); goto out; }

    /* ringbuf (shared) */
    {
        int ring_fd = bpf_map__fd(sched->maps.ringbuf);
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
    if (sirq)  trace_softirq_bpf__destroy(sirq);
    if (irq)   trace_irq_bpf__destroy(irq);
    if (sched) trace_sched_bpf__destroy(sched);
    if (app.out_fp) fclose(app.out_fp);
    return (err < 0) ? 1 : 0;
}

