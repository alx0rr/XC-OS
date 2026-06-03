#include "../include/graphics/vbe.h"
#include "../include/graphics/framebuffer.h"
#include "../../lib/time.h"
#include "../../lib/string.h"
#include "../../lib/types.h"
#include "../../include/memory/pmm.h"

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

void fb_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint32_t color) {
    vbe_info_t vbe = get_vbe_struct();
    uint8_t  *fb   = vbe_get_framebuffer();
    uint16_t  pitch = vbe.pitch;

    if (x >= vbe.width || y >= vbe.height) return;
    if ((uint32_t)x + w > vbe.width)  w = (uint16_t)(vbe.width  - x);
    if ((uint32_t)y + h > vbe.height) h = (uint16_t)(vbe.height - y);

    if (vbe.bpp == 32) {
        uint32_t row_buf[w];
        for (uint16_t i = 0; i < w; i++) row_buf[i] = color;
        for (uint16_t row = 0; row < h; row++)
            memcpy(fb + (y + row) * pitch + x * 4, row_buf, (uint32_t)w * 4);
    } else if (vbe.bpp == 24) {
        uint8_t row_buf[w * 3];
        for (uint16_t i = 0; i < w; i++) {
            row_buf[i*3+0] =  color        & 0xFF;
            row_buf[i*3+1] = (color >>  8) & 0xFF;
            row_buf[i*3+2] = (color >> 16) & 0xFF;
        }
        for (uint16_t row = 0; row < h; row++)
            memcpy(fb + (y + row) * pitch + x * 3, row_buf, (uint32_t)w * 3);
    }
}

void fb_fill(uint32_t color) {
    vbe_info_t vbe = get_vbe_struct();
    fb_fill_rect(0, 0, vbe.width, vbe.height, color);
}

uint16_t fb_get_height() { return vbe_get_height(); }
uint16_t fb_get_width()  { return vbe_get_width();  }

void fb_scroll_up(uint16_t pixels, uint32_t bg_color) {
    vbe_info_t vbe = get_vbe_struct();
    uint8_t   *fb  = vbe_get_framebuffer();
    uint32_t row_size    = vbe.pitch;
    uint32_t scroll_bytes = (uint32_t)pixels * row_size;
    uint32_t total_bytes  = (uint32_t)vbe.height * row_size;

    memmove(fb, fb + scroll_bytes, total_bytes - scroll_bytes);
    fb_fill_rect(0, (uint16_t)(vbe.height - pixels), vbe.width, pixels, bg_color);
}
