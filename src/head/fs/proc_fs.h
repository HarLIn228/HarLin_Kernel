#ifndef PROC_FS_H
#define PROC_FS_H

#include "harlin_API.h"

int  Harlin_ProcFsRead(const char* path, char* out, u32 out_size);
int  Harlin_ProcFsLs(const char* path, char* out, u32 out_size);
void Harlin_ProcFsTest(void);

#endif
