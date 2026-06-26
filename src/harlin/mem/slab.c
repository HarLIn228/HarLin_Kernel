#include "slab.h"
#include "pmm.h"
#include "vmm.h"
#include "printk.h"
#include "bug.h"
#include "harlin_API.h"

#define BP_PAGES_PER_CHUNK 1
#define BP_MAX_CLASSES 8
#define BP_GUARD_LIVE  0xC0FFEEF00D123456ULL
#define BP_GUARD_DEAD  0xFEEDF00DDEADBEEFULL
#define BP_HEAP_VA     0xFFFF800080000000ULL
#define BP_MAX_CHUNKS  128

static const u64 block_sizes[BP_MAX_CLASSES] = {
    16, 32, 64, 128, 256, 512, 1024, 2048
};

static struct block_cache caches[BP_MAX_CLASSES];
static struct block_chunk* all_chunks[BP_MAX_CHUNKS];
static u64 chunk_count;
static u64 next_chunk_va = BP_HEAP_VA;

static int class_for(u64 size)
{
    for (u64 i = 0; i < BP_MAX_CLASSES; i++) {
        if (size <= block_sizes[i]) return (int)i;
    }
    return -1;
}

static u64 header_bytes(void)
{
    u64 h = sizeof(struct block_chunk);
    h = (h + 15) & ~15ULL;
    return h;
}

static int map_pages(u64 va, u64 pages)
{
    for (u64 i = 0; i < pages; i++) {
        u64 phys = pmm_alloc();
        if (!phys) {
            for (u64 j = 0; j < i; j++) {
                u64 p = vmm_get_phys(va + j * 4096);
                vmm_unmap(va + j * 4096);
                if (p) pmm_free(p);
            }
            return -1;
        }
        if (vmm_map(va + i * 4096, phys, 0x03) != 0) {
            pmm_free(phys);
            for (u64 j = 0; j < i; j++) {
                u64 p = vmm_get_phys(va + j * 4096);
                vmm_unmap(va + j * 4096);
                if (p) pmm_free(p);
            }
            return -1;
        }
    }
    return 0;
}

static struct block_chunk* chunk_create(int class_idx)
{
    u64 block_size = block_sizes[class_idx];
    u64 hdr = header_bytes();
    u64 usable = (u64)BP_PAGES_PER_CHUNK * 4096 - hdr;
    u64 block_count = usable / block_size;
    if (block_count < 2) return (struct block_chunk*)0;
    if (block_count > 1024) block_count = 1024;

    if (chunk_count >= BP_MAX_CHUNKS) return (struct block_chunk*)0;
    u64 va = next_chunk_va;
    next_chunk_va += (u64)BP_PAGES_PER_CHUNK * 4096;

    if (map_pages(va, BP_PAGES_PER_CHUNK) != 0) {
        next_chunk_va -= (u64)BP_PAGES_PER_CHUNK * 4096;
        return (struct block_chunk*)0;
    }

    u64 phys = vmm_get_phys(va);
    struct block_chunk* c = (struct block_chunk*)va;
    c->base_virt = va;
    c->base_phys = phys;
    c->pages = BP_PAGES_PER_CHUNK;
    c->block_size = block_size;
    c->block_count = block_count;
    c->free_count = block_count;
    c->free_stack = 0;
    c->next_slot = 0;
    c->next = 0;
    c->cache = &caches[class_idx];
    c->guard = BP_GUARD_LIVE;

    u8* data = (u8*)va + hdr;
    for (u64 i = 0; i < block_count; i++) {
        u64 slot_va = (u64)(data + i * block_size);
        *(u64*)slot_va = BP_GUARD_LIVE;
    }

    all_chunks[chunk_count++] = c;
    return c;
}

void Harlin_BlockPoolInit(void)
{
    for (u64 i = 0; i < BP_MAX_CLASSES; i++) {
        caches[i].block_size = block_sizes[i];
        caches[i].payload_size = block_sizes[i] - 16;
        caches[i].give_count = 0;
        caches[i].take_count = 0;
        caches[i].partial = 0;
        caches[i].full = 0;
        caches[i].empty = 0;
    }
    chunk_count = 0;
    next_chunk_va = BP_HEAP_VA;
    pr_info("block_pool: 8 sizes ready [16..2048]");
}

