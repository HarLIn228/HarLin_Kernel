#include "smp.h"
#include "io.h"
#include "pmm.h"
#include "vmm.h"
#include "gdt.h"
#include "interrupt.h"
#include "spinlock.h"
#include "percpu.h"
#include "scheduler.h"
#include "harlin_API.h"

#define LAPIC_BASE_MSR 0x1B
#define LAPIC_REG_ID 0x20
#define LAPIC_REG_VERSION 0x30
#define LAPIC_REG_TPR 0x80
#define LAPIC_REG_EOI 0xB0
#define LAPIC_REG_SPURIOUS 0xF0
#define LAPIC_REG_ICR_LOW 0x300
#define LAPIC_REG_ICR_HIGH 0x310

#define SMP_TRAMPOLINE_PHYS 0x8000
#define SMP_TRAMPOLINE_SIZE 224
#define AP_STACK_SIZE 8192

extern char smp_trampoline_data[];
extern unsigned long smp_trampoline_size;
extern void idt_load(struct idt_ptr* ptr);
extern struct idt_ptr idtp;

static volatile u64 lapic_base = 0xFEE00000;
static struct smp_cpu smp_cpus[SMP_MAX_CPUS];
static int smp_cpu_count_value = 1;
static volatile u32 ap_started_count = 0;
static struct spinlock smp_lock;

u64 lapic_read(u32 reg)
{
    volatile u32* addr = (volatile u32*)(lapic_base + reg);
    return *addr;
}

void lapic_write(u32 reg, u32 val)
{
    volatile u32* addr = (volatile u32*)(lapic_base + reg);
    *addr = val;
}

static void lapic_init_base(void)
{
    u32 low, high;
    asm volatile ("rdmsr" : "=a"(low), "=d"(high) : "c"(LAPIC_BASE_MSR));
    lapic_base = (u64)(low & 0xFFFFF000) | ((u64)high << 32);
}

static void smp_install_trampoline(void)
{
    u8* dst = (u8*)SMP_TRAMPOLINE_PHYS;
    u8* src = (u8*)smp_trampoline_data;
    u32 i;
    u32 size = (u32)(u64)smp_trampoline_size;
    if (size > SMP_TRAMPOLINE_SIZE)
        size = SMP_TRAMPOLINE_SIZE;
    for (i = 0; i < size; i++)
        dst[i] = src[i];
}

static void smp_set_trampoline_param(u64 pml4, u64 stack, u64 entry, u32 cpu_id)
{
    u64* p_pml4 = (u64*)(SMP_TRAMPOLINE_PHYS + 0xC0);
    u64* p_stack = (u64*)(SMP_TRAMPOLINE_PHYS + 0xC8);
    u64* p_entry = (u64*)(SMP_TRAMPOLINE_PHYS + 0xD0);
    u32* p_cpu_id = (u32*)(SMP_TRAMPOLINE_PHYS + 0xD8);
    *p_pml4 = pml4;
    *p_stack = stack;
    *p_entry = entry;
    *p_cpu_id = cpu_id;
}

void smp_send_init(u32 apic_id)
{
    lapic_write(LAPIC_REG_ICR_HIGH, apic_id << 24);
    lapic_write(LAPIC_REG_ICR_LOW, 0x00000500);
}

void smp_send_sipi(u32 apic_id, u8 vector)
{
    lapic_write(LAPIC_REG_ICR_HIGH, apic_id << 24);
    lapic_write(LAPIC_REG_ICR_LOW, 0x00000600 | vector);
    lapic_write(LAPIC_REG_ICR_HIGH, apic_id << 24);
    lapic_write(LAPIC_REG_ICR_LOW, 0x00000600 | vector);
}

void smp_send_ipi(u32 apic_id, u8 vector)
{
    lapic_write(LAPIC_REG_ICR_HIGH, apic_id << 24);
    lapic_write(LAPIC_REG_ICR_LOW, 0x00004000 | vector);
}

void smp_broadcast_ipi(u8 vector)
{
    lapic_write(LAPIC_REG_ICR_HIGH, 0);
    lapic_write(LAPIC_REG_ICR_LOW, 0x000C4000 | vector);
}

void smp_ap_entry(u32 cpu_id)
{
    u64 stack;
    if (cpu_id >= SMP_MAX_CPUS)
        cpu_id = 0;
    stack = smp_cpus[cpu_id].stack_top;
    asm volatile ("mov %0, %%rsp" : : "r"(stack));
    gdt_init_for_cpu((int)cpu_id);
    tss_set_rsp0_for_cpu((int)cpu_id, stack);
    idt_load(&idtp);
    lapic_write(LAPIC_REG_TPR, 0);
    lapic_write(LAPIC_REG_SPURIOUS, 0x1FF);
    percpu_set_current((int)cpu_id);
    interrupts_enable();
    spinlock_acquire(&smp_lock);
    ap_started_count++;
    spinlock_release(&smp_lock);
    for (;;) {
        asm volatile ("hlt");
    }
}

static void smp_start_cpu(u32 apic_id, int cpu_id)
{
    u64 phys;
    u64 guard_phys;
    u64 stack_phys;
    u64 pml4;
    u64 guard_virt;
    int j;

    phys = pmm_alloc_contiguous(2);
    if (!phys)
        return;
    guard_phys = phys;
    stack_phys = phys + 4096;
    stack_phys += AP_STACK_SIZE;
    smp_cpus[cpu_id].apic_id = apic_id;
    smp_cpus[cpu_id].cpu_id = cpu_id;
    smp_cpus[cpu_id].stack_top = stack_phys;
    smp_cpus[cpu_id].online = 1;
    pml4 = vmm_get_phys(0x20000);
    smp_set_trampoline_param(pml4, stack_phys, (u64)smp_ap_entry, cpu_id);
    for (j = 0; j < 2; j++) {
        u64 gp = phys + (u64)j * 4096;
        vmm_map(gp, gp, VMM_PRESENT | VMM_WRITABLE);
    }
    guard_virt = guard_phys;
    vmm_unmap(guard_virt);
    (void)phys;
    smp_send_init(apic_id);
    {
        volatile u32 i;
        for (i = 0; i < 10000000; i++) asm volatile ("");
    }
    smp_send_sipi(apic_id, SMP_TRAMPOLINE_PHYS >> 12);
    {
        volatile u32 i;
        for (i = 0; i < 10000000; i++) asm volatile ("");
    }
}

void smp_init(void)
{
    int i;
    u32 bsp_apic_id;
    spinlock_init(&smp_lock);
    for (i = 0; i < SMP_MAX_CPUS; i++) {
        smp_cpus[i].apic_id = 0;
        smp_cpus[i].cpu_id = i;
        smp_cpus[i].stack_top = 0;
        smp_cpus[i].online = 0;
    }
    lapic_init_base();
    bsp_apic_id = (u32)(lapic_read(LAPIC_REG_ID) >> 24);
    smp_cpus[0].apic_id = bsp_apic_id;
    smp_cpus[0].cpu_id = 0;
    smp_cpus[0].online = 1;
    smp_cpu_count_value = 1;
    ap_started_count = 1;
    smp_install_trampoline();
    lapic_write(LAPIC_REG_SPURIOUS, 0x1FF);
    for (i = 1; i < SMP_MAX_CPUS; i++) {
        smp_start_cpu((u32)i, i);
        if (smp_cpus[i].online)
            smp_cpu_count_value++;
    }
}

int smp_cpu_count(void)
{
    return smp_cpu_count_value;
}

struct smp_cpu* smp_get_cpu(int cpu_id)
{
    if (cpu_id < 0 || cpu_id >= SMP_MAX_CPUS)
        return 0;
    return &smp_cpus[cpu_id];
}

int smp_current_cpu_id(void)
{
    u32 apic_id = (u32)(lapic_read(LAPIC_REG_ID) >> 24);
    int i;
    for (i = 0; i < SMP_MAX_CPUS; i++) {
        if (smp_cpus[i].online && smp_cpus[i].apic_id == apic_id)
            return i;
    }
    return 0;
}
