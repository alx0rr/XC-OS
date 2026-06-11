#include "../../include/cpu/tss.h"
#include "../../include/text.h"

static tss_entry_t tss;
static gdt_entry_t gdt[6];
static gdt_ptr_t   gdt_ptr;

static void gdt_set(int i, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[i].base_low  = base & 0xFFFF;
    gdt[i].base_mid  = (base >> 16) & 0xFF;
    gdt[i].base_high = (base >> 24) & 0xFF;
    gdt[i].limit_low = limit & 0xFFFF;
    gdt[i].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[i].access    = access;
}

extern void gdt_flush(uint32_t);
extern void tss_flush(void);

void tss_init(uint32_t kernel_stack_top) {
    uint32_t tss_base  = (uint32_t)&tss;
    uint32_t tss_limit = sizeof(tss_entry_t) - 1;

    gdt_set(0, 0, 0,          0x00, 0x00);
    gdt_set(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);
    gdt_set(2, 0, 0xFFFFFFFF, 0x92, 0xCF);
    gdt_set(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);
    gdt_set(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);
    gdt_set(5, tss_base, tss_limit, 0x89, 0x00);

    gdt_ptr.limit = sizeof(gdt) - 1;
    gdt_ptr.base  = (uint32_t)&gdt;

    __builtin_memset(&tss, 0, sizeof(tss_entry_t));
    tss.ss0  = GDT_SEL_KDATA;
    tss.esp0 = kernel_stack_top;
    tss.cs   = GDT_SEL_KCODE;
    tss.ss   = GDT_SEL_KDATA;
    tss.ds   = GDT_SEL_KDATA;
    tss.es   = GDT_SEL_KDATA;
    tss.fs   = GDT_SEL_KDATA;
    tss.gs   = GDT_SEL_KDATA;
    tss.iomap_base = sizeof(tss_entry_t);

    gdt_flush((uint32_t)&gdt_ptr);
    tss_flush();

    printf("{FG(0,255,0)}[OK]{FG(255,255,255)} TSS + GDT (user segments) initialized\n");
}

void tss_set_kernel_stack(uint32_t esp0) {
    tss.esp0 = esp0;
}
