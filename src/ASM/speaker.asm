[BITS 16]

beep:
    push ax
    push cx
    mov al, 0xB6
    out 0x43, al
    mov ax, 1193
    out 0x42, al
    mov al, ah
    out 0x42, al
    in al, 0x61
    or al, 0x03
    out 0x61, al
    mov cx, 0x38000
.delay1:
    loop .delay1
    in al, 0x61
    and al, 0xFC
    out 0x61, al
    pop cx
    pop ax
    ret
