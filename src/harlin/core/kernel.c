#include "harlin_API.h"

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
    Harlin_Boot();
}
