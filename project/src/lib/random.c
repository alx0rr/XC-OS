#include "random.h"

static uint32_t rng_state = 12345;

// Коэффициенты из glibc
#define LCG_A 1103515245
#define LCG_C 12345
#define LCG_M 2147483648 

void rand_seed(uint32_t seed) {
    rng_state = seed;
}

uint32_t rand_next(void) {
    rng_state = (LCG_A * rng_state + LCG_C) % LCG_M;
    return rng_state;
}

uint32_t rand_max(uint32_t max) {
    if (max == 0) return 0;
    return rand_next() % max;
}

uint32_t rand_range(uint32_t min, uint32_t max) {
    if (min >= max) return min;
    return min + rand_next() % (max - min);
}