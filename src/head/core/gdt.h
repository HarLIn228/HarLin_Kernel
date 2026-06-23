#ifndef GDT_H
#define GDT_H

#include "harlin_API.h"

#define GDT_KERNEL_CODE 0x18
#define GDT_KERNEL_DATA 0x20
#define GDT_USER_CODE   0x2B
#define GDT_USER_DATA   0x33
#define GDT_TSS         0x38

void gdt_init(void);
void gdt_init_for_cpu(int cpu);
void tss_set_rsp0(u64 rsp);
void tss_set_rsp0_for_cpu(int cpu, u64 rsp);

#endif
