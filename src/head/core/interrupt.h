#ifndef INTERRUPT_H
#define INTERRUPT_H

struct idt_ptr {
    unsigned short limit;
    unsigned long base;
} __attribute__((packed));

void idt_init(void);
void interrupts_enable(void);
void interrupts_disable(void);
void irq_register(int irq, void (*handler)(void));
void pic_send_eoi(int irq);

#endif
