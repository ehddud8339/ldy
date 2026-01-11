// user/fuse_trace_user.c (direct libbpf loader, no skeleton; libbpf 0.5.x friendly iteration)

#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include <unistd.h>
#include <sys/resource.h>

#include <bpf/libbpf.h>
#include <bpf/bpf.h>

#include "fuse_trace_common.h"

static volatile sig_atomic_t exiting = 0;
static FILE *csv_fp = NULL;

static void sig_handler(int sig)
{
    (void)sig;
    exiting = 1;
}

static int libbpf_print_fn(enum libbpf_print_level level,
                           const char *format, va_list args)
{
    (void)level;
    return vfprintf(stderr, format, args);
}

static void bump_memlock_rlimit(void)
{
    struct rlimit r = {RLIM_INFINITY, RLIM_INFINITY};
    if (setrlimit(RLIMIT_MEMLOCK, &r) != 0) {
        fprintf(stderr, "warn: setrlimit(RLIMIT_MEMLOCK) failed: %s\n", strerror(errno));
    }
}

static const char *opcode_name(uint32_t opcode)
{
    switch (opcode) {
    case 1:  return "LOOKUP";
    case 2:  return "FORGET";
    case 3:  return "GETATTR";
    case 4:  return "SETATTR";
    case 5:  return "READLINK";
    case 6:  return "SYMLINK";
    case 8:  return "MKNOD";
    case 9:  return "MKDIR";
    case 10: return "UNLINK";
    case 11: return "RMDIR";
    case 12: return "RENAME";
    case 13: return "LINK";
    case 14: return "OPEN";
    case 15: return "READ";
    case 16: return "WRITE";
    case 17: return "STATFS";
    case 18: return "RELEASE";
    case 20: return "FSYNC";
    case 21: return "SETXATTR";
    case 22: return "GETXATTR";
    case 23: return "LISTXATTR";
    case 24: return "REMOVEXATTR";
    case 25: return "FLUSH";
    case 26: return "INIT";
    case 27: return "OPENDIR";
    case 28: return "READDIR";
    case 29: return "RELEASEDIR";
    case 30: return "FSYNCDIR";
    case 31: return "GETLK";
    case 32: return "SETLK";
    case 33: return "SETLKW";
    case 34: return "ACCESS";
    case 35: return "CREATE";
    case 36: return "INTERRUPT";
    case 37: return "BMAP";
    case 38: return "DESTROY";
    case 39: return "IOCTL";
    case 40: return "POLL";
    case 41: return "NOTIFY_REPLY";
    case 42: return "BATCH_FORGET";
    case 43: return "FALLOCATE";
    case 44: return "READDIRPLUS";
    case 45: return "RENAME2";
    case 46: return "LSEEK";
    case 47: return "COPY_FILE_RANGE";
    case 4096: return "CUSE_INIT";
    default: return "UNKNOWN";
    }
}

static void print_event_human(const struct fuse_req_event_v1 *e)
{
    uint64_t queuing_us  = e->queuing_ns / 1000;
    uint64_t sched_us    = e->sched_delay_ns / 1000;
    uint64_t daemon_us   = e->daemon_ns / 1000;
    uint64_t response_us = e->response_ns / 1000;

    printf("unique=%" PRIu64 " op=%s err=%d flags=0x%x | "
           "queuing=%" PRIu64 "us sched=%" PRIu64 "us daemon=%" PRIu64 "us resp=%" PRIu64 "us | "
           "k_tid=%u k_cpu=%u d_tid=%u d_cpu=%u d_tgid=%u\n",
           e->unique,
           opcode_name(e->opcode),
           e->err,
           e->flags,
           queuing_us, sched_us, daemon_us, response_us,
           e->k_tid, e->k_cpu, e->d_tid, e->d_cpu, e->d_tgid);
}

static int handle_event(void *ctx, void *data, size_t len)
{
    (void)ctx;

    if (len < sizeof(struct fuse_req_event_v1))
        return 0;

    const struct fuse_req_event_v1 *e = (const struct fuse_req_event_v1 *)data;

    print_event_human(e);

    if (csv_fp) {
        uint64_t queuing_us  = e->queuing_ns / 1000;
        uint64_t sched_us    = e->sched_delay_ns / 1000;
        uint64_t daemon_us   = e->daemon_ns / 1000;
        uint64_t response_us = e->response_ns / 1000;

        fprintf(csv_fp,
                "%" PRIu64 ","              // ts_queue_ns
                "%" PRIu64 ","              // ts_recv_ns
                "%" PRIu64 ","              // ts_send_ns
                "%" PRIu64 ","              // ts_end_ns
                "%" PRIu64 ","              // unique
                "%s,"                       // op
                "%d,"                       // err
                "0x%x,"                     // flags
                "%u,%u,%u,%u,%u,"           // k_tid,k_cpu,d_tgid,d_tid,d_cpu
                "%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 "," // queuing,sched,daemon,response (us)
                "\"%.*s\",\"%.*s\"\n",      // k_comm, d_comm
                e->ts_queue_ns,
                e->ts_recv_ns,
                e->ts_send_ns,
                e->ts_end_ns,
                e->unique,
                opcode_name(e->opcode),
                e->err,
                e->flags,
                e->k_tid, e->k_cpu,
                e->d_tgid, e->d_tid, e->d_cpu,
                queuing_us, sched_us, daemon_us, response_us,
                TASK_COMM_LEN, e->k_comm, TASK_COMM_LEN, e->d_comm);

        fflush(csv_fp);
    }

    return 0;
}

