#include "../../include/scheduler/scheduler.h"
#include "../../include/memory/pmm.h"
#include "../../include/memory/vmm.h"
#include "../../include/text.h"
#include "../../include/timer/pit.h"
#include "../../lib/string.h"

static task_t* task_list_head = 0;
static task_t* current_task = 0;
static uint32_t next_task_id = 0;
static volatile uint32_t scheduler_enabled = 0;


uint32_t get_scheduler_status(){
    return scheduler_enabled;
}


static task_t* find_next_task() {
    if (!current_task) return task_list_head;
    
    task_t* task = current_task->next;
    if (!task) task = task_list_head;
    
    uint32_t iterations = 0;
    while (task && iterations < MAX_TASKS) {
        if (task->state == TASK_STATE_READY) {
            return task;
        }
        task = task->next;
        if (!task) task = task_list_head;
        iterations++;
    }
    
    return current_task;
}

void scheduler_tick(registers_t* regs) {
    if (!scheduler_enabled || !current_task) {
        return;
    }
    
    current_task->time_used++;
    
    if (current_task->time_used >= current_task->time_slice) {
        scheduler_switch_task(regs);
    }
}

void scheduler_init() {
    task_list_head = 0;
    current_task = 0;
    next_task_id = 0;
    scheduler_enabled = 0;
    
    printf("{FG(0,255,0)}[OK]{FG(255,255,255)}Scheduler initialized\n");
}

void scheduler_start() {
    if (!task_list_head) {
        printf("{FG(255,0,0)}Scheduler: No tasks to run\n");
        return;
    }
    
    current_task = task_list_head;
    current_task->state = TASK_STATE_RUNNING;
    scheduler_enabled = 1;
    
    printf("{FG(0,255,0)}Scheduler started with task: %s\n", current_task->name);
}

task_t* task_create(const char* name, void (*entry_point)(), uint32_t flags) {
    task_t* task = (task_t*)pmm_malloc(sizeof(task_t));
    if (!task) {
        printf("{FG(255,0,0)}Task creation failed: cannot allocate task struct\n");
        return 0;
    }
    
    memset(task, 0, sizeof(task_t));
    
    task->stack = pmm_malloc(TASK_STACK_SIZE);
    if (!task->stack) {
        printf("{FG(255,0,0)}Task creation failed: cannot allocate stack\n");
        pmm_free(task);
        return 0;
    }
    
    task->id = next_task_id++;
    strncpy(task->name, name, 63);
    task->name[63] = '\0';
    task->state = TASK_STATE_READY;
    task->stack_size = TASK_STACK_SIZE;
    task->time_slice = TIME_SLICE_MS;
    task->time_used = 0;
    
    uint32_t* stack_top = (uint32_t*)((uint32_t)task->stack + TASK_STACK_SIZE);
    stack_top--;
    
    *--stack_top = 0x202;
    *--stack_top = 0x08;
    *--stack_top = (uint32_t)entry_point;
    *--stack_top = 0;
    *--stack_top = 0;
    *--stack_top = 0;
    *--stack_top = 0;
    *--stack_top = 0;
    *--stack_top = 0;
    *--stack_top = 0;
    *--stack_top = 0;
    *--stack_top = 0;
    
    task->esp = (uint32_t)stack_top;
    task->ebp = task->esp;
    task->eip = (uint32_t)entry_point;
    task->eflags = 0x202;
    
    task->next = task_list_head;
    task_list_head = task;
    
    printf("{FG(0,255,0)}Task created: %s (ID: %u) at ESP=0x%x\n", task->name, task->id, task->esp);
    
    return task;
}

void task_exit() {
    if (!current_task) return;
    
    printf("{FG(255,255,0)}Task %s (ID: %u) exited\n", 
           current_task->name, current_task->id);
    
    current_task->state = TASK_STATE_TERMINATED;
    
    task_yield();
    
    while(1) {
        asm volatile("hlt");
    }
}

void task_yield() {
    if (!scheduler_enabled || !current_task) return;
    
    asm volatile("int $0x20");
}

void task_sleep(uint32_t ms) {
    if (!current_task) return;
    
    uint64_t target = pit_get_ticks() + ms;
    while (pit_get_ticks() < target) {
        task_yield();
    }
}

task_t* scheduler_get_current_task() {
    return current_task;
}

void scheduler_switch_task(registers_t* regs) {
    if (!scheduler_enabled || !current_task) return;
    
    task_t* old_task = current_task;
    task_t* new_task = find_next_task();
    
    if (old_task == new_task) {
        old_task->time_used = 0;
        return;
    }
    
    old_task->esp = regs->useresp;
    old_task->ebp = regs->ebp;
    old_task->eip = regs->eip;
    old_task->eflags = regs->eflags;
    
    if (old_task->state == TASK_STATE_RUNNING) {
        old_task->state = TASK_STATE_READY;
    }
    old_task->time_used = 0;
    
    current_task = new_task;
    current_task->state = TASK_STATE_RUNNING;
    current_task->time_used = 0;
    
    regs->useresp = new_task->esp;
    regs->ebp = new_task->ebp;
    regs->eip = new_task->eip;
    regs->eflags = new_task->eflags;
}

void scheduler_print_tasks() {
    printf("{FG(255,255,0)}Task List:\n");
    printf("{FG(0,255,255)}ID   Name                 State        Time\n");
    printf("================================================\n");
    
    task_t* task = task_list_head;
    uint32_t task_count = 0;
    while (task) {
        task_count++;
        const char* state_str;
        uint32_t color;
        
        switch(task->state) {
            case TASK_STATE_READY:
                state_str = "READY";
                color = 0x00FF00;
                break;
            case TASK_STATE_RUNNING:
                state_str = "RUNNING";
                color = 0xFFFF00;
                break;
            case TASK_STATE_BLOCKED:
                state_str = "BLOCKED";
                color = 0xFF8800;
                break;
            case TASK_STATE_TERMINATED:
                state_str = "TERMINATED";
                color = 0xFF0000;
                break;
            default:
                state_str = "UNKNOWN";
                color = 0x888888;
        }
        
        printf("{FG(%u,%u,%u)}%u    {FG(255,255,255)}%s    {FG(%u,%u,%u)}%s    {FG(255,255,255)}%u/%u ms\n",
               (color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF,
               task->id, 
               task->name,
               (color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF,
               state_str,
               task->time_used, task->time_slice);
        
        task = task->next;
    }
    
    printf("\n{FG(0,255,255)}Total tasks: {FG(255,255,0)}%u\n", task_count);
    printf("{FG(0,255,255)}Current task: {FG(255,255,0)}%s {FG(255,255,255)}(ID: %u)\n",
           current_task ? current_task->name : "none",
           current_task ? current_task->id : 0);
    printf("{FG(0,255,255)}Scheduler: {FG(255,255,0)}%s\n\n",
           scheduler_enabled ? "ENABLED" : "DISABLED");
}
