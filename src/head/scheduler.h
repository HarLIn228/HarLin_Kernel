#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "harlin_API.h"

#define PROC_STATE_NONE   0
#define PROC_STATE_READY  1
#define PROC_STATE_RUNNING 2

struct process {
    u64 rax, rcx, rdx, rbx, rbp, rsi, rdi;
    u64 r8, r9, r10, r11, r12, r13, r14, r15;
    u64 rip;
    u64 rsp;
    u64 rflags;
    int state;
    u64 user_pages[16];
    u64 user_vaddrs[16];
    int page_count;
    u64 next_alloc_virt;
};

void scheduler_init(void);
int  process_create(u64 rip, u64 rsp);
void schedule(void);
void process_exit(void) __attribute__((noreturn));
struct process* process_current(void);
void timer_init(void);
void timer_handler(unsigned long* frame);

#endif
