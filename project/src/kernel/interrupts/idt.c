#include "../../include/interrupts/idt.h"
#include "../../lib/io.h"
#include "../../include/text.h"

static idt_entry_t idt[IDT_ENTRIES];
static idt_ptr_t idt_ptr;

extern void isr_common();
extern void irq_common();

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
    
    for (int i = 0; i < 32; i++) {
        idt_set_gate(i, (uint32_t)isr_common, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_INTERRUPT);
    }
    for (int i = 32; i < 48; i++) {
        idt_set_gate(i, (uint32_t)irq_common, 0x08, IDT_FLAG_PRESENT | IDT_FLAG_INTERRUPT);
    }
    
    asm volatile("lidt %0" : : "m"(idt_ptr));
    asm volatile("sti");
}

void isr_handler(registers_t regs) {
    if (regs.int_no < 32) {
        exception_handlers[regs.int_no](regs);
    } else if (regs.int_no >= 32 && regs.int_no < 48) {
        irq_handlers[regs.int_no - 32](regs);
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