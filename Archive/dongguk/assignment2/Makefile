# ----- Tools -----
CLANG      ?= clang
LLC        ?= llc
LLVM_STRIP ?= llvm-strip
BPFTOOL    ?= bpftool
PKGCONFIG  ?= pkg-config

# ----- Paths -----
INCDIR     := include
BPF_DIR    := bpf
USER_DIR   := user

# ----- Files -----
VMLINUX_H  := $(INCDIR)/vmlinux.h
COMMON_H   := $(INCDIR)/common.h

BPF_SRC    := $(BPF_DIR)/trace_ctx_irq.bpf.c
BPF_OBJ    := $(BPF_DIR)/trace_ctx_irq.bpf.o

SKEL_HDR   := $(USER_DIR)/trace_ctx_irq.skel.h
USER_SRC   := $(USER_DIR)/ctrl.c
USER_BIN   := $(USER_DIR)/ctrl

# ----- __TARGET_ARCH_ -----
UNAME_M := $(shell uname -m)
# Map to BPF arch macro suffix
ifeq ($(UNAME_M),x86_64)
  BPF_ARCH := x86
else ifeq ($(UNAME_M),aarch64)
  BPF_ARCH := arm64
else ifeq ($(UNAME_M),arm64)
  BPF_ARCH := arm64
else ifeq ($(UNAME_M),ppc64le)
  BPF_ARCH := powerpc
else ifeq ($(UNAME_M),s390x)
  BPF_ARCH := s390
else
  # Fallback; adjust if needed
  BPF_ARCH := x86
endif

# ----- Flags -----
CFLAGS_BPF := -O2 -g -Wall -Wno-unused-parameter -Wno-unused-variable \
              -target bpf -D__TARGET_ARCH_$(BPF_ARCH) \
              -I$(INCDIR)
# libbpf cflags/libs via pkg-config (fallback 내장)
LIBBPF_CFLAGS := $(shell $(PKGCONFIG) --cflags libbpf 2>/dev/null)
LIBBPF_LIBS   := $(shell $(PKGCONFIG) --libs   libbpf 2>/dev/null)
ifeq ($(LIBBPF_LIBS),)
  # Fallback if pkg-config not available
  LIBBPF_LIBS := -lbpf -lelf -lz
endif
CFLAGS_USER := -O2 -g -Wall -Wextra -I$(INCDIR) $(LIBBPF_CFLAGS)
LDFLAGS_USER := $(LIBBPF_LIBS)

# ----- Phony -----
.PHONY: all bpf user run clean

all: $(USER_BIN)

# ----- vmlinux.h generation -----
$(VMLINUX_H):
	@mkdir -p $(INCDIR)
	@if [ -r /sys/kernel/btf/vmlinux ]; then \
		echo "  BTF => $@ (from /sys/kernel/btf/vmlinux)"; \
		$(BPFTOOL) btf dump file /sys/kernel/btf/vmlinux format c > $@; \
	else \
		echo "  BTF => $@ (from live vmlinux)"; \
		$(BPFTOOL) btf dump vmlinux format c > $@; \
	fi

# ----- BPF object -----
$(BPF_OBJ): $(BPF_SRC) $(COMMON_H) $(VMLINUX_H)
	@mkdir -p $(BPF_DIR)
	$(CLANG) $(CFLAGS_BPF) -c $< -o $@
	$(LLVM_STRIP) -g $@ 2>/dev/null || true

# ----- Skeleton header -----
$(SKEL_HDR): $(BPF_OBJ)
	@mkdir -p $(USER_DIR)
	$(BPFTOOL) gen skeleton $< > $@

# ----- User binary -----
$(USER_BIN): $(USER_SRC) $(SKEL_HDR) $(COMMON_H)
	$(CC) $(CFLAGS_USER) $< -o $@ $(LDFLAGS_USER)

bpf: $(BPF_OBJ) $(SKEL_HDR)
user: $(USER_BIN)

run: $(USER_BIN)
	@echo "Running. Press Ctrl-C to stop."
	@sudo $(USER_BIN)

clean:
	@$(RM) -r $(BPF_OBJ) $(SKEL_HDR) $(USER_BIN)
