#include "../../include/proc/proc.h"
#include "../../include/memory/pmm.h"
#include "../../include/memory/vmm.h"
#include "../../include/gdt/gdt.h"
#include "../../lib/string.h"

static proc_t pool[PROC_MAX];
static uint32_t next_pid = 1;

static proc_t* alloc_proc() {
    for (int i = 0; i < PROC_MAX; i++) {
        if (pool[i].state == PS_FREE) {
            memset(&pool[i], 0, sizeof(proc_t));
            pool[i].pid = next_pid++;
            return &pool[i];
        }
    }
    return 0;
}

proc_t* proc_create_kernel(const char *name, void (*fn)()) {
    proc_t *p = alloc_proc();
    if (!p) return 0;

    strncpy(p->name, name, PROC_NAME_LEN - 1);

    p->kstack = pmm_malloc(KSTACK_SIZE);
    if (!p->kstack) { p->state = PS_FREE; return 0; }
    memset(p->kstack, 0, KSTACK_SIZE);
    p->kstack_top = (uint32_t)p->kstack + KSTACK_SIZE;

    p->vm = 0;

    p->ctx.eip    = (uint32_t)fn;
    p->ctx.esp    = p->kstack_top - 4;
    p->ctx.ebp    = p->kstack_top;
    p->ctx.eflags = 0x202;
    p->ctx.cs     = KCODE_SEL;
    p->ctx.ds     = KDATA_SEL;
    p->ctx.ss     = KDATA_SEL;

    p->state = PS_READY;
    return p;
}

proc_t* proc_create_user(const char *name, void *code, uint32_t code_sz) {
    proc_t *p = alloc_proc();
    if (!p) return 0;

    strncpy(p->name, name, PROC_NAME_LEN - 1);

    p->kstack = pmm_malloc(KSTACK_SIZE);
    if (!p->kstack) { p->state = PS_FREE; return 0; }
    memset(p->kstack, 0, KSTACK_SIZE);
    p->kstack_top = (uint32_t)p->kstack + KSTACK_SIZE;

    p->vm = vmm_create_space();
    if (!p->vm) { pmm_free(p->kstack); p->state = PS_FREE; return 0; }

    uint32_t pages = (code_sz + 0xFFF) >> 12;
    for (uint32_t i = 0; i < pages; i++) {
        void *phys = pmm_malloc(0x1000);
        if (!phys) { vmm_destroy_space(p->vm); pmm_free(p->kstack); p->state = PS_FREE; return 0; }
        uint32_t dst = UCODE_BASE + i * 0x1000;
        vmm_map_page_in(p->vm->directory, dst, (uint32_t)phys,
                        PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
        uint32_t chunk = (i + 1) * 0x1000 > code_sz ? code_sz - i * 0x1000 : 0x1000;
        memcpy((void*)phys, (uint8_t*)code + i * 0x1000, chunk);
    }

    void *ustk_phys = pmm_malloc(USTACK_SIZE);
    if (!ustk_phys) { vmm_destroy_space(p->vm); pmm_free(p->kstack); p->state = PS_FREE; return 0; }
    uint32_t ustk_base = USTACK_TOP - USTACK_SIZE;
    for (uint32_t i = 0; i < USTACK_SIZE / 0x1000; i++) {
        vmm_map_page_in(p->vm->directory,
                        ustk_base + i * 0x1000,
                        (uint32_t)ustk_phys + i * 0x1000,
                        PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
    }

    p->ctx.eip    = UCODE_BASE;
    p->ctx.esp    = USTACK_TOP - 4;
    p->ctx.ebp    = USTACK_TOP;
    p->ctx.eflags = 0x202;
    p->ctx.cs     = UCODE_SEL;
    p->ctx.ds     = UDATA_SEL;
    p->ctx.ss     = UDATA_SEL;

    p->state = PS_READY;
    return p;
}

void proc_free(proc_t *p) {
    if (!p) return;
    if (p->kstack) pmm_free(p->kstack);
    if (p->vm)     vmm_destroy_space(p->vm);
    memset(p, 0, sizeof(proc_t));
}

proc_t* proc_get(uint32_t pid) {
    for (int i = 0; i < PROC_MAX; i++)
        if (pool[i].pid == pid && pool[i].state != PS_FREE)
            return &pool[i];
    return 0;
}
