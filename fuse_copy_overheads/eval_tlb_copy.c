#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/mman.h>
#include <time.h>
#include <sched.h>
#include <errno.h>
#include <string.h>

static inline uint64_t now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + ts.tv_nsec;
}

static void pin_to_cpu(int cpu)
{
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    if (sched_setaffinity(0, sizeof(set), &set) != 0) {
        perror("sched_setaffinity");
    }
}

// TLB flush 비용 측정: mprotect로 페이지 단위 PROT 변경
static void tlb_flush_bench(size_t pages, int iters, long page_size)
{
    size_t len = pages * (size_t)page_size;

    void *base = mmap(NULL, len,
                      PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS,
                      -1, 0);
    if (base == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    // warmup: 페이지 터치해서 fault 제거
    for (size_t i = 0; i < len; i += page_size) {
        volatile char *p = (char *)base + i;
        *p = 0;
    }

    printf("=== TLB flush bench (mprotect) ===\n");
    printf("pages: %zu (total %zu bytes), iters: %d\n",
           pages, len, iters);

    uint64_t start = now_ns();

    for (int it = 0; it < iters; it++) {
        for (size_t i = 0; i < len; i += page_size) {
            void *addr = (char *)base + i;

            if (mprotect(addr, page_size, PROT_NONE) != 0) {
                perror("mprotect(PROT_NONE)");
                exit(1);
            }
            if (mprotect(addr, page_size,
                         PROT_READ | PROT_WRITE) != 0) {
                perror("mprotect(PROT_READ|PROT_WRITE)");
                exit(1);
            }
        }
    }

    uint64_t end = now_ns();
    double total_ns = (double)(end - start);
    double per_iter_ns = total_ns / iters;
    double per_page_ns = per_iter_ns / pages;

    printf("TLB total:      %.0f ns\n", total_ns);
    printf("TLB per iter:   %.2f ns\n", per_iter_ns);
    printf("TLB per page:   %.2f ns\n", per_page_ns);

    munmap(base, len);
}

// memcpy 비용 측정
static void memcpy_bench(size_t bytes, int iters)
{
    void *src = NULL;
    void *dst = NULL;

    if (posix_memalign(&src, 64, bytes) != 0 ||
        posix_memalign(&dst, 64, bytes) != 0) {
        fprintf(stderr, "posix_memalign failed\n");
        exit(1);
    }

    memset(src, 0xAA, bytes);
    memset(dst, 0x55, bytes);

    printf("=== memcpy bench ===\n");
    printf("bytes: %zu, iters: %d\n", bytes, iters);

    uint64_t start = now_ns();

    for (int i = 0; i < iters; i++) {
        memcpy(dst, src, bytes);
    }

    uint64_t end = now_ns();
    double total_ns = (double)(end - start);
    double per_iter_ns = total_ns / iters;
    double bandwidth_gbps = 0.0;
    if (per_iter_ns > 0.0) {
        double bytes_per_ns = (double)bytes / per_iter_ns;
        bandwidth_gbps = bytes_per_ns * 1e9 /
                         (1024.0 * 1024.0 * 1024.0);
    }

    printf("memcpy total:   %.0f ns\n", total_ns);
    printf("memcpy per op:  %.2f ns\n", per_iter_ns);
    printf("approx BW:      %.2f GB/s\n", bandwidth_gbps);

    free(src);
    free(dst);
}

int main(int argc, char **argv)
{
    // 테스트할 페이지 수 리스트 (4KB * pages)
    size_t pages_list[] = {
        1,   // 4KB
        2,   // 8KB
        4,   // 16KB
        8,   // 32KB
        16,  // 64KB
        32,  // 128KB
        64,  // 256KB
        128  // 512KB
    };
    size_t num_sizes = sizeof(pages_list) / sizeof(pages_list[0]);

    int tlb_iters = 10000;
    int memcpy_iters = 100000;

    if (argc >= 2) {
        tlb_iters = atoi(argv[1]);
    }
    if (argc >= 3) {
        memcpy_iters = atoi(argv[2]);
    }

    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0) {
        fprintf(stderr, "failed to get page size\n");
        return 1;
    }

    pin_to_cpu(0);

    printf("page size: %ld\n", page_size);
    printf("CPU pinned to 0 (if sched_setaffinity succeeded)\n");
    printf("tlb_iters=%d, memcpy_iters=%d\n\n",
           tlb_iters, memcpy_iters);

    for (size_t idx = 0; idx < num_sizes; idx++) {
        size_t pages = pages_list[idx];
        size_t bytes = pages * (size_t)page_size;

        printf("\n==============================\n");
        printf("### TEST size: %zu pages (%zu bytes) ###\n\n",
               pages, bytes);

        // 1) TLB flush bench
        tlb_flush_bench(pages, tlb_iters, page_size);

        printf("\n--- sleep(5) before memcpy bench ---\n\n");
        sleep(5);

        // 2) memcpy bench
        memcpy_bench(bytes, memcpy_iters);

        printf("\n--- sleep(2) before next size ---\n");
        sleep(2);
    }

    return 0;
}

