#ifndef COW_FS_H
#define COW_FS_H

#include "harlin_API.h"
#include "mem_fs.h"

#define COW_MAX_VERSIONS 16
#define COW_VER_STR_LEN  16

struct cow_version {
    u32 used;
    u32 file_node;
    char ver[COW_VER_STR_LEN];
    u32 payload_size;
    u8  payload[MEM_FS_PAYLOAD];
};

int  Harlin_CowFsInit(void);
int  Harlin_CowSnapshot(const char* path, char* out_ver);
int  Harlin_CowWrite(const char* path, const char* expected_ver,
                     const void* data, u32 size);
int  Harlin_CowReadAt(const char* path, const char* ver,
                      void* buf, u32 size);
int  Harlin_CowFork(const char* src_path, const char* dst_path);
int  Harlin_CowTest(void);

#endif
