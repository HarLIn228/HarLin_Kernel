#include "vmm.h"
#include "pmm.h"
#include "spinlock.h"

u64 vmm_last_replaced_phys = 0;

#define USER_ADDR_START 0x400000
#define USER_ADDR_END   0x800000

#define TEMP_MAP_VA     0x0000800000000000ULL
#define TEMP_PT_VA      (TEMP_MAP_VA + 0x0000ULL)
#define TEMP_WIN_VA     (TEMP_MAP_VA + 0x1000ULL)

static u64 g_kernel_pml4_phys = 0;
static u64 g_current_pml4_phys = 0;
static u64 g_rec_pdpt_phys = 0;
static u64 g_rec_pd_phys = 0;
static u64 g_rec_pt_phys = 0;
static u64 g_tmp_pdpt_phys = 0;
static u64 g_tmp_pd_phys = 0;
static u64 g_tmp_pt_phys = 0;
static u64 g_self_pt_phys = 0;
static struct spinlock vmm_lock = { 0 };

static void clear_page_phys(u64 phys)
{
    volatile u64* p = (volatile u64*)phys;
    int i;
    for (i = 0; i < 512; i++)
        p[i] = 0;
}

static void pml4_write_entry(u64 pml4_idx, u64 value)
{
    *(volatile u64*)(KERNEL_PML4_VIRT + pml4_idx * 8) = value;
}

static u64 pml4_read_entry(u64 pml4_idx)
{
    return *(volatile u64*)(KERNEL_PML4_VIRT + pml4_idx * 8);
}

static void set_temp_target(u64 phys, u64 flags)
{
    *(volatile u64*)(TEMP_PT_VA + 8) =
        (phys & 0x000FFFFFFFFFF000ULL) | (flags & 0xFFF) | VMM_PRESENT | VMM_WRITABLE;
    asm volatile ("invlpg (%0)" : : "r"(TEMP_WIN_VA) : "memory");
}

static u64 read_pt_entry(u64 pt_phys, u64 pt_idx)
{
    set_temp_target(pt_phys, VMM_PRESENT | VMM_WRITABLE);
    return *(volatile u64*)(TEMP_WIN_VA + pt_idx * 8);
}

static void write_pt_entry(u64 pt_phys, u64 pt_idx, u64 value)
{
    set_temp_target(pt_phys, VMM_PRESENT | VMM_WRITABLE);
    *(volatile u64*)(TEMP_WIN_VA + pt_idx * 8) = value;
    asm volatile ("invlpg (%0)" : : "r"(TEMP_WIN_VA + pt_idx * 8) : "memory");
}

static u64 read_pml4_entry_virt(u64 pml4_idx)
{
    return *(volatile u64*)(KERNEL_PML4_VIRT + pml4_idx * 8);
}

static void write_pml4_entry_virt(u64 pml4_idx, u64 value)
{
    *(volatile u64*)(KERNEL_PML4_VIRT + pml4_idx * 8) = value;
    asm volatile ("invlpg (%0)" : : "r"(KERNEL_PML4_VIRT + pml4_idx * 8) : "memory");
}

