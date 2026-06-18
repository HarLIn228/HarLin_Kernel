#ifndef VMM_H
#define VMM_H

#include "harlin_API.h"

#define VMM_PRESENT  0x001
#define VMM_WRITABLE 0x002
#define VMM_USER     0x004
#define VMM_HUGE     0x080

void vmm_init(u64 pml4_phys);
void vmm_map(u64 virt, u64 phys, u64 flags);
void vmm_unmap(u64 virt);
u64 vmm_get_phys(u64 virt);

#endif
