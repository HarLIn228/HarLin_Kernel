#include "pmm.h"

static volatile u8* bitmap = (volatile u8*)PMM_BITMAP_ADDR;

void pmm_init(void)
{
    u32 i;
    for (i = 0; i < PMM_TOTAL_PAGES / 8; i++)
        bitmap[i] = 0;
}

u64 pmm_alloc(void)
{
    u32 i, j;
    for (i = 0; i < PMM_TOTAL_PAGES / 8; i++) {
        if (bitmap[i] == 0xFF)
            continue;
        for (j = 0; j < 8; j++) {
            if ((bitmap[i] & (1 << j)) == 0) {
                bitmap[i] |= (1 << j);
                return PMM_BASE_ADDR + ((i * 8 + j) * PMM_PAGE_SIZE);
            }
        }
    }
    return 0;
}

void pmm_free(u64 addr)
{
    u32 idx;
    if (addr < PMM_BASE_ADDR)
        return;
    idx = (u32)((addr - PMM_BASE_ADDR) / PMM_PAGE_SIZE);
    if (idx >= PMM_TOTAL_PAGES)
        return;
    bitmap[idx / 8] &= ~(1 << (idx % 8));
}
