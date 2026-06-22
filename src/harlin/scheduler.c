#include "scheduler.h"
#include "gdt.h"
#include "io.h"
#include "pmm.h"
#include "vmm.h"
#include "spinlock.h"
#include "rtc.h"
#include "pipe.h"
#include "ipc.h"
#include "percpu.h"
#include "smp.h"

#define USER_CS 0x2B
#define USER_SS 0x33

#define MAX_PROCESSES 16
#define DEFAULT_TIME_SLICE 10

#define HANDLE_KIND_NONE  0
#define HANDLE_KIND_PIPE  1
#define HANDLE_KIND_MSGQ  2
#define HANDLE_KIND_SEM   3

static struct process processes[MAX_PROCESSES];
static int current_process_percpu[SMP_MAX_CPUS] = { [0 ... SMP_MAX_CPUS-1] = -1 };
static struct spinlock scheduler_lock;
static u32 tick_count = 0;

extern void jump_to_user(u64 rip, u64 rsp, u64 rdi);

void save_context(struct process* p)
{
    if (!p) return;
    asm volatile (
        "mov %%rax, %0\n"
        "mov %%rcx, %1\n"
        "mov %%rdx, %2\n"
        "mov %%rbx, %3\n"
        "mov %%rbp, %4\n"
        "mov %%rsi, %5\n"
        "mov %%rdi, %6\n"
        "mov %%r8,  %7\n"
        "mov %%r9,  %8\n"
        "mov %%r10, %9\n"
        "mov %%r11, %10\n"
        "mov %%r12, %11\n"
        "mov %%r13, %12\n"
        "mov %%r14, %13\n"
        "mov %%r15, %14\n"
        : "=m"(p->rax), "=m"(p->rcx), "=m"(p->rdx), "=m"(p->rbx),
          "=m"(p->rbp), "=m"(p->rsi), "=m"(p->rdi), "=m"(p->r8),
          "=m"(p->r9),  "=m"(p->r10), "=m"(p->r11), "=m"(p->r12),
          "=m"(p->r13), "=m"(p->r14), "=m"(p->r15)
        :
        : "memory"
    );
    asm volatile (
        "mov %%rsp, %0\n"
        "pushfq\n"
        "popq %1\n"
        : "=m"(p->rsp), "=r"(p->rflags)
        :
        : "memory"
    );
}

int process_register_handle(int kind, int id)
{
    struct process* p = process_current();
    if (!p) return -1;
    if (p->handle_count >= PROC_MAX_HANDLES) return -1;
    p->handles[p->handle_count] = id;
    p->handle_kinds[p->handle_count] = kind;
    p->handle_count++;
    return 0;
}

void process_unregister_handle(int id)
{
    struct process* p;
    int i;
    p = process_current();
    if (!p) return;
    for (i = 0; i < p->handle_count; i++) {
        if (p->handles[i] == id) {
            int j;
            for (j = i; j + 1 < p->handle_count; j++) {
                p->handles[j] = p->handles[j+1];
                p->handle_kinds[j] = p->handle_kinds[j+1];
            }
            p->handle_count--;
            return;
        }
    }
}

static void process_close_handles(int pid)
{
    int i;
    if (pid < 0 || pid >= MAX_PROCESSES) return;
    for (i = 0; i < processes[pid].handle_count; i++) {
        int kind = processes[pid].handle_kinds[i];
        int id = processes[pid].handles[i];
        if (kind == HANDLE_KIND_PIPE) {
            pipe_close(id);
        } else if (kind == HANDLE_KIND_MSGQ) {
            Harlin_MsgDestroy(id);
        } else if (kind == HANDLE_KIND_SEM) {
            Harlin_SemDestroy(id);
        }
    }
    processes[pid].handle_count = 0;
}

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
    int c;
    int j;
    spinlock_init(&scheduler_lock);
    for (i = 0; i < MAX_PROCESSES; i++) {
        processes[i].state = PROC_STATE_NONE;
        processes[i].pid = i;
        processes[i].cpu = -1;
        processes[i].priority = 1;
        processes[i].time_slice = DEFAULT_TIME_SLICE;
        processes[i].sleep_until = 0;
        processes[i].next_alloc_virt = 0;
        processes[i].pml4_phys = 0;
        processes[i].kernel_stack_top = 0;
        processes[i].kernel_stack_base = 0;
        processes[i].kernel_stack_pages_count = 0;
        for (j = 0; j < 4; j++)
            processes[i].kernel_stack_pages_phys[j] = 0;
        processes[i].handle_count = 0;
    }
    for (c = 0; c < SMP_MAX_CPUS; c++)
        current_process_percpu[c] = -1;
}

