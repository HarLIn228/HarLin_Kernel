#include "cr4_features.h"
#include "feature_probe.h"
#include "harlin_API.h"

static int g_smep_on = 0;
static int g_smap_on = 0;
static int g_pcid_on = 0;

static inline u64 read_cr4(void)
{
    u64 v;
    asm volatile ("mov %%cr4, %0" : "=r"(v));
    return v;
}

static inline void write_cr4(u64 v)
{
    asm volatile ("mov %0, %%cr4" : : "r"(v) : "memory");
}

void cr4_features_enable(void)
{
    const struct cpu_features* f = feature_get();
    u64 cr4 = read_cr4();

    if (f->has_pcid) {
        cr4 |= (1ULL << 17);
        g_pcid_on = 1;
    }
    if (f->has_smep) {
        cr4 |= (1ULL << 20);
        g_smep_on = 1;
    }
    if (f->has_smap) {
        cr4 |= (1ULL << 21);
        g_smap_on = 1;
    }
    write_cr4(cr4);
}

int cr4_smep_enabled(void) { return g_smep_on; }
int cr4_smap_enabled(void) { return g_smap_on; }
int cr4_pcid_enabled(void) { return g_pcid_on; }

int copy_from_user_safe(void* dst, const void* src, u32 n)
{
    if (!dst || !src) return -1;
    if (n == 0) return 0;
    if (!Harlin_UserPtrValid((u64)src, n)) return -1;
    Harlin_Copy(dst, src, n);
    return 0;
}

int copy_to_user_safe(void* dst, const void* src, u32 n)
{
    if (!dst || !src) return -1;
    if (n == 0) return 0;
    if (!Harlin_UserPtrValid((u64)dst, n)) return -1;
    Harlin_Copy(dst, src, n);
    return 0;
}
