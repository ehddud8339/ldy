#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <stdint.h>
#include <inttypes.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <bpf/libbpf.h>
#include "AOS.skel.h"

#define TASK_COMM_LEN 16

// bpf program과 공유하는 이벤트 구조체
struct evt {
    uint32_t pid;
    uint32_t tgid;
    uint64_t vaddr;
    char comm[TASK_COMM_LEN];
};

// PTE walk를 하기 위해 필요한 PFN(Page Frame Number Mask)와
// PAGE가 존재하는지 확인하는 PRESENT_BIT
#define PFN_MASK ((1ULL << 55) - 1ULL)  /* bits 0..54: PFN (common layout) */
#define PAGEMAP_PRESENT_BIT (1ULL << 63)
// Ctrl + C로 종료하기 위한 전역 변수
static volatile sig_atomic_t exiting = 0;
// bpf program과 공유할 링 버퍼 구조체 변수 선언
static struct ring_buffer *rb = NULL;
// bpf program의 skeleton 변수
static struct AOS_bpf *skel = NULL;
// Ctrl + C를 받으면 exiting을 1로 만들어 프로그램 종료
static void sig_int(int signo) {
    exiting = 1;
}

// /proc/<pid>/pagemap에 root 권한으로 접근하면
// 해당 pid의 페이지 매핑 정보를 얻을 수 있음
static int vaddr_to_paddr(pid_t pid, unsigned long vaddr, unsigned long *out_paddr) {
    char pagemap_path[64];
    int fd;
    // /proc/<pid>/pagemap은 가상 페이지가 8바이트로 저장
    // 가상 주소 / 4096 -> 페이지 인덱스 확인
    // 페이지 인덱스 * 8바이트로 실제 오프셋 확인
    unsigned long page_size = 4096;
    unsigned long page_index = vaddr / page_size;
    off_t offset = (off_t)page_index * sizeof(uint64_t);
    uint64_t entry = 0;
    // int snprintf(char *str, size_t size, const char *format, ...);
    // str을 format 처럼 정의하면서 오버플로우가 발생하지 않도록 만듦
    snprintf(pagemap_path, sizeof(pagemap_path), "/proc/%d/pagemap", pid);
    fd = open(pagemap_path, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "open(%s) fail: errno=%d (%s)\n",
                pagemap_path, errno, strerror(errno));
        return -1;
    }
    // pread로 /proc/<pid>/pagemap에서 offset부터 8바이트만큼 읽기
    ssize_t n = pread(fd, &entry, sizeof(entry), offset);
    close(fd);
    if (n != sizeof(entry)) {
        int e = errno;
        fprintf(stderr, "pread fail=%d\n", e);
        return -1;
    }
    // PRESENT_BIT 확인 -> PROT_NONE 이면 할당 X
    // 실패 처리
    if (!(entry & PAGEMAP_PRESENT_BIT)) return -2;
    // PFN_MASK로 PFN 추출하고 0이면 비정상
    uint64_t pfn = entry & PFN_MASK;
    if (pfn == 0) {
        fprintf(stderr, "pfn mask fail\n");
        return -1;
    }
    // (PFN * 4096)은 물리 페이지의 시작 주소로
    // (vaddr / 4096) = offset 을 더 해주면 더 자세한 정보를 알 수 있음
    unsigned long paddr = (unsigned long)((pfn * (uint64_t)page_size) + (vaddr % page_size));
    *out_paddr = paddr;
    return 0;
}

// 링 버퍼에서 이벤트를 polling 했을 때, 수행할 이벤트 핸들러
static int handle_event(void *ctx, void *data, size_t data_sz) {
    // 필수는 아니지만, 경고를 없애기 위해 사용
    (void)ctx;
    if (data == NULL || data_sz < sizeof(struct evt)) return 0;

    const struct evt *e = (const struct evt *)data;
    
    printf("event: pid=%u tgid=%u comm=%s vaddr=0x%lx\n",
           e->pid, e->tgid, e->comm, (unsigned long)e->vaddr);

    pid_t target_pid = (pid_t)e->tgid;

    unsigned long paddr = 0;
    int rc = vaddr_to_paddr(target_pid, (unsigned long)e->vaddr, &paddr);
    if (rc == 0) {
        printf(" -> paddr = 0x%lx\n", paddr);
    } else if (rc == -2) {
        printf(" -> page not present\n");
    } else {
        printf(" -> pagemap read failed\n");
    }
    return 0;
}

int main(int argc, char *argv[]) {
    struct AOS_bpf *skel = NULL;
    int err;

    libbpf_set_strict_mode(LIBBPF_STRICT_ALL);

    skel = AOS_bpf__open();
    if (!skel) {
        fprintf(stderr, "failed to open skel\n");
        return -1;
    }

    err = AOS_bpf__load(skel);
    if (err) {
        fprintf(stderr, "failed to load skel: %d\n", err);
        goto cleanup;
    }
    err = AOS_bpf__attach(skel);
    if (err) {
        fprintf(stderr, "failed to attach: %d\n", err);
        goto cleanup;
    }

    int map_fd = bpf_map__fd(skel->maps.rb);
    if (map_fd < 0) {
        fprintf(stderr, "failed to get map fd\n");
        goto cleanup;
    }

    rb =ring_buffer__new(map_fd, handle_event, NULL, NULL);
    if (!rb) {
        fprintf(stderr, "failed to create ring buffer\n");
        goto cleanup;
    }

    signal(SIGINT, sig_int);
    signal(SIGTERM, sig_int);

    printf("AOS_bpf Attached.\n");

    while (!exiting) {
        err = ring_buffer__poll(rb, 200);
        if (err < 0 && errno != EINTR) {
            fprintf(stderr, "ring buffer poll err:%d\n", err);
            break;
        }
    }

cleanup:
    if (rb) ring_buffer__free(rb);
    if (skel) AOS_bpf__destroy(skel);
    return 0;
}