static struct block_chunk* pick_chunk(struct block_cache* c, int class_idx)
{
    if (c->partial) return c->partial;
    if (c->empty) {
        struct block_chunk* x = c->empty;
        c->empty = x->next;
        x->next = c->partial;
        c->partial = x;
        return x;
    }
    struct block_chunk* x = chunk_create(class_idx);
    if (!x) return 0;
    x->next = c->partial;
    c->partial = x;
    return x;
}

static void* slot_addr(struct block_chunk* c, u64 idx)
{
    u64 hdr = header_bytes();
    return (void*)((u8*)c->base_virt + hdr + idx * c->block_size);
}

void* Harlin_BlockAlloc(u64 want_size)
{
    int idx = class_for(want_size);
    if (idx < 0) return (void*)0;
    struct block_cache* c = &caches[idx];
    struct block_chunk* ch = pick_chunk(c, idx);
    if (!ch) return (void*)0;

    u64 slot_idx;
    if (ch->free_stack) {
        slot_idx = *ch->free_stack;
        ch->free_stack++;
        ch->free_count--;
    } else {
        if (ch->next_slot >= ch->block_count) return (void*)0;
        slot_idx = ch->next_slot++;
        ch->free_count--;
    }

    void* raw = slot_addr(ch, slot_idx);
    u64* guard = (u64*)raw;
    if (*guard != BP_GUARD_LIVE) {
        pr_emerg("block_pool: guard smashed class=%d slot=%llu",
                 idx, (unsigned long long)slot_idx);
        panic("block_pool guard smashed");
    }
    *guard = BP_GUARD_DEAD;

    c->give_count++;
    c->take_count--;
    if (ch->free_count == 0) {
        if (c->partial == ch) c->partial = ch->next;
        ch->next = c->full;
        c->full = ch;
    }
    return (u8*)raw + 16;
}

void Harlin_BlockFree(void* ptr, u64 want_size)
{
    if (!ptr) return;
    int idx = class_for(want_size);
    if (idx < 0) {
        pr_err("BlockFree: bad size %llu", (unsigned long long)want_size);
        return;
    }
    struct block_cache* c = &caches[idx];

    u8* slot = (u8*)ptr - 16;
    u64 slot_va = (u64)slot;
    struct block_chunk* found = (struct block_chunk*)0;
    for (u64 i = 0; i < chunk_count; i++) {
        struct block_chunk* ch = all_chunks[i];
        if (ch->cache != c) continue;
        u64 base = ch->base_virt;
        u64 end  = base + ch->pages * 4096;
        if (slot_va >= base && slot_va < end) {
            u64 hdr = header_bytes();
            if (((slot_va - base - hdr) % ch->block_size) == 0) {
                found = ch;
                break;
            }
        }
    }
    if (!found) {
        pr_emerg("BlockFree: ptr %p not in pool (size=%llu)",
                 ptr, (unsigned long long)want_size);
        panic("BlockFree: unknown pointer");
    }
    u64* guard = (u64*)slot;
    if (*guard != BP_GUARD_DEAD) {
        pr_emerg("BlockFree: guard corrupted (got=%llx)",
                 (unsigned long long)*guard);
        panic("block_pool guard corrupted");
    }
    *guard = BP_GUARD_LIVE;

    found->free_count++;
    *((u64*)slot + 1) = 0;

    if (found->free_count == 1 && found->next_slot == found->block_count) {
        if (c->full == found) c->full = found->next;
        if (c->partial == found) c->partial = found->next;
        found->next = c->empty;
        c->empty = found;
    }
    c->give_count--;
    c->take_count++;
}

void Harlin_BlockPoolTest(void)
{
    void* a = Harlin_BlockAlloc(16);
    void* b = Harlin_BlockAlloc(64);
    void* c = Harlin_BlockAlloc(1024);
    void* d = Harlin_BlockAlloc(2048);
    void* e = Harlin_BlockAlloc(2049);
    ASSERT(a != 0);
    ASSERT(b != 0);
    ASSERT(c != 0);
    ASSERT(d != 0);
    ASSERT(e == 0);

    u8* pa = (u8*)a;
    for (int i = 0; i < 16; i++) pa[i] = (u8)(0xA0 + i);
    u8* pb = (u8*)b;
    for (int i = 0; i < 64; i++) pb[i] = (u8)(0xB0 + i);

    Harlin_BlockFree(b, 64);
    void* b2 = Harlin_BlockAlloc(32);
    ASSERT(b2 != 0);
    Harlin_BlockFree(a, 16);
    Harlin_BlockFree(b2, 32);
    Harlin_BlockFree(c, 1024);
    Harlin_BlockFree(d, 2048);

    pr_info("block_pool: test OK");
}
