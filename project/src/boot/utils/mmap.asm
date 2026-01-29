[bits 16]

MMAP_ADDR equ 0x8000
MMAP_BUFFER equ 0x9000
MAX_MMAP_ENTRIES equ 128

get_memory_map:
    push bp
    mov bp, sp
    push bx
    push cx
    push dx
    push si
    push di
    
    xor eax, eax
    mov es, ax
    xor ebx, ebx
    xor bp, bp
    mov dword [0x7FFC], 0
    
.mmap_loop:
    mov di, MMAP_BUFFER
    
    mov edx, 0x534D4150
    mov eax, 0xE820
    mov ecx, 24
    int 0x15
    
    jc .try_fallback
    
    cmp eax, 0x534D4150
    jne .try_fallback

    test ecx, ecx
    jz .check_next
    
    mov si, MMAP_BUFFER
    mov ax, bp
    push dx
    mov dx, 20
    mul dx
    pop dx
    mov di, MMAP_ADDR
    add di, ax
    
    push ecx
    mov ecx, 5
    rep movsd
    pop ecx
    
    inc bp
    
.check_next:
    cmp bp, MAX_MMAP_ENTRIES
    jge .mmap_done
    
    test ebx, ebx
    jne .mmap_loop
    
    jmp .mmap_done

.try_fallback:
    test bp, bp
    jnz .mmap_done
    
    mov ax, 0xE801
    int 0x15
    jc .manual_fallback
    
    movzx edx, ax
    shl edx, 10
    movzx ecx, bx
    shl ecx, 16
    add edx, ecx
    add edx, 0x100000
    
    mov dword [MMAP_ADDR], 0
    mov dword [MMAP_ADDR+4], 0
    mov dword [MMAP_ADDR+8], 0x9FC00
    mov dword [MMAP_ADDR+12], 0
    mov dword [MMAP_ADDR+16], 1
    
    mov dword [MMAP_ADDR+20], 0x100000
    mov dword [MMAP_ADDR+24], 0
    mov dword [MMAP_ADDR+28], edx
    mov dword [MMAP_ADDR+32], 0
    mov dword [MMAP_ADDR+36], 1
    
    mov bp, 2
    jmp .mmap_done

.manual_fallback:
    mov dword [MMAP_ADDR], 0
    mov dword [MMAP_ADDR+4], 0
    mov dword [MMAP_ADDR+8], 0x9FC00
    mov dword [MMAP_ADDR+12], 0
    mov dword [MMAP_ADDR+16], 1
    
    mov dword [MMAP_ADDR+20], 0x100000
    mov dword [MMAP_ADDR+24], 0
    mov dword [MMAP_ADDR+28], 0x3F00000
    mov dword [MMAP_ADDR+32], 0
    mov dword [MMAP_ADDR+36], 1
    
    mov bp, 2
    
.mmap_done:
    movzx eax, bp
    mov dword [0x7FFC], eax
    
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop bp
    ret
