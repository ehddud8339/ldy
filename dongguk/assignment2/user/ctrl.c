#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <errno.h>
#include <string.h>         // strerror
#include <unistd.h>
#include <sys/stat.h>
#include <sys/resource.h>   // setrlimit, RLIMIT_MEMLOCK
#include <limits.h>         // ULLONG_MAX
#include <bpf/libbpf.h>

#include "../include/common.h"
#include "trace_ctx_irq.skel.h"   // 단일 스켈레톤(= sched/irq/softirq 모두 이 BPF 안에 있음)

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
    print_stats("sched   (wake->run)", &app->st_sched);
    print_stats("irq     (entry->exit)", &app->st_irq);
    print_stats("softirq (raise/entry/exit merged)", &app->st_softirq);
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

/* 공통: 한 이벤트를 주어진 FILE* 로 NDJSON 한 줄 출력
   (공용 헤더 common.h의 enum/type 이름은 기존과 동일하다고 가정)
   - EVT_SCHED_LAT:  u.slat.delta_ns
   - EVT_CTXSW:      u.cs.*  (요약은 stdout에서만)
   - EVT_IRQ_H:      u.idur.dur_ns
   - EVT_SIRQ_LAT:   u.silat.lat_ns
   - EVT_SIRQ_DUR:   u.sidur.dur_ns
*/
static void write_event_json(FILE *fp, const struct event *e)
{
    switch (e->h.type) {
    case EVT_SCHED_LAT:
        fprintf(fp,
            "{\"type\":\"sched\",\"ts_ns\":%llu,\"cpu\":%u,"
            "\"pid\":%u,\"prio\":%u,\"target_cpu\":%u,\"lat_ns\":%llu}\n",
            (unsigned long long)e->h.ts,
            e->h.cpu,
            e->u.slat.pid,
            e->u.slat.prio,
            e->u.slat.target_cpu,
            (unsigned long long)e->u.slat.delta_ns);
        break;

    case EVT_CTXSW:
        // 컨텍스트 스위치는 통계 합산 대상이 아니므로 파일에는 필요 시만 남겨도 됨.
        fprintf(fp,
            "{\"type\":\"ctxsw\",\"ts_ns\":%llu,\"cpu\":%u,"
            "\"prev_pid\":%u,\"prev_prio\":%u,\"prev_state\":%llu,"
            "\"next_pid\":%u,\"next_prio\":%u}\n",
            (unsigned long long)e->h.ts,
            e->h.cpu,
            e->u.cs.prev_pid, e->u.cs.prev_prio, (unsigned long long)e->u.cs.prev_state,
            e->u.cs.next_pid, e->u.cs.next_prio);
        break;

    case EVT_IRQ_H:
        fprintf(fp,
            "{\"type\":\"irq\",\"ts_ns\":%llu,\"cpu\":%u,"
            "\"irq\":%u,\"ret\":%u,\"dur_ns\":%llu}\n",
            (unsigned long long)e->h.ts,
            e->h.cpu,
            e->u.idur.irq, e->u.idur.ret, (unsigned long long)e->u.idur.dur_ns);
        break;

    case EVT_SIRQ_LAT:
        fprintf(fp,
            "{\"type\":\"softirq\",\"ts_ns\":%llu,\"cpu\":%u,"
            "\"vec\":%u,\"phase\":\"raise_entry\",\"lat_ns\":%llu}\n",
            (unsigned long long)e->h.ts,
            e->h.cpu,
            e->u.silat.vec, (unsigned long long)e->u.silat.lat_ns);
        break;

    case EVT_SIRQ_DUR:
        fprintf(fp,
            "{\"type\":\"softirq\",\"ts_ns\":%llu,\"cpu\":%u,"
            "\"vec\":%u,\"phase\":\"entry_exit\",\"lat_ns\":%llu}\n",
            (unsigned long long)e->h.ts,
            e->h.cpu,
            e->u.sidur.vec, (unsigned long long)e->u.sidur.dur_ns);
        break;

    default:
        break;
    }
}

/* stdout 요약 (1/N 샘플링) */
static const char *softirq_name(unsigned int vec)
{
    switch (vec) {
    case 0: return "HI";
    case 1: return "TIMER";
    case 2: return "NET_TX";
    case 3: return "NET_RX";
    case 4: return "BLOCK";
    case 5: return "IRQ_POLL";
    case 6: return "TASKLET";
    case 7: return "SCHED";
    case 8: return "HRTIMER";
    case 9: return "RCU";
    default: return "UNKNOWN";
    }
}

