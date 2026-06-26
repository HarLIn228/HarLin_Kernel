#ifndef WAIT_H
#define WAIT_H

#include "harlin_API.h"

int  Harlin_WaitInit(void);
int  Harlin_NotifyExit(int child_pid, int code);
int  Harlin_Wait(int parent_pid, int child_pid, int* out_code);
int  Harlin_WaitAny(int parent_pid, int* out_child_pid, int* out_code);
void Harlin_WaitTest(void);

#endif
