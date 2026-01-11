# ===== 설정 =====
CLANG      ?= clang
CC         ?= clang-20
BPFTOOL    ?= bpftool

INCLUDE_DIR := include
BPF_DIR     := bpf
USER_DIR    := user

BPF_OBJ   := $(BPF_DIR)/fuse_trace.bpf.o
USER_BIN  := fuse_trace_user

# libbpf를 /usr/local(0.5.0)로 통일 (헤더/라이브러리 mismatch 방지)
LIBBPF_INC ?= /usr/local/include
LIBBPF_DIR ?= /usr/local/lib64

# ===== 플래그 =====
BPF_CFLAGS := -g -O2 -Wall \
              -target bpf \
              -D__TARGET_ARCH_x86 \
              -I$(INCLUDE_DIR) -I$(BPF_DIR)

# 유저 프로그램은 /usr/local/include를 가장 앞에 둬야 함(버전 맞추기)
USER_CFLAGS := -g -O2 -Wall -gdwarf-4 \
               -I$(LIBBPF_INC) \
               -I$(INCLUDE_DIR) -I$(USER_DIR)

# 링킹은 /usr/local/lib64의 libbpf + 보통 필요한 의존성
USER_LDLIBS := -L$(LIBBPF_DIR) -Wl,-rpath,$(LIBBPF_DIR) \
               -lbpf -lelf -lz

# ===== 기본 타겟 =====
.PHONY: all
all: $(BPF_OBJ) $(USER_BIN)

# ----- BPF 오브젝트 빌드 -----
$(BPF_OBJ): $(BPF_DIR)/fuse_trace.bpf.c \
            $(INCLUDE_DIR)/fuse_trace_common.h \
            $(INCLUDE_DIR)/vmlinux.h
	$(CLANG) $(BPF_CFLAGS) -c $< -o $@

# ----- 유저 바이너리 빌드 (direct loader) -----
# skeleton 헤더 의존성 제거!
$(USER_BIN): $(USER_DIR)/fuse_trace_user.c $(INCLUDE_DIR)/fuse_trace_common.h
	$(CC) $(USER_CFLAGS) $< -o $@ $(USER_LDLIBS)

# ===== 유틸 =====
.PHONY: clean
clean:
	rm -f $(BPF_OBJ) $(USER_BIN)

