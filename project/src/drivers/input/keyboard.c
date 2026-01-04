#include "../../include/input/keyboard.h"
#include "../../lib/io.h"
#include "../../include/text.h"
#include <stddef.h>


#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64

static char input_buffer[256];
static uint8_t buffer_index = 0;
static bool shift_pressed = false;


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
  'q', 'w', 'e', 'r',
  't', 'y', 'u', 'i', 'o', 'p', '{', '}', '\n',
    0,
  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ':',
 '\"', '~',   0,
 '|', 'z', 'x', 'c', 'v', 'b', 'n',
  'm', '<', '>', '?',   0,
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


bool keyboard_key(uint8_t keycode) {
    uint8_t status = inb(KEYBOARD_STATUS_PORT);
    if (status & 0x01) {
        uint8_t scan_code = inb(KEYBOARD_DATA_PORT);

        if (scan_code == 0x2A || scan_code == 0x36) {
            shift_pressed = true;
        } else if (scan_code == 0xAA || scan_code == 0xB6) {
            shift_pressed = false;
        }

        if (scan_code == keycode) {
            return true;
        }
    }
    return false;
}


char* keyboard_input(void) {
    while (1) {
        uint8_t status = inb(KEYBOARD_STATUS_PORT);
        if (status & 0x01) {
            uint8_t scan_code = inb(KEYBOARD_DATA_PORT);

            if (scan_code & 0x80) continue;

            if (scan_code == 0x0E) {
                if (buffer_index > 0) {
                    buffer_index--;
                    input_buffer[buffer_index] = '\0';
                    printf("\b \b");
                }
                continue;
            }

            char c = shift_pressed ? keymap_up[scan_code] : keymap[scan_code];

            if (c == '\n') {
                input_buffer[buffer_index] = '\0';
                buffer_index = 0;
                return input_buffer;
            }

            if (c && buffer_index < sizeof(input_buffer) - 1) {
                input_buffer[buffer_index++] = c;
                printf("%c", c);
            }
        }
    }
}