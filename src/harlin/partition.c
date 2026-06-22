#include "partition.h"
#include "ata.h"

#define PARTITION_MAX 16

static struct partition_entry partitions[PARTITION_MAX];
static int partition_count_value = 0;

static u32 read_le32(const u8* p)
{
    return ((u32)p[0]) | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

static void add_partition(u32 start_lba, u32 sector_count, u8 type)
{
    if (partition_count_value >= PARTITION_MAX)
        return;
    partitions[partition_count_value].active = 0;
    partitions[partition_count_value].type = type;
    partitions[partition_count_value].start_lba = start_lba;
    partitions[partition_count_value].sector_count = sector_count;
    partition_count_value++;
}

static int parse_mbr(void)
{
    u8 sector[512];
    int i;
    u32 ebr_start = 0;
    int has_extended = 0;

    if (ata_read_sectors(0, 1, sector) != 0)
        return -1;
    if (sector[510] != 0x55 || sector[511] != 0xAA)
        return -1;

    for (i = 0; i < 4; i++) {
        const u8* entry = &sector[0x1BE + i * 16];
        u8 type = entry[4];
        u32 start = read_le32(&entry[8]);
        u32 count = read_le32(&entry[12]);
        if (type == PARTITION_TYPE_EMPTY)
            continue;
        if (type == 0x05 || type == 0x0F) {
            ebr_start = start;
            has_extended = 1;
            continue;
        }
        add_partition(start, count, type);
    }

    while (has_extended && partition_count_value < PARTITION_MAX) {
        u8 ebr[512];
        u32 cur;
        const u8* entry;
        u32 lba;
        u32 cnt;
        if (ata_read_sectors(ebr_start, 1, ebr) != 0)
            break;
        if (ebr[510] != 0x55 || ebr[511] != 0xAA)
            break;
        entry = &ebr[0x1BE];
        lba = read_le32(&entry[8]);
        cnt = read_le32(&entry[12]);
        if (lba && cnt)
            add_partition(ebr_start + lba, cnt, entry[4]);

        entry = &ebr[0x1CE];
        cur = read_le32(&entry[8]);
        if (!cur)
            break;
        ebr_start += cur;
    }

    return 0;
}

int partition_init(void)
{
    int i;
    partition_count_value = 0;
    for (i = 0; i < PARTITION_MAX; i++) {
        partitions[i].active = 0;
        partitions[i].type = 0;
        partitions[i].start_lba = 0;
        partitions[i].sector_count = 0;
    }
    return parse_mbr();
}

int partition_get(int index, struct partition_entry* out)
{
    if (index < 0 || index >= PARTITION_MAX || !out)
        return -1;
    *out = partitions[index];
    return 0;
}

int partition_count(void)
{
    return partition_count_value;
}
