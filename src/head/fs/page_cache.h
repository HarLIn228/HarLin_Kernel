#ifndef READ_CACHE_H
#define READ_CACHE_H

#include "harlin_API.h"

#define RC_BUCKETS 64

int  Harlin_ReadCacheInit(void);
int  Harlin_ReadCacheGet(u32 key, void** out_ptr);
void Harlin_ReadCachePut(u32 key, const void* data);
void Harlin_ReadCacheDrop(u32 key);
u64  Harlin_ReadCacheHits(void);
u64  Harlin_ReadCacheMisses(void);
void Harlin_ReadCacheTest(void);

#endif
