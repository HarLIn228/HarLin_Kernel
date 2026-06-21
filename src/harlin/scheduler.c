#include "scheduler.h"
#include "gdt.h"
#include "io.h"
#include "pmm.h"
#include "vmm.h"
#include "spinlock.h"
#include "rtc.h"

#define USER_CS 0x2B
#define USER_SS 0x33

#define MAX_PROCESSES 16
#define DEFAULT_TIME_SLICE 10

static struct process processes[MAX_PROCESSES];
static int current_process = -1;
static struct spinlock scheduler_lock;
static u32 tick_count = 0;

extern void jump_to_user(u64 rip, u64 rsp, u64 rdi);

static void process_free_pages(int pid)
{
    int i;
    u64 virt;
    u64 phys;

    if (pid < 0 || pid >= MAX_PROCESSES)
        return;

    for (i = 0; i < processes[pid].page_count; i++) {
        virt = processes[pid].user_vaddrs[i];
        phys = processes[pid].user_pages[i];
        if (virt)
            vmm_unmap(virt);
        if (phys)
            pmm_free(phys);
    }
    processes[pid].page_count = 0;
}

void scheduler_init(void)
{
    int i;
    spinlock_init(&scheduler_lock);
    for (i = 0; i < MAX_PROCESSES; i++) {
        processes[i].state = PROC_STATE_NONE;
        processes[i].pid = i;
        processes[i].cpu = -1;
        processes[i].priority = 1;
        processes[i].time_slice = DEFAULT_TIME_SLICE;
        processes[i].sleep_until = 0;
        processes[i].next_alloc_virt = 0;
    }
    current_process = -1;
}

int process_create(u64 rip, u64 rsp)
{
    int i;
    spinlock_acquire(&scheduler_lock);
    for (i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].state == PROC_STATE_NONE) {
            processes[i].rip = rip;
            processes[i].rsp = rsp;
            processes[i].rflags = 0x202;
            processes[i].state = PROC_STATE_READY;
            processes[i].first_run = 1;
            processes[i].page_count = 0;
            processes[i].next_alloc_virt = 0;
            processes[i].cpu = -1;
            processes[i].priority = 1;
            processes[i].time_slice = DEFAULT_TIME_SLICE;
            processes[i].sleep_until = 0;
            spinlock_release(&scheduler_lock);
            return i;
        }
    }
    spinlock_release(&scheduler_lock);
    return -1;
}

struct process* process_current(void)
{
    if (current_process < 0 || current_process >= MAX_PROCESSES)
        return 0;
    return &processes[current_process];
}

struct process* process_get(int pid)
{
    if (pid < 0 || pid >= MAX_PROCESSES)
        return 0;
    return &processes[pid];
}

void process_set_current(int pid)
{
    if (pid >= 0 && pid < MAX_PROCESSES)
        current_process = pid;
}

static int pick_next_process(void)
{
    int i;
    int best = -1;
    u32 best_prio = 0;
    for (i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].state == PROC_STATE_READY || processes[i].state == PROC_STATE_RUNNING) {
            if (best < 0 || processes[i].priority > best_prio) {
                best = i;
                best_prio = processes[i].priority;
            }
        }
    }
    return best;
}

void schedule(void)
{
    int next = -1;
    unsigned long long flags;

    asm volatile ("pushfq; popq %0; cli" : "=r"(flags) : : "memory");

    if (current_process >= 0 && processes[current_process].state == PROC_STATE_RUNNING)
        processes[current_process].state = PROC_STATE_READY;
    current_process = -1;

    next = pick_next_process();

    if (next < 0) {
        asm volatile ("pushq %0; popfq" : : "r"(flags) : "memory");
        asm volatile ("sti; hlt; cli" : : : "memory");
        return;
    }

    current_process = next;
    processes[next].state = PROC_STATE_RUNNING;
    processes[next].time_slice = DEFAULT_TIME_SLICE;
    asm volatile ("pushq %0; popfq" : : "r"(flags) : "memory");
    jump_to_user(processes[next].rip, processes[next].rsp, 0);
}

