#include "../include/text.h"
#include "../include/shell.h"
#include "startup.c"
#include "cmd.c"
#include "elf.c"

void kernel_main(void) {
    startup();

    asm volatile("sti");
    while (1) asm volatile("hlt");
}
