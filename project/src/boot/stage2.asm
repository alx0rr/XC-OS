[bits 16]
[org 0x7e00]

stage2_start:
    mov ah, 0x00
    mov al, 0x03
    int 0x10
    
    mov si, stage2_msg
    call prnt
    
    jmp load_kernel

load_kernel:
    mov si, loading_kernel_msg
    call prnt
    
    mov ah, 0x41
    mov bx, 0x55AA
    mov dl, [boot_drive]
    int 0x13
    jc .no_lba_support
    
    mov word [load_counter], 0
    mov cx, 2017
    mov word [dap_remaining_sectors], cx
    mov dword [dap_current_lba], 32
    mov word [dap_current_segment], 0x1000
    mov word [dap_current_offset], 0x0000
    
.load_loop:
    mov cx, [dap_remaining_sectors]
    cmp cx, 0
    je .load_success
    
    mov si, dot_msg
    call prnt
    
    cmp cx, 127
    jbe .load_last_chunk
    mov word [dap+2], 127
    jmp .do_load
    
.load_last_chunk:
    mov [dap+2], cx
    
.do_load:
    mov si, dap
    mov byte [si], 0x10
    mov byte [si+1], 0
    mov ax, [dap_current_offset]
    mov [si+4], ax
    mov ax, [dap_current_segment]
    mov [si+6], ax
    mov eax, [dap_current_lba]
    mov [si+8], eax
    mov dword [si+12], 0
    
    mov ah, 0x42
    mov dl, [boot_drive]
    int 0x13
    jc kernel_load_error
    
    mov ax, [dap+2]
    sub [dap_remaining_sectors], ax
    
    movzx eax, ax
    add [dap_current_lba], eax
    
    shl ax, 9
    add [dap_current_offset], ax
    jnc .no_segment_wrap
    
    mov ax, [dap_current_segment]
    add ax, 0x1000
    mov [dap_current_segment], ax
    mov word [dap_current_offset], 0
    
.no_segment_wrap:
    inc word [load_counter]
    jmp .load_loop
    
.no_lba_support:
    mov si, no_lba_msg
    call prnt
    jmp halt_system
    
.load_success:
    mov si, newline_msg
    call prnt
    mov si, kernel_loaded_msg
    call prnt
    
    mov si, vbe_init_msg
    call prnt
    mov cx, 0x4118
    call init_vbe_mode
    cmp ax, 0x004F
    jne kernel_load_error
    
    mov si, mmap_init_msg
    call prnt
    call get_memory_map
    cmp eax, 0
    je kernel_load_error
    
    cli
    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp 0x08:protected_mode_start

kernel_load_error:
    mov si, kernel_error_msg
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
    mov ecx, 258304
    rep movsb
    
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
load_counter: dw 0

align 4
dap:
    times 16 db 0

dap_remaining_sectors: dw 0
dap_current_lba: dd 0
dap_current_segment: dw 0
dap_current_offset: dw 0

stage2_msg: db 'XC Bootloader Stage 2...', 13, 10, 0
loading_kernel_msg: db 'Loading...', 0
dot_msg: db '.', 0
newline_msg: db 13, 10, 0
kernel_loaded_msg: db 'Kernel loaded!', 13, 10, 0
no_lba_msg: db 'LBA not supported!', 13, 10, 0
vbe_init_msg: db 'Setting VBE mode...', 13, 10, 0
mmap_init_msg: db 'Getting memory map...', 13, 10, 0
kernel_error_msg: db 13, 10, 'Fatal: Failed to load kernel!', 13, 10, 'System halted.', 13, 10, 0

%include "src/boot/utils/vbe.asm"
%include "src/boot/utils/mmap.asm"

times 15872-($-$$) db 0
