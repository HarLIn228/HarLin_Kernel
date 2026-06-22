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
static u32 current_dir_cluster = 2;
static char lfn_name[256];
static char cwd_path[256];

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

static int write_sector(u32 lba, const void* buf)
{
    return ata_write_sectors(fs_partition_lba + lba, 1, buf);
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

int Harlin_Mount(u32 partition_lba)
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

static int write_cluster(u32 cluster, const void* buf)
{
    u32 sector = cluster_to_sector(cluster);
    return ata_write_sectors(fs_partition_lba + sector, sectors_per_cluster, buf);
}

static void write_le32(u8* p, u32 val)
{
    p[0] = (u8)(val);
    p[1] = (u8)(val >> 8);
    p[2] = (u8)(val >> 16);
    p[3] = (u8)(val >> 24);
}

static void write_le16(u8* p, u16 val)
{
    p[0] = (u8)(val);
    p[1] = (u8)(val >> 8);
}

static int write_fat_entry(u32 cluster, u32 value)
{
    u32 fat_offset = cluster * 4;
    u32 fat_sector = fat_offset / bytes_per_sector;
    u32 entry_offset = fat_offset % bytes_per_sector;
    u8 fat;

    for (fat = 0; fat < number_of_fats; fat++) {
        u32 lba = fat_start_sector + (fat * sectors_per_fat) + fat_sector;
        if (read_sector(lba, sector_buf) != 0)
            return -1;
        write_le32(&sector_buf[entry_offset], value & 0x0FFFFFFF);
        if (write_sector(lba, sector_buf) != 0)
            return -1;
    }
    return 0;
}

static u32 find_free_cluster(void)
{
    u32 cluster = 2;
    u32 total_clusters = (sectors_per_fat * bytes_per_sector) / 4;

    while (cluster < total_clusters) {
        u32 fat_offset = cluster * 4;
        u32 fat_sector = fat_offset / bytes_per_sector;
        u32 entry_offset = fat_offset % bytes_per_sector;
        u32 entry;

        if (read_sector(fat_start_sector + fat_sector, sector_buf) != 0)
            return 0;

        entry = read_le32(&sector_buf[entry_offset]) & 0x0FFFFFFF;
        if (entry == 0)
            return cluster;
        cluster++;
    }
    return 0;
}

static int allocate_cluster(u32* new_cluster)
{
    u32 cluster = find_free_cluster();
    if (!cluster)
        return -1;
    if (write_fat_entry(cluster, FAT32_EOC) != 0)
        return -1;
    *new_cluster = cluster;
    return 0;
}

static int link_cluster(u32 prev, u32 next)
{
    return write_fat_entry(prev, next);
}

static int is_dir_entry(const u8* entry)
{
    return (entry[11] & 0x10) != 0;
}

static int collect_lfn(const u8* entries, int entry_index, int entries_per_cluster, char* lfn_out)
{
    int i;
    int lfn_pos = 0;
    int order = entries[entry_index * 32] & 0x3F;
    int last_order = order;

    if ((entries[entry_index * 32] & 0x40) == 0)
        return 0;

    lfn_out[0] = 0;

    while (order > 0) {
        int idx = entry_index;
        const u8* e = &entries[idx * 32];

        if (e[0] == 0 || e[11] != FAT32_LFN)
            return 0;

        if ((e[0] & 0x3F) != order)
            return 0;

        for (i = 0; i < 5; i++) {
            u16 ch = read_le16(&e[1 + i * 2]);
            if (ch >= 0x20 && ch <= 0x7E && lfn_pos < 255)
                lfn_out[lfn_pos++] = (char)ch;
        }
        for (i = 0; i < 6; i++) {
            u16 ch = read_le16(&e[14 + i * 2]);
            if (ch >= 0x20 && ch <= 0x7E && lfn_pos < 255)
                lfn_out[lfn_pos++] = (char)ch;
        }
        for (i = 0; i < 2; i++) {
            u16 ch = read_le16(&e[28 + i * 2]);
            if (ch >= 0x20 && ch <= 0x7E && lfn_pos < 255)
                lfn_out[lfn_pos++] = (char)ch;
        }

        lfn_out[lfn_pos] = 0;

        if (order == 1)
            break;

        entry_index--;
        if (entry_index < 0)
            return 0;
        order--;
    }

    if (last_order == order)
        return 0;

    return 1;
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
    if ((entry[11] & 0x08))
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

    return Harlin_Compare(name, short_name) == 0;
}

static int find_entry_in_dir(u32 dir_cluster, const char* name, u32* out_cluster, u32* out_offset, u32* out_file_cluster, u32* out_file_size, u8* out_attr)
{
    u32 cluster = dir_cluster;
    u32 entries_per_cluster = (bytes_per_sector * sectors_per_cluster) / 32;

    while (cluster < FAT32_EOC) {
        u32 i;
        if (read_cluster(cluster, cluster_buf) != 0)
            return -1;

        for (i = 0; i < entries_per_cluster; i++) {
            u8* entry = &cluster_buf[i * 32];

            if (entry[0] == 0)
                return -1;

            if (entry[11] == FAT32_LFN)
                continue;

            if (entry[0] == FAT32_DELETED)
                continue;

            if (filename_match(name, entry)) {
                if (out_cluster) *out_cluster = cluster;
                if (out_offset) *out_offset = i * 32;
                if (out_file_cluster)
                    *out_file_cluster = ((u32)read_le16(&entry[0x14]) << 16) | read_le16(&entry[0x1A]);
                if (out_file_size)
                    *out_file_size = read_le32(&entry[0x1C]);
                if (out_attr) *out_attr = entry[11];
                return 0;
            }

            if ((entry[0] & 0x40) && entry[11] == FAT32_LFN) {
                int lfn_start = i;
                while (i + 1 < entries_per_cluster) {
                    if (cluster_buf[(i + 1) * 32] != 0 && (cluster_buf[(i + 1) * 32] & 0x3F) == ((cluster_buf[i * 32] & 0x3F) - 1) && cluster_buf[(i + 1) * 32 + 11] == FAT32_LFN)
                        i++;
                    else
                        break;
                }
                i++;
                if (i < entries_per_cluster) {
                    u8* sfn = &cluster_buf[i * 32];
                    if (sfn[0] != 0 && sfn[0] != FAT32_DELETED && (sfn[11] & FAT32_LFN) != FAT32_LFN && !(sfn[11] & 0x08)) {
                        if (collect_lfn(cluster_buf, i, entries_per_cluster, lfn_name)) {
                            if (Harlin_Compare(name, lfn_name) == 0) {
                                if (out_cluster) *out_cluster = cluster;
                                if (out_offset) *out_offset = i * 32;
                                if (out_file_cluster)
                                    *out_file_cluster = ((u32)read_le16(&sfn[0x14]) << 16) | read_le16(&sfn[0x1A]);
                                if (out_file_size)
                                    *out_file_size = read_le32(&sfn[0x1C]);
                                if (out_attr) *out_attr = sfn[11];
                                return 0;
                            }
                        }
                    }
                }
            }
        }

        cluster = next_cluster(cluster);
    }
    return -1;
}

static int resolve_path(const char* path, u32* dir_cluster_out, const char** name_out)
{
    u32 cur = root_cluster;
    char kbuf[256];
    char* comp;
    char* next;
    int has_slash = 0;
    const char* last_sep;
    const char* p;

    if (!path || !path[0])
        return -1;

    if (path[0] == '/')
        cur = root_cluster;
    else
        cur = current_dir_cluster;

    Harlin_CopyStr(kbuf, path);

    comp = kbuf;
    if (comp[0] == '/')
        comp++;

    last_sep = 0;
    for (p = path; *p; p++) {
        if (*p == '/')
            last_sep = p;
    }

    if (last_sep) {
        *name_out = last_sep + 1;
        has_slash = 1;
    } else {
        if (path[0] == '/')
            *name_out = path + 1;
        else
            *name_out = path;
    }

    if (!has_slash) {
        *dir_cluster_out = cur;
        return 0;
    }

    for (;;) {
        char* slash;
        if (comp[0] == 0) {
            *dir_cluster_out = cur;
            *name_out = 0;
            return 0;
        }

        slash = comp;
        while (*slash && *slash != '/') slash++;

        if (*slash == '/') {
            *slash = 0;
            next = slash + 1;
        } else {
            next = 0;
        }

        if (comp[0] != 0 && Harlin_Compare(comp, ".") != 0) {
            if (Harlin_Compare(comp, "..") == 0) {
                u32 parent_cluster;
                if (cur == root_cluster) {
                    comp = next;
                    continue;
                }
                if (read_cluster(cur, cluster_buf) != 0)
                    return -1;
                parent_cluster = ((u32)read_le16(&cluster_buf[0x14]) << 16) | read_le16(&cluster_buf[0x1A]);
                cur = parent_cluster;
            } else {
                u32 found_cluster;
                u8 found_attr;
                if (find_entry_in_dir(cur, comp, 0, 0, &found_cluster, 0, &found_attr) != 0)
                    return -1;
                if (!(found_attr & 0x10))
                    return -1;
                cur = found_cluster;
            }
        }

        if (!next) {
            *dir_cluster_out = cur;
            *name_out = 0;
            return 0;
        }

        comp = next;
    }
}

int Harlin_Open(const char* name, struct Harlin_File* out)
{
    u32 dir_cluster;
    const char* fname;
    u32 file_cluster;
    u32 file_size;
    u32 entry_cluster;
    u32 entry_offset;
    u8 attr;

    if (!out || !name)
        return HARLIN_FS_ERROR;

    if (resolve_path(name, &dir_cluster, &fname) != 0)
        return HARLIN_FS_ERROR;

    if (!fname)
        return HARLIN_FS_ERROR;

    if (find_entry_in_dir(dir_cluster, fname, &entry_cluster, &entry_offset, &file_cluster, &file_size, &attr) != 0)
        return HARLIN_FS_ERROR;

    if (attr & 0x10)
        return HARLIN_FS_ERROR;

    out->start_cluster = file_cluster;
    out->current_cluster = file_cluster;
    out->position = 0;
    out->size = file_size;
    out->dir_cluster = entry_cluster;
    out->dir_offset = entry_offset;
    return HARLIN_FS_OK;
}

int Harlin_Read(struct Harlin_File* file, void* buf, u32 len)
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

        Harlin_Copy(dst, &cluster_buf[cluster_pos], chunk);

        dst += chunk;
        file->position += chunk;
        remaining -= chunk;

        if (file->position % cluster_size == 0) {
            file->current_cluster = next_cluster(file->current_cluster);
        }
    }

    return (int)(len - remaining);
}

