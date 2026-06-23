#include "harlin_API.h"
#include "screen.h"
#include "scheduler.h"
#include "keyboard.h"
#include "pmm.h"
#include "vmm.h"
#include "fat32.h"
#include "pipe.h"
#include "rtc.h"
#include "smp.h"
#include "kmalloc.h"
#include "interrupt.h"
#include "spinlock.h"

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

#define MAX_OPEN_FILES 8
static struct Harlin_File open_files[MAX_OPEN_FILES];
static int file_in_use[MAX_OPEN_FILES];

static unsigned long sys_exit(struct syscall_regs* r)
{
    process_exit();
    return 0;
}

static int user_ptr_valid(u64 addr, u64 len)
{
    if (len == 0)
        return 1;
    if (addr + len < addr)
        return 0;
    if (addr < USER_ADDR_START || addr + len > USER_ADDR_END)
        return 0;
    {
        u64 p = addr & ~0xFFF;
        u64 end = (addr + len + 0xFFF) & ~0xFFF;
        for (; p < end; p += 4096) {
            if (vmm_get_phys(p) == 0)
                return 0;
        }
    }
    return 1;
}

static unsigned long sys_print(struct syscall_regs* r)
{
    char kbuf[256];
    u64 ustr = r->rdi;
    u64 i;
    if (!ustr)
        return (unsigned long)-1;
    if (!user_ptr_valid(ustr, 1))
        return (unsigned long)-1;
    {
        u64 max = sizeof(kbuf) - 1;
        for (i = 0; i < max; i++) {
            char c;
            if (ustr + i < USER_ADDR_START || ustr + i >= USER_ADDR_END)
                return (unsigned long)-1;
            if (copy_from_user(&c, ustr + i, 1) != 0)
                return (unsigned long)-1;
            kbuf[i] = c;
            if (c == 0) break;
        }
        kbuf[i] = 0;
    }
    {
        u64 j;
        for (j = 0; j < i; j++) {
            char c = kbuf[j];
            screen_put_char(c);
        }
    }
    return 0;
}

static unsigned long sys_getc(struct syscall_regs* r)
{
    unsigned char sc;
    char ch;
    for (;;) {
        interrupts_disable();
        if (keyboard_has_data()) {
            sc = keyboard_poll();
            interrupts_enable();
            if (sc) {
                ch = keyboard_scancode_to_ascii(sc);
                if (ch) return (unsigned long)ch;
            }
        } else {
            interrupts_enable();
            asm volatile ("hlt");
        }
    }
}

