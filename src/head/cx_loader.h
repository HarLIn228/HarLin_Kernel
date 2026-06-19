#ifndef CX_LOADER_H
#define CX_LOADER_H

#include "harlin_API.h"

#define CX_MAGIC "HARLINCX"
#define CX_HEADER_SIZE 0x200

int cx_load(const void* file_data, u64 file_size);

#endif
