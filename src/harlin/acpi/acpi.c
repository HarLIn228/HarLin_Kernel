#include "acpi.h"
#include "io.h"
#include "screen.h"
#include "vmm.h"
#include "pmm.h"

#define SLP_EN    (1U << 13)

static volatile u32 g_pm1a_cnt_blk = 0;
static u8 g_pm1_cnt_len = 0;
static u8 g_slp_typa = 0;
static u8 g_slp_typb = 0;
static u8 g_reset_value = 0;
static u64 g_reset_addr = 0;
static u8 g_reset_addr_space = 0;
static int g_acpi_available = 0;

struct rsdp_descriptor {
    u8  signature[8];
    u8  checksum;
    u8  oem_id[6];
    u8  revision;
    u32 rsdt_address;
} __attribute__((packed));

struct sdt_header {
    u8  signature[4];
    u32 length;
    u8  revision;
    u8  checksum;
    u8  oem_id[6];
    u8  oem_table_id[8];
    u32 oem_revision;
    u32 creator_id;
    u32 creator_revision;
} __attribute__((packed));

static u8 acpi_checksum(void* table, u32 length)
{
    u8 sum = 0;
    u8* p = (u8*)table;
    u32 i;
    for (i = 0; i < length; i++)
        sum += p[i];
    return sum;
}

static void* acpi_scan_rsdp(void)
{
    u32 addr;
    u16 ebda_seg;

    ebda_seg = *(u16*)(u64)0x40E;
    if (ebda_seg) {
        u32 ebda_start = (u32)ebda_seg * 16;
        for (addr = ebda_start; addr < ebda_start + 1024; addr += 16) {
            if (*(u64*)(u64)addr == *(u64*)"RSD PTR ") {
                struct rsdp_descriptor* rsdp = (struct rsdp_descriptor*)(u64)addr;
                if (acpi_checksum(rsdp, 20) == 0)
                    return rsdp;
            }
        }
    }

    for (addr = 0xE0000; addr < 0x100000; addr += 16) {
        if (*(u64*)(u64)addr == *(u64*)"RSD PTR ") {
            struct rsdp_descriptor* rsdp = (struct rsdp_descriptor*)(u64)addr;
            if (acpi_checksum(rsdp, 20) == 0)
                return rsdp;
        }
    }

    return 0;
}

static void* acpi_find_table(void* rsdt_addr, u32 rsdt_len, const char* sig)
{
    u32* entries = (u32*)((u8*)rsdt_addr + sizeof(struct sdt_header));
    u32 count = (rsdt_len - sizeof(struct sdt_header)) / 4;
    u32 i;

    for (i = 0; i < count; i++) {
        struct sdt_header* hdr = (struct sdt_header*)(u64)entries[i];
        if (hdr->signature[0] == sig[0] &&
            hdr->signature[1] == sig[1] &&
            hdr->signature[2] == sig[2] &&
            hdr->signature[3] == sig[3]) {
            return hdr;
        }
    }
    return 0;
}

static int acpi_parse_s5(u8* dsdt_base, u32 dsdt_len)
{
    u32 i;
    u8 s5_name[4] = { 0x5F, 0x53, 0x35, 0x5F };

    for (i = sizeof(struct sdt_header); i < dsdt_len - 16; i++) {
        if (dsdt_base[i] == 0x08 &&
            dsdt_base[i+1] == s5_name[0] &&
            dsdt_base[i+2] == s5_name[1] &&
            dsdt_base[i+3] == s5_name[2] &&
            dsdt_base[i+4] == s5_name[3]) {

            u32 j = i + 5;
            if (j >= dsdt_len) break;
            if (dsdt_base[j] != 0x12) break;
            j++;
            if (j >= dsdt_len) break;
            if (dsdt_base[j] & 0x80) {
                u32 pkg_len = dsdt_base[j] & 0x0F;
                j++;
                if (pkg_len == 0) {
                    if (j >= dsdt_len) break;
                    pkg_len = dsdt_base[j];
                    j++;
                }
            } else {
                j++;
            }
            if (j >= dsdt_len) break;
            u8 num_elem = dsdt_base[j];
            j++;
            if (num_elem >= 2 && j + 3 < dsdt_len) {
                if ((dsdt_base[j] & 0xF0) == 0x00) {
                    g_slp_typa = dsdt_base[j] & 0x0F;
                    j++;
                } else if (dsdt_base[j] == 0x0A) {
                    j++;
                    if (j >= dsdt_len) break;
                    g_slp_typa = dsdt_base[j];
                    j++;
                } else {
                    break;
                }
                if (j >= dsdt_len) break;
                if ((dsdt_base[j] & 0xF0) == 0x00) {
                    g_slp_typb = dsdt_base[j] & 0x0F;
                } else if (dsdt_base[j] == 0x0A) {
                    j++;
                    if (j >= dsdt_len) break;
                    g_slp_typb = dsdt_base[j];
                }
                return 1;
            }
            break;
        }
    }
    return 0;
}

