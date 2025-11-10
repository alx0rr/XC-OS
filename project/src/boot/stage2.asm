[bits 16]
[org 0x7e00]
stage2_start:
    mov ah, 0x00
    mov al, 0x03
    int 0x10
    jmp load_xcos
load_xcos:
    mov si, loading_xcos_msg
    call prnt
    
    mov ah, 0x02
    mov al, 64
    mov ch, 0
    mov cl, 17
    mov dh, 0
    mov dl, [boot_drive]
    mov bx, 0x1000
    mov es, bx
    xor bx, bx
    int 0x13
    jc xcos_load_error
    
    mov si, vbe_init_msg
    call prnt
    
    ; Вызываем функцию инициализации VBE
    mov cx, 0x4118
    call init_vbe_mode
    
    cmp ax, 0x004F
    jne xcos_load_error
    
    mov si, mmap_init_msg
    call prnt
    
    ; Вызываем функцию получения карты памяти
    call get_memory_map
    
    cmp eax, 0
    je xcos_load_error
    
    cli
    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp 0x08:protected_mode_start
xcos_load_error:
    mov si, xcos_error_msg
    call prnt
    jmp halt_system
[bits 32]
protected_mode_start:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000
    mov esi, 0x10000
    mov edi, 0x100000
    mov ecx, 8192
    rep movsd
    jmp 0x08:0x100000
[bits 16]
prnt:
    lodsb
    test al, al
    jz print_done
    mov ah, 0x0E
    mov bh, 0
    int 0x10
    jmp prnt
print_done:
    ret
halt_system:
    cli
    hlt
    jmp halt_system
align 8
gdt_start:
    dd 0x0
    dd 0x0
gdt_code:
    dw 0xFFFF
    dw 0x0
    db 0x0
    db 10011010b
    db 11001111b
    db 0x0
gdt_data:
    dw 0xFFFF
    dw 0x0
    db 0x0
    db 10010010b
    db 11001111b
    db 0x0
gdt_end:
gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start
boot_drive: db 0x80
loading_xcos_msg: db 'Like a rolling stones... OK', 13, 10, 13, 10, 'Loading XC OS... OK', 13, 10, 0
vbe_init_msg: db 'Setting VBE mode... OK', 13, 10, 0
mmap_init_msg: db 'Getting memory map... OK', 13, 10, 0
xcos_error_msg: db 13, 10, 'Fatal: Failed to load XCOS!', 13, 10, 'System halted.', 13, 10, 0
;includes--------
%include "src/boot/utils/vbe.asm"
%include "src/boot/utils/mmap.asm"
times 7680-($-$$) db 0