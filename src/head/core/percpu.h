#ifndef PERCPU_H
#define PERCPU_H

#include "harlin_API.h"

#define PERCPU_MAX 8

struct percpu_area {
    u64 scratch[16];
    u64 current_pid;
    u64 cpu_id;
    u64 flags;
};

void percpu_init_area(int cpu_id, u64 base);
u64 percpu_get_offset(int cpu_id);
struct percpu_area* percpu_current(void);
void percpu_set_current(int cpu_id);

#define Harlin_PercpuInitArea        percpu_init_area
#define Harlin_PercpuGetOffset       percpu_get_offset
#define Harlin_PercpuCurrent         percpu_current
#define Harlin_PercpuSetCurrent      percpu_set_current

#define PERCPU_VAR(name) (__percpu_##name)

#endif
