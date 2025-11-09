VBE_INFO_ADDR equ 0x5000

; Инициализация VBE режима
; Вход: cx - режим VBE
; Выход: ax - результат (0x004F если успешно)
; Использует: es, di
init_vbe_mode:
    push es
    push di
    push cx
    
    ; Получаем информацию о режиме
    xor ax, ax
    mov es, ax
    mov di, VBE_INFO_ADDR       
    mov ax, 0x4F01             
    pop cx
    int 0x10
    
    cmp ax, 0x004F
    jne .vbe_error
    
    ; Устанавливаем режим
    mov ax, 0x4F02
    mov bx, cx
    or bx, 0x4000              ; Установить бит 14 (линейный framebuffer)
    int 0x10
    
    cmp ax, 0x004F
    jne .vbe_error
    
    pop di
    pop es
    ret
    
.vbe_error:
    pop di
    pop es
    ret