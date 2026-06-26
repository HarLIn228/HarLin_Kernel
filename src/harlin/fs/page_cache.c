#include "page_cache.h"
#include "pmm.h"
#include "vmm.h"
#include "printk.h"
#include "bug.h"

#define RC_VA_BASE  0xFFFF800040000000ULL

struct rc_slot {
    u32 key;
    u8  used;
    u8  dirty;
    u64 phys;
    u64 touch;
};

static struct rc_slot slots[RC_BUCKETS];
static u64 tick_counter;
static u64 hit_count;
static u64 miss_count;
static u64 va_cursor = RC_VA_BASE;

static u64 pick_victim(void)
{
    u64 best = (u64)-1;
    u64 idx = 0;
    for (u64 i = 0; i < RC_BUCKETS; i++) {
        if (!slots[i].used) return i;
        if (slots[i].touch < best) {
            best = slots[i].touch;
            idx = i;
        }
    }
    return idx;
}

int Harlin_ReadCacheInit(void)
{
    for (u64 i = 0; i < RC_BUCKETS; i++) {
        slots[i].key = (u32)-1;
        slots[i].used = 0;
        slots[i].dirty = 0;
        slots[i].phys = 0;
        slots[i].touch = 0;
    }
    tick_counter = 0;
    hit_count = 0;
    miss_count = 0;
    va_cursor = RC_VA_BASE;
    pr_info("read_cache: %d slots ready", RC_BUCKETS);
    return 0;
}

int Harlin_ReadCacheGet(u32 key, void** out_ptr)
{
    tick_counter++;
    for (u64 i = 0; i < RC_BUCKETS; i++) {
        if (slots[i].used && slots[i].key == key) {
            slots[i].touch = tick_counter;
            hit_count++;
            *out_ptr = (void*)slots[i].phys;
            return 0;
        }
    }
    miss_count++;
    return -1;
}

void Harlin_ReadCachePut(u32 key, const void* data)
{
    u64 victim = pick_victim();
    u64 phys = pmm_alloc();
    if (!phys) {
        pr_err("read_cache: pmm_alloc failed");
        return;
    }
    u8* dst = (u8*)phys;
    const u8* src = (const u8*)data;
    for (u64 i = 0; i < 4096; i++) dst[i] = src[i];

    if (slots[victim].used) {
        if (slots[victim].phys) pmm_free(slots[victim].phys);
    }

    u64 va = va_cursor;
    va_cursor += 4096;
    if (va_cursor >= RC_VA_BASE + 0x400000ULL) va_cursor = RC_VA_BASE;
    vmm_map(va, phys, 0x03);

    slots[victim].key = key;
    slots[victim].used = 1;
    slots[victim].dirty = 0;
    slots[victim].phys = phys;
    slots[victim].touch = tick_counter;
}

void Harlin_ReadCacheDrop(u32 key)
{
    for (u64 i = 0; i < RC_BUCKETS; i++) {
        if (slots[i].used && slots[i].key == key) {
            if (slots[i].phys) pmm_free(slots[i].phys);
            slots[i].used = 0;
            slots[i].key = (u32)-1;
            return;
        }
    }
}

u64 Harlin_ReadCacheHits(void) { return hit_count; }
u64 Harlin_ReadCacheMisses(void) { return miss_count; }

void Harlin_ReadCacheTest(void)
{
    u8 src[4096];
    for (u64 i = 0; i < 4096; i++) src[i] = (u8)(i & 0xFF);

    Harlin_ReadCachePut(100, src);
    void* p = 0;
    int rc = Harlin_ReadCacheGet(100, &p);
    ASSERT(rc == 0);
    ASSERT(p != 0);
    const u8* q = (const u8*)p;
    for (u64 i = 0; i < 4096; i++) {
        ASSERT(q[i] == (u8)(i & 0xFF));
    }

    Harlin_ReadCachePut(101, src);
    Harlin_ReadCacheGet(100, &p);
    Harlin_ReadCacheGet(101, &p);
    Harlin_ReadCacheGet(999, &p);

    ASSERT(Harlin_ReadCacheHits() >= 2);
    ASSERT(Harlin_ReadCacheMisses() >= 1);

    Harlin_ReadCacheDrop(100);
    rc = Harlin_ReadCacheGet(100, &p);
    ASSERT(rc == -1);

    for (u64 i = 0; i < 100; i++) {
        u8 tmp[4096];
        for (u64 j = 0; j < 4096; j++) tmp[j] = (u8)((i + j) & 0xFF);
        Harlin_ReadCachePut((u32)(200 + i), tmp);
    }
    pr_info("read_cache: test OK (hits=%llu misses=%llu)",
            (unsigned long long)Harlin_ReadCacheHits(),
            (unsigned long long)Harlin_ReadCacheMisses());
}
