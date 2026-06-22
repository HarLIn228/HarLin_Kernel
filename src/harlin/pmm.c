#include "pmm.h"

static volatile u8* bitmap = (volatile u8*)PMM_BITMAP_ADDR;

extern char __bss_end[];

void pmm_init(void)
{
    u32 i;
    u32 kernel_end_page;
    u32 bitmap_start_page;
    u32 bitmap_end_page;

    for (i = 0; i < PMM_TOTAL_PAGES / 8; i++)
        bitmap[i] = 0;

    kernel_end_page = (((u32)(u64)__bss_end) + PMM_PAGE_SIZE - 1) / PMM_PAGE_SIZE;
    bitmap_start_page = PMM_BITMAP_ADDR / PMM_PAGE_SIZE;
    bitmap_end_page = (PMM_BITMAP_ADDR + (PMM_TOTAL_PAGES / 8) + PMM_PAGE_SIZE - 1) / PMM_PAGE_SIZE;

    for (i = PMM_BASE_ADDR / PMM_PAGE_SIZE; i < (PMM_BASE_ADDR / PMM_PAGE_SIZE) + PMM_TOTAL_PAGES; i++) {
        if (i < kernel_end_page || (i >= bitmap_start_page && i < bitmap_end_page)) {
            u32 idx = i - (PMM_BASE_ADDR / PMM_PAGE_SIZE);
            bitmap[idx / 8] |= (1 << (idx % 8));
        }
    }
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

u64 pmm_alloc_contiguous(u32 count)
{
    static u32 last_idx = 0;
    u32 i, j, k, found;
    u32 total_bits;
    u32 start;
    u32 full_loops = 0;
    u32 cur;

    if (count == 0)
        return 0;

    total_bits = PMM_TOTAL_PAGES;
    cur = last_idx;
    while (full_loops < 2) {
        for (i = cur / 8; i < total_bits / 8; i++) {
            if (bitmap[i] == 0xFF)
                continue;
            for (j = 0; j < 8; j++) {
                if (i * 8 + j < cur)
                    continue;
                if (bitmap[i] & (1 << j))
                    continue;
                found = 1;
                for (k = 1; k < count; k++) {
                    u32 idx = (i * 8 + j + k);
                    if (idx >= total_bits) {
                        found = 0;
                        break;
                    }
                    if (bitmap[idx / 8] & (1 << (idx % 8))) {
                        found = 0;
                        break;
                    }
                }
                if (found) {
                    for (k = 0; k < count; k++) {
                        u32 idx = (i * 8 + j + k);
                        bitmap[idx / 8] |= (1 << (idx % 8));
                    }
                    start = i * 8 + j;
                    last_idx = (start + count) % total_bits;
                    return PMM_BASE_ADDR + (start * PMM_PAGE_SIZE);
                }
            }
        }
        cur = 0;
        full_loops++;
    }
    return 0;
}

static int pmm_idx_in_range(u32 idx, u64 max_addr)
{
    u64 addr = (u64)PMM_BASE_ADDR + (u64)idx * PMM_PAGE_SIZE;
    return addr + PMM_PAGE_SIZE <= max_addr;
}

u64 pmm_alloc_low(u32 count)
{
    u32 i, j, k, found;
    u32 total_bits;
    if (count == 0)
        return 0;
    total_bits = PMM_TOTAL_PAGES;
    for (i = 0; i < total_bits / 8; i++) {
        if (bitmap[i] == 0xFF)
            continue;
        for (j = 0; j < 8; j++) {
            if (bitmap[i] & (1 << j))
                continue;
            if (!pmm_idx_in_range((u32)(i * 8 + j), ISA_DMA_LIMIT))
                continue;
            found = 1;
            for (k = 1; k < count; k++) {
                u32 idx = (i * 8 + j + k);
                if (idx >= total_bits) {
                    found = 0;
                    break;
                }
                if (bitmap[idx / 8] & (1 << (idx % 8))) {
                    found = 0;
                    break;
                }
                if (!pmm_idx_in_range(idx, ISA_DMA_LIMIT)) {
                    found = 0;
                    break;
                }
            }
            if (found) {
                for (k = 0; k < count; k++) {
                    u32 idx = (i * 8 + j + k);
                    bitmap[idx / 8] |= (1 << (idx % 8));
                }
                return PMM_BASE_ADDR + (i * 8 + j) * PMM_PAGE_SIZE;
            }
        }
    }
    return 0;
}

u64 pmm_alloc_contiguous_low(u32 count)
{
    return pmm_alloc_low(count);
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
