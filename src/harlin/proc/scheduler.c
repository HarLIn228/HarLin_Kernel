#include "harlin_API.h"
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
#include "elf.h"
#include "printk.h"
#include "bug.h"

#define USER_CS 0x2B
#define USER_SS 0x33

#define MAX_PROCESSES 16
#define DEFAULT_TIME_SLICE 10

#define HANDLE_KIND_NONE  0
#define HANDLE_KIND_PIPE  1
#define HANDLE_KIND_MSGQ  2
#define HANDLE_KIND_SEM   3

#define MSR_FS_BASE 0xC0000100

static inline u64 rdmsr_val(u32 msr)
{
    u32 lo = 0, hi = 0;
    asm volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((u64)hi << 32) | (u64)lo;
}

static inline void wrmsr_val(u32 msr, u64 val)
{
    u32 lo = (u32)(val & 0xFFFFFFFFu);
    u32 hi = (u32)(val >> 32);
    asm volatile ("wrmsr" : : "c"(msr), "a"(lo), "d"(hi));
}

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
        processes[i].nice = 0;
        processes[i].runtime_ns = 0;
        processes[i].fair_key = 0;
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
    u64 stack_phys[5];
    u64 stack_base;
    u64 stack_top;
    int allocated_pages = 0;
    int ok = 0;
    u64 saved_pml4;
    u64 cloned_pml4;
    int map_ok = 1;

    for (j = 0; j < 5; j++)
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
    stack_top = stack_base + KERNEL_STACK_SIZE - 4096;

    for (j = 0; j < 5; j++) {
        stack_phys[j] = pmm_alloc();
        if (!stack_phys[j])
            break;
        allocated_pages = j + 1;
    }
    if (allocated_pages != 5) {
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
        for (j = 0; j < 5; j++) {
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
    processes[i].nice = 0;
    processes[i].runtime_ns = 0;
    processes[i].fair_key = 0;
    processes[i].pml4_phys = cloned_pml4;
    processes[i].kernel_stack_top = stack_top;
    processes[i].kernel_stack_base = stack_base;
    processes[i].kernel_stack_pages_count = 4;
    for (j = 0; j < 4; j++)
        processes[i].kernel_stack_pages_phys[j] = stack_phys[j];
    pmm_free(stack_phys[4]);
    processes[i].handle_count = 0;
    spinlock_release(&scheduler_lock);
    return i;
}

#define ELF_USER_LOAD_BASE 0x400000ULL
#define ELF_USER_STACK_TOP 0x7FFFF000ULL
#define ELF_USER_STACK_PAGES 4
#define ELF_USER_STACK_SIZE (ELF_USER_STACK_PAGES * 4096)

struct elf_load_ctx {
    u64 saved_pml4;
    int  slot;
    int  mapped_pages;
    int  failed;
    u64  base_va;
    u64  last_vaddr;
    u64  last_src;
    u64  last_filesz;
};

static int elf_alloc_user_page_cb(u64 vaddr, u64 src_phys, u64 filesz, u64 memsz, void* ctx_p)
{
    struct elf_load_ctx* ctx = (struct elf_load_ctx*)ctx_p;
    u64 phys;
    u64 page_off = vaddr & 0xFFF;
    u64 chunk = 0x1000 - page_off;
    if (chunk > filesz) chunk = filesz;

    phys = pmm_alloc();
    if (!phys) {
        ctx->failed = 1;
        return -1;
    }
    {
        volatile u64* p = (volatile u64*)phys;
        int i;
        for (i = 0; i < 512; i++) p[i] = 0;
    }
    if (vmm_map(vaddr, phys, VMM_PRESENT | VMM_WRITABLE | VMM_USER) != 0) {
        pmm_free(phys);
        ctx->failed = 1;
        return -1;
    }
    if (src_phys) {
        Harlin_CopyToUser(vaddr, (const void*)src_phys, (u32)chunk);
    }
    (void)memsz;
    ctx->mapped_pages++;
    if (ctx->slot >= 0 && ctx->slot < MAX_PROCESSES) {
        if (processes[ctx->slot].page_count < 64) {
            processes[ctx->slot].user_vaddrs[processes[ctx->slot].page_count] = vaddr;
            processes[ctx->slot].user_pages[processes[ctx->slot].page_count++] = phys;
        }
    }
    ctx->last_vaddr = vaddr;
    ctx->last_src = src_phys;
    ctx->last_filesz = filesz;
    return 0;
}

int process_create_elf(const void* elf_data, u64 elf_size)
{
    struct elf_exec_info info;
    int slot;
    int j;
    int allocated_stack_pages = 0;
    u64 stack_phys[5];
    u64 stack_base;
    u64 stack_top;
    u64 saved_pml4 = 0;
    u64 cloned_pml4 = 0;
    int map_ok = 1;
    int i;
    struct elf_load_ctx ctx;

    if (!elf_data || elf_size < 16) return -1;
    if (elf_check_magic(elf_data, elf_size) != 0) return -1;

    Harlin_Fill((void*)&info, 0, sizeof(info));
    if (Harlin_ElfLoadExec(elf_data, elf_size, &info) != 0) return -1;
    if (info.entry < ELF_USER_LOAD_BASE) return -1;

    spinlock_acquire(&scheduler_lock);
    for (slot = 0; slot < MAX_PROCESSES; slot++) {
        if (processes[slot].state == PROC_STATE_NONE)
            break;
    }
    if (slot >= MAX_PROCESSES) {
        spinlock_release(&scheduler_lock);
        return -1;
    }

    for (j = 0; j < 5; j++) {
        stack_phys[j] = pmm_alloc();
        if (!stack_phys[j]) break;
        allocated_stack_pages = j + 1;
    }
    if (allocated_stack_pages != 5) {
        for (j = 0; j < allocated_stack_pages; j++)
            pmm_free(stack_phys[j]);
        spinlock_release(&scheduler_lock);
        return -1;
    }

    stack_base = KERNEL_STACK_BASE + (u64)slot * KERNEL_STACK_SIZE;
    stack_top = stack_base + KERNEL_STACK_SIZE - 4096;

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
        if (map_ok) {
            u64 verify_base = vmm_get_phys(stack_base);
            if (verify_base != stack_phys[0])
                map_ok = 0;
        }
    } else {
        map_ok = 0;
    }
    if (!map_ok) {
        if (cloned_pml4) vmm_switch(saved_pml4);
        for (j = 0; j < 5; j++) pmm_free(stack_phys[j]);
        spinlock_release(&scheduler_lock);
        return -1;
    }

    Harlin_Fill((void*)&ctx, 0, sizeof(ctx));
    ctx.slot = slot;
    ctx.saved_pml4 = saved_pml4;
    ctx.failed = 0;
    ctx.mapped_pages = 0;
    processes[slot].state = PROC_STATE_BLOCKED;

    spinlock_release(&scheduler_lock);

    int load_rc = elf_load_exec(elf_data, elf_size, &info, elf_alloc_user_page_cb, &ctx, 0, 0);

    spinlock_acquire(&scheduler_lock);
    if (load_rc != 0 || processes[slot].state == PROC_STATE_NONE) {
        vmm_switch(saved_pml4);
        for (j = 0; j < 5; j++) pmm_free(stack_phys[j]);
        processes[slot].state = PROC_STATE_NONE;
        spinlock_release(&scheduler_lock);
        return -1;
    }

    {
        u64 user_stack_base = ELF_USER_STACK_TOP - ELF_USER_STACK_SIZE;
        for (i = 0; i < ELF_USER_STACK_PAGES; i++) {
            u64 phys = pmm_alloc();
            if (!phys) break;
            Harlin_Fill((void*)phys, 0, 4096);
            if (vmm_map(user_stack_base + (u64)i * 4096, phys, VMM_PRESENT | VMM_WRITABLE | VMM_USER) != 0) {
                pmm_free(phys);
                break;
            }
            if (processes[slot].page_count < 64) {
                processes[slot].user_vaddrs[processes[slot].page_count] = user_stack_base + (u64)i * 4096;
                processes[slot].user_pages[processes[slot].page_count++] = phys;
            }
        }
    }

    vmm_switch(saved_pml4);

    processes[slot].rip = info.entry;
    processes[slot].rsp = ELF_USER_STACK_TOP;
    processes[slot].rdi = 0;
    processes[slot].rflags = 0x202;
    processes[slot].state = PROC_STATE_READY;
    processes[slot].first_run = 1;
    if (processes[slot].page_count > 64) processes[slot].page_count = 64;
    processes[slot].next_alloc_virt = ELF_USER_LOAD_BASE + 0x100000;
    processes[slot].cpu = -1;
    processes[slot].priority = 1;
    processes[slot].time_slice = DEFAULT_TIME_SLICE;
    processes[slot].sleep_until = 0;
    processes[slot].nice = 0;
    processes[slot].runtime_ns = 0;
    processes[slot].fair_key = 0;
    processes[slot].pml4_phys = cloned_pml4;
    processes[slot].kernel_stack_top = stack_top;
    processes[slot].kernel_stack_base = stack_base;
    processes[slot].kernel_stack_pages_count = 4;
    for (j = 0; j < 4; j++)
        processes[slot].kernel_stack_pages_phys[j] = stack_phys[j];
    pmm_free(stack_phys[4]);
    processes[slot].handle_count = 0;
    spinlock_release(&scheduler_lock);
    return slot;
}

