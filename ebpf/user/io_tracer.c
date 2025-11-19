// user/io_tracer.c

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/select.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <stdarg.h>			 // va_list
#include <bpf/libbpf.h>
#include <bpf/bpf.h>		 // bpf_map_update_elem

#include "../include/common.h"
#include "../include/io_tracer.skel.h"

static volatile sig_atomic_t exiting = 0;

/* =================================
 * 시그널 처리
 * ================================= */

static void handle_sigint(int signo)
{
	(void)signo;  // unused parameter warning 방지
	exiting = 1;
}

/* =================================
 * bpf와 통신 (filter 업데이트)
 * ================================= */

/* 먼저 프로토타입 선언해서 암시적 선언을 막는다 */
static int update_filter(struct io_tracer_bpf *skel,
						 const struct io_filter_conf *conf);

/* 실제 구현 */
static int update_filter(struct io_tracer_bpf *skel,
						 const struct io_filter_conf *conf)
{
	int map_fd = bpf_map__fd(skel->maps.filter_conf);
	__u32 key = 0;

	if (map_fd < 0) {
		fprintf(stderr, "failed to get filter_conf map fd\n");
		return -1;
	}

	if (bpf_map_update_elem(map_fd, &key, conf, BPF_ANY) < 0) {
		perror("bpf_map_update_elem(filter_conf)");
		return -1;
	}

	return 0;
}

/* =================================
 * stdin non-blocking 설정
 * ================================= */

static void make_stdin_nonblock(void)
{
	int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
	if (flags < 0)
		return;
	fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
}

/* =================================
 * stdin에서 필터 변경 명령 처리
 * ================================= */

static int handle_stdin_cmd(struct io_tracer_bpf *skel,
							struct io_filter_conf *conf)
{
	char buf[128];
	ssize_t n = read(STDIN_FILENO, buf, sizeof(buf) - 1);
	if (n <= 0)
		return 0;

	buf[n] = '\0';

	// 프로토콜:
	//	 p <pid>\n	: 해당 pid로 필터
	//	 c <comm>\n : 해당 comm으로 필터
	//	 a\n		: 필터 끄기
	// 예: "p 3107\n", "c cat\n", "a\n"

	if (buf[0] == 'p') {
		long pid = strtol(buf + 1, NULL, 10);
		if (pid > 0) {
			memset(conf, 0, sizeof(*conf));
			conf->enabled  = 1;
			conf->use_tgid = 1;
			conf->tgid	   = (unsigned int)pid;
			update_filter(skel, conf);
			fprintf(stderr, "[filter] now pid=%u\n", conf->tgid);
		}
	} else if (buf[0] == 'c') {
		char *name = buf + 1;
		while (*name == ' ' || *name == '\t')
			name++;
		size_t len = strcspn(name, "\r\n");
		if (len >= TASK_COMM_LEN)
			len = TASK_COMM_LEN - 1;

		memset(conf, 0, sizeof(*conf));
		conf->enabled  = 1;
		conf->use_comm = 1;
		memcpy(conf->comm, name, len);
		conf->comm[len] = '\0';

		update_filter(skel, conf);
		fprintf(stderr, "[filter] now comm='%s'\n", conf->comm);
	} else if (buf[0] == 'a') {
		memset(conf, 0, sizeof(*conf));
		conf->enabled = 0; // 필터 끔
		update_filter(skel, conf);
		fprintf(stderr, "[filter] disabled (trace all)\n");
	}

	return 0;
}

/* =================================
 * libbpf 로그 콜백 (선택)
 * ================================= */

static int libbpf_print_fn(enum libbpf_print_level level,
						   const char *format, va_list args)
{
	(void)level; // 필요하면 level 기반 필터링 가능
	return vfprintf(stdout, format, args);
}

/* =================================
 * RLIMIT_MEMLOCK 설정
 * ================================= */

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

/* =================================
 * 인자 안내, 파싱
 * ================================= */

