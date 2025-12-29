# Makefile for RFUSE eBPF tracer
#
# 디렉토리 구조:
#  - bpf/rfuse_trace.bpf.c
#  - include/rfuse_common.h, include/vmlinux.h
#  - user/rfuse_trace_user.c
#
# 필요 패키지 (Ubuntu 기준):
#  - clang, llvm
#  - libbpf-dev, libelf-dev, zlib1g-dev
#  - bpftool  (패키지 or 직접 빌드해서 PATH에 추가)

# =====================================================================
# 설정
# =====================================================================

BPF_CLANG ?= clang
BPF_STRIP ?= llvm-strip
CC        ?= gcc
BPFTool   ?= bpftool

BPF_CFLAGS = -g -O2 -Wall \
             -target bpf -D__TARGET_ARCH_x86 \
             -I./include -I./bpf

USER_CFLAGS = -g -O2 -Wall \
              -I./include

# libbpf가 shared library로 설치되어 있다고 가정
USER_LDFLAGS = -lbpf -lelf -lz -Wl,-rpath=/usr/lib64

# =====================================================================
# 빌드 산출물
# =====================================================================

# req tracer
BPF_OBJ_REQ  = bpf/rfuse_trace.bpf.o
SKEL_HDR_REQ = include/rfuse_trace.skel.h

# loop tracer
BPF_OBJ_LOOP  = bpf/rfuse_loop_trace.bpf.o
SKEL_HDR_LOOP = include/rfuse_loop_trace.skel.h

# user
USER_OBJ = user/rfuse_trace_user.o
USER_BIN = rfuse_trace

# =====================================================================
# 빌드 타겟
# =====================================================================

.PHONY: all clean

all: $(USER_BIN)

# ---------- BPF 오브젝트 ----------

$(BPF_OBJ_REQ): bpf/rfuse_trace.bpf.c include/rfuse_common.h include/vmlinux.h
	$(BPF_CLANG) $(BPF_CFLAGS) -c $< -o $@
	$(BPF_STRIP) -g $@

$(BPF_OBJ_LOOP): bpf/rfuse_loop_trace.bpf.c include/rfuse_common.h include/vmlinux.h
	$(BPF_CLANG) $(BPF_CFLAGS) -c $< -o $@
	$(BPF_STRIP) -g $@

# ---------- Skeleton 헤더 생성 ----------

$(SKEL_HDR_REQ): $(BPF_OBJ_REQ)
	$(BPFTool) gen skeleton $< > $@

$(SKEL_HDR_LOOP): $(BPF_OBJ_LOOP)
	$(BPFTool) gen skeleton $< > $@

# ---------- 유저 공간 바이너리 ----------

$(USER_OBJ): user/rfuse_trace_user.c include/rfuse_common.h $(SKEL_HDR_REQ) $(SKEL_HDR_LOOP)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(USER_BIN): $(USER_OBJ)
	$(CC) -o $@ $^ $(USER_LDFLAGS)

# ---------- 청소 ----------

clean:
	rm -f $(BPF_OBJ_REQ) $(BPF_OBJ_LOOP) $(USER_OBJ) $(USER_BIN) $(SKEL_HDR_REQ) $(SKEL_HDR_LOOP)
