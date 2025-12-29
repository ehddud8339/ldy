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

#define DEFAULT_FILE_GB 1
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
    double sec  = (double)(b->tv_sec  - a->tv_sec);
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
 * ready gate (all threads open+buf ready)
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
 * dd-precreated file check
 * ========================= */
static int check_precreated_file(const char *path, off_t min_size)
{
    struct stat st;
    if (stat(path, &st) != 0) {
        fprintf(stderr, "[Log] error: stat(%s) failed: %s\n", path, strerror(errno));
        return -1;
    }
    if (!S_ISREG(st.st_mode)) {
        fprintf(stderr, "[Log] error: %s is not a regular file\n", path);
        return -1;
    }
    if ((off_t)st.st_size < min_size) {
        fprintf(stderr, "[Log] error: %s size too small: have=%lld need>=%lld\n",
                path, (long long)st.st_size, (long long)min_size);
        return -1;
    }
    return 0;
}

/* =========================
 * thread args
 * ========================= */
typedef struct {
    int             tid;
    size_t          block_size;
    off_t           file_size;

    /* either shared single file, or per-thread prefix */
    const char     *shared_path;     /* used when file_prefix == N */
    const char     *file_prefix;     /* if not N: use "<prefix>.<tid>" */

    start_gate_t   *start_gate;
    ready_gate_t   *ready_gate;

    uint64_t        target_ops;
    uint64_t        ops_count;    /* full reads only */
    uint64_t        err_count;
} thread_arg_t;

static void usage(const char *prog)
{
    fprintf(stderr,
        "Usage: %s <num_threads> <ops_per_thread> [--filesize_gb=N] [--file=PATH] [--file_prefix=PREFIX]\n"
        "  NOTE: file(s) must be pre-created by dd (or equivalent) and size must be >= filesize_gb\n"
        "  If --file_prefix is set, each thread uses: PREFIX.<tid>  (e.g., /mnt/test/readfile.0)\n",
        prog);
}

static int build_thread_path(char *dst, size_t dst_len, const char *prefix, int tid)
{
    if (!dst || dst_len == 0 || !prefix) return -1;
    int n = snprintf(dst, dst_len, "%s.%d", prefix, tid);
    if (n < 0 || (size_t)n >= dst_len) return -1;
    return 0;
}

/* =========================
 * reader thread
 * ========================= */
static void *reader_thread(void *arg)
{
    thread_arg_t *t = (thread_arg_t *)arg;

    char pathbuf[PATH_MAX];
    const char *path_to_open = t->shared_path;

    if (t->file_prefix) {
        if (build_thread_path(pathbuf, sizeof(pathbuf), t->file_prefix, t->tid) != 0) {
            t->err_count++;
            ready_gate_signal_ready(t->ready_gate);
            return NULL;
        }
        path_to_open = pathbuf;
    }

    int fd = open(path_to_open, O_RDONLY);
    if (fd < 0) {
        t->err_count++;
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

    /* open + buf ready */
    ready_gate_signal_ready(t->ready_gate);

    /* synchronized start */
    start_gate_wait(t->start_gate);

    off_t max_blocks = t->file_size / (off_t)t->block_size;
    if (max_blocks <= 0 || t->target_ops == 0) {
        free(buf);
        close(fd);
        return NULL;
    }

    uint32_t seed = (uint32_t)(time(NULL) ^ (t->tid * 0x9E3779B9u));

    uint64_t ops = 0, err = 0;
    while (!atomic_load_explicit(&g_stop, memory_order_relaxed) && ops < t->target_ops) {
        off_t offset = (off_t)(xorshift32(&seed) % (uint32_t)max_blocks) * (off_t)t->block_size;

        ssize_t ret = pread(fd, buf, t->block_size, offset);
        if (ret == (ssize_t)t->block_size) {
            ops++;
        } else if (ret < 0 && errno == EINTR) {
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

    unsigned long long file_gb = DEFAULT_FILE_GB;
    const char *shared_path = "/mnt/test/shared_readfile";
    const char *file_prefix = NULL;

    for (int i = 3; i < argc; i++) {
        if (strncmp(argv[i], "--filesize_gb=", 13) == 0) {
            file_gb = strtoull(argv[i] + 13, NULL, 10);
            if (file_gb == 0) file_gb = DEFAULT_FILE_GB;
        } else if (strncmp(argv[i], "--file=", 7) == 0) {
            shared_path = argv[i] + 7;
        } else if (strncmp(argv[i], "--file_prefix=", 14) == 0) {
            file_prefix = argv[i] + 14;
        } else {
            fprintf(stderr, "[Log] warn: unknown option: %s\n", argv[i]);
            usage(argv[0]);
            return 1;
        }
    }

    size_t block_size = 4096;
    off_t file_size = (off_t)(file_gb * 1024 * 1024 * 1024);

    printf("[Config] threads=%d ops_per_thread=%llu block=%zu file_size=%llu\n",
           num_threads, (unsigned long long)ops_per_thread, block_size,
           (unsigned long long)(file_gb * 1024 * 1024 * 1024));

    if (file_prefix) {
        printf("[Config] file_mode    : per-thread files\n");
        printf("[Config] file_prefix  : %s (each uses %s.<tid>)\n", file_prefix, file_prefix);
    } else {
        printf("[Config] file_mode    : shared single file\n");
        printf("[Config] shared_file  : %s\n", shared_path);
    }
    printf("[Config] filesize_gb   : %llu\n", (unsigned long long)file_gb);

    /* precheck files */
    if (file_prefix) {
        for (int i = 0; i < num_threads; i++) {
            char p[PATH_MAX];
            if (build_thread_path(p, sizeof(p), file_prefix, i) != 0) {
                fprintf(stderr, "[Log] error: path too long for tid=%d\n", i);
                return 1;
            }
            if (check_precreated_file(p, file_size) != 0) {
                fprintf(stderr, "[Log] error: pre-created file check failed: %s\n", p);
                return 1;
            }
        }
    } else {
        if (check_precreated_file(shared_path, file_size) != 0) {
            fprintf(stderr, "[Log] error: pre-created file check failed\n");
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

    int created = 0;
    for (int i = 0; i < num_threads; i++) {
        args[i].tid = i;
        args[i].block_size = block_size;
        args[i].file_size = file_size;
        args[i].shared_path = shared_path;
        args[i].file_prefix = file_prefix;
        args[i].start_gate = &start_gate;
        args[i].ready_gate = &ready_gate;
        args[i].target_ops = ops_per_thread;
        args[i].ops_count = 0;
        args[i].err_count = 0;

        int rc = pthread_create(&threads[i], NULL, reader_thread, &args[i]);
        if (rc != 0) {
            fprintf(stderr, "[Log] error: pthread_create(%d) failed: %s\n", i, strerror(rc));
            created = i;
            atomic_store_explicit(&g_stop, 1, memory_order_relaxed);
            start_gate_broadcast(&start_gate);
            for (int j = 0; j < created; j++) pthread_join(threads[j], NULL);
            ready_gate_destroy(&ready_gate);
            start_gate_destroy(&start_gate);
            free(threads);
            free(args);
            return 1;
        }
        created = i + 1;
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

