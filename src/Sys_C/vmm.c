#include "vmm.h"
#include "pmm.h"

static volatile u64* pml4 = 0;

static void clear_page(u64 addr)
{
    u64* p = (u64*)addr;
    int i;
    for (i = 0; i < 512; i++)
        p[i] = 0;
}

static u64* get_or_alloc_table(u64* entry, u64 flags)
{
    u64 page;
    if (*entry & VMM_PRESENT)
        return (u64*)(*entry & 0xFFFFFFFFFF000);
    page = pmm_alloc();
    if (!page)
        return 0;
    clear_page(page);
    *entry = page | VMM_PRESENT | VMM_WRITABLE | (flags & VMM_USER);
    return (u64*)page;
}

void vmm_init(u64 pml4_phys)
{
    pml4 = (volatile u64*)pml4_phys;
}

void vmm_map(u64 virt, u64 phys, u64 flags)
{
    u64 pml4_idx = (virt >> 39) & 0x1FF;
    u64 pdpt_idx = (virt >> 30) & 0x1FF;
    u64 pd_idx = (virt >> 21) & 0x1FF;
    u64 pt_idx = (virt >> 12) & 0x1FF;
    u64* pdpt;
    u64* pd;
    u64* pt;

    pdpt = get_or_alloc_table((u64*)&pml4[pml4_idx], flags);
    if (!pdpt)
        return;
    pd = get_or_alloc_table(&pdpt[pdpt_idx], flags);
    if (!pd)
        return;
    pt = get_or_alloc_table(&pd[pd_idx], flags);
    if (!pt)
        return;
    pt[pt_idx] = (phys & 0xFFFFFFFFFF000) | VMM_PRESENT | flags;
}

void vmm_unmap(u64 virt)
{
    u64 pml4_idx = (virt >> 39) & 0x1FF;
    u64 pdpt_idx = (virt >> 30) & 0x1FF;
    u64 pd_idx = (virt >> 21) & 0x1FF;
    u64 pt_idx = (virt >> 12) & 0x1FF;
    u64* pdpt;
    u64* pd;
    u64* pt;

    if (!(pml4[pml4_idx] & VMM_PRESENT))
        return;
    pdpt = (u64*)(pml4[pml4_idx] & 0xFFFFFFFFFF000);
    if (!(pdpt[pdpt_idx] & VMM_PRESENT))
        return;
    pd = (u64*)(pdpt[pdpt_idx] & 0xFFFFFFFFFF000);
    if (!(pd[pd_idx] & VMM_PRESENT))
        return;
    pt = (u64*)(pd[pd_idx] & 0xFFFFFFFFFF000);
    pt[pt_idx] = 0;
}

u64 vmm_get_phys(u64 virt)
{
    u64 pml4_idx = (virt >> 39) & 0x1FF;
    u64 pdpt_idx = (virt >> 30) & 0x1FF;
    u64 pd_idx = (virt >> 21) & 0x1FF;
    u64 pt_idx = (virt >> 12) & 0x1FF;
    u64* pdpt;
    u64* pd;
    u64* pt;

    if (!(pml4[pml4_idx] & VMM_PRESENT))
        return 0;
    pdpt = (u64*)(pml4[pml4_idx] & 0xFFFFFFFFFF000);
    if (!(pdpt[pdpt_idx] & VMM_PRESENT))
        return 0;
    pd = (u64*)(pdpt[pdpt_idx] & 0xFFFFFFFFFF000);
    if (!(pd[pd_idx] & VMM_PRESENT))
        return 0;
    pt = (u64*)(pd[pd_idx] & 0xFFFFFFFFFF000);
    if (!(pt[pt_idx] & VMM_PRESENT))
        return 0;
    return (pt[pt_idx] & 0xFFFFFFFFFF000) | (virt & 0xFFF);
}