void vmm_init(u64 pml4_phys)
{
    int i;
    u64 old_cr3;
    spinlock_init(&vmm_lock);
    g_kernel_pml4_phys = pml4_phys;
    g_current_pml4_phys = pml4_phys;

    g_rec_pdpt_phys = pmm_alloc();
    g_rec_pd_phys = pmm_alloc();
    g_rec_pt_phys = pmm_alloc();
    g_tmp_pdpt_phys = pmm_alloc();
    g_tmp_pd_phys = pmm_alloc();
    g_tmp_pt_phys = pmm_alloc();
    g_self_pt_phys = pmm_alloc();

    if (!g_rec_pdpt_phys || !g_rec_pd_phys || !g_rec_pt_phys ||
        !g_tmp_pdpt_phys || !g_tmp_pd_phys || !g_tmp_pt_phys || !g_self_pt_phys) {
        for (;;) asm volatile("hlt");
    }

    clear_page_phys(g_rec_pdpt_phys);
    clear_page_phys(g_rec_pd_phys);
    clear_page_phys(g_rec_pt_phys);
    clear_page_phys(g_tmp_pdpt_phys);
    clear_page_phys(g_tmp_pd_phys);
    clear_page_phys(g_tmp_pt_phys);
    clear_page_phys(g_self_pt_phys);

    {
        u64 hi_pml4_idx = (KERNEL_PML4_VIRT >> 39) & 0x1FF;
        u64 hi_pdpt_idx = (KERNEL_PML4_VIRT >> 30) & 0x1FF;
        u64 hi_pd_idx   = (KERNEL_PML4_VIRT >> 21) & 0x1FF;
        u64 hi_pt_idx   = (KERNEL_PML4_VIRT >> 12) & 0x1FF;

        ((volatile u64*)g_rec_pt_phys)[hi_pt_idx] =
            pml4_phys | VMM_PRESENT | VMM_WRITABLE;
        ((volatile u64*)g_rec_pd_phys)[hi_pd_idx] =
            g_rec_pt_phys | VMM_PRESENT | VMM_WRITABLE;
        ((volatile u64*)g_rec_pdpt_phys)[hi_pdpt_idx] =
            g_rec_pd_phys | VMM_PRESENT | VMM_WRITABLE;
        ((volatile u64*)pml4_phys)[hi_pml4_idx] =
            g_rec_pdpt_phys | VMM_PRESENT | VMM_WRITABLE;
    }

    {
        u64 tmp_pml4_idx = (TEMP_MAP_VA >> 39) & 0x1FF;
        u64 tmp_pdpt_idx = (TEMP_MAP_VA >> 30) & 0x1FF;
        u64 tmp_pd_idx   = (TEMP_MAP_VA >> 21) & 0x1FF;

        for (i = 0; i < 512; i++)
            ((volatile u64*)g_tmp_pt_phys)[i] =
                g_tmp_pt_phys | VMM_PRESENT | VMM_WRITABLE;
        ((volatile u64*)g_tmp_pd_phys)[tmp_pd_idx] =
            g_tmp_pt_phys | VMM_PRESENT | VMM_WRITABLE;
        ((volatile u64*)g_tmp_pdpt_phys)[tmp_pdpt_idx] =
            g_tmp_pd_phys | VMM_PRESENT | VMM_WRITABLE;
        ((volatile u64*)pml4_phys)[tmp_pml4_idx] =
            g_tmp_pdpt_phys | VMM_PRESENT | VMM_WRITABLE;
    }

    asm volatile ("mov %0, %%cr3" : : "r"(pml4_phys) : "memory");
    old_cr3 = 0;
    (void)old_cr3;
}

void vmm_switch(u64 pml4_phys)
{
    spinlock_acquire(&vmm_lock);
    g_current_pml4_phys = pml4_phys;
    asm volatile ("mov %0, %%cr3" : : "r"(pml4_phys) : "memory");
    spinlock_release(&vmm_lock);
}

u64 vmm_current_pml4(void)
{
    u64 v;
    asm volatile ("mov %%cr3, %0" : "=r"(v));
    return v & 0x000FFFFFFFFFF000ULL;
}

