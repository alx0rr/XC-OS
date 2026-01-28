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
    xor cx, cx
    mov dword [0x7FFC], 0
    
.mmap_loop:
    mov di, MMAP_BUFFER
    
    mov edx, 0x534D4150
    mov eax, 0xE820
    push ecx
    mov ecx, 24
    int 0x15
    pop ecx
    
    jc .mmap_done
    
    cmp eax, 0x534D4150
    jne .mmap_done

    mov si, MMAP_BUFFER
    mov ax, cx
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
    
    inc cx
    
    cmp cx, MAX_MMAP_ENTRIES
    jge .mmap_done
    
    test ebx, ebx
    jne .mmap_loop
    
.mmap_done:
    xor eax, eax
    mov ax, cx
    mov dword [0x7FFC], eax
    
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop bp
    ret