u32 Harlin_Size(struct Harlin_File* file)
{
    if (!file)
        return 0;
    return file->size;
}

void Harlin_Close(struct Harlin_File* file)
{
    if (file) {
        file->start_cluster = 0;
        file->current_cluster = 0;
        file->position = 0;
        file->size = 0;
        file->dir_cluster = 0;
        file->dir_offset = 0;
    }
}

static int extend_file_clusters(struct Harlin_File* file, u32 total_clusters_needed)
{
    u32 cluster = file->start_cluster;
    u32 current_clusters = 0;
    u32 last_cluster = 0;
    u32 walked = 0;
    const u32 max_walk = 0x10000000;

    if (!cluster || cluster >= FAT32_EOC) {
        u32 new_cluster;
        if (allocate_cluster(&new_cluster) != 0)
            return -1;
        file->start_cluster = new_cluster;
        file->current_cluster = new_cluster;
        cluster = new_cluster;
        current_clusters = 1;
        if (current_clusters >= total_clusters_needed)
            return 0;
    }

    while (cluster < FAT32_EOC) {
        u32 next;
        if (++walked > max_walk)
            return -1;
        next = next_cluster(cluster);
        if (next == cluster)
            return -1;
        last_cluster = cluster;
        current_clusters++;
        if (next >= FAT32_EOC)
            break;
        cluster = next;
    }

    while (current_clusters < total_clusters_needed) {
        u32 new_cluster;
        if (allocate_cluster(&new_cluster) != 0)
            return -1;
        if (link_cluster(last_cluster, new_cluster) != 0)
            return -1;
        last_cluster = new_cluster;
        current_clusters++;
    }

    return 0;
}

