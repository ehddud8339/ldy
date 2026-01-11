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
 * size parser (e.g., 40M, 4K, 1G, 100G)
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
 * mode
 * ========================= */
typedef enum {
    MODE_PER_THREAD = 0,
    MODE_SHARED     = 1,
} file_mode_t;

static char g_prefix[384] = "/mnt/test/testfile_"; // per-thread default
static char g_shared_path[384] = "/mnt/test/shared_100g"; // shared default

/* =========================
 * thread args
 * ========================= */
typedef struct {
    int             tid;
    size_t          block_size;

    file_mode_t     mode;
    off_t           limit_size;     // 0이면 실제 파일 크기 사용
    const char     *shared_path;    // mode==SHARED일 때 사용
    const char     *prefix;         // mode==PER_THREAD일 때 사용

    start_gate_t   *start_gate;
    ready_gate_t   *ready_gate;

    uint64_t        target_ops;
    uint64_t        ops_count;
    uint64_t        err_count;

    int             open_flags;
} thread_arg_t;

static void usage(const char *prog)
{
    fprintf(stderr,
        "Usage: %s <num_threads> <ops_per_thread>\n"
        "  [--mode=shared|perthread]\n"
        "  [--shared=/path/to/100g_file]        (mode=shared)\n"
        "  [--prefix=/mnt/test/testfile_]       (mode=perthread; file is <prefix><tid>)\n"
        "  [--limit=100G|1G|40M|...]            (limit random range; file must be >= limit)\n"
        "  [--bs=4K|...]                         (default 4K)\n"
        "  [--direct]                            (O_DIRECT)\n",
        prog);
}

static int open_existing_and_get_size(const char *path, int open_flags, off_t *out_size, int *out_fd)
{
    int fd = open(path, open_flags);
    if (fd < 0) return -1;

    struct stat st;
    if (fstat(fd, &st) != 0) {
        close(fd);
        return -1;
    }
    if (st.st_size <= 0) {
        close(fd);
        errno = EINVAL;
        return -1;
    }

    *out_fd = fd;
    *out_size = (off_t)st.st_size;
    return 0;
}

