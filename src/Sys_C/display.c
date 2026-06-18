#include "display.h"
#include "io.h"

static int current_mode = HARLIN_DISP_VGA_TEXT;

static void clear_03h(unsigned char color)
{
    unsigned short* vm = (unsigned short*)0xB8000;
    int i;
    for (i = 0; i < 80 * 25; i++) {
        vm[i] = ((unsigned short)color << 8) | ' ';
    }
}

static void putchar_03h(int x, int y, char c, unsigned char color)
{
    unsigned short* vm = (unsigned short*)0xB8000;
    if (x < 0 || x >= 80) return;
    if (y < 0 || y >= 25) return;
    vm[y * 80 + x] = ((unsigned short)color << 8) | (unsigned char)c;
}

static void putstring_03h(int x, int y, const char* str, unsigned char color)
{
    int px = x;
    int py = y;
    while (*str) {
        if (*str == '\n') {
            px = x;
            py++;
        } else {
            putchar_03h(px, py, *str, color);
            px++;
            if (px >= 80) { px = 0; py++; }
        }
        str++;
    }
}

int display_set_mode(int mode)
{
    if (mode == HARLIN_DISP_VGA_TEXT) {
        current_mode = mode;
        return 0;
    }
    if (mode == HARLIN_DISP_VGA_13H) {
        return -1;
    }
    if (mode == HARLIN_DISP_VESA) {
        return -1;
    }
    return -1;
}

void display_clear(unsigned char color)
{
    if (current_mode == HARLIN_DISP_VGA_TEXT) {
        clear_03h(color);
    }
}

void display_put_pixel(int x, int y, unsigned char color)
{
    if (current_mode == HARLIN_DISP_VGA_TEXT) {
        unsigned short* vm = (unsigned short*)0xB8000;
        if (x < 0 || x >= 80) return;
        if (y < 0 || y >= 25) return;
        vm[y * 80 + x] = ((unsigned short)color << 8) | 0xDB;
    }
}

void display_put_string(int x, int y, const char* str, unsigned char color)
{
    if (current_mode == HARLIN_DISP_VGA_TEXT) {
        putstring_03h(x, y, str, color);
    }
}
