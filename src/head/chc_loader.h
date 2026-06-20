#ifndef CHC_LOADER_H
#define CHC_LOADER_H

#include "harlin_API.h"

#define CHC_MAGIC "HARLINCHC"
#define CHC_HEADER_SIZE 0x200

int chc_load(const void* file_data, u64 file_size);

#endif
