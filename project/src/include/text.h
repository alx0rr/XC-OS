#ifndef TEXT_H
#define TEXT_H
#include <stdint.h>
extern uint8_t current_font_scale;
#define CHAR_WIDTH (12 * current_font_scale)
#define CHAR_HEIGHT (16 * current_font_scale)
#define FONT_SCALE current_font_scale
#define X_MARGIN 0
#define Y_MARGIN 0
void text_init();
void text_set_scale(uint8_t scale);
uint8_t text_get_scale();
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