static void *reader_thread(void *arg)
{
    thread_arg_t *t = (thread_arg_t *)arg;

    char path[512];
    if (t->mode == MODE_SHARED) {
        snprintf(path, sizeof(path), "%s", t->shared_path);
    } else {
        snprintf(path, sizeof(path), "%s%d", t->prefix, t->tid);
    }

    off_t file_size = 0;
    int fd = -1;
    if (open_existing_and_get_size(path, t->open_flags, &file_size, &fd) != 0) {
        t->err_count++;
        fprintf(stderr, "[Log] error: open/fstat failed: %s (%s)\n", path, strerror(errno));
        ready_gate_signal_ready(t->ready_gate);
        return NULL;
    }

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
        fprintf(stderr, "[Log] error: posix_memalign failed\n");
        close(fd);
        ready_gate_signal_ready(t->ready_gate);
        return NULL;
    }
    memset(buf, 0, t->block_size);

    ready_gate_signal_ready(t->ready_gate);
    start_gate_wait(t->start_gate);

    off_t max_blocks = file_size / (off_t)t->block_size;
    uint32_t seed = (uint32_t)(time(NULL) ^ (t->tid * 0x9E3779B9u));

    uint64_t ops = 0, err = 0;
    while (!atomic_load_explicit(&g_stop, memory_order_relaxed) && ops < t->target_ops) {
        off_t offset = (off_t)(xorshift32(&seed) % (uint32_t)max_blocks) * (off_t)t->block_size;

        ssize_t n = pread(fd, buf, t->block_size, offset);
        if (n == (ssize_t)t->block_size) {
            ops++;
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

    file_mode_t mode = MODE_PER_THREAD;
    size_t block_size = 4096;
    off_t limit_size = 0;

    int open_flags = O_RDONLY;

    for (int i = 3; i < argc; i++) {
        if (strncmp(argv[i], "--mode=", 7) == 0) {
            const char *m = argv[i] + 7;
            if (strcmp(m, "shared") == 0) mode = MODE_SHARED;
            else if (strcmp(m, "perthread") == 0) mode = MODE_PER_THREAD;
            else {
                fprintf(stderr, "[Log] error: invalid --mode=%s\n", m);
                return 1;
            }
        } else if (strncmp(argv[i], "--shared=", 9) == 0) {
            const char *p = argv[i] + 9;
            if (!*p || strlen(p) >= sizeof(g_shared_path)) {
                fprintf(stderr, "[Log] error: invalid --shared path\n");
                return 1;
            }
            strcpy(g_shared_path, p);
        } else if (strncmp(argv[i], "--prefix=", 9) == 0) {
            const char *p = argv[i] + 9;
            if (!*p || strlen(p) >= sizeof(g_prefix)) {
                fprintf(stderr, "[Log] error: invalid --prefix\n");
                return 1;
            }
            strcpy(g_prefix, p);
        } else if (strncmp(argv[i], "--limit=", 8) == 0) {
            unsigned long long v = 0;
            if (parse_size_bytes(argv[i] + 8, &v) != 0 || v == 0) {
                fprintf(stderr, "[Log] error: invalid --limit value: %s\n", argv[i] + 8);
                return 1;
            }
            limit_size = (off_t)v;
        } else if (strncmp(argv[i], "--bs=", 5) == 0) {
            unsigned long long v = 0;
            if (parse_size_bytes(argv[i] + 5, &v) != 0 || v == 0) {
                fprintf(stderr, "[Log] error: invalid --bs value: %s\n", argv[i] + 5);
                return 1;
            }
            block_size = (size_t)v;
        } else if (strcmp(argv[i], "--direct") == 0) {
#ifdef O_DIRECT
            open_flags |= O_DIRECT;
#else
            fprintf(stderr, "[Log] warn: O_DIRECT not supported.\n");
#endif
        } else {
            fprintf(stderr, "[Log] warn: unknown option: %s\n", argv[i]);
            usage(argv[0]);
            return 1;
        }
    }

    if (block_size == 0 || (block_size & (block_size - 1)) != 0) {
        fprintf(stderr, "[Log] error: block size should be power-of-two (got %zu)\n", block_size);
        return 1;
    }
    if (limit_size > 0 && (limit_size % (off_t)block_size) != 0) {
        fprintf(stderr, "[Log] error: --limit must be multiple of bs\n");
        return 1;
    }

    printf("[Config] threads=%d ops/thread=%llu bs=%zu mode=%s\n",
           num_threads, (unsigned long long)ops_per_thread, block_size,
           (mode == MODE_SHARED) ? "shared" : "perthread");
    printf("[Config] open_flags=0x%x\n", open_flags);
    if (mode == MODE_SHARED) printf("[Config] shared=%s\n", g_shared_path);
    else printf("[Config] prefix=%s (file: %s<tid>)\n", g_prefix, g_prefix);
    if (limit_size > 0) printf("[Config] limit=%lld\n", (long long)limit_size);

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

    printf("[Log] Create threads (%d)\n", num_threads);

    for (int i = 0; i < num_threads; i++) {
        args[i].tid = i;
        args[i].block_size = block_size;
        args[i].mode = mode;
        args[i].limit_size = limit_size;
        args[i].shared_path = g_shared_path;
        args[i].prefix = g_prefix;
        args[i].start_gate = &start_gate;
        args[i].ready_gate = &ready_gate;
        args[i].target_ops = ops_per_thread;
        args[i].ops_count = 0;
        args[i].err_count = 0;
        args[i].open_flags = open_flags;

        int rc = pthread_create(&threads[i], NULL, reader_thread, &args[i]);
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
    printf("[Log] All threads ready\n");

    printf("[Log] Start I/O\n");
    struct timespec ts0, ts1;
    clock_gettime(CLOCK_MONOTONIC, &ts0);
    start_gate_broadcast(&start_gate);

    uint64_t total_ops = 0, total_err = 0;
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
        total_ops += args[i].ops_count;
        total_err += args[i].err_count;
    }
    clock_gettime(CLOCK_MONOTONIC, &ts1);

    double elapsed = ts_diff_sec(&ts0, &ts1);
    double iops = (elapsed > 0.0) ? ((double)total_ops / elapsed) : 0.0;
    double mbps = (elapsed > 0.0)
        ? ((double)(total_ops * block_size) / (1024.0 * 1024.0) / elapsed)
        : 0.0;

    printf("------------------------------------------------\n");
    printf("[Result] threads      : %d\n", num_threads);
    printf("[Result] ops/thread   : %llu\n", (unsigned long long)ops_per_thread);
    printf("[Result] total_reads  : %llu (full reads only)\n", (unsigned long long)total_ops);
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

