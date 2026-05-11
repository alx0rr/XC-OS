VBE_INFO_ADDR equ 0x5000

init_vbe_mode:
    push es
    push di
    push cx

    xor ax, ax
    mov es, ax
    mov di, VBE_INFO_ADDR
    mov ax, 0x4F01
    int 0x10
    cmp ax, 0x004F
    jne .vbe_error

    pop bx
    or bx, 0x4000
    mov ax, 0x4F02
    int 0x10
    cmp ax, 0x004F
    jne .vbe_error_nostack

    pop di
    pop es
    ret

.vbe_error:
    pop cx
.vbe_error_nostack:
    pop di
    pop es
    xor ax, ax
    ret