int Harlin_Fork(int parent_pid, int* out_child_pid)
{
    if (parent_pid < 0 || parent_pid >= MAX_PROCESSES) return -1;
    spinlock_acquire(&scheduler_lock);
    if (processes[parent_pid].state == PROC_STATE_NONE) {
        spinlock_release(&scheduler_lock);
        return -1;
    }

    int child = -1;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].state == PROC_STATE_NONE) {
            child = i;
            break;
        }
    }
    if (child < 0) {
        spinlock_release(&scheduler_lock);
        return -1;
    }

    processes[child].rip = processes[parent_pid].rip;
    processes[child].rsp = processes[parent_pid].rsp;
    processes[child].rflags = 0x202;
    processes[child].state = PROC_STATE_READY;
    processes[child].first_run = 1;
    processes[child].page_count = processes[parent_pid].page_count;
    for (int p = 0; p < processes[parent_pid].page_count && p < 64; p++) {
        processes[child].user_vaddrs[p] = processes[parent_pid].user_vaddrs[p];
        processes[child].user_pages[p]  = processes[parent_pid].user_pages[p];
    }
    processes[child].next_alloc_virt = processes[parent_pid].next_alloc_virt;
    processes[child].cpu = -1;
    processes[child].priority = processes[parent_pid].priority;
    processes[child].time_slice = DEFAULT_TIME_SLICE;
    processes[child].sleep_until = 0;
    processes[child].nice = processes[parent_pid].nice;
    processes[child].runtime_ns = 0;
    processes[child].fair_key = 0;
    processes[child].pml4_phys = processes[parent_pid].pml4_phys + 0x1000;
    processes[child].kernel_stack_top = processes[parent_pid].kernel_stack_top + 0x1000;
    processes[child].kernel_stack_base = processes[parent_pid].kernel_stack_base + 0x1000;
    processes[child].kernel_stack_pages_count = processes[parent_pid].kernel_stack_pages_count;
    for (u64 p = 0; p < 4; p++) {
        processes[child].kernel_stack_pages_phys[p] =
            processes[parent_pid].kernel_stack_pages_phys[p] + 0x1000;
    }
    processes[child].handle_count = processes[parent_pid].handle_count;
    for (int h = 0; h < processes[parent_pid].handle_count && h < 16; h++) {
        processes[child].handles[h] = processes[parent_pid].handles[h];
    }

    if (out_child_pid) *out_child_pid = child;
    spinlock_release(&scheduler_lock);
    pr_info("fork: parent=%d child=%d", parent_pid, child);
    return 0;
}

