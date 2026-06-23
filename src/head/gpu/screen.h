#ifndef SCREEN_H
#define SCREEN_H

void screen_clear(void);
void screen_put_char(char c);
void screen_puts(const char* s);

#define Harlin_ScreenClear           screen_clear
#define Harlin_ScreenPutChar         screen_put_char
#define Harlin_ScreenPuts            screen_puts

#endif
