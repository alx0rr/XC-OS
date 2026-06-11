#ifndef TSS_H
#define TSS_H

#include <stdint.h>

typedef struct {
    uint32_t prev_tss;
    uint32_t esp0;
    uint32_t ss0;
    uint32_t esp1;
    uint32_t ss1;
    uint32_t esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax, ecx, edx, ebx;
    uint32_t esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap_base;
} __attribute__((packed)) tss_entry_t;

typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed)) gdt_entry_t;

typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) gdt_ptr_t;

#define GDT_SEL_KCODE  0x08
#define GDT_SEL_KDATA  0x10
#define GDT_SEL_UCODE  0x1B
#define GDT_SEL_UDATA  0x23
#define GDT_SEL_TSS    0x28

void tss_init(uint32_t kernel_stack_top);
void tss_set_kernel_stack(uint32_t esp0);

extern uint8_t stack_top[];

#endif