u64 vmm_clone_kernel_pml4(void)
{
    u64 new_phys = pmm_alloc();
    u64 new_pdpt_phys;
    u64 new_pd_phys;
    u64 new_pt_phys;
    u64 hi_pml4_idx;
    u64 hi_pdpt_idx;
    u64 hi_pd_idx;
    u64 hi_pt_idx;
    int i;
    volatile u64* p;

    if (!new_phys) return 0;

    new_pdpt_phys = pmm_alloc();
    new_pd_phys = pmm_alloc();
    new_pt_phys = pmm_alloc();
    if (!new_pdpt_phys || !new_pd_phys || !new_pt_phys) {
        if (new_pdpt_phys) pmm_free(new_pdpt_phys);
        if (new_pd_phys) pmm_free(new_pd_phys);
        if (new_pt_phys) pmm_free(new_pt_phys);
        pmm_free(new_phys);
        return 0;
    }

    spinlock_acquire(&vmm_lock);
    set_temp_target(new_phys, VMM_PRESENT | VMM_WRITABLE);
    p = (volatile u64*)TEMP_WIN_VA;
    for (i = 0; i < 512; i++) p[i] = 0;

    set_temp_target(new_pdpt_phys, VMM_PRESENT | VMM_WRITABLE);
    p = (volatile u64*)TEMP_WIN_VA;
    for (i = 0; i < 512; i++) p[i] = 0;

    set_temp_target(new_pd_phys, VMM_PRESENT | VMM_WRITABLE);
    p = (volatile u64*)TEMP_WIN_VA;
    for (i = 0; i < 512; i++) p[i] = 0;

    set_temp_target(new_pt_phys, VMM_PRESENT | VMM_WRITABLE);
    p = (volatile u64*)TEMP_WIN_VA;
    for (i = 0; i < 512; i++) p[i] = 0;

    set_temp_target(new_phys, VMM_PRESENT | VMM_WRITABLE);
    p = (volatile u64*)TEMP_WIN_VA;
    for (i = 0; i < 512; i++)
        p[i] = *(volatile u64*)(KERNEL_PML4_VIRT + i * 8);

    hi_pml4_idx = (KERNEL_PML4_VIRT >> 39) & 0x1FF;
    hi_pdpt_idx = (KERNEL_PML4_VIRT >> 30) & 0x1FF;
    hi_pd_idx   = (KERNEL_PML4_VIRT >> 21) & 0x1FF;
    hi_pt_idx   = (KERNEL_PML4_VIRT >> 12) & 0x1FF;

    p[hi_pml4_idx] = new_pdpt_phys | VMM_PRESENT | VMM_WRITABLE;

    set_temp_target(new_pdpt_phys, VMM_PRESENT | VMM_WRITABLE);
    p = (volatile u64*)TEMP_WIN_VA;
    p[hi_pdpt_idx] = new_pd_phys | VMM_PRESENT | VMM_WRITABLE;

    set_temp_target(new_pd_phys, VMM_PRESENT | VMM_WRITABLE);
    p = (volatile u64*)TEMP_WIN_VA;
    p[hi_pd_idx] = new_pt_phys | VMM_PRESENT | VMM_WRITABLE;

    set_temp_target(new_pt_phys, VMM_PRESENT | VMM_WRITABLE);
    p = (volatile u64*)TEMP_WIN_VA;
    p[hi_pt_idx] = new_phys | VMM_PRESENT | VMM_WRITABLE;
    spinlock_release(&vmm_lock);

    return new_phys;
}

static u64 get_or_alloc_table(u64 table_phys, u64 idx, u64 flags)
{
    u64 entry = read_pt_entry(table_phys, idx);
    u64 page;
    if (entry & VMM_PRESENT) {
        u64 new_entry = entry;
        if ((flags & VMM_USER) && !(new_entry & VMM_USER))
            new_entry |= VMM_USER;
        if (new_entry != entry) {
            write_pt_entry(table_phys, idx, new_entry);
        }
        return entry & 0x000FFFFFFFFFF000ULL;
    }
    page = pmm_alloc();
    if (!page)
        return 0;
    clear_page_phys(page);
    {
        u64 new_entry = page | VMM_PRESENT | VMM_WRITABLE | (flags & VMM_USER);
        write_pt_entry(table_phys, idx, new_entry);
    }
    return page;
}

