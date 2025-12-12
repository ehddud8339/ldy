cmd_/home/ldy/src/ldy/fuse_copy_overheads/remap_bench.o := gcc -Wp,-MMD,/home/ldy/src/ldy/fuse_copy_overheads/.remap_bench.o.d  -nostdinc -isystem /usr/lib/gcc/x86_64-linux-gnu/9/include -I/home/ldy/src/linux/arch/x86/include -I./arch/x86/include/generated -I/home/ldy/src/linux/include -I./include -I/home/ldy/src/linux/arch/x86/include/uapi -I./arch/x86/include/generated/uapi -I/home/ldy/src/linux/include/uapi -I./include/generated/uapi -include /home/ldy/src/linux/include/linux/compiler-version.h -include /home/ldy/src/linux/include/linux/kconfig.h -include /home/ldy/src/linux/include/linux/compiler_types.h -D__KERNEL__ -fmacro-prefix-map=/home/ldy/src/linux/= -Wall -Wundef -Werror=strict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -fshort-wchar -fno-PIE -Werror=implicit-function-declaration -Werror=implicit-int -Werror=return-type -Wno-format-security -std=gnu89 -mno-sse -mno-mmx -mno-sse2 -mno-3dnow -mno-avx -fcf-protection=none -m64 -falign-jumps=1 -falign-loops=1 -mno-80387 -mno-fp-ret-in-387 -mpreferred-stack-boundary=3 -mskip-rax-setup -mtune=generic -mno-red-zone -mcmodel=kernel -DCONFIG_X86_X32_ABI -Wno-sign-compare -fno-asynchronous-unwind-tables -mindirect-branch=thunk-extern -mindirect-branch-register -fno-jump-tables -fno-delete-null-pointer-checks -Wno-frame-address -Wno-format-truncation -Wno-format-overflow -Wno-address-of-packed-member -O2 --param=allow-store-data-races=0 -Wframe-larger-than=1024 -fstack-protector-strong -Wimplicit-fallthrough=5 -Wno-main -Wno-unused-but-set-variable -Wno-unused-const-variable -fno-omit-frame-pointer -fno-optimize-sibling-calls -fno-stack-clash-protection -g -gdwarf-4 -pg -mrecord-mcount -mfentry -DCC_USING_FENTRY -Wdeclaration-after-statement -Wvla -Wno-pointer-sign -Wno-stringop-truncation -Wno-array-bounds -Wno-stringop-overflow -Wno-restrict -Wno-maybe-uninitialized -fno-strict-overflow -fno-stack-check -fconserve-stack -Werror=date-time -Werror=incompatible-pointer-types -Werror=designated-init -Wno-packed-not-aligned  -DMODULE  -DKBUILD_BASENAME='"remap_bench"' -DKBUILD_MODNAME='"remap_bench"' -D__KBUILD_MODNAME=kmod_remap_bench -c -o /home/ldy/src/ldy/fuse_copy_overheads/remap_bench.o /home/ldy/src/ldy/fuse_copy_overheads/remap_bench.c

source_/home/ldy/src/ldy/fuse_copy_overheads/remap_bench.o := /home/ldy/src/ldy/fuse_copy_overheads/remap_bench.c

