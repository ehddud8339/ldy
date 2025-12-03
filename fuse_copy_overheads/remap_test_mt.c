// remap_test_mt.c
#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>

/* === remap_bench uapi (커널과 동일하게 맞출 것) === */

#define REMAP_BENCH_MAGIC  'r'

struct remap_reg {
    uint64_t addr;
    uint64_t len;
};

#define REMAP_BENCH_REGISTER   _IOW (REMAP_BENCH_MAGIC, 1, struct remap_reg)
#define REMAP_BENCH_REMAP_NEXT _IOWR(REMAP_BENCH_MAGIC, 2, uint64_t)

/* === 전역 컨텍스트 === */

static void   *g_view;
static int     g_fd;
static long    g_iters;
static int     g_nthreads;

static volatile int g_start = 0;
static volatile int g_stop  = 0;

static void pin_to_cpu(int cpu)
{
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);

    int ret = pthread_setaffinity_np(pthread_self(), sizeof(set), &set);
    if (ret != 0) {
        fprintf(stderr, "pthread_setaffinity_np(cpu=%d): %s\n",
                cpu, strerror(ret));
    }
}

/* remap + zap를 반복 호출하는 스레드 (thread 0) */
static void *remap_thread(void *arg)
{
    int cpu = (int)(intptr_t)arg;
    pin_to_cpu(cpu);

    while (!g_start)
        ;   /* busy-wait sync */

    uint64_t sum = 0;
    uint64_t min = UINT64_MAX;
    uint64_t max = 0;

    for (long i = 0; i < g_iters; i++) {
        uint64_t delta_ns = 0;

        if (ioctl(g_fd, REMAP_BENCH_REMAP_NEXT, &delta_ns) < 0) {
            perror("ioctl REMAP_BENCH_REMAP_NEXT");
            break;
        }

        sum += delta_ns;
        if (delta_ns < min) min = delta_ns;
        if (delta_ns > max) max = delta_ns;
    }

    double avg = (g_iters > 0) ? (double)sum / (double)g_iters : 0.0;

    printf("[remap cpu=%d] iterations: %ld, avg: %.2f ns, min: %" PRIu64
           " ns, max: %" PRIu64 " ns\n",
           cpu, g_iters, avg, min, max);

    g_stop = 1;
    return NULL;
}

/* 동일한 VMA를 계속 touch하는 worker 스레드들 */
static void *worker_thread(void *arg)
{
    int cpu = (int)(intptr_t)arg;
    pin_to_cpu(cpu);

    volatile unsigned char *p = (volatile unsigned char *)g_view;

    while (!g_start)
        ;

    /* stop 신호 올 때까지 view를 계속 읽어서
     * 이 mm이 해당 CPU에서 active 상태가 되도록 유지
     */
    while (!g_stop) {
        for (int i = 0; i < 4096; i++)
            (void)p[i];
    }

    return NULL;
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <iterations> <nthreads>\n", argv[0]);
        return 1;
    }

    g_iters    = atol(argv[1]);
    g_nthreads = atoi(argv[2]);

    if (g_nthreads <= 0) {
        fprintf(stderr, "nthreads must be >= 1\n");
        return 1;
    }

    int fd = open("/dev/remap_bench", O_RDWR);
    if (fd < 0) {
        perror("open /dev/remap_bench");
        return 1;
    }
    g_fd = fd;

    size_t map_len = 4096;   /* 커널이 PAGE_SIZE=4KB만 허용하도록 되어 있음 */

    void *view = mmap(NULL, map_len,
                      PROT_READ | PROT_WRITE,
                      MAP_SHARED, fd, 0);
    if (view == MAP_FAILED) {
        perror("mmap");
        return 1;
    }
    g_view = view;

    /* 커널에 mmap 정보 전달 (검증/확장용) */
    struct remap_reg reg = {
        .addr = (uint64_t)view,
        .len  = map_len,
    };

    if (ioctl(fd, REMAP_BENCH_REGISTER, &reg) < 0) {
        perror("ioctl REMAP_BENCH_REGISTER");
        return 1;
    }

    pthread_t *ths = calloc(g_nthreads, sizeof(pthread_t));
    if (!ths) {
        perror("calloc");
        return 1;
    }

    /* 0번 스레드는 remap 전용 (CPU 0에 pinning) */
    if (pthread_create(&ths[0], NULL, remap_thread, (void *)(intptr_t)0) != 0) {
        perror("pthread_create(remap)");
        return 1;
    }

    /* 나머지 스레드는 worker (CPU 1..nthreads-1에 pinning) */
    for (int i = 1; i < g_nthreads; i++) {
        if (pthread_create(&ths[i], NULL, worker_thread,
                           (void *)(intptr_t)i) != 0) {
            perror("pthread_create(worker)");
            return 1;
        }
    }

    /* 어느 정도 준비 시간 주고 start 플래그 올림 */
    sleep(1);
    g_start = 1;

    /* remap 스레드 완료 대기 */
    pthread_join(ths[0], NULL);

    /* worker 스레드 종료 신호 */
    g_stop = 1;
    for (int i = 1; i < g_nthreads; i++) {
        pthread_join(ths[i], NULL);
    }

    return 0;
}

