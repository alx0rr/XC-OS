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
    jc .try_fallback
    cmp eax, 0x534D4150
    jne .try_fallback

    mov ax, [COUNT_ADDR]
    inc ax
    cmp ax, MAX_ENTRIES
    jae .e820_done
    mov [COUNT_ADDR], ax
    add di, 24

    test ebx, ebx
    jnz .e820_loop
    jmp .check_entries

.try_fallback:
    mov ax, [COUNT_ADDR]
    test ax, ax
    jnz .check_entries
    
    mov ah, 0x88
    int 0x15
    jc .e820_done
    
    test ax, ax
    jz .e820_done
    
    mov di, MMAP_ADDR
    
    mov dword [di], 0x00000000
    mov dword [di+4], 0x00000000
    mov dword [di+8], 0x0009fc00
    mov dword [di+12], 0x00000000
    mov dword [di+16], 1
    mov dword [di+20], 0
    add di, 24
    
    movzx eax, ax
    shl eax, 10
    mov dword [di], 0x00100000
    mov dword [di+4], 0x00000000
    mov dword [di+8], eax
    mov dword [di+12], 0x00000000
    mov dword [di+16], 1
    mov dword [di+20], 0
    
    mov word [COUNT_ADDR], 2
    jmp .e820_done

.check_entries:
    mov ax, [COUNT_ADDR]
    cmp ax, 1
    jbe .try_fallback

.e820_done:
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