int acpi_init(void)
{
    static int acpi_initialized = 0;
    struct rsdp_descriptor* rsdp;
    struct sdt_header* rsdt_hdr;
    struct sdt_header* fadt_hdr;

    if (acpi_initialized)
        return 0;
    acpi_initialized = 1;

    rsdp = (struct rsdp_descriptor*)acpi_scan_rsdp();
    if (!rsdp) {
        screen_puts("[acpi] rsdp not found\n");
        return -1;
    }
    screen_puts("[acpi] rsdp found\n");

    rsdt_hdr = (struct sdt_header*)(u64)rsdp->rsdt_address;
    if (acpi_checksum(rsdt_hdr, rsdt_hdr->length) != 0) {
        screen_puts("[acpi] rsdt checksum error\n");
        return -1;
    }

    fadt_hdr = (struct sdt_header*)acpi_find_table(rsdt_hdr, rsdt_hdr->length, "FACP");
    if (!fadt_hdr) {
        screen_puts("[acpi] fadt not found\n");
        return -1;
    }
    screen_puts("[acpi] fadt found\n");

    if (acpi_checksum(fadt_hdr, fadt_hdr->length) != 0) {
        screen_puts("[acpi] fadt checksum error\n");
        return -1;
    }

    {
        u8* fadt = (u8*)fadt_hdr;
        u32 pm1a_cnt = *(u32*)(fadt + 64);
        u32 dsdt_addr = *(u32*)(fadt + 40);
        u8  cnt_len = *(u8*)(fadt + 89);
        u8* reset_reg = fadt + 116;

        g_pm1a_cnt_blk = pm1a_cnt;
        g_pm1_cnt_len = cnt_len ? cnt_len : 2;

        g_reset_addr_space = reset_reg[0];
        g_reset_value = *(u8*)(fadt + 128);
        g_reset_addr = *(u64*)(reset_reg + 4);

        if (dsdt_addr) {
            u32 dsdt_len_field = 0;
            int dsdt_mapped = 0;
            {
                u8* tmp = (u8*)(u64)dsdt_addr;
                if (vmm_mapped((u64)tmp)) {
                    dsdt_len_field = *(u32*)(tmp + 4);
                }
            }
            if (dsdt_len_field == 0) {
                u32 pages = 1;
                u64 phys = dsdt_addr & ~0xFFFULL;
                u64 virt_base = ACPI_TABLE_VIRT;
                if (vmm_map(virt_base, phys, VMM_PRESENT | VMM_WRITABLE) == 0) {
                    dsdt_mapped = 1;
                    {
                        u8* tmp = (u8*)(virt_base + (dsdt_addr & 0xFFF));
                        dsdt_len_field = *(u32*)(tmp + 4);
                    }
                    if (dsdt_len_field > 4096) {
                        u32 extra = (dsdt_len_field + 0xFFF) / 4096;
                        u32 i;
                        int map_ok = 1;
                        for (i = 1; i < extra; i++) {
                            if (virt_base + i * 4096 < ACPI_TABLE_END) {
                                if (vmm_map(virt_base + i * 4096, phys + i * 4096, VMM_PRESENT | VMM_WRITABLE) != 0) {
                                    map_ok = 0;
                                    break;
                                }
                            }
                        }
                        if (!map_ok) {
                            screen_puts("[acpi] dsdt map partial fail, using defaults\n");
                            g_slp_typa = 0x00;
                            g_slp_typb = 0x00;
                            dsdt_len_field = 0;
                        }
                        (void)pages;
                    }
                } else {
                    screen_puts("[acpi] dsdt map failed, using defaults\n");
                    g_slp_typa = 0x00;
                    g_slp_typb = 0x00;
                }
            }
            if (dsdt_len_field > 0 && dsdt_mapped) {
                u8* dsdt_base = (u8*)(ACPI_TABLE_VIRT + (dsdt_addr & 0xFFF));
                if (acpi_checksum(dsdt_base, dsdt_len_field) == 0) {
                    if (acpi_parse_s5(dsdt_base, dsdt_len_field)) {
                        screen_puts("[acpi] s5 package parsed\n");
                    } else {
                        screen_puts("[acpi] s5 not found, using defaults\n");
                        g_slp_typa = 0x00;
                        g_slp_typb = 0x00;
                    }
                } else {
                    screen_puts("[acpi] dsdt checksum error, using defaults\n");
                    g_slp_typa = 0x00;
                    g_slp_typb = 0x00;
                }
            }
        } else {
            screen_puts("[acpi] no dsdt, using defaults\n");
            g_slp_typa = 0x00;
            g_slp_typb = 0x00;
        }
    }

    g_acpi_available = 1;
    screen_puts("[acpi] init ok\n");
    return 0;
}

void acpi_power_off(void)
{
    u32 val;

    if (!g_acpi_available) {
        screen_puts("[acpi] not available\n");
        return;
    }

    screen_puts("[acpi] power off\n");
    cli();

    val = inl(g_pm1a_cnt_blk);
    val &= ~(0x1FFF);
    val |= (u32)g_slp_typa | SLP_EN;
    outl(g_pm1a_cnt_blk, val);

    if (g_pm1_cnt_len >= 2 && g_pm1a_cnt_blk + 4 != g_pm1a_cnt_blk) {
        val = inl(g_pm1a_cnt_blk + 4);
        val &= ~(0x1FFF);
        val |= (u32)g_slp_typb | SLP_EN;
        outl(g_pm1a_cnt_blk + 4, val);
    }

    for (;;) asm volatile ("hlt");
}

void acpi_reboot(void)
{
    cli();
    screen_puts("[acpi] reboot\n");

    if (g_acpi_available && g_reset_addr && g_reset_addr_space == 1) {
        outb((u16)g_reset_addr, g_reset_value);
        for (;;) asm volatile ("hlt");
    }

    outb(0x64, 0xFE);
    for (;;) asm volatile ("hlt");
}
