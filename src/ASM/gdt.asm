[BITS 16]

gdt_start:
    dq 0
gdt_code_32:
    dw 0xFFFF
    dw 0
    db 0
    db 10011010b
    db 11001111b
    db 0
gdt_data_32:
    dw 0xFFFF
    dw 0
    db 0
    db 10010010b
    db 11001111b
    db 0
gdt_code_64:
    dw 0
    dw 0
    db 0
    db 10011010b
    db 00100000b
    db 0
gdt_data_64:
    dw 0
    dw 0
    db 0
    db 10010010b
    db 0
    db 0
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

CODE_SEG_32 equ gdt_code_32 - gdt_start
DATA_SEG_32 equ gdt_data_32 - gdt_start
CODE_SEG_64 equ gdt_code_64 - gdt_start
DATA_SEG_64 equ gdt_data_64 - gdt_start
