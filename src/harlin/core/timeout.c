#include "timeout.h"
#include "io.h"
#include "bug.h"

extern int atomic_cmpxchg(volatile u64* ptr, u64 expected, u64 desired);

#define PIT_CHANNEL0 0x40
#define PIT_COMMAND  0x43
#define PIT_FREQ     1193182ULL

u64 pit_ticks(void)
{
    u16 lo, hi;
    asm volatile ("cli");
    outb(PIT_COMMAND, 0x00);
    inb(PIT_CHANNEL0);
    lo = inb(PIT_CHANNEL0);
    hi = inb(PIT_CHANNEL0);
    asm volatile ("sti");
    return ((u64)hi << 8) | lo;
}

static u64 pit_to_ms(u64 ticks)
{
    return ticks / (PIT_FREQ / 1000);
}

int spinlock_acquire_timeout(struct spinlock* lk, u32 timeout_ms)
{
    u64 start = pit_ticks();
    u64 deadline = start + (u64)timeout_ms * (PIT_FREQ / 1000);
    u64 spins = 0;

    while (1) {
        while (lk->lock) {
            asm volatile ("pause");
            spins++;
            if (pit_to_ms(pit_ticks() - start) >= timeout_ms) {
                pr_emerg("spinlock timeout after %u ms, %llu spins", timeout_ms, (unsigned long long)spins);
                pr_emerg("lock addr: %p", (void*)lk);
                BUG();
            }
        }
        if (atomic_cmpxchg((volatile u64*)&lk->lock, 0, 1))
            return 0;
    }
}
