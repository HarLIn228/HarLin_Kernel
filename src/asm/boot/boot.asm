[BITS 16]
[ORG 0x7C00]

start:
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    mov al, 'B'
    out 0xE9, al

    mov ah, 0x00
    mov al, 0x03
    int 0x10

    mov ah, 0x00
    mov dl, 0x00
    int 0x13
    jc disk_error

    mov al, 'R'
    out 0xE9, al

    call enable_a20

    mov al, 'L'
    out 0xE9, al

    call load_stage2
    mov al, 'J'
    out 0xE9, al
    jmp 0x0000:0x7E00

disk_error:
    mov al, 'E'
    out 0xE9, al
    hlt
    jmp disk_error

load_stage2:
    ret

dap:
    db 0x10
    db 0
    dw 16
    dw 0x0000
    dw 0x8000
    dq 20

%include "src/asm/boot/a20.asm"

times 510-($-$$) db 0
dw 0xAA55
