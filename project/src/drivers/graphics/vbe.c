
#include <stdint.h>
#include "../include/graphics/vbe.h"

static vbe_info_t vbe;


void vbe_init() {
    vbe.width = vbe_mode_info_data.width;
    vbe.height = vbe_mode_info_data.height;
    vbe.pitch = vbe_mode_info_data.bytes_per_scanline;
    vbe.bpp = vbe_mode_info_data.bpp;
    vbe.framebuffer = (uint32_t*)vbe_mode_info_data.framebuffer;
}

uint16_t vbe_get_width() { return vbe.width; }
uint16_t vbe_get_height() { return vbe.height; }
uint16_t vbe_get_pitch() { return vbe.pitch; }
uint8_t vbe_get_bpp() { return vbe.bpp; }
uint8_t* vbe_get_framebuffer() { return (uint8_t*)vbe.framebuffer; }
vbe_info_t get_vbe_struct() {return vbe;}