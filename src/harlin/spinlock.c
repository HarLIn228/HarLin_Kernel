#include "spinlock.h"

extern u64 atomic_xchg(volatile u64* ptr, u64 val);
extern u64 atomic_add(volatile u64* ptr, u64 val);
extern u64 atomic_sub(volatile u64* ptr, u64 val);
extern int atomic_cmpxchg(volatile u64* ptr, u64 expected, u64 desired);

void spinlock_init(struct spinlock* lk)
{
    lk->lock = 0;
}

void spinlock_acquire(struct spinlock* lk)
{
    while (1) {
        while (lk->lock)
            asm volatile ("pause");
        if (atomic_cmpxchg((volatile u64*)&lk->lock, 0, 1))
            break;
    }
}

void spinlock_release(struct spinlock* lk)
{
    lk->lock = 0;
}

int spinlock_try_acquire(struct spinlock* lk)
{
    return atomic_cmpxchg((volatile u64*)&lk->lock, 0, 1);
}

u64 atomic_xchg_wrapper(volatile u64* ptr, u64 val)
{
    return atomic_xchg(ptr, val);
}

u64 atomic_add_wrapper(volatile u64* ptr, u64 val)
{
    return atomic_add(ptr, val);
}

u64 atomic_sub_wrapper(volatile u64* ptr, u64 val)
{
    return atomic_sub(ptr, val);
}

int atomic_cmpxchg_wrapper(volatile u64* ptr, u64 expected, u64 desired)
{
    return atomic_cmpxchg(ptr, expected, desired);
}
