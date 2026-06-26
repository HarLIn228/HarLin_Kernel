#include "page_fault.h"
#include "vmm.h"
#include "pmm.h"
#include "printk.h"
#include "bug.h"

#define VMM_USER_BIT  0x04
#define VMM_RW_BIT    0x02
#define VMM_P_BIT     0x01

static inline void invlpg(u64 addr)
{
    asm volatile ("invlpg (%0)" : : "r"(addr) : "memory");
}

int Harlin_PageFaultDemandInstall(u64 virt, u64 flags)
{
    u64 page = pmm_alloc();
    if (page == 0) {
        pr_err("demand alloc failed at %p", (void*)virt);
        return -1;
    }
    u8* p = (u8*)page;
    for (u64 i = 0; i < 4096; i++) {
        p[i] = 0;
    }
    if (vmm_map(virt, page, flags) != 0) {
        pmm_free(page);
        pr_err("demand map failed at %p", (void*)virt);
        return -1;
    }
    invlpg(virt);
    return 0;
}

int Harlin_PageFaultCowResolve(u64 virt, u64 flags)
{
    u64 phys = vmm_get_phys(virt);
    if (phys == 0) {
        pr_err("cow: no phys at %p", (void*)virt);
        return -1;
    }
    u64 new_page = pmm_alloc();
    if (new_page == 0) {
        pr_err("cow: alloc failed at %p", (void*)virt);
        return -1;
    }
    u8* src = (u8*)phys;
    u8* dst = (u8*)new_page;
    for (u64 i = 0; i < 4096; i++) {
        dst[i] = src[i];
    }
    vmm_unmap(virt);
    if (vmm_map(virt, new_page, flags) != 0) {
        pmm_free(new_page);
        pr_err("cow: remap failed at %p", (void*)virt);
        return -1;
    }
    invlpg(virt);
    return 0;
}

void Harlin_PageFaultHandler(u64 fault_addr, u64 error_code)
{
    int present = (error_code & PF_PRESENT) ? 1 : 0;
    int write   = (error_code & PF_WRITE)   ? 1 : 0;
    int user    = (error_code & PF_USER)    ? 1 : 0;

    if (!present) {
        u64 page = fault_addr & ~0xFFFULL;
        u64 flags = VMM_P_BIT | VMM_RW_BIT;
        if (user) flags |= VMM_USER_BIT;
        if (Harlin_PageFaultDemandInstall(page, flags) == 0) {
            pr_debug("demand map ok at %p (user=%d)", (void*)page, user);
            return;
        }
    }

    if (present && write) {
        u64 page = fault_addr & ~0xFFFULL;
        u64 flags = VMM_P_BIT | VMM_RW_BIT;
        if (user) flags |= VMM_USER_BIT;
        if (Harlin_PageFaultCowResolve(page, flags) == 0) {
            pr_debug("cow resolve ok at %p", (void*)page);
            return;
        }
    }

    pr_emerg("*** UNHANDLED PAGE FAULT ***");
    pr_emerg("addr=%p err=%llx present=%d write=%d user=%d",
             (void*)fault_addr,
             (unsigned long long)error_code,
             present, write, user);
    panic("unhandled page fault");
}