deps_/home/ldy/src/ldy/fuse_copy_overheads/remap_bench.o := \
    $(wildcard include/config/COMPAT) \
  /home/ldy/src/linux/include/linux/compiler-version.h \
    $(wildcard include/config/CC_VERSION_TEXT) \
  /home/ldy/src/linux/include/linux/kconfig.h \
    $(wildcard include/config/CPU_BIG_ENDIAN) \
    $(wildcard include/config/BOOGER) \
    $(wildcard include/config/FOO) \
  /home/ldy/src/linux/include/linux/compiler_types.h \
    $(wildcard include/config/HAVE_ARCH_COMPILER_H) \
    $(wildcard include/config/CC_HAS_ASM_INLINE) \
  /home/ldy/src/linux/include/linux/compiler_attributes.h \
  /home/ldy/src/linux/include/linux/compiler-gcc.h \
    $(wildcard include/config/RETPOLINE) \
    $(wildcard include/config/ARCH_USE_BUILTIN_BSWAP) \
    $(wildcard include/config/KCOV) \
  /home/ldy/src/linux/include/linux/module.h \
    $(wildcard include/config/MODULES) \
    $(wildcard include/config/SYSFS) \
    $(wildcard include/config/MODULES_TREE_LOOKUP) \
    $(wildcard include/config/LIVEPATCH) \
    $(wildcard include/config/STACKTRACE_BUILD_ID) \
    $(wildcard include/config/CFI_CLANG) \
    $(wildcard include/config/MODULE_SIG) \
    $(wildcard include/config/GENERIC_BUG) \
    $(wildcard include/config/KALLSYMS) \
    $(wildcard include/config/SMP) \
    $(wildcard include/config/TRACEPOINTS) \
    $(wildcard include/config/TREE_SRCU) \
    $(wildcard include/config/BPF_EVENTS) \
    $(wildcard include/config/DEBUG_INFO_BTF_MODULES) \
    $(wildcard include/config/JUMP_LABEL) \
    $(wildcard include/config/TRACING) \
    $(wildcard include/config/EVENT_TRACING) \
    $(wildcard include/config/FTRACE_MCOUNT_RECORD) \
    $(wildcard include/config/KPROBES) \
    $(wildcard include/config/HAVE_STATIC_CALL_INLINE) \
    $(wildcard include/config/PRINTK_INDEX) \
    $(wildcard include/config/MODULE_UNLOAD) \
    $(wildcard include/config/CONSTRUCTORS) \
    $(wildcard include/config/FUNCTION_ERROR_INJECTION) \
  /home/ldy/src/linux/include/linux/list.h \
    $(wildcard include/config/DEBUG_LIST) \
  /home/ldy/src/linux/include/linux/types.h \
    $(wildcard include/config/HAVE_UID16) \
    $(wildcard include/config/UID16) \
    $(wildcard include/config/ARCH_DMA_ADDR_T_64BIT) \
    $(wildcard include/config/PHYS_ADDR_T_64BIT) \
    $(wildcard include/config/64BIT) \
    $(wildcard include/config/ARCH_32BIT_USTAT_F_TINODE) \
  /home/ldy/src/linux/include/uapi/linux/types.h \
  arch/x86/include/generated/uapi/asm/types.h \
  /home/ldy/src/linux/include/uapi/asm-generic/types.h \
  /home/ldy/src/linux/include/asm-generic/int-ll64.h \
  /home/ldy/src/linux/include/uapi/asm-generic/int-ll64.h \
  /home/ldy/src/linux/arch/x86/include/uapi/asm/bitsperlong.h \
  /home/ldy/src/linux/include/asm-generic/bitsperlong.h \
  /home/ldy/src/linux/include/uapi/asm-generic/bitsperlong.h \
  /home/ldy/src/linux/include/uapi/linux/posix_types.h \
  /home/ldy/src/linux/include/linux/stddef.h \
  /home/ldy/src/linux/include/uapi/linux/stddef.h \
  /home/ldy/src/linux/include/linux/compiler_types.h \
  /home/ldy/src/linux/arch/x86/include/asm/posix_types.h \
    $(wildcard include/config/X86_32) \
  /home/ldy/src/linux/arch/x86/include/uapi/asm/posix_types_64.h \
  /home/ldy/src/linux/include/uapi/asm-generic/posix_types.h \
  /home/ldy/src/linux/include/linux/poison.h \
    $(wildcard include/config/ILLEGAL_POINTER_VALUE) \
  /home/ldy/src/linux/include/linux/const.h \
  /home/ldy/src/linux/include/vdso/const.h \
  /home/ldy/src/linux/include/uapi/linux/const.h \
  /home/ldy/src/linux/include/linux/kernel.h \
    $(wildcard include/config/PREEMPT_VOLUNTARY) \
    $(wildcard include/config/PREEMPT_DYNAMIC) \
    $(wildcard include/config/PREEMPT_) \
    $(wildcard include/config/DEBUG_ATOMIC_SLEEP) \
    $(wildcard include/config/MMU) \
    $(wildcard include/config/PROVE_LOCKING) \
  /home/ldy/src/linux/include/linux/stdarg.h \
  /home/ldy/src/linux/include/linux/align.h \
  /home/ldy/src/linux/include/linux/limits.h \
  /home/ldy/src/linux/include/uapi/linux/limits.h \
  /home/ldy/src/linux/include/vdso/limits.h \
  /home/ldy/src/linux/include/linux/linkage.h \
    $(wildcard include/config/ARCH_USE_SYM_ANNOTATIONS) \
  /home/ldy/src/linux/include/linux/stringify.h \
  /home/ldy/src/linux/include/linux/export.h \
    $(wildcard include/config/MODVERSIONS) \
    $(wildcard include/config/MODULE_REL_CRCS) \
    $(wildcard include/config/HAVE_ARCH_PREL32_RELOCATIONS) \
    $(wildcard include/config/TRIM_UNUSED_KSYMS) \
  /home/ldy/src/linux/include/linux/compiler.h \
    $(wildcard include/config/TRACE_BRANCH_PROFILING) \
    $(wildcard include/config/PROFILE_ALL_BRANCHES) \
    $(wildcard include/config/STACK_VALIDATION) \
  arch/x86/include/generated/asm/rwonce.h \
  /home/ldy/src/linux/include/asm-generic/rwonce.h \
  /home/ldy/src/linux/include/linux/kasan-checks.h \
    $(wildcard include/config/KASAN_GENERIC) \
    $(wildcard include/config/KASAN_SW_TAGS) \
  /home/ldy/src/linux/include/linux/kcsan-checks.h \
    $(wildcard include/config/KCSAN) \
    $(wildcard include/config/KCSAN_IGNORE_ATOMICS) \
  /home/ldy/src/linux/arch/x86/include/asm/linkage.h \
    $(wildcard include/config/X86_64) \
    $(wildcard include/config/X86_ALIGNMENT_16) \
  /home/ldy/src/linux/include/linux/bitops.h \
  /home/ldy/src/linux/include/linux/bits.h \
  /home/ldy/src/linux/include/vdso/bits.h \
  /home/ldy/src/linux/include/linux/build_bug.h \
  /home/ldy/src/linux/include/linux/typecheck.h \
  /home/ldy/src/linux/include/uapi/linux/kernel.h \
  /home/ldy/src/linux/include/uapi/linux/sysinfo.h \
  /home/ldy/src/linux/arch/x86/include/asm/bitops.h \
    $(wildcard include/config/X86_CMOV) \
  /home/ldy/src/linux/arch/x86/include/asm/alternative.h \
  /home/ldy/src/linux/arch/x86/include/asm/asm.h \
  /home/ldy/src/linux/arch/x86/include/asm/rmwcc.h \
    $(wildcard include/config/CC_HAS_ASM_GOTO) \
  /home/ldy/src/linux/arch/x86/include/asm/barrier.h \
  /home/ldy/src/linux/arch/x86/include/asm/nops.h \
  /home/ldy/src/linux/include/asm-generic/barrier.h \
  /home/ldy/src/linux/include/asm-generic/bitops/find.h \
    $(wildcard include/config/GENERIC_FIND_FIRST_BIT) \
  /home/ldy/src/linux/include/asm-generic/bitops/sched.h \
  /home/ldy/src/linux/arch/x86/include/asm/arch_hweight.h \
  /home/ldy/src/linux/arch/x86/include/asm/cpufeatures.h \
  /home/ldy/src/linux/arch/x86/include/asm/required-features.h \
    $(wildcard include/config/X86_MINIMUM_CPU_FAMILY) \
    $(wildcard include/config/MATH_EMULATION) \
    $(wildcard include/config/X86_PAE) \
    $(wildcard include/config/X86_CMPXCHG64) \
    $(wildcard include/config/X86_USE_3DNOW) \
    $(wildcard include/config/X86_P6_NOP) \
    $(wildcard include/config/MATOM) \
    $(wildcard include/config/PARAVIRT_XXL) \
  /home/ldy/src/linux/arch/x86/include/asm/disabled-features.h \
    $(wildcard include/config/X86_SMAP) \
    $(wildcard include/config/X86_UMIP) \
    $(wildcard include/config/X86_INTEL_MEMORY_PROTECTION_KEYS) \
    $(wildcard include/config/X86_5LEVEL) \
    $(wildcard include/config/PAGE_TABLE_ISOLATION) \
    $(wildcard include/config/X86_SGX) \
  /home/ldy/src/linux/include/asm-generic/bitops/const_hweight.h \
  /home/ldy/src/linux/include/asm-generic/bitops/instrumented-atomic.h \
  /home/ldy/src/linux/include/linux/instrumented.h \
  /home/ldy/src/linux/include/asm-generic/bitops/instrumented-non-atomic.h \
    $(wildcard include/config/KCSAN_ASSUME_PLAIN_WRITES_ATOMIC) \
  /home/ldy/src/linux/include/asm-generic/bitops/instrumented-lock.h \
  /home/ldy/src/linux/include/asm-generic/bitops/le.h \
  /home/ldy/src/linux/arch/x86/include/uapi/asm/byteorder.h \
  /home/ldy/src/linux/include/linux/byteorder/little_endian.h \
  /home/ldy/src/linux/include/uapi/linux/byteorder/little_endian.h \
  /home/ldy/src/linux/include/linux/swab.h \
  /home/ldy/src/linux/include/uapi/linux/swab.h \
  /home/ldy/src/linux/arch/x86/include/uapi/asm/swab.h \
  /home/ldy/src/linux/include/linux/byteorder/generic.h \
  /home/ldy/src/linux/include/asm-generic/bitops/ext2-atomic-setbit.h \
  /home/ldy/src/linux/include/linux/kstrtox.h \
  /home/ldy/src/linux/include/linux/log2.h \
    $(wildcard include/config/ARCH_HAS_ILOG2_U32) \
    $(wildcard include/config/ARCH_HAS_ILOG2_U64) \
  /home/ldy/src/linux/include/linux/math.h \
  /home/ldy/src/linux/arch/x86/include/asm/div64.h \
  /home/ldy/src/linux/include/asm-generic/div64.h \
  /home/ldy/src/linux/include/linux/minmax.h \
  /home/ldy/src/linux/include/linux/panic.h \
    $(wildcard include/config/PANIC_TIMEOUT) \
  /home/ldy/src/linux/include/linux/printk.h \
    $(wildcard include/config/MESSAGE_LOGLEVEL_DEFAULT) \
    $(wildcard include/config/CONSOLE_LOGLEVEL_DEFAULT) \
    $(wildcard include/config/CONSOLE_LOGLEVEL_QUIET) \
    $(wildcard include/config/EARLY_PRINTK) \
    $(wildcard include/config/PRINTK) \
    $(wildcard include/config/DYNAMIC_DEBUG) \
    $(wildcard include/config/DYNAMIC_DEBUG_CORE) \
  /home/ldy/src/linux/include/linux/init.h \
    $(wildcard include/config/STRICT_KERNEL_RWX) \
    $(wildcard include/config/STRICT_MODULE_RWX) \
    $(wildcard include/config/LTO_CLANG) \
  /home/ldy/src/linux/include/linux/kern_levels.h \
  /home/ldy/src/linux/include/linux/cache.h \
    $(wildcard include/config/ARCH_HAS_CACHE_LINE_SIZE) \
  /home/ldy/src/linux/arch/x86/include/asm/cache.h \
    $(wildcard include/config/X86_L1_CACHE_SHIFT) \
    $(wildcard include/config/X86_INTERNODE_CACHE_SHIFT) \
    $(wildcard include/config/X86_VSMP) \
  /home/ldy/src/linux/include/linux/ratelimit_types.h \
  /home/ldy/src/linux/include/uapi/linux/param.h \
  arch/x86/include/generated/uapi/asm/param.h \
  /home/ldy/src/linux/include/asm-generic/param.h \
    $(wildcard include/config/HZ) \
  /home/ldy/src/linux/include/uapi/asm-generic/param.h \
  /home/ldy/src/linux/include/linux/spinlock_types.h \
    $(wildcard include/config/PREEMPT_RT) \
    $(wildcard include/config/DEBUG_LOCK_ALLOC) \
  /home/ldy/src/linux/include/linux/spinlock_types_raw.h \
    $(wildcard include/config/DEBUG_SPINLOCK) \
  /home/ldy/src/linux/arch/x86/include/asm/spinlock_types.h \
  /home/ldy/src/linux/include/asm-generic/qspinlock_types.h \
    $(wildcard include/config/NR_CPUS) \
  /home/ldy/src/linux/include/asm-generic/qrwlock_types.h \
  /home/ldy/src/linux/include/linux/lockdep_types.h \
    $(wildcard include/config/PROVE_RAW_LOCK_NESTING) \
    $(wildcard include/config/PREEMPT_LOCK) \
    $(wildcard include/config/LOCKDEP) \
    $(wildcard include/config/LOCK_STAT) \
  /home/ldy/src/linux/include/linux/rwlock_types.h \
  /home/ldy/src/linux/include/linux/once_lite.h \
  /home/ldy/src/linux/include/linux/dynamic_debug.h \
  /home/ldy/src/linux/include/linux/jump_label.h \
    $(wildcard include/config/HAVE_ARCH_JUMP_LABEL_RELATIVE) \
  /home/ldy/src/linux/arch/x86/include/asm/jump_label.h \
  /home/ldy/src/linux/include/linux/static_call_types.h \
    $(wildcard include/config/HAVE_STATIC_CALL) \
  /home/ldy/src/linux/include/linux/stat.h \
  /home/ldy/src/linux/arch/x86/include/uapi/asm/stat.h \
  /home/ldy/src/linux/include/uapi/linux/stat.h \
  /home/ldy/src/linux/include/linux/time.h \
    $(wildcard include/config/POSIX_TIMERS) \
  /home/ldy/src/linux/include/linux/math64.h \
    $(wildcard include/config/ARCH_SUPPORTS_INT128) \
  /home/ldy/src/linux/include/vdso/math64.h \
  /home/ldy/src/linux/include/linux/time64.h \
  /home/ldy/src/linux/include/vdso/time64.h \
  /home/ldy/src/linux/include/uapi/linux/time.h \
  /home/ldy/src/linux/include/uapi/linux/time_types.h \
  /home/ldy/src/linux/include/linux/time32.h \
  /home/ldy/src/linux/include/linux/timex.h \
  /home/ldy/src/linux/include/uapi/linux/timex.h \
  /home/ldy/src/linux/arch/x86/include/asm/timex.h \
  /home/ldy/src/linux/arch/x86/include/asm/processor.h \
    $(wildcard include/config/X86_VMX_FEATURE_NAMES) \
    $(wildcard include/config/X86_IOPL_IOPERM) \
    $(wildcard include/config/STACKPROTECTOR) \
    $(wildcard include/config/VM86) \
    $(wildcard include/config/X86_DEBUGCTLMSR) \
    $(wildcard include/config/CPU_SUP_AMD) \
    $(wildcard include/config/XEN) \
  /home/ldy/src/linux/arch/x86/include/asm/processor-flags.h \
  /home/ldy/src/linux/arch/x86/include/uapi/asm/processor-flags.h \
  /home/ldy/src/linux/include/linux/mem_encrypt.h \
    $(wildcard include/config/ARCH_HAS_MEM_ENCRYPT) \
    $(wildcard include/config/AMD_MEM_ENCRYPT) \
  /home/ldy/src/linux/arch/x86/include/asm/mem_encrypt.h \
  /home/ldy/src/linux/arch/x86/include/uapi/asm/bootparam.h \
  /home/ldy/src/linux/include/linux/screen_info.h \
  /home/ldy/src/linux/include/uapi/linux/screen_info.h \
  /home/ldy/src/linux/include/linux/apm_bios.h \
  /home/ldy/src/linux/include/uapi/linux/apm_bios.h \
  /home/ldy/src/linux/include/uapi/linux/ioctl.h \
  arch/x86/include/generated/uapi/asm/ioctl.h \
  /home/ldy/src/linux/include/asm-generic/ioctl.h \
  /home/ldy/src/linux/include/uapi/asm-generic/ioctl.h \
  /home/ldy/src/linux/include/linux/edd.h \
  /home/ldy/src/linux/include/uapi/linux/edd.h \
  /home/ldy/src/linux/arch/x86/include/asm/ist.h \
  /home/ldy/src/linux/arch/x86/include/uapi/asm/ist.h \
  /home/ldy/src/linux/include/video/edid.h \
    $(wildcard include/config/X86) \
  /home/ldy/src/linux/include/uapi/video/edid.h \
  /home/ldy/src/linux/arch/x86/include/asm/math_emu.h \
  /home/ldy/src/linux/arch/x86/include/asm/ptrace.h \
    $(wildcard include/config/PARAVIRT) \
    $(wildcard include/config/IA32_EMULATION) \
  /home/ldy/src/linux/arch/x86/include/asm/segment.h \
    $(wildcard include/config/XEN_PV) \
  /home/ldy/src/linux/arch/x86/include/asm/page_types.h \
    $(wildcard include/config/PHYSICAL_START) \
    $(wildcard include/config/PHYSICAL_ALIGN) \
    $(wildcard include/config/DYNAMIC_PHYSICAL_MASK) \
  /home/ldy/src/linux/arch/x86/include/asm/page_64_types.h \
    $(wildcard include/config/KASAN) \
    $(wildcard include/config/DYNAMIC_MEMORY_LAYOUT) \
    $(wildcard include/config/RANDOMIZE_BASE) \
  /home/ldy/src/linux/arch/x86/include/asm/kaslr.h \
    $(wildcard include/config/RANDOMIZE_MEMORY) \
  /home/ldy/src/linux/arch/x86/include/uapi/asm/ptrace.h \
  /home/ldy/src/linux/arch/x86/include/uapi/asm/ptrace-abi.h \
  /home/ldy/src/linux/arch/x86/include/asm/paravirt_types.h \
    $(wildcard include/config/PGTABLE_LEVELS) \
    $(wildcard include/config/PARAVIRT_DEBUG) \
  /home/ldy/src/linux/arch/x86/include/asm/desc_defs.h \
  /home/ldy/src/linux/arch/x86/include/asm/pgtable_types.h \
    $(wildcard include/config/MEM_SOFT_DIRTY) \
    $(wildcard include/config/HAVE_ARCH_USERFAULTFD_WP) \
    $(wildcard include/config/PROC_FS) \
  /home/ldy/src/linux/arch/x86/include/asm/pgtable_64_types.h \
    $(wildcard include/config/DEBUG_KMAP_LOCAL_FORCE_MAP) \
  /home/ldy/src/linux/arch/x86/include/asm/sparsemem.h \
    $(wildcard include/config/SPARSEMEM) \
    $(wildcard include/config/NUMA_KEEP_MEMINFO) \
  /home/ldy/src/linux/include/asm-generic/pgtable-nop4d.h \
  /home/ldy/src/linux/arch/x86/include/asm/nospec-branch.h \
  /home/ldy/src/linux/include/linux/static_key.h \
  /home/ldy/src/linux/include/linux/objtool.h \
  /home/ldy/src/linux/arch/x86/include/asm/msr-index.h \
  /home/ldy/src/linux/arch/x86/include/asm/unwind_hints.h \
  /home/ldy/src/linux/arch/x86/include/asm/orc_types.h \
  /home/ldy/src/linux/arch/x86/include/asm/proto.h \
  /home/ldy/src/linux/arch/x86/include/uapi/asm/ldt.h \
  /home/ldy/src/linux/arch/x86/include/uapi/asm/sigcontext.h \
  /home/ldy/src/linux/arch/x86/include/asm/current.h \
  /home/ldy/src/linux/arch/x86/include/asm/percpu.h \
    $(wildcard include/config/X86_64_SMP) \
  /home/ldy/src/linux/include/asm-generic/percpu.h \
    $(wildcard include/config/DEBUG_PREEMPT) \
    $(wildcard include/config/HAVE_SETUP_PER_CPU_AREA) \
  /home/ldy/src/linux/include/linux/threads.h \
    $(wildcard include/config/BASE_SMALL) \
  /home/ldy/src/linux/include/linux/percpu-defs.h \
    $(wildcard include/config/DEBUG_FORCE_WEAK_PER_CPU) \
  /home/ldy/src/linux/arch/x86/include/asm/page.h \
  /home/ldy/src/linux/arch/x86/include/asm/page_64.h \
    $(wildcard include/config/DEBUG_VIRTUAL) \
    $(wildcard include/config/FLATMEM) \
    $(wildcard include/config/X86_VSYSCALL_EMULATION) \
  /home/ldy/src/linux/include/linux/range.h \
  /home/ldy/src/linux/include/asm-generic/memory_model.h \
    $(wildcard include/config/SPARSEMEM_VMEMMAP) \
  /home/ldy/src/linux/include/linux/pfn.h \
  /home/ldy/src/linux/include/asm-generic/getorder.h \
  /home/ldy/src/linux/arch/x86/include/asm/msr.h \
  /home/ldy/src/linux/arch/x86/include/asm/msr-index.h \
  arch/x86/include/generated/uapi/asm/errno.h \
  /home/ldy/src/linux/include/uapi/asm-generic/errno.h \
  /home/ldy/src/linux/include/uapi/asm-generic/errno-base.h \
  /home/ldy/src/linux/arch/x86/include/asm/cpumask.h \
  /home/ldy/src/linux/include/linux/cpumask.h \
    $(wildcard include/config/CPUMASK_OFFSTACK) \
    $(wildcard include/config/HOTPLUG_CPU) \
    $(wildcard include/config/DEBUG_PER_CPU_MAPS) \
  /home/ldy/src/linux/include/linux/bitmap.h \
  /home/ldy/src/linux/include/linux/string.h \
    $(wildcard include/config/BINARY_PRINTF) \
    $(wildcard include/config/FORTIFY_SOURCE) \
  /home/ldy/src/linux/include/linux/errno.h \
  /home/ldy/src/linux/include/uapi/linux/errno.h \
  /home/ldy/src/linux/include/uapi/linux/string.h \
  /home/ldy/src/linux/arch/x86/include/asm/string.h \
  /home/ldy/src/linux/arch/x86/include/asm/string_64.h \
    $(wildcard include/config/ARCH_HAS_UACCESS_FLUSHCACHE) \
  /home/ldy/src/linux/include/linux/fortify-string.h \
  /home/ldy/src/linux/include/linux/atomic.h \
  /home/ldy/src/linux/arch/x86/include/asm/atomic.h \
  /home/ldy/src/linux/arch/x86/include/asm/cmpxchg.h \
  /home/ldy/src/linux/arch/x86/include/asm/cmpxchg_64.h \
  /home/ldy/src/linux/arch/x86/include/asm/atomic64_64.h \
  /home/ldy/src/linux/include/linux/atomic/atomic-arch-fallback.h \
    $(wildcard include/config/GENERIC_ATOMIC64) \
  /home/ldy/src/linux/include/linux/atomic/atomic-long.h \
  /home/ldy/src/linux/include/linux/atomic/atomic-instrumented.h \
  /home/ldy/src/linux/include/linux/bug.h \
    $(wildcard include/config/BUG_ON_DATA_CORRUPTION) \
  /home/ldy/src/linux/arch/x86/include/asm/bug.h \
    $(wildcard include/config/DEBUG_BUGVERBOSE) \
  /home/ldy/src/linux/include/linux/instrumentation.h \
    $(wildcard include/config/DEBUG_ENTRY) \
  /home/ldy/src/linux/include/asm-generic/bug.h \
    $(wildcard include/config/BUG) \
    $(wildcard include/config/GENERIC_BUG_RELATIVE_POINTERS) \
  /home/ldy/src/linux/arch/x86/include/uapi/asm/msr.h \
  /home/ldy/src/linux/include/linux/tracepoint-defs.h \
  /home/ldy/src/linux/arch/x86/include/asm/paravirt.h \
    $(wildcard include/config/PARAVIRT_SPINLOCKS) \
  /home/ldy/src/linux/arch/x86/include/asm/frame.h \
    $(wildcard include/config/FRAME_POINTER) \
  /home/ldy/src/linux/arch/x86/include/asm/special_insns.h \
  /home/ldy/src/linux/include/linux/irqflags.h \
    $(wildcard include/config/TRACE_IRQFLAGS) \
    $(wildcard include/config/IRQSOFF_TRACER) \
    $(wildcard include/config/PREEMPT_TRACER) \
    $(wildcard include/config/DEBUG_IRQFLAGS) \
    $(wildcard include/config/TRACE_IRQFLAGS_SUPPORT) \
  /home/ldy/src/linux/arch/x86/include/asm/irqflags.h \
  /home/ldy/src/linux/arch/x86/include/asm/fpu/types.h \
  /home/ldy/src/linux/arch/x86/include/asm/vmxfeatures.h \
  /home/ldy/src/linux/arch/x86/include/asm/vdso/processor.h \
  /home/ldy/src/linux/include/linux/personality.h \
  /home/ldy/src/linux/include/uapi/linux/personality.h \
  /home/ldy/src/linux/include/linux/err.h \
  /home/ldy/src/linux/arch/x86/include/asm/tsc.h \
    $(wildcard include/config/X86_TSC) \
  /home/ldy/src/linux/arch/x86/include/asm/cpufeature.h \
    $(wildcard include/config/X86_FEATURE_NAMES) \
  /home/ldy/src/linux/include/vdso/time32.h \
  /home/ldy/src/linux/include/vdso/time.h \
  /home/ldy/src/linux/include/linux/uidgid.h \
    $(wildcard include/config/MULTIUSER) \
    $(wildcard include/config/USER_NS) \
  /home/ldy/src/linux/include/linux/highuid.h \
  /home/ldy/src/linux/include/linux/buildid.h \
    $(wildcard include/config/CRASH_CORE) \
  /home/ldy/src/linux/include/linux/mm_types.h \
    $(wildcard include/config/HAVE_ALIGNED_STRUCT_PAGE) \
    $(wildcard include/config/MEMCG) \
    $(wildcard include/config/USERFAULTFD) \
    $(wildcard include/config/SWAP) \
    $(wildcard include/config/NUMA) \
    $(wildcard include/config/HAVE_ARCH_COMPAT_MMAP_BASES) \
    $(wildcard include/config/MEMBARRIER) \
    $(wildcard include/config/AIO) \
    $(wildcard include/config/MMU_NOTIFIER) \
    $(wildcard include/config/TRANSPARENT_HUGEPAGE) \
    $(wildcard include/config/NUMA_BALANCING) \
    $(wildcard include/config/ARCH_WANT_BATCHED_UNMAP_TLB_FLUSH) \
    $(wildcard include/config/HUGETLB_PAGE) \
    $(wildcard include/config/IOMMU_SUPPORT) \
  /home/ldy/src/linux/include/linux/mm_types_task.h \
    $(wildcard include/config/SPLIT_PTLOCK_CPUS) \
    $(wildcard include/config/ARCH_ENABLE_SPLIT_PMD_PTLOCK) \
  /home/ldy/src/linux/arch/x86/include/asm/tlbbatch.h \
  /home/ldy/src/linux/include/linux/auxvec.h \
  /home/ldy/src/linux/include/uapi/linux/auxvec.h \
  /home/ldy/src/linux/arch/x86/include/uapi/asm/auxvec.h \
  /home/ldy/src/linux/include/linux/spinlock.h \
    $(wildcard include/config/PREEMPTION) \
  /home/ldy/src/linux/include/linux/preempt.h \
    $(wildcard include/config/PREEMPT_COUNT) \
    $(wildcard include/config/TRACE_PREEMPT_TOGGLE) \
    $(wildcard include/config/PREEMPT_NOTIFIERS) \
  /home/ldy/src/linux/arch/x86/include/asm/preempt.h \
  /home/ldy/src/linux/include/linux/thread_info.h \
    $(wildcard include/config/THREAD_INFO_IN_TASK) \
    $(wildcard include/config/GENERIC_ENTRY) \
    $(wildcard include/config/HAVE_ARCH_WITHIN_STACK_FRAMES) \
    $(wildcard include/config/HARDENED_USERCOPY) \
  /home/ldy/src/linux/include/linux/restart_block.h \
  /home/ldy/src/linux/arch/x86/include/asm/thread_info.h \
  /home/ldy/src/linux/include/linux/bottom_half.h \
  /home/ldy/src/linux/include/linux/lockdep.h \
    $(wildcard include/config/DEBUG_LOCKING_API_SELFTESTS) \
  /home/ldy/src/linux/include/linux/smp.h \
    $(wildcard include/config/UP_LATE_INIT) \
  /home/ldy/src/linux/include/linux/smp_types.h \
  /home/ldy/src/linux/include/linux/llist.h \
    $(wildcard include/config/ARCH_HAVE_NMI_SAFE_CMPXCHG) \
  /home/ldy/src/linux/arch/x86/include/asm/smp.h \
    $(wildcard include/config/X86_LOCAL_APIC) \
    $(wildcard include/config/DEBUG_NMI_SELFTEST) \
  arch/x86/include/generated/asm/mmiowb.h \
  /home/ldy/src/linux/include/asm-generic/mmiowb.h \
    $(wildcard include/config/MMIOWB) \
  /home/ldy/src/linux/arch/x86/include/asm/spinlock.h \
  /home/ldy/src/linux/arch/x86/include/asm/qspinlock.h \
  /home/ldy/src/linux/include/asm-generic/qspinlock.h \
  /home/ldy/src/linux/arch/x86/include/asm/qrwlock.h \
  /home/ldy/src/linux/include/asm-generic/qrwlock.h \
  /home/ldy/src/linux/include/linux/rwlock.h \
    $(wildcard include/config/PREEMPT) \
  /home/ldy/src/linux/include/linux/spinlock_api_smp.h \
    $(wildcard include/config/INLINE_SPIN_LOCK) \
    $(wildcard include/config/INLINE_SPIN_LOCK_BH) \
    $(wildcard include/config/INLINE_SPIN_LOCK_IRQ) \
    $(wildcard include/config/INLINE_SPIN_LOCK_IRQSAVE) \
    $(wildcard include/config/INLINE_SPIN_TRYLOCK) \
    $(wildcard include/config/INLINE_SPIN_TRYLOCK_BH) \
    $(wildcard include/config/UNINLINE_SPIN_UNLOCK) \
    $(wildcard include/config/INLINE_SPIN_UNLOCK_BH) \
    $(wildcard include/config/INLINE_SPIN_UNLOCK_IRQ) \
    $(wildcard include/config/INLINE_SPIN_UNLOCK_IRQRESTORE) \
    $(wildcard include/config/GENERIC_LOCKBREAK) \
  /home/ldy/src/linux/include/linux/rwlock_api_smp.h \
    $(wildcard include/config/INLINE_READ_LOCK) \
    $(wildcard include/config/INLINE_WRITE_LOCK) \
    $(wildcard include/config/INLINE_READ_LOCK_BH) \
    $(wildcard include/config/INLINE_WRITE_LOCK_BH) \
    $(wildcard include/config/INLINE_READ_LOCK_IRQ) \
    $(wildcard include/config/INLINE_WRITE_LOCK_IRQ) \
    $(wildcard include/config/INLINE_READ_LOCK_IRQSAVE) \
    $(wildcard include/config/INLINE_WRITE_LOCK_IRQSAVE) \
    $(wildcard include/config/INLINE_READ_TRYLOCK) \
    $(wildcard include/config/INLINE_WRITE_TRYLOCK) \
    $(wildcard include/config/INLINE_READ_UNLOCK) \
    $(wildcard include/config/INLINE_WRITE_UNLOCK) \
    $(wildcard include/config/INLINE_READ_UNLOCK_BH) \
    $(wildcard include/config/INLINE_WRITE_UNLOCK_BH) \
    $(wildcard include/config/INLINE_READ_UNLOCK_IRQ) \
    $(wildcard include/config/INLINE_WRITE_UNLOCK_IRQ) \
    $(wildcard include/config/INLINE_READ_UNLOCK_IRQRESTORE) \
    $(wildcard include/config/INLINE_WRITE_UNLOCK_IRQRESTORE) \
  /home/ldy/src/linux/include/linux/rbtree.h \
  /home/ldy/src/linux/include/linux/rbtree_types.h \
  /home/ldy/src/linux/include/linux/rcupdate.h \
    $(wildcard include/config/PREEMPT_RCU) \
    $(wildcard include/config/TINY_RCU) \
    $(wildcard include/config/TASKS_RCU_GENERIC) \
    $(wildcard include/config/RCU_STALL_COMMON) \
    $(wildcard include/config/NO_HZ_FULL) \
    $(wildcard include/config/RCU_NOCB_CPU) \
    $(wildcard include/config/TASKS_RCU) \
    $(wildcard include/config/TASKS_TRACE_RCU) \
    $(wildcard include/config/TASKS_RUDE_RCU) \
    $(wildcard include/config/TREE_RCU) \
    $(wildcard include/config/DEBUG_OBJECTS_RCU_HEAD) \
    $(wildcard include/config/PROVE_RCU) \
    $(wildcard include/config/ARCH_WEAK_RELEASE_ACQUIRE) \
  /home/ldy/src/linux/include/linux/rcutree.h \
  /home/ldy/src/linux/include/linux/rwsem.h \
    $(wildcard include/config/RWSEM_SPIN_ON_OWNER) \
    $(wildcard include/config/DEBUG_RWSEMS) \
  /home/ldy/src/linux/include/linux/osq_lock.h \
  /home/ldy/src/linux/include/linux/completion.h \
  /home/ldy/src/linux/include/linux/swait.h \
  /home/ldy/src/linux/include/linux/wait.h \
  /home/ldy/src/linux/include/uapi/linux/wait.h \
  /home/ldy/src/linux/include/linux/uprobes.h \
    $(wildcard include/config/UPROBES) \
  /home/ldy/src/linux/arch/x86/include/asm/uprobes.h \
  /home/ldy/src/linux/include/linux/notifier.h \
  /home/ldy/src/linux/include/linux/mutex.h \
    $(wildcard include/config/MUTEX_SPIN_ON_OWNER) \
    $(wildcard include/config/DEBUG_MUTEXES) \
  /home/ldy/src/linux/include/linux/debug_locks.h \
  /home/ldy/src/linux/include/linux/srcu.h \
    $(wildcard include/config/TINY_SRCU) \
    $(wildcard include/config/SRCU) \
  /home/ldy/src/linux/include/linux/workqueue.h \
    $(wildcard include/config/DEBUG_OBJECTS_WORK) \
    $(wildcard include/config/FREEZER) \
    $(wildcard include/config/WQ_WATCHDOG) \
  /home/ldy/src/linux/include/linux/timer.h \
    $(wildcard include/config/DEBUG_OBJECTS_TIMERS) \
    $(wildcard include/config/NO_HZ_COMMON) \
  /home/ldy/src/linux/include/linux/ktime.h \
  /home/ldy/src/linux/include/linux/jiffies.h \
  /home/ldy/src/linux/include/vdso/jiffies.h \
  include/generated/timeconst.h \
  /home/ldy/src/linux/include/vdso/ktime.h \
  /home/ldy/src/linux/include/linux/timekeeping.h \
    $(wildcard include/config/GENERIC_CMOS_UPDATE) \
  /home/ldy/src/linux/include/linux/clocksource_ids.h \
  /home/ldy/src/linux/include/linux/debugobjects.h \
    $(wildcard include/config/DEBUG_OBJECTS) \
    $(wildcard include/config/DEBUG_OBJECTS_FREE) \
  /home/ldy/src/linux/include/linux/rcu_segcblist.h \
  /home/ldy/src/linux/include/linux/srcutree.h \
  /home/ldy/src/linux/include/linux/rcu_node_tree.h \
    $(wildcard include/config/RCU_FANOUT) \
    $(wildcard include/config/RCU_FANOUT_LEAF) \
  /home/ldy/src/linux/include/linux/page-flags-layout.h \
    $(wildcard include/config/KASAN_HW_TAGS) \
  /home/ldy/src/linux/include/linux/numa.h \
    $(wildcard include/config/NODES_SHIFT) \
  include/generated/bounds.h \
  /home/ldy/src/linux/include/linux/seqlock.h \
  /home/ldy/src/linux/include/linux/ww_mutex.h \
    $(wildcard include/config/DEBUG_RT_MUTEXES) \
    $(wildcard include/config/DEBUG_WW_MUTEX_SLOWPATH) \
  /home/ldy/src/linux/include/linux/rtmutex.h \
  /home/ldy/src/linux/arch/x86/include/asm/mmu.h \
    $(wildcard include/config/MODIFY_LDT_SYSCALL) \
  /home/ldy/src/linux/include/linux/kmod.h \
  /home/ldy/src/linux/include/linux/umh.h \
  /home/ldy/src/linux/include/linux/gfp.h \
    $(wildcard include/config/HIGHMEM) \
    $(wildcard include/config/ZONE_DMA) \
    $(wildcard include/config/ZONE_DMA32) \
    $(wildcard include/config/ZONE_DEVICE) \
    $(wildcard include/config/PM_SLEEP) \
    $(wildcard include/config/CONTIG_ALLOC) \
    $(wildcard include/config/CMA) \
  /home/ldy/src/linux/include/linux/mmdebug.h \
    $(wildcard include/config/DEBUG_VM) \
    $(wildcard include/config/DEBUG_VM_PGFLAGS) \
  /home/ldy/src/linux/include/linux/mmzone.h \
    $(wildcard include/config/FORCE_MAX_ZONEORDER) \
    $(wildcard include/config/MEMORY_ISOLATION) \
    $(wildcard include/config/ZSMALLOC) \
    $(wildcard include/config/SHADOW_CALL_STACK) \
    $(wildcard include/config/MEMORY_HOTPLUG) \
    $(wildcard include/config/COMPACTION) \
    $(wildcard include/config/PAGE_EXTENSION) \
    $(wildcard include/config/DEFERRED_STRUCT_PAGE_INIT) \
    $(wildcard include/config/HAVE_MEMORYLESS_NODES) \
    $(wildcard include/config/SPARSEMEM_EXTREME) \
    $(wildcard include/config/HAVE_ARCH_PFN_VALID) \
  /home/ldy/src/linux/include/linux/nodemask.h \
  /home/ldy/src/linux/include/linux/pageblock-flags.h \
    $(wildcard include/config/HUGETLB_PAGE_SIZE_VARIABLE) \
  /home/ldy/src/linux/include/linux/page-flags.h \
    $(wildcard include/config/ARCH_USES_PG_UNCACHED) \
    $(wildcard include/config/MEMORY_FAILURE) \
    $(wildcard include/config/PAGE_IDLE_FLAG) \
    $(wildcard include/config/THP_SWAP) \
    $(wildcard include/config/KSM) \
  /home/ldy/src/linux/include/linux/local_lock.h \
  /home/ldy/src/linux/include/linux/local_lock_internal.h \
  /home/ldy/src/linux/include/linux/memory_hotplug.h \
    $(wildcard include/config/ARCH_HAS_ADD_PAGES) \
    $(wildcard include/config/HAVE_ARCH_NODEDATA_EXTENSION) \
    $(wildcard include/config/MEMORY_HOTREMOVE) \
  /home/ldy/src/linux/arch/x86/include/asm/mmzone.h \
  /home/ldy/src/linux/arch/x86/include/asm/mmzone_64.h \
  /home/ldy/src/linux/include/linux/topology.h \
    $(wildcard include/config/USE_PERCPU_NUMA_NODE_ID) \
    $(wildcard include/config/SCHED_SMT) \
  /home/ldy/src/linux/include/linux/arch_topology.h \
    $(wildcard include/config/GENERIC_ARCH_TOPOLOGY) \
  /home/ldy/src/linux/include/linux/percpu.h \
    $(wildcard include/config/NEED_PER_CPU_EMBED_FIRST_CHUNK) \
    $(wildcard include/config/NEED_PER_CPU_PAGE_FIRST_CHUNK) \
  /home/ldy/src/linux/arch/x86/include/asm/topology.h \
    $(wildcard include/config/SCHED_MC_PRIO) \
    $(wildcard include/config/ACPI_CPPC_LIB) \
  /home/ldy/src/linux/arch/x86/include/asm/mpspec.h \
    $(wildcard include/config/EISA) \
    $(wildcard include/config/X86_MPPARSE) \
  /home/ldy/src/linux/arch/x86/include/asm/mpspec_def.h \
  /home/ldy/src/linux/arch/x86/include/asm/x86_init.h \
  /home/ldy/src/linux/arch/x86/include/asm/apicdef.h \
  /home/ldy/src/linux/include/asm-generic/topology.h \
  /home/ldy/src/linux/include/linux/sysctl.h \
    $(wildcard include/config/SYSCTL) \
  /home/ldy/src/linux/include/uapi/linux/sysctl.h \
  /home/ldy/src/linux/include/linux/elf.h \
    $(wildcard include/config/ARCH_USE_GNU_PROPERTY) \
    $(wildcard include/config/ARCH_HAVE_ELF_PROT) \
  /home/ldy/src/linux/arch/x86/include/asm/elf.h \
    $(wildcard include/config/X86_X32_ABI) \
  /home/ldy/src/linux/arch/x86/include/asm/user.h \
  /home/ldy/src/linux/arch/x86/include/asm/user_64.h \
  /home/ldy/src/linux/arch/x86/include/asm/fsgsbase.h \
  /home/ldy/src/linux/arch/x86/include/asm/vdso.h \
    $(wildcard include/config/X86_X32) \
  /home/ldy/src/linux/include/uapi/linux/elf.h \
  /home/ldy/src/linux/include/uapi/linux/elf-em.h \
  /home/ldy/src/linux/include/linux/kobject.h \
    $(wildcard include/config/UEVENT_HELPER) \
    $(wildcard include/config/DEBUG_KOBJECT_RELEASE) \
  /home/ldy/src/linux/include/linux/sysfs.h \
  /home/ldy/src/linux/include/linux/kernfs.h \
    $(wildcard include/config/KERNFS) \
  /home/ldy/src/linux/include/linux/idr.h \
  /home/ldy/src/linux/include/linux/radix-tree.h \
  /home/ldy/src/linux/include/linux/xarray.h \
    $(wildcard include/config/XARRAY_MULTI) \
  /home/ldy/src/linux/include/linux/kconfig.h \
  /home/ldy/src/linux/include/linux/kobject_ns.h \
  /home/ldy/src/linux/include/linux/kref.h \
  /home/ldy/src/linux/include/linux/refcount.h \
  /home/ldy/src/linux/include/linux/moduleparam.h \
    $(wildcard include/config/ALPHA) \
    $(wildcard include/config/IA64) \
    $(wildcard include/config/PPC64) \
  /home/ldy/src/linux/include/linux/rbtree_latch.h \
  /home/ldy/src/linux/include/linux/error-injection.h \
  /home/ldy/src/linux/include/asm-generic/error-injection.h \
  /home/ldy/src/linux/include/linux/cfi.h \
    $(wildcard include/config/CFI_CLANG_SHADOW) \
  /home/ldy/src/linux/arch/x86/include/asm/module.h \
    $(wildcard include/config/UNWINDER_ORC) \
  /home/ldy/src/linux/include/asm-generic/module.h \
    $(wildcard include/config/HAVE_MOD_ARCH_SPECIFIC) \
    $(wildcard include/config/MODULES_USE_ELF_REL) \
    $(wildcard include/config/MODULES_USE_ELF_RELA) \
  /home/ldy/src/linux/arch/x86/include/asm/orc_types.h \
  /home/ldy/src/linux/include/linux/fs.h \
    $(wildcard include/config/READ_ONLY_THP_FOR_FS) \
    $(wildcard include/config/FS_POSIX_ACL) \
    $(wildcard include/config/SECURITY) \
    $(wildcard include/config/CGROUP_WRITEBACK) \
    $(wildcard include/config/IMA) \
    $(wildcard include/config/FILE_LOCKING) \
    $(wildcard include/config/FSNOTIFY) \
    $(wildcard include/config/FS_ENCRYPTION) \
    $(wildcard include/config/FS_VERITY) \
    $(wildcard include/config/EPOLL) \
    $(wildcard include/config/UNICODE) \
    $(wildcard include/config/QUOTA) \
    $(wildcard include/config/FS_DAX) \
    $(wildcard include/config/BLOCK) \
    $(wildcard include/config/MIGRATION) \
  /home/ldy/src/linux/include/linux/wait_bit.h \
  /home/ldy/src/linux/include/linux/kdev_t.h \
  /home/ldy/src/linux/include/uapi/linux/kdev_t.h \
  /home/ldy/src/linux/include/linux/dcache.h \
  /home/ldy/src/linux/include/linux/rculist.h \
    $(wildcard include/config/PROVE_RCU_LIST) \
  /home/ldy/src/linux/include/linux/rculist_bl.h \
  /home/ldy/src/linux/include/linux/list_bl.h \
  /home/ldy/src/linux/include/linux/bit_spinlock.h \
  /home/ldy/src/linux/include/linux/lockref.h \
    $(wildcard include/config/ARCH_USE_CMPXCHG_LOCKREF) \
  /home/ldy/src/linux/include/linux/stringhash.h \
    $(wildcard include/config/DCACHE_WORD_ACCESS) \
  /home/ldy/src/linux/include/linux/hash.h \
    $(wildcard include/config/HAVE_ARCH_HASH) \
  /home/ldy/src/linux/include/linux/path.h \
  /home/ldy/src/linux/include/linux/list_lru.h \
    $(wildcard include/config/MEMCG_KMEM) \
  /home/ldy/src/linux/include/linux/shrinker.h \
  /home/ldy/src/linux/include/linux/pid.h \
  /home/ldy/src/linux/include/linux/capability.h \
  /home/ldy/src/linux/include/uapi/linux/capability.h \
  /home/ldy/src/linux/include/linux/semaphore.h \
  /home/ldy/src/linux/include/linux/fcntl.h \
    $(wildcard include/config/ARCH_32BIT_OFF_T) \
  /home/ldy/src/linux/include/uapi/linux/fcntl.h \
  arch/x86/include/generated/uapi/asm/fcntl.h \
  /home/ldy/src/linux/include/uapi/asm-generic/fcntl.h \
  /home/ldy/src/linux/include/uapi/linux/openat2.h \
  /home/ldy/src/linux/include/linux/migrate_mode.h \
  /home/ldy/src/linux/include/linux/percpu-rwsem.h \
  /home/ldy/src/linux/include/linux/rcuwait.h \
  /home/ldy/src/linux/include/linux/sched/signal.h \
    $(wildcard include/config/SCHED_AUTOGROUP) \
    $(wildcard include/config/BSD_PROCESS_ACCT) \
    $(wildcard include/config/TASKSTATS) \
    $(wildcard include/config/AUDIT) \
    $(wildcard include/config/STACK_GROWSUP) \
  /home/ldy/src/linux/include/linux/signal.h \
  /home/ldy/src/linux/include/linux/signal_types.h \
    $(wildcard include/config/OLD_SIGACTION) \
  /home/ldy/src/linux/include/uapi/linux/signal.h \
  /home/ldy/src/linux/arch/x86/include/asm/signal.h \
  /home/ldy/src/linux/arch/x86/include/uapi/asm/signal.h \
  /home/ldy/src/linux/include/uapi/asm-generic/signal-defs.h \
  /home/ldy/src/linux/arch/x86/include/uapi/asm/siginfo.h \
  /home/ldy/src/linux/include/uapi/asm-generic/siginfo.h \
  /home/ldy/src/linux/include/linux/sched.h \
    $(wildcard include/config/VIRT_CPU_ACCOUNTING_NATIVE) \
    $(wildcard include/config/SCHED_INFO) \
    $(wildcard include/config/SCHEDSTATS) \
    $(wildcard include/config/FAIR_GROUP_SCHED) \
    $(wildcard include/config/RT_GROUP_SCHED) \
    $(wildcard include/config/RT_MUTEXES) \
    $(wildcard include/config/UCLAMP_TASK) \
    $(wildcard include/config/UCLAMP_BUCKETS_COUNT) \
    $(wildcard include/config/KMAP_LOCAL) \
    $(wildcard include/config/SCHED_CORE) \
    $(wildcard include/config/CGROUP_SCHED) \
    $(wildcard include/config/BLK_DEV_IO_TRACE) \
    $(wildcard include/config/PSI) \
    $(wildcard include/config/COMPAT_BRK) \
    $(wildcard include/config/CGROUPS) \
    $(wildcard include/config/BLK_CGROUP) \
    $(wildcard include/config/PAGE_OWNER) \
    $(wildcard include/config/EVENTFD) \
    $(wildcard include/config/ARCH_HAS_SCALED_CPUTIME) \
    $(wildcard include/config/VIRT_CPU_ACCOUNTING_GEN) \
    $(wildcard include/config/POSIX_CPUTIMERS) \
    $(wildcard include/config/POSIX_CPU_TIMERS_TASK_WORK) \
    $(wildcard include/config/KEYS) \
    $(wildcard include/config/SYSVIPC) \
    $(wildcard include/config/DETECT_HUNG_TASK) \
    $(wildcard include/config/IO_URING) \
    $(wildcard include/config/AUDITSYSCALL) \
    $(wildcard include/config/UBSAN) \
    $(wildcard include/config/UBSAN_TRAP) \
    $(wildcard include/config/TASK_XACCT) \
    $(wildcard include/config/CPUSETS) \
    $(wildcard include/config/X86_CPU_RESCTRL) \
    $(wildcard include/config/FUTEX) \
    $(wildcard include/config/PERF_EVENTS) \
    $(wildcard include/config/RSEQ) \
    $(wildcard include/config/TASK_DELAY_ACCT) \
    $(wildcard include/config/FAULT_INJECTION) \
    $(wildcard include/config/LATENCYTOP) \
    $(wildcard include/config/KUNIT) \
    $(wildcard include/config/FUNCTION_GRAPH_TRACER) \
    $(wildcard include/config/BCACHE) \
    $(wildcard include/config/VMAP_STACK) \
    $(wildcard include/config/BPF_SYSCALL) \
    $(wildcard include/config/GCC_PLUGIN_STACKLEAK) \
    $(wildcard include/config/X86_MCE) \
    $(wildcard include/config/KRETPROBES) \
    $(wildcard include/config/ARCH_HAS_PARANOID_L1D_FLUSH) \
    $(wildcard include/config/ARCH_TASK_STRUCT_ON_STACK) \
    $(wildcard include/config/DEBUG_RSEQ) \
  /home/ldy/src/linux/include/uapi/linux/sched.h \
  /home/ldy/src/linux/include/linux/sem.h \
  /home/ldy/src/linux/include/uapi/linux/sem.h \
  /home/ldy/src/linux/include/linux/ipc.h \
  /home/ldy/src/linux/include/linux/rhashtable-types.h \
  /home/ldy/src/linux/include/uapi/linux/ipc.h \
  arch/x86/include/generated/uapi/asm/ipcbuf.h \
  /home/ldy/src/linux/include/uapi/asm-generic/ipcbuf.h \
  /home/ldy/src/linux/arch/x86/include/uapi/asm/sembuf.h \
  /home/ldy/src/linux/include/linux/shm.h \
  /home/ldy/src/linux/include/uapi/linux/shm.h \
  /home/ldy/src/linux/include/uapi/asm-generic/hugetlb_encode.h \
  /home/ldy/src/linux/arch/x86/include/uapi/asm/shmbuf.h \
  /home/ldy/src/linux/include/uapi/asm-generic/shmbuf.h \
  /home/ldy/src/linux/arch/x86/include/asm/shmparam.h \
  /home/ldy/src/linux/include/linux/plist.h \
    $(wildcard include/config/DEBUG_PLIST) \
  /home/ldy/src/linux/include/linux/hrtimer.h \
    $(wildcard include/config/HIGH_RES_TIMERS) \
    $(wildcard include/config/TIME_LOW_RES) \
    $(wildcard include/config/TIMERFD) \
  /home/ldy/src/linux/include/linux/hrtimer_defs.h \
  /home/ldy/src/linux/include/linux/timerqueue.h \
  /home/ldy/src/linux/include/linux/seccomp.h \
    $(wildcard include/config/SECCOMP) \
    $(wildcard include/config/HAVE_ARCH_SECCOMP_FILTER) \
    $(wildcard include/config/SECCOMP_FILTER) \
    $(wildcard include/config/CHECKPOINT_RESTORE) \
    $(wildcard include/config/SECCOMP_CACHE_DEBUG) \
  /home/ldy/src/linux/include/uapi/linux/seccomp.h \
  /home/ldy/src/linux/arch/x86/include/asm/seccomp.h \
  /home/ldy/src/linux/arch/x86/include/asm/unistd.h \
  /home/ldy/src/linux/arch/x86/include/uapi/asm/unistd.h \
  arch/x86/include/generated/uapi/asm/unistd_64.h \
  arch/x86/include/generated/asm/unistd_64_x32.h \
  arch/x86/include/generated/asm/unistd_32_ia32.h \
  /home/ldy/src/linux/arch/x86/include/asm/ia32_unistd.h \
  /home/ldy/src/linux/include/asm-generic/seccomp.h \
  /home/ldy/src/linux/include/uapi/linux/unistd.h \
  /home/ldy/src/linux/include/linux/resource.h \
  /home/ldy/src/linux/include/uapi/linux/resource.h \
  arch/x86/include/generated/uapi/asm/resource.h \
  /home/ldy/src/linux/include/asm-generic/resource.h \
  /home/ldy/src/linux/include/uapi/asm-generic/resource.h \
  /home/ldy/src/linux/include/linux/latencytop.h \
  /home/ldy/src/linux/include/linux/sched/prio.h \
  /home/ldy/src/linux/include/linux/sched/types.h \
  /home/ldy/src/linux/include/linux/syscall_user_dispatch.h \
  /home/ldy/src/linux/include/linux/task_io_accounting.h \
    $(wildcard include/config/TASK_IO_ACCOUNTING) \
  /home/ldy/src/linux/include/linux/posix-timers.h \
  /home/ldy/src/linux/include/linux/alarmtimer.h \
    $(wildcard include/config/RTC_CLASS) \
  /home/ldy/src/linux/include/linux/task_work.h \
  /home/ldy/src/linux/include/uapi/linux/rseq.h \
  /home/ldy/src/linux/include/linux/kcsan.h \
  arch/x86/include/generated/asm/kmap_size.h \
  /home/ldy/src/linux/include/asm-generic/kmap_size.h \
    $(wildcard include/config/DEBUG_KMAP_LOCAL) \
  /home/ldy/src/linux/include/linux/sched/jobctl.h \
  /home/ldy/src/linux/include/linux/sched/task.h \
    $(wildcard include/config/HAVE_EXIT_THREAD) \
    $(wildcard include/config/ARCH_WANTS_DYNAMIC_TASK_STRUCT) \
    $(wildcard include/config/HAVE_ARCH_THREAD_STRUCT_WHITELIST) \
  /home/ldy/src/linux/include/linux/uaccess.h \
    $(wildcard include/config/SET_FS) \
  /home/ldy/src/linux/include/linux/fault-inject-usercopy.h \
    $(wildcard include/config/FAULT_INJECTION_USERCOPY) \
  /home/ldy/src/linux/arch/x86/include/asm/uaccess.h \
    $(wildcard include/config/CC_HAS_ASM_GOTO_OUTPUT) \
    $(wildcard include/config/CC_ASM_GOTO_OUTPUT) \
    $(wildcard include/config/ARCH_HAS_COPY_MC) \
    $(wildcard include/config/X86_INTEL_USERCOPY) \
  /home/ldy/src/linux/arch/x86/include/asm/smap.h \
  /home/ldy/src/linux/arch/x86/include/asm/extable.h \
  /home/ldy/src/linux/arch/x86/include/asm/uaccess_64.h \
  /home/ldy/src/linux/include/linux/cred.h \
    $(wildcard include/config/DEBUG_CREDENTIALS) \
  /home/ldy/src/linux/include/linux/key.h \
    $(wildcard include/config/KEY_NOTIFICATIONS) \
    $(wildcard include/config/NET) \
  /home/ldy/src/linux/include/linux/assoc_array.h \
    $(wildcard include/config/ASSOCIATIVE_ARRAY) \
  /home/ldy/src/linux/include/linux/sched/user.h \
    $(wildcard include/config/WATCH_QUEUE) \
  /home/ldy/src/linux/include/linux/percpu_counter.h \
  /home/ldy/src/linux/include/linux/ratelimit.h \
  /home/ldy/src/linux/include/linux/rcu_sync.h \
  /home/ldy/src/linux/include/linux/delayed_call.h \
  /home/ldy/src/linux/include/linux/uuid.h \
  /home/ldy/src/linux/include/uapi/linux/uuid.h \
  /home/ldy/src/linux/include/linux/errseq.h \
  /home/ldy/src/linux/include/linux/ioprio.h \
  /home/ldy/src/linux/include/linux/sched/rt.h \
  /home/ldy/src/linux/include/linux/iocontext.h \
  /home/ldy/src/linux/include/uapi/linux/ioprio.h \
  /home/ldy/src/linux/include/linux/fs_types.h \
  /home/ldy/src/linux/include/linux/mount.h \
  /home/ldy/src/linux/include/uapi/linux/fs.h \
  /home/ldy/src/linux/include/linux/quota.h \
    $(wildcard include/config/QUOTA_NETLINK_INTERFACE) \
  /home/ldy/src/linux/include/uapi/linux/dqblk_xfs.h \
  /home/ldy/src/linux/include/linux/dqblk_v1.h \
  /home/ldy/src/linux/include/linux/dqblk_v2.h \
  /home/ldy/src/linux/include/linux/dqblk_qtree.h \
  /home/ldy/src/linux/include/linux/projid.h \
  /home/ldy/src/linux/include/uapi/linux/quota.h \
  /home/ldy/src/linux/include/linux/nfs_fs_i.h \
  /home/ldy/src/linux/include/linux/mm.h \
    $(wildcard include/config/HAVE_ARCH_MMAP_RND_BITS) \
    $(wildcard include/config/HAVE_ARCH_MMAP_RND_COMPAT_BITS) \
    $(wildcard include/config/DEBUG_INFO_BTF) \
    $(wildcard include/config/ARCH_USES_HIGH_VMA_FLAGS) \
    $(wildcard include/config/ARCH_HAS_PKEYS) \
    $(wildcard include/config/PPC) \
    $(wildcard include/config/PARISC) \
    $(wildcard include/config/SPARC64) \
    $(wildcard include/config/ARM64) \
    $(wildcard include/config/ARM64_MTE) \
    $(wildcard include/config/HAVE_ARCH_USERFAULTFD_MINOR) \
    $(wildcard include/config/SHMEM) \
    $(wildcard include/config/DEV_PAGEMAP_OPS) \
    $(wildcard include/config/DEVICE_PRIVATE) \
    $(wildcard include/config/PCI_P2PDMA) \
    $(wildcard include/config/ARCH_HAS_PTE_SPECIAL) \
    $(wildcard include/config/ARCH_HAS_PTE_DEVMAP) \
    $(wildcard include/config/DEBUG_VM_RB) \
    $(wildcard include/config/PAGE_POISONING) \
    $(wildcard include/config/INIT_ON_ALLOC_DEFAULT_ON) \
    $(wildcard include/config/INIT_ON_FREE_DEFAULT_ON) \
    $(wildcard include/config/DEBUG_PAGEALLOC) \
    $(wildcard include/config/HUGETLBFS) \
    $(wildcard include/config/MAPPING_DIRTY_HELPERS) \
  /home/ldy/src/linux/include/linux/mmap_lock.h \
  /home/ldy/src/linux/include/linux/percpu-refcount.h \
  /home/ldy/src/linux/include/linux/page_ext.h \
  /home/ldy/src/linux/include/linux/stacktrace.h \
    $(wildcard include/config/STACKTRACE) \
    $(wildcard include/config/ARCH_STACKWALK) \
    $(wildcard include/config/HAVE_RELIABLE_STACKTRACE) \
  /home/ldy/src/linux/include/linux/stackdepot.h \
    $(wildcard include/config/STACKDEPOT) \
  /home/ldy/src/linux/include/linux/page_ref.h \
    $(wildcard include/config/DEBUG_PAGE_REF) \
  /home/ldy/src/linux/include/linux/memremap.h \
  /home/ldy/src/linux/include/linux/ioport.h \
  /home/ldy/src/linux/include/linux/overflow.h \
  /home/ldy/src/linux/include/linux/sizes.h \
  /home/ldy/src/linux/include/linux/pgtable.h \
    $(wildcard include/config/HIGHPTE) \
    $(wildcard include/config/GUP_GET_PTE_LOW_HIGH) \
    $(wildcard include/config/HAVE_ARCH_TRANSPARENT_HUGEPAGE_PUD) \
    $(wildcard include/config/HAVE_ARCH_SOFT_DIRTY) \
    $(wildcard include/config/ARCH_ENABLE_THP_MIGRATION) \
    $(wildcard include/config/HAVE_ARCH_HUGE_VMAP) \
    $(wildcard include/config/X86_ESPFIX64) \
  /home/ldy/src/linux/arch/x86/include/asm/pgtable.h \
    $(wildcard include/config/DEBUG_WX) \
  /home/ldy/src/linux/arch/x86/include/asm/pkru.h \
  /home/ldy/src/linux/arch/x86/include/asm/fpu/xstate.h \
  /home/ldy/src/linux/arch/x86/include/asm/fpu/api.h \
    $(wildcard include/config/X86_DEBUG_FPU) \
  /home/ldy/src/linux/include/asm-generic/pgtable_uffd.h \
  /home/ldy/src/linux/arch/x86/include/asm/pgtable_64.h \
  /home/ldy/src/linux/arch/x86/include/asm/fixmap.h \
    $(wildcard include/config/PROVIDE_OHCI1394_DMA_INIT) \
    $(wildcard include/config/X86_IO_APIC) \
    $(wildcard include/config/PCI_MMCONFIG) \
    $(wildcard include/config/ACPI_APEI_GHES) \
    $(wildcard include/config/INTEL_TXT) \
  /home/ldy/src/linux/arch/x86/include/uapi/asm/vsyscall.h \
  /home/ldy/src/linux/include/asm-generic/fixmap.h \
  /home/ldy/src/linux/arch/x86/include/asm/pgtable-invert.h \
  /home/ldy/src/linux/include/linux/kasan.h \
    $(wildcard include/config/KASAN_STACK) \
    $(wildcard include/config/KASAN_VMALLOC) \
    $(wildcard include/config/KASAN_INLINE) \
  /home/ldy/src/linux/include/linux/huge_mm.h \
  /home/ldy/src/linux/include/linux/sched/coredump.h \
    $(wildcard include/config/CORE_DUMP_DEFAULT_ELF_HEADERS) \
  /home/ldy/src/linux/include/linux/vmstat.h \
    $(wildcard include/config/VM_EVENT_COUNTERS) \
    $(wildcard include/config/DEBUG_TLBFLUSH) \
    $(wildcard include/config/DEBUG_VM_VMACACHE) \
  /home/ldy/src/linux/include/linux/vm_event_item.h \
    $(wildcard include/config/MEMORY_BALLOON) \
    $(wildcard include/config/BALLOON_COMPACTION) \
  /home/ldy/src/linux/include/linux/mman.h \
  /home/ldy/src/linux/include/uapi/linux/mman.h \
  /home/ldy/src/linux/arch/x86/include/uapi/asm/mman.h \
  /home/ldy/src/linux/include/uapi/asm-generic/mman.h \
  /home/ldy/src/linux/include/uapi/asm-generic/mman-common.h \
  /home/ldy/src/linux/include/linux/miscdevice.h \
  /home/ldy/src/linux/include/uapi/linux/major.h \
  /home/ldy/src/linux/include/linux/device.h \
    $(wildcard include/config/ENERGY_MODEL) \
    $(wildcard include/config/GENERIC_MSI_IRQ_DOMAIN) \
    $(wildcard include/config/PINCTRL) \
    $(wildcard include/config/GENERIC_MSI_IRQ) \
    $(wildcard include/config/DMA_OPS) \
    $(wildcard include/config/DMA_DECLARE_COHERENT) \
    $(wildcard include/config/DMA_CMA) \
    $(wildcard include/config/SWIOTLB) \
    $(wildcard include/config/ARCH_HAS_SYNC_DMA_FOR_DEVICE) \
    $(wildcard include/config/ARCH_HAS_SYNC_DMA_FOR_CPU) \
    $(wildcard include/config/ARCH_HAS_SYNC_DMA_FOR_CPU_ALL) \
    $(wildcard include/config/DMA_OPS_BYPASS) \
    $(wildcard include/config/OF) \
    $(wildcard include/config/DEVTMPFS) \
    $(wildcard include/config/SYSFS_DEPRECATED) \
  /home/ldy/src/linux/include/linux/dev_printk.h \
  /home/ldy/src/linux/include/linux/energy_model.h \
  /home/ldy/src/linux/include/linux/sched/cpufreq.h \
    $(wildcard include/config/CPU_FREQ) \
  /home/ldy/src/linux/include/linux/sched/topology.h \
    $(wildcard include/config/SCHED_DEBUG) \
    $(wildcard include/config/SCHED_MC) \
    $(wildcard include/config/CPU_FREQ_GOV_SCHEDUTIL) \
  /home/ldy/src/linux/include/linux/sched/idle.h \
  /home/ldy/src/linux/include/linux/sched/sd_flags.h \
  /home/ldy/src/linux/include/linux/klist.h \
  /home/ldy/src/linux/include/linux/pm.h \
    $(wildcard include/config/VT_CONSOLE_SLEEP) \
    $(wildcard include/config/PM) \
    $(wildcard include/config/PM_CLK) \
    $(wildcard include/config/PM_GENERIC_DOMAINS) \
  /home/ldy/src/linux/include/linux/device/bus.h \
    $(wildcard include/config/ACPI) \
  /home/ldy/src/linux/include/linux/device/class.h \
  /home/ldy/src/linux/include/linux/device/driver.h \
  /home/ldy/src/linux/arch/x86/include/asm/device.h \
  /home/ldy/src/linux/include/linux/pm_wakeup.h \
  /home/ldy/src/linux/include/linux/slab.h \
    $(wildcard include/config/DEBUG_SLAB) \
    $(wildcard include/config/FAILSLAB) \
    $(wildcard include/config/HAVE_HARDENED_USERCOPY_ALLOCATOR) \
    $(wildcard include/config/SLAB) \
    $(wildcard include/config/SLUB) \
    $(wildcard include/config/SLOB) \
    $(wildcard include/config/CC_IS_GCC) \
    $(wildcard include/config/CLANG_VERSION) \

/home/ldy/src/ldy/fuse_copy_overheads/remap_bench.o: $(deps_/home/ldy/src/ldy/fuse_copy_overheads/remap_bench.o)

$(deps_/home/ldy/src/ldy/fuse_copy_overheads/remap_bench.o):
