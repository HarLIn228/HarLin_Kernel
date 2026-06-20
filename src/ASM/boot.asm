[BITS 16]
[ORG 0x7C00]

start:
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    push dx

    mov ah, 0x00
    mov al, 0x03
    int 0x10

    pop dx
    push dx
    xor ax, ax
    int 0x13
    jc disk_error

    pop dx
    mov ah, 0x02
    mov al, 120
    mov ch, 0
    mov cl, 2
    mov dh, 0
    mov bx, 0x1000
    mov es, bx
    xor bx, bx
    int 0x13
    jc disk_error

    call delay_1s
    call enable_a20

    cli
    lgdt [gdt_descriptor]
    call beep

    mov ah, 0x06
    mov al, 0
    mov bh, 0x07
    mov cx, 0
    mov dh, 24
    mov dl, 79
    int 0x10

    mov ah, 0x02
    mov bh, 0
    mov dx, 0
    int 0x10

    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp CODE_SEG_32:protected_mode

[BITS 32]
protected_mode:
    cli
    mov ax, DATA_SEG_32
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000

    mov edi, 0x20000
    xor eax, eax
    mov ecx, 0xC00
    rep stosd

    mov eax, 0x21003
    mov [0x20000], eax

    mov eax, 0x22003
    mov [0x21000], eax

    mov edi, 0x22000
    mov eax, 0x83
    mov ecx, 512
set_pd:
    mov [edi], eax
    add edi, 8
    add eax, 0x200000
    loop set_pd

    mov eax, 0x20000
    mov cr3, eax
    mov eax, cr4
    or eax, 0x20
    mov cr4, eax
    mov ecx, 0xC0000080
    rdmsr
    or eax, 0x100
    wrmsr
    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax
    jmp CODE_SEG_64:long_mode

[BITS 64]
long_mode:
    mov ax, DATA_SEG_64
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov rsp, 0x90000
    mov rax, 0x10000
    call rax
    jmp $

[BITS 16]
setup_vesa:
    mov ax, 0x4F02
    mov bx, 0x4115
    int 0x10
    cmp ax, 0x004F
    jne .vbe_done

    xor ax, ax
    mov es, ax
    mov di, 0x7000
    mov ax, 0x4F01
    mov cx, 0x4115
    int 0x10

.vbe_done:
    ret

disk_error:
    jmp $

%include "src/ASM/delay.asm"
%include "src/ASM/a20.asm"
%include "src/ASM/speaker.asm"
%include "src/ASM/gdt.asm"

times 510-($-$$) db 0
dw 0xAA55
