#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <stdatomic.h>
#include <limits.h>

/* =========================
 * globals
 * ========================= */
static atomic_bool g_stop = ATOMIC_VAR_INIT(0);

/* =========================
 * utils
 * ========================= */
static void raise_fd_limit(void)
{
    struct rlimit lim;
    if (getrlimit(RLIMIT_NOFILE, &lim) == 0) {
        lim.rlim_cur = lim.rlim_max;
        if (setrlimit(RLIMIT_NOFILE, &lim) != 0)
            fprintf(stderr, "[Log] warn: setrlimit failed: %s\n", strerror(errno));
    }
}

static double ts_diff_sec(const struct timespec *a, const struct timespec *b)
{
    double sec = (double)(b->tv_sec - a->tv_sec);
    double nsec = (double)(b->tv_nsec - a->tv_nsec) / 1e9;
    return sec + nsec;
}

/* =========================
 * start gate (broadcast)
 * ========================= */
typedef struct {
    pthread_mutex_t mtx;
    pthread_cond_t  cv;
    int             started;
} start_gate_t;

static void start_gate_init(start_gate_t *g)
{
    pthread_mutex_init(&g->mtx, NULL);
    pthread_cond_init(&g->cv, NULL);
    g->started = 0;
}

static void start_gate_destroy(start_gate_t *g)
{
    pthread_cond_destroy(&g->cv);
    pthread_mutex_destroy(&g->mtx);
}

static void start_gate_wait(start_gate_t *g)
{
    pthread_mutex_lock(&g->mtx);
    while (!g->started) pthread_cond_wait(&g->cv, &g->mtx);
    pthread_mutex_unlock(&g->mtx);
}

static void start_gate_broadcast(start_gate_t *g)
{
    pthread_mutex_lock(&g->mtx);
    g->started = 1;
    pthread_cond_broadcast(&g->cv);
    pthread_mutex_unlock(&g->mtx);
}

/* =========================
 * ready gate (all threads open+buffer ready)
 * ========================= */
typedef struct {
    pthread_mutex_t mtx;
    pthread_cond_t  cv;
    int             ready;
    int             total;
} ready_gate_t;

static void ready_gate_init(ready_gate_t *rg, int total)
{
    pthread_mutex_init(&rg->mtx, NULL);
    pthread_cond_init(&rg->cv, NULL);
    rg->ready = 0;
    rg->total = total;
}

static void ready_gate_destroy(ready_gate_t *rg)
{
    pthread_cond_destroy(&rg->cv);
    pthread_mutex_destroy(&rg->mtx);
}

static void ready_gate_signal_ready(ready_gate_t *rg)
{
    pthread_mutex_lock(&rg->mtx);
    rg->ready++;
    if (rg->ready >= rg->total)
        pthread_cond_signal(&rg->cv);
    pthread_mutex_unlock(&rg->mtx);
}

static void ready_gate_wait_all(ready_gate_t *rg)
{
    pthread_mutex_lock(&rg->mtx);
    while (rg->ready < rg->total)
        pthread_cond_wait(&rg->cv, &rg->mtx);
    pthread_mutex_unlock(&rg->mtx);
}

/* =========================
 * PRNG: xorshift32
 * ========================= */
static inline uint32_t xorshift32(uint32_t *s)
{
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x;
    return x;
}

/* =========================
 * size parser (e.g., 40M, 4K, 1G)
 * ========================= */
static int parse_size_bytes(const char *s, unsigned long long *out)
{
    if (!s || !*s) return -1;

    char *end = NULL;
    errno = 0;
    unsigned long long v = strtoull(s, &end, 10);
    if (errno != 0) return -1;
    if (end == s) return -1;

    unsigned long long mul = 1;
    if (*end != '\0') {
        if (end[1] != '\0') return -1; // one suffix char only
        switch (*end) {
            case 'K': case 'k': mul = 1024ULL; break;
            case 'M': case 'm': mul = 1024ULL * 1024ULL; break;
            case 'G': case 'g': mul = 1024ULL * 1024ULL * 1024ULL; break;
            default: return -1;
        }
    }

    if (v > ULLONG_MAX / mul) return -1;
    *out = v * mul;
    return 0;
}

