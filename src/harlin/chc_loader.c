#include "chc_loader.h"
#include "vmm.h"
#include "pmm.h"
#include "harlin_API.h"
#include "gdt.h"
#include "scheduler.h"

#define USER_CODE_BASE 0x400000
#define USER_STACK_TOP 0x800000
#define USER_MAX_SIZE  0x100000

struct chc_header {
    u8  magic[9];
    u8  reserved[7];
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

static void unmap_user_pages(u64 virt, u64 size, struct process* proc)
{
    u64 pages = (size + 4095) / 4096;
    u64 i;
    for (i = 0; i < pages; i++) {
        u64 phys = vmm_get_phys(virt + i * 4096);
        vmm_unmap(virt + i * 4096);
        if (phys) pmm_free(phys);
    }
}

static int map_user_pages(u64 virt, u64 size, u64 flags, struct process* proc)
{
    u64 pages = (size + 4095) / 4096;
    u64 i;
    for (i = 0; i < pages; i++) {
        u64 phys = pmm_alloc();
        if (!phys) {
            unmap_user_pages(virt, i * 4096, proc);
            return -1;
        }
        vmm_map(virt + i * 4096, phys, flags);
        if (proc && proc->page_count < 16) {
            proc->user_vaddrs[proc->page_count] = virt + i * 4096;
            proc->user_pages[proc->page_count++] = phys;
        }
    }
    return 0;
}

static int header_valid(const struct chc_header* hdr, u64 file_size)
{
    u64 total;

    if (!hdr || file_size < CHC_HEADER_SIZE)
        return 0;
    if (Harlin_Compare((const char*)hdr->magic, CHC_MAGIC) != 0)
        return 0;
    if (hdr->version != 1)
        return 0;
    if (hdr->code_offset < CHC_HEADER_SIZE)
        return 0;
    if (hdr->code_size == 0 || hdr->code_size > USER_MAX_SIZE)
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
        if (hdr->data_offset < CHC_HEADER_SIZE)
            return 0;
        total = hdr->data_offset + hdr->data_size;
        if (total < hdr->data_offset || total > file_size)
            return 0;
    }

    if (hdr->reloc_count > 0) {
        if (hdr->reloc_offset < CHC_HEADER_SIZE)
            return 0;
        total = hdr->reloc_offset + hdr->reloc_count * 8;
        if (total < hdr->reloc_offset || total > file_size)
            return 0;
    }

    return 1;
}

int chc_load(const void* file_data, u64 file_size)
{
    const struct chc_header* hdr = (const struct chc_header*)file_data;
    struct process* proc;
    u64 code_base;
    u64 data_base;
    u64 bss_base;
    u64 stack_bottom;
    u64 code_pages;
    u64 i;
    int r;
    int pid;

    if (!header_valid(hdr, file_size))
        return -1;

    code_base = USER_CODE_BASE;
    code_pages = (hdr->code_size + 4095) & ~4095;
    data_base = code_base + code_pages;
    bss_base = data_base + ((hdr->data_size + 4095) & ~4095);

    pid = process_create(code_base + hdr->entry_offset, USER_STACK_TOP);
    if (pid < 0)
        return -1;
    proc = process_get(pid);
    if (!proc)
        return -1;

    if (map_user_pages(code_base, hdr->code_size, VMM_PRESENT | VMM_USER, proc) != 0)
        return -1;
    if (hdr->data_size > 0) {
        if (map_user_pages(data_base, hdr->data_size, VMM_PRESENT | VMM_WRITABLE | VMM_USER, proc) != 0)
            goto fail_code;
    }
    if (hdr->bss_size > 0) {
        if (bss_base < data_base) {
            r = -1;
            goto fail_data;
        }
        if (map_user_pages(bss_base, hdr->bss_size, VMM_PRESENT | VMM_WRITABLE | VMM_USER, proc) != 0)
            goto fail_data;
    }
    if (hdr->stack_size > 0) {
        stack_bottom = USER_STACK_TOP - ((hdr->stack_size + 4095) & ~4095);
        if (stack_bottom >= USER_STACK_TOP) {
            r = -1;
            goto fail_bss;
        }
        if (map_user_pages(stack_bottom, hdr->stack_size, VMM_PRESENT | VMM_WRITABLE | VMM_USER, proc) != 0)
            goto fail_bss;
    } else {
        stack_bottom = USER_STACK_TOP - 4096;
        if (map_user_pages(stack_bottom, 4096, VMM_PRESENT | VMM_WRITABLE | VMM_USER, proc) != 0)
            goto fail_bss;
    }

    Harlin_Copy((void*)code_base, (const u8*)file_data + hdr->code_offset, hdr->code_size);
    if (hdr->data_size > 0)
        Harlin_Copy((void*)data_base, (const u8*)file_data + hdr->data_offset, hdr->data_size);
    if (hdr->bss_size > 0)
        Harlin_Fill((void*)bss_base, 0, hdr->bss_size);

    for (i = 0; i < hdr->reloc_count; i++) {
        u64 offset = ((const u64*)((const u8*)file_data + hdr->reloc_offset))[i];
        if (offset & 7)
            goto fail_all;
        if (offset + 8 > hdr->code_size)
            goto fail_all;
        *(u64*)(code_base + offset) += code_base;
    }

    return pid;

fail_all:
    unmap_user_pages(stack_bottom, hdr->stack_size ? hdr->stack_size : 4096, proc);
fail_bss:
    if (hdr->bss_size > 0)
        unmap_user_pages(bss_base, hdr->bss_size, proc);
fail_data:
    if (hdr->data_size > 0)
        unmap_user_pages(data_base, hdr->data_size, proc);
fail_code:
    unmap_user_pages(code_base, hdr->code_size, proc);
    return -1;
}
