# ===== 설정 =====

# BPF용 clang / llvm
CLANG      ?= clang
LLC        ?= llc

# BPF용 컴파일 옵션 (arch는 환경에 맞게 조정)
BPF_CFLAGS = -g -O2 -Wall -target bpf \
             -D__TARGET_ARCH_x86 \
             -I./include \
             -I./bpf
#             -I/usr/include

# libbpf, libelf, zlib 등은 pkg-config 사용
PKG_CONFIG      ?= pkg-config
LIBBPF_CFLAGS   := $(shell $(PKG_CONFIG) --cflags libbpf 2>/dev/null)
LIBBPF_LDLIBS   := $(shell $(PKG_CONFIG) --libs libbpf 2>/dev/null)

# bpftool (PATH에 있다면 그냥 bpftool)
BPFT_TOOL ?= bpftool

# ===== 파일 경로 =====

BPF_SRC      := bpf/fuse_req_lat.bpf.c
BPF_OBJ      := bpf/fuse_req_lat.bpf.o
BPF_SKEL     := include/fuse_req_lat.bpf.skel.h
# vmlinux.h 경로
VMLINUX_H    := include/vmlinux.h

USER_SRC     := user/fuse_req_lat_user.c
USER_BIN     := user/fuse_req_lat_user

INCLUDES     := include/fuse_req_lat_user.h include/fuse_opname.h $(BPF_SKEL) $(VMLINUX_H)

# ===== 최상위 타겟 =====

.PHONY: all
all: $(USER_BIN)

# ===== vmlinux.h 생성 =====
# 없으면 /sys/kernel/btf/vmlinux 에서 BTF를 덤프해서 include/vmlinux.h 생성

$(VMLINUX_H):
	@mkdir -p $(dir $@)
	$(BPFT_TOOL) btf dump file /sys/kernel/btf/vmlinux format c > $@

# ===== BPF 오브젝트 빌드 =====
# bpf/fuse_req_lat.bpf.o 생성

$(BPF_OBJ): $(BPF_SRC) $(VMLINUX_H)
	$(CLANG) $(BPF_CFLAGS) -c $< -o $@

# ===== skeleton 헤더 생성 =====
# include/fuse_req_lat.bpf.skel.h 생성
#  - bpftool gen skeleton bpf/fuse_req_lat.bpf.o > include/...

$(BPF_SKEL): $(BPF_OBJ)
	@mkdir -p $(dir $@)
	$(BPFT_TOOL) gen skeleton $< > $@

# ===== 유저 프로그램 빌드 =====
# user/fuse_req_lat_user.c + include/*.h + skel.h

$(USER_BIN): $(USER_SRC) $(INCLUDES)
	$(CLANG) -g -O2 -Wall \
		$(LIBBPF_CFLAGS) \
		-I./include \
		-o $@ $< \
		$(LIBBPF_LDLIBS) -lelf -lz

# ===== 청소 =====

.PHONY: clean
clean:
	rm -f $(BPF_OBJ) $(BPF_SKEL) $(USER_BIN)
	# rm -f $(VMLINUX_H)

