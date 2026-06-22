#ifndef CHC_LOADER_H
#define CHC_LOADER_H

#include "harlin_API.h"

#define CHC_MAGIC "HARLINCHC"
#define CHC_HEADER_SIZE 0x200

#define CHC_FLAG_IMPORT 0x0001
#define CHC_FLAG_EXPORT 0x0002

#define CHC_MAX_LIBRARIES 16
#define CHC_LIB_INVALID (-1)

struct chc_import_entry {
    u32 name_offset;
    u32 addr_offset;
    u16 ordinal;
    u16 flags;
} __attribute__((packed));

struct chc_export_entry {
    u32 name_offset;
    u32 rva;
    u16 ordinal;
    u16 flags;
} __attribute__((packed));

struct shared_lib {
    int used;
    u64 code_base;
    u64 data_base;
    u64 code_size;
    u64 data_size;
    u64 code_pages;
    struct chc_export_entry* exports;
    u32 export_count;
    const u8* file_image;
};

int chc_load(const void* file_data, u64 file_size);
int  Harlin_DlOpen(const char* path);
void* Harlin_DlSym(int lib_id, const char* name);
int  Harlin_DlClose(int lib_id);

#endif
