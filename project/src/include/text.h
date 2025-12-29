#ifndef TEXT_H
#define TEXT_H

#include <stdint.h>

void bitmapblt(
    uint16_t x, 
    uint16_t y,
    uint8_t h,
    uint8_t* bitpattern,
    uint32_t fore_color,
    uint32_t back_color
    );

void drawchar_at_pos(
    char c,
    uint16_t x, 
    uint16_t y,
    uint32_t fore_color,
    uint32_t back_color
    );

void putchar(char c);
void print_at_pos(const char* str, uint16_t x, uint16_t y, uint32_t fore_color, uint32_t back_color);
void printf(const char *format, ...);
void clear();


#endif 
