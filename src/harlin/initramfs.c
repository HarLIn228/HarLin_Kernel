#include "initramfs.h"
#include "fat32.h"

static struct Harlin_Initramfs_Entry g_entries[HARLIN_INITRAMFS_MAX];
static int g_count;

void Harlin_Initramfs_Init(void)
{
    int i;
    g_count = 0;
    for (i = 0; i < HARLIN_INITRAMFS_MAX; i++) {
        g_entries[i].name = 0;
        g_entries[i].data = 0;
        g_entries[i].size = 0;
    }
}

int Harlin_Initramfs_Register(const char* name, const u8* data, u32 size)
{
    int i;
    if (!name || !data || size == 0)
        return -1;
    for (i = 0; i < g_count; i++) {
        if (Harlin_Compare(g_entries[i].name, name) == 0) {
            g_entries[i].data = data;
            g_entries[i].size = size;
            return 0;
        }
    }
    if (g_count >= HARLIN_INITRAMFS_MAX)
        return -1;
    g_entries[g_count].name = name;
    g_entries[g_count].data = data;
    g_entries[g_count].size = size;
    g_count++;
    return 0;
}

static int name_basename(const char* path, const char** out)
{
    const char* p = path;
    const char* last = path;
    if (!path) return -1;
    if (path[0] == '/') {
        p = path + 1;
        last = p;
    } else {
        last = path;
    }
    while (*p) {
        if (*p == '/') last = p + 1;
        p++;
    }
    if (last[0] == 0) return -1;
    *out = last;
    return 0;
}

int Harlin_Initramfs_Open(const char* name, struct Harlin_File* out)
{
    const char* base;
    int i;
    if (!name || !out) return -1;
    if (name_basename(name, &base) != 0) return -1;
    for (i = 0; i < g_count; i++) {
        if (Harlin_Compare(g_entries[i].name, base) == 0) {
            out->is_ramfs = 1;
            out->ramfs_data = g_entries[i].data;
            out->start_cluster = 0;
            out->current_cluster = 0;
            out->position = 0;
            out->size = g_entries[i].size;
            out->dir_cluster = 0;
            out->dir_offset = 0;
            out->damaged = 0;
            return 0;
        }
    }
    return -1;
}
