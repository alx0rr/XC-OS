#include <stdint.h>
#include <stddef.h>
#include "../include/graphics/vbe.h"
#include "../include/text.h"
#include "../include/input/keyboard.h"
#include "../include/memory/pmm.h"
#include "../include/cpu/cpu.h"
#include "../lib/io.h"
#include "../lib/string.h"
#include "demo.c"

void memory_stress_test() {
    printf("{FG(100,200,255)}=== Memory Stress Test ===\n\n");
    
    void* ptrs[10];
    
    // Выделить память
    printf("{FG(255,255,0)}Allocating 10 blocks of 50 MB...\n");
    for (int i = 0; i < 10; i++) {
        ptrs[i] = pmm_malloc(50 * 1024 * 1024);
        if (!ptrs[i]) {
            printf("{FG(255,0,0)}Allocation failed at block %u\n", i);
            break;
        }
        printf("{FG(0,255,0)}Block %u allocated\n", i);
    }
    
    printf("\n");
    pmm_print_stats();
    
    printf("\n{FG(255,255,0)}Press any key to free...\n");
    keyboard_input();
    
    // Освободить память
    printf("{FG(255,255,0)}Freeing all blocks...\n\n");
    for (int i = 0; i < 10; i++) {
        if (ptrs[i]) {
            pmm_free(ptrs[i]);
        }
    }
    
    pmm_print_stats();
}

void kernel_main() {
    vbe_init();
    init_pmm();
    cpu_init();
    clear();
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
        } else {
            printf("{FG(255,0,0)}Unknown command: %s\n", text);
        }
    }
}