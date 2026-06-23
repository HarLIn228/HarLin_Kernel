#ifndef FEATURE_PROBE_H
#define FEATURE_PROBE_H

#include "harlin_API.h"

struct cpu_features {
    u32 max_basic_leaf;
    u32 max_ext_leaf;
    u32 vendor_id[3];
    u32 family;
    u32 model;
    u32 stepping;

    u32 has_smep;        // CR4.SMEP support
    u32 has_smap;        // CR4.SMAP support
    u32 has_nx;          // EFER.NX support
    u32 has_pcid;        // CR4.PCID support
    u32 has_fsgsbase;    // CR4.FSGSBASE support
    u32 has_rdrand;      // RDRAND instruction
    u32 has_rdseed;      // RDSEED instruction
    u32 has_tsc;         // TSC
    u32 has_tsc_invariant;
    u32 has_x2apic;
    u32 has_apic;
    u32 has_msr;

    u32 hpet_base;       // physical base of HPET, 0 if absent
    u32 hpet_present;
    u32 tsc_hz;          // estimated TSC frequency
};

void feature_probe_init(void);
const struct cpu_features* feature_get(void);
int feature_has_hpet(void);
u64 feature_rdrand(void);
u64 feature_tsc(void);

#define Harlin_FeatureProbeInit       feature_probe_init
#define Harlin_FeatureGet             feature_get
#define Harlin_FeatureHasHpet         feature_has_hpet
#define Harlin_FeatureRdrand          feature_rdrand
#define Harlin_FeatureTsc             feature_tsc

#endif
