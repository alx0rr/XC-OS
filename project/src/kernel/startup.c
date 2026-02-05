/* /ᐠ - ˕ -マ */
#include "../include/graphics/vbe.h"
#include "../include/text.h"
#include "../include/memory/pmm.h"
#include "../include/memory/vmm.h"
#include "../include/cpu/cpu.h"
#include "../include/interrupts/idt.h"
#include "../include/storage/ata.h"
#include "../include/fs/xcfs.h"
#include "../include/input/keyboard.h"
#include "../include/scheduler/scheduler.h"
#include "../include/timer/pit.h"
#include "../include/graphics/framebuffer.h"
#include "../lib/types.h"


void startup() {
    vbe_init();
    text_init();
    clear();
    printf("{FG(0,255,0)}[OK]{FG(255,255,255)} VBE initialized\n");
    init_pmm();
    printf("{FG(0,255,0)}[OK]{FG(255,255,255)} Memory Manager initialized\n");
    vmm_init();
    printf("{FG(0,255,0)}[OK]{FG(255,255,255)} Virtual Memory Manager initialized\n");
    cpu_init();
    printf("{FG(0,255,0)}[OK]{FG(255,255,255)} CPU detected\n");
    pic_init();
    printf("{FG(0,255,0)}[OK]{FG(255,255,255)} PIC initialized\n");
    idt_init();
    printf("{FG(0,255,0)}[OK]{FG(255,255,255)} IDT initialized\n");
    pit_init(1000);
    printf("{FG(0,255,0)}[OK]{FG(255,255,255)} PIT initialized (1000 Hz)\n");
    keyboard_init();
    printf("{FG(0,255,0)}[OK]{FG(255,255,255)} Keyboard initialized\n");
    ata_init();
    printf("{FG(0,255,0)}[OK]{FG(255,255,255)} ATA Driver initialized\n");
    xcfs_init(0);
    printf("{FG(0,255,0)}[OK]{FG(255,255,255)} XCFS File System initialized\n");
    scheduler_init();
    printf("{FG(0,255,0)}[OK]{FG(255,255,255)} Task Scheduler initialized\n");
    /*
    uint8_t* buffer = fb_init_backbuffer();
    if (buffer != NULL) {
        fb_copy_to_backbuffer();
        printf("{FG(0,255,0)}[OK]{FG(255,255,255)} Video Back Buffer initialized [0x%x]\n", buffer);
        printf("{FG(0,255,0)}[OK]{FG(255,255,255)} FB Swap via timer (60 FPS)\n");
    } else {
        printf("{FG(255,0,0)}[FAIL]{FG(255,255,255)} Video Back Buffer initialization failed\n");
    }
    */

    printf("\n");
}
