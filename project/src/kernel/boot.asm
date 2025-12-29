[bits 32]
[extern kernel_main]
[global _start]

[global vbe_mode_info_data]

section .bss
align 16
stack_bottom:
    resb 16384
stack_top:

section .data
vbe_mode_info_data equ 0x5000

section .text
_start:
    mov esp, stack_top
    mov ebp, esp
    
    xor eax, eax
    xor ebx, ebx
    xor ecx, ecx
    xor edx, edx
    xor esi, esi
    xor edi, edi
    
    call kernel_main
    
    cli
.hang:
    hlt
    jmp .hang