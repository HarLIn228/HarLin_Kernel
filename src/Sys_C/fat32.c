#include "fat32.h"
#include "ata.h"

#define FAT32_EOC      0x0FFFFFF8
#define FAT32_BAD      0x0FFFFFF7
#define FAT32_DELETED  0xE5
#define FAT32_LFN      0x0F

static u32 fs_partition_lba = 0;
static u16 bytes_per_sector = 512;
static u8  sectors_per_cluster = 1;
static u16 reserved_sectors = 0;
static u8  number_of_fats = 2;
static u32 sectors_per_fat = 0;
static u32 root_cluster = 2;
static u32 first_data_sector = 0;
static u32 fat_start_sector = 0;

static u8 sector_buf[512];
static u8 cluster_buf[4096];

static u32 read_le32(const u8* p)
{
    return ((u32)p[0]) | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

static u16 read_le16(const u8* p)
{
    return ((u16)p[0]) | ((u16)p[1] << 8);
}

static u32 cluster_to_sector(u32 cluster)
{
    return first_data_sector + ((cluster - 2) * sectors_per_cluster);
}

static int read_sector(u32 lba, void* buf)
{
    return ata_read_sectors(fs_partition_lba + lba, 1, buf);
}

static u32 next_cluster(u32 cluster)
{
    u32 fat_offset = cluster * 4;
    u32 fat_sector = fat_offset / bytes_per_sector;
    u32 entry_offset = fat_offset % bytes_per_sector;
    u32 next;

    if (read_sector(fat_start_sector + fat_sector, sector_buf) != 0)
        return FAT32_EOC;

    next = read_le32(&sector_buf[entry_offset]) & 0x0FFFFFFF;
    return next;
}

int Harlin_FsMount(u32 partition_lba)
{
    fs_partition_lba = partition_lba;

    if (read_sector(0, sector_buf) != 0)
        return HARLIN_FS_ERROR;

    if (sector_buf[510] != 0x55 || sector_buf[511] != 0xAA)
        return HARLIN_FS_ERROR;

    bytes_per_sector = read_le16(&sector_buf[0x0B]);
    sectors_per_cluster = sector_buf[0x0D];
    reserved_sectors = read_le16(&sector_buf[0x0E]);
    number_of_fats = sector_buf[0x10];
    sectors_per_fat = read_le32(&sector_buf[0x24]);
    root_cluster = read_le32(&sector_buf[0x2C]);

    if (bytes_per_sector != 512 || sectors_per_cluster == 0 || sectors_per_cluster > 8)
        return HARLIN_FS_ERROR;

    if ((u32)sectors_per_cluster * bytes_per_sector > sizeof(cluster_buf))
        return HARLIN_FS_ERROR;

    fat_start_sector = reserved_sectors;
    first_data_sector = reserved_sectors + (number_of_fats * sectors_per_fat);

    return HARLIN_FS_OK;
}

static int read_cluster(u32 cluster, void* buf)
{
    u32 sector = cluster_to_sector(cluster);
    return ata_read_sectors(fs_partition_lba + sector, sectors_per_cluster, buf);
}

static int filename_match(const char* name, const u8* entry)
{
    char short_name[13];
    int i;
    int pos;

    if (entry[0] == FAT32_DELETED || entry[0] == 0)
        return 0;
    if ((entry[11] & FAT32_LFN) == FAT32_LFN)
        return 0;
    if ((entry[11] & 0x08) || (entry[11] & 0x10))
        return 0;

    pos = 0;
    for (i = 0; i < 8 && entry[i] != ' '; i++) {
        short_name[pos++] = entry[i];
    }
    if (entry[8] != ' ') {
        short_name[pos++] = '.';
        short_name[pos++] = entry[8];
        if (entry[9] != ' ') short_name[pos++] = entry[9];
        if (entry[10] != ' ') short_name[pos++] = entry[10];
    }
    short_name[pos] = '\0';

    return Harlin_StrCmp(name, short_name) == 0;
}

int Harlin_FsOpen(const char* name, struct Harlin_File* out)
{
    u32 cluster = root_cluster;
    u32 i;

    if (!out)
        return HARLIN_FS_ERROR;

    while (cluster < FAT32_EOC) {
        if (read_cluster(cluster, cluster_buf) != 0)
            return HARLIN_FS_ERROR;

        for (i = 0; i < bytes_per_sector * sectors_per_cluster; i += 32) {
            u8* entry = &cluster_buf[i];

            if (entry[0] == 0)
                return HARLIN_FS_ERROR;

            if (filename_match(name, entry)) {
                out->start_cluster = ((u32)read_le16(&entry[0x14]) << 16) | read_le16(&entry[0x1A]);
                out->current_cluster = out->start_cluster;
                out->position = 0;
                out->size = read_le32(&entry[0x1C]);
                return HARLIN_FS_OK;
            }
        }

        cluster = next_cluster(cluster);
    }

    return HARLIN_FS_ERROR;
}

int Harlin_FsRead(struct Harlin_File* file, void* buf, u32 len)
{
    u8* dst = (u8*)buf;
    u32 remaining = len;
    u32 cluster_size = bytes_per_sector * sectors_per_cluster;

    if (!file || !buf)
        return HARLIN_FS_ERROR;

    if (file->position >= file->size)
        return HARLIN_FS_EOF;

    if (file->position + remaining > file->size)
        remaining = file->size - file->position;

    while (remaining > 0 && file->current_cluster < FAT32_EOC) {
        u32 cluster_pos = file->position % cluster_size;
        u32 chunk = cluster_size - cluster_pos;

        if (chunk > remaining)
            chunk = remaining;

        if (read_cluster(file->current_cluster, cluster_buf) != 0)
            return HARLIN_FS_ERROR;

        Harlin_MemCopy(dst, &cluster_buf[cluster_pos], chunk);

        dst += chunk;
        file->position += chunk;
        remaining -= chunk;

        if (file->position % cluster_size == 0) {
            file->current_cluster = next_cluster(file->current_cluster);
        }
    }

    return (int)(len - remaining);
}

u32 Harlin_FsSize(struct Harlin_File* file)
{
    if (!file)
        return 0;
    return file->size;
}

void Harlin_FsClose(struct Harlin_File* file)
{
    if (file) {
        file->start_cluster = 0;
        file->current_cluster = 0;
        file->position = 0;
        file->size = 0;
    }
}
