#include "harlin_API.h"
#include "screen.h"
#include "scheduler.h"

struct syscall_regs {
    unsigned long rax;
    unsigned long rcx;
    unsigned long rdx;
    unsigned long rbx;
    unsigned long rbp;
    unsigned long rsi;
    unsigned long rdi;
    unsigned long r8;
    unsigned long r9;
    unsigned long r10;
    unsigned long r11;
    unsigned long r12;
    unsigned long r13;
    unsigned long r14;
    unsigned long r15;
};

typedef unsigned long (*syscall_t)(struct syscall_regs* r);

static unsigned long sys_putchar(struct syscall_regs* r)
{
    screen_put_char((char)r->rdi);
    return 0;
}

static unsigned long sys_exit(struct syscall_regs* r)
{
    process_exit();
    return 0;
}

static syscall_t syscall_table[] = {
    [HARLIN_SYS_EXIT] = sys_exit,
    [HARLIN_SYS_PRINT] = sys_putchar,
};

#define SYSCALL_COUNT (sizeof(syscall_table) / sizeof(syscall_table[0]))

void syscall_dispatch(struct syscall_regs* r)
{
    unsigned long num = r->rax;
    if (num < SYSCALL_COUNT && syscall_table[num]) {
        r->rax = syscall_table[num](r);
    } else {
        r->rax = (unsigned long)-1;
    }
}
