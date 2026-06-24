[BITS 16]
[ORG 0x7E00]

stage2_entry:
    cli
    mov al, 'X'
    out 0xE9, al
    mov al, 'Y'
    out 0xE9, al
    mov al, 'Z'
    out 0xE9, al
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    mov al, 'S'
    out 0xE9, al

    call load_kernel
    mov al, 'K'
    out 0xE9, al

    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp CODE_SEG_32:protected_mode

load_kernel:
    pusha
    mov si, kernel_src
    xor di, di
    mov ax, 0x1000
    mov es, ax
    mov cx, 0x8000
    rep movsw
    popa
    ret

kernel_src: equ 0x9D50

%include "src/asm/core/gdt.asm"

[BITS 32]
protected_mode:
    cli

    mov al, 'P'
    out 0xE9, al

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
    mov al, 'M'
    out 0xE9, al

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

times 7680 db 0
