#include "pmm.h"
#include "spinlock.h"

static volatile u8* bitmap = (volatile u8*)PMM_BITMAP_ADDR;
static struct spinlock pmm_lock = { 0 };

static u64 g_pmm_total_pages = PMM_TOTAL_PAGES_MIN;
static u64 g_pmm_top_addr    = PMM_BASE_ADDR + (u64)PMM_TOTAL_PAGES_MIN * PMM_PAGE_SIZE;

extern char __bss_end[];

static u64 pmm_bitmap_bytes(u64 total_pages)
{
    return (total_pages + 7) / 8;
}

static int pmm_check_layout(u64 total_pages)
{
    u64 bitmap_bytes = pmm_bitmap_bytes(total_pages);
    u64 bitmap_end   = PMM_BITMAP_ADDR + bitmap_bytes;
    if (bitmap_end > PMM_BASE_ADDR)
        return -1;
    return 0;
}

void pmm_init(void)
{
    int rc = pmm_init_from_e820(PMM_BASE_ADDR + (u64)PMM_TOTAL_PAGES_MIN * PMM_PAGE_SIZE);
    if (rc != 0) {
        g_pmm_total_pages = PMM_TOTAL_PAGES_MIN;
        g_pmm_top_addr    = PMM_BASE_ADDR + (u64)PMM_TOTAL_PAGES_MIN * PMM_PAGE_SIZE;
    }
}

int pmm_init_from_e820(u64 top_usable_addr)
{
    u64 total_bytes;
    u64 total_pages;
    u32 i;
    u32 kernel_end_page;
    u32 bitmap_start_page;
    u32 bitmap_end_page;

    if (top_usable_addr <= PMM_BASE_ADDR)
        return -1;

    total_bytes = top_usable_addr - PMM_BASE_ADDR;
    total_pages = total_bytes / PMM_PAGE_SIZE;
    if (total_pages > PMM_MAX_PAGES)
        total_pages = PMM_MAX_PAGES;
    if (total_pages < PMM_TOTAL_PAGES_MIN)
        total_pages = PMM_TOTAL_PAGES_MIN;

    if (pmm_check_layout(total_pages) != 0)
        return -1;

    spinlock_init(&pmm_lock);

    g_pmm_total_pages = total_pages;
    g_pmm_top_addr    = PMM_BASE_ADDR + total_pages * PMM_PAGE_SIZE;

    for (i = 0; i < (u32)pmm_bitmap_bytes(total_pages); i++)
        bitmap[i] = 0;

    kernel_end_page = (((u32)(u64)__bss_end) + PMM_PAGE_SIZE - 1) / PMM_PAGE_SIZE;
    bitmap_start_page = (u32)(PMM_BITMAP_ADDR / PMM_PAGE_SIZE);
    bitmap_end_page   = (u32)((PMM_BITMAP_ADDR + pmm_bitmap_bytes(total_pages) + PMM_PAGE_SIZE - 1) / PMM_PAGE_SIZE);

    for (i = (u32)(PMM_BASE_ADDR / PMM_PAGE_SIZE); i < (u32)(PMM_BASE_ADDR / PMM_PAGE_SIZE) + (u32)total_pages; i++) {
        if (i < kernel_end_page || (i >= bitmap_start_page && i < bitmap_end_page)) {
            u32 idx = i - (u32)(PMM_BASE_ADDR / PMM_PAGE_SIZE);
            bitmap[idx / 8] |= (1 << (idx % 8));
        }
    }
    return 0;
}

u64 pmm_total_pages(void)
{
    return g_pmm_total_pages;
}

u64 pmm_total_bytes(void)
{
    return g_pmm_total_pages * PMM_PAGE_SIZE;
}

u64 pmm_top_addr(void)
{
    return g_pmm_top_addr;
}

u64 pmm_alloc(void)
{
    u32 i, j;
    u32 total;
    u64 addr;
    spinlock_acquire(&pmm_lock);
    total = (u32)g_pmm_total_pages;
    for (i = 0; i < total / 8; i++) {
        if (bitmap[i] == 0xFF)
            continue;
        for (j = 0; j < 8; j++) {
            u32 idx = i * 8 + j;
            if (idx >= total) break;
            if ((bitmap[i] & (1 << j)) == 0) {
                bitmap[i] |= (1 << j);
                addr = PMM_BASE_ADDR + (u64)idx * PMM_PAGE_SIZE;
                spinlock_release(&pmm_lock);
                return addr;
            }
        }
    }
    spinlock_release(&pmm_lock);
    return 0;
}

u64 pmm_alloc_contiguous(u32 count)
{
    static u32 last_idx = 0;
    u32 i, j, k, found;
    u32 total;
    u32 start;
    u32 cur;
    u32 full_loops = 0;
    u64 result;

    if (count == 0)
        return 0;

    spinlock_acquire(&pmm_lock);
    total = (u32)g_pmm_total_pages;
    cur = last_idx;
    while (full_loops < 2) {
        for (i = cur / 8; i < total / 8; i++) {
            if (bitmap[i] == 0xFF)
                continue;
            for (j = 0; j < 8; j++) {
                u32 base_idx = i * 8 + j;
                if (base_idx < cur) continue;
                if (base_idx + count > total) continue;
                if (bitmap[i] & (1 << j))
                    continue;
                found = 1;
                for (k = 1; k < count; k++) {
                    u32 idx = base_idx + k;
                    if (bitmap[idx / 8] & (1 << (idx % 8))) {
                        found = 0;
                        break;
                    }
                }
                if (found) {
                    for (k = 0; k < count; k++) {
                        u32 idx = base_idx + k;
                        bitmap[idx / 8] |= (1 << (idx % 8));
                    }
                    start = base_idx;
                    last_idx = (start + count) % total;
                    result = PMM_BASE_ADDR + (u64)start * PMM_PAGE_SIZE;
                    spinlock_release(&pmm_lock);
                    return result;
                }
            }
        }
        cur = 0;
        full_loops++;
    }
    spinlock_release(&pmm_lock);
    return 0;
}

static int pmm_idx_in_range(u32 idx, u64 max_addr)
{
    u64 addr = PMM_BASE_ADDR + (u64)idx * PMM_PAGE_SIZE;
    return addr + PMM_PAGE_SIZE <= max_addr;
}

u64 pmm_alloc_low(u32 count)
{
    u32 i, j, k, found;
    u32 total;
    u64 result;
    if (count == 0)
        return 0;
    spinlock_acquire(&pmm_lock);
    total = (u32)g_pmm_total_pages;
    for (i = 0; i < total / 8; i++) {
        if (bitmap[i] == 0xFF)
            continue;
        for (j = 0; j < 8; j++) {
            u32 base_idx = i * 8 + j;
            if (base_idx + count > total) continue;
            if (bitmap[i] & (1 << j))
                continue;
            if (!pmm_idx_in_range(base_idx, ISA_DMA_LIMIT))
                continue;
            found = 1;
            for (k = 1; k < count; k++) {
                u32 idx = base_idx + k;
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
                    u32 idx = base_idx + k;
                    bitmap[idx / 8] |= (1 << (idx % 8));
                }
                result = PMM_BASE_ADDR + (u64)base_idx * PMM_PAGE_SIZE;
                spinlock_release(&pmm_lock);
                return result;
            }
        }
    }
    spinlock_release(&pmm_lock);
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
    if (idx >= g_pmm_total_pages)
        return;
    spinlock_acquire(&pmm_lock);
    bitmap[idx / 8] &= ~(1 << (idx % 8));
    spinlock_release(&pmm_lock);
}
