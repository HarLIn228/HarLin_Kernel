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
#include "cx_loader.h"
#include "scheduler.h"
#include "pipe.h"

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

u8  Harlin_PortIn8(u16 port)  { return PortIn8(port); }
u16 Harlin_PortIn16(u16 port) { return PortIn16(port); }
u32 Harlin_PortIn32(u16 port) { return PortIn32(port); }
void Harlin_PortOut8(u16 port, u8  data)  { PortOut8(port, data); }
void Harlin_PortOut16(u16 port, u16 data) { PortOut16(port, data); }
void Harlin_PortOut32(u16 port, u32 data) { PortOut32(port, data); }

void Harlin_IntOn(void)  { IntOn(); }
void Harlin_IntOff(void) { IntOff(); }

void Harlin_ConClear(void)   { screen_clear(); }
void Harlin_ConPutChar(char c) { screen_put_char(c); }

void Harlin_ConPrint(const char* str)
{
    while (*str) {
        screen_put_char(*str++);
    }
}

void Harlin_ConPrintHex(u64 val)
{
    const char* hex = "0123456789ABCDEF";
    int i;
    Harlin_ConPutChar('0');
    Harlin_ConPutChar('x');
    for (i = 60; i >= 0; i -= 4) {
        Harlin_ConPutChar(hex[(val >> i) & 0xF]);
    }
}

void Harlin_ConPrintDec(s32 val)
{
    char buf[12];
    int i = 0;
    if (val == 0) {
        Harlin_ConPutChar('0');
        return;
    }
    if (val < 0) {
        Harlin_ConPutChar('-');
        val = (s32)((u32)(-(val + 1)) + 1);
    }
    while ((u32)val > 0) {
        buf[i++] = '0' + ((u32)val % 10);
        val = (s32)((u32)val / 10);
    }
    while (i > 0) Harlin_ConPutChar(buf[--i]);
}

void Harlin_ConSetColor(u8 fg, u8 bg)
{
    int i;
    u16 attr;
    volatile u16* vga = (volatile u16*)0xB8000;
    attr = (u16)(((bg & 0x0F) << 4) | (fg & 0x0F));
    for (i = 0; i < 80 * 25; i++) {
        vga[i] = (vga[i] & 0x00FF) | (attr << 8);
    }
}

u32 Harlin_StrLen(const char* str)
{
    const char* p = str;
    while (*p) p++;
    return (u32)(p - str);
}

void Harlin_StrCopy(char* dst, const char* src)
{
    while (*src) *dst++ = *src++;
    *dst = 0;
}

int Harlin_StrCmp(const char* a, const char* b)
{
    while (*a && *b) {
        if (*a != *b) return *a - *b;
        a++;
        b++;
    }
    return *a - *b;
}

void Harlin_MemCopy(void* dst, const void* src, u32 n)
{
    u32 i;
    unsigned char* d = (unsigned char*)dst;
    const unsigned char* s = (const unsigned char*)src;
    for (i = 0; i < n; i++) d[i] = s[i];
}

void Harlin_MemSet(void* dst, u8 val, u32 n)
{
    u32 i;
    unsigned char* d = (unsigned char*)dst;
    for (i = 0; i < n; i++) d[i] = val;
}

s32 Harlin_StrToInt(const char* str)
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

void Harlin_IntToStr(s32 val, char* buf)
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

int Harlin_NetInit(void)   { return network_init(); }
int Harlin_HttpGet(const char* host, const char* path) { return network_http_get(host, path); }
int Harlin_DNS(const char* domain, u8* out_ip)          { return dns_resolve(domain, out_ip); }

void Harlin_PmmInit(void) { pmm_init(); }
u64  Harlin_PmmAlloc(void) { return pmm_alloc(); }
void Harlin_PmmFree(u64 addr) { pmm_free(addr); }

void Harlin_VmmInit(u64 pml4_phys) { vmm_init(pml4_phys); }
void Harlin_VmmMap(u64 virt, u64 phys, u64 flags) { vmm_map(virt, phys, flags); }
void Harlin_VmmUnmap(u64 virt) { vmm_unmap(virt); }
u64  Harlin_VmmGetPhys(u64 virt) { return vmm_get_phys(virt); }

int Harlin_DiskInit(void) { return ata_init(); }
int Harlin_DiskReadSector(u64 lba, u8 count, void* buf) { return ata_read_sectors(lba, count, buf); }
int Harlin_DiskWriteSector(u64 lba, u8 count, const void* buf) { return ata_write_sectors(lba, count, buf); }

int Harlin_PartitionInit(void) { return partition_init(); }
int Harlin_PartitionCount(void) { return partition_count(); }

int Harlin_PartitionGet(int index, struct Harlin_PartitionInfo* out)
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

int Harlin_DisplaySetMode(int mode)
{
    return display_set_mode(mode);
}

void Harlin_DisplayClear(unsigned char color)
{
    display_clear(color);
}

void Harlin_DisplayPutPixel(int x, int y, unsigned char color)
{
    display_put_pixel(x, y, color);
}

void Harlin_DisplayPutString(int x, int y, const char* str, unsigned char color)
{
    display_put_string(x, y, str, color);
}

int Harlin_PipeCreate(struct Harlin_Pipe* pipe)
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

int Harlin_PipeRead(struct Harlin_Pipe* pipe, void* buf, u32 len)
{
    if (!pipe)
        return -1;
    return pipe_read(pipe->id, buf, len);
}

int Harlin_PipeWrite(struct Harlin_Pipe* pipe, const void* buf, u32 len)
{
    if (!pipe)
        return -1;
    return pipe_write(pipe->id, buf, len);
}

int Harlin_PipeReady(struct Harlin_Pipe* pipe)
{
    if (!pipe)
        return 0;
    return pipe_ready(pipe->id);
}

void Harlin_PipeClose(struct Harlin_Pipe* pipe)
{
    if (!pipe)
        return;
    pipe_close(pipe->id);
    pipe->id = -1;
}

int Harlin_KeyReady(void)
{
    return keyboard_has_data();
}

int Harlin_KeyOverflowCount(void)
{
    return keyboard_overflow_count();
}

char Harlin_KeyGet(void)
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

void Harlin_Shutdown(void)
{
    Harlin_ConPrint("System halted.\n");
    Harlin_IntOff();
    for (;;) asm volatile ("hlt");
}

void Harlin_Boot(void)
{
    extern char __bss_start[];
    extern char __bss_end[];
    int disk_ok;

    Harlin_MemSet(__bss_start, 0, (u32)(__bss_end - __bss_start));

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

    Harlin_PmmInit();
    Harlin_VmmInit(0x20000);
    scheduler_init();
    timer_init();
    pipe_init();

    for (;;) {
        Harlin_IntOn();
        asm volatile ("hlt");
    }
}
