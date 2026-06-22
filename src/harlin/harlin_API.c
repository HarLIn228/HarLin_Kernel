#include "harlin_API.h"
#include "keyboard.h"
#include "screen.h"
#include "network.h"
#include "interrupt.h"
#include "display.h"
#include "pmm.h"
#include "vmm.h"
#include "ata.h"
#include "partition.h"
#include "io.h"
#include "gdt.h"
#include "chc_loader.h"
#include "scheduler.h"
#include "pipe.h"
#include "smp.h"
#include "spinlock.h"
#include "kmalloc.h"
#include "rtc.h"
#include "harlin_chc.h"


extern void screen_put_char(char c);

extern void pic_init(void);

static u8  PortIn8(u16 port)  { u8  r; __asm__ volatile ("inb %1, %0" : "=a"(r) : "Nd"(port)); return r; }
static u16 PortIn16(u16 port) { u16 r; __asm__ volatile ("inw %1, %0" : "=a"(r) : "Nd"(port)); return r; }
static u32 PortIn32(u16 port) { u32 r; __asm__ volatile ("inl %1, %0" : "=a"(r) : "Nd"(port)); return r; }
static void PortOut8(u16 port, u8  data) { __asm__ volatile ("outb %0, %1" : : "a"(data), "Nd"(port)); }
static void PortOut16(u16 port, u16 data) { __asm__ volatile ("outw %0, %1" : : "a"(data), "Nd"(port)); }
static void PortOut32(u16 port, u32 data) { __asm__ volatile ("outl %0, %1" : : "a"(data), "Nd"(port)); }
static void IntOff(void) { __asm__ volatile ("cli"); }
static void IntOn(void)  { __asm__ volatile ("sti"); }

static void Harlin_IntOn(void)  { IntOn(); }
static void Harlin_IntOff(void) { IntOff(); }

u8  Harlin_PortIn8(u16 port)  { return PortIn8(port); }
u16 Harlin_PortIn16(u16 port) { return PortIn16(port); }
u32 Harlin_PortIn32(u16 port) { return PortIn32(port); }
void Harlin_PortOut8(u16 port, u8  data)  { PortOut8(port, data); }
void Harlin_PortOut16(u16 port, u16 data) { PortOut16(port, data); }
void Harlin_PortOut32(u16 port, u32 data) { PortOut32(port, data); }

void Harlin_Clear(void)   { screen_clear(); }
void Harlin_PutChar(char c) { screen_put_char(c); }

void Harlin_Print(const char* str)
{
    while (*str) {
        screen_put_char(*str++);
    }
}

void Harlin_PrintHex(u64 val)
{
    const char* hex = "0123456789ABCDEF";
    int i;
    Harlin_PutChar('0');
    Harlin_PutChar('x');
    for (i = 60; i >= 0; i -= 4) {
        Harlin_PutChar(hex[(val >> i) & 0xF]);
    }
}

void Harlin_PrintDec(s32 val)
{
    char buf[12];
    int i = 0;
    if (val == 0) {
        Harlin_PutChar('0');
        return;
    }
    if (val < 0) {
        Harlin_PutChar('-');
        val = (s32)((u32)(-(val + 1)) + 1);
    }
    while ((u32)val > 0) {
        buf[i++] = '0' + ((u32)val % 10);
        val = (s32)((u32)val / 10);
    }
    while (i > 0) Harlin_PutChar(buf[--i]);
}

void Harlin_SetColor(u8 fg, u8 bg)
{
    int i;
    u16 attr;
    volatile u16* vga = (volatile u16*)0xB8000;
    attr = (u16)(((bg & 0x0F) << 4) | (fg & 0x0F));
    for (i = 0; i < 80 * 25; i++) {
        vga[i] = (vga[i] & 0x00FF) | (attr << 8);
    }
}

u32 Harlin_Len(const char* str)
{
    const char* p = str;
    while (*p) p++;
    return (u32)(p - str);
}

void Harlin_CopyStr(char* dst, const char* src)
{
    while (*src) *dst++ = *src++;
    *dst = 0;
}

int Harlin_Compare(const char* a, const char* b)
{
    while (*a && *b) {
        if (*a != *b) return *a - *b;
        a++;
        b++;
    }
    return *a - *b;
}

void Harlin_Copy(void* dst, const void* src, u32 n)
{
    u32 i;
    unsigned char* d = (unsigned char*)dst;
    const unsigned char* s = (const unsigned char*)src;
    for (i = 0; i < n; i++) d[i] = s[i];
}

