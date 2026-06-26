#include "printk.h"

static void dbg_putc(char c)
{
    asm volatile ("outb %0, %1" : : "a"((unsigned char)c), "Nd"((unsigned short)0x0402));
}

static void dbg_puts(const char* s)
{
    while (*s) dbg_putc(*s++);
}

static void dbg_putu(unsigned long long v, unsigned int base, int width, char pad)
{
    char buf[24];
    int i = 0;
    const char* digits = "0123456789abcdef";
    if (v == 0) {
        buf[i++] = '0';
    } else {
        while (v) {
            buf[i++] = digits[v % base];
            v /= base;
        }
    }
    while (i < width) buf[i++] = pad;
    while (i > 0) dbg_putc(buf[--i]);
}

static void dbg_puti(long long v, unsigned int base, int width, char pad)
{
    if (v < 0) {
        dbg_putc('-');
        dbg_putu((unsigned long long)(-(v + 1)) + 1ULL, base, width, pad);
    } else {
        dbg_putu((unsigned long long)v, base, width, pad);
    }
}

static void dbg_puts_padded(const char* s, int width, char pad)
{
    int n = 0;
    const char* p = s;
    while (*p) { n++; p++; }
    while (n < width) { dbg_putc(pad); width--; }
    dbg_puts(s);
}

void printk(const char* fmt, ...)
{
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);

    dbg_putc('[');
    const char* p = fmt;
    if (p[0] == '[' && p[1] == 'e' && p[2] == 'm' && p[3] == 'e' && p[4] == 'r' && p[5] == 'g' && p[6] == ']') {
        dbg_puts("emerg");
        p += 7;
    } else if (p[0] == '[') {
        char tag[8];
        int t = 0;
        p++;
        while (*p && *p != ']' && t < 7) tag[t++] = *p++;
        tag[t] = 0;
        if (*p == ']') p++;
        dbg_puts(tag);
    } else {
        dbg_puts("info");
    }
    dbg_putc(']');
    dbg_putc(' ');

    while (*p) {
        if (*p != '%') {
            dbg_putc(*p++);
            continue;
        }
        p++;
        char pad = ' ';
        int width = 0;
        if (*p == '0') { pad = '0'; p++; }
        while (*p >= '0' && *p <= '9') {
            width = width * 10 + (*p - '0');
            p++;
        }
        int is_long = 0;
        if (*p == 'l') { is_long = 1; p++; if (*p == 'l') { is_long = 2; p++; } }

        switch (*p) {
            case 'd': case 'i':
                if (is_long == 2) dbg_puti(__builtin_va_arg(ap, long long), 10, width, pad);
                else if (is_long) dbg_puti(__builtin_va_arg(ap, long), 10, width, pad);
                else dbg_puti(__builtin_va_arg(ap, int), 10, width, pad);
                break;
            case 'u':
                if (is_long == 2) dbg_putu(__builtin_va_arg(ap, unsigned long long), 10, width, pad);
                else if (is_long) dbg_putu(__builtin_va_arg(ap, unsigned long), 10, width, pad);
                else dbg_putu(__builtin_va_arg(ap, unsigned int), 10, width, pad);
                break;
            case 'x': case 'X':
                if (is_long == 2) dbg_putu(__builtin_va_arg(ap, unsigned long long), 16, width, pad);
                else if (is_long) dbg_putu(__builtin_va_arg(ap, unsigned long), 16, width, pad);
                else dbg_putu(__builtin_va_arg(ap, unsigned int), 16, width, pad);
                break;
            case 'p':
                dbg_puts("0x");
                dbg_putu((unsigned long long)__builtin_va_arg(ap, void*), 16, width, pad);
                break;
            case 's': {
                const char* s = __builtin_va_arg(ap, const char*);
                if (!s) s = "(null)";
                dbg_puts_padded(s, width, pad);
                break;
            }
            case 'c':
                dbg_putc((char)__builtin_va_arg(ap, int));
                break;
            case '%':
                dbg_putc('%');
                break;
            default:
                dbg_putc('%');
                dbg_putc(*p);
                break;
        }
        p++;
    }
    dbg_putc('\n');

    __builtin_va_end(ap);
}
