#ifndef FAT32_H
#define FAT32_H

#include "harlin_API.h"

int  Harlin_Mount(u32 partition_lba);
int  Harlin_Open(const char* name, struct Harlin_File* out);
int  Harlin_Create(const char* name, struct Harlin_File* out);
int  Harlin_Read(struct Harlin_File* file, void* buf, u32 len);
int  Harlin_Write(struct Harlin_File* file, const void* buf, u32 len);
u32  Harlin_Size(struct Harlin_File* file);
void Harlin_Close(struct Harlin_File* file);

#endif
