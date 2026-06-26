#include "bug.h"

void show_stack(unsigned long max_frames);

static inline void halt(void)
{
    asm volatile ("cli");
    for (;;) asm volatile ("hlt");
}

HARLIN_NORETURN void panic(const char* msg)
{
    asm volatile ("cli");
    pr_emerg("*** KERNEL PANIC ***");
    pr_emerg("reason: %s", msg);
    show_stack(0);
    halt();
    __builtin_unreachable();
}

HARLIN_NORETURN void panic_assert(const char* cond, const char* file, int line)
{
    asm volatile ("cli");
    pr_emerg("*** ASSERTION FAILED ***");
    pr_emerg("condition: %s", cond);
    pr_emerg("location : %s:%d", file, line);
    show_stack(0);
    halt();
    __builtin_unreachable();
}
