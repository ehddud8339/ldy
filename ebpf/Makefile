# Makefile for io_tracer
#
# 기본 사용법:
#	make		  # io_tracer 바이너리 빌드
#	make clean	  # 빌드 산출물 삭제

# ==========
# 설정 영역
# ==========

# BPF용 clang (필요하면 환경변수로 덮어쓸 수 있음: make BPF_CLANG=/opt/llvm-18/bin/clang)
BPF_CLANG ?= clang

# 일반 C 컴파일러
CC		  ?= gcc

# 공용 디렉터리
INCDIR	  := include
BPFDIR	  := bpf
USERDIR   := user

# 산출물
BPF_OBJ   := $(BPFDIR)/io_tracer.bpf.o
SKEL_HDR  := $(INCDIR)/io_tracer.skel.h
VMLINUX_H := $(INCDIR)/vmlinux.h
USER_BIN  := $(USERDIR)/io_tracer

# libbpf 관련 플래그 (pkg-config 사용)
LIBBPF_CFLAGS  := $(shell pkg-config --cflags libbpf 2>/dev/null)
LIBBPF_LDLIBS  := $(shell pkg-config --libs libbpf 2>/dev/null)

# BPF CFLAGS
BPF_CFLAGS := -target bpf \
			  -D__TARGET_ARCH_x86 \
			  -O2 -g \
			  -Wall -Wextra \
			  -I$(INCDIR)

# 유저 프로그램 CFLAGS
USER_CFLAGS := -O2 -g -Wall -Wextra \
			   -I$(INCDIR) \
			   $(LIBBPF_CFLAGS)

USER_LDLIBS := $(LIBBPF_LDLIBS)

# 기본 타겟
.PHONY: all
all: $(USER_BIN)

# ==========
# vmlinux.h 생성
# ==========

# vmlinux.h가 없으면 /sys/kernel/btf/vmlinux에서 자동 생성
$(VMLINUX_H):
	@echo "[+] Generating vmlinux.h from /sys/kernel/btf/vmlinux"
	@if [ ! -r /sys/kernel/btf/vmlinux ]; then \
		echo "ERROR: /sys/kernel/btf/vmlinux not found (BTF 미지원 커널이거나 권한 문제일 수 있음)"; \
		exit 1; \
	fi
	bpftool btf dump file /sys/kernel/btf/vmlinux format c > $@

# ==========
# BPF 오브젝트 빌드
# ==========

$(BPF_OBJ): $(BPFDIR)/io_tracer.bpf.c $(INCDIR)/io_tracer.h $(VMLINUX_H)
	@echo "[+] Compiling BPF object $@"
	$(BPF_CLANG) $(BPF_CFLAGS) -c $< -o $@

# ==========
# Skeleton 헤더 생성
# ==========

$(SKEL_HDR): $(BPF_OBJ)
	@echo "[+] Generating skeleton header $@"
	bpftool gen skeleton $< > $@

# ==========
# 유저 프로그램 빌드
# ==========

$(USER_BIN): $(USERDIR)/io_tracer.c $(INCDIR)/io_tracer.h $(SKEL_HDR)
	@echo "[+] Building user binary $@"
	$(CC) $(USER_CFLAGS) -o $@ $(USERDIR)/io_tracer.c $(USER_LDLIBS)

# ==========
# 기타 유틸 타겟
# ==========

.PHONY: clean
clean:
	@echo "[+] Cleaning build artifacts"
	rm -f $(BPF_OBJ) $(SKEL_HDR) $(USER_BIN)

.PHONY: distclean
distclean: clean
	@echo "[+] Removing generated vmlinux.h"
	rm -f $(VMLINUX_H)

