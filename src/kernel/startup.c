#include "../include/graphics/vbe.h"
#include "../include/text.h"
#include "../include/memory/pmm.h"
#include "../include/memory/vmm.h"
#include "../include/cpu/cpu.h"
#include "../include/interrupts/idt.h"
#include "../include/storage/ata.h"
#include "../include/fs/xcfs.h"
#include "../include/input/keyboard.h"
#include "../include/timer/pit.h"
#include "../include/sound/pcspk.h"
#include "../include/graphics/framebuffer.h"
#include "../include/gdt/gdt.h"
#include "../include/syscall/syscall.h"
#include "../include/scheduler/sched.h"
#include "../include/proc/proc.h"
#include "../lib/types.h"

void startup() {
    vbe_init();
    text_init();
    clear();
    printf("{FG(0,255,0)}[OK]{FG(255,255,255)} VBE initialized\n");
    init_pmm();
    printf("{FG(0,255,0)}[OK]{FG(255,255,255)} Memory Manager initialized\n");
    vmm_init();
    vmm_enable_paging();
    printf("{FG(0,255,0)}[OK]{FG(255,255,255)} Virtual Memory Manager initialized\n");
    gdt_init();
    printf("{FG(0,255,0)}[OK]{FG(255,255,255)} GDT/TSS initialized\n");
    cpu_init();
    printf("{FG(0,255,0)}[OK]{FG(255,255,255)} CPU detected\n");
    pic_init();
    printf("{FG(0,255,0)}[OK]{FG(255,255,255)} PIC initialized\n");
    idt_init();
    printf("{FG(0,255,0)}[OK]{FG(255,255,255)} IDT initialized\n");
    pit_init(1000);
    printf("{FG(0,255,0)}[OK]{FG(255,255,255)} PIT initialized\n");
    syscall_init();
    printf("{FG(0,255,0)}[OK]{FG(255,255,255)} Syscall (int 0x80) initialized\n");
    sched_init();
    idt_register_syscall_handler(syscall_handler);
    idt_register_sched_tick(sched_tick);
    printf("{FG(0,255,0)}[OK]{FG(255,255,255)} Scheduler initialized\n");
    pcspk_init();
    keyboard_init();
    printf("{FG(0,255,0)}[OK]{FG(255,255,255)} Keyboard initialized\n");
    ata_init();
    printf("{FG(0,255,0)}[OK]{FG(255,255,255)} ATA Driver initialized\n");
    xcfs_init(0);
    printf("{FG(0,255,0)}[OK]{FG(255,255,255)} XCFS File System initialized\n");
    pcspk_play_note(1000, 50);
}
