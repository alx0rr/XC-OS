#include <stdint.h>
#include "../include/text.h"
#include "../include/graphics/framebuffer.h"

#include "../lib/time.h"
#include "../lib/random.h"

void help() {
    printf("Available command:\nhelp\nclear\nreboot\ntime\nrandom\ncpu\nmemtest\nmmap\n");
}

void time_demo(){
    printf("TIME NOW DEMO");
    datetime_t now = time_get_datetime();
    printf("\nTime: %d:%d:%d\n", now.hour, now.minute, now.second);
    uint32_t unix_ts = time_get_unix_timestamp();
    printf("\nUnix: %d\n", unix_ts);
}

void random_demo(){
    printf("{FG(255,100,100)}RANDOM DEMO\n");
    
    uint32_t seed = time_get_unix_timestamp();
    rand_seed(seed);
    printf("Seed: %d\n\n", seed);
    
    printf("{FG(100,200,255)}Random numbers (0 to 2^31-1):\n");
    for (int i = 0; i < 5; i++) {
        printf("  %d\n", rand_next());
    }
    
    printf("\n{FG(100,255,100)}Dice rolls (1-6):\n");
    for (int i = 0; i < 5; i++) {
        printf("  Roll %d: %d\n", i + 1, rand_range(1, 7));
    }
    
    printf("\n{FG(255,200,100)}Random colors (0-255):\n");
    for (int i = 0; i < 3; i++) {
        uint8_t r = rand_max(256);
        uint8_t g = rand_max(256);
        uint8_t b = rand_max(256);
        printf("  RGB(%d, %d, %d)\n", r, g, b);
    }
    
    printf("\n");
}




