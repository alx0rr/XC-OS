[bits 32]
extern syscall_handler

global syscall_stub_asm
syscall_stub_asm:
    cli
    push byte 0
    push byte 0x80
    pusha
    mov ax, ds
    push eax
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    push esp
    call syscall_handler
    add esp, 4
    pop ebx
    mov ds, bx
    mov es, bx
    mov fs, bx
    mov gs, bx
    popa
    add esp, 8
    sti
    iret