void Harlin_Fill(void* dst, u8 val, u32 n)
{
    u32 i;
    unsigned char* d = (unsigned char*)dst;
    for (i = 0; i < n; i++) d[i] = val;
}

s32 Harlin_ToInt(const char* str)
{
    s32 val = 0;
    int sign = 1;
    if (*str == '-') { sign = -1; str++; }
    while (*str >= '0' && *str <= '9') {
        val = val * 10 + (*str - '0');
        str++;
    }
    return val * sign;
}

void Harlin_FromInt(s32 val, char* buf)
{
    int i = 0, j;
    char tmp[12];
    if (val == 0) { buf[0] = '0'; buf[1] = 0; return; }
    if (val < 0) {
        *buf++ = '-';
        val = (s32)((u32)(-(val + 1)) + 1);
    }
    while ((u32)val > 0) {
        tmp[i++] = '0' + ((u32)val % 10);
        val = (s32)((u32)val / 10);
    }
    for (j = i - 1; j >= 0; j--) *buf++ = tmp[j];
    *buf = 0;
}

int Harlin_InitNet(void)   { return network_init(); }
int Harlin_HttpGet(const char* host, const char* path) { return network_http_get(host, path); }
int Harlin_Resolve(const char* domain, u8* out_ip)          { return dns_resolve(domain, out_ip); }

void Harlin_InitPmm(void) { pmm_init(); }
u64  Harlin_AllocPage(void) { return pmm_alloc(); }
void Harlin_FreePage(u64 addr) { pmm_free(addr); }

void Harlin_InitVmm(u64 pml4_phys) { vmm_init(pml4_phys); }
void Harlin_Map(u64 virt, u64 phys, u64 flags) { vmm_map(virt, phys, flags); }
void Harlin_Unmap(u64 virt) { vmm_unmap(virt); }
u64  Harlin_ToPhys(u64 virt) { return vmm_get_phys(virt); }

void Harlin_InitKmalloc(void) { kmalloc_init(); }
void* Harlin_Kmalloc(u64 size) { return kmalloc(size); }
void  Harlin_Kfree(void* ptr) { kfree(ptr); }
void* Harlin_Krealloc(void* ptr, u64 size) { return krealloc(ptr, size); }
u64   Harlin_Ksize(void* ptr) { return ksize(ptr); }

void Harlin_InitSmp(void) { smp_init(); }
int  Harlin_CpuCount(void) { return smp_cpu_count(); }
int  Harlin_CurrentCpu(void) { return smp_current_cpu_id(); }
void Harlin_SendIpi(int cpu, u8 vector) { smp_send_ipi((u32)cpu, vector); }

void Harlin_SpinlockInit(struct Harlin_Spinlock* lk) { spinlock_init((struct spinlock*)lk); }
void Harlin_SpinlockAcquire(struct Harlin_Spinlock* lk) { spinlock_acquire((struct spinlock*)lk); }
void Harlin_SpinlockRelease(struct Harlin_Spinlock* lk) { spinlock_release((struct spinlock*)lk); }

void Harlin_InitRtc(void) { rtc_init(); }
void Harlin_RtcRead(struct Harlin_RtcTime* out)
{
    struct rtc_time t;
    rtc_read(&t);
    if (out) {
        out->second = t.second;
        out->minute = t.minute;
        out->hour = t.hour;
        out->day = t.day;
        out->month = t.month;
        out->year = t.year;
    }
}
u64 Harlin_RtcBootSeconds(void) { return rtc_boot_seconds(); }

int Harlin_InitDisk(void) { return ata_init(); }
int Harlin_ReadSectors(u64 lba, u8 count, void* buf) { return ata_read_sectors(lba, count, buf); }
int Harlin_WriteSectors(u64 lba, u8 count, const void* buf) { return ata_write_sectors(lba, count, buf); }

int Harlin_InitPart(void) { return partition_init(); }
int Harlin_PartCount(void) { return partition_count(); }

int Harlin_GetPart(int index, struct Harlin_Part* out)
{
    struct partition_entry entry;
    if (partition_get(index, &entry) != 0)
        return -1;
    if (out) {
        out->active = entry.active;
        out->type = entry.type;
        out->start_lba = entry.start_lba;
        out->sector_count = entry.sector_count;
    }
    return 0;
}

int Harlin_SetMode(int mode)
{
    return display_set_mode(mode);
}

