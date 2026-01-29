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
    
    jc .finish_and_add_extended
    
    cmp eax, 0x534D4150
    jne .finish_and_add_extended

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
    jge .finish_and_add_extended
    
    test ebx, ebx
    jne .mmap_loop

.finish_and_add_extended:
    mov si, MMAP_ADDR
    mov cx, bp
    test cx, cx
    jz .add_extended_only
    
    xor bx, bx
    
.check_extended_loop:
    mov eax, [si + 0]
    mov edx, [si + 4]
    
    cmp edx, 0
    jne .next_entry
    cmp eax, 0x100000
    jb .next_entry
    
    mov bx, 1
    jmp .mmap_done
    
.next_entry:
    add si, 20
    dec cx
    jnz .check_extended_loop
    
    test bx, bx
    jnz .mmap_done

.add_extended_only:
    mov ax, bp
    mov cx, 20
    mul cx
    mov di, MMAP_ADDR
    add di, ax
    
    mov eax, 0x100000
    stosd
    xor eax, eax
    stosd
    mov eax, 0x3F00000
    stosd
    xor eax, eax
    stosd
    mov eax, 1
    stosd
    
    inc bp
    
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
