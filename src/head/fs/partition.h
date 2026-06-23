#ifndef PARTITION_H
#define PARTITION_H

#include "harlin_API.h"

#define PARTITION_TYPE_EMPTY  0x00
#define PARTITION_TYPE_FAT32  0x0C
#define PARTITION_TYPE_FAT32_LBA 0x0C

struct partition_entry {
    u8  active;
    u8  type;
    u32 start_lba;
    u32 sector_count;
};

int partition_init(void);
int partition_get(int index, struct partition_entry* out);
int partition_count(void);

#define Harlin_PartitionInit          partition_init
#define Harlin_PartitionGet           partition_get
#define Harlin_PartitionCount         partition_count

#endif
