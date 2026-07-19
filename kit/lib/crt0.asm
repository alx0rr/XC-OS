bits 32

global _start
extern main

section .text
_start:
    xor  ebp, ebp
    call main
    mov  ebx, eax
    mov  eax, 0
    int  0x80
    hlt

section .note.GNU-stack noalloc noexec nowrite progbits
