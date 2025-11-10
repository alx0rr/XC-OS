[bits 16]

MMAP_ADDR equ 0x8000
MMAP_BUFFER equ 0x9000    ; Временный буфер для BIOS
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
    mov di, MMAP_BUFFER     ; Всегда пишем в один и тот же буфер
    
    mov edx, 0x534D4150
    mov eax, 0xE820
    mov ecx, 20
    int 0x15
    
    jc .mmap_done
    
    cmp edx, 0x534D4150
    jne .mmap_done
    
    ; Копируем 20 байт из MMAP_BUFFER в нужное место
    mov si, MMAP_BUFFER
    mov ax, bp
    mov cx, 20
    mul cx                  ; ax = bp * 20
    mov di, MMAP_ADDR
    add di, ax
    
    mov cx, 5               ; Копируем 5 DWORD (20 байт)
    rep movsd
    
    inc bp
    
    cmp bp, MAX_MMAP_ENTRIES
    jge .mmap_done
    
    test ebx, ebx
    jne .mmap_loop
    
.mmap_done:
    mov dword [0x7FFC], ebp
    mov eax, ebp
    
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop bp
    ret