static int update_directory_size(struct Harlin_File* file)
{
    if (!file->dir_cluster)
        return 0;
    if (read_cluster(file->dir_cluster, cluster_buf) != 0)
        return -1;
    write_le32(&cluster_buf[file->dir_offset + 0x1C], file->size);
    if (write_cluster(file->dir_cluster, cluster_buf) != 0)
        return -1;
    return 0;
}

int Harlin_Write(struct Harlin_File* file, const void* buf, u32 len)
{
    const u8* src = (const u8*)buf;
    u32 remaining = len;
    u32 cluster_size = bytes_per_sector * sectors_per_cluster;
    u32 total_clusters_needed;

    if (!file || !buf)
        return HARLIN_FS_ERROR;

    if (len == 0)
        return 0;

    total_clusters_needed = (file->position + len + cluster_size - 1) / cluster_size;

    if (extend_file_clusters(file, total_clusters_needed) != 0)
        return HARLIN_FS_ERROR;

    if (file->current_cluster == 0 || file->current_cluster >= FAT32_EOC)
        file->current_cluster = file->start_cluster;

    while (remaining > 0 && file->current_cluster < FAT32_EOC) {
        u32 cluster_pos = file->position % cluster_size;
        u32 chunk = cluster_size - cluster_pos;

        if (chunk > remaining)
            chunk = remaining;

        if (chunk < cluster_size) {
            if (read_cluster(file->current_cluster, cluster_buf) != 0)
                return HARLIN_FS_ERROR;
        } else {
            int i;
            for (i = 0; i < (int)cluster_size; i++)
                cluster_buf[i] = 0;
        }

        Harlin_Copy(&cluster_buf[cluster_pos], src, chunk);

        if (write_cluster(file->current_cluster, cluster_buf) != 0)
            return HARLIN_FS_ERROR;

        src += chunk;
        file->position += chunk;
        remaining -= chunk;

        if (remaining > 0) {
            file->current_cluster = next_cluster(file->current_cluster);
        }
    }

    if (file->position > file->size)
        file->size = file->position;

    update_directory_size(file);

    return (int)(len - remaining);
}

