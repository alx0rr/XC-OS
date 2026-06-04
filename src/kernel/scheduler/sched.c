#include "../../include/scheduler/sched.h"
#include "../../include/proc/proc.h"
#include "../../include/gdt/gdt.h"
#include "../../include/memory/vmm.h"
#include "../../include/interrupts/idt.h"
#include "../../lib/string.h"

#define TIME_SLICE 20

static proc_t *queue[PROC_MAX];
static int     q_head, q_tail, q_cnt;
static proc_t *cur;
static uint32_t ticks_ms;
static volatile uint8_t need_resched;

void sched_init() {
    q_head = q_tail = q_cnt = 0;
    cur = 0;
    ticks_ms = 0;
    need_resched = 0;
}

void sched_add(proc_t *p) {
    if (!p || q_cnt >= PROC_MAX) return;
    queue[q_tail] = p;
    q_tail = (q_tail + 1) % PROC_MAX;
    q_cnt++;
}

void sched_remove(uint32_t pid) {
    for (int i = 0; i < q_cnt; i++) {
        int idx = (q_head + i) % PROC_MAX;
        if (queue[idx] && queue[idx]->pid == pid) {
            queue[idx] = 0;
        }
    }
}

static proc_t* next_ready() {
    for (int i = 0; i < q_cnt; i++) {
        int idx = (q_head + i) % PROC_MAX;
        proc_t *p = queue[idx];
        if (p && p->state == PS_READY) {
            q_head = (idx + 1) % PROC_MAX;
            return p;
        }
    }
    return 0;
}

static void switch_to(proc_t *nxt, registers_t *regs) {
    if (!nxt) return;

    if (cur && cur->state == PS_RUNNING) {
        cur->ctx.eip    = regs->eip;
        cur->ctx.esp    = regs->useresp;
        cur->ctx.ebp    = regs->ebp;
        cur->ctx.eflags = regs->eflags;
        cur->ctx.eax    = regs->eax;
        cur->ctx.ebx    = regs->ebx;
        cur->ctx.ecx    = regs->ecx;
        cur->ctx.edx    = regs->edx;
        cur->ctx.esi    = regs->esi;
        cur->ctx.edi    = regs->edi;
        cur->state = PS_READY;
    }

    cur = nxt;
    cur->state = PS_RUNNING;

    tss_set_kernel_stack(cur->kstack_top);

    if (cur->vm)
        vmm_switch_space(cur->vm);
    else
        vmm_flush_tlb();

    regs->eip    = cur->ctx.eip;
    regs->eflags = cur->ctx.eflags | 0x200;
    regs->eax    = cur->ctx.eax;
    regs->ebx    = cur->ctx.ebx;
    regs->ecx    = cur->ctx.ecx;
    regs->edx    = cur->ctx.edx;
    regs->esi    = cur->ctx.esi;
    regs->edi    = cur->ctx.edi;
    regs->ebp    = cur->ctx.ebp;

    if (cur->ctx.cs == UCODE_SEL) {
        regs->cs      = UCODE_SEL;
        regs->ss      = UDATA_SEL;
        regs->useresp = cur->ctx.esp;
        regs->ds      = UDATA_SEL;
    } else {
        regs->cs      = KCODE_SEL;
        regs->ss      = KDATA_SEL;
        regs->useresp = cur->ctx.esp;
        regs->ds      = KDATA_SEL;
    }
}

void sched_tick(registers_t *regs) {
    ticks_ms++;

    for (int i = 0; i < q_cnt; i++) {
        int idx = (q_head + i) % PROC_MAX;
        proc_t *p = queue[idx];
        if (p && p->state == PS_BLOCKED && p->ticks > 0) {
            p->ticks--;
            if (p->ticks == 0) p->state = PS_READY;
        }
    }

    proc_t *n = next_ready();

    if (!cur) {
        if (n) switch_to(n, regs);
        return;
    }

    if (cur->state == PS_ZOMBIE || need_resched) {
        need_resched = 0;
        cur->ticks = 0;
        proc_t *dying = (cur->state == PS_ZOMBIE) ? cur : 0;
        if (n) switch_to(n, regs);
        else cur = 0;
        if (dying) {
            sched_remove(dying->pid);
            proc_free(dying);
        }
        return;
    }

    cur->ticks++;
    if (cur->ticks >= TIME_SLICE) {
        cur->ticks = 0;
        if (n) switch_to(n, regs);
    }
}

proc_t* sched_current()      { return cur; }
uint32_t sched_current_pid() { return cur ? cur->pid : 0; }
uint8_t sched_need_resched() { return need_resched; }
void sched_resched_clear()   { need_resched = 0; }

void task_yield() {
    need_resched = 1;
}

void task_sleep(uint32_t ms) {
    if (!cur) return;
    cur->ticks  = ms;
    cur->state  = PS_BLOCKED;
    need_resched = 1;
}

void task_exit_by_code(uint32_t code) {
    if (!cur) return;
    cur->exit_code = code;
    cur->state     = PS_ZOMBIE;
    need_resched   = 1;
}
