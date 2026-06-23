#ifndef HARLIN_INITRAMFS_H
#define HARLIN_INITRAMFS_H

#include "harlin_API.h"

#define HARLIN_INITRAMFS_MAGIC "HARLINRAMFS"
#define HARLIN_INITRAMFS_MAX   8

struct Harlin_Initramfs_Entry {
    const char* name;
    const u8*  data;
    u32        size;
};

void Harlin_Initramfs_Init(void);
int  Harlin_Initramfs_Register(const char* name, const u8* data, u32 size);
int  Harlin_Initramfs_Open(const char* name, struct Harlin_File* out);

#endif
