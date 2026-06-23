#ifndef ATA_H
#define ATA_H

#include "harlin_API.h"

#define ATA_PRIMARY_BASE   0x1F0
#define ATA_PRIMARY_CTRL   0x3F6
#define ATA_SECONDARY_BASE 0x170
#define ATA_SECONDARY_CTRL 0x376

#define ATA_CMD_READ  0x20
#define ATA_CMD_WRITE 0x30
#define ATA_CMD_IDENTIFY 0xEC

int ata_init(void);
int ata_read_sectors(u64 lba, u8 count, void* buf);
int ata_write_sectors(u64 lba, u8 count, const void* buf);
int ata_is_present(void);

#endif
