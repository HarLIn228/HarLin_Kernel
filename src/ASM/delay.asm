[BITS 16]

delay_1s:
    push ax
    push cx
    push dx
    mov ah, 0x86
    mov cx, 0x000F
    mov dx, 0x4240
    int 0x15
    pop dx
    pop cx
    pop ax
    ret
