#include "../include/text.h"
#include "../include/kshell/kshell.h"
#include "startup.c"

void kernel_main(void) {
    startup();

    printf("{FG(0,255,0)}Welcome to {FG(194,122,255)}XC-OS {FG(0,255,0)}\n");

    kshell_launch();
}