static void print_event_sample(const struct event *e)
{
    switch (e->h.type) {
    case EVT_SCHED_LAT:
        printf("[SCHED_LAT] ts=%llu cpu=%u pid=%u prio=%u target_cpu=%u delta=%.3f us\n",
               (unsigned long long)e->h.ts, e->h.cpu,
               e->u.slat.pid, e->u.slat.prio, e->u.slat.target_cpu,
               (double)e->u.slat.delta_ns / 1000.0);
        break;
    case EVT_CTXSW:
        printf("[CTXSW] ts=%llu cpu=%u prev=%u(prio=%u,state=0x%llx) -> next=%u(prio=%u)\n",
               (unsigned long long)e->h.ts, e->h.cpu,
               e->u.cs.prev_pid, e->u.cs.prev_prio, (unsigned long long)e->u.cs.prev_state,
               e->u.cs.next_pid, e->u.cs.next_prio);
        break;
    case EVT_IRQ_H:
        printf("[IRQ] ts=%llu cpu=%u irq=%u ret=%u dur=%.3f us\n",
               (unsigned long long)e->h.ts, e->h.cpu,
               e->u.idur.irq, e->u.idur.ret, (double)e->u.idur.dur_ns / 1000.0);
        break;
    case EVT_SIRQ_LAT:
        printf("[SIRQ_LAT] ts=%llu cpu=%u vec=%u(%s) lat=%.3f us\n",
               (unsigned long long)e->h.ts, e->h.cpu,
               e->u.silat.vec, softirq_name(e->u.silat.vec),
               (double)e->u.silat.lat_ns / 1000.0);
        break;
    case EVT_SIRQ_DUR:
        printf("[SIRQ_DUR] ts=%llu cpu=%u vec=%u(%s) dur=%.3f us\n",
               (unsigned long long)e->h.ts, e->h.cpu,
               e->u.sidur.vec, softirq_name(e->u.sidur.vec),
               (double)e->u.sidur.dur_ns / 1000.0);
        break;
    default:
        break;
    }
}

/* ringbuf handler: 파일 저장(전체), stdout 샘플링(1/N), 통계 누적 */
static int handle_event(void *ctx, void *data, size_t len)
{
    if (len < sizeof(struct event)) return 0;

    struct app_ctx *app = (struct app_ctx *)ctx;
    const struct event *e = (const struct event *)data;

    app->seen++;

    /* 통계 업데이트 */
    switch (e->h.type) {
    case EVT_SCHED_LAT:
        stats_add(&app->st_sched, (unsigned long long)e->u.slat.delta_ns);
        break;
    case EVT_IRQ_H:
        stats_add(&app->st_irq, (unsigned long long)e->u.idur.dur_ns);
        break;
    case EVT_SIRQ_LAT:
        stats_add(&app->st_softirq, (unsigned long long)e->u.silat.lat_ns);
        break;
    case EVT_SIRQ_DUR:
        stats_add(&app->st_softirq, (unsigned long long)e->u.sidur.dur_ns);
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
        print_event_sample(e);
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
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; // NO SA_RESTART
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    bump_memlock_rlimit();

    struct trace_ctx_irq_bpf *skel = NULL;

    /* open -> load -> attach */
    skel = trace_ctx_irq_bpf__open();
    if (!skel) { fprintf(stderr, "open skel failed\n"); goto out; }

    if ((err = trace_ctx_irq_bpf__load(skel))) {
        fprintf(stderr, "load skel: %d\n", err);
        goto out;
    }
    if ((err = trace_ctx_irq_bpf__attach(skel))) {
        fprintf(stderr, "attach skel: %d\n", err);
        goto out;
    }

    /* ringbuf */
    {
        int ring_fd = bpf_map__fd(skel->maps.events);
        if (ring_fd < 0) { fprintf(stderr, "ringbuf fd: %d\n", ring_fd); goto out; }

        struct ring_buffer *rb = ring_buffer__new(ring_fd, handle_event, &app, NULL);
        if (!rb) { fprintf(stderr, "ring_buffer__new failed\n"); goto out; }

        printf("Running... (Ctrl-C to stop)\n");
        fflush(stdout);

        /* consume-loop: epoll 없이 즉시 리턴 -> Ctrl-C에 매우 빠르게 반응 */
        const long ns_per_ms = 1000000L;
        const long sleep_ms = 50; // 50ms: CPU 낭비 줄이면서도 반응성 확보
        while (!stop) {
            err = ring_buffer__consume(rb);
            if (err < 0) {
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
    if (skel) trace_ctx_irq_bpf__destroy(skel);
    if (app.out_fp) fclose(app.out_fp);
    return (err < 0) ? 1 : 0;
}

