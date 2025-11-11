#include "../../include/graphics/framebuffer.h"
#include "../../include/font.h"
#include "../../lib/string.h"
#include "../include/graphics/vbe.h"

#define RGB(r, g, b)       ((uint32_t)(((r) << 16) | ((g) << 8) | (b)))
#define RGBA(r, g, b, a)   ((uint32_t)(((r) << 16) | ((g) << 8) | (b) | ((a) << 24)))


#define R_PART(color) (((color) >> 16) & 0xFF)
#define G_PART(color) (((color) >> 8) & 0xFF)
#define B_PART(color) ((color) & 0xFF)
#define A_PART(color) (((color) >> 24) & 0xFF)


uint16_t xpos = 0;
uint16_t ypos = 0;
uint32_t bg_color = 0;
uint32_t fg_color = 0xFFFFFF;


void bitmapblt(
    uint16_t x, 
    uint16_t y,
    uint8_t h,
    uint8_t* bitpattern,
    uint32_t fore_color,
    uint32_t back_color
    )
{
    uint16_t xx;

    uint16_t yy = y;
    for (uint32_t j = 0; j < h; j++) {

        xx = x;
        for (uint32_t i = 128; i > 0; i >>= 1)
        {

            if (i & *bitpattern)
                fb_putpixel(xx, yy, fore_color);
            else 
                fb_putpixel(xx, yy, back_color);

            xx++;
        }

        bitpattern++;
        yy++;
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
    bitmapblt(x, y, 13, &FontData[(uint32_t)c * 13], fore_color, back_color);
}

void scroll() {
    uint32_t w = fb_get_width();
    uint32_t h = fb_get_height();
    uint32_t pitch = w * 4;
    uint32_t line_size = 14 * pitch;

    for (uint32_t y = 0; y < h - 14; y++) {
        for (uint32_t x = 0; x < w; x++) {
            uint32_t src_y = y + 14;
            uint32_t color = 0;
            
            if (src_y < h) {
                vbe_info_t vbe = get_vbe_struct();
                uint8_t* fb = vbe_get_framebuffer();
                if (vbe.bpp == 32) {
                    uint32_t* pixel = (uint32_t*)(fb + src_y * vbe.pitch + x * 4);
                    color = *pixel;
                }
            }
            
            fb_putpixel(x, y, color);
        }
    }

    for (uint32_t y = h - 14; y < h; y++) {
        for (uint32_t x = 0; x < w; x++) {
            fb_putpixel(x, y, 0);
        }
    }
    
    ypos -= 14;
}

void putchar(char c) {
    if (c == '\n') {
        xpos = 0;
        ypos += 14;
        if (ypos + 14 >= fb_get_height()) scroll();
    } else if (c == '\r') {
        xpos = 0;
    } else if (c == '\b') {
        if (xpos >= 8) {
            xpos -= 8;
            drawchar_at_pos(' ', xpos, ypos, fg_color, bg_color);
        }
    } else {
        drawchar_at_pos(c, xpos, ypos, fg_color, bg_color);
        xpos += 8;
        if (xpos >= fb_get_width()) {
            xpos = 0;
            ypos += 14;
            if (ypos + 14 >= fb_get_height()) scroll();
        }
    }
}



void print_at_pos(const char* str, uint16_t x, uint16_t y, uint32_t fore_color, uint32_t back_color) {
    while (*str != '\0') {
        drawchar_at_pos(*str, x, y, fore_color, back_color);
        x += 8;
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


            r = 0;
            while (*format >= '0' && *format <= '9') {
                r = r * 10 + (*format - '0');
                format++;
            }

            if (*format == ',') format++;

            g = 0;
            while (*format >= '0' && *format <= '9') {
                g = g * 10 + (*format - '0');
                format++;
            }

            if (*format == ',') format++;

            b = 0;
            while (*format >= '0' && *format <= '9') {
                b = b * 10 + (*format - '0');
                format++;
            }

            if (*format == ')') format++;
            if (*format == '}') format++;

            bg_color = RGB(r, g, b);
        } else if (c == '{' && strncmp(format, "FG(", 3) == 0) {
            format += 3;
            int r = 0, g = 0, b = 0;

            r = 0;
            while (*format >= '0' && *format <= '9') {
                r = r * 10 + (*format - '0');
                format++;
            }

            if (*format == ',') format++;

            g = 0;
            while (*format >= '0' && *format <= '9') {
                g = g * 10 + (*format - '0');
                format++;
            }

            if (*format == ',') format++;

            b = 0;
            while (*format >= '0' && *format <= '9') {
                b = b * 10 + (*format - '0');
                format++;
            }

            if (*format == ')') format++;
            if (*format == '}') format++;

            fg_color = RGB(r, g, b);
        } else if (c != '%') {
            putchar(c);
        } else {
            char *p, *p2;
            int pad0 = 0, pad = 0;

            c = *format++;
            if (c == '0') {
                pad0 = 1;
                c = *format++;
            }

            if (c >= '0' && c <= '9') {
                pad = c - '0';
                c = *format++;
            }

            switch (c) {
                case 'd':
                case 'u':
                case 'x':
                    itoa(buf, c, *((int *) arg++));
                    p = buf;
                    goto string;
                    break;
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
                    if (!p) {
                        p = "(null)";
                    }

                string:
                    for (p2 = p; *p2; p2++);
                    for (; p2 < p + pad; p2++) 
                        putchar(' ');
                    while (*p) 
                        putchar(*p++);
                    break;

                default:
                    putchar(c);
                    break;
            }
        }
    }
}


void clear(){
    fb_fill(RGB(0, 0, 0));
    xpos = 0;
    ypos = 0;
}







