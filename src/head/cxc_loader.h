#ifndef CXC_LOADER_H
#define CXC_LOADER_H

#include "harlin_API.h"

#define CXC_MAGIC "HARLINCXC"
#define CXC_HEADER_SIZE 0x200

int cxc_load(const void* file_data, u64 file_size);

#endif
