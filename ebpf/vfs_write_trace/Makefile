# 프로그램 이름 정의
APP := fio_tracer

# 파일 이름 정의
BPF_SRC := vfs_write_latency.bpf.c
BPF_OBJ := vfs_write_latency.bpf.o
BPF_SKEL := vfs_write_latency.skel.h
USER_SRC := loader.c
VMLINUX := vmlinux.h

# 툴체인 및 컴파일러 설정
CLANG ?= clang
GCC ?= gcc
BPFTOOL ?= bpftool

# 아키텍처 자동 감지 (x86_64 -> x86, aarch64 -> arm64)
ARCH := $(shell uname -m | sed 's/x86_64/x86/' | sed 's/aarch64/arm64/' | sed 's/ppc64le/powerpc/')

# 컴파일 플래그
CFLAGS := -g -O2 -Wall
LDFLAGS := -lbpf -lelf

.PHONY: all clean

# 기본 타겟: 앱 빌드
all: $(APP)

# 1. vmlinux.h 생성 (없을 경우에만 실행됨)
$(VMLINUX):
	@echo "  GEN     $@"
	$(BPFTOOL) btf dump file /sys/kernel/btf/vmlinux format c > $@

# 2. BPF 소스(.bpf.c)를 오브젝트(.bpf.o)로 컴파일
# -target bpf: BPF 바이트코드로 컴파일
# -D__TARGET_ARCH_$(ARCH): 아키텍처 의존적인 매크로(pt_regs 접근 등)를 위해 필요
$(BPF_OBJ): $(BPF_SRC) $(VMLINUX)
	@echo "  CLANG   $@"
	$(CLANG) -g -O2 -target bpf -D__TARGET_ARCH_$(ARCH) -I. -c $(BPF_SRC) -o $@

# 3. BPF 오브젝트로부터 스켈레톤 헤더(.skel.h) 생성
$(BPF_SKEL): $(BPF_OBJ)
	@echo "  GEN-SKEL $@"
	$(BPFTOOL) gen skeleton $(BPF_OBJ) > $(BPF_SKEL)

# 4. 유저 공간 C 파일과 스켈레톤을 링크하여 최종 실행 파일 생성
$(APP): $(USER_SRC) $(BPF_SKEL)
	@echo "  CC      $@"
	$(GCC) $(CFLAGS) $(USER_SRC) -o $(APP) $(LDFLAGS)

# 정리 (Clean)
clean:
	rm -f $(APP) $(BPF_OBJ) $(BPF_SKEL) $(VMLINUX)