static void usage(const char *prog)
{
	fprintf(stderr,
			"Usage: %s [-p pid] [-c comm]\n"
			"  -p pid	: trace only this PID (tgid)\n"
			"  -c comm	: trace only this process name (comm)\n"
			"			  (max %d bytes)\n",
			prog, TASK_COMM_LEN - 1);
}

static int parse_args(int argc, char **argv, struct io_filter_conf *conf)
{
	int opt;
	memset(conf, 0, sizeof(*conf));

	// 기본은 필터 꺼진 상태 (enabled=0)
	conf->enabled  = 0;
	conf->use_tgid = 0;
	conf->use_comm = 0;
	conf->tgid	   = 0;
	memset(conf->comm, 0, sizeof(conf->comm));

	while ((opt = getopt(argc, argv, "p:c:h")) != -1) {
		switch (opt) {
		case 'p': {
			long pid = strtol(optarg, NULL, 10);
			if (pid <= 0) {
				fprintf(stderr, "invalid pid: %s\n", optarg);
				return -1;
			}
			conf->enabled  = 1;
			conf->use_tgid = 1;
			conf->tgid	   = (unsigned int)pid;
			break;
		}
		case 'c': {
			size_t len = strlen(optarg);
			if (len >= TASK_COMM_LEN)
				len = TASK_COMM_LEN - 1;
			conf->enabled  = 1;
			conf->use_comm = 1;
			memcpy(conf->comm, optarg, len);
			conf->comm[len] = '\0';
			break;
		}
		case 'h':
		default:
			usage(argv[0]);
			return -1;
		}
	}

	return 0;
}

/* =================================
 * 메인 루틴
 * ================================= */

int main(int argc, char **argv)
{
	struct io_tracer_bpf *skel = NULL;
	struct io_filter_conf conf;
	int err = 0;

	if (parse_args(argc, argv, &conf) < 0)
		return 1;

	libbpf_set_strict_mode(LIBBPF_STRICT_ALL);
	// 디버깅 원하면 주석 해제
	// libbpf_set_print(libbpf_print_fn);

	if (signal(SIGINT, handle_sigint) == SIG_ERR) {
		fprintf(stderr, "failed to set SIGINT handler\n");
		return 1;
	}
	if (signal(SIGTERM, handle_sigint) == SIG_ERR) {
		fprintf(stderr, "failed to set SIGTERM handler\n");
		return 1;
	}

	if (bump_memlock_rlimit())
		return 1;

	skel = io_tracer_bpf__open_and_load();
	if (!skel) {
		fprintf(stderr, "failed to open and load BPF skeleton\n");
		return 1;
	}

	err = io_tracer_bpf__attach(skel);
	if (err) {
		fprintf(stderr, "failed to attach BPF programs: %d\n", err);
		goto cleanup;
	}

	if (conf.enabled) {
		if (update_filter(skel, &conf) < 0) {
			fprintf(stderr, "failed to set initial filter\n");
			goto cleanup;
		}
		fprintf(stderr, "[filter] enabled (");
		if (conf.use_tgid)
			fprintf(stderr, "pid=%u ", conf.tgid);
		if (conf.use_comm)
			fprintf(stderr, "comm='%s' ", conf.comm);
		fprintf(stderr, ")\n");
	} else {
		fprintf(stderr, "[filter] disabled (trace all)\n");
	}

	printf("io_tracer: BPF 프로그램 attach 완료.\n");
	printf(" - bpf_printk 출력은 'sudo cat /sys/kernel/tracing/trace_pipe' 로 확인\n");
	printf(" - stdin 에서 'p <pid>', 'c <comm>', 'a' 로 필터 변경 가능\n");
	printf("종료하려면 Ctrl-C 를 누르세요.\n");

	make_stdin_nonblock();

	while (!exiting) {
		handle_stdin_cmd(skel, &conf);
		// 추후 ring_buffer__poll(...) 등을 여기에 추가 예정
		usleep(50 * 1000); // busy loop 방지용 (필요 없으면 줄여도 됨)
	}

	printf("io_tracer: 종료 중...\n");

cleanup:
	io_tracer_bpf__destroy(skel);
	return err != 0;
}

