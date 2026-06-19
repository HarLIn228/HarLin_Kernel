#include "scheduler.h"
#include "gdt.h"

#define MAX_PROCESSES 8

static struct process processes[MAX_PROCESSES];
static int current_process = -1;

extern void jump_to_user(u64 rip, u64 rsp, u64 rdi);

void scheduler_init(void)
{
    int i;
    for (i = 0; i < MAX_PROCESSES; i++) {
        processes[i].state = PROC_STATE_NONE;
    }
    current_process = -1;
}

int process_create(u64 rip, u64 rsp)
{
    int i;
    for (i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].state == PROC_STATE_NONE) {
            processes[i].rip = rip;
            processes[i].rsp = rsp;
            processes[i].rflags = 0x202;
            processes[i].state = PROC_STATE_READY;
            return i;
        }
    }
    return -1;
}

struct process* process_current(void)
{
    if (current_process < 0 || current_process >= MAX_PROCESSES)
        return 0;
    return &processes[current_process];
}

void schedule(void)
{
    int i;
    int next = -1;

    if (current_process >= 0)
        return;

    for (i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].state == PROC_STATE_READY) {
            next = i;
            break;
        }
    }

    if (next < 0)
        return;

    current_process = next;
    processes[next].state = PROC_STATE_RUNNING;
    jump_to_user(processes[next].rip, processes[next].rsp, 0);
}

void process_exit(void)
{
    asm volatile ("cli");
    if (current_process >= 0 && current_process < MAX_PROCESSES) {
        processes[current_process].state = PROC_STATE_NONE;
        current_process = -1;
    }
    asm volatile ("sti");
    schedule();
    for (;;) {
        asm volatile ("cli; hlt");
    }
}
