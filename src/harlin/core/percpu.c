#include "percpu.h"
#include "pmm.h"

#define PERCPU_AREA_SIZE 4096

static u64 percpu_bases[PERCPU_MAX];
static int percpu_current_cpu = 0;

void percpu_init_area(int cpu_id, u64 base)
{
    if (cpu_id < 0 || cpu_id >= PERCPU_MAX)
        return;
    percpu_bases[cpu_id] = base;
}

u64 percpu_get_offset(int cpu_id)
{
    if (cpu_id < 0 || cpu_id >= PERCPU_MAX)
        return 0;
    return percpu_bases[cpu_id];
}

struct percpu_area* percpu_current(void)
{
    return (struct percpu_area*)percpu_bases[percpu_current_cpu];
}

void percpu_set_current(int cpu_id)
{
    if (cpu_id >= 0 && cpu_id < PERCPU_MAX)
        percpu_current_cpu = cpu_id;
}

void percpu_alloc_areas(int cpu_count)
{
    int i;
    for (i = 0; i < cpu_count && i < PERCPU_MAX; i++) {
        u64 phys = pmm_alloc();
        if (!phys)
            continue;
        percpu_bases[i] = phys;
        {
            u64* p = (u64*)phys;
            int j;
            for (j = 0; j < PERCPU_AREA_SIZE / 8; j++)
                p[j] = 0;
        }
    }
}
