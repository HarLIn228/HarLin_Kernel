#include "feature_probe.h"
#include "io.h"
#include "harlin_API.h"

static struct cpu_features g_feat;
static int g_inited = 0;

static inline void cpuid(u32 leaf, u32* a, u32* b, u32* c, u32* d)
{
    asm volatile ("cpuid" : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d) : "a"(leaf), "c"(0));
}

static inline u64 rdtsc(void)
{
    u32 lo, hi;
    asm volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((u64)hi << 32) | lo;
}

static inline u64 rdmsr(u32 msr)
{
    u32 lo, hi;
    asm volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((u64)hi << 32) | lo;
}

static u32 probe_hpet(void)
{
    u32 base;
    u32 lo, hi;
    base = *(volatile u32*)((u8*)0xFED00000 + 0x00);
    if ((base & 0xFFFFFFFFu) == 0) {
        return 0;
    }
    lo = *(volatile u32*)((u8*)0xFED00000 + 0x10);
    hi = *(volatile u32*)((u8*)0xFED00000 + 0x14);
    if (lo == 0 || hi == 0) {
        return 0;
    }
    return 0xFED00000u;
}

static u32 estimate_tsc_hz(void)
{
    u64 t0, t1;
    u32 pit0, pit1;
    u32 acc = 0;
    int i;
    outb(0x43, 0xB0);
    outb(0x42, 0xFF);
    outb(0x42, 0xFF);
    t0 = rdtsc();
    for (i = 0; i < 1000000; i++) {
        inb(0x60);
    }
    t1 = rdtsc();
    pit0 = inb(0x42);
    pit1 = inb(0x42);
    acc = ((pit1 << 8) | pit0);
    if (acc == 0) {
        return 0;
    }
    return (u32)(((t1 - t0) * 1193182u) / acc);
}

void feature_probe_init(void)
{
    u32 a, b, c, d;
    if (g_inited) return;

    g_feat.max_basic_leaf = 0;
    g_feat.max_ext_leaf = 0;
    g_feat.vendor_id[0] = 0;
    g_feat.vendor_id[1] = 0;
    g_feat.vendor_id[2] = 0;
    g_feat.family = 0;
    g_feat.model = 0;
    g_feat.stepping = 0;

    g_feat.has_smep = 0;
    g_feat.has_smap = 0;
    g_feat.has_nx = 0;
    g_feat.has_pcid = 0;
    g_feat.has_fsgsbase = 0;
    g_feat.has_rdrand = 0;
    g_feat.has_rdseed = 0;
    g_feat.has_tsc = 0;
    g_feat.has_tsc_invariant = 0;
    g_feat.has_x2apic = 0;
    g_feat.has_apic = 0;
    g_feat.has_msr = 0;

    g_feat.hpet_base = 0;
    g_feat.hpet_present = 0;
    g_feat.tsc_hz = 0;

    cpuid(0, &a, &b, &c, &d);
    g_feat.max_basic_leaf = a;
    g_feat.vendor_id[0] = b;
    g_feat.vendor_id[1] = d;
    g_feat.vendor_id[2] = c;

    if (g_feat.max_basic_leaf >= 1) {
        cpuid(1, &a, &b, &c, &d);
        g_feat.family   = (a >> 8) & 0x0F;
        g_feat.model    = (a >> 4) & 0x0F;
        g_feat.stepping = a & 0x0F;
        g_feat.has_msr   = (d >> 5) & 1;
        g_feat.has_apic  = (d >> 9) & 1;
        g_feat.has_tsc   = (d >> 4) & 1;
        g_feat.has_pcid  = (c >> 17) & 1;
        g_feat.has_x2apic = (c >> 21) & 1;
        g_feat.has_smep  = (c >> 7)  & 1;
        g_feat.has_rdrand = (c >> 30) & 1;
        g_feat.has_fsgsbase = (c >> 0) & 1;
    }

    if (g_feat.max_basic_leaf >= 7) {
        cpuid(7, &a, &b, &c, &d);
        g_feat.has_smap    = (b >> 20) & 1;
        g_feat.has_rdseed  = (b >> 18) & 1;
    }

    cpuid(0x80000000u, &a, &b, &c, &d);
    g_feat.max_ext_leaf = a;
    if (g_feat.max_ext_leaf >= 0x80000001u) {
        cpuid(0x80000001u, &a, &b, &c, &d);
        g_feat.has_nx = (d >> 20) & 1;
    }
    if (g_feat.max_ext_leaf >= 0x80000007u) {
        cpuid(0x80000007u, &a, &b, &c, &d);
        g_feat.has_tsc_invariant = (c >> 8) & 1;
    }

    g_feat.hpet_base = probe_hpet();
    g_feat.hpet_present = g_feat.hpet_base ? 1 : 0;

    g_feat.tsc_hz = estimate_tsc_hz();

    g_inited = 1;
}

const struct cpu_features* feature_get(void)
{
    if (!g_inited) feature_probe_init();
    return &g_feat;
}

int feature_has_hpet(void)
{
    if (!g_inited) feature_probe_init();
    return g_feat.hpet_present;
}

u64 feature_tsc(void)
{
    return rdtsc();
}

u64 feature_rdrand(void)
{
    u32 retry = 10;
    while (retry--) {
        u32 lo, hi;
        u8 ok;
        asm volatile (".byte 0x0F, 0xC7, 0xF0" : "=a"(lo), "=d"(hi), "=@ccc"(ok) : : "cc");
        if (ok) return ((u64)hi << 32) | lo;
    }
    return rdtsc();
}
