// drivers/vbe.c
#include <stdint.h>
#include "../include/vbe.h"
#include "../include/font.h"

static vbe_info_t vbe;
static uint16_t cursor_x = 0;
static uint16_t cursor_y = 0;
static uint32_t text_color = 0xFFFFFF;
static uint32_t bg_color = 0x000000;

void vbe_putpixel(uint16_t x, uint16_t y, uint32_t color) {
    if (x >= vbe.width || y >= vbe.height) return;
    uint8_t* fb = (uint8_t*)vbe.framebuffer;
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

void vbe_init() {
    vbe.width = vbe_mode_info_data.width;
    vbe.height = vbe_mode_info_data.height;
    vbe.pitch = vbe_mode_info_data.bytes_per_scanline;
    vbe.bpp = vbe_mode_info_data.bpp;
    vbe.framebuffer = (uint32_t*)vbe_mode_info_data.framebuffer;
}

void vbe_fill(uint32_t color) {
    for (uint32_t y = 0; y < vbe.height; y++)
        for (uint32_t x = 0; x < vbe.width; x++)
            vbe_putpixel(x, y, color);
}

void vbe_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint32_t color) {
    for (uint16_t dy = 0; dy < h; dy++)
        for (uint16_t dx = 0; dx < w; dx++)
            vbe_putpixel(x + dx, y + dy, color);
}

void vbe_putchar_16(char c, uint16_t x, uint16_t y, uint32_t fg, uint32_t bg) {
    if ((uint8_t)c >= 128) c = '?';
    for (uint8_t row = 0; row < 16; row++) {
        uint32_t bitmap = font_bitmap[(uint8_t)c][row];
        for (uint8_t col = 0; col < 16; col++) {
            uint32_t color = (bitmap & (1 << (15 - col))) ? fg : bg;
            vbe_putpixel(x + col, y + row, color);
        }
    }
}

void printx(const char* str) {
    const uint16_t char_width = 16;
    const uint16_t char_height = 16;
    const uint16_t line_spacing = 8;
    while (*str) {
        char c = *str++;
        if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
        if (c == '\n') {
            cursor_x = 0;
            cursor_y += char_height + line_spacing;
            if (cursor_y >= vbe.height) cursor_y = 0;
            continue;
        }
        if (c == '\r') { cursor_x = 0; continue; }
        if (c == '\t') {
            cursor_x = (cursor_x + char_width * 4) & ~(char_width * 4 - 1);
            if (cursor_x >= vbe.width) {
                cursor_x = 0;
                cursor_y += char_height + line_spacing;
                if (cursor_y >= vbe.height) cursor_y = 0;
            }
            continue;
        }
        vbe_putchar_16(c, cursor_x, cursor_y, text_color, bg_color);
        cursor_x += char_width;
        if (cursor_x >= vbe.width) {
            cursor_x = 0;
            cursor_y += char_height + line_spacing;
            if (cursor_y >= vbe.height) cursor_y = 0;
        }
    }
}

void vbe_set_color(uint32_t fg, uint32_t bg) {
    text_color = fg;
    bg_color = bg;
}

void vbe_clear() {
    vbe_fill(bg_color);
    cursor_x = 0;
    cursor_y = 0;
}

uint16_t vbe_get_width() { return vbe.width; }
uint16_t vbe_get_height() { return vbe.height; }
