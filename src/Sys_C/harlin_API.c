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
        __asm__ volatile ("outb %0, $0xE9" : : "a"(*str));
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
    u16 pos, attr;
    u8 lo, hi;
    pos = 80 * 25;
    attr = ((bg & 0x0F) << 4) | (fg & 0x0F);
    lo = (u8)(pos & 0xFF);
    hi = (u8)((pos >> 8) & 0xFF);
    PortOut8(0x3D4, 0x0F);
    PortOut8(0x3D5, lo);
    PortOut8(0x3D4, 0x0E);
    PortOut8(0x3D5, hi);
    PortOut8(0x3D5, (u8)(attr));
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

int Harlin_KeyReady(void)
{
    return keyboard_has_data();
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
    idt_init();
    pic_init();
    keyboard_init();
    network_init();
    interrupts_enable();

    Harlin_PmmInit();
    Harlin_VmmInit(0x20000);

    screen_clear();
    Harlin_ConPrint("\n");
    Harlin_ConPrint("The HarLin\n");
    Harlin_ConPrint("------------------------\n");
    Harlin_ConPrint("As an open-source project, HarLin kernel allows third-party development that\n");
    Harlin_ConPrint("includes: 1. modifying the kernel implementation, 2. adding new features, and\n");
    Harlin_ConPrint("3. releasing it while complying with the MIT open-source license.\n");
    Harlin_ConPrint("\n");
    Harlin_ConPrint("(C) 2026 HarLin228 Studio\n");
    Harlin_ConPrint("\n");

    for (;;) {
        Harlin_IntOff();
        if (!Harlin_KeyReady()) {
            Harlin_IntOn();
            asm volatile ("hlt");
        }
        Harlin_IntOn();
    }
}
