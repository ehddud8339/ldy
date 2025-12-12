#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/resource.h>
#include <syslog.h>
#include <bpf/libbpf.h>
#include "vfs_write_latency.skel.h"

// BPF 코드와 동일한 구조체 및 Enum 정의
enum event_type {
    EVENT_ENTRY = 0,
    EVENT_EXIT = 1
};

struct event_t {
    int type;
    unsigned long long ts;
    unsigned int pid;
    unsigned int tid;
    char comm[16];
    unsigned long long len;
    unsigned int flags;
    long ret;
    unsigned long long duration_ns;
};

static volatile bool exiting = false;

static void sig_handler(int sig)
{
    exiting = true;
}

static int handle_event(void *ctx, void *data, size_t data_sz)
{
    const struct event_t *e = data;

    if (data_sz < sizeof(*e)) {
        fprintf(stderr, "Invalid data size: %zu\n", data_sz);
        return 0;
    }

    // 타입에 따라 로그 분기
    if (e->type == EVENT_ENTRY) {
        // [진입 로그]
        // 타임스탬프와 요청 정보를 출력
        syslog(LOG_INFO, "[ENTRY] [%s:%d] vfs_write called. len: %llu, flags: %x, ts: %llu",
               e->comm, e->pid, e->len, e->flags, e->ts);
        
        printf("[ENTRY] [%s:%d] len: %llu (ts: %llu)\n", 
               e->comm, e->pid, e->len, e->ts);

    } else if (e->type == EVENT_EXIT) {
        // [종료 로그]
        // 결과값(ret)과 소요시간(duration)을 포함하여 출력
        syslog(LOG_INFO, "[EXIT]  [%s:%d] vfs_write return. ret: %ld, duration: %llu ns, ts: %llu",
               e->comm, e->pid, e->ret, e->duration_ns, e->ts);

        printf("[EXIT]  [%s:%d] ret: %ld, dur: %llu ns (ts: %llu)\n", 
               e->comm, e->pid, e->ret, e->duration_ns, e->ts);
    }

    return 0;
}

int main(int argc, char **argv)
{
    struct vfs_write_latency_bpf *skel;
    struct ring_buffer *rb = NULL;
    int err;

    openlog("ebpf-fio-tracer", LOG_PID | LOG_NDELAY, LOG_USER);

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    skel = vfs_write_latency_bpf__open();
    if (!skel) {
        fprintf(stderr, "Failed to open BPF skeleton\n");
        return 1;
    }

    if (vfs_write_latency_bpf__load(skel)) {
        fprintf(stderr, "Failed to load BPF skeleton\n");
        goto cleanup;
    }

    if (vfs_write_latency_bpf__attach(skel)) {
        fprintf(stderr, "Failed to attach BPF skeleton\n");
        goto cleanup;
    }

    printf("Successfully started! Tracing ENTRY/EXIT for 'fio'...\n");

    rb = ring_buffer__new(bpf_map__fd(skel->maps.rb), handle_event, NULL, NULL);
    if (!rb) {
        fprintf(stderr, "Failed to create ring buffer\n");
        goto cleanup;
    }

    while (!exiting) {
        err = ring_buffer__poll(rb, 100);
        if (err == -EINTR) {
            err = 0;
            break;
        }
        if (err < 0) {
            fprintf(stderr, "Error polling ring buffer\n");
            break;
        }
    }

cleanup:
    ring_buffer__free(rb);
    vfs_write_latency_bpf__destroy(skel);
    closelog();
    
    return 0;
}
