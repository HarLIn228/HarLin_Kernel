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

u64 spinlock_acquire_irqsave(struct spinlock* lk);
void spinlock_release_irqrestore(struct spinlock* lk, u64 flags);
int spinlock_try_acquire_irqsave(struct spinlock* lk, u64* out_flags);
void spinlock_release_from_try(struct spinlock* lk, u64 flags);

u64 atomic_xchg(volatile u64* ptr, u64 val);
u64 atomic_add(volatile u64* ptr, u64 val);
u64 atomic_sub(volatile u64* ptr, u64 val);
int atomic_cmpxchg(volatile u64* ptr, u64 expected, u64 desired);

#define Harlin_SpinlockInit          spinlock_init
#define Harlin_SpinlockAcquire       spinlock_acquire
#define Harlin_SpinlockRelease       spinlock_release
#define Harlin_SpinlockTryAcquire    spinlock_try_acquire
#define Harlin_SpinlockAcquireIrqsave spinlock_acquire_irqsave
#define Harlin_SpinlockReleaseIrqrestore spinlock_release_irqrestore
#define Harlin_AtomicXchg            atomic_xchg
#define Harlin_AtomicAdd             atomic_add
#define Harlin_AtomicSub             atomic_sub
#define Harlin_AtomicCmpxchg         atomic_cmpxchg

#endif
