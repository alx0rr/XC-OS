#include "../../include/graphics/framebuffer.h"
#include "../../include/font.h"
#include "../../lib/string.h"
#include "../../include/text.h"
#define RGB(r, g, b)       ((uint32_t)(((r) << 16) | ((g) << 8) | (b)))
#define RGBA(r, g, b, a)   ((uint32_t)(((r) << 16) | ((g) << 8) | (b) | ((a) << 24)))
#define R_PART(color) (((color) >> 16) & 0xFF)
#define G_PART(color) (((color) >> 8) & 0xFF)
#define B_PART(color) ((color) & 0xFF)
#define A_PART(color) (((color) >> 24) & 0xFF)
uint16_t xpos = X_MARGIN;
uint16_t ypos = Y_MARGIN;
uint32_t bg_color = 0;
uint32_t fg_color = 0xFFFFFF;
uint8_t current_font_scale = 3;
void text_init() {
    uint16_t width = fb_get_width();
    uint16_t height = fb_get_height();
    if (width <= 640 && height <= 480) {
        current_font_scale = 1;
    } else if (width <= 1024 && height <= 768) {
        current_font_scale = 3;
    } else if (width <= 1920 && height <= 1080) {
        current_font_scale = 3;
    } else {
        current_font_scale = 4;
    }
    xpos = X_MARGIN;
    ypos = Y_MARGIN;
}
void text_set_scale(uint8_t scale) {
    if (scale >= 1 && scale <= 4) {
        current_font_scale = scale;
        xpos = X_MARGIN;
        ypos = Y_MARGIN;
    }
}
uint8_t text_get_scale() {
    return current_font_scale;
}
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
		for (uint32_t i = 128; i > 0; i >>= 1) {
			if (i & *bitpattern) {
				fb_putpixel(xx, yy, fore_color);
			} else {
				fb_putpixel(xx, yy, back_color);
			}
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
	bitmapblt(x, y, 13, &FontData[(uint32_t)(uint8_t)c * 13], fore_color, back_color);
}
static uint8_t u8buf[3];
static uint8_t u8len = 0;
static uint8_t u8need = 0;

static void pc(char c) {
    const uint16_t char_width  = 8;
    const uint16_t char_height = 14;
    if (c == '\n') {
        xpos = X_MARGIN;
        ypos += char_height;
        if (ypos + char_height >= fb_get_height() - Y_MARGIN) {
            fb_scroll_up(char_height, bg_color);
            ypos -= char_height;
        }
        return;
    }
    if (c == '\r') { xpos = X_MARGIN; return; }
    if (c == '\b') {
        if (xpos >= X_MARGIN + char_width) xpos -= char_width;
        return;
    }
    drawchar_at_pos(c, xpos, ypos, fg_color, bg_color);
    xpos += char_width;
    if (xpos + char_width > fb_get_width() - X_MARGIN) {
        xpos = X_MARGIN;
        ypos += char_height;
        if (ypos + char_height >= fb_get_height() - Y_MARGIN) {
            fb_scroll_up(char_height, bg_color);
            ypos -= char_height;
        }
    }
}

static uint8_t u8decode(void) {
    uint8_t b0 = u8buf[0];
    uint8_t b1 = u8buf[1];
    uint8_t b2 = u8buf[2];
    if (b0 == 0xD0) {
        if (b1 == 0x81) return 0xF0;
        if (b1 >= 0x90 && b1 <= 0xBF) return (uint8_t)(b1 - 0x90 + 0x80);
    }
    if (b0 == 0xD1) {
        if (b1 == 0x91) return 0xF1;
        if (b1 >= 0x80 && b1 <= 0x8F) return (uint8_t)(b1 - 0x80 + 0xE0);
    }
    if (b0 == 0xE2 && b1 == 0x80) {
        if (b2 == 0x93) return 0xFB;
        if (b2 == 0x94) return 0xFC;
    }
    return '?';
}

void putchar(char c) {
    uint8_t b = (uint8_t)c;
    if (u8need > 0) {
        u8buf[u8len++] = b;
        if (u8len == u8need) {
            pc((char)u8decode());
            u8len = 0;
            u8need = 0;
        }
        return;
    }
    if (b >= 0x80) {
        u8buf[0] = b;
        u8len = 1;
        if ((b & 0xE0) == 0xC0) { u8need = 2; }
        else if ((b & 0xF0) == 0xE0) { u8need = 3; }
        else { u8len = 0; u8need = 0; pc('?'); }
        return;
    }
    pc(c);
}


void print_at_pos(const char* s, uint16_t x, uint16_t y, uint32_t fore_color, uint32_t back_color) {
	uint16_t sx = xpos, sy = ypos;
	uint32_t sfg = fg_color, sbg = bg_color;
	xpos = x; ypos = y;
	fg_color = fore_color; bg_color = back_color;
	while (*s != '\0') { putchar(*s++); }
	xpos = sx; ypos = sy;
	fg_color = sfg; bg_color = sbg;
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
void clear() {
    fb_fill(bg_color);
    xpos = X_MARGIN;
    ypos = Y_MARGIN;
}
