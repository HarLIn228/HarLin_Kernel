#ifndef SPINLOCK_H
#define SPINLOCK_H

#include "harlin_API.h"

struct spinlock {
    volatile u32 lock;
};

void spinlock_init(struct spinlock* lk);
void spinlock_acquire(struct spinlock* lk);
void spinlock_release(struct spinlock* lk);
int spinlock_try_acquire(struct spinlock* lk);

u64 atomic_xchg(volatile u64* ptr, u64 val);
u64 atomic_add(volatile u64* ptr, u64 val);
u64 atomic_sub(volatile u64* ptr, u64 val);
int atomic_cmpxchg(volatile u64* ptr, u64 expected, u64 desired);

#endif