/* =========================
 * thread args
 * ========================= */
typedef struct {
    int             tid;
    size_t          block_size;

    // 0이면 fstat()로 실제 파일 크기 사용
    // 0이 아니면 "랜덤 범위 제한" 용도로 사용 (실제 파일 크기보다 작아야 함)
    off_t           limit_size;

    start_gate_t   *start_gate;
    ready_gate_t   *ready_gate;

    uint64_t        target_ops;
    uint64_t        ops_count;
    uint64_t        err_count;

    int             open_flags;
    int             fdatasync_every;
} thread_arg_t;

static void usage(const char *prog)
{
    fprintf(stderr,
        "Usage: %s <num_threads> <ops_per_thread>\n"
        "       [--direct] [--dsync] [--fdatasync=N]\n"
        "       [--filesize=40M]    (limit random range; file must be >= this)\n"
        "       [--path_prefix=/mnt/test/testfile_]\n",
        prog);
}

static void *writer_thread(void *arg)
{
    thread_arg_t *t = (thread_arg_t *)arg;

    char path[512];
    // 기본: /mnt/test/testfile_<tid>
    // main에서 prefix를 바꿀 수 있게 하려면 전역/인자에 prefix를 넣으면 됨.
    // 여기서는 간단히 고정 prefix를 쓰되, 아래 main에서 override 가능하게 구현함.
    // (main에서 t->tid를 제외한 path를 이미 세팅해도 됨)
    // -> 이 구현에서는 main에서 path_prefix 전역을 사용.
    extern char g_path_prefix[384];
    snprintf(path, sizeof(path), "%s%d", g_path_prefix, t->tid);

    // CREATE/TRUNC 없음: 반드시 기존 파일이 있어야 함
    int fd = open(path, t->open_flags);
    if (fd < 0) {
        t->err_count++;
        fprintf(stderr, "[Log] error: open %s failed: %s\n", path, strerror(errno));
        ready_gate_signal_ready(t->ready_gate);
        return NULL;
    }

    struct stat st;
    if (fstat(fd, &st) != 0) {
        t->err_count++;
        fprintf(stderr, "[Log] error: fstat %s failed: %s\n", path, strerror(errno));
        close(fd);
        ready_gate_signal_ready(t->ready_gate);
        return NULL;
    }

    off_t file_size = st.st_size;
    if (file_size <= 0) {
        t->err_count++;
        fprintf(stderr, "[Log] error: file %s size is %lld\n", path, (long long)file_size);
        close(fd);
        ready_gate_signal_ready(t->ready_gate);
        return NULL;
    }

    // 랜덤 범위 제한 옵션이 있으면 그 범위까지만 사용
    if (t->limit_size > 0) {
        if (file_size < t->limit_size) {
            t->err_count++;
            fprintf(stderr, "[Log] error: file %s size(%lld) < limit(%lld)\n",
                    path, (long long)file_size, (long long)t->limit_size);
            close(fd);
            ready_gate_signal_ready(t->ready_gate);
            return NULL;
        }
        file_size = t->limit_size;
    }

    if ((file_size % (off_t)t->block_size) != 0) {
        t->err_count++;
        fprintf(stderr, "[Log] error: file %s size(%lld) not multiple of block(%zu)\n",
                path, (long long)file_size, t->block_size);
        close(fd);
        ready_gate_signal_ready(t->ready_gate);
        return NULL;
    }

    void *buf = NULL;
    if (posix_memalign(&buf, t->block_size, t->block_size) != 0) {
        t->err_count++;
        close(fd);
        ready_gate_signal_ready(t->ready_gate);
        return NULL;
    }

    memset(buf, (unsigned char)(0xA5 ^ (t->tid & 0xFF)), t->block_size);

    ready_gate_signal_ready(t->ready_gate);
    start_gate_wait(t->start_gate);

    off_t max_blocks = file_size / (off_t)t->block_size;
    if (max_blocks <= 0 || t->target_ops == 0) {
        free(buf);
        close(fd);
        return NULL;
    }

    uint32_t seed = (uint32_t)(time(NULL) ^ (t->tid * 0x9E3779B9u));

    uint64_t ops = 0, err = 0;
    while (!atomic_load_explicit(&g_stop, memory_order_relaxed) && ops < t->target_ops) {
        off_t offset = (off_t)(xorshift32(&seed) % (uint32_t)max_blocks) * (off_t)t->block_size;

        ssize_t n = pwrite(fd, buf, t->block_size, offset);
        if (n == (ssize_t)t->block_size) {
            ops++;
            if (t->fdatasync_every > 0 && (ops % (uint64_t)t->fdatasync_every) == 0) {
                if (fdatasync(fd) != 0) err++;
            }
        } else if (n < 0 && errno == EINTR) {
            continue;
        } else {
            err++;
        }
    }

    t->ops_count = ops;
    t->err_count += err;

    free(buf);
    close(fd);
    return NULL;
}