static void to_upper(char* dst, const char* src, int max)
{
    int i;
    for (i = 0; i < max && src[i]; i++) {
        char c = src[i];
        if (c >= 'a' && c <= 'z')
            c = (char)(c - 'a' + 'A');
        dst[i] = c;
    }
    for (; i < max; i++)
        dst[i] = ' ';
    dst[max] = '\0';
}

static int make_short_name(const char* name, u8* out)
{
    char base[9];
    char ext[4];
    const char* dot;
    int i, j;

    for (i = 0; name[i]; i++) {
        if (name[i] >= 'A' && name[i] <= 'Z') continue;
        if (name[i] >= 'a' && name[i] <= 'z') continue;
        if (name[i] >= '0' && name[i] <= '9') continue;
        if (name[i] == '~' || name[i] == '_' || name[i] == '-' || name[i] == '.') continue;
        return -1;
    }

    dot = 0;
    for (i = 0; name[i]; i++) {
        if (name[i] == '.') dot = &name[i];
    }

    if (dot) {
        int base_len = (int)(dot - name);
        int ext_len = 0;
        for (i = 0; dot[i + 1] && ext_len < 3; i++)
            ext_len++;
        if (base_len > 8 || ext_len > 3)
            return -1;
        to_upper(base, name, 8);
        to_upper(ext, dot + 1, 3);
    } else {
        if (i > 8)
            return -1;
        to_upper(base, name, 8);
        to_upper(ext, "", 3);
    }

    j = 0;
    for (i = 0; i < 8; i++)
        out[j++] = (u8)base[i];
    for (i = 0; i < 3; i++)
        out[j++] = (u8)ext[i];
    return 0;
}

static int find_directory_slot_in(u32 dcluster, u32* out_cluster, u32* out_offset)
{
    u32 cluster = dcluster;

    while (cluster < FAT32_EOC) {
        u32 i;
        if (read_cluster(cluster, cluster_buf) != 0)
            return -1;
        for (i = 0; i < bytes_per_sector * sectors_per_cluster; i += 32) {
            u8* entry = &cluster_buf[i];
            if (entry[0] == 0 || entry[0] == FAT32_DELETED) {
                *out_cluster = cluster;
                *out_offset = i;
                return 0;
            }
        }
        cluster = next_cluster(cluster);
    }
    return -1;
}