static void usage(const char *prog)
{
    fprintf(stderr,
        "Usage:\n"
        "  %s <daemon_bin> <recv_off_hex> <libfuse_so> <send_off_hex>\n"
        "     [--pid N] [--csv PATH] [--obj PATH]\n"
        "\n"
        "Default:\n"
        "  --pid -1 (all)\n"
        "  --csv fuse_trace.csv\n"
        "  --obj bpf/fuse_trace.bpf.o\n"
        "\n"
        "Example:\n"
        "  %s ./StackFS_ll 0x152f0 /usr/local/lib/x86_64-linux-gnu/libfuse3.so 0x15920 --pid 1234 --csv out.csv\n",
        prog, prog);
}

static int parse_opt_int(const char *s, int *out)
{
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (!s[0] || (end && *end != '\0'))
        return -1;
    if (v < -1 || v > (1<<30))
        return -1;
    *out = (int)v;
    return 0;
}

static int starts_with(const char *s, const char *pfx)
{
    return strncmp(s, pfx, strlen(pfx)) == 0;
}

static struct bpf_link *attach_by_section(struct bpf_program *prog)
{
    const char *sec = bpf_program__section_name(prog);
    struct bpf_link *link = NULL;

    // 1) best-effort generic attach
    link = bpf_program__attach(prog);
    if (link && !libbpf_get_error(link))
        return link;
    link = NULL;

    // 2) fallback by section prefix
    if (sec && starts_with(sec, "kprobe/")) {
        const char *func = sec + strlen("kprobe/");
        link = bpf_program__attach_kprobe(prog, false, func);
        if (link && !libbpf_get_error(link))
            return link;
        return NULL;
    }

    if (sec && starts_with(sec, "tracepoint/")) {
        // "tracepoint/<cat>/<name>"
        const char *p = sec + strlen("tracepoint/");
        const char *slash = strchr(p, '/');
        if (!slash)
            return NULL;

        char cat[64] = {0};
        char name[64] = {0};
        size_t cat_len = (size_t)(slash - p);
        if (cat_len >= sizeof(cat))
            return NULL;
        memcpy(cat, p, cat_len);
        cat[cat_len] = '\0';

        snprintf(name, sizeof(name), "%s", slash + 1);

        link = bpf_program__attach_tracepoint(prog, cat, name);
        if (link && !libbpf_get_error(link))
            return link;
        return NULL;
    }

    return NULL;
}

