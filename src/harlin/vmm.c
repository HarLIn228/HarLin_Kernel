#include "vmm.h"
#include "pmm.h"

static volatile u64* pml4 = 0;

u64 vmm_last_replaced_phys = 0;

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
    if (*entry & VMM_PRESENT) {
        if ((flags & VMM_USER) && !(*entry & VMM_USER))
            *entry |= VMM_USER;
        return (u64*)(*entry & 0xFFFFFFFFFF000);
    }
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

#define USER_ADDR_START 0x400000
#define USER_ADDR_END   0x800000

void vmm_map(u64 virt, u64 phys, u64 flags)
{
    u64 pml4_idx;
    u64 pdpt_idx;
    u64 pd_idx;
    u64 pt_idx;
    u64* pdpt;
    u64* pd;
    u64* pt;
    u64 old_entry;
    u64 old_phys;
    u64 old_flags;
    u64 old_pte;
    int i;

    if ((flags & VMM_USER) && (virt < USER_ADDR_START || virt >= USER_ADDR_END))
        return;

    pml4_idx = (virt >> 39) & 0x1FF;
    pdpt_idx = (virt >> 30) & 0x1FF;
    pd_idx = (virt >> 21) & 0x1FF;
    pt_idx = (virt >> 12) & 0x1FF;

    pdpt = get_or_alloc_table((u64*)&pml4[pml4_idx], flags);
    if (!pdpt)
        return;
    pd = get_or_alloc_table(&pdpt[pdpt_idx], flags);
    if (!pd)
        return;

    vmm_last_replaced_phys = 0;

    old_entry = pd[pd_idx];
    if (old_entry & VMM_PRESENT) {
        if (old_entry & VMM_HUGE) {
            old_phys = old_entry & 0xFFFFFFFFFF000;
            old_flags = old_entry & 0xFFF;
            pt = (u64*)pmm_alloc();
            if (!pt)
                return;
            clear_page((u64)pt);
            for (i = 0; i < 512; i++) {
                pt[i] = (old_phys + i * 4096) | (old_flags & ~VMM_HUGE);
            }
            old_pte = pt[pt_idx];
            if (old_pte & VMM_PRESENT)
                vmm_last_replaced_phys = old_pte & 0xFFFFFFFFFF000;
            pt[pt_idx] = (phys & 0xFFFFFFFFFF000) | VMM_PRESENT | flags;
            pd[pd_idx] = ((u64)pt & 0xFFFFFFFFFF000) | VMM_PRESENT | VMM_WRITABLE | (flags & VMM_USER);
            return;
        } else {
            pt = (u64*)(old_entry & 0xFFFFFFFFFF000);
        }
    } else {
        pt = (u64*)pmm_alloc();
        if (!pt)
            return;
        clear_page((u64)pt);
        pd[pd_idx] = ((u64)pt & 0xFFFFFFFFFF000) | VMM_PRESENT | VMM_WRITABLE | (flags & VMM_USER);
    }

    old_pte = pt[pt_idx];
    if (old_pte & VMM_PRESENT)
        vmm_last_replaced_phys = old_pte & 0xFFFFFFFFFF000;
    pt[pt_idx] = (phys & 0xFFFFFFFFFF000) | VMM_PRESENT | flags;
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

    if (!pml4)
        return 0;
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

int copy_from_user(void* kdst, u64 usrc, u64 len)
{
    u8* dst = (u8*)kdst;
    u64 off = 0;
    while (off < len) {
        u64 phys;
        u64 page_remain;
        u64 chunk;
        if (usrc + off < USER_ADDR_START || usrc + off >= USER_ADDR_END)
            return -1;
        phys = vmm_get_phys(usrc + off);
        if (!phys)
            return -1;
        page_remain = 4096 - ((usrc + off) & 0xFFF);
        chunk = len - off;
        if (chunk > page_remain)
            chunk = page_remain;
        {
            u64 i;
            const u8* src = (const u8*)(phys);
            for (i = 0; i < chunk; i++)
                dst[off + i] = src[i];
        }
        off += chunk;
    }
    return 0;
}

int copy_to_user(u64 udst, const void* ksrc, u64 len)
{
    const u8* src = (const u8*)ksrc;
    u64 off = 0;
    while (off < len) {
        u64 phys;
        u64 page_remain;
        u64 chunk;
        if (udst + off < USER_ADDR_START || udst + off >= USER_ADDR_END)
            return -1;
        phys = vmm_get_phys(udst + off);
        if (!phys)
            return -1;
        page_remain = 4096 - ((udst + off) & 0xFFF);
        chunk = len - off;
        if (chunk > page_remain)
            chunk = page_remain;
        {
            u64 i;
            u8* dst = (u8*)(phys);
            for (i = 0; i < chunk; i++)
                dst[i] = src[off + i];
        }
        off += chunk;
    }
    return 0;
}

int strncpy_from_user(char* kdst, u64 usrc, u64 maxlen)
{
    u64 i;
    if (maxlen == 0)
        return -1;
    for (i = 0; i < maxlen; i++) {
        u64 phys;
        u8 c;
        if (usrc + i < USER_ADDR_START || usrc + i >= USER_ADDR_END)
            return -1;
        phys = vmm_get_phys(usrc + i);
        if (!phys)
            return -1;
        c = *((const u8*)(phys));
        kdst[i] = c;
        if (c == 0)
            return 0;
    }
    kdst[maxlen - 1] = 0;
    return 0;
}
