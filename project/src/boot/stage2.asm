[bits 16]
[org 0x7e00]

VBE_INFO_ADDR equ 0x5000

stage2_start:
    mov ah, 0x00
    mov al, 0x03
    int 0x10
    mov si, detect_msg
    call prnt
    call dtct
    mov si, found_msg
    call prnt
    mov al, [drive_count]
    add al, '0'
    mov ah, 0x0E
    mov bh, 0
    int 0x10
    mov cx, 0xFFFF
delay_start:
    loop delay_start

    mov byte [selection], 0
    call drw

ml:
    xor ah, ah
    int 0x16
    cmp ah, 0x48
    je kup
    cmp ah, 0x50
    je kdn
    cmp ah, 0x1C
    je kent
    jmp ml

kup:
    cmp byte [selection], 0
    je ml
    dec byte [selection]
    call drw
    jmp ml

kdn:
    mov al, [selection]
    mov bl, [total_items]
    dec bl
    cmp al, bl
    jae ml
    inc byte [selection]
    call drw
    jmp ml

kent:
    mov al, [selection]
    mov bl, [total_items]
    sub bl, 3
    cmp al, bl
    je load_xcos
    inc bl
    cmp al, bl
    je rebt
    inc bl
    cmp al, bl
    je shtdn
    jmp ldos

rebt:
    db 0xEA
    dw 0x0000
    dw 0xFFFF

shtdn:
    mov ax, 0x5307
    mov bx, 0x0001
    mov cx, 0x0003
    int 0x15
    jmp $

load_xcos:
    mov ah, 0x00
    mov al, 0x03
    int 0x10
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
    
    push es
    xor ax, ax
    mov es, ax
    mov di, VBE_INFO_ADDR       
    mov ax, 0x4F01             
    mov cx, 0x4118              
    int 0x10
    pop es
    
    cmp ax, 0x004F            
    jne xcos_load_error

    mov ax, 0x4F02
    mov bx, 0x4118
    or bx, 0x4000
    int 0x10
    
    cmp ax, 0x004F
    jne xcos_load_error
    
    cli
    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp 0x08:protected_mode_start

xcos_load_error:
    mov si, xcos_error_msg
    call prnt
    jmp wait_key

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

dtct:
    xor cx, cx
    mov di, drive_list
    mov dl, 0x00
detect_floppy:
    cmp dl, 0x02
    jae detect_hdd_start
    push dx
    push cx
    push di
    mov ah, 0x08
    int 0x13
    pop di
    pop cx
    pop dx
    jc skip_floppy
    test cl, cl
    jz skip_floppy
    mov byte [di], dl
    inc di
    inc cx
skip_floppy:
    inc dl
    jmp detect_floppy

detect_hdd_start:
    mov dl, 0x80
detect_hdd:
    cmp dl, 0x88
    jae detect_done
    push dx
    push cx
    push di
    mov ah, 0x08
    int 0x13
    pop di
    pop cx
    pop dx
    jc skip_hdd
    mov byte [di], dl
    inc di
    inc cx
skip_hdd:
    inc dl
    jmp detect_hdd
detect_done:
    cmp cl, 10
    jbe dtct_ok
    mov cl, 10
dtct_ok:
    mov [drive_count], cl
    add cl, 3
    mov [total_items], cl
    ret

drw:
    mov ah, 0x00
    mov al, 0x03
    int 0x10
    mov dh, 0
    mov dl, 0
    call scur
    mov si, header
    call prnt
    call show_sysinfo
    xor cx, cx
dit:
    mov al, [total_items]
    cmp cl, al
    jae dd
    cmp cl, 20
    jae dd
    mov dh, cl
    inc dh
    mov dl, 0
    call scur
    mov si, item_prefix
    call prnt
    mov al, [selection]
    cmp al, cl
    jne ns
    mov si, arrow
    call prnt
    jmp sa
ns:
    mov si, space
    call prnt
sa:
    mov al, [drive_count]
    cmp cl, al
    jae print_special
    mov bx, cx
    mov al, [drive_list + bx]
    cmp al, 0x80
    jae print_hdd
print_floppy:
    mov si, floppy_str
    call print_string
    mov bx, cx
    mov al, [drive_list + bx]
    add al, '0'
    mov ah, 0x0E
    mov bh, 0
    int 0x10
    jmp print_suffix
