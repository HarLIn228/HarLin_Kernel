#ifndef FAT32_H
#define FAT32_H

#include "harlin_API.h"

int  Harlin_FsMount(u32 partition_lba);
int  Harlin_FsOpen(const char* name, struct Harlin_File* out);
int  Harlin_FsCreate(const char* name, struct Harlin_File* out);
int  Harlin_FsRead(struct Harlin_File* file, void* buf, u32 len);
int  Harlin_FsWrite(struct Harlin_File* file, const void* buf, u32 len);
u32  Harlin_FsSize(struct Harlin_File* file);
void Harlin_FsClose(struct Harlin_File* file);

#endif