int Harlin_Create(const char* name, struct Harlin_File* out)
{
    u8 short_name[11];
    u32 cluster;
    u32 dir_cluster, dir_offset;
    u32 parent_dir;
    const char* fname;
    u8 attr;

    if (!name || !out)
        return HARLIN_FS_ERROR;

    if (resolve_path(name, &parent_dir, &fname) != 0)
        return HARLIN_FS_ERROR;

    if (!fname)
        return HARLIN_FS_ERROR;

    if (make_short_name(fname, short_name) != 0)
        return HARLIN_FS_ERROR;

    if (find_entry_in_dir(parent_dir, fname, 0, 0, 0, 0, &attr) == 0 && !(attr & 0x10))
        return HARLIN_FS_ERROR;

    if (find_directory_slot_in(parent_dir, &dir_cluster, &dir_offset) != 0)
        return HARLIN_FS_ERROR;
    if (allocate_cluster(&cluster) != 0)
        return HARLIN_FS_ERROR;

    {
        int i;
        for (i = 0; i < (int)(bytes_per_sector * sectors_per_cluster); i++)
            cluster_buf[i] = 0;
        if (write_cluster(cluster, cluster_buf) != 0)
            return HARLIN_FS_ERROR;
    }

    if (read_cluster(dir_cluster, cluster_buf) != 0)
        return HARLIN_FS_ERROR;

    {
        u8* entry = &cluster_buf[dir_offset];
        int i;
        for (i = 0; i < 11; i++)
            entry[i] = short_name[i];
        entry[11] = 0x20;
        entry[12] = 0;
        entry[13] = 0;
        write_le16(&entry[14], 0);
        write_le16(&entry[16], 0);
        write_le16(&entry[18], 0);
        write_le16(&entry[20], 0);
        write_le16(&entry[22], 0);
        write_le16(&entry[24], 0);
        write_le16(&entry[26], (u16)(cluster >> 16));
        write_le16(&entry[28], 0);
        write_le16(&entry[30], (u16)(cluster & 0xFFFF));
        write_le32(&entry[0x1C], 0);
    }

    if (write_cluster(dir_cluster, cluster_buf) != 0)
        return HARLIN_FS_ERROR;

    out->start_cluster = cluster;
    out->current_cluster = cluster;
    out->position = 0;
    out->size = 0;
    out->dir_cluster = dir_cluster;
    out->dir_offset = dir_offset;
    return HARLIN_FS_OK;
}

int Harlin_DeleteFile(const char* name)
{
    u32 dir_cluster;
    const char* fname;
    u32 entry_cluster;
    u32 entry_offset;
    u32 file_cluster;
    u8 attr;

    if (!name)
        return HARLIN_FS_ERROR;

    if (resolve_path(name, &dir_cluster, &fname) != 0)
        return HARLIN_FS_ERROR;

    if (!fname)
        return HARLIN_FS_ERROR;

    if (find_entry_in_dir(dir_cluster, fname, &entry_cluster, &entry_offset, &file_cluster, 0, &attr) != 0)
        return HARLIN_FS_ERROR;

    if (attr & 0x10)
        return HARLIN_FS_ERROR;

    if (read_cluster(entry_cluster, cluster_buf) != 0)
        return HARLIN_FS_ERROR;

    {
        u8* entry = &cluster_buf[entry_offset];
        u8 k;
        entry[0] = FAT32_DELETED;
        for (k = 1; k < 32; k++)
            entry[k] = 0;
        if (write_cluster(entry_cluster, cluster_buf) != 0)
            return HARLIN_FS_ERROR;

        while (file_cluster >= 2 && file_cluster < FAT32_EOC) {
            u32 next = next_cluster(file_cluster);
            write_fat_entry(file_cluster, 0);
            file_cluster = next;
        }

        return HARLIN_FS_OK;
    }
}

