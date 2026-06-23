[BITS 64]

SECTION .text

global atomic_xchg
global atomic_add
global atomic_sub
global atomic_inc
global atomic_dec
global atomic_cmpxchg

atomic_xchg:
    mov rax, rsi
    lock xchg [rdi], rax
    ret

atomic_add:
    mov rax, rsi
    lock xadd [rdi], rax
    ret

atomic_sub:
    mov rax, rsi
    neg rax
    lock xadd [rdi], rax
    ret

atomic_inc:
    mov rax, 1
    lock xadd [rdi], rax
    ret

atomic_dec:
    mov rax, -1
    lock xadd [rdi], rax
    ret

atomic_cmpxchg:
    mov rax, rsi
    lock cmpxchg [rdi], rdx
    sete al
    movzx eax, al
    ret
