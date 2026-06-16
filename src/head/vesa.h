#ifndef VESA_H
#define VESA_H

extern unsigned char* vga_framebuffer;

int vesa_init(void);
void vga_clear(unsigned char color);
void vga_putpixel(int x, int y, unsigned char color);
void vga_draw_char(int x, int y, char c, unsigned char color);
void vga_draw_string(int x, int y, const char* str, unsigned char color);

#endif
