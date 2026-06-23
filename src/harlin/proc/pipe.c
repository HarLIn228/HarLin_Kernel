#include "pipe.h"
#include "kmalloc.h"
#include "harlin_API.h"
#include "scheduler.h"
#include "screen.h"
#include "spinlock.h"

#define PIPE_COUNT 16
#define PIPE_BUFFER_SIZE 4096
#define PIPE_WAIT_MAX 32

struct pipe {
    u8* buffer;
    u32 head;
    u32 tail;
    u32 size;
    int readers;
    int writers;
    int used;
    struct spinlock lock;
    int read_waiters[PIPE_WAIT_MAX];
    int read_wait_count;
    int write_waiters[PIPE_WAIT_MAX];
    int write_wait_count;
};

static struct pipe pipe_table[PIPE_COUNT];

static void pipe_wake_some(struct pipe* p, int* waits, int* count)
{
    int i;
    int n = *count;
    for (i = 0; i < n; i++) {
        int pid = waits[i];
        waits[i] = 0;
        process_wake(pid);
    }
    *count = 0;
}

int pipe_init(void)
{
    int i;
    int j;
    for (i = 0; i < PIPE_COUNT; i++) {
        pipe_table[i].buffer = 0;
        pipe_table[i].head = 0;
        pipe_table[i].tail = 0;
        pipe_table[i].size = PIPE_BUFFER_SIZE;
        pipe_table[i].readers = 0;
        pipe_table[i].writers = 0;
        pipe_table[i].used = 0;
        pipe_table[i].read_wait_count = 0;
        pipe_table[i].write_wait_count = 0;
        for (j = 0; j < PIPE_WAIT_MAX; j++) {
            pipe_table[i].read_waiters[j] = 0;
            pipe_table[i].write_waiters[j] = 0;
        }
        spinlock_init(&pipe_table[i].lock);
    }
    return 0;
}

int pipe_create(void)
{
    int i;
    int j;
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

    spinlock_acquire(&pipe_table[i].lock);
    pipe_table[i].buffer = buf;
    pipe_table[i].head = 0;
    pipe_table[i].tail = 0;
    pipe_table[i].size = PIPE_BUFFER_SIZE;
    pipe_table[i].readers = 1;
    pipe_table[i].writers = 1;
    pipe_table[i].used = 1;
    pipe_table[i].read_wait_count = 0;
    pipe_table[i].write_wait_count = 0;
    for (j = 0; j < PIPE_WAIT_MAX; j++) {
        pipe_table[i].read_waiters[j] = 0;
        pipe_table[i].write_waiters[j] = 0;
    }
    spinlock_release(&pipe_table[i].lock);
    return i;
}

static int pipe_add_waiter(int* waits, int* count, int pid)
{
    int i;
    for (i = 0; i < PIPE_WAIT_MAX; i++) {
        if (waits[i] == 0) {
            waits[i] = pid;
            if (i + 1 > *count)
                *count = i + 1;
            return 0;
        }
    }
    screen_puts("[pipe] waiter queue full\n");
    return -1;
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

    spinlock_acquire(&p->lock);
    if (p->writers == 0 && p->head == p->tail) {
        spinlock_release(&p->lock);
        return 0;
    }

    dst = (u8*)buf;
    while (count < len && p->head != p->tail) {
        dst[count++] = p->buffer[p->head];
        p->head = (p->head + 1) % p->size;
    }
    if (count > 0 && p->write_wait_count > 0)
        pipe_wake_some(p, p->write_waiters, &p->write_wait_count);
    spinlock_release(&p->lock);

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

    spinlock_acquire(&p->lock);
    if (p->readers == 0) {
        spinlock_release(&p->lock);
        return HARLIN_INVALID;
    }

    src = (const u8*)buf;
    while (count < len) {
        u32 next = (p->tail + 1) % p->size;
        if (next == p->head)
            break;
        p->buffer[p->tail] = src[count++];
        p->tail = next;
    }
    if (count > 0 && p->read_wait_count > 0)
        pipe_wake_some(p, p->read_waiters, &p->read_wait_count);
    spinlock_release(&p->lock);

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
    struct process* self;
    int pid;
    if (id < 0 || id >= PIPE_COUNT)
        return HARLIN_INVALID;
    p = &pipe_table[id];
    if (!p->used || !p->buffer)
        return HARLIN_INVALID;
    self = process_current();
    pid = self ? self->pid : -1;
    while (total < (int)len) {
        n = pipe_read(id, dst + total, len - (u32)total);
        if (n < 0)
            return n;
        if (n == 0) {
            if (!p->used || p->writers == 0)
                return total;
            spinlock_acquire(&p->lock);
            if (pid >= 0)
                pipe_add_waiter(p->read_waiters, &p->read_wait_count, pid);
            spinlock_release(&p->lock);
            process_block_current();
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
    struct process* self;
    int pid;
    if (id < 0 || id >= PIPE_COUNT)
        return HARLIN_INVALID;
    p = &pipe_table[id];
    if (!p->used || !p->buffer)
        return HARLIN_INVALID;
    if (p->readers == 0)
        return HARLIN_INVALID;
    self = process_current();
    pid = self ? self->pid : -1;
    while (total < (int)len) {
        n = pipe_write(id, src + total, len - (u32)total);
        if (n < 0)
            return n;
        if (n == 0) {
            if (!p->used || p->readers == 0)
                return total;
            spinlock_acquire(&p->lock);
            if (pid >= 0)
                pipe_add_waiter(p->write_waiters, &p->write_wait_count, pid);
            spinlock_release(&p->lock);
            process_block_current();
        } else {
            total += n;
        }
    }
    return total;
}

void pipe_close(int id)
{
    struct pipe* p;
    int to_wake_read[PIPE_WAIT_MAX];
    int to_wake_write[PIPE_WAIT_MAX];
    int read_n;
    int write_n;
    int i;

    if (id < 0 || id >= PIPE_COUNT)
        return;

    p = &pipe_table[id];
    if (!p->used)
        return;

    spinlock_acquire(&p->lock);
    read_n = p->read_wait_count;
    write_n = p->write_wait_count;
    for (i = 0; i < PIPE_WAIT_MAX; i++) {
        to_wake_read[i] = p->read_waiters[i];
        to_wake_write[i] = p->write_waiters[i];
        p->read_waiters[i] = 0;
        p->write_waiters[i] = 0;
    }
    p->read_wait_count = 0;
    p->write_wait_count = 0;

    if (p->readers > 0) p->readers--;
    if (p->writers > 0) p->writers--;

    if (p->readers == 0 && p->writers == 0) {
        if (p->buffer)
            kfree(p->buffer);
        p->buffer = 0;
        p->used = 0;
    }
    spinlock_release(&p->lock);

    for (i = 0; i < read_n; i++)
        if (to_wake_read[i]) process_wake(to_wake_read[i]);
    for (i = 0; i < write_n; i++)
        if (to_wake_write[i]) process_wake(to_wake_write[i]);
}
