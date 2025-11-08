#include <stdint.h>
#include <stddef.h>
#include "../include/memory.h"

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64

void vbe_init();
void vbe_clear();
void printx(const char* str);
void vbe_set_color(uint32_t fg, uint32_t bg);
void vbe_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint32_t color);
void vbe_putpixel(uint16_t x, uint16_t y, uint32_t color);

static inline void outb(uint16_t port, uint8_t value) {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

void __stack_chk_fail() { while(1); }

char* utoa_dec(unsigned v, char *buf, unsigned bufsz) {
    if (bufsz == 0) return buf;
    if (v == 0) {
        if (bufsz > 1) { buf[0] = '0'; buf[1] = 0; return buf; }
        buf[0] = 0; return buf;
    }
    char tmp[12];
    unsigned t = 0;
    while (v && t + 1 < sizeof(tmp)) { tmp[t++] = '0' + (v % 10); v /= 10; }
    if (t + 1 > bufsz) { buf[0] = 0; return buf; }
    for (unsigned i = 0; i < t; ++i) buf[i] = tmp[t - 1 - i];
    buf[t] = 0;
    return buf;
}

static const char keymap[128] = {
    0,27,'1','2','3','4','5','6','7','8','9','0','-','=',8,
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',0,
    'a','s','d','f','g','h','j','k','l',';','\'','`',0,
    '\\','z','x','c','v','b','n','m',',','.','/',0,
    '*',0,' ',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

void kernel_main() {
    memory_init();
    vbe_init();
    vbe_clear();
    vbe_set_color(0xFFFFFF, 0x000000);
    printx("Keyboard echo ready:\n");

    uint32_t used, free_mem, blocks;
    memory_stats(&used, &free_mem, &blocks);
    char buf[16];

    printx("Used: "); utoa_dec(used, buf, sizeof(buf)); printx(buf); printx("\n");
    printx("Free: "); utoa_dec(free_mem, buf, sizeof(buf)); printx(buf); printx("\n");
    printx("Blocks: "); utoa_dec(blocks, buf, sizeof(buf)); printx(buf); printx("\n\n");

    while (1) {
        if (inb(KEYBOARD_STATUS_PORT) & 1) {
            uint8_t sc = inb(KEYBOARD_DATA_PORT);
            if (!(sc & 0x80)) {
                char c = keymap[sc];
                if (c) {
                    char s[2] = {c,0};
                    printx(s);
                }
            }
        }
    }
}
