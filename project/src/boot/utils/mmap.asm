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
    mov ds, ax
    xor ebx, ebx
    xor bp, bp
    mov dword [0x7FFC], 0
    
.mmap_loop:
    mov di, MMAP_BUFFER
    
    mov edx, 0x534D4150
    mov eax, 0xE820
    mov ecx, 24
    int 0x15
    
    jc .mmap_done
    
    cmp eax, 0x534D4150
    jne .mmap_done

    cmp ecx, 20
    jl .check_next
    
    mov si, MMAP_BUFFER
    mov ax, bp
    mov cx, 20
    mul cx
    mov di, MMAP_ADDR
    add di, ax
    
    mov cx, 20
.copy_loop:
    mov al, [si]
    mov [di], al
    inc si
    inc di
    dec cx
    jnz .copy_loop
    
    inc bp
    
.check_next:
    cmp bp, MAX_MMAP_ENTRIES - 1
    jge .mmap_done
    
    test ebx, ebx
    jne .mmap_loop

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
