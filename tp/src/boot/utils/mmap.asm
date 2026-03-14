; Fuck yeah
; /ᐠ - ˕ -マ
; Booga Booga im forget about asm comments
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
.try_e820:
    xor ebx, ebx
    mov di, MMAP_ADDR
.e820_loop:
    mov eax, 0xE820
    mov edx, 0x534D4150
    mov ecx, 24
    mov dword [es:di+20], 1
    int 0x15
    jc .e820_failed
    cmp eax, 0x534D4150
    jne .e820_failed
    cmp ecx, 20
    jb .e820_failed
    mov ax, [COUNT_ADDR]
    inc ax
    cmp ax, MAX_ENTRIES
    jae .validate_e820
    mov [COUNT_ADDR], ax
    add di, 24
    test ebx, ebx
    jnz .e820_loop
.validate_e820:
    mov cx, [COUNT_ADDR]
    test cx, cx
    jz .e820_failed
    xor si, si
    mov di, MMAP_ADDR
.validate_loop:
    mov eax, [di]
    mov edx, [di+4]
    mov ebx, [di+16]
    cmp ebx, 1
    jne .validate_next
    cmp edx, 0
    ja .found_extended
    cmp eax, 0x100000
    jae .found_extended
.validate_next:
    add di, 24
    inc si
    cmp si, cx
    jb .validate_loop
    jmp .e820_failed
.found_extended:
    jmp .finalize
.e820_failed:
    mov word [COUNT_ADDR], 0
    mov di, MMAP_ADDR
.try_e801:
    xor cx, cx
    xor dx, dx
    mov ax, 0xE801
    int 0x15
    jc .try_88
    test cx, cx
    jnz .e801_use_cx
    test ax, ax
    jz .try_88
    mov cx, ax
    mov dx, bx
.e801_use_cx:
    mov di, MMAP_ADDR
    mov dword [di], 0x00000000
    mov dword [di+4], 0x00000000
    mov dword [di+8], 0x0009fc00
    mov dword [di+12], 0x00000000
    mov dword [di+16], 1
    mov dword [di+20], 0
    add di, 24
    movzx eax, cx
    shl eax, 10
    cmp eax, 0x3c00000
    jbe .e801_below_15m
    mov eax, 0x3c00000
.e801_below_15m:
    mov dword [di], 0x00100000
    mov dword [di+4], 0x00000000
    mov dword [di+8], eax
    mov dword [di+12], 0x00000000
    mov dword [di+16], 1
    mov dword [di+20], 0
    add di, 24
    test dx, dx
    jz .e801_done
    movzx eax, dx
    shl eax, 16
    mov dword [di], 0x01000000
    mov dword [di+4], 0x00000000
    mov dword [di+8], eax
    mov dword [di+12], 0x00000000
    mov dword [di+16], 1
    mov dword [di+20], 0
    mov word [COUNT_ADDR], 3
    jmp .finalize
.e801_done:
    mov word [COUNT_ADDR], 2
    jmp .finalize
.try_88:
    clc
    mov ah, 0x88
    int 0x15
    jc .try_cmos
    test ax, ax
    jz .try_cmos
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
    jmp .finalize
.try_cmos:
    mov al, 0x17
    out 0x70, al
    in al, 0x71
    mov ah, al
    mov al, 0x18
    out 0x70, al
    in al, 0x71
    mov cx, ax
    test cx, cx
    jz .assume_memory
    mov di, MMAP_ADDR
    mov dword [di], 0x00000000
    mov dword [di+4], 0x00000000
    mov dword [di+8], 0x0009fc00
    mov dword [di+12], 0x00000000
    mov dword [di+16], 1
    mov dword [di+20], 0
    add di, 24
    movzx eax, cx
    shl eax, 10
    mov dword [di], 0x00100000
    mov dword [di+4], 0x00000000
    mov dword [di+8], eax
    mov dword [di+12], 0x00000000
    mov dword [di+16], 1
    mov dword [di+20], 0
    mov word [COUNT_ADDR], 2
    jmp .finalize
.assume_memory:
    mov di, MMAP_ADDR
    mov dword [di], 0x00000000
    mov dword [di+4], 0x00000000
    mov dword [di+8], 0x0009fc00
    mov dword [di+12], 0x00000000
    mov dword [di+16], 1
    mov dword [di+20], 0
    add di, 24
    mov eax, 15 * 1024 * 1024
    mov dword [di], 0x00100000
    mov dword [di+4], 0x00000000
    mov dword [di+8], eax
    mov dword [di+12], 0x00000000
    mov dword [di+16], 1
    mov dword [di+20], 0
    mov word [COUNT_ADDR], 2
.finalize:
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
