#ifndef EVENT_H
#define EVENT_H

#include "harlin_API.h"

#define MAX_TASKS 32

#define NOTIFY_KILL  9
#define NOTIFY_FAULT 11
#define NOTIFY_END   15
#define NOTIFY_CHILD 17
#define NOTIFY_STOP  19
#define NOTIFY_INT   2
#define NOTIFY_USER1 10
#define NOTIFY_USER2 12

#define NOTIFY_BLOCK_DEFAULT (1u << NOTIFY_KILL)

int  Harlin_NotifyInit(void);
int  Harlin_NotifySend(int target_pid, int what);
int  Harlin_NotifyPoll(int pid);
int  Harlin_NotifyPop(int pid);
int  Harlin_NotifyMask(int pid, int what);
int  Harlin_NotifyUnmask(int pid, int what);
void Harlin_NotifyPump(int pid);
void Harlin_NotifyTest(void);

#endif
