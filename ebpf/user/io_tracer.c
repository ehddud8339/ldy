// user/io_tracer.c

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/resource.h>

#include <bpf/libbpf.h>

// bpftool 또는 빌드 스텝에서 다음과 같이 생성했다고 가정:
//   bpftool gen skeleton bpf/io_tracer.bpf.o > include/io_tracer.skel.h
#include "../include/io_tracer.skel.h"

static volatile sig_atomic_t exiting = 0;

//
// ========== 시그널 처리 ==========
//

static void handle_sigint(int signo)
{
  exiting = 1;
}

//
// ========== libbpf 로그 콜백 (선택) ==========
//

static int libbpf_print_fn(enum libbpf_print_level level,
     const char *format, va_list args)
{
  // 디버깅이 필요하면 level에 따라 필터링해서 출력 가능
  // 여기서는 단순 stdout에 출력
  return vfprintf(stdout, format, args);
}

//
// ========== RLIMIT_MEMLOCK 설정 ==========
//
// - eBPF 프로그램/맵 로드를 위해 memlock 제한을 크게 늘린다.
//

static int bump_memlock_rlimit(void)
{
  struct rlimit rlim = {
  .rlim_cur = RLIM_INFINITY,
  .rlim_max = RLIM_INFINITY,
  };

  if (setrlimit(RLIMIT_MEMLOCK, &rlim)) {
  fprintf(stderr, "failed to setrlimit(RLIMIT_MEMLOCK): %s\n",
  strerror(errno));
  return -1;
  }
  return 0;
}

//
// ========== 메인 루틴 ==========
//

int main(int argc, char **argv)
{
  struct io_tracer_bpf *skel = NULL;
  int err;

  // libbpf 로그를 보고 싶으면 활성화
  libbpf_set_strict_mode(LIBBPF_STRICT_ALL);
  libbpf_set_print(libbpf_print_fn);

  // SIGINT(Ctrl-C), SIGTERM 핸들러 등록
  if (signal(SIGINT, handle_sigint) == SIG_ERR) {
  fprintf(stderr, "failed to set SIGINT handler\n");
  return 1;
  }
  if (signal(SIGTERM, handle_sigint) == SIG_ERR) {
  fprintf(stderr, "failed to set SIGTERM handler\n");
  return 1;
  }

  // memlock 제한 올리기
  if (bump_memlock_rlimit()) {
  return 1;
  }

  //
  // ========== BPF 스켈레톤 open+load ========== //
  //

  skel = io_tracer_bpf__open();
  if (!skel) {
  fprintf(stderr, "failed to open BPF skeleton\n");
  return 1;
  }

  // 여기서 필요하면 설정 맵(io_cfg 등)에 값을 써 넣을 수 있음
  // 현재 io_tracer.bpf.c에서 io_cfg를 사용하지 않고 있으므로 생략

  err = io_tracer_bpf__load(skel);
  if (err) {
  fprintf(stderr, "failed to load BPF skeleton: %d\n", err);
  goto cleanup;
  }

  //
  // ========== BPF 프로그램 attach ========== //
  //

  err = io_tracer_bpf__attach(skel);
  if (err) {
  fprintf(stderr, "failed to attach BPF skeleton: %d\n", err);
  goto cleanup;
  }

  printf("io_tracer: BPF 프로그램 attach 완료.\n");
  printf(" - sys_enter/exit_write, vfs/ext4, block, nvme 훅이 모두 활성화됨\n");
  printf(" - bpf_printk 출력은 'sudo cat /sys/kernel/tracing/trace_pipe' 로 확인 가능\n");
  printf("종료하려면 Ctrl-C 를 누르세요.\n");

  //
  // ========== 이벤트 대기 루프 ========== //
  //
  // - 현재는 bpf_printk 로만 확인하므로 유저 프로그램은 단순히 살아있기만 하면 됨
  // - 추후 ring buffer/poll 기반 이벤트 수집 로직을 여기에 추가하면 된다.
  //

  while (!exiting) {
  sleep(1);
  }

  printf("io_tracer: 종료 중...\n");

cleanup:
  io_tracer_bpf__destroy(skel);
  return err != 0;
}

