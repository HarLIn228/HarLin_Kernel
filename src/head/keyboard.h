#ifndef KEYBOARD_H
#define KEYBOARD_H

void keyboard_init(void);
unsigned char keyboard_poll(void);
char keyboard_scancode_to_ascii(unsigned char scancode);

#endif
