// remap_test.c
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <time.h>
#include <errno.h>
#include <string.h>

#define REMAP_BENCH_IOC _IOR('r', 1, uint64_t)

int main(int argc, char **argv)
{
    const char *dev = "/dev/remap_bench";
    int fd;
    void *addr;
    int i, iters = 100000; // 실험 반복 횟수
    uint64_t ns;
    long double sum = 0.0L;
    uint64_t min = (uint64_t)-1;
    uint64_t max = 0;
    int len = 4096;

    if (argc > 1)
        iters = atoi(argv[1]);

    fd = open(dev, O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    addr = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return 1;
    }

    // 미리 한 번 접근해서 TLB warm-up
    volatile char *p = (volatile char *)addr;
    p[0] = 1;

    for (i = 0; i < iters; i++) {
        if (ioctl(fd, REMAP_BENCH_IOC, &ns) < 0) {
            perror("ioctl");
            munmap(addr, len);
            close(fd);
            return 1;
        }

        if (ns < min) min = ns;
        if (ns > max) max = ns;
        sum += (long double)ns;

        // 매핑이 실제로 바뀌었는지 약간 건드려 보기 (성능 영향은 매우 미미)
        (void)p[0];
    }

    printf("iterations: %d\n", iters);
    printf("avg remap+zap time: %.2Lf ns\n", sum / iters);
    printf("min: %lu ns, max: %lu ns\n",
           (unsigned long)min, (unsigned long)max);

    munmap(addr, len);
    close(fd);
    return 0;
}

