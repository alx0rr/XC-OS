#include "../../include/interrupts/idt.h"
#include "../../lib/io.h"
#include "../../include/text.h"

static idt_entry_t idt[IDT_ENTRIES];
static idt_ptr_t idt_ptr;

extern void isr0();
extern void isr1();
extern void isr2();
extern void isr3();
extern void isr4();
extern void isr5();
extern void isr6();
extern void isr7();
extern void isr8();
extern void isr10();
extern void isr11();
extern void isr12();
extern void isr13();
extern void isr14();
extern void isr16();
extern void isr17();
extern void isr18();
extern void isr19();

extern void irq0();
extern void irq1();
extern void irq2();
extern void irq3();
extern void irq4();
extern void irq5();
extern void irq6();
extern void irq7();
extern void irq8();
extern void irq9();
extern void irq10();
extern void irq11();
extern void irq12();
extern void irq13();
extern void irq14();
extern void irq15();

typedef void (*exception_handler_t)(registers_t);
static exception_handler_t exception_handlers[32];
static exception_handler_t irq_handlers[16];

static void default_exception_handler(registers_t regs) {
    printf("{FG(255,0,0)}=== EXCEPTION ===\n");
    printf("Int: %u\n", regs.int_no);
    printf("Error Code: 0x%x\n", regs.err_code);
    printf("EIP: 0x%x\n", regs.eip);
    printf("CS: 0x%x\n", regs.cs);
    printf("EFLAGS: 0x%x\n", regs.eflags);
    printf("System halted.{FG(255,255,255)}\n");
    
    asm volatile("cli; hlt");
}

static void default_irq_handler(registers_t regs) {
    printf("{FG(255,255,0)}IRQ %u triggered\n", regs.int_no - 32);
}

static void idt_set_gate(uint8_t n, uint32_t handler, uint16_t sel, uint8_t flags) {
    idt[n].offset_low = handler & 0xFFFF;
    idt[n].offset_high = (handler >> 16) & 0xFFFF;
    idt[n].selector = sel;
    idt[n].zero = 0;
    idt[n].flags = flags;
}

void idt_register_exception_handler(uint8_t int_no, exception_handler_t handler) {
    if (int_no < 32) {
        exception_handlers[int_no] = handler;
    }
}

void idt_register_irq_handler(uint8_t irq_no, exception_handler_t handler) {
    if (irq_no < 16) {
        irq_handlers[irq_no] = handler;
    }
}

void idt_init() {
    idt_ptr.limit = sizeof(idt) - 1;
    idt_ptr.base = (uint32_t)&idt;
    
    for (int i = 0; i < IDT_ENTRIES; i++) {
        idt[i].offset_low = 0;
        idt[i].offset_high = 0;
        idt[i].selector = 0;
        idt[i].zero = 0;
        idt[i].flags = 0;
    }
    
    for (int i = 0; i < 32; i++) {
        exception_handlers[i] = default_exception_handler;
    }
    for (int i = 0; i < 16; i++) {
        irq_handlers[i] = default_irq_handler;
    }
    
    idt_set_gate(0, (uint32_t)isr0, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_INTERRUPT);
    idt_set_gate(1, (uint32_t)isr1, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_INTERRUPT);
    idt_set_gate(2, (uint32_t)isr2, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_INTERRUPT);
    idt_set_gate(3, (uint32_t)isr3, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_INTERRUPT);
    idt_set_gate(4, (uint32_t)isr4, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_INTERRUPT);
    idt_set_gate(5, (uint32_t)isr5, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_INTERRUPT);
    idt_set_gate(6, (uint32_t)isr6, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_INTERRUPT);
    idt_set_gate(7, (uint32_t)isr7, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_INTERRUPT);
    idt_set_gate(8, (uint32_t)isr8, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_INTERRUPT);
    idt_set_gate(10, (uint32_t)isr10, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_INTERRUPT);
    idt_set_gate(11, (uint32_t)isr11, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_INTERRUPT);
    idt_set_gate(12, (uint32_t)isr12, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_INTERRUPT);
    idt_set_gate(13, (uint32_t)isr13, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_INTERRUPT);
    idt_set_gate(14, (uint32_t)isr14, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_INTERRUPT);
    idt_set_gate(16, (uint32_t)isr16, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_INTERRUPT);
    idt_set_gate(17, (uint32_t)isr17, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_INTERRUPT);
    idt_set_gate(18, (uint32_t)isr18, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_INTERRUPT);
    idt_set_gate(19, (uint32_t)isr19, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_INTERRUPT);
    
    idt_set_gate(32, (uint32_t)irq0, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_INTERRUPT);
    idt_set_gate(33, (uint32_t)irq1, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_INTERRUPT);
    idt_set_gate(34, (uint32_t)irq2, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_INTERRUPT);
    idt_set_gate(35, (uint32_t)irq3, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_INTERRUPT);
    idt_set_gate(36, (uint32_t)irq4, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_INTERRUPT);
    idt_set_gate(37, (uint32_t)irq5, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_INTERRUPT);
    idt_set_gate(38, (uint32_t)irq6, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_INTERRUPT);
    idt_set_gate(39, (uint32_t)irq7, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_INTERRUPT);
    idt_set_gate(40, (uint32_t)irq8, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_INTERRUPT);
    idt_set_gate(41, (uint32_t)irq9, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_INTERRUPT);
    idt_set_gate(42, (uint32_t)irq10, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_INTERRUPT);
    idt_set_gate(43, (uint32_t)irq11, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_INTERRUPT);
    idt_set_gate(44, (uint32_t)irq12, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_INTERRUPT);
    idt_set_gate(45, (uint32_t)irq13, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_INTERRUPT);
    idt_set_gate(46, (uint32_t)irq14, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_INTERRUPT);
    idt_set_gate(47, (uint32_t)irq15, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_INTERRUPT);
    
    asm volatile("lidt %0" : : "m"(idt_ptr));
    asm volatile("sti");
}

void isr_handler(registers_t regs) {
    if (regs.int_no < 32) {
        exception_handlers[regs.int_no](regs);
    } else if (regs.int_no >= 32 && regs.int_no < 48) {
        irq_handlers[regs.int_no - 32](regs);
        
        if (regs.int_no >= 40) {
            outb(0xA0, 0x20);
        }
        outb(0x20, 0x20);
    }
}

void pic_init() {
    outb(0x20, 0x11); 
    outb(0xA0, 0x11);
    
    outb(0x21, 0x20);
    outb(0xA1, 0x28);
    
    outb(0x21, 0x04); 
    outb(0xA1, 0x02);
    
    outb(0x21, 0x01); 
    outb(0xA1, 0x01);
    
    outb(0x21, 0xFF); 
    outb(0xA1, 0xFF);
}