print_hdd:
    mov si, disk_str
    call print_string
    mov bx, cx
    mov al, [drive_list + bx]
    sub al, 0x80
    add al, '1'
    mov ah, 0x0E
    mov bh, 0
    int 0x10
    jmp print_suffix
print_special:
    mov al, [drive_count]
    cmp cl, al
    je print_xcos_item
    inc al
    cmp cl, al
    je print_reboot_item
    mov si, shutdown_str
    call print_string
    jmp print_suffix
print_xcos_item:
    mov si, xcos_str
    call print_string
    jmp print_suffix
print_reboot_item:
    mov si, reboot_str
    call print_string
print_suffix:
    mov si, item_suffix
    call print_string
    inc cx
    jmp dit
dd:
    mov dh, cl
    inc dh
    mov dl, 0
    call scur
    mov si, footer
    call prnt
    ret

scur:
    mov ah, 0x02
    mov bh, 0
    int 0x10
    ret

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

print_string:
    lodsb
    test al, al
    jz ps_done
    mov ah, 0x0E
    mov bh, 0
    int 0x10
    jmp print_string
ps_done:
    ret

ldos:
    mov ah, 0x00
    mov al, 0x03
    int 0x10
    mov si, loading_msg
    call prnt
    mov bx, cx
    mov dl, [drive_list + bx]
    mov ah, 0x00
    int 0x13
    jc load_error
    mov ah, 0x02
    mov al, 1
    mov ch, 0
    mov cl, 1
    mov dh, 0
    mov bx, 0x7C00
    int 0x13
    jc load_error
    cmp word [0x7DFE], 0xAA55
    jne no_bootable
    mov si, boot_msg
    call prnt
    mov cx, 0x0F
    mov dx, 0xFFFF
delay:
    loop delay
    dec dx
    jnz delay
    jmp 0x0000:0x7C00

load_error:
    mov si, error_msg
    call prnt
    jmp wait_key

no_bootable:
    mov si, not_bootable_msg
    call prnt
    jmp wait_key

wait_key:
    mov si, press_key_msg
    call prnt
    xor ah, ah
    int 0x16
    jmp stage2_start

show_sysinfo:
    mov si, sysinfo_header
    call prnt
    mov dx, 0x70
    mov al, 0x00
    out dx, al
    inc dx
    in al, dx
    call print_hex
    mov al, ':'
    call prnt_char
    mov dx, 0x70
    mov al, 0x02
    out dx, al
    inc dx
    in al, dx
    call print_hex
    mov al, 13
    call prnt_char
    mov al, 10
    call prnt_char
    ret

print_hex:
    push ax
    mov ah, al
    shr ah, 4
    call hex_digit
    mov ah, al
    mov al, ah
    and al, 0x0F
    call hex_digit
    pop ax
    ret

hex_digit:
    cmp al, 9
    jbe digit_ok
    add al, 7
digit_ok:
    add al, '0'
    call prnt_char
    ret

prnt_char:
    mov ah, 0x0E
    mov bh, 0
    int 0x10
    ret

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

selection db 0
drive_count db 0
total_items db 0
boot_drive db 0x80

header db '=== XC Loader ===', 13, 10, 13, 10, 0
item_prefix db ' ', 0
item_suffix db 13, 10, 0
arrow db '> ', 0
space db '  ', 0
footer db 13, 10, '=================', 13, 10, 0

disk_str db 'DISK ', 0
floppy_str db 'FLOPPY ', 0
xcos_str db 'Load XC OS', 0
reboot_str db 'Reboot', 0
shutdown_str db 'Shutdown', 0

loading_msg db 13, 10, 'Loading from selected drive...', 13, 10, 0
loading_xcos_msg db 13, 10, 'Loading XC OS...', 13, 10, 0
vbe_init_msg db 'Setting VBE mode...', 13, 10, 0
error_msg db 'ERROR: Failed to read from drive!', 13, 10, 0
xcos_error_msg db 'ERROR: Failed to load XC OS!', 13, 10, 0
not_bootable_msg db 'ERROR: No bootable sector found!', 13, 10, 0
boot_msg db 'Booting...', 13, 10, 0
press_key_msg db 'Press any key to return to menu...', 0
detect_msg db 'Detecting drives...', 13, 10, 0
found_msg db 'Found drives: ', 0
sysinfo_header db 'SYS INFO:', 13, 10, 0

drive_list times 20 db 0

times 7680-($-$$) db 0