#include "gdt.h"
#include "harlin_API.h"
#include "smp.h"

#define MAX_GDT_CPUS 8

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

static u64 gdt[MAX_GDT_CPUS][9];
static struct gdt_ptr gdtr[MAX_GDT_CPUS];
static struct tss tss[MAX_GDT_CPUS];
static int gdt_initialized[MAX_GDT_CPUS];

extern void gdt_flush(struct gdt_ptr* ptr);
extern void tss_flush(u16 selector);

void tss_set_rsp0_for_cpu(int cpu, u64 rsp)
{
    if (cpu < 0 || cpu >= MAX_GDT_CPUS)
        cpu = 0;
    tss[cpu].rsp0 = rsp;
}

void tss_set_rsp0(u64 rsp)
{
    int cpu = 0;
    if (smp_cpu_count() > 1)
        cpu = smp_current_cpu_id();
    tss_set_rsp0_for_cpu(cpu, rsp);
}

void gdt_init_for_cpu(int cpu)
{
    u64 tss_base;

    if (cpu < 0 || cpu >= MAX_GDT_CPUS)
        cpu = 0;

    Harlin_Fill((void*)gdt[cpu], 0, sizeof(gdt[cpu]));
    Harlin_Fill((void*)&tss[cpu], 0, sizeof(tss[cpu]));

    gdt[cpu][0] = 0x0000000000000000;
    gdt[cpu][1] = 0x00CF9A000000FFFF;
    gdt[cpu][2] = 0x00CF92000000FFFF;
    gdt[cpu][3] = 0x00AF9A000000FFFF;
    gdt[cpu][4] = 0x0000920000000000;
    gdt[cpu][5] = 0x00AFFA000000FFFF;
    gdt[cpu][6] = 0x00AFF2000000FFFF;

    tss_base = (u64)&tss[cpu];
    gdt[cpu][7] = 0x0000890000000000 | (sizeof(tss[cpu]) - 1) | ((tss_base & 0xFFFFFF) << 16);
    gdt[cpu][7] |= ((tss_base >> 24) & 0xFF) << 56;
    gdt[cpu][8] = (tss_base >> 32) & 0xFFFFFFFF;

    gdtr[cpu].limit = sizeof(gdt[cpu]) - 1;
    gdtr[cpu].base = (u64)gdt[cpu];

    gdt_flush(&gdtr[cpu]);
    tss_flush(GDT_TSS);

    gdt_initialized[cpu] = 1;
}

void gdt_init(void)
{
    int cpu = 0;
    if (smp_cpu_count() > 1)
        cpu = smp_current_cpu_id();
    gdt_init_for_cpu(cpu);
}
