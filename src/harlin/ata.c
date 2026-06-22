#include "ata.h"
#include "io.h"
#include "interrupt.h"
#include "pci.h"

static u16 ata_base = 0;
static u16 ata_ctrl = 0;
static int ata_present = 0;
static volatile int ata_irq_done = 0;

static u8 ata_status(void)
{
    return inb(ata_base + 7);
}

static int ata_wait_bsy(void)
{
    u32 i;
    for (i = 0; i < 1000000; i++) {
        u8 status = ata_status();
        if (status == 0xFF)
            return -1;
        if ((status & 0x80) == 0)
            return 0;
    }
    return -1;
}

static void ata_irq_handler(void)
{
    inb(ata_base + 7);
    ata_irq_done = 1;
}

static int ata_wait_transfer(void)
{
    u32 timeout = 10000000;
    unsigned long long flags;
    asm volatile ("pushfq; popq %0" : "=r"(flags) : : "memory");
    if (!(flags & (1ULL << 9))) {
        return -1;
    }
    ata_irq_done = 0;
    while (!ata_irq_done && timeout--) {
        asm volatile ("sti; hlt; cli" : : : "memory");
    }
    if (!ata_irq_done) {
        inb(ata_base + 7);
        return -1;
    }
    return 0;
}

static int ata_try_channel(u16 base, u16 ctrl, int primary)
{
    u32 i;
    u8 status;
    outb(ctrl, 0x04);
    for (i = 0; i < 1000; i++) inb(ctrl);
    outb(ctrl, 0x00);
    if (ata_wait_bsy() != 0)
        return -1;

    outb(base + 6, 0xA0);
    if (ata_wait_bsy() != 0)
        return -1;

    status = ata_status();
    if (status == 0xFF || (status & 0x40) == 0)
        return -1;

    outb(base + 7, ATA_CMD_IDENTIFY);
    if (ata_wait_transfer() != 0)
        return -1;

    for (i = 0; i < 256; i++) {
        inw(base);
    }

    ata_wait_bsy();
    if (primary)
        irq_register(14, ata_irq_handler);
    else
        irq_register(15, ata_irq_handler);
    return 0;
}

int ata_init(void)
{
    struct pci_device dev;
    u64 bar0 = 0, bar1 = 0, bar2 = 0, bar3 = 0;
    int has_pci = 0;
    u16 primary_base = ATA_PRIMARY_BASE;
    u16 primary_ctrl = ATA_PRIMARY_CTRL;
    u16 secondary_base = ATA_SECONDARY_BASE;
    u16 secondary_ctrl = ATA_SECONDARY_CTRL;

    if (pci_find_class(0x01, 0x01, &dev, 0) >= 0) {
        pci_enable_busmaster(&dev);
        if (pci_get_bar(&dev, 0, &bar0) == 0) has_pci = 1;
        if (pci_get_bar(&dev, 1, &bar1) == 0) {}
        if (pci_get_bar(&dev, 2, &bar2) == 0) {}
        if (pci_get_bar(&dev, 3, &bar3) == 0) {}
    }
    (void)bar1;
    (void)bar3;
    if (has_pci) {
        if ((bar0 & 0x1) && (u16)(bar0 & 0xFFFC) != 0) {
            primary_base = (u16)(bar0 & 0xFFFC);
        }
        if ((bar2 & 0x1) && (u16)(bar2 & 0xFFFC) != 0) {
            secondary_base = (u16)(bar2 & 0xFFFC);
        }
        if (primary_base + 0x0E != 0 && primary_ctrl == ATA_PRIMARY_CTRL) {
            primary_ctrl = (u16)((primary_base & 0xFFF8) + 0x06);
        }
        if (secondary_base + 0x0E != 0 && secondary_ctrl == ATA_SECONDARY_CTRL) {
            secondary_ctrl = (u16)((secondary_base & 0xFFF8) + 0x06);
        }
    }

    ata_base = primary_base;
    ata_ctrl = primary_ctrl;
    if (ata_try_channel(primary_base, primary_ctrl, 1) == 0) {
        ata_present = 1;
        return 0;
    }

    ata_base = secondary_base;
    ata_ctrl = secondary_ctrl;
    if (ata_try_channel(secondary_base, secondary_ctrl, 0) == 0) {
        ata_present = 1;
        return 0;
    }

    return -1;
}

int ata_is_present(void)
{
    return ata_present;
}

static int ata_do_sector(u64 lba, u8 cmd)
{
    if (lba > 0x0FFFFFFF)
        return -1;
    if (ata_wait_bsy() != 0)
        return -1;
    outb(ata_base + 6, 0xE0 | (u8)((lba >> 24) & 0x0F));
    outb(ata_base + 2, 1);
    outb(ata_base + 3, (u8)(lba & 0xFF));
    outb(ata_base + 4, (u8)((lba >> 8) & 0xFF));
    outb(ata_base + 5, (u8)((lba >> 16) & 0xFF));
    outb(ata_base + 7, cmd);
    return 0;
}

int ata_read_sectors(u64 lba, u8 count, void* buf)
{
    u16* ptr = (u16*)buf;
    u32 i;
    u32 current;
    if (!ata_present)
        return -1;
    if (!buf || count == 0)
        return -1;

    for (current = 0; current < count; current++) {
        if (ata_do_sector(lba + current, ATA_CMD_READ) != 0)
            return -1;

        if (ata_wait_transfer() != 0)
            return -1;

        for (i = 0; i < 256; i++) {
            ptr[current * 256 + i] = inw(ata_base);
        }

        ata_wait_bsy();
    }

    return 0;
}

int ata_write_sectors(u64 lba, u8 count, const void* buf)
{
    const u16* ptr = (const u16*)buf;
    u32 i;
    u32 current;
    if (!ata_present)
        return -1;
    if (!buf || count == 0)
        return -1;

    for (current = 0; current < count; current++) {
        if (ata_do_sector(lba + current, ATA_CMD_WRITE) != 0)
            return -1;

        if (ata_wait_transfer() != 0)
            return -1;

        for (i = 0; i < 256; i++) {
            outw(ata_base, ptr[current * 256 + i]);
        }

        ata_wait_bsy();
    }

    return 0;
}
