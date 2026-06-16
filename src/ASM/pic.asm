[BITS 32]

pic_init:
    push eax

    mov al, 0x11
    out 0x20, al
    out 0xA0, al

    mov al, 0x20
    out 0x21, al
    mov al, 0x28
    out 0xA1, al

    mov al, 0x04
    out 0x21, al
    mov al, 0x02
    out 0xA1, al

    mov al, 0x01
    out 0x21, al
    out 0xA1, al

    mov al, 0xFD
    out 0x21, al
    mov al, 0xFF
    out 0xA1, al

    pop eax
    ret

pic_eoi:
    push eax
    mov al, 0x20
    out 0x20, al
    pop eax
    ret
