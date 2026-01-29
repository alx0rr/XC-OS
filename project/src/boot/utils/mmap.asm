[bits 16]

MMAP_ADDR equ 0x8000
COUNT_ADDR equ 0x7FFC
MAX_ENTRIES equ 128

get_memory_map:
    push bp
    mov bp, sp
    push bx
    push si
    push di
    push dx
    push cx
    push ax

    xor eax, eax
    mov ax, 0
    mov es, ax
    mov ds, ax
    xor ebx, ebx
    mov di, MMAP_ADDR
    mov word [COUNT_ADDR], 0
    mov word [COUNT_ADDR+2], 0

.e820_loop:
    mov eax, 0xE820
    mov edx, 0x534D4150
    mov ecx, 24
    int 0x15
    jc .e820_done
    cmp eax, 0x534D4150
    jne .e820_done

    mov ax, [COUNT_ADDR]
    inc ax
    cmp ax, MAX_ENTRIES
    jae .e820_done
    mov [COUNT_ADDR], ax
    add di, 24

    test ebx, ebx
    jnz .e820_loop

.e820_done:
    mov ax, [COUNT_ADDR]
    mov word [COUNT_ADDR], ax
    mov word [COUNT_ADDR+2], 0

    pop ax
    pop cx
    pop dx
    pop di
    pop si
    pop bx
    pop bp
    ret
