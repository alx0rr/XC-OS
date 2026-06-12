#include "../include/ring3.h"
#include "../include/memory/vmm.h"
#include "../include/memory/pmm.h"
#include "../include/interrupts/idt.h"
#include "../include/cpu/tss.h"
#include "../include/text.h"
#include "../lib/string.h"

extern uint8_t r3test_code[];
extern uint8_t r3test_code_end[];

static vm_space_t* user_space = 0;

static void syscall_r3test(registers_t *regs) {
    printf("{FG(0,255,0)}[Ring3]{FG(255,255,255)} syscall eax=%u cs=0x%x (CPL=%u)\n",
           regs->eax, regs->cs, regs->cs & 3);
}

static uint32_t get_phys_from_space(vm_space_t* space, uint32_t virt) {
    uint32_t pd_index = virt >> 22;
    uint32_t pt_index = (virt >> 12) & 0x3FF;
    uint32_t pde = space->directory->entries[pd_index];
    if (!(pde & PAGE_PRESENT)) return 0;
    page_table_t* table = (page_table_t*)(pde & 0xFFFFF000);
    if (!(table->entries[pt_index] & PAGE_PRESENT)) return 0;
    return (table->entries[pt_index] & 0xFFFFF000) | (virt & 0xFFF);
}

void ring3_run_test(void) {
    syscall_register(1, syscall_r3test);

    void* kstack = vmm_alloc_pages(1, 0);
    if (!kstack) {
        printf("{FG(255,0,0)}ring3: kstack alloc failed\n");
        return;
    }
    uint32_t kstack_top = (uint32_t)kstack + PAGE_SIZE;

    user_space = vmm_create_space();
    if (!user_space) {
        printf("{FG(255,0,0)}ring3: vmm_create_space failed\n");
        vmm_free_pages(kstack, 1);
        return;
    }

    void* code_page  = vmm_alloc_pages_in_space(user_space, 1);
    void* stack_page = vmm_alloc_pages_in_space(user_space, 1);

    if (!code_page || !stack_page) {
        printf("{FG(255,0,0)}ring3: user alloc failed\n");
        vmm_destroy_space(user_space);
        vmm_free_pages(kstack, 1);
        user_space = 0;
        return;
    }

    uint32_t code_virt  = (uint32_t)code_page;
    uint32_t stack_virt = (uint32_t)stack_page + PAGE_SIZE;
    uint32_t code_size  = (uint32_t)(r3test_code_end - r3test_code);

    uint32_t code_phys = get_phys_from_space(user_space, code_virt);
    if (!code_phys) {
        printf("{FG(255,0,0)}ring3: phys resolve failed\n");
        vmm_destroy_space(user_space);
        vmm_free_pages(kstack, 1);
        user_space = 0;
        return;
    }
    memcpy((void*)code_phys, r3test_code, code_size);

    tss_set_kernel_stack(kstack_top);

    printf("{FG(255,255,0)}[Ring3]{FG(255,255,255)} code=0x%x stack=0x%x kstack=0x%x\n",
           code_virt, stack_virt, kstack_top);
    printf("{FG(255,255,0)}[Ring3]{FG(255,255,255)} entering CPL=3...\n");

    vmm_switch_space(user_space);
    ring3_enter(code_virt, stack_virt);
}