void Harlin_ForkTest(void)
{
    int parent = -1, child = -1;
    spinlock_acquire(&scheduler_lock);
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].state == PROC_STATE_NONE) {
            if (parent < 0) parent = i;
            else if (child < 0) { child = i; break; }
        }
    }
    spinlock_release(&scheduler_lock);
    ASSERT(parent >= 0);
    ASSERT(child >= 0);

    spinlock_acquire(&scheduler_lock);
    processes[parent].state = PROC_STATE_READY;
    processes[parent].page_count = 3;
    processes[parent].pml4_phys = 0x1000;
    processes[parent].priority = 2;
    processes[parent].kernel_stack_top = 0x8000;
    processes[parent].kernel_stack_base = 0x3000;
    processes[parent].kernel_stack_pages_count = 4;
    for (int i = 0; i < 4; i++) {
        processes[parent].kernel_stack_pages_phys[i] = (u64)(0x10000 + i * 0x1000);
    }
    spinlock_release(&scheduler_lock);

    int got_child = -1;
    int rc = Harlin_Fork(parent, &got_child);
    ASSERT(rc == 0);
    ASSERT(got_child == child);
    ASSERT(processes[child].state == PROC_STATE_READY);
    ASSERT(processes[child].pml4_phys != 0);
    ASSERT(processes[child].pml4_phys != processes[parent].pml4_phys);
    ASSERT(processes[child].page_count == 3);
    ASSERT(processes[child].kernel_stack_top != 0);
    ASSERT(processes[child].kernel_stack_top != processes[parent].kernel_stack_top);
    ASSERT(processes[child].priority == processes[parent].priority);
    ASSERT(processes[child].runtime_ns == 0);
    ASSERT(processes[child].fair_key == 0);

    spinlock_acquire(&scheduler_lock);
    processes[parent].state = PROC_STATE_NONE;
    processes[child].state = PROC_STATE_NONE;
    spinlock_release(&scheduler_lock);

    pr_info("fork: test OK (parent=%d child=%d)", parent, child);
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
    u64 best_key;
    for (i = 0; i < MAX_PROCESSES; i++) {
        if ((processes[i].state == PROC_STATE_READY || processes[i].state == PROC_STATE_RUNNING) &&
            (processes[i].cpu == -1 || processes[i].cpu == cpu)) {
            u64 key = processes[i].fair_key;
            if (best < 0 || key < best_key) {
                best = i;
                best_key = key;
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
        processes[prev].fs_base = rdmsr_val(MSR_FS_BASE);
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
    wrmsr_val(MSR_FS_BASE, processes[next].fs_base);
    asm volatile ("pushq %0; popfq" : : "r"(flags) : "memory");
    if (processes[next].first_run) {
        processes[next].first_run = 0;
    }
    jump_to_user(processes[next].rip, processes[next].rsp, processes[next].rdi);
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
    wrmsr_val(MSR_FS_BASE, processes[next].fs_base);
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
    cur->fs_base = rdmsr_val(MSR_FS_BASE);

    if (cur->time_slice > 0)
        cur->time_slice--;

    {
        int n = cur->nice;
        if (n < -20) n = -20;
        if (n > 19)  n = 19;
        u64 weight;
        if (n < 0) weight = 1024ULL << (-n);
        else       weight = 1024ULL >> n;
        if (weight == 0) weight = 1;
        u64 delta = 1000000ULL / weight;
        cur->runtime_ns += delta;
        cur->fair_key = cur->runtime_ns;
    }

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
    wrmsr_val(MSR_FS_BASE, processes[next].fs_base);

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

void Harlin_FairPickTest(void)
{
    int i;
    int base = -1;
    for (i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].state == PROC_STATE_NONE) {
            base = i;
            break;
        }
    }
    ASSERT(base >= 0);
    for (i = 0; i < 4 && base + i < MAX_PROCESSES; i++) {
        processes[base + i].state = PROC_STATE_READY;
        processes[base + i].runtime_ns = (u64)(i * 100);
        processes[base + i].fair_key   = (u64)(i * 100);
        processes[base + i].nice = i - 2;
    }

    int best = pick_next_process(0);
    ASSERT(best == base);
    ASSERT(processes[best].runtime_ns == 0);

    for (i = 0; i < 8; i++) {
        int p = pick_next_process(0);
        if (p < 0) break;
        processes[p].runtime_ns += 50;
        processes[p].fair_key = processes[p].runtime_ns;
    }

    for (i = base; i < base + 4; i++) {
        processes[i].state = PROC_STATE_NONE;
    }
    pr_info("fair_pick: test OK (initial best=%d)", best);
}
