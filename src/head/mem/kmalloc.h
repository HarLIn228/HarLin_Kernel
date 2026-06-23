#ifndef KMALLOC_H
#define KMALLOC_H

#include "harlin_API.h"

#define KMALLOC_BLOCK_MAGIC 0x48414E44

struct kmalloc_block {
    u32 magic;
    u32 size;
    u32 used;
    u32 pages;
    u64 base_virt;
    struct kmalloc_block* next;
    struct kmalloc_block* prev;
};

void kmalloc_init(void);
void* kmalloc(u64 size);
void kfree(void* ptr);
void* krealloc(void* ptr, u64 size);
u64 ksize(void* ptr);

#endif
