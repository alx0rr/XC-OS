#ifndef RANDOM_H
#define RANDOM_H
#include <stdint.h>

void rand_seed(uint32_t seed);

uint32_t rand_next(void);

uint32_t rand_range(uint32_t min, uint32_t max);

uint32_t rand_max(uint32_t max);

#endif