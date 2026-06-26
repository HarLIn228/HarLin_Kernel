#ifndef PAGE_FAULT_H
#define PAGE_FAULT_H

#include "harlin_API.h"

#define PF_PRESENT   0x01
#define PF_WRITE     0x02
#define PF_USER      0x04
#define PF_RESERVED  0x08
#define PF_INSTR     0x10

#define PF_ERR_PROTECTION  (1 << 1)

void Harlin_PageFaultHandler(u64 fault_addr, u64 error_code);
int  Harlin_PageFaultDemandInstall(u64 virt, u64 flags);
int  Harlin_PageFaultCowResolve(u64 virt, u64 flags);

#endif
