#include "harlin_API.h"
#include "printk.h"
#include "bug.h"
#include "vmm.h"
#include "pmm.h"
#include "page_fault.h"

#define SELFTEST_VIRT_BASE 0xFFFF800010000000ULL
#define SELFTEST_VIRT_STRIDE 0x1000ULL
#define SELFTEST_VIRT_COUNT 8

static u64 selftest_va_array[SELFTEST_VIRT_COUNT];

static void selftest_pf_demand(void)
{
    u64 phys;
    u8* p;
    u64 i;
    u64 va;

    pr_info("selftest: page_fault demand mapping start");
    for (i = 0; i < SELFTEST_VIRT_COUNT; i++) {
        va = SELFTEST_VIRT_BASE + i * SELFTEST_VIRT_STRIDE;
        selftest_va_array[i] = va;
        phys = pmm_alloc();
        ASSERT(phys != 0);
        p = (u8*)phys;
        p[0] = 0x5A;
        p[1] = 0xA5;
        p[4095] = (u8)i;
        ASSERT(vmm_map(va, phys, 0x03) == 0);
    }
    for (i = 0; i < SELFTEST_VIRT_COUNT; i++) {
        va = selftest_va_array[i];
        u8* q = (u8*)va;
        ASSERT(q[0] == 0x5A);
        ASSERT(q[1] == 0xA5);
        ASSERT(q[4095] == (u8)i);
    }
    pr_info("selftest: page_fault demand mapping OK (%llu pages)",
            (unsigned long long)SELFTEST_VIRT_COUNT);
}

static void selftest_pf_cow(void)
{
    u64 va0 = SELFTEST_VIRT_BASE - SELFTEST_VIRT_STRIDE;
    u64 phys0 = pmm_alloc();
    u8* p0 = (u8*)phys0;
    u64 phys_after;
    u8* p_after;

    pr_info("selftest: COW simulate start");
    ASSERT(phys0 != 0);
    for (u64 i = 0; i < 4096; i++) p0[i] = (u8)(i & 0xFF);
    ASSERT(vmm_map(va0, phys0, 0x01) == 0);

    vmm_unmap(va0);
    ASSERT(vmm_mapped(va0) == 0);

    ASSERT(Harlin_PageFaultDemandInstall(va0, 0x01) == 0);
    p_after = (u8*)va0;
    p_after[0] = 0xCC;

    ASSERT(Harlin_PageFaultCowResolve(va0, 0x03) == 0);
    phys_after = vmm_get_phys(va0);
    ASSERT(phys_after != 0);
    ASSERT(phys_after != phys0);
    p_after = (u8*)va0;
    p_after[0] = 0xDD;
    ASSERT(p_after[0] == 0xDD);
    ASSERT(((u8*)phys0)[0] == 0xCC);
    pr_info("selftest: COW simulate OK (orig phys=%p new phys=%p)",
            (void*)phys0, (void*)phys_after);
}

static void selftest_pf(void)
{
    selftest_pf_demand();
    selftest_pf_cow();
}

void Harlin_SelftestPf(void)
{
    selftest_pf();
}

extern char __bss_start[];
extern char __bss_end[];

#define EFI_BOOT_INFO_MAGIC 0x484C4E45ULL

typedef struct {
    unsigned long long Magic;
    unsigned long long FrameBufferBase;
    unsigned long long FrameBufferSize;
    unsigned long long HoriResolution;
    unsigned long long VertResolution;
    unsigned long long PixelsPerScanLine;
    unsigned long long PixelFormat;
    unsigned long long RsdpAddress;
    unsigned long long KernelEntryPhys;
} __attribute__((packed)) EFI_BOOT_INFO;

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

static unsigned long long read_be64(const unsigned char* p)
{
    return ((unsigned long long)p[0] << 56) |
           ((unsigned long long)p[1] << 48) |
           ((unsigned long long)p[2] << 40) |
           ((unsigned long long)p[3] << 32) |
           ((unsigned long long)p[4] << 24) |
           ((unsigned long long)p[5] << 16) |
           ((unsigned long long)p[6] << 8) |
           ((unsigned long long)p[7]);
}

static int detect_efi_boot(void)
{
    volatile const unsigned char* p = (volatile const unsigned char*)0x8000;
    unsigned long long magic = read_be64((const unsigned char*)p);
    if (magic == EFI_BOOT_INFO_MAGIC) {
        return 1;
    }
    return 0;
}

static void clear_bss(void)
{
    volatile char* p = __bss_start;
    volatile char* end = __bss_end;
    while (p < end) {
        *p = 0;
        p++;
    }
}

void __attribute__((section(".text.kernel_main"))) kernel_main(void)
{
    debugcon_puts("[KMAIN] start\n");
    if (detect_efi_boot()) {
        debugcon_puts("[KMAIN] efi boot\n");
    } else {
        debugcon_puts("[KMAIN] bios boot\n");
    }
    clear_bss();
    debugcon_puts("[KMAIN] bss cleared\n");
    pr_info("HarLin Kernel v1.7.4+ boot, bss cleared");
    ASSERT(__bss_start <= __bss_end);
    Harlin_Boot();
}
