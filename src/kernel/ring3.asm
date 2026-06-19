[bits 32]

global ring3_enter
ring3_enter:
    mov eax, [esp+4]
    mov ecx, [esp+8]

    mov bx, 0x23
    mov ds, bx
    mov es, bx
    mov fs, bx
    mov gs, bx

    push dword 0x23
    push ecx
    pushfd
    pop edx
    or  edx, 0x200
    push edx
    push dword 0x1B
    push eax
    iret

global r3test_code
global r3test_code_end
r3test_code:
    mov eax, 1
    int 0x80
.loop:
    jmp .loop
r3test_code_end:
