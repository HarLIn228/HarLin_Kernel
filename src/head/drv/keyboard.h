#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "harlin_API.h"

#define KEY_STATE_SHIFT  0x01
#define KEY_STATE_CTRL   0x02
#define KEY_STATE_ALT    0x04
#define KEY_STATE_CAPS   0x08
#define KEY_STATE_NUM    0x10
#define KEY_STATE_SCROLL 0x20

#define KEY_LED_SCROLL   0x01
#define KEY_LED_NUM      0x02
#define KEY_LED_CAPS     0x04

void keyboard_init(void);
int keyboard_has_data(void);
unsigned char keyboard_poll(void);
char keyboard_scancode_to_ascii(unsigned char scancode);
int keyboard_overflow_count(void);
void keyboard_flush(void);
u8 keyboard_get_state(void);
void keyboard_set_leds(u8 leds);
int keyboard_set_scancode_set(u8 set);

#define Harlin_KeyboardInit           keyboard_init
#define Harlin_KeyboardHasData        keyboard_has_data
#define Harlin_KeyboardPoll           keyboard_poll
#define Harlin_KeyboardScancodeToAscii keyboard_scancode_to_ascii
#define Harlin_KeyboardOverflowCount  keyboard_overflow_count
#define Harlin_KeyboardFlush          keyboard_flush
#define Harlin_KeyboardGetState       keyboard_get_state
#define Harlin_KeyboardSetLeds        keyboard_set_leds
#define Harlin_KeyboardSetScancodeSet keyboard_set_scancode_set

#endif
