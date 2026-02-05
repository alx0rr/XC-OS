#include "../../include/input/keyboard.h"
#include "../../lib/io.h"
#include "../../include/text.h"
#include "../../include/interrupts/idt.h"
#include <stddef.h>
#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64
#define KEYBOARD_BUFFER_SIZE 256
static char input_buffer[KEYBOARD_BUFFER_SIZE];
static volatile uint32_t buffer_head = 0;
static volatile uint32_t buffer_tail = 0;
static volatile uint8_t shift_pressed = 0;
volatile uint8_t ctrl_pressed = 0;
static volatile uint8_t alt_pressed = 0;
static volatile uint8_t caps_lock = 0;
unsigned char keymap[128] =
{
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8',
  '9', '0', '-', '=', '\b',
  '\t',
  'q', 'w', 'e', 'r',
  't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,
  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
 '\'', '`',   0,
 '\\', 'z', 'x', 'c', 'v', 'b', 'n',
  'm', ',', '.', '/',   0,
  '*',
    0,
  ' ',
    0,
    0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,
    0,
    0,
    0,
    19,
    0,
  '-',
    18,
    0,
    17,
  '+',
    0,
    20,
    0,
    0,
    0,
    0,   0,   0,
    0,
    0,
    0,
};
unsigned char keymap_up[128] =
{
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*',
  '(', ')', '_', '+', '\b',
  '\t',
  'Q', 'W', 'E', 'R',
  'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,
  'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',
 '\"', '~',   0,
 '|', 'Z', 'X', 'C', 'V', 'B', 'N',
  'M', '<', '>', '?',   0,
  '*',
    0,
  ' ',
    0,
    0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,
    0,
    0,
    0,
    19,
    0,
  '-',
    18,
    0,
    17,
  '+',
    0,
    20,
    0,
    0,
    0,
    0,   0,   0,
    0,
    0,
    0,
};
static void keyboard_irq_handler(registers_t* regs) {
    uint8_t scan_code = inb(KEYBOARD_DATA_PORT);
    
    if (scan_code == 0x3B) {
        printf("\n{FG(255,0,0)}[F1] Emergency shutdown initiated...\n");
        __asm__ volatile("cli");
        __asm__ volatile("hlt");
        while(1);
    }
    
    if (scan_code == 0x3C) {
        printf("\n{FG(255,255,0)}[F2] Rebooting system...\n");
        outb(0x64, 0xFE);
        while(1);
    }
    
    if (scan_code == 0x2A || scan_code == 0x36) {
        shift_pressed = 1;
        return;
    } else if (scan_code == 0xAA || scan_code == 0xB6) {
        shift_pressed = 0;
        return;
    }
    if (scan_code == 0x1D) {
        ctrl_pressed = 1;
        return;
    } else if (scan_code == 0x9D) {
        ctrl_pressed = 0;
        return;
    }
    if (scan_code == 0x38) {
        alt_pressed = 1;
        return;
    } else if (scan_code == 0xB8) {
        alt_pressed = 0;
        return;
    }
    if (scan_code == 0x3A) {
        caps_lock = !caps_lock;
        return;
    }
    if (scan_code & 0x80) {
        return;
    }
    char c = 0;
    if (shift_pressed) {
        c = keymap_up[scan_code];
    } else {
        c = keymap[scan_code];
        if (caps_lock && c >= 'a' && c <= 'z') {
            c = c - 'a' + 'A';
        }
    }
    if (c) {
        uint32_t next_head = (buffer_head + 1) % KEYBOARD_BUFFER_SIZE;
        if (next_head != buffer_tail) {
            input_buffer[buffer_head] = c;
            buffer_head = next_head;
        }
    }
}
void keyboard_init(void) {
    buffer_head = 0;
    buffer_tail = 0;
    shift_pressed = 0;
    ctrl_pressed = 0;
    alt_pressed = 0;
    caps_lock = 0;
    idt_register_irq_handler(1, keyboard_irq_handler);
    uint8_t mask = inb(0x21);
    mask &= ~(1 << 1);
    outb(0x21, mask);
}
static char keyboard_getchar(void) {
    while (buffer_head == buffer_tail) {
        asm volatile("hlt");
    }
    char c = input_buffer[buffer_tail];
    buffer_tail = (buffer_tail + 1) % KEYBOARD_BUFFER_SIZE;
    return c;
}
char* keyboard_input(void) {
    static char line_buffer[256];
    uint32_t index = 0;
    while (1) {
        char c = keyboard_getchar();
        if (c == '\b') {
            if (index > 0) {
                index--;
                line_buffer[index] = '\0';
                printf("\b \b");
            }
            continue;
        }
        if (c == '\n') {
            line_buffer[index] = '\0';
            return line_buffer;
        }
        if (index < sizeof(line_buffer) - 1) {
            line_buffer[index++] = c;
            printf("%c", c);
        }
    }
}
bool keyboard_key(uint8_t keycode) {
    return 0;
}
