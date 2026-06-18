[BITS 32]

SECTION .text

global dummy_stub
global irq1_stub
global idt_load

extern irq1_handler

dummy_stub:
    iret

irq1_stub:
    pusha
    push ds
    push es
    push fs
    push gs
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    call irq1_handler
    mov al, 0x20
    out 0x20, al
    pop gs
    pop fs
    pop es
    pop ds
    popa
    iret

idt_load:
    mov eax, [esp + 4]
    lidt [eax]
    ret
