#ifndef GDT_H
#define GDT_H
#include <stdint.h>

#define GDT_ENTRIES 6

#define SEG_NULL        0x00
#define SEG_KCODE       0x08
#define SEG_KDATA       0x10
#define SEG_UCODE       0x18
#define SEG_UDATA       0x20
#define SEG_TSS         0x28

#define RPL0 0
#define RPL3 3

#define KCODE_SEL (SEG_KCODE | RPL0)
#define KDATA_SEL (SEG_KDATA | RPL0)
#define UCODE_SEL (SEG_UCODE | RPL3)
#define UDATA_SEL (SEG_UDATA | RPL3)
#define TSS_SEL   (SEG_TSS   | RPL0)

typedef struct {
    uint16_t limit_lo;
    uint16_t base_lo;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  gran;
    uint8_t  base_hi;
} __attribute__((packed)) gdt_entry_t;

typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) gdt_ptr_t;

typedef struct {
    uint32_t prev_tss;
    uint32_t esp0;
    uint32_t ss0;
    uint32_t esp1, ss1;
    uint32_t esp2, ss2;
    uint32_t cr3;
    uint32_t eip, eflags;
    uint32_t eax, ecx, edx, ebx;
    uint32_t esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap_base;
} __attribute__((packed)) tss_t;

void gdt_init();
void tss_set_kernel_stack(uint32_t esp0);
tss_t* gdt_get_tss();

#endif
