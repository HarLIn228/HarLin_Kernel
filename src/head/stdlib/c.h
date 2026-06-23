#ifndef HARLIN_C_H
#define HARLIN_C_H

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>
#include <limits.h>
#include <float.h>
#include <iso646.h>
#include <stdalign.h>
#include <stdnoreturn.h>

#include "harlin_API.h"

typedef unsigned char       uchar;
typedef unsigned short      ushort;
typedef unsigned int        uint;
typedef unsigned long       ulong;
typedef long long           s64;
typedef int                 s32;
typedef short               s16;
typedef signed char         s8;
typedef unsigned long long  u64;
typedef unsigned int        u32;
typedef unsigned short      u16;
typedef unsigned char       u8;

#ifndef NULL
#define NULL ((void*)0)
#endif

#ifndef HARLIN_API
#define HARLIN_API __attribute__((visibility("default")))
#endif

#ifndef HARLIN_PACKED
#define HARLIN_PACKED __attribute__((packed))
#endif

#ifndef HARLIN_NORETURN
#define HARLIN_NORETURN __attribute__((noreturn))
#endif

#ifndef HARLIN_ALIGN
#define HARLIN_ALIGN(x) __attribute__((aligned(x)))
#endif

#ifndef HARLIN_USED
#define HARLIN_USED __attribute__((used))
#endif

#ifndef HARLIN_WEAK
#define HARLIN_WEAK __attribute__((weak))
#endif

#define harlin_offsetof(type, member) __builtin_offsetof(type, member)
#define harlin_container_of(ptr, type, member) \
    ((type*)((char*)(ptr) - harlin_offsetof(type, member)))
#define harlin_align_up(x, a)   (((x) + ((a) - 1)) & ~((a) - 1))
#define harlin_align_down(x, a) ((x) & ~((a) - 1))
#define harlin_min(a, b)        ((a) < (b) ? (a) : (b))
#define harlin_max(a, b)        ((a) > (b) ? (a) : (b))
#define harlin_abs(x)           ((x) < 0 ? -(x) : (x))
#define harlin_swap(a, b)       do { __typeof__(a) _t = (a); (a) = (b); (b) = _t; } while (0)
#define harlin_array_len(a)     (sizeof(a) / sizeof((a)[0]))
#define harlin_bit(n)           (1ULL << (n))

#define HARLIN_OK         0
#define HARLIN_ERR       -1
#define HARLIN_INVAL     -2
#define HARLIN_NOMEM     -3
#define HARLIN_NOENT     -4
#define HARLIN_BUSY      -5
#define HARLIN_AGAIN     -6
#define HARLIN_NOSYS     -7
#define HARLIN_PERM      -8

#endif
