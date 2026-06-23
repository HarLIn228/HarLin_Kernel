[BITS 16]
[ORG 0x7C00]

start:
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    mov ah, 0x00
    mov al, 0x03
    int 0x10

    mov ah, 0x00
    mov dl, 0x00
    int 0x13
    jc disk_error

    mov bx, 0x1000
    mov es, bx
    xor bx, bx
    xor ch, ch
    xor dh, dh
    mov cl, 10
read_track:
    push cx
read_loop:
    push bx
    push cx
    push dx
    mov di, 3
.retry:
    mov ah, 0x02
    mov al, 1
    mov dl, 0x00
    int 0x13
    jnc .read_ok
    pushf
    xor ah, ah
    mov dl, 0x00
    int 0x13
    popf
    dec di
    jnz .retry
    pop dx
    pop cx
    pop bx
    jmp disk_error
.read_ok:
    pop dx
    pop cx
    pop bx
    add bx, 512
    inc cl
    cmp cl, 19
    jb read_loop
    pop cx
    inc ch
    cmp ch, 80
    jb read_track

    call delay_1s
    call enable_a20

    call load_stage2
    jmp 0x0000:0x8000

disk_error:
    mov al, ah
    add al, '0'
    mov dx, 0x0402
    out dx, al
    hlt
    jmp disk_error

load_stage2:
    pusha
    mov ah, 0x02
    mov al, 8
    mov cl, 2
    mov ch, 0
    mov dh, 1
    mov dl, 0x00
    mov bx, 0x8000
    mov es, bx
    xor bx, bx
    int 0x13
    jc disk_error
    popa
    ret

%include "src/asm/delay.asm"
%include "src/asm/a20.asm"
%include "src/asm/speaker.asm"
%include "src/asm/gdt.asm"

times 510-($-$$) db 0
dw 0xAA55
