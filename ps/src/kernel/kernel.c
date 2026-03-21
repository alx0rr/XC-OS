#include "../include/text.h"
#include "../include/scheduler/scheduler.h"
#include "startup.c"
#include "cmd.c"

static void idle_task(void) {
    while (1) {
        asm volatile("hlt");
    }
}

static void shell_task(void) {
    printf("{FG(0,255,0)}Welcome to {FG(194,122,255)}XC-OS :3{FG(0,255,0)}\n");
    printf("\nType '{FG(255,255,0)}help{FG(255,255,255)}' for available commands\n\n");
    cmd();
    task_exit();
}

void kernel_main(void) {
    startup();

    task_create("idle",  idle_task,  0);
    task_create("shell", shell_task, 0);

    scheduler_start();

    scheduler_jump_to_first(scheduler_get_current_task()->esp);
}

