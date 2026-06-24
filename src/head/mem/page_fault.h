#ifndef PAGE_FAULT_H
#define PAGE_FAULT_H

#include "harlin_API.h"

#define PF_PRESENT   0x01
#define PF_WRITE     0x02
#define PF_USER      0x04
#define PF_RESERVED  0x08
#define PF_INSTR     0x10

#define PF_ERR_PROTECTION  (1 << 1)

void page_fault_handler(u64 fault_addr, u64 error_code);
int page_fault_demand_mapping_install(u64 virt, u64 flags);
int page_fault_cow_resolve(u64 virt, u64 flags);

#define Harlin_PageFaultHandler         page_fault_handler
#define Harlin_PageFaultDemandInstall   page_fault_demand_mapping_install
#define Harlin_PageFaultCowResolve      page_fault_cow_resolve

#endif
