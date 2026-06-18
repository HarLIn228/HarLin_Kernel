#include "ata.h"
#include "io.h"

static u16 ata_base = ATA_PRIMARY_BASE;
static u16 ata_ctrl = ATA_PRIMARY_CTRL;

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

static int ata_wait_drdy(void)
{
    u32 i;
    for (i = 0; i < 1000000; i++) {
        u8 status = ata_status();
        if (status == 0xFF)
            return -1;
        if (status & 0x40)
            return 0;
    }
    return -1;
}

static int ata_poll_drq(void)
{
    u32 i;
    u8 status;
    for (i = 0; i < 1000000; i++) {
        status = ata_status();
        if (status == 0xFF)
            return -1;
        if (status & 0x01)
            return -1;
        if (status & 0x20)
            return -1;
        if ((status & 0x88) == 0x08)
            return 0;
    }
    return -1;
}

int ata_init(void)
{
    u32 i;
    u8 status;

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
    if (ata_poll_drq() != 0)
        return -1;

    for (i = 0; i < 256; i++) {
        inw(ata_base);
    }

    ata_wait_bsy();
    return 0;
}

static int ata_do_sector(u32 lba, u8 cmd)
{
    if (ata_wait_bsy() != 0)
        return -1;
    outb(ata_base + 6, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ata_base + 2, 1);
    outb(ata_base + 3, lba & 0xFF);
    outb(ata_base + 4, (lba >> 8) & 0xFF);
    outb(ata_base + 5, (lba >> 16) & 0xFF);
    outb(ata_base + 7, cmd);
    return ata_poll_drq();
}

int ata_read_sectors(u64 lba, u8 count, void* buf)
{
    u16* ptr = (u16*)buf;
    u32 i;
    u32 current;

    if (count == 0)
        return 0;

    for (current = 0; current < count; current++) {
        if (ata_do_sector((u32)(lba + current), ATA_CMD_READ) != 0)
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
        if (ata_do_sector((u32)(lba + current), ATA_CMD_WRITE) != 0)
            return -1;

        for (i = 0; i < 256; i++) {
            outw(ata_base, ptr[current * 256 + i]);
        }

        outb(ata_base + 7, 0xE7);
        ata_wait_bsy();
    }

    return 0;
}
