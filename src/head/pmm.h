#ifndef PMM_H
#define PMM_H

#include "harlin_API.h"

#define PMM_PAGE_SIZE 4096
#define PMM_BASE_ADDR 0x300000
#define PMM_BITMAP_ADDR 0x200000
#define PMM_TOTAL_PAGES 32000

void pmm_init(void);
u64 pmm_alloc(void);
u64 pmm_alloc_contiguous(u32 count);
u64 pmm_alloc_low(u32 count);
u64 pmm_alloc_contiguous_low(u32 count);
void pmm_free(u64 addr);

#define ISA_DMA_LIMIT 0x01000000U

#endif
