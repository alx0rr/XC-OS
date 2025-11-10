#include "../include/text.h"
#include "../include/input/keyboard.h"
#include "../lib/io.h"
#include "../lib/string.h"

#include "demo.c"
#include "startup.c"

void kernel_main() {
    startup();

    printf("\nXC-OS kernel v0.09.11\n\n");
    printf("\nhelp - all commands\n");
    
    while (1) { 
        printf("{FG(194,122,255)}root#~ {FG(255,255,255)}");
        char* text = keyboard_input();
        printf("%c", '\n');
        if (strcmp(text, "clear") == 0) {
            clear();
        } else if (strcmp(text, "help") == 0) {
            help();
        } else if (strcmp(text, "mmap") == 0) {
            pmm_print_stats();
        } else if (strcmp(text, "memtest") == 0) {
            memory_stress_test();
        } else if (strcmp(text, "cpu") == 0) {
            cpu_print_info();
        } else if (strcmp(text, "time") == 0) {
            time_demo();
        } else if (strcmp(text, "random") == 0) {
            random_demo();
        } else if (strcmp(text, "reboot") == 0) {
            outb(0x64, 0xFE);
        }else if (strcmp(text, "test-div") == 0) {
            printf("{FG(255,255,0)}Triggering divide by zero...\n");
            asm volatile("mov $0, %eax; mov $0, %ecx; div %ecx");
        }else {
            printf("{FG(255,0,0)}Unknown command: %s\n", text);
        }
    }
}