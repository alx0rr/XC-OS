#ifndef RING3_H
#define RING3_H

#include <stdint.h>

void ring3_enter(uint32_t eip, uint32_t esp);
void ring3_run_test(void);

#endif
