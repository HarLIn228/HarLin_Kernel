#ifndef HARLIN_TIMEOUT_H
#define HARLIN_TIMEOUT_H

#include "printk.h"
#include "spinlock.h"

u64 pit_ticks(void);

int spinlock_acquire_timeout(struct spinlock* lk, u32 timeout_ms);

#endif