int process_create(u64 rip, u64 rsp)
{
    int i;
    int j;
    u64 stack_phys[4];
    u64 stack_base;
    u64 stack_top;
    int allocated_pages = 0;
    int ok = 0;
    u64 saved_pml4;
    u64 cloned_pml4;
    int map_ok = 1;

    for (j = 0; j < 4; j++)
        stack_phys[j] = 0;

    spinlock_acquire(&scheduler_lock);
    for (i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].state == PROC_STATE_NONE)
            break;
    }
    if (i >= MAX_PROCESSES) {
        spinlock_release(&scheduler_lock);
        return -1;
    }

    stack_base = KERNEL_STACK_BASE + (u64)i * KERNEL_STACK_SIZE;
    stack_top = stack_base + KERNEL_STACK_SIZE;

    for (j = 0; j < 4; j++) {
        stack_phys[j] = pmm_alloc();
        if (!stack_phys[j])
            break;
        allocated_pages = j + 1;
    }
    if (allocated_pages != 4) {
        for (j = 0; j < allocated_pages; j++)
            pmm_free(stack_phys[j]);
        spinlock_release(&scheduler_lock);
        return -1;
    }

    saved_pml4 = vmm_current_pml4();
    cloned_pml4 = vmm_clone_kernel_pml4();
    if (cloned_pml4) {
        vmm_switch(cloned_pml4);
        for (j = 0; j < 4; j++) {
            if (vmm_map(stack_base + (u64)j * 4096, stack_phys[j], VMM_PRESENT | VMM_WRITABLE) != 0) {
                map_ok = 0;
                break;
            }
        }
        {
            u64 verify_base = vmm_get_phys(stack_base);
            if (verify_base != stack_phys[0])
                map_ok = 0;
        }
        vmm_switch(saved_pml4);
        if (map_ok)
            ok = 1;
    }
    (void)cloned_pml4;

    if (!ok) {
        for (j = 0; j < 4; j++) {
            pmm_free(stack_phys[j]);
        }
        spinlock_release(&scheduler_lock);
        return -1;
    }

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
    processes[i].pml4_phys = cloned_pml4;
    processes[i].kernel_stack_top = stack_top;
    processes[i].kernel_stack_base = stack_base;
    processes[i].kernel_stack_pages_count = 4;
    for (j = 0; j < 4; j++)
        processes[i].kernel_stack_pages_phys[j] = stack_phys[j];
    processes[i].handle_count = 0;
    spinlock_release(&scheduler_lock);
    return i;
}

void process_set_kernel_stack(int pid, u64 stack_top)
{
    if (pid < 0 || pid >= MAX_PROCESSES)
        return;
    processes[pid].kernel_stack_top = stack_top;
}

static int current_cpu_id(void)
{
    int c = smp_current_cpu_id();
    if (c < 0 || c >= SMP_MAX_CPUS) c = 0;
    return c;
}

struct process* process_current(void)
{
    int c = current_cpu_id();
    int cp = current_process_percpu[c];
    if (cp < 0 || cp >= MAX_PROCESSES)
        return 0;
    return &processes[cp];
}

struct process* process_get(int pid)
{
    if (pid < 0 || pid >= MAX_PROCESSES)
        return 0;
    return &processes[pid];
}

void process_set_current(int pid)
{
    int c = current_cpu_id();
    if (pid >= 0 && pid < MAX_PROCESSES)
        current_process_percpu[c] = pid;
    else
        current_process_percpu[c] = -1;
}

