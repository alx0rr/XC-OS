#include <stdint.h>

uint8_t vbe_mode_info_data[256] = {0};

void* vbe_get_framebuffer(void) {
    return (void*)0xE0000000;
}

uint16_t vbe_get_width(void) {
    return 1024;
}

uint16_t vbe_get_height(void) {
    return 768;
}

void* get_vbe_struct(void) {
    return (void*)vbe_mode_info_data;
}
