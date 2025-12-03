cmd_/home/ldy/src/ldy/fuse_copy_overheads/remap_bench.mod.o := gcc -Wp,-MMD,/home/ldy/src/ldy/fuse_copy_overheads/.remap_bench.mod.o.d -nostdinc -isystem /usr/lib/gcc/x86_64-linux-gnu/9/include -I/home/ldy/src/linux/arch/x86/include -I./arch/x86/include/generated -I/home/ldy/src/linux/include -I./include -I/home/ldy/src/linux/arch/x86/include/uapi -I./arch/x86/include/generated/uapi -I/home/ldy/src/linux/include/uapi -I./include/generated/uapi -include /home/ldy/src/linux/include/linux/compiler-version.h -include /home/ldy/src/linux/include/linux/kconfig.h -include /home/ldy/src/linux/include/linux/compiler_types.h -D__KERNEL__ -fmacro-prefix-map=/home/ldy/src/linux/= -Wall -Wundef -Werror=strict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -fshort-wchar -fno-PIE -Werror=implicit-function-declaration -Werror=implicit-int -Werror=return-type -Wno-format-security -std=gnu89 -mno-sse -mno-mmx -mno-sse2 -mno-3dnow -mno-avx -fcf-protection=none -m64 -falign-jumps=1 -falign-loops=1 -mno-80387 -mno-fp-ret-in-387 -mpreferred-stack-boundary=3 -mskip-rax-setup -mtune=generic -mno-red-zone -mcmodel=kernel -DCONFIG_X86_X32_ABI -Wno-sign-compare -fno-asynchronous-unwind-tables -mindirect-branch=thunk-extern -mindirect-branch-register -fno-jump-tables -fno-delete-null-pointer-checks -Wno-frame-address -Wno-format-truncation -Wno-format-overflow -Wno-address-of-packed-member -O2 --param=allow-store-data-races=0 -Wframe-larger-than=1024 -fstack-protector-strong -Wimplicit-fallthrough=5 -Wno-main -Wno-unused-but-set-variable -Wno-unused-const-variable -fno-omit-frame-pointer -fno-optimize-sibling-calls -fno-stack-clash-protection -g -gdwarf-4 -pg -mrecord-mcount -mfentry -DCC_USING_FENTRY -Wdeclaration-after-statement -Wvla -Wno-pointer-sign -Wno-stringop-truncation -Wno-array-bounds -Wno-stringop-overflow -Wno-restrict -Wno-maybe-uninitialized -fno-strict-overflow -fno-stack-check -fconserve-stack -Werror=date-time -Werror=incompatible-pointer-types -Werror=designated-init -Wno-packed-not-aligned -DMODULE -DKBUILD_BASENAME='"remap_bench.mod"' -DKBUILD_MODNAME='"remap_bench"' -D__KBUILD_MODNAME=kmod_remap_bench -c -o /home/ldy/src/ldy/fuse_copy_overheads/remap_bench.mod.o /home/ldy/src/ldy/fuse_copy_overheads/remap_bench.mod.c

source_/home/ldy/src/ldy/fuse_copy_overheads/remap_bench.mod.o := /home/ldy/src/ldy/fuse_copy_overheads/remap_bench.mod.c

deps_/home/ldy/src/ldy/fuse_copy_overheads/remap_bench.mod.o := \
    $(wildcard include/config/MODULE_UNLOAD) \
    $(wildcard include/config/RETPOLINE) \
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
    $(wildcard include/config/COMPAT) \
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
  /home/ldy/src/linux/include/linux/build-salt.h \
    $(wildcard include/config/BUILD_SALT) \
  /home/ldy/src/linux/include/linux/elfnote.h \
  /home/ldy/src/linux/include/linux/elfnote-lto.h \
    $(wildcard include/config/LTO) \
  /home/ldy/src/linux/include/linux/vermagic.h \
  include/generated/utsrelease.h \
  /home/ldy/src/linux/arch/x86/include/asm/vermagic.h \
    $(wildcard include/config/M486SX) \
    $(wildcard include/config/M486) \
    $(wildcard include/config/M586) \
    $(wildcard include/config/M586TSC) \
    $(wildcard include/config/M586MMX) \
    $(wildcard include/config/MCORE2) \
    $(wildcard include/config/M686) \
    $(wildcard include/config/MPENTIUMII) \
    $(wildcard include/config/MPENTIUMIII) \
    $(wildcard include/config/MPENTIUMM) \
    $(wildcard include/config/MPENTIUM4) \
    $(wildcard include/config/MK6) \
    $(wildcard include/config/MK7) \
    $(wildcard include/config/MK8) \
    $(wildcard include/config/MELAN) \
    $(wildcard include/config/MCRUSOE) \
    $(wildcard include/config/MEFFICEON) \
    $(wildcard include/config/MWINCHIPC6) \
    $(wildcard include/config/MWINCHIP3D) \
    $(wildcard include/config/MCYRIXIII) \
    $(wildcard include/config/MVIAC3_2) \
    $(wildcard include/config/MVIAC7) \
    $(wildcard include/config/MGEODEGX1) \
    $(wildcard include/config/MGEODE_LX) \

/home/ldy/src/ldy/fuse_copy_overheads/remap_bench.mod.o: $(deps_/home/ldy/src/ldy/fuse_copy_overheads/remap_bench.mod.o)

$(deps_/home/ldy/src/ldy/fuse_copy_overheads/remap_bench.mod.o):
