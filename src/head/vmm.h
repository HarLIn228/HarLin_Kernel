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
int vmm_mapped(u64 virt);
int vmm_unmap_and_free(u64 virt);

extern u64 vmm_last_replaced_phys;

int copy_from_user(void* kdst, u64 usrc, u64 len);
int copy_to_user(u64 udst, const void* ksrc, u64 len);
int strncpy_from_user(char* kdst, u64 usrc, u64 maxlen);

#endif
