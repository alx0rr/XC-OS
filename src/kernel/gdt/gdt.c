#include "../../include/gdt/gdt.h"
#include "../../lib/string.h"

static gdt_entry_t gdt[GDT_ENTRIES];
static gdt_ptr_t   gdt_ptr;
static tss_t       tss;

static void set_entry(int i, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[i].base_lo  = base & 0xFFFF;
    gdt[i].base_mid = (base >> 16) & 0xFF;
    gdt[i].base_hi  = (base >> 24) & 0xFF;
    gdt[i].limit_lo = limit & 0xFFFF;
    gdt[i].gran     = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[i].access   = access;
}

extern void gdt_flush(uint32_t);
extern void tss_flush();

void gdt_init() {
    gdt_ptr.limit = sizeof(gdt) - 1;
    gdt_ptr.base  = (uint32_t)&gdt;

    set_entry(0, 0, 0,          0,    0);
    set_entry(1, 0, 0xFFFFF, 0x9A, 0xCF);
    set_entry(2, 0, 0xFFFFF, 0x92, 0xCF);
    set_entry(3, 0, 0xFFFFF, 0xFA, 0xCF);
    set_entry(4, 0, 0xFFFFF, 0xF2, 0xCF);

    uint32_t tss_base  = (uint32_t)&tss;
    uint32_t tss_limit = sizeof(tss) - 1;
    set_entry(5, tss_base, tss_limit, 0x89, 0x40);

    memset(&tss, 0, sizeof(tss));
    tss.ss0  = KDATA_SEL;
    tss.esp0 = 0;
    tss.iomap_base = sizeof(tss);

    gdt_flush((uint32_t)&gdt_ptr);
    tss_flush();
}

void tss_set_kernel_stack(uint32_t esp0) {
    tss.esp0 = esp0;
}

tss_t* gdt_get_tss() {
    return &tss;
}
