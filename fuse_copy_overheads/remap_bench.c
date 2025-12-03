// remap_bench.c
// SPDX-License-Identifier: GPL-2.0
#define pr_fmt(fmt) "remap_bench: " fmt

#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>
#include <linux/timekeeping.h>
#include <linux/mutex.h>

#define DEVICE_NAME "remap_bench"

/* 유저 프로그램과 동일한 ioctl 번호 */
#define REMAP_BENCH_MAGIC  'r'

struct remap_reg {
    __u64 addr;
    __u64 len;
};

#define REMAP_BENCH_REGISTER   _IOW (REMAP_BENCH_MAGIC, 1, struct remap_reg)
#define REMAP_BENCH_REMAP_NEXT _IOWR(REMAP_BENCH_MAGIC, 2, __u64)

/* per-open context */
struct remap_ctx {
    struct page *pages[2];          /* 번갈아가며 매핑할 두 개의 페이지 */
    int          cur_idx;           /* 현재 매핑된 인덱스 */

    struct vm_area_struct *vma;     /* mmap 된 VMA */
    unsigned long addr;             /* vma->vm_start */
    unsigned long len;              /* vma 길이 (4096) */
    struct mm_struct *mm;           /* vma->vm_mm */
    struct remap_reg reg;           /* 유저가 REGISTER로 넘긴 정보 (옵션) */

    struct mutex lock;              /* ioctl 동시호출 보호 */
};

static int remap_bench_open(struct inode *inode, struct file *filp)
{
    struct remap_ctx *ctx;
    int i;

    ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
    if (!ctx)
        return -ENOMEM;

    mutex_init(&ctx->lock);

    /* 4KB 페이지 두 개 할당 (번갈아 remap) */
    for (i = 0; i < 2; i++) {
        ctx->pages[i] = alloc_page(GFP_KERNEL | __GFP_ZERO);
        if (!ctx->pages[i]) {
            while (i--)
                __free_page(ctx->pages[i]);
            kfree(ctx);
            return -ENOMEM;
        }
        /* 디버깅용 패턴 */
        memset(page_address(ctx->pages[i]), (i == 0) ? 0x11 : 0x22,
               PAGE_SIZE);
    }

    ctx->cur_idx = 0;
    filp->private_data = ctx;

    return 0;
}

static int remap_bench_release(struct inode *inode, struct file *filp)
{
    struct remap_ctx *ctx = filp->private_data;
    int i;

    if (!ctx)
        return 0;

    for (i = 0; i < 2; i++) {
        if (ctx->pages[i])
            __free_page(ctx->pages[i]);
    }

    kfree(ctx);
    return 0;
}

static int remap_bench_mmap(struct file *filp, struct vm_area_struct *vma)
{
    struct remap_ctx *ctx = filp->private_data;
    unsigned long len = vma->vm_end - vma->vm_start;
    unsigned long pfn;
    int ret;

    if (!ctx)
        return -EINVAL;

    if (len != PAGE_SIZE)
        return -EINVAL;

    /* PFN 매핑용 VMA 표시 */
    vma->vm_flags |= VM_DONTEXPAND | VM_DONTDUMP | VM_PFNMAP | VM_IO;

    pfn = page_to_pfn(ctx->pages[ctx->cur_idx]);

    ret = remap_pfn_range(vma, vma->vm_start, pfn,
                          len, vma->vm_page_prot);
    if (ret)
        return ret;

    /* VMA / mm 정보 저장 (실험에서는 mmap 이후에만 ioctl 호출한다고 가정) */
    ctx->vma = vma;
    ctx->addr = vma->vm_start;
    ctx->len  = len;
    ctx->mm   = vma->vm_mm;

    /* vma에서 다시 ctx 찾고 싶으면 이거 써도 됨 */
    vma->vm_private_data = ctx;

    return 0;
}

static long remap_bench_ioctl(struct file *filp,
                              unsigned int cmd, unsigned long arg)
{
    struct remap_ctx *ctx = filp->private_data;
    long ret = 0;

    if (!ctx)
        return -EINVAL;

    switch (cmd) {
    case REMAP_BENCH_REGISTER: {
        struct remap_reg reg;

        if (copy_from_user(&reg, (void __user *)arg, sizeof(reg)))
            return -EFAULT;

        /*
         * 여기서는 그냥 저장만 한다.
         * 원하면 reg.addr / reg.len이 ctx->addr / ctx->len과
         * 일치하는지 검증 로직을 추가할 수 있음.
         */
        ctx->reg = reg;
        return 0;
    }

    case REMAP_BENCH_REMAP_NEXT: {
        __u64 __user *up = (__u64 __user *)arg;
        __u64 start, end, delta;
        unsigned long new_pfn;
        int next;

        if (!ctx->vma || !ctx->mm)
            return -EINVAL;

        mutex_lock(&ctx->lock);

        /* 다음에 매핑할 페이지 index */
        next = ctx->cur_idx ^ 1;
        new_pfn = page_to_pfn(ctx->pages[next]);

        /* 실제로 PTE를 zapping + remap 하는 구간 시간 측정 */
        start = ktime_get_ns();

        /*
         * zap_vma_ptes:
         *  - PFNMAP VMA에서 해당 범위의 PTE 제거 (TLB flush 포함)
         * remap_pfn_range:
         *  - 동일 VA에 새 PFN을 다시 매핑
         */
        mmap_write_lock(ctx->mm);

        zap_vma_ptes(ctx->vma, ctx->addr, ctx->len);

        ret = remap_pfn_range(ctx->vma, ctx->addr, new_pfn,
                              ctx->len, ctx->vma->vm_page_prot);

        mmap_write_unlock(ctx->mm);

        end = ktime_get_ns();
        delta = end - start;

        if (!ret) {
            ctx->cur_idx = next;
            if (copy_to_user(up, &delta, sizeof(delta)))
                ret = -EFAULT;
        }

        mutex_unlock(&ctx->lock);
        return ret;
    }

    default:
        return -ENOTTY;
    }
}

static const struct file_operations remap_bench_fops = {
    .owner          = THIS_MODULE,
    .open           = remap_bench_open,
    .release        = remap_bench_release,
    .mmap           = remap_bench_mmap,
    .unlocked_ioctl = remap_bench_ioctl,
#ifdef CONFIG_COMPAT
    .compat_ioctl   = remap_bench_ioctl,
#endif
};

static struct miscdevice remap_bench_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = DEVICE_NAME,
    .fops  = &remap_bench_fops,
    .mode  = 0666,   /* 실험 편의상 world-writable */
};

static int __init remap_bench_init(void)
{
    int ret;

    ret = misc_register(&remap_bench_dev);
    if (ret) {
        pr_err("misc_register failed: %d\n", ret);
        return ret;
    }

    pr_info("/dev/%s registered\n", DEVICE_NAME);
    return 0;
}

static void __exit remap_bench_exit(void)
{
    misc_deregister(&remap_bench_dev);
    pr_info("unloaded\n");
}

module_init(remap_bench_init);
module_exit(remap_bench_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("You");
MODULE_DESCRIPTION("PFN remap benchmark device");

