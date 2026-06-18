#include "interrupt.h"
#include "io.h"

struct idt_entry {
    unsigned short offset_low;
    unsigned short selector;
    unsigned char ist;
    unsigned char type_attr;
    unsigned short offset_mid;
    unsigned int offset_high;
    unsigned int reserved;
} __attribute__((packed));

struct idt_ptr {
    unsigned short limit;
    unsigned long base;
} __attribute__((packed));

static struct idt_entry idt[256];
static struct idt_ptr idtp;

extern void dummy_stub(void);
extern void irq1_stub(void);
extern void idt_load(struct idt_ptr* ptr);

static void idt_set_gate(int num, unsigned long handler, unsigned short selector, unsigned char type)
{
    idt[num].offset_low = handler & 0xFFFF;
    idt[num].selector = selector;
    idt[num].ist = 0;
    idt[num].type_attr = type;
    idt[num].offset_mid = (handler >> 16) & 0xFFFF;
    idt[num].offset_high = (handler >> 32) & 0xFFFFFFFF;
    idt[num].reserved = 0;
}

void idt_init(void)
{
    int i;
    for (i = 0; i < 256; i++) {
        idt_set_gate(i, (unsigned long)dummy_stub, 0x18, 0x8E);
    }
    idt_set_gate(0x21, (unsigned long)irq1_stub, 0x18, 0x8E);
    idtp.limit = sizeof(idt) - 1;
    idtp.base = (unsigned long)&idt;
    idt_load(&idtp);
}

void interrupts_enable(void)
{
    __asm__ volatile ("sti");
}

void interrupts_disable(void)
{
    __asm__ volatile ("cli");
}