static unsigned long sys_keypoll(struct syscall_regs* r)
{
    unsigned char sc;
    char ch;
    (void)r;
    interrupts_disable();
    if (keyboard_has_data()) {
        sc = keyboard_poll();
        interrupts_enable();
        if (sc) {
            ch = keyboard_scancode_to_ascii(sc);
            if (ch) return (unsigned long)(unsigned char)ch;
        }
        return 0;
    }
    interrupts_enable();
    return 0;
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

    if (proc->page_count < 64) {
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
    int owned = 0;

    if (virt & 0xFFF)
        return (unsigned long)-1;
    if (!user_ptr_valid(virt, 4096))
        return (unsigned long)-1;

    phys = vmm_get_phys(virt);
    if (!phys)
        return (unsigned long)-1;

    proc = process_current();
    if (proc) {
        for (i = 0; i < proc->page_count; i++) {
            if (proc->user_vaddrs[i] == virt) {
                owned = 1;
                break;
            }
        }
    }
    if (!owned)
        return (unsigned long)-1;

    vmm_unmap(virt);
    pmm_free(phys);

    if (proc) {
        for (i = 0; i < proc->page_count; i++) {
            if (proc->user_vaddrs[i] == virt) {
                u64 j;
                for (j = i; j + 1 < proc->page_count; j++) {
                    proc->user_vaddrs[j] = proc->user_vaddrs[j + 1];
                    proc->user_pages[j] = proc->user_pages[j + 1];
                }
                proc->page_count--;
                break;
            }
        }
    }

    return 0;
}

static unsigned long sys_open(struct syscall_regs* r)
{
    char kname[256];
    int i;
    if (!r->rdi)
        return (unsigned long)-1;
    if (!user_ptr_valid(r->rdi, 1))
        return (unsigned long)-1;
    if (strncpy_from_user(kname, r->rdi, sizeof(kname)) != 0)
        return (unsigned long)-1;
    for (i = 0; i < MAX_OPEN_FILES; i++) {
        if (!file_in_use[i]) {
            if (Harlin_Open(kname, &open_files[i]) == HARLIN_FS_OK) {
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
    void* ubuf = (void*)r->rsi;
    u32 len = (u32)r->rdx;
    u8* kbuf;
    int n;
    if (fd < 0 || fd >= MAX_OPEN_FILES || !file_in_use[fd] || !ubuf)
        return (unsigned long)-1;
    if (len == 0)
        return 0;
    if (len > 4096)
        len = 4096;
    if (!user_ptr_valid((u64)ubuf, len))
        return (unsigned long)-1;
    kbuf = (u8*)kmalloc(len);
    if (!kbuf)
        return (unsigned long)-1;
    n = Harlin_Read(&open_files[fd], (void*)(u64)kbuf, len);
    if (n > 0) {
        if (copy_to_user((u64)ubuf, kbuf, (u32)n) != 0) {
            kfree(kbuf);
            return (unsigned long)-1;
        }
    }
    kfree(kbuf);
    return (unsigned long)n;
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
    (void)r;
    return (unsigned long)-1;
}

static unsigned long sys_yield(struct syscall_regs* r)
{
    (void)r;
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
    process_register_handle(1, id);
    if (r->rdi) {
        struct Harlin_Pipe pipe;
        if (!user_ptr_valid(r->rdi, sizeof(struct Harlin_Pipe)))
            return (unsigned long)-1;
        pipe.id = id;
        if (copy_to_user(r->rdi, &pipe, sizeof(pipe)) != 0)
            return (unsigned long)-1;
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
    void* ubuf = (void*)r->rsi;
    u32 len = (u32)r->rdx;
    u32 flags = (u32)r->rcx;
    u8* kbuf;
    int n;
    if (len == 0)
        return 0;
    if (len > 4096)
        len = 4096;
    if (!ubuf || !user_ptr_valid((u64)ubuf, len))
        return (unsigned long)-1;
    kbuf = (u8*)kmalloc(len);
    if (!kbuf)
        return (unsigned long)-1;
    if (flags & 0x1)
        n = pipe_read_blocking(id, kbuf, len);
    else
        n = pipe_read(id, kbuf, len);
    if (n > 0) {
        if (copy_to_user((u64)ubuf, kbuf, (u32)n) != 0) {
            kfree(kbuf);
            return (unsigned long)-1;
        }
    }
    kfree(kbuf);
    return (unsigned long)n;
}

static unsigned long sys_pipe_write(struct syscall_regs* r)
{
    int id = (int)r->rdi;
    const void* ubuf = (const void*)r->rsi;
    u32 len = (u32)r->rdx;
    u32 flags = (u32)r->rcx;
    u8* kbuf;
    int n;
    if (len == 0)
        return 0;
    if (len > 4096)
        len = 4096;
    if (!ubuf || !user_ptr_valid((u64)ubuf, len))
        return (unsigned long)-1;
    kbuf = (u8*)kmalloc(len);
    if (!kbuf)
        return (unsigned long)-1;
    if (copy_from_user(kbuf, (u64)ubuf, len) != 0) {
        kfree(kbuf);
        return (unsigned long)-1;
    }
    if (flags & 0x1)
        n = pipe_write_blocking(id, kbuf, len);
    else
        n = pipe_write(id, kbuf, len);
    kfree(kbuf);
    return (unsigned long)n;
}

static unsigned long sys_pipe_close(struct syscall_regs* r)
{
    int id = (int)r->rdi;
    process_unregister_handle(id);
    pipe_close(id);
    return 0;
}

static unsigned long sys_sleep(struct syscall_regs* r)
{
    scheduler_sleep((u32)r->rdi);
    return 0;
}

static unsigned long sys_getpid(struct syscall_regs* r)
{
    struct process* proc = process_current();
    if (!proc)
        return (unsigned long)-1;
    return (unsigned long)proc->pid;
}

static unsigned long sys_getcpu(struct syscall_regs* r)
{
    return (unsigned long)smp_current_cpu_id();
}

static unsigned long sys_time(struct syscall_regs* r)
{
    struct rtc_time t;
    if (!r->rdi || !user_ptr_valid(r->rdi, sizeof(struct rtc_time)))
        return (unsigned long)-1;
    rtc_read(&t);
    if (copy_to_user(r->rdi, &t, sizeof(t)) != 0)
        return (unsigned long)-1;
    return 0;
}

static unsigned long sys_beep(struct syscall_regs* r)
{
    extern void speaker_beep(u32 freq, u32 ms);
    speaker_beep((u32)r->rdi, (u32)r->rsi);
    return 0;
}

static unsigned long sys_kmalloc(struct syscall_regs* r)
{
    if (r->rdi == 0 || r->rdi > 0x100000)
        return 0;
    return (unsigned long)kmalloc(r->rdi);
}

static unsigned long sys_kfree(struct syscall_regs* r)
{
    void* ptr = (void*)r->rdi;
    struct kmalloc_block* block;
    if (!ptr)
        return 0;
    if (r->rdi < 0xFFFF800000000000 || r->rdi >= 0xFFFF800010000000)
        return (unsigned long)-1;
    block = (struct kmalloc_block*)((u8*)ptr - sizeof(struct kmalloc_block));
    if (block->magic != 0x48414E44 || block->used != 1)
        return (unsigned long)-1;
    kfree(ptr);
    return 0;
}

static struct spinlock mmap_lock;
static int mmap_lock_inited = 0;

static unsigned long sys_mmap(struct syscall_regs* r)
{
    u64 virt = r->rdi;
    u64 size = r->rsi;
    u64 pages;
    u64 i;
    struct process* proc;
    if (virt & 0xFFF)
        return (unsigned long)-1;
    if (virt < USER_ADDR_START || virt + size > USER_ADDR_END || virt + size < virt)
        return (unsigned long)-1;
    if (size == 0)
        return (unsigned long)-1;
    proc = process_current();
    if (!proc)
        return (unsigned long)-1;
    pages = (size + 4095) / 4096;

    if (!mmap_lock_inited) {
        spinlock_init(&mmap_lock);
        mmap_lock_inited = 1;
    }
    spinlock_acquire(&mmap_lock);

    for (i = 0; i < pages; i++) {
        if (vmm_mapped(virt + i * 4096)) {
            spinlock_release(&mmap_lock);
            return (unsigned long)-1;
        }
    }
    for (i = 0; i < pages; i++) {
        u64 phys;
        phys = pmm_alloc();
        if (!phys) {
            while (i > 0) {
                i--;
                vmm_unmap_and_free(virt + i * 4096);
            }
            spinlock_release(&mmap_lock);
            return (unsigned long)-1;
        }
        vmm_map(virt + i * 4096, phys, VMM_PRESENT | VMM_WRITABLE | VMM_USER);
        if (proc->page_count < 64) {
            proc->user_vaddrs[proc->page_count] = virt + i * 4096;
            proc->user_pages[proc->page_count++] = phys;
        }
    }
    spinlock_release(&mmap_lock);
    return virt;
}

static unsigned long sys_munmap(struct syscall_regs* r)
{
    u64 virt = r->rdi;
    u64 size = r->rsi;
    u64 pages;
    u64 i;
    struct process* proc;
    if (virt & 0xFFF)
        return (unsigned long)-1;
    if (virt < USER_ADDR_START || virt + size > USER_ADDR_END)
        return (unsigned long)-1;
    pages = (size + 4095) / 4096;
    proc = process_current();

    if (!mmap_lock_inited) {
        spinlock_init(&mmap_lock);
        mmap_lock_inited = 1;
    }
    spinlock_acquire(&mmap_lock);

    for (i = 0; i < pages; i++) {
        u64 va = virt + i * 4096;
        int j;
        int found = 0;
        if (proc) {
            for (j = 0; j < proc->page_count; j++) {
                if (proc->user_vaddrs[j] == va) {
                    u64 k;
                    for (k = j; k + 1 < proc->page_count; k++) {
                        proc->user_vaddrs[k] = proc->user_vaddrs[k + 1];
                        proc->user_pages[k] = proc->user_pages[k + 1];
                    }
                    proc->page_count--;
                    found = 1;
                    break;
                }
            }
        }
        (void)found;
        vmm_unmap_and_free(va);
    }
    spinlock_release(&mmap_lock);
    return 0;
}

static unsigned long sys_getkeystate(struct syscall_regs* r)
{
    return (unsigned long)keyboard_get_state();
}

static unsigned long sys_keyled(struct syscall_regs* r)
{
    keyboard_set_leds((u8)r->rdi);
    return 0;
}

static unsigned long sys_set_priority(struct syscall_regs* r)
{
    struct process* proc = process_current();
    if (!proc)
        return (unsigned long)-1;
    proc->priority = (u32)r->rdi;
    if (proc->priority == 0)
        proc->priority = 1;
    return 0;
}

static unsigned long sys_cd(struct syscall_regs* r)
{
    char kpath[256];
    if (!r->rdi || !user_ptr_valid(r->rdi, 1))
        return (unsigned long)-1;
    if (strncpy_from_user(kpath, r->rdi, sizeof(kpath)) != 0)
        return (unsigned long)-1;
    return (unsigned long)Harlin_Cd(kpath);
}

static unsigned long sys_mkdir(struct syscall_regs* r)
{
    char kpath[256];
    if (!r->rdi || !user_ptr_valid(r->rdi, 1))
        return (unsigned long)-1;
    if (strncpy_from_user(kpath, r->rdi, sizeof(kpath)) != 0)
        return (unsigned long)-1;
    return (unsigned long)Harlin_Mkdir(kpath);
}

static unsigned long sys_rmdir(struct syscall_regs* r)
{
    char kpath[256];
    if (!r->rdi || !user_ptr_valid(r->rdi, 1))
        return (unsigned long)-1;
    if (strncpy_from_user(kpath, r->rdi, sizeof(kpath)) != 0)
        return (unsigned long)-1;
    return (unsigned long)Harlin_Rmdir(kpath);
}

static unsigned long sys_getcwd(struct syscall_regs* r)
{
    char kbuf[256];
    u32 size = (u32)r->rsi;
    int ret;
    if (!r->rdi || !user_ptr_valid(r->rdi, size > 0 ? size : 1))
        return (unsigned long)-1;
    if (size == 0 || size > sizeof(kbuf))
        return (unsigned long)-1;
    ret = Harlin_GetCwd(kbuf, size);
    if (ret != 0)
        return (unsigned long)ret;
    if (copy_to_user(r->rdi, kbuf, Harlin_Len(kbuf) + 1) != 0)
        return (unsigned long)-1;
    return 0;
}

static unsigned long sys_msg_create(struct syscall_regs* r)
{
    return (unsigned long)Harlin_MsgCreate();
}

static unsigned long sys_msg_send(struct syscall_regs* r)
{
    int qid = (int)r->rdi;
    u32 type = (u32)r->rsi;
    u32 len = (u32)r->rcx;
    char kbuf[64];

    if (!r->rdx || !user_ptr_valid(r->rdx, len > 0 ? len : 1))
        return (unsigned long)-1;
    if (len > 64)
        return (unsigned long)-1;
    if (copy_from_user(kbuf, r->rdx, len) != 0)
        return (unsigned long)-1;
    return (unsigned long)Harlin_MsgSend(qid, type, kbuf, len);
}

static unsigned long sys_msg_recv(struct syscall_regs* r)
{
    int qid = (int)r->rdi;
    u32 len = (u32)r->rcx;
    u32 expected_type = (u32)r->r8;
    u32 type;
    char kbuf[64];
    int ret;

    if (!r->rsi || !user_ptr_valid(r->rsi, sizeof(u32)))
        return (unsigned long)-1;
    if (!r->rdx || !user_ptr_valid(r->rdx, len > 0 ? len : 1))
        return (unsigned long)-1;
    if (len > 64)
        return (unsigned long)-1;

    ret = Harlin_MsgRecv(qid, &type, kbuf, len, expected_type);
    if (ret != 0)
        return (unsigned long)ret;

    if (copy_to_user(r->rsi, &type, sizeof(u32)) != 0)
        return (unsigned long)-1;
    if (copy_to_user(r->rdx, kbuf, len) != 0)
        return (unsigned long)-1;
    return 0;
}

static unsigned long sys_msg_destroy(struct syscall_regs* r)
{
    return (unsigned long)Harlin_MsgDestroy((int)r->rdi);
}

static unsigned long sys_sem_create(struct syscall_regs* r)
{
    return (unsigned long)Harlin_SemCreate((int)r->rdi);
}

static unsigned long sys_sem_wait(struct syscall_regs* r)
{
    return (unsigned long)Harlin_SemWait((int)r->rdi);
}

static unsigned long sys_sem_post(struct syscall_regs* r)
{
    return (unsigned long)Harlin_SemPost((int)r->rdi);
}

static unsigned long sys_sem_destroy(struct syscall_regs* r)
{
    return (unsigned long)Harlin_SemDestroy((int)r->rdi);
}

static unsigned long sys_dlopen(struct syscall_regs* r)
{
    char kpath[256];
    if (!r->rdi || !user_ptr_valid(r->rdi, 1))
        return (unsigned long)-1;
    if (strncpy_from_user(kpath, r->rdi, sizeof(kpath)) != 0)
        return (unsigned long)-1;
    return (unsigned long)Harlin_DlOpen(kpath);
}

static unsigned long sys_dlsym(struct syscall_regs* r)
{
    int lib_id = (int)r->rdi;
    char kname[256];
    if (!r->rsi || !user_ptr_valid(r->rsi, 1))
        return (unsigned long)0;
    if (strncpy_from_user(kname, r->rsi, sizeof(kname)) != 0)
        return (unsigned long)0;
    return (unsigned long)Harlin_DlSym(lib_id, kname);
}

static unsigned long sys_dlclose(struct syscall_regs* r)
{
    return (unsigned long)Harlin_DlClose((int)r->rdi);
}

#include "display.h"

static unsigned long sys_set_mode(struct syscall_regs* r)
{
    return (unsigned long)Harlin_SetMode((int)r->rdi);
}

static unsigned long sys_clear(struct syscall_regs* r)
{
    Harlin_ClearScreen((unsigned char)r->rdi);
    return 0;
}

static unsigned long sys_draw_rect(struct syscall_regs* r)
{
    Harlin_DrawRect((int)r->rdi, (int)r->rsi, (int)r->rdx, (int)r->r8, (unsigned int)r->r9);
    return 0;
}

static unsigned long sys_draw_char(struct syscall_regs* r)
{
    Harlin_DrawChar((int)r->rdi, (int)r->rsi, (char)r->rdx, (unsigned int)r->r8, (unsigned int)r->r9);
    return 0;
}

static unsigned long sys_draw_string(struct syscall_regs* r)
{
    char kbuf[256];
    u64 ustr = r->rdx;
    u64 i;
    if (!ustr)
        return (unsigned long)-1;
    if (!user_ptr_valid(ustr, 1))
        return (unsigned long)-1;
    {
        u64 max = sizeof(kbuf) - 1;
        for (i = 0; i < max; i++) {
            char c;
            if (ustr + i < USER_ADDR_START || ustr + i >= USER_ADDR_END)
                return (unsigned long)-1;
            if (copy_from_user(&c, ustr + i, 1) != 0)
                return (unsigned long)-1;
            kbuf[i] = c;
            if (c == 0) break;
        }
        kbuf[i] = 0;
    }
    Harlin_DrawString((int)r->rdi, (int)r->rsi, kbuf, (unsigned int)r->r8, (unsigned int)r->r9);
    return 0;
}

static unsigned long sys_get_fb(struct syscall_regs* r)
{
    (void)r;
    return (unsigned long)0xA0000ULL;
}

static syscall_t syscall_table[] = {
    [HARLIN_SYS_EXIT]  = sys_exit,
    [HARLIN_SYS_PRINT] = sys_print,
    [HARLIN_SYS_GETC]  = sys_getc,
    [HARLIN_SYS_KEYPOLL] = sys_keypoll,
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
    [HARLIN_SYS_GETPID]      = sys_getpid,
    [HARLIN_SYS_GETCPU]      = sys_getcpu,
    [HARLIN_SYS_TIME]        = sys_time,
    [HARLIN_SYS_BEEP]        = sys_beep,
    [HARLIN_SYS_KMALLOC]     = sys_kmalloc,
    [HARLIN_SYS_KFREE]       = sys_kfree,
    [HARLIN_SYS_MMAP]        = sys_mmap,
    [HARLIN_SYS_UNMAP]       = sys_munmap,
    [HARLIN_SYS_GETKEYSTATE] = sys_getkeystate,
    [HARLIN_SYS_KEYLED]      = sys_keyled,
    [HARLIN_SYS_SETPRIORITY] = sys_set_priority,
    [HARLIN_SYS_CD]     = sys_cd,
    [HARLIN_SYS_MKDIR]  = sys_mkdir,
    [HARLIN_SYS_RMDIR]  = sys_rmdir,
    [HARLIN_SYS_GETCWD] = sys_getcwd,
    [HARLIN_SYS_MSGCREATE]  = sys_msg_create,
    [HARLIN_SYS_MSGSEND]    = sys_msg_send,
    [HARLIN_SYS_MSGRECV]    = sys_msg_recv,
    [HARLIN_SYS_MSGDESTROY] = sys_msg_destroy,
    [HARLIN_SYS_SEMCREATE]  = sys_sem_create,
    [HARLIN_SYS_SEMWAIT]    = sys_sem_wait,
    [HARLIN_SYS_SEMPOST]    = sys_sem_post,
    [HARLIN_SYS_SEMDESTROY] = sys_sem_destroy,
    [HARLIN_SYS_DLOPEN]     = sys_dlopen,
    [HARLIN_SYS_DLSYM]      = sys_dlsym,
    [HARLIN_SYS_DLCLOSE]    = sys_dlclose,
    [HARLIN_SYS_SETMODE]    = sys_set_mode,
    [HARLIN_SYS_CLEAR]      = sys_clear,
    [HARLIN_SYS_DRAWRECT]   = sys_draw_rect,
    [HARLIN_SYS_DRAWCHAR]   = sys_draw_char,
    [HARLIN_SYS_DRAWSTRING] = sys_draw_string,
    [HARLIN_SYS_GETFB]      = sys_get_fb,
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
