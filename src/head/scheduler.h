#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "harlin_API.h"

#define PROC_STATE_NONE     0
#define PROC_STATE_READY    1
#define PROC_STATE_RUNNING  2
#define PROC_STATE_SLEEPING 3
#define PROC_STATE_BLOCKED  4

struct process {
    u64 rax, rcx, rdx, rbx, rbp, rsi, rdi;
    u64 r8, r9, r10, r11, r12, r13, r14, r15;
    u64 rip;
    u64 rsp;
    u64 rflags;
    int state;
    int first_run;
    int pid;
    int cpu;
    u32 priority;
    u32 time_slice;
    u32 sleep_until;
    u64 user_pages[16];
    u64 user_vaddrs[16];
    int page_count;
    u64 next_alloc_virt;
};

void scheduler_init(void);
int  process_create(u64 rip, u64 rsp);
struct process* process_get(int pid);
void process_set_current(int pid);
void schedule(void);
void process_exit(void) __attribute__((noreturn));
struct process* process_current(void);
void timer_init(void);
void timer_handler(unsigned long* frame);
void scheduler_tick(void);
void scheduler_add_ready(int pid);
void scheduler_sleep(u32 ms);
void process_block_current(void);
void process_wake(int pid);
int scheduler_get_load(int cpu);

#endif
