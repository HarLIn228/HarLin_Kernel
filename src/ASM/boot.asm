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

    mov si, msg_title
    call print_string

    mov si, msg_boot
    call print_string

    mov ah, 0x02
    mov al, 50
    mov ch, 0
    mov cl, 2
    mov dh, 0
    mov bx, 0x1000
    mov es, bx
    xor bx, bx
    int 0x13
    jc disk_error

    mov si, msg_ok
    call print_string
    call delay_1s

    mov si, msg_a20
    call print_string
    call enable_a20
    mov si, msg_ok
    call print_string
    call delay_1s

    mov si, msg_gdt
    call print_string
    cli
    lgdt [gdt_descriptor]
    mov si, msg_ok
    call print_string
    call delay_1s

    mov si, msg_spk
    call print_string
    call beep
    mov si, msg_ok
    call print_string
    call delay_1s

    mov si, msg_prot
    call print_string
    call delay_1s

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
    jmp CODE_SEG:protected_mode

[BITS 32]
protected_mode:
    cli
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000
    call 0x10000
    jmp $

[BITS 16]
disk_error:
    mov si, msg_err
    call print_string
    jmp $

msg_title:
    db "HarLin Boot", 0x0D, 0x0A, 0
msg_boot:
    db "Load Kernel....", 0
msg_ok:
    db "OK", 0x0D, 0x0A, 0
msg_a20:
    db "A20.......", 0
msg_gdt:
    db "GDT.......", 0
msg_spk:
    db "Speaker...", 0
msg_prot:
    db "PM Mode", 0x0D, 0x0A, 0
msg_err:
    db "Error!", 0

%include "src/ASM/print.asm"
%include "src/ASM/delay.asm"
%include "src/ASM/a20.asm"
%include "src/ASM/speaker.asm"
%include "src/ASM/gdt.asm"

times 510-($-$$) db 0
dw 0xAA55