int Harlin_Cd(const char* path)
{
    u32 dir_cluster;
    const char* fname;

    if (!path || !path[0])
        return HARLIN_FS_ERROR;

    if (Harlin_Compare(path, "/") == 0) {
        current_dir_cluster = root_cluster;
        return HARLIN_FS_OK;
    }

    if (resolve_path(path, &dir_cluster, &fname) != 0)
        return HARLIN_FS_ERROR;

    if (fname && fname[0]) {
        u32 target_cluster;
        u8 attr;
        if (find_entry_in_dir(dir_cluster, fname, 0, 0, &target_cluster, 0, &attr) != 0)
            return HARLIN_FS_ERROR;
        if (!(attr & 0x10))
            return HARLIN_FS_ERROR;
        current_dir_cluster = target_cluster;
    } else {
        current_dir_cluster = dir_cluster;
    }

    return HARLIN_FS_OK;
}

static int build_cwd_path(u32 cluster, char* buf, u32 size)
{
    if (cluster == root_cluster) {
        buf[0] = '/';
        buf[1] = 0;
        return 0;
    }

    {
        u32 parent_cluster;
        if (read_cluster(cluster, cluster_buf) != 0)
            return -1;
        parent_cluster = ((u32)read_le16(&cluster_buf[0x14]) << 16) | read_le16(&cluster_buf[0x1A]);

        if (parent_cluster == cluster || parent_cluster == root_cluster || parent_cluster < 2 || parent_cluster >= FAT32_EOC) {
            buf[0] = '/';
            buf[1] = 0;
            return 0;
        }

        {
            u32 i;
            u32 entries_per_cluster = (bytes_per_sector * sectors_per_cluster) / 32;
            char name_buf[256];
            int found = 0;

            if (read_cluster(parent_cluster, cluster_buf) != 0)
                return -1;

            name_buf[0] = 0;
            for (i = 0; i < entries_per_cluster; i++) {
                u8* entry = &cluster_buf[i * 32];
                if (entry[0] == 0)
                    break;
                if (entry[0] == FAT32_DELETED)
                    continue;
                if (entry[11] == FAT32_LFN)
                    continue;
                if (!(entry[11] & 0x10))
                    continue;

                {
                    u32 entry_cluster = ((u32)read_le16(&entry[0x14]) << 16) | read_le16(&entry[0x1A]);
                    if (entry_cluster == cluster) {
                        int j, pos;
                        pos = 0;
                        for (j = 0; j < 8 && entry[j] != ' '; j++)
                            name_buf[pos++] = (char)entry[j];
                        if (entry[8] != ' ') {
                            name_buf[pos++] = '.';
                            if (entry[9] != ' ') name_buf[pos++] = (char)entry[9];
                            if (entry[10] != ' ') name_buf[pos++] = (char)entry[10];
                        }
                        name_buf[pos] = 0;
                        found = 1;
                        break;
                    }
                }
            }

            if (!found)
                return -1;

            if (build_cwd_path(parent_cluster, buf, size) != 0)
                return -1;

            {
                u32 cur_len = 0;
                while (buf[cur_len]) cur_len++;
                if (cur_len + Harlin_Len(name_buf) + 2 > size)
                    return -1;
                if (buf[cur_len - 1] != '/') {
                    buf[cur_len++] = '/';
                    buf[cur_len] = 0;
                }
                Harlin_CopyStr(buf + cur_len, name_buf);
            }
        }
    }
    return 0;
}

int Harlin_GetCwd(char* buf, u32 size)
{
    if (!buf || size == 0)
        return HARLIN_FS_ERROR;
    return build_cwd_path(current_dir_cluster, buf, size);
}

static int is_dir_empty(u32 dir_cluster)
{
    u32 cluster = dir_cluster;
    u32 entries_per_cluster = (bytes_per_sector * sectors_per_cluster) / 32;
    int count = 0;

    while (cluster < FAT32_EOC) {
        u32 i;
        if (read_cluster(cluster, cluster_buf) != 0)
            return 0;
        for (i = 0; i < entries_per_cluster; i++) {
            u8* entry = &cluster_buf[i * 32];
            if (entry[0] == 0)
                return (count <= 2) ? 1 : 0;
            if (entry[0] != FAT32_DELETED && entry[11] != FAT32_LFN) {
                count++;
                if (count > 2) return 0;
            }
        }
        cluster = next_cluster(cluster);
    }
    return (count <= 2) ? 1 : 0;
}

