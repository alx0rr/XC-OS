#include "../include/text.h"
#include "../lib/time.h"

int main(int argc, char** argv) {
    uint32_t uptime_ms = get_uptime();
    uint32_t seconds = uptime_ms / 1000;
    uint32_t minutes = seconds / 60;
    uint32_t hours = minutes / 60;
    
    seconds %= 60;
    minutes %= 60;
    
    printf("Uptime: %u:%02u:%02u\n", hours, minutes, seconds);
    return 0;
}
