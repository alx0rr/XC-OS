[bits 16]
[org 0x7c00]
start:
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    
    mov [boot_drive], dl
    
    mov ah, 0x00
    mov al, 0x03
    int 0x10
    
    mov si, lmsg
    call prnt
    
    call ld2
    
    mov si, smsg
    call prnt
    
    jmp 0x0000:0x7e00

ld2:
    mov si, 0
.retry:
    cmp si, 3
    je .failed
    
    mov ah, 0x02
    mov al, 15
    mov ch, 0
    mov cl, 2
    mov dh, 0
    mov dl, [boot_drive]
    mov bx, 0x7E00
    
    int 0x13
    jc .error
    ret

.error:
    inc si
    mov ax, 0
    int 0x13
    
    mov si, rtmsg
    call prnt
    jmp .retry

.failed:
    mov si, emsg
    call prnt
    xor ah, ah
    int 0x16
    jmp ld2

prnt:
    lodsb
    test al, al
    jz .pd
    mov ah, 0x0E
    mov bh, 0
    int 0x10
    jmp prnt
.pd:
    ret

lmsg db 'XC Loader Stage 1...', 13, 10, 0
smsg db 'Jumping to Stage 2...', 13, 10, 0
rtmsg db 'Retrying...', 13, 10, 0
emsg db 'Disk error! System halted.', 13, 10, 0
boot_drive db 0

times 510-($-$$) db 0
dw 0xaa55