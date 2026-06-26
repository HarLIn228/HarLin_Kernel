#include "signal.h"
#include "printk.h"
#include "bug.h"
#include "harlin_API.h"

static u32 pending_bits[MAX_TASKS];
static u32 blocked_bits[MAX_TASKS];

int Harlin_NotifyInit(void)
{
    for (int i = 0; i < MAX_TASKS; i++) {
        pending_bits[i] = 0;
        blocked_bits[i] = NOTIFY_BLOCK_DEFAULT;
    }
    pr_info("notify: %d slots ready", MAX_TASKS);
    return 0;
}

int Harlin_NotifySend(int target_pid, int what)
{
    if (target_pid < 0 || target_pid >= MAX_TASKS) return -1;
    if (what < 0 || what > 31) return -1;
    pending_bits[target_pid] |= (1u << what);
    pr_debug("notify: pid=%d what=%d pending", target_pid, what);
    return 0;
}

int Harlin_NotifyPoll(int pid)
{
    if (pid < 0 || pid >= MAX_TASKS) return 0;
    return (int)(pending_bits[pid] & ~blocked_bits[pid]);
}

int Harlin_NotifyPop(int pid)
{
    if (pid < 0 || pid >= MAX_TASKS) return -1;
    u32 mask = pending_bits[pid] & ~blocked_bits[pid];
    if (mask == 0) return -1;
    int what = 0;
    for (int i = 0; i < 32; i++) {
        if (mask & (1u << i)) { what = i; break; }
    }
    pending_bits[pid] &= ~(1u << what);
    return what;
}

int Harlin_NotifyMask(int pid, int what)
{
    if (pid < 0 || pid >= MAX_TASKS) return -1;
    if (what < 0 || what > 31) return -1;
    blocked_bits[pid] |= (1u << what);
    return 0;
}

int Harlin_NotifyUnmask(int pid, int what)
{
    if (pid < 0 || pid >= MAX_TASKS) return -1;
    if (what <= 0 || what > 31) return -1;
    blocked_bits[pid] &= ~(1u << what);
    return 0;
}

void Harlin_NotifyPump(int pid)
{
    int what;
    while ((what = Harlin_NotifyPop(pid)) > 0) {
        pr_info("notify: pid=%d delivered what=%d", pid, what);
    }
}

void Harlin_NotifyTest(void)
{
    int test_pid = 1;
    Harlin_NotifySend(test_pid, NOTIFY_USER1);
    Harlin_NotifySend(test_pid, NOTIFY_USER2);
    ASSERT(Harlin_NotifyPoll(test_pid) != 0);

    int s = Harlin_NotifyPop(test_pid);
    ASSERT(s == NOTIFY_USER1);
    s = Harlin_NotifyPop(test_pid);
    ASSERT(s == NOTIFY_USER2);
    ASSERT(Harlin_NotifyPop(test_pid) < 0);

    Harlin_NotifySend(test_pid, NOTIFY_KILL);
    Harlin_NotifyMask(test_pid, NOTIFY_KILL);
    ASSERT(Harlin_NotifyPop(test_pid) < 0);

    Harlin_NotifyUnmask(test_pid, NOTIFY_KILL);
    s = Harlin_NotifyPop(test_pid);
    ASSERT(s == NOTIFY_KILL);

    pr_info("notify: test OK");
}
