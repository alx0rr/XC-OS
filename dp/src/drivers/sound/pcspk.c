#include "../../include/sound/pcspk.h"
#include "../../include/timer/pit.h"
#include "../../lib/io.h"

#define PCSPK_PORT 0x61
#define PIT_CHANNEL_2 0x42
#define PIT_COMMAND 0x43

void pcspk_init(void) {
    pcspk_off();
}

void pcspk_play_note(u32 frequency, u32 duration_ms) {
    if (frequency < 20) {
        pcspk_off();
        return;
    }
    
    u32 divisor = 1193180 / frequency;
    
    outb(PIT_COMMAND, 0xB6);
    outb(PIT_CHANNEL_2, (u8)(divisor & 0xFF));
    outb(PIT_CHANNEL_2, (u8)((divisor >> 8) & 0xFF));
    
    u8 tmp = inb(PCSPK_PORT);
    if (tmp != (tmp | 3)) {
        outb(PCSPK_PORT, tmp | 3);
    }
    
    if (duration_ms > 0) {
        pit_sleep(duration_ms);
        pcspk_off();
    }
}

void pcspk_beep(u32 frequency, u32 duration_ms) {
    pcspk_play_note(frequency, duration_ms);
}

void pcspk_off(void) {
    u8 tmp = inb(PCSPK_PORT) & 0xFC;
    outb(PCSPK_PORT, tmp);
}
