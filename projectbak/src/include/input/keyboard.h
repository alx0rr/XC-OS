#ifndef KEYBOARD_H
#define KEYBOARD_H
#include <stdint.h>
#include <stdbool.h>
void keyboard_init(void);
bool keyboard_key(uint8_t keycode);
char* keyboard_input(void);
#endif
