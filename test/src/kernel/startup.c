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
#include "../include/timer/pit.h"
#include "../include/sound/pcspk.h"
#include "../include/graphics/framebuffer.h"
#include "../include/net/ne2000.h"
#include "../include/net/eth.h"
#include "../include/net/arp.h"
#include "../include/net/udp.h"
#include "../include/net/dns.h"
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
    cpu_init();
    printf("{FG(0,255,0)}[OK]{FG(255,255,255)} CPU detected\n");
    pic_init();
    printf("{FG(0,255,0)}[OK]{FG(255,255,255)} PIC initialized\n");
    idt_init();
    printf("{FG(0,255,0)}[OK]{FG(255,255,255)} IDT initialized\n");
    pit_init(1000);
    printf("{FG(0,255,0)}[OK]{FG(255,255,255)} PIT initialized\n");
    pcspk_init();
    keyboard_init();
    printf("{FG(0,255,0)}[OK]{FG(255,255,255)} Keyboard initialized\n");
    ata_init();
    printf("{FG(0,255,0)}[OK]{FG(255,255,255)} ATA Driver initialized\n");
    xcfs_init(0);
    printf("{FG(0,255,0)}[OK]{FG(255,255,255)} XCFS File System initialized\n");

    if (ne2000_init() == 0) {
        u8 m[6];
        eth_init();
        ne2000_get_mac(m);
        arp_init(
            0x0A00020F,   /* 10.0.2.15       */
            0xFFFFFF00,   /* 255.255.255.0    */
            0x0A000202    /* 10.0.2.2  */
        );
        dns_init(0x08080808); /* 8.8.8.8 - Google DNS */
        printf("{FG(0,255,0)}[OK]{FG(255,255,255)} NE2000 MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
               m[0],m[1],m[2],m[3],m[4],m[5]);
        printf("{FG(0,255,0)}[OK]{FG(255,255,255)} Network ready i think?\n");
    } else {
        printf("{FG(255,255,0)}[--]{FG(255,255,255)} NE2000 not found\n");
    }

    printf("\n");
    pcspk_play_note(1000, 50);
}