void process_exit(void) __attribute__((noreturn));
void process_exit(void)
{
    unsigned long long flags;

    asm volatile ("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    if (current_process >= 0 && current_process < MAX_PROCESSES) {
        process_free_pages(current_process);
        processes[current_process].state = PROC_STATE_NONE;
        current_process = -1;
    }
    asm volatile ("pushq %0; popfq" : : "r"(flags) : "memory");

    schedule();
    for (;;) {
        asm volatile ("cli; hlt");
    }
}

void scheduler_tick(void)
{
    int i;
    tick_count++;
    for (i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].state == PROC_STATE_SLEEPING) {
            if (processes[i].sleep_until && tick_count >= processes[i].sleep_until)
                processes[i].state = PROC_STATE_READY;
        }
    }
}

void scheduler_add_ready(int pid)
{
    if (pid >= 0 && pid < MAX_PROCESSES)
        processes[pid].state = PROC_STATE_READY;
}

void scheduler_sleep(u32 ms)
{
    if (current_process < 0 || current_process >= MAX_PROCESSES)
        return;
    processes[current_process].sleep_until = tick_count + ms;
    processes[current_process].state = PROC_STATE_SLEEPING;
    schedule();
}

int scheduler_get_load(int cpu)
{
    int i;
    int count = 0;
    for (i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].state != PROC_STATE_NONE && (cpu < 0 || processes[i].cpu == cpu))
            count++;
    }
    return count;
}

void timer_init(void)
{
    u16 divisor = 1193180 / 100;
    outb(0x43, 0x36);
    outb(0x40, (u8)(divisor & 0xFF));
    outb(0x40, (u8)(divisor >> 8));
    outb(0x21, inb(0x21) & ~0x01);
}

void timer_handler(unsigned long* frame)
{
    int next;
    struct process* cur;

    scheduler_tick();

    if (current_process < 0 || current_process >= MAX_PROCESSES)
        return;

    cur = &processes[current_process];

    cur->rax = frame[0];
    cur->rcx = frame[1];
    cur->rdx = frame[2];
    cur->rbx = frame[3];
    cur->rbp = frame[4];
    cur->rsi = frame[5];
    cur->rdi = frame[6];
    cur->r8  = frame[7];
    cur->r9  = frame[8];
    cur->r10 = frame[9];
    cur->r11 = frame[10];
    cur->r12 = frame[11];
    cur->r13 = frame[12];
    cur->r14 = frame[13];
    cur->r15 = frame[14];
    cur->rip    = frame[16];
    cur->rflags = frame[18];
    cur->rsp    = frame[19];

    if (cur->time_slice > 0)
        cur->time_slice--;

    if (cur->time_slice > 0 && cur->state == PROC_STATE_RUNNING)
        return;

    cur->state = PROC_STATE_READY;

    next = pick_next_process();

    if (next < 0 || next == current_process) {
        cur->state = PROC_STATE_RUNNING;
        cur->time_slice = DEFAULT_TIME_SLICE;
        return;
    }

    current_process = next;
    processes[next].state = PROC_STATE_RUNNING;
    processes[next].time_slice = DEFAULT_TIME_SLICE;

    if (!processes[next].first_run) {
        frame[0]  = processes[next].rax;
        frame[1]  = processes[next].rcx;
        frame[2]  = processes[next].rdx;
        frame[3]  = processes[next].rbx;
        frame[4]  = processes[next].rbp;
        frame[5]  = processes[next].rsi;
        frame[6]  = processes[next].rdi;
        frame[7]  = processes[next].r8;
        frame[8]  = processes[next].r9;
        frame[9]  = processes[next].r10;
        frame[10] = processes[next].r11;
        frame[11] = processes[next].r12;
        frame[12] = processes[next].r13;
        frame[13] = processes[next].r14;
        frame[14] = processes[next].r15;
    }
    processes[next].first_run = 0;
    frame[16] = processes[next].rip;
    frame[17] = USER_CS;
    frame[18] = processes[next].rflags;
    frame[19] = processes[next].rsp;
    frame[20] = USER_SS;
}
