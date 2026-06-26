#ifndef HARLIN_BUG_H
#define HARLIN_BUG_H

#include "printk.h"

#define HARLIN_NORETURN __attribute__((noreturn))

HARLIN_NORETURN void panic(const char* msg);
HARLIN_NORETURN void panic_assert(const char* cond, const char* file, int line);

#define BUG()                              \
    do {                                   \
        pr_emerg("BUG at %s:%d", __FILE__, __LINE__); \
        panic("BUG");                      \
    } while (0)

#define BUG_ON(cond)                       \
    do {                                   \
        if (cond) {                        \
            pr_emerg("BUG_ON(%s) at %s:%d", #cond, __FILE__, __LINE__); \
            panic("BUG_ON");               \
        }                                  \
    } while (0)

#define ASSERT(cond)                       \
    do {                                   \
        if (!(cond)) {                     \
            panic_assert(#cond, __FILE__, __LINE__); \
        }                                  \
    } while (0)

#define ASSERT_MSG(cond, fmt, ...)         \
    do {                                   \
        if (!(cond)) {                     \
            pr_emerg("ASSERT(%s) failed at %s:%d: " fmt, #cond, __FILE__, __LINE__, ##__VA_ARGS__); \
            panic_assert(#cond, __FILE__, __LINE__); \
        }                                  \
    } while (0)

#endif