void Harlin_ClearScreen(unsigned char color)
{
    display_clear(color);
}

void Harlin_PutPixel(int x, int y, unsigned char color)
{
    display_put_pixel(x, y, color);
}

void Harlin_PutString(int x, int y, const char* str, unsigned char color)
{
    display_put_string(x, y, str, color);
}

int Harlin_CreatePipe(struct Harlin_Pipe* pipe)
{
    int id;
    if (!pipe)
        return -1;
    id = pipe_create();
    if (id < 0)
        return id;
    pipe->id = id;
    return 0;
}

int Harlin_ReadPipe(struct Harlin_Pipe* pipe, void* buf, u32 len)
{
    if (!pipe)
        return -1;
    return pipe_read(pipe->id, buf, len);
}

int Harlin_WritePipe(struct Harlin_Pipe* pipe, const void* buf, u32 len)
{
    if (!pipe)
        return -1;
    return pipe_write(pipe->id, buf, len);
}

int Harlin_ReadyPipe(struct Harlin_Pipe* pipe)
{
    if (!pipe)
        return 0;
    return pipe_ready(pipe->id);
}

void Harlin_ClosePipe(struct Harlin_Pipe* pipe)
{
    if (!pipe)
        return;
    pipe_close(pipe->id);
    pipe->id = -1;
}

int Harlin_KeyAvail(void)
{
    return keyboard_has_data();
}

int Harlin_KeyOverflow(void)
{
    return keyboard_overflow_count();
}

void Harlin_KeyFlush(void)
{
    keyboard_flush();
}

u8 Harlin_KeyState(void)
{
    return keyboard_get_state();
}

void Harlin_KeyLed(u8 leds)
{
    keyboard_set_leds(leds);
}

char Harlin_GetKey(void)
{
    unsigned char sc;
    for (;;) {
        Harlin_IntOff();
        if (keyboard_has_data()) {
            sc = keyboard_poll();
            Harlin_IntOn();
            if (sc) {
                char ch = keyboard_scancode_to_ascii(sc);
                if (ch) return ch;
            }
        } else {
            Harlin_IntOn();
            asm volatile ("hlt");
        }
    }
}

int Harlin_GetPid(void)
{
    struct process* p = process_current();
    if (!p)
        return -1;
    return p->pid;
}

void Harlin_SetPriority(u32 priority)
{
    struct process* p = process_current();
    if (p) {
        p->priority = priority ? priority : 1;
    }
}

void Harlin_Beep(u32 freq, u32 ms)
{
    extern void speaker_beep(u32 freq, u32 ms);
    speaker_beep(freq, ms);
}

void Harlin_Shutdown(void)
{
    Harlin_Print("System halted.\n");
    Harlin_IntOff();
    for (;;) asm volatile ("hlt");
}

void Harlin_Boot(void)
{
    extern char __bss_start[];
    extern char __bss_end[];

    Harlin_Fill(__bss_start, 0, (u32)(__bss_end - __bss_start));

    gdt_init();
    tss_set_rsp0(0x90000);
    idt_init();
    pic_init();
    {
        unsigned char reg_b;
        outb(0x70, 0x0B);
        reg_b = inb(0x71);
        outb(0x70, 0x0B);
        outb(0x71, reg_b & ~0x70);
        outb(0x70, 0x0C);
        inb(0x71);
    }
    {
        unsigned long apic_base_low, apic_base_high;
        asm volatile ("rdmsr" : "=a"(apic_base_low), "=d"(apic_base_high) : "c"(0x1B));
        if (apic_base_low & (1UL << 11)) {
            apic_base_low &= ~(1UL << 11);
            asm volatile ("wrmsr" : : "a"(apic_base_low), "d"(apic_base_high), "c"(0x1B));
        }
    }
    outb(0x21, inb(0x21) | 0x04);
    keyboard_init();
    interrupts_enable();

    Harlin_InitPmm();
    Harlin_InitKmalloc();
    Harlin_InitVmm(0x20000);
    scheduler_init();
    timer_init();
    pipe_init();
    Harlin_InitRtc();
    vmm_map(0xFEE00000, 0xFEE00000, VMM_PRESENT | VMM_WRITABLE);
    __asm__ volatile ("invlpg (%0)" : : "r"(0xFEE00000ULL) : "memory");
    Harlin_InitSmp();

    chc_load(harlin_chc_data, harlin_chc_data_size);
    schedule();
    for (;;) {
        asm volatile("hlt");
    }
}
