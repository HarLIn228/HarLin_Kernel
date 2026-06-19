#include "cx_loader.h"
#include "vmm.h"
#include "pmm.h"
#include "string.h"
#include "gdt.h"
#include "scheduler.h"

#define USER_CODE_BASE 0x400000
#define USER_STACK_TOP 0x800000
#define USER_MAX_SIZE  0x100000

struct cx_header {
    u8  magic[8];
    u16 version;
    u16 flags;
    u32 entry_offset;
    u64 code_offset;
    u64 code_size;
    u64 data_offset;
    u64 data_size;
    u64 bss_size;
    u64 reloc_offset;
    u64 reloc_count;
    u64 stack_size;
} __attribute__((packed));

extern void jump_to_user(u64 rip, u64 rsp, u64 rdi);

static int map_user_pages(u64 virt, u64 size, u64 flags)
{
    u64 pages = (size + 4095) / 4096;
    u64 i;
    for (i = 0; i < pages; i++) {
        u64 phys = pmm_alloc();
        if (!phys)
            return -1;
        vmm_map(virt + i * 4096, phys, flags);
    }
    return 0;
}

static int header_valid(const struct cx_header* hdr, u64 file_size)
{
    u64 total;

    if (file_size < CX_HEADER_SIZE)
        return 0;
    if (str_cmp((const char*)hdr->magic, CX_MAGIC) != 0)
        return 0;
    if (hdr->version != 1)
        return 0;
    if (hdr->code_offset < CX_HEADER_SIZE)
        return 0;
    if (hdr->code_size > USER_MAX_SIZE)
        return 0;
    if (hdr->data_size > USER_MAX_SIZE)
        return 0;
    if (hdr->bss_size > USER_MAX_SIZE)
        return 0;
    if (hdr->stack_size > USER_MAX_SIZE)
        return 0;
    if (hdr->entry_offset >= hdr->code_size)
        return 0;

    total = hdr->code_offset + hdr->code_size;
    if (total < hdr->code_offset || total > file_size)
        return 0;

    if (hdr->data_size > 0) {
        total = hdr->data_offset + hdr->data_size;
        if (total < hdr->data_offset || total > file_size)
            return 0;
    }

    total = hdr->reloc_offset + hdr->reloc_count * 8;
    if (total < hdr->reloc_offset || total > file_size)
        return 0;

    return 1;
}

int cx_load(const void* file_data, u64 file_size)
{
    const struct cx_header* hdr = (const struct cx_header*)file_data;
    u64 code_base;
    u64 data_base;
    u64 bss_base;
    u64 stack_bottom;
    u64 i;

    if (!header_valid(hdr, file_size))
        return -1;

    code_base = USER_CODE_BASE;
    data_base = code_base + ((hdr->code_size + 4095) & ~4095);
    bss_base = data_base + ((hdr->data_size + 4095) & ~4095);

    if (map_user_pages(code_base, hdr->code_size, VMM_PRESENT | VMM_USER) != 0)
        return -1;
    if (hdr->data_size > 0) {
        if (map_user_pages(data_base, hdr->data_size, VMM_PRESENT | VMM_WRITABLE | VMM_USER) != 0)
            return -1;
    }
    if (hdr->bss_size > 0) {
        if (map_user_pages(bss_base, hdr->bss_size, VMM_PRESENT | VMM_WRITABLE | VMM_USER) != 0)
            return -1;
    }
    if (hdr->stack_size > 0) {
        stack_bottom = USER_STACK_TOP - ((hdr->stack_size + 4095) & ~4095);
        if (map_user_pages(stack_bottom, hdr->stack_size, VMM_PRESENT | VMM_WRITABLE | VMM_USER) != 0)
            return -1;
    } else {
        stack_bottom = USER_STACK_TOP - 4096;
        if (map_user_pages(stack_bottom, 4096, VMM_PRESENT | VMM_WRITABLE | VMM_USER) != 0)
            return -1;
    }

    Harlin_MemCopy((void*)code_base, (const u8*)file_data + hdr->code_offset, hdr->code_size);
    if (hdr->data_size > 0)
        Harlin_MemCopy((void*)data_base, (const u8*)file_data + hdr->data_offset, hdr->data_size);
    if (hdr->bss_size > 0)
        Harlin_MemSet((void*)bss_base, 0, hdr->bss_size);

    for (i = 0; i < hdr->reloc_count; i++) {
        u64 offset = ((const u64*)((const u8*)file_data + hdr->reloc_offset))[i];
        if (offset + 8 > hdr->code_size)
            return -1;
        *(u64*)(code_base + offset) += code_base;
    }

    return process_create(code_base + hdr->entry_offset, USER_STACK_TOP);
}
