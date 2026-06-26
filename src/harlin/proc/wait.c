#include "harlin_API.h"
#include "wait.h"
#include "printk.h"
#include "bug.h"

#define WAIT_MAX_EXITS 64

struct wait_exit {
    int used;
    int child_pid;
    int code;
    int reaped;
};

static struct wait_exit exits[WAIT_MAX_EXITS];
static int wait_inited;

int Harlin_WaitInit(void)
{
    if (wait_inited) return 0;
    for (int i = 0; i < WAIT_MAX_EXITS; i++) {
        exits[i].used = 0;
        exits[i].child_pid = 0;
        exits[i].code = 0;
        exits[i].reaped = 0;
    }
    wait_inited = 1;
    pr_info("wait: %d exit slots ready", WAIT_MAX_EXITS);
    return 0;
}

int Harlin_NotifyExit(int child_pid, int code)
{
    if (!wait_inited) Harlin_WaitInit();
    for (int i = 0; i < WAIT_MAX_EXITS; i++) {
        if (exits[i].used && exits[i].child_pid == child_pid) {
            exits[i].code = code;
            return 0;
        }
    }
    for (int i = 0; i < WAIT_MAX_EXITS; i++) {
        if (!exits[i].used) {
            exits[i].used = 1;
            exits[i].reaped = 0;
            exits[i].child_pid = child_pid;
            exits[i].code = code;
            return 0;
        }
    }
    return -1;
}

int Harlin_Wait(int parent_pid, int child_pid, int* out_code)
{
    (void)parent_pid;
    if (!wait_inited) Harlin_WaitInit();
    for (int i = 0; i < WAIT_MAX_EXITS; i++) {
        if (exits[i].used && exits[i].child_pid == child_pid) {
            if (out_code) *out_code = exits[i].code;
            if (!exits[i].reaped) {
                exits[i].reaped = 1;
                exits[i].used = 0;
            }
            return 0;
        }
    }
    return -2;
}

int Harlin_WaitAny(int parent_pid, int* out_child_pid, int* out_code)
{
    (void)parent_pid;
    if (!wait_inited) Harlin_WaitInit();
    for (int i = 0; i < WAIT_MAX_EXITS; i++) {
        if (exits[i].used && !exits[i].reaped) {
            if (out_child_pid) *out_child_pid = exits[i].child_pid;
            if (out_code) *out_code = exits[i].code;
            exits[i].reaped = 1;
            exits[i].used = 0;
            return 0;
        }
    }
    return -2;
}

void Harlin_WaitTest(void)
{
    ASSERT(Harlin_WaitInit() == 0);

    int code = 0;
    int rc = Harlin_Wait(0, 999, &code);
    ASSERT(rc == -2);

    ASSERT(Harlin_NotifyExit(101, 0) == 0);
    ASSERT(Harlin_NotifyExit(102, 7) == 0);

    int got = 0;
    int rc1 = Harlin_Wait(0, 101, &got);
    ASSERT(rc1 == 0);
    ASSERT(got == 0);

    int rc2 = Harlin_Wait(0, 101, &got);
    ASSERT(rc2 == -2);

    int c2 = 0;
    int rc3 = Harlin_Wait(0, 102, &c2);
    ASSERT(rc3 == 0);
    ASSERT(c2 == 7);

    ASSERT(Harlin_NotifyExit(103, 42) == 0);
    int any_pid = 0, any_code = 0;
    int rc4 = Harlin_WaitAny(0, &any_pid, &any_code);
    ASSERT(rc4 == 0);
    ASSERT(any_pid == 103);
    ASSERT(any_code == 42);

    int rc5 = Harlin_WaitAny(0, &any_pid, &any_code);
    ASSERT(rc5 == -2);

    pr_info("wait: test OK");
}
