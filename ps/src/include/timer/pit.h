#ifndef PIT_H
#define PIT_H

#include "../../lib/types.h"

#define PIT_CHANNEL_0 0x40
#define PIT_CHANNEL_1 0x41
#define PIT_CHANNEL_2 0x42
#define PIT_COMMAND   0x43

#define PIT_FREQUENCY 1193182

#define PIT_MODE_0 0x00
#define PIT_MODE_1 0x02
#define PIT_MODE_2 0x04
#define PIT_MODE_3 0x06
#define PIT_MODE_4 0x08
#define PIT_MODE_5 0x0A

#define PIT_ACCESS_LATCH   0x00
#define PIT_ACCESS_LOBYTE  0x10
#define PIT_ACCESS_HIBYTE  0x20
#define PIT_ACCESS_LOHIBYTE 0x30

#define PIT_CHANNEL_0_SELECT 0x00
#define PIT_CHANNEL_1_SELECT 0x40
#define PIT_CHANNEL_2_SELECT 0x80

void pit_init(u32 frequency);
u64 pit_get_ticks();
void pit_sleep(u32 milliseconds);

#endif
