#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "harlin_API.h"

#define PROC_STATE_NONE     0
#define PROC_STATE_READY    1
#define PROC_STATE_RUNNING  2
#define PROC_STATE_SLEEPING 3
#define PROC_STATE_BLOCKED  4

#define PROC_MAX_HANDLES 16

struct process {
    u64 rax, rcx, rdx, rbx, rbp, rsi, rdi;
    u64 r8, r9, r10, r11, r12, r13, r14, r15;
    u64 rip;
    u64 rsp;
    u64 rflags;
    u64 fs_base;
    int state;
    int first_run;
    int pid;
    int cpu;
    u32 priority;
    u32 time_slice;
    u32 sleep_until;
    u64 user_pages[64];
    u64 user_vaddrs[64];
    int page_count;
    u64 next_alloc_virt;
    u64 pml4_phys;
    u64 kernel_stack_top;
    u64 kernel_stack_base;
    u64 kernel_stack_pages_phys[4];
    int kernel_stack_pages_count;
    int handles[PROC_MAX_HANDLES];
    int handle_kinds[PROC_MAX_HANDLES];
    int handle_count;
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
void save_context(struct process* p);
int process_register_handle(int kind, int id);
void process_unregister_handle(int id);
void scheduler_secondary_loop(void) __attribute__((noreturn));
int scheduler_try_run_user(void);
void scheduler_dispatch_from_timer(unsigned long* frame);
void process_set_kernel_stack(int pid, u64 stack_top);

#define Harlin_SchedulerInit         scheduler_init
#define Harlin_ProcessCreate         process_create
#define Harlin_ProcessGet            process_get
#define Harlin_ProcessSetCurrent     process_set_current
#define Harlin_Schedule              schedule
#define Harlin_ProcessExit           process_exit
#define Harlin_ProcessCurrent        process_current
#define Harlin_TimerInit             timer_init
#define Harlin_TimerHandler          timer_handler
#define Harlin_SchedulerTick         scheduler_tick
#define Harlin_SchedulerAddReady     scheduler_add_ready
#define Harlin_SchedulerSleep        scheduler_sleep
#define Harlin_ProcessBlockCurrent   process_block_current
#define Harlin_ProcessWake           process_wake
#define Harlin_SchedulerGetLoad      scheduler_get_load
#define Harlin_SaveContext           save_context
#define Harlin_ProcessRegisterHandle process_register_handle
#define Harlin_ProcessUnregisterHandle process_unregister_handle
#define Harlin_SchedulerTryRunUser   scheduler_try_run_user
#define Harlin_SchedulerDispatchFromTimer scheduler_dispatch_from_timer
#define Harlin_ProcessSetKernelStack process_set_kernel_stack

#endif