static int pick_next_process(int cpu)
{
    int i;
    int best = -1;
    u32 best_prio = 0;
    for (i = 0; i < MAX_PROCESSES; i++) {
        if ((processes[i].state == PROC_STATE_READY || processes[i].state == PROC_STATE_RUNNING) &&
            (processes[i].cpu == -1 || processes[i].cpu == cpu)) {
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
    int prev;
    int cpu;
    unsigned long long flags;

    asm volatile ("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    cpu = current_cpu_id();
    prev = current_process_percpu[cpu];

    if (prev >= 0 && processes[prev].state == PROC_STATE_RUNNING) {
        save_context(&processes[prev]);
        processes[prev].state = PROC_STATE_READY;
    }
    current_process_percpu[cpu] = -1;

    next = pick_next_process(cpu);

    if (next < 0) {
        asm volatile ("pushq %0; popfq" : : "r"(flags) : "memory");
        while (pick_next_process(cpu) < 0) {
            asm volatile ("sti; hlt; cli" : : : "memory");
        }
        next = pick_next_process(cpu);
    }

    current_process_percpu[cpu] = next;
    processes[next].state = PROC_STATE_RUNNING;
    processes[next].time_slice = DEFAULT_TIME_SLICE;
    processes[next].cpu = cpu;
    if (processes[next].pml4_phys)
        vmm_switch(processes[next].pml4_phys);
    tss_set_rsp0_for_cpu(cpu, processes[next].kernel_stack_top);
    asm volatile ("pushq %0; popfq" : : "r"(flags) : "memory");
    if (processes[next].first_run) {
        processes[next].first_run = 0;
    }
    jump_to_user(processes[next].rip, processes[next].rsp, 0);
}

static void process_free_kernel_stack(int pid)
{
    int j;
    if (pid < 0 || pid >= MAX_PROCESSES)
        return;
    if (!processes[pid].kernel_stack_base)
        return;
    for (j = 0; j < processes[pid].kernel_stack_pages_count; j++) {
        u64 v = processes[pid].kernel_stack_base + (u64)j * 4096;
        vmm_unmap(v);
        if (processes[pid].kernel_stack_pages_phys[j])
            pmm_free(processes[pid].kernel_stack_pages_phys[j]);
        processes[pid].kernel_stack_pages_phys[j] = 0;
    }
    processes[pid].kernel_stack_base = 0;
    processes[pid].kernel_stack_top = 0;
    processes[pid].kernel_stack_pages_count = 0;
}

void process_exit(void) __attribute__((noreturn));
void process_exit(void)
{
    unsigned long long flags;
    int cpu;
    int cp;

    asm volatile ("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    cpu = current_cpu_id();
    cp = current_process_percpu[cpu];
    if (cp >= 0 && cp < MAX_PROCESSES) {
        process_close_handles(cp);
        process_free_pages(cp);
        process_free_kernel_stack(cp);
        processes[cp].state = PROC_STATE_NONE;
        current_process_percpu[cpu] = -1;
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
    int cpu = current_cpu_id();
    int cp = current_process_percpu[cpu];
    if (cp < 0 || cp >= MAX_PROCESSES)
        return;
    save_context(&processes[cp]);
    processes[cp].sleep_until = tick_count + ms;
    processes[cp].state = PROC_STATE_SLEEPING;
    schedule();
}

void process_block_current(void)
{
    int cpu = current_cpu_id();
    int cp = current_process_percpu[cpu];
    if (cp < 0 || cp >= MAX_PROCESSES)
        return;
    save_context(&processes[cp]);
    processes[cp].state = PROC_STATE_BLOCKED;
    schedule();
}

void process_wake(int pid)
{
    if (pid >= 0 && pid < MAX_PROCESSES && processes[pid].state == PROC_STATE_BLOCKED)
        processes[pid].state = PROC_STATE_READY;
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
    int cpu;
    int cp;
    struct process* cur;

    scheduler_tick();

    cpu = current_cpu_id();
    cp = current_process_percpu[cpu];

    if (cp < 0 || cp >= MAX_PROCESSES) {
        next = pick_next_process(cpu);
        if (next < 0) return;
        {
            int i;
            for (i = 0; i < MAX_PROCESSES; i++) {
                if (processes[i].state == PROC_STATE_RUNNING && i != cp)
                    processes[i].state = PROC_STATE_READY;
            }
        }
        current_process_percpu[cpu] = next;
    processes[next].state = PROC_STATE_RUNNING;
    processes[next].time_slice = DEFAULT_TIME_SLICE;
    processes[next].cpu = cpu;
    if (processes[next].pml4_phys)
        vmm_switch(processes[next].pml4_phys);
    tss_set_rsp0_for_cpu(cpu, processes[next].kernel_stack_top);
    if (processes[next].first_run) {
        processes[next].first_run = 0;
        return;
    }
        cur = &processes[next];
        frame[0]  = cur->rax;
        frame[1]  = cur->rcx;
        frame[2]  = cur->rdx;
        frame[3]  = cur->rbx;
        frame[4]  = cur->rbp;
        frame[5]  = cur->rsi;
        frame[6]  = cur->rdi;
        frame[7]  = cur->r8;
        frame[8]  = cur->r9;
        frame[9]  = cur->r10;
        frame[10] = cur->r11;
        frame[11] = cur->r12;
        frame[12] = cur->r13;
        frame[13] = cur->r14;
        frame[14] = cur->r15;
        frame[16] = cur->rip;
        frame[17] = USER_CS;
        frame[18] = cur->rflags;
        frame[19] = cur->rsp;
        frame[20] = USER_SS;
        return;
    }

    cur = &processes[cp];

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

    next = pick_next_process(cpu);

    if (next < 0 || next == cp) {
        cur->state = PROC_STATE_RUNNING;
        cur->time_slice = DEFAULT_TIME_SLICE;
        return;
    }

    current_process_percpu[cpu] = next;
    processes[next].state = PROC_STATE_RUNNING;
    processes[next].time_slice = DEFAULT_TIME_SLICE;
    processes[next].cpu = cpu;

    if (processes[next].pml4_phys)
        vmm_switch(processes[next].pml4_phys);
    tss_set_rsp0_for_cpu(cpu, processes[next].kernel_stack_top);

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

void scheduler_secondary_loop(void)
{
    for (;;) {
        schedule();
    }
}

int scheduler_try_run_user(void)
{
    int cpu = current_cpu_id();
    int next;
    if (current_process_percpu[cpu] >= 0)
        return -1;
    next = pick_next_process(cpu);
    if (next < 0) return -1;
    current_process_percpu[cpu] = next;
    processes[next].state = PROC_STATE_RUNNING;
    processes[next].time_slice = DEFAULT_TIME_SLICE;
    processes[next].cpu = cpu;
    if (processes[next].pml4_phys)
        vmm_switch(processes[next].pml4_phys);
    if (processes[next].kernel_stack_top)
        tss_set_rsp0_for_cpu(cpu, processes[next].kernel_stack_top);
    if (processes[next].first_run) {
        processes[next].first_run = 0;
        return 0;
    }
    return 0;
}

void scheduler_dispatch_from_timer(unsigned long* frame)
{
    timer_handler(frame);
}
