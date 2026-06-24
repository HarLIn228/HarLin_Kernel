#include "efi_types.h"

static unsigned char g_memmap[16384];
static unsigned char g_boot_info_storage[4096];

static void outb(UINT16 port, UINT8 val)
{
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static void debugcon_putc(char c)
{
    outb(0x0402, (UINT8)c);
}

static void debugcon_puts(const char* s)
{
    while (*s) debugcon_putc(*s++);
}

static UINTN strlen16(const UINT16* s)
{
    UINTN n = 0;
    while (s[n]) n++;
    return n;
}

static EFI_STATUS find_gop(EFI_HANDLE ImageHandle, void* SystemTable, EFI_GRAPHICS_OUTPUT_PROTOCOL** out_gop)
{
    EFI_GUID gop_guid = {
        0x9042A9DE, 0x23DC, 0x4A38,
        { 0x96, 0xFB, 0x7A, 0xDE, 0xD0, 0x80, 0x51, 0x6A }
    };

    UINT64 st_base = (UINT64)SystemTable;
    UINT64 bs_ptr_addr = st_base + 96;
    UINT64 bs = *(UINT64*)bs_ptr_addr;

    UINT64 locate_protocol_addr = bs + (24 + 37 * 8);
    EFI_LOCATE_PROTOCOL locate = (EFI_LOCATE_PROTOCOL)locate_protocol_addr;

    EFI_STATUS status = locate(&gop_guid, 0, (void**)out_gop);
    (void)ImageHandle;
    return status;
}

static EFI_STATUS get_memory_map(EFI_HANDLE ImageHandle, void* SystemTable, UINT64* out_map_key, UINT32* out_desc_size)
{
    UINT64 st_base = (UINT64)SystemTable;
    UINT64 bs = *(UINT64*)(st_base + 96);
    UINT64 get_memmap_addr = bs + (24 + 4 * 8);
    EFI_GET_MEMORY_MAP get_memmap = (EFI_GET_MEMORY_MAP)get_memmap_addr;

    UINT64 memmap_size = sizeof(g_memmap);
    EFI_STATUS status = get_memmap(&memmap_size, g_memmap, out_map_key, out_desc_size, 0);
    (void)ImageHandle;
    return status;
}

static EFI_STATUS exit_boot_services(EFI_HANDLE ImageHandle, void* SystemTable, UINT64 map_key)
{
    UINT64 st_base = (UINT64)SystemTable;
    UINT64 bs = *(UINT64*)(st_base + 96);
    UINT64 exit_bs_addr = bs + (24 + 26 * 8);
    EFI_EXIT_BOOT_SERVICES exit_bs = (EFI_EXIT_BOOT_SERVICES)exit_bs_addr;
    return exit_bs(ImageHandle, map_key);
}

static EFI_STATUS find_acpi(EFI_HANDLE ImageHandle, void* SystemTable, UINT64* out_rsdp)
{
    EFI_GUID acpi20_guid = {
        0x8868E871, 0xE4F1, 0x11D3,
        { 0xBC, 0x22, 0x00, 0x80, 0xC7, 0x3C, 0x88, 0x81 }
    };

    EFI_GUID acpi10_guid = {
        0xEB9D2D30, 0x2D88, 0x11D3,
        { 0x9A, 0x16, 0x00, 0x90, 0x27, 0x3F, 0xC1, 0x4D }
    };

    UINT64 st_base = (UINT64)SystemTable;
    UINT64 st_entries = *(UINT64*)(st_base + 72);
    UINT64 config_table_addr = *(UINT64*)(st_base + 64);

    UINT64 i;
    struct {
        EFI_GUID VendorGuid;
        void* VendorTable;
    }* entries = (void*)config_table_addr;

    *out_rsdp = 0;
    for (i = 0; i < st_entries; i++) {
        EFI_GUID* g = &entries[i].VendorGuid;
        if (g->Data1 == acpi20_guid.Data1 && g->Data2 == acpi20_guid.Data2 && g->Data3 == acpi20_guid.Data3) {
            if (g->Data4[0] == acpi20_guid.Data4[0] && g->Data4[1] == acpi20_guid.Data4[1]) {
                *out_rsdp = (UINT64)entries[i].VendorTable;
                (void)ImageHandle;
                return 0;
            }
        }
        if (g->Data1 == acpi10_guid.Data1 && g->Data2 == acpi10_guid.Data2 && g->Data3 == acpi10_guid.Data3) {
            if (g->Data4[0] == acpi10_guid.Data4[0] && g->Data4[1] == acpi10_guid.Data4[1]) {
                if (*out_rsdp == 0) {
                    *out_rsdp = (UINT64)entries[i].VendorTable;
                }
            }
        }
    }
    (void)ImageHandle;
    return 0;
}

static void store_boot_info(EFI_GRAPHICS_OUTPUT_PROTOCOL* gop, UINT64 rsdp)
{
    EFI_BOOT_INFO info;
    UINT8* dst = (UINT8*)0x8000;
    UINTN i;

    info.Magic = EFI_BOOT_INFO_MAGIC;
    info.FrameBufferBase = gop ? gop->Mode->FrameBufferBase : 0;
    info.FrameBufferSize = gop ? gop->Mode->FrameBufferSize : 0;
    info.HoriResolution = gop ? gop->Mode->ResolutionX : 0;
    info.VertResolution = gop ? gop->Mode->ResolutionY : 0;
    info.PixelsPerScanLine = gop ? gop->Mode->PixelsPerScanLine : 0;
    info.PixelFormat = gop ? gop->Mode->PixelFormat : 0;
    info.RsdpAddress = rsdp;
    info.KernelEntryPhys = 0x10000;

    for (i = 0; i < sizeof(info); i++) dst[i] = ((UINT8*)&info)[i];
}

static EFI_STATUS locate_fs(EFI_HANDLE ImageHandle, void* SystemTable, EFI_SIMPLE_FILE_SYSTEM_PROTOCOL** out_fs)
{
    EFI_GUID fs_guid = {
        0x0964E5B2, 0x6459, 0x11D2,
        { 0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x2B }
    };

    UINT64 st_base = (UINT64)SystemTable;
    UINT64 bs = *(UINT64*)(st_base + 96);
    UINT64 locate_protocol_addr = bs + (24 + 37 * 8);
    EFI_LOCATE_PROTOCOL locate = (EFI_LOCATE_PROTOCOL)locate_protocol_addr;

    EFI_STATUS status = locate(&fs_guid, 0, (void**)out_fs);
    (void)ImageHandle;
    return status;
}

static EFI_STATUS load_kernel(EFI_HANDLE ImageHandle, void* SystemTable)
{
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* fs = 0;
    EFI_FILE_PROTOCOL* root = 0;
    EFI_FILE_PROTOCOL* kfile = 0;
    UINT16 name[16];
    UINT64 read_size = 0;
    UINT8* dst = (UINT8*)0x10000;
    UINT64 total = 0;
    EFI_STATUS status;
    UINTN i;
    EFI_FILE_OPEN file_open;
    EFI_FILE_READ file_read;
    EFI_FILE_CLOSE file_close;

    (void)ImageHandle;

    for (i = 0; i < 16; i++) name[i] = 0;
    name[0]  = '\\';
    name[1]  = 'k';
    name[2]  = 'e';
    name[3]  = 'r';
    name[4]  = 'n';
    name[5]  = 'e';
    name[6]  = 'l';
    name[7]  = '.';
    name[8]  = 'b';
    name[9]  = 'i';
    name[10] = 'n';

    status = locate_fs(ImageHandle, SystemTable, &fs);
    if (status != 0 || fs == 0) {
        debugcon_puts("[EFI] no fs\n");
        return -1;
    }

    EFI_SIMPLE_FILE_OPEN_VOLUME open_volume = (EFI_SIMPLE_FILE_OPEN_VOLUME)fs->OpenVolume;
    status = open_volume(fs, &root);
    if (status != 0 || root == 0) {
        debugcon_puts("[EFI] no root\n");
        return -1;
    }

    file_open = (EFI_FILE_OPEN)root->Open;
    file_read = (EFI_FILE_READ)root->Read;
    file_close = (EFI_FILE_CLOSE)root->Close;

    status = file_open(root, &kfile, name, 1, 0);
    if (status != 0 || kfile == 0) {
        debugcon_puts("[EFI] no file\n");
        file_close(root);
        return -1;
    }

    for (i = 0; i < 1024; i++) {
        read_size = 4096;
        status = file_read(kfile, &read_size, dst + total);
        if (status != 0) {
            debugcon_puts("[EFI] read err\n");
            file_close(kfile);
            file_close(root);
            return -1;
        }
        if (read_size == 0) break;
        total += read_size;
        if (total >= 0x200000) break;
    }

    file_close(kfile);
    file_close(root);

    debugcon_puts("[EFI] kernel loaded\n");
    return 0;
}

EFI_STATUS efi_main(EFI_HANDLE ImageHandle, void* SystemTable)
{
    EFI_GRAPHICS_OUTPUT_PROTOCOL* gop = 0;
    UINT64 map_key = 0;
    UINT32 desc_size = 0;
    UINT64 rsdp = 0;
    EFI_STATUS status;

    debugcon_puts("[EFI] entry\n");

    status = find_gop(ImageHandle, SystemTable, &gop);
    if (status != 0 || gop == 0) {
        debugcon_puts("[EFI] GOP missing\n");
    } else {
        debugcon_puts("[EFI] GOP ok\n");
    }

    find_acpi(ImageHandle, SystemTable, &rsdp);
    if (rsdp != 0) {
        debugcon_puts("[EFI] ACPI ok\n");
    } else {
        debugcon_puts("[EFI] ACPI missing\n");
    }

    status = load_kernel(ImageHandle, SystemTable);
    if (status != 0) {
        debugcon_puts("[EFI] load fail\n");
        return status;
    }

    status = get_memory_map(ImageHandle, SystemTable, &map_key, &desc_size);
    if (status != 0) {
        debugcon_puts("[EFI] memmap fail\n");
        return status;
    }
    (void)desc_size;

    store_boot_info(gop, rsdp);

    debugcon_puts("[EFI] ExitBootServices\n");
    status = exit_boot_services(ImageHandle, SystemTable, map_key);
    if (status != 0) {
        return status;
    }

    __asm__ volatile (
        "cli\n"
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "mov %%ax, %%ss\n"
        "mov $0x90000, %%rsp\n"
        "mov $0x10000, %%rax\n"
        "call *%%rax\n"
        "1: hlt\n"
        "jmp 1b\n"
        ::: "rax", "memory"
    );

    return 0;
}
