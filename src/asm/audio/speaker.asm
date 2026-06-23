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
    mov cx, 0xFFFF
.delay1:
    loop .delay1
    mov cx, 0xFFFF
.delay2:
    loop .delay2
    mov cx, 0xFFFF
.delay3:
    loop .delay3
    in al, 0x61
    and al, 0xFC
    out 0x61, al
    pop cx
    pop ax
    ret

[BITS 64]

SECTION .text

global speaker_beep

speaker_beep:
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    mov rdx, 1193180
    mov rax, rdx
    xor rdx, rdx
    div rdi
    mov rdi, rax
    mov al, 0xB6
    out 0x43, al
    mov al, dil
    out 0x42, al
    mov al, dil
    shr rdi, 8
    out 0x42, al
    in al, 0x61
    or al, 0x03
    out 0x61, al
    mov rcx, rsi
    shl rcx, 16
.delay:
    loop .delay
    in al, 0x61
    and al, 0xFC
    out 0x61, al
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax
    ret
