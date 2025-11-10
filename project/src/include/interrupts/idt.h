#ifndef IDT_H
#define IDT_H
#include <stdint.h>

#define IDT_ENTRIES 256

typedef struct {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t zero;
    uint8_t flags;
    uint16_t offset_high;
} __attribute__((packed)) idt_entry_t;

typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) idt_ptr_t;

typedef struct {
    uint32_t ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags, useresp, ss;
} __attribute__((packed)) registers_t;

#define IDT_FLAG_PRESENT (1 << 7)
#define IDT_FLAG_DPL_0 (0 << 5)
#define IDT_FLAG_DPL_3 (3 << 5)
#define IDT_FLAG_INTERRUPT (0xE << 0)
#define IDT_FLAG_TRAP (0xF << 0)

// Коды исключений
#define INT_DIVIDE_BY_ZERO 0
#define INT_DEBUG 1
#define INT_NMI 2
#define INT_BREAKPOINT 3
#define INT_OVERFLOW 4
#define INT_BOUND_RANGE 5
#define INT_INVALID_OPCODE 6
#define INT_DEVICE_NOT_AVAILABLE 7
#define INT_DOUBLE_FAULT 8
#define INT_INVALID_TSS 10
#define INT_SEGMENT_NOT_PRESENT 11
#define INT_STACK_FAULT 12
#define INT_GENERAL_PROTECTION 13
#define INT_PAGE_FAULT 14
#define INT_FLOATING_POINT 16
#define INT_ALIGNMENT_CHECK 17
#define INT_MACHINE_CHECK 18
#define INT_SIMD_EXCEPTION 19

void idt_init();
void pic_init();
void idt_register_exception_handler(uint8_t int_no, void (*handler)(registers_t));
void idt_register_irq_handler(uint8_t irq_no, void (*handler)(registers_t));

#endif