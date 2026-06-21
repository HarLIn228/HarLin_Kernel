#ifndef HARLIN_H
#define HARLIN_H

typedef unsigned long long harlin_u64;
typedef unsigned int       harlin_u32;
typedef unsigned short     harlin_u16;
typedef unsigned char      harlin_u8;

#define HARLIN_SYS_EXIT  0
#define HARLIN_SYS_PRINT 1

static inline harlin_u64 harlin_syscall(harlin_u64 num, harlin_u64 a1, harlin_u64 a2, harlin_u64 a3)
{
    harlin_u64 ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "D"(a1), "S"(a2), "d"(a3)
        : "rcx", "r8", "r9", "r10", "r11", "memory"
    );
    return ret;
}

static inline void harlin_print(const char *str)
{
    harlin_syscall(HARLIN_SYS_PRINT, (harlin_u64)str, 0, 0);
}

static inline void harlin_exit(int code)
{
    harlin_syscall(HARLIN_SYS_EXIT, (harlin_u64)code, 0, 0);
}

#endif
