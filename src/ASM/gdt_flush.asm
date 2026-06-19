[BITS 64]

SECTION .text

GDT_USER_CODE equ 0x2B
GDT_USER_DATA equ 0x33

global gdt_flush
global tss_flush
global jump_to_user

gdt_flush:
    lgdt [rdi]
    mov ax, 0x20
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    ret

tss_flush:
    ltr di
    ret

jump_to_user:
    push qword (GDT_USER_DATA | 3)
    push rdx
    push qword 0x202
    push qword (GDT_USER_CODE | 3)
    push rdi
    iretq
