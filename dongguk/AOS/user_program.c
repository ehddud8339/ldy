#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>

static volatile sig_atomic_t exiting = 0;
static void sig_int(int signo) {
  exiting = 1;
}

void heap_allocate(int bytes) {
    int local = 26;
    void *ptr = malloc(4096);
    if (!ptr) {
        fprintf(stderr, "failed to malloc\n");
        return;
    }
    printf("location of stack   : %p\n", (void *)&local);
    printf("location of heap    : %p\n", ptr);
    fflush(stdout);
}

static inline unsigned long long now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (unsigned long long)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

int main(int argc, char *argv[]) {
    int bytes = 4096;
    int iter = 1;

    signal(SIGINT,  sig_int);
    signal(SIGTERM, sig_int);

    // 첫 호출
    unsigned long long start = now_ns();
    heap_allocate(bytes * iter++);
    unsigned long long end = now_ns();
    printf("duration(ns): %llu\n", end - start);

    while(!exiting) {
        int ch = getchar();
        if (ch == EOF) break;
        if (ch != '\n') continue;

        start = now_ns();
        heap_allocate(bytes * iter++);
        end = now_ns();
        printf("duration(ns): %llu\n", end - start);
    }

    return 0;
}
