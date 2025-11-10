#include "../include/graphics/vbe.h"
#include "../include/graphics/framebuffer.h"


void fb_putpixel(uint16_t x, uint16_t y, uint32_t color) {
    vbe_info_t vbe = get_vbe_struct();
    if (x >= vbe.width || y >= vbe.height) return;
    uint8_t* fb = vbe_get_framebuffer();
    if (vbe.bpp == 24) {
        uint8_t* pixel = fb + y * vbe.pitch + x * 3;
        pixel[0] = color & 0xFF;
        pixel[1] = (color >> 8) & 0xFF;
        pixel[2] = (color >> 16) & 0xFF;
    } else if (vbe.bpp == 32) {
        uint32_t* pixel = (uint32_t*)(fb + y * vbe.pitch + x * 4);
        *pixel = color;
    }
}


void fb_fill(uint32_t color) {
    vbe_info_t vbe = get_vbe_struct();
    for (uint32_t y = 0; y < vbe.height; y++)
        for (uint32_t x = 0; x < vbe.width; x++)
            fb_putpixel(x, y, color);
}

uint16_t fb_get_height() { return vbe_get_height(); }
uint16_t fb_get_width() { return vbe_get_width(); }

//TODO: свап буферов, подсчет коло-ва фпс 