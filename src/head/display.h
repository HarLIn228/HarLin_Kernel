#ifndef DISPLAY_H
#define DISPLAY_H

#include "harlin_API.h"

#define HARLIN_DISP_VGA_TEXT 0
#define HARLIN_DISP_VGA_13H  1
#define HARLIN_DISP_VESA     2

int  display_set_mode(int mode);
void display_clear(unsigned char color);
void display_put_pixel(int x, int y, unsigned char color);
void display_put_string(int x, int y, const char* str, unsigned char color);

void display_draw_rect(int x, int y, int w, int h, u32 color);
void display_draw_char(int x, int y, char c, u32 fg, u32 bg);
void display_draw_string(int x, int y, const char* str, u32 fg, u32 bg);
u32  display_rgb(u8 r, u8 g, u8 b);
int  display_get_width(void);
int  display_get_height(void);

#endif
