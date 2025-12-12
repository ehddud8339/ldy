BPF_ARCH	:= __TARGET_ARCH_x86
BPF_PROG	:= KAC.bpf.c
BPF_OBJ		:= KAC.bpf.o
SKEL_HDR	:= KAC.skel.h
USER_PROG	:= control_program.c
USER_BIN	:= control_program

LBIBBPF_PC_FLAGS := $(shell pkg-config --cflags --libs libbpf)

all: $(USER_BIN)

vmlinux:
	cp /sys/kernel/btf/vmlinux vmlinux

$(BPF_OBJ): $(BPF_PROG)
	clang -O2 -g -target bpf -D$(BPF_ARCH) -I. -c $< -o $@

$(SKEL_HDR): $(BPF_OBJ)
	bpftool gen skeleton $< > $@

$(USER_BIN): $(USER_PROG) $(SKEL_HDR)
	gcc -O2 -g -o $@ $(USER_PROG) $(LBIBBPF_PC_FLAGS)

clean:
	rm -f $(BPF_OBJ) $(SKEL_HDR) $(USER_BIN) vmlinux
