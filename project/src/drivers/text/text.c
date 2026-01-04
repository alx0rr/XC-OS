#include "../../include/graphics/framebuffer.h"
#include "../../include/font.h"
#include "../../lib/string.h"
#include <stdint.h>

#define RGB(r, g, b)       ((uint32_t)(((r) << 16) | ((g) << 8) | (b)))

static uint16_t xpos = 0;
static uint16_t ypos = 0;
static uint32_t bg_color = 0;
static uint32_t fg_color = 0xFFFFFF;

static const uint8_t MY_FONT_W = 8;
static const uint8_t MY_FONT_H = 13;
static const int BOLD_PERCENT = 25;
static uint8_t char_width = MY_FONT_W;
static uint8_t line_height = MY_FONT_H + 1;
static int font_metrics_inited = 0;

static void init_font_metrics() {
    if (font_metrics_inited) return;
    int extra_h = (MY_FONT_W * BOLD_PERCENT + 50) / 100;
    int extra_v = (MY_FONT_H * BOLD_PERCENT + 50) / 100;
    char_width = MY_FONT_W + extra_h;
    line_height = MY_FONT_H + 1 + extra_v;
    font_metrics_inited = 1;
}

void bitmapblt(
    uint16_t x,
    uint16_t y,
    uint8_t h,
    const uint8_t* bitpattern,
    uint32_t fore_color,
    uint32_t back_color
    )
{
    init_font_metrics();
    int extra_h = (MY_FONT_W * BOLD_PERCENT + 50) / 100;
    int extra_v = (h * BOLD_PERCENT + 50) / 100;
    int total_w = MY_FONT_W + extra_h;
    int total_h = h + extra_v;
    uint16_t xx, yy;
    uint32_t fbw = fb_get_width();
    uint32_t fbh = fb_get_height();
    for (yy = y; yy < y + total_h && yy < fbh; yy++) {
        for (xx = x; xx < x + total_w && xx < fbw; xx++) {
            fb_putpixel(xx, yy, back_color);
        }
    }
    for (uint32_t row = 0; row < h; row++) {
        uint8_t pattern = bitpattern[row];
        int colx = 0;
        for (int bit = 7; bit >= 0; bit--) {
            int set = (pattern >> bit) & 1;
            if (set) {
                for (int dy = 0; dy <= extra_v; dy++) {
                    for (int dx = 0; dx <= extra_h; dx++) {
                        uint32_t px = x + colx + dx;
                        uint32_t py = y + row + dy;
                        if (px < fbw && py < fbh) fb_putpixel(px, py, fore_color);
                    }
                }
            }
            colx++;
        }
    }
}

void drawchar_at_pos(
    char c,
    uint16_t x,
    uint16_t y,
    uint32_t fore_color,
    uint32_t back_color
    )
{
    bitmapblt(x, y, MY_FONT_H, &FontData[(uint32_t)(uint8_t)c * MY_FONT_H], fore_color, back_color);
}

void putchar(char c) {
    init_font_metrics();
    if (c == '\n') {
        xpos = 0;
        ypos += line_height;
        if (ypos + line_height >= fb_get_height()) {
            fb_fill(bg_color);
            xpos = 0;
            ypos = 0;
        }
    } else if (c == '\r') {
        xpos = 0;
    } else if (c == '\b') {
        if (xpos >= char_width) {
            xpos -= char_width;
            drawchar_at_pos(' ', xpos, ypos, fg_color, bg_color);
        }
    } else {
        drawchar_at_pos(c, xpos, ypos, fg_color, bg_color);
        xpos += char_width;
        if (xpos >= fb_get_width()) {
            xpos = 0;
            ypos += line_height;
            if (ypos + line_height >= fb_get_height()) {
                fb_fill(bg_color);
                xpos = 0;
                ypos = 0;
            }
        }
    }
}

void print_at_pos(const char* str, uint16_t x, uint16_t y, uint32_t fore_color, uint32_t back_color) {
    init_font_metrics();
    while (*str != '\0') {
        drawchar_at_pos(*str, x, y, fore_color, back_color);
        x += char_width;
        str++;
    }
}

void printf(const char *format, ...) {
    char **arg = (char **) &format;
    int c;
    char buf[20];
    arg++;
    while ((c = *format++) != '\0') {
        if (c == '{' && strncmp(format, "BG(", 3) == 0) {
            format += 3;
            int r = 0, g = 0, b = 0;
            while (*format >= '0' && *format <= '9') { r = r * 10 + (*format - '0'); format++; }
            if (*format == ',') format++;
            while (*format >= '0' && *format <= '9') { g = g * 10 + (*format - '0'); format++; }
            if (*format == ',') format++;
            while (*format >= '0' && *format <= '9') { b = b * 10 + (*format - '0'); format++; }
            if (*format == ')') format++;
            if (*format == '}') format++;
            bg_color = RGB(r, g, b);
        } else if (c == '{' && strncmp(format, "FG(", 3) == 0) {
            format += 3;
            int r = 0, g = 0, b = 0;
            while (*format >= '0' && *format <= '9') { r = r * 10 + (*format - '0'); format++; }
            if (*format == ',') format++;
            while (*format >= '0' && *format <= '9') { g = g * 10 + (*format - '0'); format++; }
            if (*format == ',') format++;
            while (*format >= '0' && *format <= '9') { b = b * 10 + (*format - '0'); format++; }
            if (*format == ')') format++;
            if (*format == '}') format++;
            fg_color = RGB(r, g, b);
        } else if (c != '%') {
            putchar(c);
        } else {
            char *p, *p2;
            int pad0 = 0, pad = 0;
            c = *format++;
            if (c == '0') { pad0 = 1; c = *format++; }
            if (c >= '0' && c <= '9') { pad = c - '0'; c = *format++; }
            switch (c) {
                case 'd':
                case 'u':
                case 'x':
                    itoa(buf, c, *((int *) arg++));
                    p = buf;
                    goto string;
                case 'c': {
                    char char_value = (char) *((int *) arg++);
                    for (int i = 0; i < pad - 1; i++) putchar(' ');
                    putchar(char_value);
                    break;
                }
                case 'b':
                    putchar('\b');
                    break;
                case 's':
                    p = *arg++;
                    if (!p) p = "(null)";
                string:
                    for (p2 = p; *p2; p2++);
                    for (; p2 < p + pad; p2++) putchar(' ');
                    while (*p) putchar(*p++);
                    break;
                default:
                    putchar(c);
                    break;
            }
        }
    }
}

void clear(){
    init_font_metrics();
    fb_fill(RGB(0, 0, 0));
    xpos = 0;
    ypos = 0;
}
