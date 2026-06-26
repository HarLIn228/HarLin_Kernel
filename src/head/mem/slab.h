#ifndef BLOCK_POOL_H
#define BLOCK_POOL_H

#include "harlin_API.h"

#define BLOCK_GUARD_MAGIC 0xDEADBEEFCAFEBABEULL

struct block_chunk;
struct block_cache;

struct block_chunk {
    u64 base_virt;
    u64 base_phys;
    u64 pages;
    u64 block_size;
    u64 block_count;
    u64 free_count;
    u64* free_stack;
    u32 next_slot;
    struct block_chunk* next;
    struct block_cache* cache;
    u64 guard;
};

struct block_cache {
    u64 block_size;
    u64 payload_size;
    u64 give_count;
    u64 take_count;
    struct block_chunk* partial;
    struct block_chunk* full;
    struct block_chunk* empty;
};

void Harlin_BlockPoolInit(void);
void* Harlin_BlockAlloc(u64 want_size);
void Harlin_BlockFree(void* ptr, u64 want_size);
void Harlin_BlockPoolTest(void);

#endif
