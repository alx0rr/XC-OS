#include "../include/scheduler/scheduler.h"
#include "../include/text.h"
#include "../lib/time.h"

void demo_task_1() {
    uint32_t counter = 0;
    while (1) {
        printf("{FG(255,100,100)}[Task 1] Counter: %u\n", counter++);
        task_sleep(1000);
        
        if (counter >= 5) {
            printf("{FG(255,100,100)}[Task 1] Exiting after 5 iterations\n");
            task_exit();
        }
    }
}

void demo_task_2() {
    uint32_t counter = 0;
    while (1) {
        printf("{FG(100,255,100)}[Task 2] Counter: %u\n", counter++);
        task_sleep(1500);
        
        if (counter >= 5) {
            printf("{FG(100,255,100)}[Task 2] Exiting after 5 iterations\n");
            task_exit();
        }
    }
}

void demo_task_3() {
    uint32_t counter = 0;
    while (1) {
        printf("{FG(100,100,255)}[Task 3] Counter: %u\n", counter++);
        task_sleep(2000);
        
        if (counter >= 5) {
            printf("{FG(100,100,255)}[Task 3] Exiting after 5 iterations\n");
            task_exit();
        }
    }
}

void demo_multitasking() {
    printf("{FG(255,255,0)}=== Multitasking Demo ===\n\n");
    
    printf("{FG(0,255,255)}Creating tasks...\n");
    task_create("Demo Task 1", demo_task_1, 0);
    task_create("Demo Task 2", demo_task_2, 0);
    task_create("Demo Task 3", demo_task_3, 0);
    
    printf("{FG(0,255,0)}Tasks created! Starting scheduler...\n\n");
    
    scheduler_start();
    
    printf("{FG(255,255,0)}Scheduler is now running!\n");
    printf("{FG(0,255,255)}Watch the tasks execute in parallel...\n\n");
}
