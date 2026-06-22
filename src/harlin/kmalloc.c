#include "kmalloc.h"
#include "pmm.h"
#include "vmm.h"
#include "spinlock.h"

#define KMALLOC_MIN_SIZE 16
#define KMALLOC_MAX_SIZE 0x100000
#define KMALLOC_BLOCK_MAGIC 0x48414E44
#define KERNEL_HEAP_START 0xFFFF800000000000

struct kmalloc_block {
    u32 magic;
    u32 size;
    u32 used;
    u32 pages;
    u64 base_virt;
    struct kmalloc_block* next;
    struct kmalloc_block* prev;
};

static struct kmalloc_block* kmalloc_head = 0;
static struct spinlock kmalloc_lock;
static u64 next_heap_virt = KERNEL_HEAP_START;

void kmalloc_init(void)
{
    spinlock_init(&kmalloc_lock);
    kmalloc_head = 0;
    next_heap_virt = KERNEL_HEAP_START;
}

static int map_pages(u64 virt, u64 size)
{
    u64 pages = (size + 4095) / 4096;
    u64 i;
    for (i = 0; i < pages; i++) {
        u64 phys = pmm_alloc();
        if (!phys) {
            while (i > 0) {
                i--;
                phys = vmm_get_phys(virt + i * 4096);
                vmm_unmap(virt + i * 4096);
                if (phys) pmm_free(phys);
            }
            return -1;
        }
        vmm_map(virt + i * 4096, phys, VMM_PRESENT | VMM_WRITABLE);
    }
    return 0;
}

static void unmap_pages(u64 virt, u64 pages)
{
    u64 i;
    for (i = 0; i < pages; i++) {
        u64 phys = vmm_get_phys(virt + i * 4096);
        vmm_unmap(virt + i * 4096);
        if (phys) pmm_free(phys);
    }
}

static struct kmalloc_block* create_block(u32 size)
{
    u32 total;
    u64 virt;
    struct kmalloc_block* block;

    total = sizeof(struct kmalloc_block) + size;
    if (total < 4096)
        total = 4096;
    total = (total + 4095) & ~4095;

    virt = next_heap_virt;
    if (map_pages(virt, total) != 0)
        return 0;
    next_heap_virt += total;

    block = (struct kmalloc_block*)virt;
    block->magic = KMALLOC_BLOCK_MAGIC;
    block->size = total - sizeof(struct kmalloc_block);
    block->used = 0;
    block->pages = total / 4096;
    block->base_virt = virt;
    block->next = 0;
    block->prev = 0;
    return block;
}

void* kmalloc(u64 size)
{
    struct kmalloc_block* block;
    struct kmalloc_block* remaining;
    if (size == 0 || size > KMALLOC_MAX_SIZE)
        return 0;
    spinlock_acquire(&kmalloc_lock);
    block = kmalloc_head;
    while (block) {
        if (!block->used && block->size >= size) {
            if (block->size >= size + sizeof(struct kmalloc_block) + KMALLOC_MIN_SIZE) {
                remaining = (struct kmalloc_block*)((u8*)block + sizeof(struct kmalloc_block) + size);
                remaining->magic = KMALLOC_BLOCK_MAGIC;
                remaining->size = block->size - size - sizeof(struct kmalloc_block);
                remaining->used = 0;
                remaining->pages = block->pages;
                remaining->base_virt = block->base_virt;
                remaining->next = block->next;
                remaining->prev = block;
                if (block->next)
                    block->next->prev = remaining;
                block->next = remaining;
                block->size = size;
            }
            block->used = 1;
            spinlock_release(&kmalloc_lock);
            return (u8*)block + sizeof(struct kmalloc_block);
        }
        block = block->next;
    }
    block = create_block((u32)size);
    if (!block) {
        spinlock_release(&kmalloc_lock);
        return 0;
    }
    block->next = kmalloc_head;
    if (kmalloc_head)
        kmalloc_head->prev = block;
    kmalloc_head = block;
    block->used = 1;
    spinlock_release(&kmalloc_lock);
    return (u8*)block + sizeof(struct kmalloc_block);
}

void kfree(void* ptr)
{
    struct kmalloc_block* block;
    if (!ptr)
        return;
    block = (struct kmalloc_block*)((u8*)ptr - sizeof(struct kmalloc_block));
    if (block->magic != KMALLOC_BLOCK_MAGIC)
        return;
    spinlock_acquire(&kmalloc_lock);
    block->used = 0;
    if (block->next && !block->next->used) {
        block->size += sizeof(struct kmalloc_block) + block->next->size;
        block->next = block->next->next;
        if (block->next)
            block->next->prev = block;
    }
    if (block->prev && !block->prev->used) {
        block->prev->size += sizeof(struct kmalloc_block) + block->size;
        block->prev->next = block->next;
        if (block->next)
            block->next->prev = block->prev;
    }
    spinlock_release(&kmalloc_lock);
}

void* krealloc(void* ptr, u64 size)
{
    void* new_ptr;
    struct kmalloc_block* block;
    u64 old_size;
    if (!ptr)
        return kmalloc(size);
    if (size == 0) {
        kfree(ptr);
        return 0;
    }
    block = (struct kmalloc_block*)((u8*)ptr - sizeof(struct kmalloc_block));
    if (block->magic != KMALLOC_BLOCK_MAGIC)
        return 0;
    old_size = block->size;
    if (old_size >= size)
        return ptr;
    new_ptr = kmalloc(size);
    if (!new_ptr)
        return 0;
    {
        u8* d = (u8*)new_ptr;
        const u8* s = (const u8*)ptr;
        u64 i;
        for (i = 0; i < old_size; i++)
            d[i] = s[i];
    }
    block = (struct kmalloc_block*)((u8*)ptr - sizeof(struct kmalloc_block));
    if (block->magic == KMALLOC_BLOCK_MAGIC)
        kfree(ptr);
    return new_ptr;
}

u64 ksize(void* ptr)
{
    struct kmalloc_block* block;
    if (!ptr)
        return 0;
    block = (struct kmalloc_block*)((u8*)ptr - sizeof(struct kmalloc_block));
    if (block->magic != KMALLOC_BLOCK_MAGIC)
        return 0;
    return block->size;
}
