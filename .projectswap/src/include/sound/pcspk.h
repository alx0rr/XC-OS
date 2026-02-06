#ifndef PCSPK_H
#define PCSPK_H

#include "../../lib/types.h"

void pcspk_init(void);
void pcspk_beep(u32 frequency, u32 duration_ms);
void pcspk_play_note(u32 frequency, u32 duration_ms);
void pcspk_off(void);

#endif
