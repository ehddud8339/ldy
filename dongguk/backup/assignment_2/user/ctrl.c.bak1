// user/ctrl.c
// Minimal controller: load & attach 3 BPF objects, share one ringbuf, print JSON lines.

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
#include <bpf/libbpf.h>

#include "../include/common.h"
#include "../bpf/trace_sched.skel.h"
#include "../bpf/trace_irq.skel.h"
#include "../bpf/trace_softirq.skel.h"

#define PIN_ROOT "/sys/fs/bpf/ebpf_proj"
#define PIN_RING PIN_ROOT "/ringbuf"

static volatile sig_atomic_t stop;

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

/* ringbuf JSON printer */
static int handle_event(void *ctx, void *data, size_t len)
{
    (void)ctx;
    if (len < sizeof(struct event)) return 0;

    struct event *e = (struct event *)data;

    switch (e->h.type) {
    case EVENT_SCHED_WAKE_RUN:
        printf("{\"type\":\"sched\",\"ts_ns\":%llu,\"cpu\":%u,"
               "\"pid\":%u,\"tgid\":%u,\"comm\":\"%.*s\",\"lat_ns\":%llu}\n",
               (unsigned long long)e->h.ts_ns,
               e->h.cpu,
               e->d.sched.pid,
               e->d.sched.tgid,
               COMM_LEN, e->d.sched.comm,
               (unsigned long long)e->d.sched.latency_ns);
        break;

    case EVENT_IRQ_LATENCY:
        printf("{\"type\":\"irq\",\"ts_ns\":%llu,\"cpu\":%u,"
               "\"irq\":%u,\"lat_ns\":%llu}\n",
               (unsigned long long)e->h.ts_ns,
               e->h.cpu,
               e->d.irq.irq,
               (unsigned long long)e->d.irq.latency_ns);
        break;

    case EVENT_SOFTIRQ_LATENCY:
        printf("{\"type\":\"softirq\",\"ts_ns\":%llu,\"cpu\":%u,"
               "\"vec\":%u,\"phase\":\"%s\",\"lat_ns\":%llu}\n",
               (unsigned long long)e->h.ts_ns,
               e->h.cpu,
               e->d.softirq.vec_nr,
               (e->d.softirq.phase == SOFTIRQ_RAISE_TO_ENTRY) ? "raise_entry" : "entry_exit",
               (unsigned long long)e->d.softirq.latency_ns);
        break;
    default:
        break;
    }

    fflush(stdout);
    return 0;
}

int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    int err = 0;

    libbpf_set_strict_mode(LIBBPF_STRICT_ALL);
    libbpf_set_print(NULL);

    struct sigaction sa = { .sa_handler = on_sigint };
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    bump_memlock_rlimit();
    hint_mount_bpffs();

    /* ensure /sys/fs/bpf exists */
    err = ensure_dir("/sys/fs/bpf");
    if (err) { fprintf(stderr, "bpffs missing? %s\n", strerror(-err)); return 1; }
    err = ensure_dir(PIN_ROOT);
    if (err) { fprintf(stderr, "mkdir %s: %s\n", PIN_ROOT, strerror(-err)); return 1; }

    /* 🔥 NULL init to silence warnings */
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
    if ((err = trace_sched_bpf__attach(sched))) { fprintf(stderr, "attach sched: %d\n", err); goto out; }
    if ((err = trace_irq_bpf__attach(irq)))     { fprintf(stderr, "attach irq: %d\n", err); goto out; }
    if ((err = trace_softirq_bpf__attach(sirq))) { fprintf(stderr, "attach softirq: %d\n", err); goto out; }

    /* ringbuf */
    int ring_fd = bpf_map__fd(sched->maps.ringbuf);
    if (ring_fd < 0) { fprintf(stderr, "ringbuf fd: %d\n", ring_fd); goto out; }

    struct ring_buffer *rb = ring_buffer__new(ring_fd, handle_event, NULL, NULL);
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

out:
    if (sirq) trace_softirq_bpf__destroy(sirq);
    if (irq)  trace_irq_bpf__destroy(irq);
    if (sched) trace_sched_bpf__destroy(sched);
    return err < 0 ? 1 : 0;
}

