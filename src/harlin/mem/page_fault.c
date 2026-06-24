#include "page_fault.h"
#include "vmm.h"
#include "pmm.h"

#define VMM_USER_BIT  0x04
#define VMM_RW_BIT    0x02
#define VMM_P_BIT     0x01

static inline void debugcon_putc(char c)
{
    asm volatile ("outb %0, %1" : : "a"(c), "Nd"((unsigned short)0x0402));
}

static void debugcon_puts(const char* s)
{
    while (*s) {
        debugcon_putc(*s++);
    }
}

static void debugcon_put_hex64(u64 v)
{
    char hex[] = "0123456789ABCDEF";
    int i;
    for (i = 15; i >= 0; i--) {
        debugcon_putc(hex[(v >> (i * 4)) & 0xF]);
    }
}

static inline void invlpg(u64 addr)
{
    asm volatile ("invlpg (%0)" : : "r"(addr) : "memory");
}

int page_fault_demand_mapping_install(u64 virt, u64 flags)
{
    u64 page = pmm_alloc();
    if (page == 0) {
        return -1;
    }
    u8* p = (u8*)page;
    for (u64 i = 0; i < 4096; i++) {
        p[i] = 0;
    }
    if (vmm_map(virt, page, flags) != 0) {
        pmm_free(page);
        return -1;
    }
    invlpg(virt);
    return 0;
}

int page_fault_cow_resolve(u64 virt, u64 flags)
{
    u64 phys = vmm_get_phys(virt);
    if (phys == 0) {
        return -1;
    }
    u64 new_page = pmm_alloc();
    if (new_page == 0) {
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
        return -1;
    }
    invlpg(virt);
    return 0;
}

void page_fault_handler(u64 fault_addr, u64 error_code)
{
    int present = (error_code & PF_PRESENT) ? 1 : 0;
    int write = (error_code & PF_WRITE) ? 1 : 0;
    int user = (error_code & PF_USER) ? 1 : 0;

    if (!present && !write) {
        u64 page = fault_addr & ~0xFFFULL;
        u64 flags = VMM_P_BIT | VMM_RW_BIT;
        if (user) flags |= VMM_USER_BIT;
        if (page_fault_demand_mapping_install(page, flags) == 0) {
            return;
        }
    }

    if (present && write) {
        u64 page = fault_addr & ~0xFFFULL;
        u64 flags = VMM_P_BIT | VMM_RW_BIT;
        if (user) flags |= VMM_USER_BIT;
        if (page_fault_cow_resolve(page, flags) == 0) {
            return;
        }
    }

    debugcon_puts("[PF] addr=");
    debugcon_put_hex64(fault_addr);
    debugcon_puts(" err=");
    debugcon_put_hex64(error_code);
    debugcon_puts(" halt\n");

    asm volatile ("cli; hlt");
}
