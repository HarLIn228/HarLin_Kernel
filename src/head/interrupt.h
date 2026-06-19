#ifndef INTERRUPT_H
#define INTERRUPT_H

void idt_init(void);
void interrupts_enable(void);
void interrupts_disable(void);
void irq_register(int irq, void (*handler)(void));
void pic_send_eoi(int irq);

#endif
