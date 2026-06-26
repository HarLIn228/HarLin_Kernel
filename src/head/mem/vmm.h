#ifndef VMM_H
#define VMM_H

#include "harlin_API.h"

#define VMM_PRESENT  0x001
#define VMM_WRITABLE 0x002
#define VMM_USER     0x004
#define VMM_HUGE     0x080

#define KERNEL_PML4_VIRT  0xFF8000000000ULL
#define ACPI_TABLE_VIRT   0xFFFF800000200000ULL
#define ACPI_TABLE_END    0xFFFF800000210000ULL
#define KERNEL_STACK_BASE 0xFFFF800001000000ULL
#define KERNEL_STACK_SIZE 0x5000ULL
#define KERNEL_STACK_TOP  0xFFFF800002000000ULL

void vmm_init(u64 pml4_phys);
int  vmm_map(u64 virt, u64 phys, u64 flags);
void vmm_unmap(u64 virt);
u64 vmm_get_phys(u64 virt);
int vmm_mapped(u64 virt);
int vmm_unmap_and_free(u64 virt);

void vmm_switch(u64 pml4_phys);
u64 vmm_current_pml4(void);
u64 vmm_clone_kernel_pml4(void);

extern u64 vmm_last_replaced_phys;

int copy_from_user(void* kdst, u64 usrc, u64 len);
int copy_to_user(u64 udst, const void* ksrc, u64 len);
int strncpy_from_user(char* kdst, u64 usrc, u64 maxlen);

#define Harlin_VmmInit               vmm_init
#define Harlin_VmmMap                vmm_map
#define Harlin_VmmUnmap              vmm_unmap
#define Harlin_VmmGetPhys            vmm_get_phys
#define Harlin_VmmMapped             vmm_mapped
#define Harlin_VmmUnmapAndFree       vmm_unmap_and_free
#define Harlin_VmmSwitch             vmm_switch
#define Harlin_VmmCurrentPml4        vmm_current_pml4
#define Harlin_VmmCloneKernelPml4    vmm_clone_kernel_pml4
#define Harlin_CopyFromUser          copy_from_user
#define Harlin_CopyToUser            copy_to_user
#define Harlin_StrncpyFromUser       strncpy_from_user

#endif
