#include "partition.h"
#include "ata.h"

static struct partition_entry partitions[4];
static int partition_valid = 0;

static u32 read_le32(const u8* p)
{
    return ((u32)p[0]) | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

int partition_init(void)
{
    u8 sector[512];
    int i;

    for (i = 0; i < 4; i++) {
        partitions[i].active = 0;
        partitions[i].type = 0;
        partitions[i].start_lba = 0;
        partitions[i].sector_count = 0;
    }

    if (ata_read_sectors(0, 1, sector) != 0)
        return -1;

    if (sector[510] != 0x55 || sector[511] != 0xAA)
        return -1;

    for (i = 0; i < 4; i++) {
        const u8* entry = &sector[0x1BE + i * 16];
        partitions[i].active = entry[0];
        partitions[i].type = entry[4];
        partitions[i].start_lba = read_le32(&entry[8]);
        partitions[i].sector_count = read_le32(&entry[12]);
    }

    partition_valid = 1;
    return 0;
}

int partition_get(int index, struct partition_entry* out)
{
    if (!partition_valid || index < 0 || index >= 4 || !out)
        return -1;
    *out = partitions[index];
    return 0;
}

int partition_count(void)
{
    int i;
    int count = 0;
    if (!partition_valid)
        return 0;
    for (i = 0; i < 4; i++) {
        if (partitions[i].type != PARTITION_TYPE_EMPTY)
            count++;
    }
    return count;
}
