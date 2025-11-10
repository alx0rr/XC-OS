[bits 32]

extern isr_handler

global isr_common
isr_common:
    cli
    
    push ds
    push es
    push fs
    push gs
    
    push eax
    push ecx
    push edx
    push ebx
    push ebp
    push esi
    push edi
    
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    mov eax, esp
    push eax
    call isr_handler
    pop eax
    
    pop edi
    pop esi
    pop ebp
    pop ebx
    pop edx
    pop ecx
    pop eax
    
    pop gs
    pop fs
    pop es
    pop ds
    
    add esp, 8
    sti
    iret

global irq_common
irq_common:
    cli
    
    push ds
    push es
    push fs
    push gs
    
    push eax
    push ecx
    push edx
    push ebx
    push ebp
    push esi
    push edi
    
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    mov eax, esp
    push eax
    call isr_handler
    pop eax
    
    pop edi
    pop esi
    pop ebp
    pop ebx
    pop edx
    pop ecx
    pop eax
    
    pop gs
    pop fs
    pop es
    pop ds
    
    add esp, 8
    sti
    iret

; ISR stubs для исключений (0-31)
global isr0
isr0:
    push 0
    push 0
    jmp isr_common

global isr1
isr1:
    push 0
    push 1
    jmp isr_common

global isr2
isr2:
    push 0
    push 2
    jmp isr_common

global isr3
isr3:
    push 0
    push 3
    jmp isr_common

global isr4
isr4:
    push 0
    push 4
    jmp isr_common

global isr5
isr5:
    push 0
    push 5
    jmp isr_common

global isr6
isr6:
    push 0
    push 6
    jmp isr_common

global isr7
isr7:
    push 0
    push 7
    jmp isr_common

global isr8
isr8:
    push 8
    jmp isr_common

global isr10
isr10:
    push 10
    jmp isr_common

global isr11
isr11:
    push 11
    jmp isr_common

global isr12
isr12:
    push 12
    jmp isr_common

global isr13
isr13:
    push 13
    jmp isr_common

global isr14
isr14:
    push 14
    jmp isr_common

global isr16
isr16:
    push 0
    push 16
    jmp isr_common

global isr17
isr17:
    push 17
    jmp isr_common

global isr18
isr18:
    push 0
    push 18
    jmp isr_common

global isr19
isr19:
    push 0
    push 19
    jmp isr_common

; IRQ stubs (0-15) -> INT (32-47)
global irq0
irq0:
    push 0
    push 32
    jmp irq_common

global irq1
irq1:
    push 0
    push 33
    jmp irq_common

global irq2
irq2:
    push 0
    push 34
    jmp irq_common

global irq3
irq3:
    push 0
    push 35
    jmp irq_common

global irq4
irq4:
    push 0
    push 36
    jmp irq_common

global irq5
irq5:
    push 0
    push 37
    jmp irq_common

global irq6
irq6:
    push 0
    push 38
    jmp irq_common

global irq7
irq7:
    push 0
    push 39
    jmp irq_common

global irq8
irq8:
    push 0
    push 40
    jmp irq_common

global irq9
irq9:
    push 0
    push 41
    jmp irq_common

global irq10
irq10:
    push 0
    push 42
    jmp irq_common

global irq11
irq11:
    push 0
    push 43
    jmp irq_common

global irq12
irq12:
    push 0
    push 44
    jmp irq_common

global irq13
irq13:
    push 0
    push 45
    jmp irq_common

global irq14
irq14:
    push 0
    push 46
    jmp irq_common

global irq15
irq15:
    push 0
    push 47
    jmp irq_common