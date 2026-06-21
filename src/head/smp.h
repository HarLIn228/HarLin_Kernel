#ifndef SMP_H
#define SMP_H

#include "harlin_API.h"

#define SMP_MAX_CPUS 8

struct smp_cpu {
    u32 apic_id;
    u32 cpu_id;
    u64 stack_top;
    int online;
};

void smp_init(void);
int smp_cpu_count(void);
struct smp_cpu* smp_get_cpu(int cpu_id);
int smp_current_cpu_id(void);
void smp_send_init(u32 apic_id);
void smp_send_sipi(u32 apic_id, u8 vector);
void smp_send_ipi(u32 apic_id, u8 vector);
void smp_broadcast_ipi(u8 vector);
void smp_ap_entry(u32 cpu_id);

u64 lapic_read(u32 reg);
void lapic_write(u32 reg, u32 val);

#endif
