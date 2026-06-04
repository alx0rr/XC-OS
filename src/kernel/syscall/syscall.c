#include "../../include/syscall/syscall.h"
#include "../../include/interrupts/idt.h"
#include "../../include/scheduler/sched.h"
#include "../../include/text.h"

static syscall_fn_t tbl[SYSCALL_MAX];

extern void syscall_stub();

static uint32_t sys_exit(uint32_t code, uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    (void)a; (void)b; (void)c; (void)d;
    task_exit_by_code(code);
    while (1) asm volatile("hlt");
    return 0;
}

static uint32_t sys_write(uint32_t fd, uint32_t buf, uint32_t len, uint32_t a, uint32_t b) {
    (void)a; (void)b;
    if (fd != 1 && fd != 2) return (uint32_t)-1;
    const char *s = (const char *)buf;
    for (uint32_t i = 0; i < len; i++) {
        char tmp[2] = {s[i], 0};
        printf("%s", tmp);
    }
    return len;
}

static uint32_t sys_getpid(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e) {
    (void)a; (void)b; (void)c; (void)d; (void)e;
    extern uint32_t sched_current_pid();
    return sched_current_pid();
}

static uint32_t sys_yield(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e) {
    (void)a; (void)b; (void)c; (void)d; (void)e;
    extern void task_yield();
    task_yield();
    return 0;
}

static uint32_t sys_sleep(uint32_t ms, uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    (void)a; (void)b; (void)c; (void)d;
    extern void task_sleep(uint32_t);
    task_sleep(ms);
    return 0;
}

void syscall_register(uint32_t n, syscall_fn_t fn) {
    if (n < SYSCALL_MAX) tbl[n] = fn;
}

void syscall_handler(registers_t *regs) {
    uint32_t n = regs->eax;
    if (n >= SYSCALL_MAX || !tbl[n]) {
        regs->eax = (uint32_t)-1;
        return;
    }
    regs->eax = tbl[n](regs->ebx, regs->ecx, regs->edx, regs->esi, regs->edi);
}

void syscall_init() {
    for (int i = 0; i < SYSCALL_MAX; i++) tbl[i] = 0;
    tbl[SYS_EXIT]   = sys_exit;
    tbl[SYS_WRITE]  = sys_write;
    tbl[SYS_GETPID] = sys_getpid;
    tbl[SYS_YIELD]  = sys_yield;
    tbl[SYS_SLEEP]  = sys_sleep;

    extern void idt_set_gate_dpl3(uint8_t, uint32_t, uint16_t, uint8_t);
    extern void syscall_stub_asm(void);
    idt_set_gate_dpl3(SYSCALL_INT, (uint32_t)syscall_stub_asm, 0x08,
                      0x80 | 0x60 | 0x0F);
}
