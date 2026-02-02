#include "../include/graphics/vbe.h"
#include "../include/graphics/framebuffer.h"
#include "../../lib/string.h"
#include "../../lib/types.h"
#include "../../include/memory/pmm.h"

static uint8_t* backbuffer = NULL;

static uint8_t* fb_get_buffer() {
    return backbuffer ? backbuffer : vbe_get_framebuffer();
}

void fb_putpixel(uint16_t x, uint16_t y, uint32_t color) {
    vbe_info_t vbe = get_vbe_struct();
    if (x >= vbe.width || y >= vbe.height) return;
    
    uint8_t* fb = fb_get_buffer();
    
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

void fb_scroll_up(uint16_t pixels, uint32_t bg_color) {
    vbe_info_t vbe = get_vbe_struct();
    uint8_t* fb = fb_get_buffer();

    uint32_t bytes_per_pixel = vbe.bpp / 8;
    uint32_t row_size = vbe.pitch;
    uint32_t scroll_bytes = pixels * row_size;
    uint32_t total_bytes = vbe.height * row_size;

    memmove(fb, fb + scroll_bytes, total_bytes - scroll_bytes);

    uint8_t* bottom = fb + (vbe.height * row_size) - scroll_bytes;
    for (uint32_t y = 0; y < pixels; y++) {
        for (uint32_t x = 0; x < vbe.width; x++) {
            if (bytes_per_pixel == 3) {
                uint8_t* p = bottom + y * row_size + x * 3;
                p[0] = bg_color & 0xFF;
                p[1] = (bg_color >> 8) & 0xFF;
                p[2] = (bg_color >> 16) & 0xFF;
            } else {
                uint32_t* p = (uint32_t*)(bottom + y * row_size + x * 4);
                *p = bg_color;
            }
        }
    }
}

int8_t fb_init_backbuffer() {
    vbe_info_t vbe = get_vbe_struct();
    backbuffer = (uint8_t*)pmm_malloc(vbe.height * vbe.pitch);
    return backbuffer ? 0 : -1;
}

int8_t fb_swap_buffers() {
    if (!backbuffer) return -1;
    vbe_info_t vbe = get_vbe_struct();
    uint8_t* fb = vbe_get_framebuffer();
    memcpy(fb, backbuffer, vbe.height * vbe.pitch);
    return 0;
}

void fb_copy_to_backbuffer() {
    if (!backbuffer) return;
    vbe_info_t vbe = get_vbe_struct();
    uint8_t* fb = vbe_get_framebuffer();
    memcpy(backbuffer, fb, vbe.height * vbe.pitch);
}


void fb_swap_task(void) {
    while (1) {
        fb_swap_buffers();
        for (volatile int i = 0; i < 1000000; i++);
    }
}