int main(int argc, char **argv)
{
    int exit_code = 0;
    int err = 0;

    if (argc < 5) {
        usage(argv[0]);
        return 1;
    }

    const char *daemon_bin = argv[1];
    unsigned long long recv_off = strtoull(argv[2], NULL, 0);
    const char *libfuse_so = argv[3];
    unsigned long long send_off = strtoull(argv[4], NULL, 0);

    int target_pid = -1;
    const char *csv_path = "fuse_trace.csv";
    const char *obj_path = "bpf/fuse_trace.bpf.o";

    for (int i = 5; i < argc; i++) {
        if (!strcmp(argv[i], "--pid")) {
            if (i + 1 >= argc) {
                fprintf(stderr, "--pid requires a value\n");
                return 1;
            }
            if (parse_opt_int(argv[i + 1], &target_pid) != 0) {
                fprintf(stderr, "invalid --pid value: %s\n", argv[i + 1]);
                return 1;
            }
            i++;
        } else if (!strcmp(argv[i], "--csv")) {
            if (i + 1 >= argc) {
                fprintf(stderr, "--csv requires a value\n");
                return 1;
            }
            csv_path = argv[i + 1];
            i++;
        } else if (!strcmp(argv[i], "--obj")) {
            if (i + 1 >= argc) {
                fprintf(stderr, "--obj requires a value\n");
                return 1;
            }
            obj_path = argv[i + 1];
            i++;
        } else {
            fprintf(stderr, "unknown option: %s\n", argv[i]);
            usage(argv[0]);
            return 1;
        }
    }

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    libbpf_set_print(libbpf_print_fn);
    libbpf_set_strict_mode(LIBBPF_STRICT_ALL);
    bump_memlock_rlimit();

    csv_fp = fopen(csv_path, "w");
    if (!csv_fp) {
        fprintf(stderr, "fopen(%s) failed: %s\n", csv_path, strerror(errno));
    } else {
        fprintf(csv_fp,
                "ts_queue_ns,ts_recv_ns,ts_send_ns,ts_end_ns,"
                "unique,op,err,flags,"
                "k_tid,k_cpu,d_tgid,d_tid,d_cpu,"
                "queuing_us,sched_us,daemon_us,response_us,"
                "k_comm,d_comm\n");
        fflush(csv_fp);
    }

    // ----------------------------
    // 1) open/load BPF object
    // ----------------------------
    struct bpf_object_open_opts open_opts;
    memset(&open_opts, 0, sizeof(open_opts));

    struct bpf_object *obj = bpf_object__open_file(obj_path, &open_opts);
    if (libbpf_get_error(obj)) {
        long e = libbpf_get_error(obj);
        fprintf(stderr, "bpf_object__open_file(%s) failed: %s\n",
                obj_path, strerror((int)-e));
        obj = NULL;
        exit_code = 1;
        goto cleanup;
    }

    err = bpf_object__load(obj);
    if (err) {
        fprintf(stderr, "bpf_object__load failed: %d (%s)\n", err, strerror(-err));
        exit_code = 1;
        goto cleanup;
    }

    // ----------------------------
    // 2) find ringbuf map + create ring_buffer
    // ----------------------------
    struct bpf_map *events_map = bpf_object__find_map_by_name(obj, "events");
    if (!events_map) {
        fprintf(stderr, "map 'events' not found in %s\n", obj_path);
        exit_code = 1;
        goto cleanup;
    }

    int events_fd = bpf_map__fd(events_map);
    if (events_fd < 0) {
        fprintf(stderr, "failed to get fd for map 'events'\n");
        exit_code = 1;
        goto cleanup;
    }

    struct ring_buffer *rb = ring_buffer__new(events_fd, handle_event, NULL, NULL);
    if (!rb) {
        fprintf(stderr, "ring_buffer__new failed\n");
        exit_code = 1;
        goto cleanup;
    }

    // ----------------------------
    // 3) attach programs
    // ----------------------------
    struct bpf_link *links[64];
    int nlinks = 0;
    memset(links, 0, sizeof(links));

    // ✅ libbpf 0.5.x friendly program iteration
    struct bpf_program *prog = NULL;
    while ((prog = bpf_program__next(prog, obj)) != NULL) {
        const char *sec  = bpf_program__section_name(prog);
        const char *name = bpf_program__name(prog);

        if (!sec)
            continue;

        // uprobe programs: attach manually with provided paths/offsets
        if (starts_with(sec, "uprobe/")) {
            struct bpf_link *l = NULL;

            if (name && !strcmp(name, "up_receive_buf")) {
                l = bpf_program__attach_uprobe(prog, false, target_pid, daemon_bin, recv_off);
            } else if (name && !strcmp(name, "up_fuse_send_msg")) {
                l = bpf_program__attach_uprobe(prog, false, target_pid, libfuse_so, send_off);
            } else {
                // 다른 uprobe가 있으면 여기에 추가
                continue;
            }

            if (!l || libbpf_get_error(l)) {
                long e = l ? libbpf_get_error(l) : -EINVAL;
                fprintf(stderr, "attach uprobe failed: prog=%s sec=%s: %s\n",
                        name ? name : "(null)", sec, strerror((int)-e));
                exit_code = 1;
                goto cleanup_rb;
            }

            if (nlinks < (int)(sizeof(links)/sizeof(links[0])))
                links[nlinks++] = l;
            continue;
        }

        // non-uprobe: try attach by section
        {
            struct bpf_link *l = attach_by_section(prog);
            if (!l || libbpf_get_error(l)) {
                long e = l ? libbpf_get_error(l) : -EINVAL;
                fprintf(stderr, "attach failed: prog=%s sec=%s: %s\n",
                        name ? name : "(null)", sec, strerror((int)-e));
                exit_code = 1;
                goto cleanup_rb;
            }

            if (nlinks < (int)(sizeof(links)/sizeof(links[0])))
                links[nlinks++] = l;
        }
    }

    printf("Tracing (direct loader)...\n");
    printf("  obj        : %s\n", obj_path);
    printf("  daemon_bin : %s (receive_buf off=0x%llx)\n", daemon_bin, recv_off);
    printf("  libfuse_so : %s (fuse_send_msg off=0x%llx)\n", libfuse_so, send_off);
    printf("  pid filter : %d\n", target_pid);
    printf("  csv        : %s\n", csv_path);
    printf("Press Ctrl+C to stop.\n");

    while (!exiting) {
        int r = ring_buffer__poll(rb, 200);
        if (r == -EINTR)
            break;
        if (r < 0) {
            fprintf(stderr, "ring_buffer__poll failed: %d\n", r);
            exit_code = 1;
            break;
        }
    }

cleanup_rb:
    for (int i = 0; i < nlinks; i++) {
        if (links[i])
            bpf_link__destroy(links[i]);
    }
    ring_buffer__free(rb);

cleanup:
    if (obj)
        bpf_object__close(obj);
    if (csv_fp)
        fclose(csv_fp);
    return exit_code;
}

