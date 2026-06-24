#include "acpi.h"
#include "io.h"
#include "screen.h"
#include "vmm.h"
#include "pmm.h"
#include "kmalloc.h"
#include "smp.h"

#define RSDP_SIGNATURE 0x2052545020445352ULL
#define RSDP_SIG_EBDA 0x80000
#define RSDP_SIG_EBDA_END 0x9FFFF
#define RSDP_SIG_BIOS 0xE0000
#define RSDP_SIG_BIOS_END 0xFFFFF

#define RSDP_EBDA_SEG 0x40E

static struct acpi_rsdp* rsdp_find(void)
{
    unsigned char* addr;
    unsigned short ebda_seg;
    ebda_seg = *(unsigned short*)(unsigned long)RSDP_EBDA_SEG;
    if (ebda_seg) {
        unsigned long ebda_start = (unsigned long)ebda_seg << 4;
        unsigned long ebda_end = ebda_start + 1024;
        for (addr = (unsigned char*)ebda_start; addr < (unsigned char*)ebda_end; addr += 16) {
            if (*(unsigned long long*)addr == RSDP_SIGNATURE) return (struct acpi_rsdp*)addr;
        }
    }
    for (addr = (unsigned char*)RSDP_SIG_EBDA; addr < (unsigned char*)RSDP_SIG_EBDA_END; addr += 16) {
        if (*(unsigned long long*)addr == RSDP_SIGNATURE) return (struct acpi_rsdp*)addr;
    }
    for (addr = (unsigned char*)RSDP_SIG_BIOS; addr < (unsigned char*)RSDP_SIG_BIOS_END; addr += 16) {
        if (*(unsigned long long*)addr == RSDP_SIGNATURE) return (struct acpi_rsdp*)addr;
    }
    return 0;
}

static void* acpi_find_table(struct acpi_rsdp* rsdp, const char* sig)
{
    struct acpi_xsdt* xsdt;
    unsigned long i, count, entry_cnt;
    struct acpi_sdt_header* hdr;
    if (!rsdp || !sig) return 0;
    if (!rsdp->xsdt_addr) return 0;
    xsdt = (struct acpi_xsdt*)(unsigned long)rsdp->xsdt_addr;
    if (xsdt->header.length < sizeof(struct acpi_sdt_header)) return 0;
    entry_cnt = (xsdt->header.length - sizeof(struct acpi_sdt_header)) / 8;
    for (i = 0, count = 0; i < entry_cnt && count < 128; i++, count++) {
        unsigned long long addr = xsdt->entries[i];
        if (!addr) continue;
        hdr = (struct acpi_sdt_header*)(unsigned long)addr;
        if (hdr->signature[0] == sig[0] && hdr->signature[1] == sig[1] &&
            hdr->signature[2] == sig[2] && hdr->signature[3] == sig[3]) {
            return (void*)hdr;
        }
    }
    return 0;
}

static struct acpi_fadt* fadt_cache = 0;

static int acpi_read_dword(u32 addr, u32* val)
{
    if (!val) return -1;
    *val = *(volatile u32*)(unsigned long)addr;
    return 0;
}

static int acpi_write_dword(u32 addr, u32 val)
{
    *(volatile u32*)(unsigned long)addr = val;
    return 0;
}

void acpi_power_off(void)
{
    struct acpi_fadt* fadt;
    u32 pm1a_cnt, pm1b_cnt;
    int has_fadt = 0;
    fadt = fadt_cache;
    if (!fadt) {
        struct acpi_rsdp* rsdp = rsdp_find();
        if (!rsdp) return;
        fadt = (struct acpi_fadt*)acpi_find_table(rsdp, "FACP");
        if (!fadt) return;
        fadt_cache = fadt;
    }
    has_fadt = 1;
    if (fadt->smi_cmd && fadt->acpi_enable) {
        outb(fadt->smi_cmd, fadt->acpi_enable);
    }
    if (acpi_read_dword(fadt->pm1a_cnt_blk, &pm1a_cnt) != 0) return;
    pm1a_cnt |= (1 << 13);
    if (acpi_write_dword(fadt->pm1a_cnt_blk, pm1a_cnt) != 0) return;
    if (fadt->pm1b_cnt_blk) {
        if (acpi_read_dword(fadt->pm1b_cnt_blk, &pm1b_cnt) != 0) return;
        pm1b_cnt |= (1 << 13);
        if (acpi_write_dword(fadt->pm1b_cnt_blk, pm1b_cnt) != 0) return;
    }
    (void)has_fadt;
}

void acpi_reboot(void)
{
    struct acpi_fadt* fadt;
    fadt = fadt_cache;
    if (!fadt) {
        struct acpi_rsdp* rsdp = rsdp_find();
        if (!rsdp) return;
        fadt = (struct acpi_fadt*)acpi_find_table(rsdp, "FACP");
        if (!fadt) return;
        fadt_cache = fadt;
    }
    if (fadt->reset_reg.space_id == 1) {
        u32 val = fadt->reset_value;
        outb(fadt->reset_reg.address, (unsigned char)val);
    }
}

int acpi_init(void)
{
    struct acpi_rsdp* rsdp;
    rsdp = rsdp_find();
    if (!rsdp) return -1;
    (void)rsdp;
    return 0;
}
