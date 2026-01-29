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
    test ax, ax
    jz .add_extended
    
    mov bx, ax
    dec bx
    imul bx, 24
    add bx, MMAP_ADDR
    add bx, 8
    
    mov eax, [bx]
    mov edx, [bx+4]
    
    cmp edx, 0
    ja .add_extended
    cmp eax, 0x100000
    jae .skip_extended

.add_extended:
    mov ah, 0x88
    int 0x15
    jc .skip_extended
    
    test ax, ax
    jz .skip_extended
    
    mov bx, [COUNT_ADDR]
    cmp bx, MAX_ENTRIES
    jae .skip_extended
    
    imul di, bx, 24
    add di, MMAP_ADDR
    
    mov dword [di], 0x00100000
    mov dword [di+4], 0x00000000
    
    movzx eax, ax
    shl eax, 10
    mov dword [di+8], eax
    mov dword [di+12], 0x00000000
    mov dword [di+16], 1
    mov dword [di+20], 0
    
    inc bx
    mov [COUNT_ADDR], bx

.skip_extended:
    mov ax, [COUNT_ADDR]
    mov word [COUNT_ADDR], ax
    mov word [COUNT_ADDR+2], 0

    pop cx
    pop dx
    pop di
    pop si
    pop bx
    pop bp
    
    movzx eax, word [COUNT_ADDR]
    ret
