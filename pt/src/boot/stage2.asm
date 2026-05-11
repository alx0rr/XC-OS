[bits 16]
[org 0x7e00]

KERNEL_SECTORS equ 2017
KERNEL_SIZE    equ KERNEL_SECTORS * 512

stage2_start:
    mov [boot_drive], dl
    mov ah, 0x00
    mov al, 0x03
    int 0x10

    mov si, msg_s2start
    call prnt

    mov ah, 0x41
    mov bx, 0x55AA
    mov dl, [boot_drive]
    int 0x13
    jc .no_lba

    mov si, msg_lba_ok
    call prnt
    jmp .do_load

.no_lba:
    mov si, msg_no_lba
    call prnt
    jmp halt_system

.do_load:
    mov word [dap_remaining_sectors], KERNEL_SECTORS
    mov dword [dap_current_lba], 32
    mov dword [linear_addr], 0x10000

    mov si, msg_loading
    call prnt

.load_loop:
    mov cx, [dap_remaining_sectors]
    cmp cx, 0
    je .load_done

    cmp cx, 127
    jbe .last_chunk
    mov word [dap+2], 127
    jmp .do_int13

.last_chunk:
    mov [dap+2], cx

.do_int13:
    mov eax, [linear_addr]
    mov ebx, eax
    shr ebx, 4
    mov [dap+6], bx
    and ax, 0x000F
    mov [dap+4], ax

    mov byte [dap], 0x10
    mov byte [dap+1], 0
    mov eax, [dap_current_lba]
    mov [dap+8], eax
    mov dword [dap+12], 0

    mov ah, 0x42
    mov dl, [boot_drive]
    mov si, dap
    int 0x13
    jc .disk_error

    movzx eax, word [dap+2]
    sub [dap_remaining_sectors], ax
    add [dap_current_lba], eax
    shl eax, 9
    add [linear_addr], eax

    mov si, msg_dot
    call prnt
    jmp .load_loop

.disk_error:
    mov si, msg_disk_err
    call prnt
    mov si, msg_ah
    call prnt
    mov al, ah
    call print_hex_byte
    mov si, msg_nl
    call prnt
    jmp halt_system

.load_done:
    mov si, msg_nl
    call prnt
    mov si, msg_loaded
    call prnt

    mov si, msg_vbe
    call prnt
    mov cx, 0x4118
    call init_vbe_mode
    push ax
    mov si, msg_vbe_ret
    call prnt
    pop ax
    push ax
    call print_hex_word
    mov si, msg_nl
    call prnt
    pop ax
    cmp ax, 0x004F
    jne .vbe_fail

    mov si, msg_vbe_ok
    call prnt
    jmp .do_mmap

.vbe_fail:
    mov si, msg_vbe_fail
    call prnt
    jmp halt_system

.do_mmap:
    mov si, msg_mmap
    call prnt
    call get_memory_map
    push eax
    mov si, msg_mmap_ret
    call prnt
    pop eax
    push eax
    movzx eax, ax
    call print_hex_word
    mov si, msg_nl
    call prnt
    pop eax
    cmp eax, 0
    je .mmap_fail

    mov si, msg_mmap_ok
    call prnt
    jmp .go_pm

.mmap_fail:
    mov si, msg_mmap_fail
    call prnt
    jmp halt_system

.go_pm:
    mov si, msg_pm
    call prnt
    cli
    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp 0x08:protected_mode_start

print_hex_byte:
    push ax
    shr al, 4
    call .nibble
    pop ax
    and al, 0x0F
    call .nibble
    ret
.nibble:
    cmp al, 9
    jbe .digit
    add al, 'A' - 10
    jmp .out
.digit:
    add al, '0'
.out:
    mov ah, 0x0E
    mov bh, 0
    int 0x10
    ret

print_hex_word:
    push ax
    mov al, ah
    call print_hex_byte
    pop ax
    call print_hex_byte
    ret

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
    mov ecx, KERNEL_SIZE
    rep movsb

    jmp 0x08:0x100000

[bits 16]
prnt:
    lodsb
    test al, al
    jz .done
    mov ah, 0x0E
    mov bh, 0
    int 0x10
    jmp prnt
.done:
    ret

halt_system:
    mov si, msg_halt
    call prnt
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

boot_drive:  db 0x80

align 4
dap:
    times 16 db 0

dap_remaining_sectors: dw 0
dap_current_lba:       dd 0
linear_addr:           dd 0

msg_s2start:   db 'Stage2 OK', 13, 10, 0
msg_lba_ok:    db 'LBA OK', 13, 10, 0
msg_no_lba:    db 'NO LBA!', 13, 10, 0
msg_loading:   db 'Loading', 0
msg_dot:       db '.', 0
msg_loaded:    db 'Kernel loaded', 13, 10, 0
msg_disk_err:  db 13, 10, 'DISK ERR AH=0x', 0
msg_ah:        db 0
msg_vbe:       db 'VBE init...', 13, 10, 0
msg_vbe_ret:   db 'VBE ret=0x', 0
msg_vbe_ok:    db 'VBE OK', 13, 10, 0
msg_vbe_fail:  db 'VBE FAIL!', 13, 10, 0
msg_mmap:      db 'MMAP...', 13, 10, 0
msg_mmap_ret:  db 'MMAP entries=0x', 0
msg_mmap_ok:   db 'MMAP OK', 13, 10, 0
msg_mmap_fail: db 'MMAP FAIL!', 13, 10, 0
msg_pm:        db 'Entering PM...', 13, 10, 0
msg_halt:      db 13, 10, '*** HALTED ***', 13, 10, 0
msg_nl:        db 13, 10, 0

%include "src/boot/utils/vbe.asm"
%include "src/boot/utils/mmap.asm"

times 15872-($-$$) db 0
