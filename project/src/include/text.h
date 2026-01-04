#ifndef TEXT_H
#define TEXT_H

#include <stdint.h>
/* CHAR_WIDTH = orig_CHAR_WIDTH / now_SCALE * new_SCALE
CHAR_HEIGHT = orig_CHAR_HEIGHT / now_SCALE * new_SCALE
*/
#define CHAR_WIDTH 12
#define CHAR_HEIGHT 21
#define FONT_SCALE 1

#define X_MARGIN 30
#define Y_MARGIN 30

void putchar(char c);
void printf(const char *format, ...);
void clear();
void print_at_pos(const char* str, uint16_t x, uint16_t y, uint32_t fore_color, uint32_t back_color);
void drawchar_at_pos(char c, uint16_t x, uint16_t y, uint32_t fore_color, uint32_t back_color);

extern uint16_t xpos;
extern uint16_t ypos;
extern uint32_t fg_color;
extern uint32_t bg_color;

#endif 
