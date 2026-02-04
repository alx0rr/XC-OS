#include "../include/text.h"
#include "../include/scheduler/scheduler.h"
#include "startup.c"
#include "cmd.c"



void kernel_main(void) {
    startup();

    printf("{FG(0,255,0)}Welcome to {FG(194,122,255)}XC-OS v0.3{FG(0,255,0)}\n");
    printf("\nType '{FG(255,255,0)}help{FG(255,255,255)}' for available commands\n\n");

    /*
    if (get_scheduler_status()){
        task_create("System CMD", cmd, 0);
        scheduler_start();
    } else {
        cmd();
    }
    */

    cmd();
}