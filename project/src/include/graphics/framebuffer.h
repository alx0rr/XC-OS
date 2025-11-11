#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <stdint.h>

void fb_putpixel(uint16_t x, uint16_t y, uint32_t color);
void fb_fill(uint32_t color);
uint16_t fb_get_width();
uint16_t fb_get_height();
#endif
