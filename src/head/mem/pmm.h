#ifndef PMM_H
#define PMM_H

#include "harlin_API.h"

#define PMM_PAGE_SIZE         4096
#define PMM_PAGE_SHIFT        12
#define PMM_BASE_ADDR         0x300000ULL
#define PMM_BITMAP_ADDR       0x200000ULL

#define PMM_TOTAL_PAGES_MIN   32000U
#define PMM_MAX_PAGES         0x100000U

void pmm_init(void);
int  pmm_init_from_e820(u64 top_usable_addr);
u64  pmm_total_pages(void);
u64  pmm_total_bytes(void);
u64  pmm_top_addr(void);
u64  pmm_alloc(void);
u64  pmm_alloc_contiguous(u32 count);
u64  pmm_alloc_low(u32 count);
u64  pmm_alloc_contiguous_low(u32 count);
void pmm_free(u64 addr);

#define ISA_DMA_LIMIT 0x01000000U

#define Harlin_PmmInit               pmm_init
#define Harlin_PmmInitFromE820       pmm_init_from_e820
#define Harlin_PmmTotalPages         pmm_total_pages
#define Harlin_PmmTotalBytes         pmm_total_bytes
#define Harlin_PmmTopAddr            pmm_top_addr
#define Harlin_PmmAlloc              pmm_alloc
#define Harlin_PmmAllocContiguous    pmm_alloc_contiguous
#define Harlin_PmmAllocLow           pmm_alloc_low
#define Harlin_PmmAllocContiguousLow pmm_alloc_contiguous_low
#define Harlin_PmmFree               pmm_free

#endif
