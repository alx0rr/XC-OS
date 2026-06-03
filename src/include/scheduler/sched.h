#ifndef SCHED_H
#define SCHED_H
#include <stdint.h>
#include "../proc/proc.h"
#include "../interrupts/idt.h"

void sched_init();
void sched_add(proc_t *p);
void sched_remove(uint32_t pid);
void sched_tick(registers_t *regs);

proc_t*  sched_current();
uint32_t sched_current_pid();

void task_yield();
void task_sleep(uint32_t ms);
void task_exit_by_code(uint32_t code);

#endif
