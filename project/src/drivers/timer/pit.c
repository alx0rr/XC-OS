#include "../../include/timer/pit.h"
#include "../../include/interrupts/idt.h"
#include "../../lib/io.h"

static volatile u64 pit_ticks = 0;
static u32 pit_frequency = 0;

static void pit_irq_handler(registers_t regs) {
    (void)regs;
    pit_ticks++;
}

void pit_init(u32 frequency) {
    pit_frequency = frequency;
    pit_ticks = 0;
    
    u32 divisor = PIT_FREQUENCY / frequency;
    
    u8 command = PIT_CHANNEL_0_SELECT | PIT_ACCESS_LOHIBYTE | PIT_MODE_3;
    outb(PIT_COMMAND, command);
    
    outb(PIT_CHANNEL_0, (u8)(divisor & 0xFF));
    outb(PIT_CHANNEL_0, (u8)((divisor >> 8) & 0xFF));
    
    idt_register_irq_handler(0, pit_irq_handler);
    
    u8 mask = inb(0x21);
    mask &= ~0x01;
    outb(0x21, mask);
}

u64 pit_get_ticks() {
    return pit_ticks;
}

void pit_sleep(u32 milliseconds) {
    u32 ticks_to_wait = (pit_frequency * milliseconds) / 1000;
    u32 start_ticks = (u32)pit_ticks;
    
    while (((u32)pit_ticks - start_ticks) < ticks_to_wait) {
    }
}
