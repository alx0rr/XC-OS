#ifndef KEYBOARD_H
#define KEYBOARD_H
#include <stdint.h>
#include <stdbool.h>
void keyboard_init(void);
bool keyboard_key(uint8_t keycode);
void keyboard_input(char* buf, uint32_t size);
#endif
char keyboard_getchar_raw(void);
