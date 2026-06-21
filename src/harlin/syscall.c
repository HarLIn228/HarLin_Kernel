#include "harlin_API.h"
#include "screen.h"
#include "scheduler.h"
#include "keyboard.h"
#include "pmm.h"
#include "vmm.h"
#include "fat32.h"
#include "chc_loader.h"
#include "pipe.h"

#define USER_ADDR_START 0x400000
#define USER_ADDR_END   0x800000

struct syscall_regs {
    u64 rax;
    u64 rcx;
    u64 rdx;
    u64 rbx;
    u64 rbp;
    u64 rsi;
    u64 rdi;
    u64 r8;
    u64 r9;
    u64 r10;
    u64 r11;
    u64 r12;
    u64 r13;
    u64 r14;
    u64 r15;
};

typedef unsigned long (*syscall_t)(struct syscall_regs* r);

#define MAX_OPEN_FILES 4
static struct Harlin_File open_files[MAX_OPEN_FILES];
static int file_in_use[MAX_OPEN_FILES];

static unsigned long sys_exit(struct syscall_regs* r)
{
    process_exit();
    return 0;
}

static int user_ptr_valid(u64 addr, u64 len)
{
    u64 p;
    if (addr + len < addr)
        return 0;
    if (addr < USER_ADDR_START || addr + len > USER_ADDR_END)
        return 0;
    for (p = addr; p < addr + len; p += 4096) {
        if (vmm_get_phys(p) == 0)
            return 0;
    }
    return 1;
}

static unsigned long sys_print(struct syscall_regs* r)
{
    const char* str = (const char*)r->rdi;
    u64 len;
    if (!str)
        return 0;
    len = 0;
    while (str[len])
        len++;
    if (!user_ptr_valid((u64)str, len + 1))
        return (unsigned long)-1;
    while (*str) {
        char c = *str++;
        screen_put_char(c);
    }
    return 0;
}

static unsigned long sys_getc(struct syscall_regs* r)
{
    unsigned char sc;
    char ch;
    for (;;) {
        Harlin_IntOff();
        if (keyboard_has_data()) {
            sc = keyboard_poll();
            Harlin_IntOn();
            if (sc) {
                ch = keyboard_scancode_to_ascii(sc);
                if (ch) return (unsigned long)ch;
            }
        } else {
            Harlin_IntOn();
            asm volatile ("hlt");
        }
    }
}

static unsigned long sys_alloc(struct syscall_regs* r)
{
    u64 phys;
    u64 virt;
    u64 start;
    struct process* proc;

    proc = process_current();
    if (!proc || proc->page_count >= 16)
        return 0;

    if (proc->next_alloc_virt == 0 || proc->next_alloc_virt < USER_ADDR_START + 0x100000 || proc->next_alloc_virt >= USER_ADDR_END)
        proc->next_alloc_virt = USER_ADDR_START + 0x100000;

    start = proc->next_alloc_virt;
    virt = start;
    while (vmm_get_phys(virt) != 0) {
        virt += 4096;
        if (virt >= USER_ADDR_END)
            virt = USER_ADDR_START + 0x100000;
        if (virt == start)
            return 0;
    }

    proc->next_alloc_virt = virt + 4096;
    if (proc->next_alloc_virt >= USER_ADDR_END)
        proc->next_alloc_virt = USER_ADDR_START + 0x100000;

    phys = pmm_alloc();
    if (!phys)
        return 0;

    vmm_map(virt, phys, VMM_PRESENT | VMM_WRITABLE | VMM_USER);

    if (proc->page_count < 16) {
        proc->user_vaddrs[proc->page_count] = virt;
        proc->user_pages[proc->page_count++] = phys;
    }

    return virt;
}

static unsigned long sys_free(struct syscall_regs* r)
{
    u64 virt = r->rdi;
    u64 phys;
    struct process* proc;
    int i;

    if (!user_ptr_valid(virt, 4096))
        return (unsigned long)-1;

    phys = vmm_get_phys(virt);
    if (!phys)
        return (unsigned long)-1;

    vmm_unmap(virt);
    pmm_free(phys);

    proc = process_current();
    if (proc) {
        for (i = 0; i < proc->page_count; i++) {
            if (proc->user_vaddrs[i] == virt) {
                proc->user_vaddrs[i] = 0;
                proc->user_pages[i] = 0;
                break;
            }
        }
    }

    return 0;
}

static unsigned long sys_open(struct syscall_regs* r)
{
    const char* name = (const char*)r->rdi;
    int i;
    if (!name)
        return (unsigned long)-1;
    if (!user_ptr_valid((u64)name, 256))
        return (unsigned long)-1;
    for (i = 0; i < MAX_OPEN_FILES; i++) {
        if (!file_in_use[i]) {
            if (Harlin_Open(name, &open_files[i]) == HARLIN_FS_OK) {
                file_in_use[i] = 1;
                return (unsigned long)i;
            }
            return (unsigned long)-1;
        }
    }
    return (unsigned long)-1;
}

static unsigned long sys_read(struct syscall_regs* r)
{
    int fd = (int)r->rdi;
    void* buf = (void*)r->rsi;
    u32 len = (u32)r->rdx;
    if (fd < 0 || fd >= MAX_OPEN_FILES || !file_in_use[fd] || !buf)
        return (unsigned long)-1;
    if (!user_ptr_valid((u64)buf, len))
        return (unsigned long)-1;
    return (unsigned long)Harlin_Read(&open_files[fd], buf, len);
}

