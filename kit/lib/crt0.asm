bits 32

global _start
extern main

section .text
_start:
    xor  ebp, ebp
    call main
    mov  ebx, eax
    mov  eax, 0        ; SYS_EXIT
    int  0x80
    hlt
