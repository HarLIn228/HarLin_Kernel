#ifndef HARLIN_H
#define HARLIN_H

typedef unsigned long long harlin_u64;
typedef unsigned int       harlin_u32;
typedef unsigned short     harlin_u16;
typedef unsigned char      harlin_u8;

#define HARLIN_SYS_EXIT  0
#define HARLIN_SYS_PRINT 1
#define HARLIN_SYS_GETC  2
#define HARLIN_SYS_ALLOC 3
#define HARLIN_SYS_FREE  4
#define HARLIN_SYS_OPEN  5
#define HARLIN_SYS_READ  6
#define HARLIN_SYS_CLOSE 7
#define HARLIN_SYS_EXEC  8
#define HARLIN_SYS_YIELD 9
#define HARLIN_SYS_SLEEP 10
#define HARLIN_SYS_KEYOVERFLOW 11
#define HARLIN_SYS_PIPE_CREATE 12
#define HARLIN_SYS_PIPE_READ   13
#define HARLIN_SYS_PIPE_WRITE  14
#define HARLIN_SYS_PIPE_CLOSE  15
#define HARLIN_SYS_PIPE_READY  16
#define HARLIN_SYS_GETPID      17
#define HARLIN_SYS_GETCPU      18
#define HARLIN_SYS_TIME        19
#define HARLIN_SYS_BEEP        20
#define HARLIN_SYS_KMALLOC     21
#define HARLIN_SYS_KFREE       22
#define HARLIN_SYS_MMAP        23
#define HARLIN_SYS_UNMAP       24
#define HARLIN_SYS_GETKEYSTATE 25
#define HARLIN_SYS_KEYLED      26
#define HARLIN_SYS_SETPRIORITY 27

struct harlin_rtc_time {
    harlin_u8 second;
    harlin_u8 minute;
    harlin_u8 hour;
    harlin_u8 day;
    harlin_u8 month;
    harlin_u16 year;
};

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

static inline char harlin_getc(void)
{
    return (char)harlin_syscall(HARLIN_SYS_GETC, 0, 0, 0);
}

static inline harlin_u64 harlin_alloc(void)
{
    return harlin_syscall(HARLIN_SYS_ALLOC, 0, 0, 0);
}

static inline void harlin_free(harlin_u64 addr)
{
    harlin_syscall(HARLIN_SYS_FREE, addr, 0, 0);
}

static inline int harlin_open(const char* name)
{
    return (int)harlin_syscall(HARLIN_SYS_OPEN, (harlin_u64)name, 0, 0);
}

static inline int harlin_read(int fd, void* buf, harlin_u32 len)
{
    return (int)harlin_syscall(HARLIN_SYS_READ, (harlin_u64)fd, (harlin_u64)buf, (harlin_u64)len);
}

static inline void harlin_close(int fd)
{
    harlin_syscall(HARLIN_SYS_CLOSE, (harlin_u64)fd, 0, 0);
}

static inline int harlin_exec(const char* name)
{
    return (int)harlin_syscall(HARLIN_SYS_EXEC, (harlin_u64)name, 0, 0);
}

static inline void harlin_yield(void)
{
    harlin_syscall(HARLIN_SYS_YIELD, 0, 0, 0);
}

static inline void harlin_sleep(harlin_u32 ms)
{
    harlin_syscall(HARLIN_SYS_SLEEP, (harlin_u64)ms, 0, 0);
}

static inline int harlin_getpid(void)
{
    return (int)harlin_syscall(HARLIN_SYS_GETPID, 0, 0, 0);
}

static inline int harlin_getcpu(void)
{
    return (int)harlin_syscall(HARLIN_SYS_GETCPU, 0, 0, 0);
}

static inline void harlin_time(struct harlin_rtc_time* out)
{
    harlin_syscall(HARLIN_SYS_TIME, (harlin_u64)out, 0, 0);
}

static inline void harlin_beep(harlin_u32 freq, harlin_u32 ms)
{
    harlin_syscall(HARLIN_SYS_BEEP, (harlin_u64)freq, (harlin_u64)ms, 0);
}

static inline void* harlin_kmalloc(harlin_u64 size)
{
    return (void*)harlin_syscall(HARLIN_SYS_KMALLOC, size, 0, 0);
}

static inline void harlin_kfree(void* ptr)
{
    harlin_syscall(HARLIN_SYS_KFREE, (harlin_u64)ptr, 0, 0);
}

static inline harlin_u64 harlin_mmap(harlin_u64 addr, harlin_u64 size)
{
    return harlin_syscall(HARLIN_SYS_MMAP, addr, size, 0);
}

static inline void harlin_unmap(harlin_u64 addr, harlin_u64 size)
{
    harlin_syscall(HARLIN_SYS_UNMAP, addr, size, 0);
}

static inline harlin_u8 harlin_key_state(void)
{
    return (harlin_u8)harlin_syscall(HARLIN_SYS_GETKEYSTATE, 0, 0, 0);
}

static inline void harlin_key_led(harlin_u8 leds)
{
    harlin_syscall(HARLIN_SYS_KEYLED, (harlin_u64)leds, 0, 0);
}

static inline void harlin_set_priority(harlin_u32 priority)
{
    harlin_syscall(HARLIN_SYS_SETPRIORITY, (harlin_u64)priority, 0, 0);
}

#endif
