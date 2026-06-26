#ifndef MEM_FS_H
#define MEM_FS_H

#include "harlin_API.h"

#define MEM_FS_NODES     128
#define MEM_FS_NAME_MAX  12
#define MEM_FS_PAYLOAD   256

#define MEM_FS_FILE  1
#define MEM_FS_DIR   2

struct mem_fs_node {
    u8  used;
    u8  type;
    u8  name_len;
    char name[MEM_FS_NAME_MAX];
    u32 parent;
    u32 first_child;
    u32 next_sibling;
    u16 mode;
    u16 owner;
    u16 group;
    u32 payload_size;
    u8  payload[MEM_FS_PAYLOAD];
};

struct mem_fs_dir_entry {
    char name[MEM_FS_NAME_MAX];
    u8  name_len;
    u8  type;
    u32 node;
};

int  Harlin_MemFsInit(void);
int  Harlin_MemFsMkdir(const char* path);
int  Harlin_MemFsRmdir(const char* path);
int  Harlin_MemFsCreate(const char* path);
int  Harlin_MemFsRemove(const char* path);
int  Harlin_MemFsWrite(const char* path, const void* data, u32 size);
int  Harlin_MemFsRead(const char* path, void* buf, u32 size);
int  Harlin_MemFsLs(const char* path, struct mem_fs_dir_entry* out, u32 max);
u32  Harlin_MemFsLookupNode(const char* path);
int  Harlin_MemFsTest(void);

#endif
