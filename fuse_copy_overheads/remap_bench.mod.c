#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;
BUILD_LTO_INFO;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif

static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0xa155f925, "module_layout" },
	{ 0xb404fa06, "misc_deregister" },
	{ 0x92997ed8, "_printk" },
	{ 0x32799290, "misc_register" },
	{ 0xd0da656b, "__stack_chk_fail" },
	{ 0x6b10bee1, "_copy_to_user" },
	{ 0x26f07645, "__mmap_lock_do_trace_acquire_returned" },
	{ 0x4ef25e2b, "__mmap_lock_do_trace_released" },
	{ 0xd7e6d549, "__mmap_lock_do_trace_start_locking" },
	{ 0x13c49cc2, "_copy_from_user" },
	{ 0x3213f038, "mutex_unlock" },
	{ 0xce807a25, "up_write" },
	{ 0x8e4d2b19, "__tracepoint_mmap_lock_released" },
	{ 0xb68771f7, "zap_vma_ptes" },
	{ 0x452208eb, "__tracepoint_mmap_lock_acquire_returned" },
	{ 0x57bc19d2, "down_write" },
	{ 0x26ce0657, "__tracepoint_mmap_lock_start_locking" },
	{ 0xb43f9365, "ktime_get" },
	{ 0x4dfa8d4b, "mutex_lock" },
	{ 0x897dd41b, "remap_pfn_range" },
	{ 0x7cd8d75e, "page_offset_base" },
	{ 0x97651e6c, "vmemmap_base" },
	{ 0xfa6d8a29, "alloc_pages" },
	{ 0xcefb0c9f, "__mutex_init" },
	{ 0xcea5897, "kmem_cache_alloc_trace" },
	{ 0xd5f5fcad, "kmalloc_caches" },
	{ 0x37a0cba, "kfree" },
	{ 0xd24fe6eb, "__free_pages" },
	{ 0xbdfb6dbb, "__fentry__" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "843021E03A5EAAB5C6D31A0");
