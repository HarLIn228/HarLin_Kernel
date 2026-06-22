#include "chc_loader.h"
#include "vmm.h"
#include "pmm.h"
#include "harlin_API.h"
#include "gdt.h"
#include "scheduler.h"
#include "fat32.h"

#define USER_CODE_BASE 0x400000
#define USER_STACK_TOP 0x800000
#define USER_MAX_SIZE  0x100000
#define LIB_BASE       0x700000

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
    u64 import_offset;
    u64 import_count;
    u64 export_offset;
    u64 export_count;
} __attribute__((packed));

extern void jump_to_user(u64 rip, u64 rsp, u64 rdi);

static struct shared_lib g_chc_libs[CHC_MAX_LIBRARIES];

static void lib_array_init(void)
{
    static int init_done = 0;
    if (!init_done) {
        int i;
        for (i = 0; i < CHC_MAX_LIBRARIES; i++)
            g_chc_libs[i].used = 0;
        init_done = 1;
    }
}

static int lib_alloc_slot(void)
{
    int i;
    lib_array_init();
    for (i = 0; i < CHC_MAX_LIBRARIES; i++) {
        if (!g_chc_libs[i].used) {
            g_chc_libs[i].used = 1;
            return i;
        }
    }
    return CHC_LIB_INVALID;
}

static void unmap_pages(u64 virt, u64 size)
{
    u64 pages = (size + 4095) / 4096;
    u64 i;
    for (i = 0; i < pages; i++) {
        u64 va = virt + i * 4096;
        u64 phys = vmm_get_phys(va);
        if (phys) {
            vmm_unmap(va);
            pmm_free(phys);
        }
    }
}

static int map_pages(u64 virt, u64 size, u64 flags)
{
    u64 pages = (size + 4095) / 4096;
    u64 i;
    for (i = 0; i < pages; i++) {
        u64 phys = pmm_alloc();
        if (!phys) {
            unmap_pages(virt, i * 4096);
            return -1;
        }
        vmm_map(virt + i * 4096, phys, flags);
    }
    return 0;
}

