# ===== eBPF Project Makefile =====
# Layout:
#  - bpf/trace_*.bpf.c     -> .bpf.o -> .skel.h
#  - include/common.h
#  - include/vmlinux.h     (auto-generated)
#  - user/ctrl.c           -> ctrl

BPF_DIR   := bpf
USER_DIR  := user
INC_DIR   := include

# BPF sources
BPF_SRC   := $(BPF_DIR)/trace_sched.bpf.c \
             $(BPF_DIR)/trace_irq.bpf.c \
             $(BPF_DIR)/trace_softirq.bpf.c
BPF_OBJ   := $(BPF_SRC:.bpf.c=.bpf.o)
SKEL_HDRS := $(BPF_SRC:.bpf.c=.skel.h)

# User binary
USER_BIN  := ctrl

# Toolchain
CLANG      ?= clang
CC         ?= cc
BPFTB      ?= bpftool
LIBBPF_INC ?= /usr/include       # typically contains bpf/ headers

# User-space compile/link flags
CFLAGS   := -O2 -g -Wall -I$(INC_DIR) -I$(LIBBPF_INC)
LDFLAGS  := -lbpf -lelf -lz

# BPF compile flags
# - include/common.h 를 선행 include 하여 __u{8,16,32,64} 보장
BPF_CFLAGS := -target bpf -D__TARGET_ARCH_x86 -O2 -g \
              -I$(INC_DIR) -I$(LIBBPF_INC) \
              -include $(INC_DIR)/common.h

# Generated headers
VMLINUX_H := $(INC_DIR)/vmlinux.h

# Default target
all: $(USER_BIN)

# ----- Generate include/vmlinux.h if missing -----
$(VMLINUX_H):
	@if [ ! -f "$(VMLINUX_H)" ]; then \
		echo "[*] $(VMLINUX_H) not found, generating via bpftool..."; \
		if ! command -v $(BPFTB) >/dev/null 2>&1; then \
			echo "ERROR: bpftool not found. Install: sudo apt install bpftool"; exit 1; \
		fi; \
		$(BPFTB) btf dump file /sys/kernel/btf/vmlinux format c > $(VMLINUX_H); \
		echo "[+] $(VMLINUX_H) generated."; \
	fi

# ----- Build BPF objects (.bpf.o) -----
$(BPF_DIR)/%.bpf.o: $(BPF_DIR)/%.bpf.c $(INC_DIR)/common.h $(VMLINUX_H)
	$(CLANG) $(BPF_CFLAGS) -c $< -o $@

# ----- Generate skeleton headers (.skel.h) -----
$(BPF_DIR)/%.skel.h: $(BPF_DIR)/%.bpf.o
	$(BPFTB) gen skeleton $< > $@

# ----- Build user controller -----
$(USER_BIN): $(USER_DIR)/ctrl.c $(SKEL_HDRS)
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o $@

# ----- Utilities -----
clean:
	rm -f $(BPF_DIR)/*.bpf.o \
	      $(BPF_DIR)/*.skel.h \
	      $(USER_BIN)

distclean: clean
	rm -f $(INC_DIR)/vmlinux.h

re: clean all

.PHONY: all clean distclean re

