# ===== 설정 =====
# 필요하면 clang-20, llvm-strip-20 같은 식으로 바꿔 써도 됨
CLANG      ?= clang
CC         ?= gcc
BPFTOOL    ?= bpftool

# libbpf를 pkg-config로 찾는 경우 (설치돼 있으면)
LIBBPF_CFLAGS  ?= $(shell pkg-config --cflags libbpf 2>/dev/null)
LIBBPF_LDLIBS  ?= $(shell pkg-config --libs   libbpf 2>/dev/null)

# 없으면 수동 설정도 가능 (필요시 주석 해제해서 사용)
# LIBBPF_CFLAGS  ?= -I/usr/include
# LIBBPF_LDLIBS  ?= -lbpf
LIBBPF_DIR   ?= /usr/local/lib64

# 공통 디렉터리
INCLUDE_DIR := include
BPF_DIR     := bpf
USER_DIR    := user

# 결과물
BPF_OBJ   := $(BPF_DIR)/fuse_trace.bpf.o
SKEL_HDR  := $(INCLUDE_DIR)/fuse_trace.skel.h
USER_BIN  := fuse_trace_user

# ===== 플래그 =====

# BPF용 CFLAGS
BPF_CFLAGS := -g -O2 -Wall \
              -target bpf \
              -D__TARGET_ARCH_x86 \
              -I$(INCLUDE_DIR) -I$(BPF_DIR)

# 유저 프로그램용 CFLAGS
USER_CFLAGS := -g -O2 -Wall \
               -I$(INCLUDE_DIR) -I$(USER_DIR) \
               $(LIBBPF_CFLAGS)

USER_LDLIBS := -L$(LIBBPF_DIR) -Wl,-rpath,$(LIBBPF_DIR) -lbpf

# ===== 기본 타겟 =====
.PHONY: all
all: $(USER_BIN)

# ----- BPF 오브젝트 빌드 -----
$(BPF_OBJ): $(BPF_DIR)/fuse_trace.bpf.c \
            $(INCLUDE_DIR)/fuse_trace_common.h \
            $(INCLUDE_DIR)/vmlinux.h
	$(CLANG) $(BPF_CFLAGS) -c $< -o $@

# ----- skeleton 생성 -----
$(SKEL_HDR): $(BPF_OBJ)
	$(BPFTOOL) gen skeleton $< > $@

# ----- 유저 바이너리 빌드 -----
$(USER_BIN): $(USER_DIR)/fuse_trace_user.c $(SKEL_HDR) $(INCLUDE_DIR)/fuse_trace_common.h
	$(CC) $(USER_CFLAGS) $< -o $@ $(USER_LDLIBS)

# ===== 유틸 =====
.PHONY: clean
clean:
	rm -f $(BPF_OBJ) $(SKEL_HDR) $(USER_BIN)