static void unmap_user_pages(u64 virt, u64 size, struct process* proc)
{
    u64 pages = (size + 4095) / 4096;
    u64 i;
    for (i = 0; i < pages; i++) {
        u64 va = virt + i * 4096;
        u64 phys = vmm_get_phys(va);
        vmm_unmap(va);
        if (phys) pmm_free(phys);
        if (proc) {
            int j;
            for (j = 0; j < proc->page_count; j++) {
                if (proc->user_vaddrs[j] == va) {
                    int k;
                    for (k = j; k + 1 < proc->page_count; k++) {
                        proc->user_vaddrs[k] = proc->user_vaddrs[k + 1];
                        proc->user_pages[k] = proc->user_pages[k + 1];
                    }
                    proc->page_count--;
                    break;
                }
            }
        }
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

    if (hdr->import_count > 0) {
        if (hdr->import_offset < CHC_HEADER_SIZE)
            return 0;
        total = hdr->import_offset + hdr->import_count * sizeof(struct chc_import_entry);
        if (total < hdr->import_offset || total > file_size)
            return 0;
    }

    if (hdr->export_count > 0) {
        if (hdr->export_offset < CHC_HEADER_SIZE)
            return 0;
        total = hdr->export_offset + hdr->export_count * sizeof(struct chc_export_entry);
        if (total < hdr->export_offset || total > file_size)
            return 0;
    }

    return 1;
}

static int dl_find_symbol(const char* name, u64* out_addr)
{
    int i;
    for (i = 0; i < CHC_MAX_LIBRARIES; i++) {
        u32 j;
        if (!g_chc_libs[i].used || !g_chc_libs[i].exports)
            continue;
        for (j = 0; j < g_chc_libs[i].export_count; j++) {
            u32 name_off = g_chc_libs[i].exports[j].name_offset;
            if (name_off == 0)
                continue;
            const char* sym_name = (const char*)g_chc_libs[i].file_image + name_off;
            if (Harlin_Compare(sym_name, name) == 0) {
                *out_addr = g_chc_libs[i].code_base + g_chc_libs[i].exports[j].rva;
                return 0;
            }
        }
    }
    return -1;
}

static int dl_resolve_imports(const struct chc_header* hdr, const u8* file_image, u64 code_base)
{
    u64 i;
    if (hdr->import_count == 0)
        return 0;
    for (i = 0; i < hdr->import_count; i++) {
        const struct chc_import_entry* ent;
        u64 name_off;
        u64 addr_off;
        u64 sym_addr;
        char sym_name[128];
        const char* src;
        int len;
        int j;

        ent = (const struct chc_import_entry*)(file_image + hdr->import_offset + i * sizeof(struct chc_import_entry));
        name_off = ent->name_offset;
        addr_off = ent->addr_offset;

        if (name_off == 0 || name_off >= 0x100000)
            return -1;
        src = (const char*)file_image + name_off;
        len = 0;
        while (src[len] && len < 127) len++;
        for (j = 0; j < len; j++)
            sym_name[j] = src[j];
        sym_name[len] = 0;

        if (dl_find_symbol(sym_name, &sym_addr) != 0)
            return -1;

        if (addr_off + 8 <= hdr->code_size)
            *(u64*)(code_base + addr_off) = sym_addr;
    }
    return 0;
}

static int dl_load_library_image(const void* file_data, u64 file_size, struct shared_lib* lib)
{
    const struct chc_header* hdr = (const struct chc_header*)file_data;
    u64 code_base;
    u64 data_base;
    u64 code_pages;
    u64 i;

    if (!header_valid(hdr, file_size))
        return -1;

    code_base = LIB_BASE;
    code_pages = (hdr->code_size + 4095) & ~4095;
    data_base = code_base + code_pages;

    if (map_pages(code_base, hdr->code_size, VMM_PRESENT | VMM_WRITABLE) != 0)
        return -1;
    if (hdr->data_size > 0) {
        if (map_pages(data_base, hdr->data_size, VMM_PRESENT | VMM_WRITABLE) != 0)
            goto fail_code;
    }
    if (hdr->bss_size > 0) {
        u64 bss_base = data_base + ((hdr->data_size + 4095) & ~4095);
        if (map_pages(bss_base, hdr->bss_size, VMM_PRESENT | VMM_WRITABLE) != 0)
            goto fail_data;
    }

    Harlin_Copy((void*)code_base, (const u8*)file_data + hdr->code_offset, hdr->code_size);
    if (hdr->data_size > 0)
        Harlin_Copy((void*)data_base, (const u8*)file_data + hdr->data_offset, hdr->data_size);
    if (hdr->bss_size > 0) {
        u64 bss_base = data_base + ((hdr->data_size + 4095) & ~4095);
        Harlin_Fill((void*)bss_base, 0, hdr->bss_size);
    }

    for (i = 0; i < hdr->reloc_count; i++) {
        u64 offset = ((const u64*)((const u8*)file_data + hdr->reloc_offset))[i];
        if (offset & 7)
            goto fail_all;
        if (offset + 8 > hdr->code_size)
            goto fail_all;
        *(u64*)(code_base + offset) += code_base;
    }

    lib->code_base = code_base;
    lib->data_base = data_base;
    lib->code_size = hdr->code_size;
    lib->data_size = hdr->data_size;
    lib->code_pages = code_pages;
    lib->file_image = (const u8*)file_data;

    if (hdr->export_count > 0 && hdr->export_offset < file_size) {
        lib->exports = (struct chc_export_entry*)((const u8*)file_data + hdr->export_offset);
        lib->export_count = hdr->export_count;
    } else {
        lib->exports = 0;
        lib->export_count = 0;
    }

    return 0;

fail_all:
    if (hdr->bss_size > 0) {
        u64 bss_base = data_base + ((hdr->data_size + 4095) & ~4095);
        unmap_pages(bss_base, hdr->bss_size);
    }
fail_data:
    if (hdr->data_size > 0)
        unmap_pages(data_base, hdr->data_size);
fail_code:
    unmap_pages(code_base, hdr->code_size);
    return -1;
}

int Harlin_DlOpen(const char* path)
{
    struct Harlin_File file;
    struct shared_lib* lib;
    u8* buf;
    u32 size;
    int ret;
    int lid;

    if (!path)
        return CHC_LIB_INVALID;

    if (Harlin_Open(path, &file) != HARLIN_FS_OK)
        return CHC_LIB_INVALID;

    size = Harlin_Size(&file);
    if (size == 0 || size > 0x100000) {
        Harlin_Close(&file);
        return CHC_LIB_INVALID;
    }

    buf = (u8*)pmm_alloc();
    if (!buf) {
        Harlin_Close(&file);
        return CHC_LIB_INVALID;
    }

    if (Harlin_Read(&file, buf, size) != (int)size) {
        pmm_free((u64)buf);
        Harlin_Close(&file);
        return CHC_LIB_INVALID;
    }
    Harlin_Close(&file);

    lid = lib_alloc_slot();
    if (lid < 0) {
        pmm_free((u64)buf);
        return CHC_LIB_INVALID;
    }

    lib = &g_chc_libs[lid];
    ret = dl_load_library_image(buf, size, lib);
    if (ret != 0) {
        lib->used = 0;
        pmm_free((u64)buf);
        return CHC_LIB_INVALID;
    }

    return lid;
}

void* Harlin_DlSym(int lib_id, const char* name)
{
    u32 i;
    if (lib_id < 0 || lib_id >= CHC_MAX_LIBRARIES)
        return 0;
    if (!g_chc_libs[lib_id].used || !g_chc_libs[lib_id].exports)
        return 0;
    if (!name)
        return 0;

    for (i = 0; i < g_chc_libs[lib_id].export_count; i++) {
        u32 name_off = g_chc_libs[lib_id].exports[i].name_offset;
        if (name_off == 0)
            continue;
        if (Harlin_Compare((const char*)g_chc_libs[lib_id].file_image + name_off, name) == 0)
            return (void*)(g_chc_libs[lib_id].code_base + g_chc_libs[lib_id].exports[i].rva);
    }

    return 0;
}

int Harlin_DlClose(int lib_id)
{
    struct shared_lib* lib;
    if (lib_id < 0 || lib_id >= CHC_MAX_LIBRARIES)
        return -1;
    lib = &g_chc_libs[lib_id];
    if (!lib->used)
        return -1;

    if (lib->file_image)
        pmm_free((u64)lib->file_image);

    unmap_pages(lib->code_base, lib->code_size);
    if (lib->data_size > 0)
        unmap_pages(lib->data_base, lib->data_size);

    lib->used = 0;
    lib->exports = 0;
    lib->export_count = 0;
    lib->file_image = 0;
    return 0;
}

int chc_load(const void* file_data, u64 file_size)
{
    const struct chc_header* hdr = (const struct chc_header*)file_data;
    struct process* proc;
    u64 code_base;
    u64 data_base;
    u64 bss_base;
    u64 stack_bottom;
    u64 stack_size_actual;
    u64 code_pages;
    u64 kernel_pml4_saved;
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

    kernel_pml4_saved = vmm_current_pml4();
    vmm_switch(proc->pml4_phys);

    if (map_user_pages(code_base, hdr->code_size, VMM_PRESENT | VMM_USER | VMM_WRITABLE, proc) != 0) {
        vmm_switch(kernel_pml4_saved);
        return -1;
    }
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
        stack_size_actual = (hdr->stack_size + 4095) & ~4095;
        stack_bottom = USER_STACK_TOP - stack_size_actual;
        if (stack_bottom >= USER_STACK_TOP) {
            r = -1;
            goto fail_bss;
        }
        if (map_user_pages(stack_bottom, hdr->stack_size, VMM_PRESENT | VMM_WRITABLE | VMM_USER, proc) != 0)
            goto fail_bss;
    } else {
        stack_size_actual = 4096;
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

    if (hdr->flags & CHC_FLAG_IMPORT) {
        if (dl_resolve_imports(hdr, (const u8*)file_data, code_base) != 0)
            goto fail_all;
    }

    {
        u64 cp;
        for (cp = 0; cp < code_pages / 4096; cp++) {
            u64 va = code_base + cp * 4096;
            u64 phys = vmm_get_phys(va);
            if (phys)
                vmm_map(va, phys, VMM_PRESENT | VMM_USER);
        }
    }

    vmm_switch(kernel_pml4_saved);
    return pid;

fail_all:
    unmap_user_pages(stack_bottom, stack_size_actual, proc);
fail_bss:
    if (hdr->bss_size > 0)
        unmap_user_pages(bss_base, hdr->bss_size, proc);
fail_data:
    if (hdr->data_size > 0)
        unmap_user_pages(data_base, hdr->data_size, proc);
fail_code:
    unmap_user_pages(code_base, hdr->code_size, proc);
    vmm_switch(kernel_pml4_saved);
    return -1;
}
