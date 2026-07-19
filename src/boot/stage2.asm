[bits 16]
[org 0x7e00]

%include "src/boot/config.inc"

stage2_start:
    mov [boot_drive], dl
    mov ah, 0x00
    mov al, 0x03
    int 0x10

    mov si, s2msg
    call prnt

    call enter_unreal

    jmp load_kernel

enter_unreal:
    push eax
    push ds

    lgdt [ugdt_desc]

    mov eax, cr0
    or  eax, 1
    mov cr0, eax

    mov ax, 0x08
    mov ds, ax

    and eax, ~1
    mov cr0, eax

    jmp 0x0000:.back
.back:
    xor ax, ax
    mov ds, ax
    pop ds
    pop eax
    ret

load_kernel:
    mov si, lkmsg
    call prnt

    mov ah, 0x41
    mov bx, 0x55AA
    mov dl, [boot_drive]
    int 0x13
    jc  .no_lba

    mov word  [rem],  CFG_KERNEL_MAX_SECTORS
    mov dword [lba],  CFG_KERNEL_START_SECTOR
    mov dword [dst],  0x100000

.loop:
    mov cx, [rem]
    cmp cx, 0
    je  .done

    mov si, dotmsg
    call prnt

    cmp cx, 127
    jbe .last
    mov cx, 127
.last:
    mov [dap+2], cx

    mov byte  [dap],   0x10
    mov byte  [dap+1], 0
    mov word  [dap+4], 0x0000
    mov word  [dap+6], 0x1000

    mov eax, [lba]
    mov [dap+8],  eax
    mov dword [dap+12], 0

    mov ah, 0x42
    mov dl, [boot_drive]
    mov si, dap
    int 0x13
    jc  .err

    movzx eax, word [dap+2]
    mov [tmp], eax

    push ds
    xor ax, ax
    mov ds, ax

    mov esi, 0x10000
    mov edi, [dst]
    mov ecx, [tmp]
    shl ecx, 9
    mov [csz], ecx
    a32 rep movsb

    pop ds

    mov eax, [tmp]
    mov ecx, [csz]
    add dword [dst], ecx
    sub [rem], ax
    add [lba], eax

    jmp .loop

.done:
    mov si, nlmsg
    call prnt
    mov si, okmsg
    call prnt
    jmp init_vbe

.no_lba:
    mov si, nolbamsg
    call prnt
    jmp halt

.err:
    mov si, errmsg
    call prnt
    jmp halt

init_vbe:
    mov si, vbemsg
    call prnt
    mov cx, 0x4118
    call init_vbe_mode
    cmp ax, 0x004F
    jne .fail
    jmp init_mmap
.fail:
    mov si, errmsg
    call prnt
    jmp halt

init_mmap:
    mov si, mmapmsg
    call prnt
    xor ax, ax
    mov es, ax
    mov ds, ax
    call get_memory_map
    cmp eax, 0
    je  halt

    cli
    lgdt [gdt_desc]
    mov eax, cr0
    or  eax, 1
    mov cr0, eax
    jmp 0x08:pm_start

[bits 32]
pm_start:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000
    jmp 0x08:0x100000

[bits 16]
prnt:
    lodsb
    test al, al
    jz   .d
    mov  ah, 0x0E
    mov  bh, 0
    int  0x10
    jmp  prnt
.d: ret

halt:
    cli
    hlt
    jmp halt

align 8
ugdt_start:
    dq 0
ugdt_flat:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b
    db 11001111b
    db 0x00
ugdt_end:
ugdt_desc:
    dw ugdt_end - ugdt_start - 1
    dd ugdt_start

align 8
gdt_start:
    dq 0
gdt_code:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10011010b
    db 11001111b
    db 0x00
gdt_data:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b
    db 11001111b
    db 0x00
gdt_end:
gdt_desc:
    dw gdt_end - gdt_start - 1
    dd gdt_start

boot_drive: db 0x80

align 4
dap:        times 16 db 0

rem:        dw 0
lba:        dd 0
dst:        dd 0
tmp:        dd 0
csz:        dd 0

s2msg:    db 'XC Bootloader Stage 2...', 13, 10, 0
lkmsg:    db 'Loading kernel...', 0
dotmsg:   db '.', 0
nlmsg:    db 13, 10, 0
okmsg:    db 'Kernel loaded!', 13, 10, 0
nolbamsg: db 'No LBA support!', 13, 10, 0
errmsg:   db 13, 10, 'Fatal error. Halted.', 13, 10, 0
vbemsg:   db 'Setting VBE...', 13, 10, 0
mmapmsg:  db 'Getting memory map...', 13, 10, 0

%include "src/boot/utils/vbe.asm"
%include "src/boot/utils/mmap.asm"

times 15872-($-$$) db 0