int vmm_map(u64 virt, u64 phys, u64 flags)
{
    u64 pml4_idx;
    u64 pdpt_idx;
    u64 pd_idx;
    u64 pt_idx;
    u64 pml4_phys;
    u64 pdpt_phys;
    u64 pd_phys;
    u64 pt_phys;
    u64 old_entry;
    u64 old_phys;
    u64 old_flags;
    int i;
    int ret = -1;

    if ((flags & VMM_USER) && (virt < USER_ADDR_START || virt >= USER_ADDR_END))
        return -1;

    spinlock_acquire(&vmm_lock);

    pml4_idx = (virt >> 39) & 0x1FF;
    pdpt_idx = (virt >> 30) & 0x1FF;
    pd_idx = (virt >> 21) & 0x1FF;
    pt_idx = (virt >> 12) & 0x1FF;

    pml4_phys = g_current_pml4_phys;
    pdpt_phys = get_or_alloc_table(pml4_phys, pml4_idx, flags);
    if (!pdpt_phys) goto out;
    pd_phys = get_or_alloc_table(pdpt_phys, pdpt_idx, flags);
    if (!pd_phys) goto out;

    vmm_last_replaced_phys = 0;

    old_entry = read_pt_entry(pd_phys, pd_idx);
    if (old_entry & VMM_PRESENT) {
        if (old_entry & VMM_HUGE) {
            old_phys = old_entry & 0x000FFFFFFFFFF000ULL;
            old_flags = old_entry & 0xFFF;
            pt_phys = pmm_alloc();
            if (!pt_phys) goto out;
            clear_page_phys(pt_phys);
            for (i = 0; i < 512; i++) {
                write_pt_entry(pt_phys, i,
                    (old_phys + i * 4096) | (old_flags & ~VMM_HUGE));
            }
            {
                u64 old_pte = read_pt_entry(pt_phys, pt_idx);
                if (old_pte & VMM_PRESENT)
                    vmm_last_replaced_phys = old_pte & 0x000FFFFFFFFFF000ULL;
            }
            write_pt_entry(pt_phys, pt_idx,
                (phys & 0x000FFFFFFFFFF000ULL) | VMM_PRESENT | flags);
            write_pt_entry(pd_phys, pd_idx,
                (pt_phys & 0x000FFFFFFFFFF000ULL) | VMM_PRESENT | VMM_WRITABLE | (flags & VMM_USER));
            ret = 0;
            goto out;
        } else {
            pt_phys = old_entry & 0x000FFFFFFFFFF000ULL;
        }
    } else {
        pt_phys = pmm_alloc();
        if (!pt_phys) goto out;
        clear_page_phys(pt_phys);
        write_pt_entry(pd_phys, pd_idx,
            (pt_phys & 0x000FFFFFFFFFF000ULL) | VMM_PRESENT | VMM_WRITABLE | (flags & VMM_USER));
    }

    {
        u64 old_pte = read_pt_entry(pt_phys, pt_idx);
        if (old_pte & VMM_PRESENT)
            vmm_last_replaced_phys = old_pte & 0x000FFFFFFFFFF000ULL;
    }
    write_pt_entry(pt_phys, pt_idx,
        (phys & 0x000FFFFFFFFFF000ULL) | VMM_PRESENT | flags);
    ret = 0;
out:
    spinlock_release(&vmm_lock);
    return ret;
}

int vmm_mapped(u64 virt)
{
    return vmm_get_phys(virt) != 0;
}

int vmm_unmap_and_free(u64 virt)
{
    u64 phys = vmm_get_phys(virt);
    if (!phys)
        return -1;
    vmm_unmap(virt);
    pmm_free(phys);
    return 0;
}

void vmm_unmap(u64 virt)
{
    u64 pml4_idx = (virt >> 39) & 0x1FF;
    u64 pdpt_idx = (virt >> 30) & 0x1FF;
    u64 pd_idx = (virt >> 21) & 0x1FF;
    u64 pt_idx = (virt >> 12) & 0x1FF;
    u64 pml4_phys;
    u64 pdpt_phys;
    u64 pd_phys;
    u64 pt_phys;
    u64 entry;

    pml4_phys = g_current_pml4_phys;
    entry = read_pt_entry(pml4_phys, pml4_idx);
    if (!(entry & VMM_PRESENT)) return;
    pdpt_phys = entry & 0x000FFFFFFFFFF000ULL;
    entry = read_pt_entry(pdpt_phys, pdpt_idx);
    if (!(entry & VMM_PRESENT)) return;
    pd_phys = entry & 0x000FFFFFFFFFF000ULL;
    entry = read_pt_entry(pd_phys, pd_idx);
    if (!(entry & VMM_PRESENT)) return;
    if (entry & VMM_HUGE) {
        write_pt_entry(pd_phys, pd_idx, 0);
        return;
    }
    pt_phys = entry & 0x000FFFFFFFFFF000ULL;
    write_pt_entry(pt_phys, pt_idx, 0);
    asm volatile ("invlpg (%0)" : : "r"(virt) : "memory");
}

