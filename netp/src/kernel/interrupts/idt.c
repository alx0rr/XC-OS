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
extern void isr9();
extern void isr10();
extern void isr11();
extern void isr12();
extern void isr13();
extern void isr14();
extern void isr15();
extern void isr16();
extern void isr17();
extern void isr18();
extern void isr19();
extern void isr20();
extern void isr21();
extern void isr22();
extern void isr23();
extern void isr24();
extern void isr25();
extern void isr26();
extern void isr27();
extern void isr28();
extern void isr29();
extern void isr30();
extern void isr31();
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
typedef void (*irq_handler_t)(registers_t);
static irq_handler_t irq_handlers[16] = {0};
static void idt_set_gate(uint8_t n, uint32_t handler, uint16_t sel, uint8_t flags) {
    idt[n].offset_low = handler & 0xFFFF;
    idt[n].offset_high = (handler >> 16) & 0xFFFF;
    idt[n].selector = sel;
    idt[n].zero = 0;
    idt[n].flags = flags;
}
void idt_register_irq_handler(uint8_t irq_no, irq_handler_t handler) {
    if (irq_no < 16) {
        irq_handlers[irq_no] = handler;
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
    outb(0x21, 0xFB);
    outb(0xA1, 0xFF);
}
void idt_init() {
    idt_ptr.limit = sizeof(idt) - 1;
    idt_ptr.base = (uint32_t)&idt;
    for (int i = 0; i < IDT_ENTRIES; i++) {
        idt_set_gate(i, 0, 0, 0);
    }
    idt_set_gate(0, (uint32_t)isr0, 0x08, 0x8E);
    idt_set_gate(1, (uint32_t)isr1, 0x08, 0x8E);
    idt_set_gate(2, (uint32_t)isr2, 0x08, 0x8E);
    idt_set_gate(3, (uint32_t)isr3, 0x08, 0x8E);
    idt_set_gate(4, (uint32_t)isr4, 0x08, 0x8E);
    idt_set_gate(5, (uint32_t)isr5, 0x08, 0x8E);
    idt_set_gate(6, (uint32_t)isr6, 0x08, 0x8E);
    idt_set_gate(7, (uint32_t)isr7, 0x08, 0x8E);
    idt_set_gate(8, (uint32_t)isr8, 0x08, 0x8E);
    idt_set_gate(9, (uint32_t)isr9, 0x08, 0x8E);
    idt_set_gate(10, (uint32_t)isr10, 0x08, 0x8E);
    idt_set_gate(11, (uint32_t)isr11, 0x08, 0x8E);
    idt_set_gate(12, (uint32_t)isr12, 0x08, 0x8E);
    idt_set_gate(13, (uint32_t)isr13, 0x08, 0x8E);
    idt_set_gate(14, (uint32_t)isr14, 0x08, 0x8E);
    idt_set_gate(15, (uint32_t)isr15, 0x08, 0x8E);
    idt_set_gate(16, (uint32_t)isr16, 0x08, 0x8E);
    idt_set_gate(17, (uint32_t)isr17, 0x08, 0x8E);
    idt_set_gate(18, (uint32_t)isr18, 0x08, 0x8E);
    idt_set_gate(19, (uint32_t)isr19, 0x08, 0x8E);
    idt_set_gate(20, (uint32_t)isr20, 0x08, 0x8E);
    idt_set_gate(21, (uint32_t)isr21, 0x08, 0x8E);
    idt_set_gate(22, (uint32_t)isr22, 0x08, 0x8E);
    idt_set_gate(23, (uint32_t)isr23, 0x08, 0x8E);
    idt_set_gate(24, (uint32_t)isr24, 0x08, 0x8E);
    idt_set_gate(25, (uint32_t)isr25, 0x08, 0x8E);
    idt_set_gate(26, (uint32_t)isr26, 0x08, 0x8E);
    idt_set_gate(27, (uint32_t)isr27, 0x08, 0x8E);
    idt_set_gate(28, (uint32_t)isr28, 0x08, 0x8E);
    idt_set_gate(29, (uint32_t)isr29, 0x08, 0x8E);
    idt_set_gate(30, (uint32_t)isr30, 0x08, 0x8E);
    idt_set_gate(31, (uint32_t)isr31, 0x08, 0x8E);
    idt_set_gate(32, (uint32_t)irq0, 0x08, 0x8E);
    idt_set_gate(33, (uint32_t)irq1, 0x08, 0x8E);
    idt_set_gate(34, (uint32_t)irq2, 0x08, 0x8E);
    idt_set_gate(35, (uint32_t)irq3, 0x08, 0x8E);
    idt_set_gate(36, (uint32_t)irq4, 0x08, 0x8E);
    idt_set_gate(37, (uint32_t)irq5, 0x08, 0x8E);
    idt_set_gate(38, (uint32_t)irq6, 0x08, 0x8E);
    idt_set_gate(39, (uint32_t)irq7, 0x08, 0x8E);
    idt_set_gate(40, (uint32_t)irq8, 0x08, 0x8E);
    idt_set_gate(41, (uint32_t)irq9, 0x08, 0x8E);
    idt_set_gate(42, (uint32_t)irq10, 0x08, 0x8E);
    idt_set_gate(43, (uint32_t)irq11, 0x08, 0x8E);
    idt_set_gate(44, (uint32_t)irq12, 0x08, 0x8E);
    idt_set_gate(45, (uint32_t)irq13, 0x08, 0x8E);
    idt_set_gate(46, (uint32_t)irq14, 0x08, 0x8E);
    idt_set_gate(47, (uint32_t)irq15, 0x08, 0x8E);
    asm volatile("lidt %0" : : "m"(idt_ptr));
    asm volatile("sti");
}
void isr_handler(registers_t *regs) {
    if (regs->int_no < 32) {
        const char* exceptions[] = {
            "Division By Zero", "Debug", "NMI", "Breakpoint",
            "Overflow", "Bound Range", "Invalid Opcode", "Device Not Available",
            "Double Fault", "Coprocessor", "Invalid TSS", "Segment Not Present",
            "Stack Fault", "General Protection", "Page Fault", "Reserved",
            "x87 FPU", "Alignment Check", "Machine Check", "SIMD FP"
        };
        clear();
        printf("\n{FG(255,0,0)}CPU EXCEPTION: %s (%u)\n", 
               (regs->int_no < 20) ? exceptions[regs->int_no] : "Unknown", regs->int_no);
        printf("{FG(255,255,0)}EIP=0x%x CS=0x%x EFLAGS=0x%x\n", regs->eip, regs->cs, regs->eflags);
        printf("EAX=0x%x EBX=0x%x ECX=0x%x EDX=0x%x\n", regs->eax, regs->ebx, regs->ecx, regs->edx);
        printf("ESP=0x%x EBP=0x%x ESI=0x%x EDI=0x%x\n", regs->esp, regs->ebp, regs->esi, regs->edi);
        printf("DS=0x%x SS=0x%x ERR=0x%x\n", regs->ds, regs->ss, regs->err_code);
        if (regs->int_no == 14) {
            uint32_t cr2;
            asm volatile("mov %%cr2, %0" : "=r"(cr2));
            printf("{FG(255,100,100)}Page Fault at: 0x%x ", cr2);
            if (!(regs->err_code & 1)) printf("(not present) ");
            if (regs->err_code & 2) printf("(write) "); else printf("(read) ");
            if (regs->err_code & 4) printf("(user)"); else printf("(kernel)");
            printf("\n");
        }
        printf("{FG(255,0,0)}System halted.\n");
        asm volatile("cli; hlt");
    } else if (regs->int_no >= 32 && regs->int_no < 48) {
        uint8_t irq_no = regs->int_no - 32;
        if (irq_handlers[irq_no]) {
            irq_handlers[irq_no](*regs);
        }
        if (regs->int_no >= 40) {
            outb(0xA0, 0x20);
        }
        outb(0x20, 0x20);
    }
}
