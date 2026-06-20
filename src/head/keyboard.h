#ifndef KEYBOARD_H
#define KEYBOARD_H

void keyboard_init(void);
int keyboard_has_data(void);
unsigned char keyboard_poll(void);
char keyboard_scancode_to_ascii(unsigned char scancode);
int keyboard_overflow_count(void);

#endif
