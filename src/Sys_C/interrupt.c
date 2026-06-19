#include "interrupt.h"
#include "io.h"
#include "screen.h"
#include "gdt.h"

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

extern void isr0(void);
extern void isr1(void);
extern void isr2(void);
extern void isr3(void);
extern void isr4(void);
extern void isr5(void);
extern void isr6(void);
extern void isr7(void);
extern void isr8(void);
extern void isr9(void);
extern void isr10(void);
extern void isr11(void);
extern void isr12(void);
extern void isr13(void);
extern void isr14(void);
extern void isr15(void);
extern void isr16(void);
extern void isr17(void);
extern void isr18(void);
extern void isr19(void);
extern void isr20(void);
extern void isr21(void);
extern void isr22(void);
extern void isr23(void);
extern void isr24(void);
extern void isr25(void);
extern void isr26(void);
extern void isr27(void);
extern void isr28(void);
extern void isr29(void);
extern void isr30(void);
extern void isr31(void);

static void* isr_stubs[32] = {
    isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7,
    isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15,
    isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
    isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31
};

extern void irq0_stub(void);
extern void irq1_stub(void);
extern void irq2_stub(void);
extern void irq3_stub(void);
extern void irq4_stub(void);
extern void irq5_stub(void);
extern void irq6_stub(void);
extern void irq7_stub(void);
extern void irq8_stub(void);
extern void irq9_stub(void);
extern void irq10_stub(void);
extern void irq11_stub(void);
extern void irq12_stub(void);
extern void irq13_stub(void);
extern void irq14_stub(void);
extern void irq15_stub(void);
extern void syscall_stub(void);
extern void idt_load(struct idt_ptr* ptr);

static void (*irq_handlers[16])(void);
static void* irq_stubs[16] = {
    irq0_stub, irq1_stub, irq2_stub, irq3_stub,
    irq4_stub, irq5_stub, irq6_stub, irq7_stub,
    irq8_stub, irq9_stub, irq10_stub, irq11_stub,
    irq12_stub, irq13_stub, irq14_stub, irq15_stub
};

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
        idt_set_gate(i, (unsigned long)isr0, GDT_KERNEL_CODE, 0x8E);
    }
    for (i = 0; i < 32; i++) {
        idt_set_gate(i, (unsigned long)isr_stubs[i], GDT_KERNEL_CODE, 0x8E);
    }
    for (i = 0; i < 16; i++) {
        idt_set_gate(0x20 + i, (unsigned long)irq_stubs[i], GDT_KERNEL_CODE, 0x8E);
    }
    idt_set_gate(0x80, (unsigned long)syscall_stub, GDT_KERNEL_CODE, 0xEE);
    idtp.limit = sizeof(idt) - 1;
    idtp.base = (unsigned long)&idt;
    idt_load(&idtp);
}

void pic_send_eoi(int irq)
{
    if (irq >= 8) {
        outb(0xA0, 0x20);
    }
    outb(0x20, 0x20);
}

void irq_register(int irq, void (*handler)(void))
{
    if (irq >= 0 && irq < 16) {
        irq_handlers[irq] = handler;
    }
}

void irq_dispatch(unsigned int irq)
{
    if (irq < 16 && irq_handlers[irq]) {
        irq_handlers[irq]();
    }
    pic_send_eoi(irq);
}

static void print_hex_digit(unsigned int n)
{
    const char* hex = "0123456789ABCDEF";
    screen_put_char(hex[n & 0xF]);
}

static void print_hex64(unsigned long val)
{
    int i;
    for (i = 60; i >= 0; i -= 4) {
        print_hex_digit((val >> i) & 0xF);
    }
}

void isr_handler(unsigned int vector, unsigned long error_code)
{
    screen_put_char('E');
    screen_put_char('X');
    screen_put_char('C');
    screen_put_char(':');
    print_hex_digit(vector);
    screen_put_char(' ');
    screen_put_char('E');
    screen_put_char('R');
    screen_put_char('R');
    screen_put_char(':');
    print_hex64(error_code);
    screen_put_char('\n');
    interrupts_disable();
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

void interrupts_enable(void)
{
    __asm__ volatile ("sti");
}

void interrupts_disable(void)
{
    __asm__ volatile ("cli");
}