int Harlin_Mkdir(const char* path)
{
    u32 parent_dir;
    const char* dirname;
    u8 short_name[11];
    u32 dir_cluster, dir_offset;
    u32 new_cluster;

    if (!path || !path[0])
        return HARLIN_FS_ERROR;

    if (resolve_path(path, &parent_dir, &dirname) != 0)
        return HARLIN_FS_ERROR;

    if (!dirname || !dirname[0])
        return HARLIN_FS_ERROR;

    if (make_short_name(dirname, short_name) != 0)
        return HARLIN_FS_ERROR;

    if (find_entry_in_dir(parent_dir, dirname, 0, 0, 0, 0, 0) == 0)
        return HARLIN_FS_ERROR;

    if (find_directory_slot_in(parent_dir, &dir_cluster, &dir_offset) != 0)
        return HARLIN_FS_ERROR;

    if (allocate_cluster(&new_cluster) != 0)
        return HARLIN_FS_ERROR;

    {
        int i;
        for (i = 0; i < (int)(bytes_per_sector * sectors_per_cluster); i++)
            cluster_buf[i] = 0;

        cluster_buf[0] = '.';
        write_le16(&cluster_buf[0x1A], (u16)(new_cluster & 0xFFFF));
        write_le16(&cluster_buf[0x14], (u16)(new_cluster >> 16));
        cluster_buf[11] = 0x10;

        cluster_buf[32] = '.';
        cluster_buf[33] = '.';
        write_le16(&cluster_buf[0x1A + 32], (u16)(parent_dir & 0xFFFF));
        write_le16(&cluster_buf[0x14 + 32], (u16)(parent_dir >> 16));
        cluster_buf[32 + 11] = 0x10;

        if (write_cluster(new_cluster, cluster_buf) != 0)
            return HARLIN_FS_ERROR;
    }

    if (read_cluster(dir_cluster, cluster_buf) != 0)
        return HARLIN_FS_ERROR;

    {
        u8* entry = &cluster_buf[dir_offset];
        int j;
        for (j = 0; j < 11; j++)
            entry[j] = short_name[j];
        entry[11] = 0x10;
        entry[12] = 0;
        entry[13] = 0;
        write_le16(&entry[14], 0);
        write_le16(&entry[16], 0);
        write_le16(&entry[18], 0);
        write_le16(&entry[20], 0);
        write_le16(&entry[22], 0);
        write_le16(&entry[24], 0);
        write_le16(&entry[26], (u16)(new_cluster >> 16));
        write_le16(&entry[28], 0);
        write_le16(&entry[30], (u16)(new_cluster & 0xFFFF));
        write_le32(&entry[0x1C], 0);
    }

    if (write_cluster(dir_cluster, cluster_buf) != 0)
        return HARLIN_FS_ERROR;

    return HARLIN_FS_OK;
}

int Harlin_Rmdir(const char* path)
{
    u32 parent_dir;
    const char* dirname;
    u32 entry_cluster;
    u32 entry_offset;
    u32 target_cluster;
    u8 attr;

    if (!path || !path[0])
        return HARLIN_FS_ERROR;

    if (resolve_path(path, &parent_dir, &dirname) != 0)
        return HARLIN_FS_ERROR;

    if (!dirname || !dirname[0])
        return HARLIN_FS_ERROR;

    if (find_entry_in_dir(parent_dir, dirname, &entry_cluster, &entry_offset, &target_cluster, 0, &attr) != 0)
        return HARLIN_FS_ERROR;

    if (!(attr & 0x10))
        return HARLIN_FS_ERROR;

    if (!is_dir_empty(target_cluster))
        return HARLIN_FS_ERROR;

    if (read_cluster(entry_cluster, cluster_buf) != 0)
        return HARLIN_FS_ERROR;

    {
        u8* entry = &cluster_buf[entry_offset];
        u8 k;
        entry[0] = FAT32_DELETED;
        for (k = 1; k < 32; k++)
            entry[k] = 0;
        if (write_cluster(entry_cluster, cluster_buf) != 0)
            return HARLIN_FS_ERROR;
    }

    while (target_cluster >= 2 && target_cluster < FAT32_EOC) {
        u32 next = next_cluster(target_cluster);
        write_fat_entry(target_cluster, 0);
        target_cluster = next;
    }

    return HARLIN_FS_OK;
}