static unsigned long sys_close(struct syscall_regs* r)
{
    int fd = (int)r->rdi;
    if (fd < 0 || fd >= MAX_OPEN_FILES || !file_in_use[fd])
        return (unsigned long)-1;
    Harlin_Close(&open_files[fd]);
    file_in_use[fd] = 0;
    return 0;
}

static unsigned long sys_exec(struct syscall_regs* r)
{
    const char* name = (const char*)r->rdi;
    struct Harlin_File file;
    void* buf;
    u32 size;
    if (!name)
        return (unsigned long)-1;
    if (!user_ptr_valid((u64)name, 256))
        return (unsigned long)-1;
    if (Harlin_Open(name, &file) != HARLIN_FS_OK)
        return (unsigned long)-1;
    size = Harlin_Size(&file);
    if (size == 0 || size > 4096) {
        Harlin_Close(&file);
        return (unsigned long)-1;
    }
    buf = (void*)pmm_alloc();
    if (!buf) {
        Harlin_Close(&file);
        return (unsigned long)-1;
    }
    if (Harlin_Read(&file, buf, size) != (int)size) {
        pmm_free((u64)buf);
        Harlin_Close(&file);
        return (unsigned long)-1;
    }
    Harlin_Close(&file);
    if (chc_load(buf, size) >= 0) {
        process_exit();
        __builtin_unreachable();
    }
    pmm_free((u64)buf);
    return (unsigned long)-1;
}

static unsigned long sys_yield(struct syscall_regs* r)
{
    schedule();
    return 0;
}

static unsigned long sys_key_overflow_count(struct syscall_regs* r)
{
    return (unsigned long)keyboard_overflow_count();
}

static unsigned long sys_pipe_create(struct syscall_regs* r)
{
    int id = pipe_create();
    if (id < 0)
        return (unsigned long)id;
    if (r->rdi) {
        struct Harlin_Pipe* pipe = (struct Harlin_Pipe*)r->rdi;
        if (!user_ptr_valid((u64)pipe, sizeof(*pipe)))
            return (unsigned long)-1;
        pipe->id = id;
    }
    return (unsigned long)id;
}

static unsigned long sys_pipe_ready(struct syscall_regs* r)
{
    int id = (int)r->rdi;
    return (unsigned long)pipe_ready(id);
}

static unsigned long sys_pipe_read(struct syscall_regs* r)
{
    int id = (int)r->rdi;
    void* buf = (void*)r->rsi;
    u32 len = (u32)r->rdx;

    if (!user_ptr_valid((u64)buf, len))
        return (unsigned long)-1;
    return (unsigned long)pipe_read(id, buf, len);
}

static unsigned long sys_pipe_write(struct syscall_regs* r)
{
    int id = (int)r->rdi;
    const void* buf = (const void*)r->rsi;
    u32 len = (u32)r->rdx;

    if (!user_ptr_valid((u64)buf, len))
        return (unsigned long)-1;
    return (unsigned long)pipe_write(id, buf, len);
}

static unsigned long sys_pipe_close(struct syscall_regs* r)
{
    int id = (int)r->rdi;
    pipe_close(id);
    return 0;
}

static unsigned long sys_sleep(struct syscall_regs* r)
{
    unsigned long ms = r->rdi;
    volatile unsigned long i;
    unsigned long j;
    for (j = 0; j < ms; j++) {
        for (i = 0; i < 50000; i++) {
            asm volatile ("" : : : "memory");
        }
    }
    return 0;
}

static syscall_t syscall_table[] = {
    [HARLIN_SYS_EXIT]  = sys_exit,
    [HARLIN_SYS_PRINT] = sys_print,
    [HARLIN_SYS_GETC]  = sys_getc,
    [HARLIN_SYS_ALLOC] = sys_alloc,
    [HARLIN_SYS_FREE]  = sys_free,
    [HARLIN_SYS_OPEN]  = sys_open,
    [HARLIN_SYS_READ]  = sys_read,
    [HARLIN_SYS_CLOSE] = sys_close,
    [HARLIN_SYS_EXEC]        = sys_exec,
    [HARLIN_SYS_YIELD]       = sys_yield,
    [HARLIN_SYS_SLEEP]       = sys_sleep,
    [HARLIN_SYS_KEYOVERFLOW] = sys_key_overflow_count,
    [HARLIN_SYS_PIPE_CREATE] = sys_pipe_create,
    [HARLIN_SYS_PIPE_READ]   = sys_pipe_read,
    [HARLIN_SYS_PIPE_WRITE]  = sys_pipe_write,
    [HARLIN_SYS_PIPE_CLOSE]  = sys_pipe_close,
    [HARLIN_SYS_PIPE_READY]  = sys_pipe_ready,
};

#define SYSCALL_COUNT (sizeof(syscall_table) / sizeof(syscall_table[0]))

void syscall_dispatch(struct syscall_regs* r)
{
    unsigned long num = r->rax;
    if (num < SYSCALL_COUNT && syscall_table[num]) {
        r->rax = syscall_table[num](r);
    } else {
        r->rax = (unsigned long)-1;
    }
}