/* path prefix (default) */
char g_path_prefix[384] = "/mnt/test/testfile_";

int main(int argc, char **argv)
{
    if (argc < 3) {
        usage(argv[0]);
        return 1;
    }

    raise_fd_limit();

    int num_threads = atoi(argv[1]);
    long long ops_ll = atoll(argv[2]);
    if (num_threads <= 0 || ops_ll <= 0) {
        usage(argv[0]);
        return 1;
    }

    uint64_t ops_per_thread = (uint64_t)ops_ll;
    size_t block_size = 4096;

    // CREATE/TRUNC 제거: 일반 OPEN
    int open_flags = O_WRONLY;
    int fdatasync_every = 0;

    // 0이면 실제 파일 크기 사용. 0이 아니면 랜덤 범위 제한.
    off_t limit_size = 0;

    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "--direct") == 0) {
#ifdef O_DIRECT
            open_flags |= O_DIRECT;
#else
            fprintf(stderr, "[Log] warn: O_DIRECT not supported.\n");
#endif
        } else if (strcmp(argv[i], "--dsync") == 0) {
            open_flags |= O_DSYNC;
        } else if (strncmp(argv[i], "--fdatasync=", 12) == 0) {
            fdatasync_every = atoi(argv[i] + 12);
            if (fdatasync_every < 0) fdatasync_every = 0;
        } else if (strncmp(argv[i], "--filesize=", 11) == 0) {
            unsigned long long v = 0;
            if (parse_size_bytes(argv[i] + 11, &v) != 0 || v == 0) {
                fprintf(stderr, "[Log] error: invalid --filesize value: %s\n", argv[i] + 11);
                return 1;
            }
            limit_size = (off_t)v;
        } else if (strncmp(argv[i], "--path_prefix=", 14) == 0) {
            const char *p = argv[i] + 14;
            if (!*p) {
                fprintf(stderr, "[Log] error: empty --path_prefix\n");
                return 1;
            }
            if (strlen(p) >= sizeof(g_path_prefix)) {
                fprintf(stderr, "[Log] error: --path_prefix too long\n");
                return 1;
            }
            strcpy(g_path_prefix, p);
        } else {
            fprintf(stderr, "[Log] warn: unknown option: %s\n", argv[i]);
            usage(argv[0]);
            return 1;
        }
    }

    printf("[Config] threads=%d ops_per_thread=%llu block=%zu\n",
           num_threads, (unsigned long long)ops_per_thread, block_size);
    printf("[Config] open_flags=0x%x fdatasync_every=%d\n", open_flags, fdatasync_every);
    printf("[Config] path_prefix=%s (file per thread: %s<tid>)\n", g_path_prefix, g_path_prefix);
    if (limit_size > 0) {
        printf("[Config] limit_size=%lld (random range limited)\n", (long long)limit_size);
        if ((limit_size % (off_t)block_size) != 0) {
            fprintf(stderr, "[Log] error: --filesize must be multiple of block_size\n");
            return 1;
        }
    }

    pthread_t *threads = calloc((size_t)num_threads, sizeof(pthread_t));
    thread_arg_t *args = calloc((size_t)num_threads, sizeof(thread_arg_t));
    if (!threads || !args) {
        fprintf(stderr, "[Log] error: calloc failed\n");
        free(threads);
        free(args);
        return 1;
    }

    start_gate_t start_gate;
    ready_gate_t ready_gate;

    start_gate_init(&start_gate);
    ready_gate_init(&ready_gate, num_threads);
    atomic_store_explicit(&g_stop, 0, memory_order_relaxed);

    printf("[Log] Create threads (%d)\n", num_threads);

    for (int i = 0; i < num_threads; i++) {
        args[i].tid = i;
        args[i].block_size = block_size;
        args[i].limit_size = limit_size;
        args[i].start_gate = &start_gate;
        args[i].ready_gate = &ready_gate;
        args[i].target_ops = ops_per_thread;
        args[i].ops_count = 0;
        args[i].err_count = 0;
        args[i].open_flags = open_flags;
        args[i].fdatasync_every = fdatasync_every;

        int rc = pthread_create(&threads[i], NULL, writer_thread, &args[i]);
        if (rc != 0) {
            fprintf(stderr, "[Log] error: pthread_create(%d) failed: %s\n", i, strerror(rc));
            atomic_store_explicit(&g_stop, 1, memory_order_relaxed);
            start_gate_broadcast(&start_gate);
            for (int j = 0; j < i; j++) pthread_join(threads[j], NULL);
            ready_gate_destroy(&ready_gate);
            start_gate_destroy(&start_gate);
            free(threads);
            free(args);
            return 1;
        }
    }

    printf("[Log] Wait all threads open+buffer ready\n");
    ready_gate_wait_all(&ready_gate);
    printf("[Log] All threads ready (open+buf done)\n");

    printf("[Log] Start I/O (broadcast start)\n");
    struct timespec start_ts, done_ts;
    clock_gettime(CLOCK_MONOTONIC, &start_ts);
    start_gate_broadcast(&start_gate);

    printf("[Log] Wait threads complete (join)\n");
    uint64_t total_ops = 0, total_err = 0;
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
        total_ops += args[i].ops_count;
        total_err += args[i].err_count;
    }
    clock_gettime(CLOCK_MONOTONIC, &done_ts);

    double elapsed = ts_diff_sec(&start_ts, &done_ts);
    double iops = (elapsed > 0.0) ? ((double)total_ops / elapsed) : 0.0;
    double mbps = (elapsed > 0.0)
        ? ((double)(total_ops * block_size) / (1024.0 * 1024.0) / elapsed)
        : 0.0;

    printf("------------------------------------------------\n");
    printf("[Result] threads      : %d\n", num_threads);
    printf("[Result] ops/thread   : %llu\n", (unsigned long long)ops_per_thread);
    printf("[Result] total_writes : %llu (full writes only)\n", (unsigned long long)total_ops);
    printf("[Result] total_errors : %llu\n", (unsigned long long)total_err);
    printf("[Result] elapsed      : %.6f sec\n", elapsed);
    printf("[Result] IOPS         : %.2f\n", iops);
    printf("[Result] Throughput   : %.2f MB/s\n", mbps);
    printf("------------------------------------------------\n");

    ready_gate_destroy(&ready_gate);
    start_gate_destroy(&start_gate);
    free(threads);
    free(args);
    return 0;
}

