#include "../include/ring3.h"
#include "../include/memory/vmm.h"
#include "../include/memory/pmm.h"
#include "../include/interrupts/idt.h"
#include "../include/text.h"
#include "../lib/string.h"

extern uint8_t r3test_code[];
extern uint8_t r3test_code_end[];

#define USER_CODE_VIRT  0x40000000
#define USER_STACK_VIRT 0x40001000
#define USER_STACK_TOP  (USER_STACK_VIRT + PAGE_SIZE)

static void syscall_r3test(registers_t *regs) {
    printf("{FG(0,255,0)}[Ring3]{FG(255,255,255)} syscall eax=%u cs=0x%x (CPL=%u)\n",
           regs->eax, regs->cs, regs->cs & 3);
}

void ring3_run_test(void) {
    syscall_register(1, syscall_r3test);

    void *code_page  = vmm_alloc_pages(1, PAGE_USER | PAGE_WRITE);
    void *stack_page = vmm_alloc_pages(1, PAGE_USER | PAGE_WRITE);

    if (!code_page || !stack_page) {
        printf("{FG(255,0,0)}ring3: out of memory\n");
        if (code_page)  vmm_free_pages(code_page,  1);
        if (stack_page) vmm_free_pages(stack_page, 1);
        return;
    }

    uint32_t code_size = (uint32_t)(r3test_code_end - r3test_code);
    memcpy(code_page, r3test_code, code_size);

    uint32_t code_virt  = (uint32_t)code_page;
    uint32_t stack_virt = (uint32_t)stack_page + PAGE_SIZE;

    printf("{FG(255,255,0)}[Ring3]{FG(255,255,255)} code=0x%x stack=0x%x size=%u\n",
           code_virt, stack_virt, code_size);
    printf("{FG(255,255,0)}[Ring3]{FG(255,255,255)} entering CPL=3...\n");

    ring3_enter(code_virt, stack_virt);
}