static u64 vmm_get_phys_locked(u64 virt)
{
    u64 pml4_idx = (virt >> 39) & 0x1FF;
    u64 pdpt_idx = (virt >> 30) & 0x1FF;
    u64 pd_idx = (virt >> 21) & 0x1FF;
    u64 pt_idx = (virt >> 12) & 0x1FF;
    u64 pml4_phys;
    u64 pdpt_phys;
    u64 pd_phys;
    u64 pt_phys;
    u64 entry;

    if (!g_current_pml4_phys)
        return 0;

    pml4_phys = g_current_pml4_phys;
    entry = read_pt_entry(pml4_phys, pml4_idx);
    if (!(entry & VMM_PRESENT)) return 0;
    pdpt_phys = entry & 0x000FFFFFFFFFF000ULL;
    entry = read_pt_entry(pdpt_phys, pdpt_idx);
    if (!(entry & VMM_PRESENT)) return 0;
    pd_phys = entry & 0x000FFFFFFFFFF000ULL;
    entry = read_pt_entry(pd_phys, pd_idx);
    if (!(entry & VMM_PRESENT)) return 0;
    if (entry & VMM_HUGE)
        return (entry & 0x000FFFFFFFFFF000ULL) | (virt & 0x1FFFFF);
    pt_phys = entry & 0x000FFFFFFFFFF000ULL;
    entry = read_pt_entry(pt_phys, pt_idx);
    if (!(entry & VMM_PRESENT)) return 0;
    return (entry & 0x000FFFFFFFFFF000ULL) | (virt & 0xFFF);
}

u64 vmm_get_phys(u64 virt)
{
    u64 phys;
    spinlock_acquire(&vmm_lock);
    phys = vmm_get_phys_locked(virt);
    spinlock_release(&vmm_lock);
    return phys;
}

int copy_from_user(void* kdst, u64 usrc, u64 len)
{
    u8* dst = (u8*)kdst;
    u64 off = 0;
    int ret = -1;

    if (len == 0)
        return 0;
    if (usrc < USER_ADDR_START || usrc + len < usrc || usrc + len > USER_ADDR_END)
        return -1;

    spinlock_acquire(&vmm_lock);

    while (off < len) {
        u64 phys;
        u64 page_remain;
        u64 chunk;
        u64 i;
        u8* src;
        phys = vmm_get_phys_locked(usrc + off);
        if (!phys) goto out;
        page_remain = 4096 - ((usrc + off) & 0xFFF);
        chunk = len - off;
        if (chunk > page_remain)
            chunk = page_remain;
        set_temp_target(phys & ~0xFFFULL, VMM_PRESENT | VMM_USER);
        src = (u8*)(TEMP_WIN_VA + (phys & 0xFFF));
        for (i = 0; i < chunk; i++)
            dst[off + i] = src[i];
        off += chunk;
    }
    ret = 0;
out:
    spinlock_release(&vmm_lock);
    return ret;
}

int copy_to_user(u64 udst, const void* ksrc, u64 len)
{
    const u8* src = (const u8*)ksrc;
    u64 off = 0;
    int ret = -1;

    if (len == 0)
        return 0;
    if (udst < USER_ADDR_START || udst + len < udst || udst + len > USER_ADDR_END)
        return -1;

    spinlock_acquire(&vmm_lock);

    while (off < len) {
        u64 phys;
        u64 page_remain;
        u64 chunk;
        u64 i;
        u8* dst;
        phys = vmm_get_phys_locked(udst + off);
        if (!phys) goto out;
        page_remain = 4096 - ((udst + off) & 0xFFF);
        chunk = len - off;
        if (chunk > page_remain)
            chunk = page_remain;
        set_temp_target(phys & ~0xFFFULL, VMM_PRESENT | VMM_USER | VMM_WRITABLE);
        dst = (u8*)(TEMP_WIN_VA + (phys & 0xFFF));
        for (i = 0; i < chunk; i++)
            dst[i] = src[off + i];
        off += chunk;
    }
    ret = 0;
out:
    spinlock_release(&vmm_lock);
    return ret;
}

int strncpy_from_user(char* kdst, u64 usrc, u64 maxlen)
{
    u64 i;
    int ret = -1;

    if (maxlen == 0)
        return -1;
    if (usrc < USER_ADDR_START || usrc + maxlen < usrc || usrc + maxlen > USER_ADDR_END)
        return -1;

    spinlock_acquire(&vmm_lock);

    for (i = 0; i < maxlen; i++) {
        u64 phys;
        u8 c;
        phys = vmm_get_phys_locked(usrc + i);
        if (!phys) goto out;
        set_temp_target(phys & ~0xFFFULL, VMM_PRESENT | VMM_USER);
        c = *(volatile u8*)(TEMP_WIN_VA + (phys & 0xFFF));
        kdst[i] = c;
        if (c == 0) {
            ret = 0;
            goto out;
        }
    }
    kdst[maxlen - 1] = 0;
    ret = 0;
out:
    spinlock_release(&vmm_lock);
    return ret;
}
