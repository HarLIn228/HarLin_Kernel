#include "scheduler.h"
#include "gdt.h"
#include "io.h"
#include "pmm.h"
#include "vmm.h"

#define MAX_PROCESSES 8

static struct process processes[MAX_PROCESSES];
static int current_process = -1;

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
    for (i = 0; i < MAX_PROCESSES; i++) {
        processes[i].state = PROC_STATE_NONE;
    }
    current_process = -1;
    for (i = 0; i < MAX_PROCESSES; i++) {
        processes[i].next_alloc_virt = 0;
    }
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
            processes[i].page_count = 0;
            processes[i].next_alloc_virt = 0;
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
    unsigned long long flags;

    asm volatile ("pushf; pop %0; cli" : "=r"(flags) : : "memory");

    if (current_process >= 0) {
        processes[current_process].state = PROC_STATE_READY;
        current_process = -1;
    }

    for (i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].state == PROC_STATE_READY) {
            next = i;
            break;
        }
    }

    if (next < 0) {
        asm volatile ("push %0; popf" : : "r"(flags) : "memory");
        asm volatile ("sti; hlt; cli" : : : "memory");
        return;
    }

    current_process = next;
    processes[next].state = PROC_STATE_RUNNING;
    asm volatile ("push %0; popf" : : "r"(flags) : "memory");
    jump_to_user(processes[next].rip, processes[next].rsp, 0);
}

void process_exit(void) __attribute__((noreturn));
void process_exit(void)
{
    unsigned long long flags;

    asm volatile ("pushf; pop %0; cli" : "=r"(flags) : : "memory");
    if (current_process >= 0 && current_process < MAX_PROCESSES) {
        process_free_pages(current_process);
        processes[current_process].state = PROC_STATE_NONE;
        current_process = -1;
    }
    asm volatile ("push %0; popf" : : "r"(flags) : "memory");

    schedule();
    for (;;) {
        asm volatile ("cli; hlt");
    }
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

    if (current_process < 0 || current_process >= MAX_PROCESSES)
        return;

    cur = &processes[current_process];

    cur->r15 = frame[0];
    cur->r14 = frame[1];
    cur->r13 = frame[2];
    cur->r12 = frame[3];
    cur->r11 = frame[4];
    cur->r10 = frame[5];
    cur->r9  = frame[6];
    cur->r8  = frame[7];
    cur->rdi = frame[8];
    cur->rsi = frame[9];
    cur->rbp = frame[10];
    cur->rbx = frame[11];
    cur->rdx = frame[12];
    cur->rcx = frame[13];
    cur->rax = frame[14];
    cur->rip    = frame[16];
    cur->rflags = frame[18];
    cur->rsp    = frame[19];

    cur->state = PROC_STATE_READY;

    next = (current_process + 1) % MAX_PROCESSES;
    while (next != current_process && processes[next].state != PROC_STATE_READY) {
        next = (next + 1) % MAX_PROCESSES;
    }

    if (next == current_process) {
        cur->state = PROC_STATE_RUNNING;
        return;
    }

    current_process = next;
    processes[next].state = PROC_STATE_RUNNING;

    frame[0]  = processes[next].r15;
    frame[1]  = processes[next].r14;
    frame[2]  = processes[next].r13;
    frame[3]  = processes[next].r12;
    frame[4]  = processes[next].r11;
    frame[5]  = processes[next].r10;
    frame[6]  = processes[next].r9;
    frame[7]  = processes[next].r8;
    frame[8]  = processes[next].rdi;
    frame[9]  = processes[next].rsi;
    frame[10] = processes[next].rbp;
    frame[11] = processes[next].rbx;
    frame[12] = processes[next].rdx;
    frame[13] = processes[next].rcx;
    frame[14] = processes[next].rax;
    frame[16] = processes[next].rip;
    frame[18] = processes[next].rflags;
    frame[19] = processes[next].rsp;
}