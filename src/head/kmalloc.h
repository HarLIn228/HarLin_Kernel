#ifndef KMALLOC_H
#define KMALLOC_H

#include "harlin_API.h"

void kmalloc_init(void);
void* kmalloc(u64 size);
void kfree(void* ptr);
void* krealloc(void* ptr, u64 size);
u64 ksize(void* ptr);

#endif
