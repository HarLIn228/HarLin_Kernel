#ifndef INTERRUPT_H
#define INTERRUPT_H

void idt_init(void);
void interrupts_enable(void);
void interrupts_disable(void);

#endif
