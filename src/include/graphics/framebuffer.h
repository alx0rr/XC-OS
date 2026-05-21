#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H
#include <stdint.h>

void fb_putpixel(uint16_t x, uint16_t y, uint32_t color);
void fb_fill(uint32_t color);
void fb_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint32_t color);
void fb_scroll_up(uint16_t pixels, uint32_t bg_color);

uint16_t fb_get_width();
uint16_t fb_get_height();

#endif
