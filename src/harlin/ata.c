#include "ata.h"
#include "io.h"
#include "interrupt.h"

static u16 ata_base = ATA_PRIMARY_BASE;
static u16 ata_ctrl = ATA_PRIMARY_CTRL;
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
    ata_irq_done = 0;
    while (!ata_irq_done && timeout--) {
        asm volatile ("hlt" : : : "memory");
    }
    if (!ata_irq_done) {
        inb(ata_base + 7);
        return -1;
    }
    return 0;
}

int ata_init(void)
{
    u32 i;
    u8 status;

    irq_register(14, ata_irq_handler);

    outb(ata_ctrl, 0x04);
    for (i = 0; i < 1000; i++) inb(ata_ctrl);
    outb(ata_ctrl, 0x00);
    if (ata_wait_bsy() != 0)
        return -1;

    outb(ata_base + 6, 0xA0);
    if (ata_wait_bsy() != 0)
        return -1;

    status = ata_status();
    if (status == 0xFF || (status & 0x40) == 0)
        return -1;

    outb(ata_base + 7, ATA_CMD_IDENTIFY);
    if (ata_wait_transfer() != 0)
        return -1;

    for (i = 0; i < 256; i++) {
        inw(ata_base);
    }

    ata_wait_bsy();
    return 0;
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

    if (count == 0)
        return 0;

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

    if (count == 0)
        return 0;

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
