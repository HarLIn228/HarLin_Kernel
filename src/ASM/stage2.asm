[BITS 16]
[ORG 0x8000]

stage2_entry:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    call e820_probe_16
    call clear_screen

    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp CODE_SEG_32:protected_mode

clear_screen:
    pusha
    mov ah, 0x06
    mov al, 0
    mov bh, 0x07
    mov cx, 0
    mov dh, 24
    mov dl, 79
    int 0x10
    popa
    ret

e820_probe_16:
    pusha
    push ds
    push es
    xor ax, ax
    mov ds, ax
    mov es, ax

    mov di, e820_buf
    xor ebx, ebx
.e820_loop:
    mov dword [di], 0
    mov dword [di+4], 0
    mov dword [di+8], 0
    mov dword [di+12], 0
    mov dword [di+16], 0

    mov eax, 0xE820
    mov edx, 0x534D4150
    mov ecx, 20
    int 0x15
    jc .e820_done
    cmp eax, 0x534D4150
    jne .e820_done
    test ebx, ebx
    jz .e820_done

    add di, 20
    cmp di, e820_buf_end
    jae .e820_done
    jmp .e820_loop
.e820_done:
    pop es
    pop ds
    popa
    ret

%include "src/asm/gdt.asm"

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

e820_buf:
    times 2048 db 0
e820_buf_end:

times 8192-($-$$) db 0
