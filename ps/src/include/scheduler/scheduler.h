#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>
#include "../interrupts/idt.h"

#define MAX_TASKS 32
#define TASK_STACK_SIZE 8192
#define TIME_SLICE_MS 50

typedef enum {
    TASK_STATE_READY,
    TASK_STATE_RUNNING,
    TASK_STATE_BLOCKED,
    TASK_STATE_TERMINATED
} task_state_t;

typedef struct task {
    uint32_t id;
    char name[64];
    task_state_t state;

    uint32_t esp;
    uint32_t ebp;
    uint32_t eip;
    uint32_t eflags;

    uint32_t page_directory;

    void* stack;
    uint32_t stack_size;

    uint32_t time_slice;
    uint32_t time_used;

    struct task* next;
} task_t;

void scheduler_init(void);
void scheduler_start(void);

task_t* task_create(const char* name, void (*entry_point)(void), uint32_t flags);
void task_exit(void);
void task_yield(void);
void task_sleep(uint32_t ms);

task_t* scheduler_get_current_task(void);
void scheduler_switch_task(registers_t* regs);
void scheduler_tick(registers_t* regs);
void scheduler_print_tasks(void);

uint32_t get_scheduler_status(void);
uint32_t scheduler_get_next_esp(void);

extern void scheduler_jump_to_first(uint32_t esp);

#endif
