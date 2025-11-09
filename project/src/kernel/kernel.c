#include <stdint.h>
#include <stddef.h>

#include "../include/graphics/vbe.h"
#include "../include/text.h"
#include "../include/input/keyboard.h"
#include "../lib/io.h"
#include "../lib/string.h"
#include "demo.c"

void kernel_main() {
    vbe_init();
    clear();
    printf("\nXC-OS kernel v0.09.11\n\n");
    printf("\nhelp - all commands");
    memory();

    while (1) { 
        printf("{FG(194,122,255)}root#~ {FG(255,255,255)}");
        char* text = keyboard_input();
        printf("%c", '\n');

        if (strcmp(text, "clear") == 0) {
            clear();
        } else if (strcmp(text, "help") == 0) {
            help();
        } else if (strcmp(text, "time") == 0) {
            time_demo();
        } else if (strcmp(text, "random") == 0) {
            random_demo();
        } else if (strcmp(text, "image") == 0) {
            image_demo();
        } else if (strcmp(text, "reboot") == 0) {
            outb(0x64, 0xFE);
        } else {
            printf("Unknown command: %s\n", text);
        }
    }
}
