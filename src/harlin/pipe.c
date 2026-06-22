#include "pipe.h"
#include "kmalloc.h"
#include "harlin_API.h"
#include "scheduler.h"

#define PIPE_COUNT 16
#define PIPE_BUFFER_SIZE 4096

struct pipe {
    u8* buffer;
    u32 head;
    u32 tail;
    u32 size;
    int readers;
    int writers;
    int used;
    int read_waiters;
    int write_waiters;
};

static struct pipe pipe_table[PIPE_COUNT];

int pipe_init(void)
{
    int i;
    for (i = 0; i < PIPE_COUNT; i++) {
        pipe_table[i].buffer = 0;
        pipe_table[i].head = 0;
        pipe_table[i].tail = 0;
        pipe_table[i].size = PIPE_BUFFER_SIZE;
        pipe_table[i].readers = 0;
        pipe_table[i].writers = 0;
        pipe_table[i].used = 0;
        pipe_table[i].read_waiters = 0;
        pipe_table[i].write_waiters = 0;
    }
    return 0;
}

int pipe_create(void)
{
    int i;
    u8* buf;

    for (i = 0; i < PIPE_COUNT; i++) {
        if (!pipe_table[i].used)
            break;
    }
    if (i >= PIPE_COUNT)
        return HARLIN_NO_MEMORY;

    buf = (u8*)kmalloc(PIPE_BUFFER_SIZE);
    if (!buf)
        return HARLIN_NO_MEMORY;

    pipe_table[i].buffer = buf;
    pipe_table[i].head = 0;
    pipe_table[i].tail = 0;
    pipe_table[i].size = PIPE_BUFFER_SIZE;
    pipe_table[i].readers = 1;
    pipe_table[i].writers = 1;
    pipe_table[i].used = 1;
    pipe_table[i].read_waiters = 0;
    pipe_table[i].write_waiters = 0;
    return i;
}

int pipe_read(int id, void* buf, u32 len)
{
    struct pipe* p;
    u8* dst;
    u32 count = 0;

    if (id < 0 || id >= PIPE_COUNT)
        return HARLIN_INVALID;
    if (!buf || len == 0)
        return 0;

    p = &pipe_table[id];
    if (!p->used || !p->buffer)
        return HARLIN_INVALID;
    if (p->writers == 0 && p->head == p->tail)
        return 0;

    dst = (u8*)buf;
    while (count < len && p->head != p->tail) {
        dst[count++] = p->buffer[p->head];
        p->head = (p->head + 1) % p->size;
    }

    return (int)count;
}

int pipe_write(int id, const void* buf, u32 len)
{
    struct pipe* p;
    const u8* src;
    u32 count = 0;

    if (id < 0 || id >= PIPE_COUNT)
        return HARLIN_INVALID;
    if (!buf || len == 0)
        return 0;

    p = &pipe_table[id];
    if (!p->used || !p->buffer)
        return HARLIN_INVALID;
    if (p->readers == 0)
        return HARLIN_INVALID;

    src = (const u8*)buf;
    while (count < len) {
        u32 next = (p->tail + 1) % p->size;
        if (next == p->head)
            break;
        p->buffer[p->tail] = src[count++];
        p->tail = next;
    }

    return (int)count;
}

int pipe_ready(int id)
{
    struct pipe* p;

    if (id < 0 || id >= PIPE_COUNT)
        return 0;
    p = &pipe_table[id];
    if (!p->used || !p->buffer)
        return 0;
    return p->head != p->tail;
}

int pipe_space(int id)
{
    struct pipe* p;
    if (id < 0 || id >= PIPE_COUNT)
        return 0;
    p = &pipe_table[id];
    if (!p->used || !p->buffer)
        return 0;
    return (int)((p->head + p->size - p->tail - 1) % p->size);
}

int pipe_read_blocking(int id, void* buf, u32 len)
{
    int n;
    int total = 0;
    u8* dst = (u8*)buf;
    struct pipe* p;
    if (id < 0 || id >= PIPE_COUNT)
        return HARLIN_INVALID;
    p = &pipe_table[id];
    if (!p->used || !p->buffer)
        return HARLIN_INVALID;
    while (total < (int)len) {
        n = pipe_read(id, dst + total, len - (u32)total);
        if (n < 0)
            return n;
        if (n == 0) {
            if (!p->used || p->writers == 0)
                return total;
            p->read_waiters++;
            schedule();
            p->read_waiters--;
        } else {
            total += n;
        }
    }
    return total;
}

int pipe_write_blocking(int id, const void* buf, u32 len)
{
    int n;
    int total = 0;
    const u8* src = (const u8*)buf;
    struct pipe* p;
    if (id < 0 || id >= PIPE_COUNT)
        return HARLIN_INVALID;
    p = &pipe_table[id];
    if (!p->used || !p->buffer)
        return HARLIN_INVALID;
    if (p->readers == 0)
        return HARLIN_INVALID;
    while (total < (int)len) {
        n = pipe_write(id, src + total, len - (u32)total);
        if (n < 0)
            return n;
        if (n == 0) {
            if (!p->used || p->readers == 0)
                return total;
            p->write_waiters++;
            schedule();
            p->write_waiters--;
        } else {
            total += n;
        }
    }
    return total;
}

void pipe_close(int id)
{
    struct pipe* p;

    if (id < 0 || id >= PIPE_COUNT)
        return;

    p = &pipe_table[id];
    if (!p->used)
        return;

    if (p->readers > 0) p->readers--;
    if (p->writers > 0) p->writers--;

    if (p->readers == 0 && p->writers == 0) {
        if (p->buffer)
            kfree(p->buffer);
        p->buffer = 0;
        p->used = 0;
    }
}
