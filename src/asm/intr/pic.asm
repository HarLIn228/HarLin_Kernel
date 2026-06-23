[BITS 64]

SECTION .text

global pic_init
global pic_eoi

pic_init:
    push rax

    mov al, 0xFF
    out 0x21, al
    out 0xA1, al

    mov al, 0x11
    out 0x20, al
    out 0x80, al
    out 0xA0, al
    out 0x80, al

    mov al, 0x20
    out 0x21, al
    out 0x80, al
    mov al, 0x28
    out 0xA1, al
    out 0x80, al

    mov al, 0x04
    out 0x21, al
    out 0x80, al
    mov al, 0x02
    out 0xA1, al
    out 0x80, al

    mov al, 0x01
    out 0x21, al
    out 0x80, al
    out 0xA1, al
    out 0x80, al

    mov al, 0xF8
    out 0x21, al
    mov al, 0xFF
    out 0xA1, al

    in al, 0x21
    mov cl, al
    and cl, 0x07
    test cl, cl
    jz .pic_mask_ok
    mov al, 0xF8
    out 0x21, al
.pic_mask_ok:

    pop rax
    ret
