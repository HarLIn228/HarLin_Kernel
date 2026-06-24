#ifndef CR4_FEATURES_H
#define CR4_FEATURES_H

#include "harlin_API.h"

void cr4_features_enable(void);

int cr4_smep_enabled(void);
int cr4_smap_enabled(void);
int cr4_pcid_enabled(void);

int copy_from_user_safe(void* dst, const void* src, u32 n);
int copy_to_user_safe(void* dst, const void* src, u32 n);

#define Harlin_Cr4FeaturesEnable      cr4_features_enable
#define Harlin_Cr4SmepEnabled         cr4_smep_enabled
#define Harlin_Cr4SmapEnabled         cr4_smap_enabled
#define Harlin_Cr4PcidEnabled         cr4_pcid_enabled
#define Harlin_CopyFromUserSafe       copy_from_user_safe
#define Harlin_CopyToUserSafe         copy_to_user_safe

#endif
