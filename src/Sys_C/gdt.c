#include "gdt.h"
#include "harlin_API.h"

struct gdt_ptr {
    u16 limit;
    u64 base;
} __attribute__((packed));

struct tss {
    u32 reserved0;
    u64 rsp0;
    u64 rsp1;
    u64 rsp2;
    u64 reserved1;
    u64 ist1;
    u64 ist2;
    u64 ist3;
    u64 ist4;
    u64 ist5;
    u64 ist6;
    u64 ist7;
    u64 reserved2;
    u16 reserved3;
    u16 iomap_base;
} __attribute__((packed));

static u64 gdt[9];
static struct gdt_ptr gdtr;
static struct tss tss;

extern void gdt_flush(struct gdt_ptr* ptr);
extern void tss_flush(u16 selector);

void tss_set_rsp0(u64 rsp)
{
    tss.rsp0 = rsp;
}

void gdt_init(void)
{
    u64 tss_base;

    Harlin_MemSet((void*)gdt, 0, sizeof(gdt));
    Harlin_MemSet((void*)&tss, 0, sizeof(tss));

    gdt[0] = 0x0000000000000000;
    gdt[1] = 0x00CF9A000000FFFF;
    gdt[2] = 0x00CF92000000FFFF;
    gdt[3] = 0x00AF9A000000FFFF;
    gdt[4] = 0x0000920000000000;
    gdt[5] = 0x008FFA000000FFFF;
    gdt[6] = 0x008FF2000000FFFF;

    tss_base = (u64)&tss;
    gdt[7] = 0x0000890000000000 | (sizeof(tss) - 1) | ((tss_base & 0xFFFFFF) << 16);
    gdt[7] |= ((tss_base >> 24) & 0xFF) << 56;
    gdt[8] = (tss_base >> 32) & 0xFFFFFFFF;

    gdtr.limit = sizeof(gdt) - 1;
    gdtr.base = (u64)gdt;

    gdt_flush(&gdtr);
    tss_flush(GDT_TSS);
}
