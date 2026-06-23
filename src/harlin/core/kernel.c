#include "harlin_API.h"

extern char __bss_start[];
extern char __bss_end[];

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
    clear_bss();
    debugcon_puts("[KMAIN] bss cleared\n");
    Harlin_Boot();
}
