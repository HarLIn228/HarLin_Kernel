#ifndef DISPLAY_H
#define DISPLAY_H

#define HARLIN_DISP_VGA_TEXT 0
#define HARLIN_DISP_VGA_13H  1
#define HARLIN_DISP_VESA     2

int  display_set_mode(int mode);
void display_clear(unsigned char color);
void display_put_pixel(int x, int y, unsigned char color);
void display_put_string(int x, int y, const char* str, unsigned char color);

#endif
