#include "io.h"

#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 25
#define VIDEO_MEMORY ((unsigned short*)0xB8000)

static int cursor_x = 0;
static int cursor_y = 0;

static void screen_scroll(void)
{
    int i;
    for (i = 0; i < (SCREEN_HEIGHT - 1) * SCREEN_WIDTH; i++) {
        VIDEO_MEMORY[i] = VIDEO_MEMORY[i + SCREEN_WIDTH];
    }
    for (i = (SCREEN_HEIGHT - 1) * SCREEN_WIDTH; i < SCREEN_HEIGHT * SCREEN_WIDTH; i++) {
        VIDEO_MEMORY[i] = (0x07 << 8) | ' ';
    }
}

static void update_cursor(void)
{
    unsigned short pos = cursor_y * SCREEN_WIDTH + cursor_x;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (unsigned char)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (unsigned char)((pos >> 8) & 0xFF));
}

void screen_clear(void)
{
    int i;
    for (i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
        VIDEO_MEMORY[i] = (0x07 << 8) | ' ';
    }
    cursor_x = 0;
    cursor_y = 0;
    update_cursor();
}

void screen_put_char(char c)
{
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
        if (cursor_y >= SCREEN_HEIGHT) {
            cursor_y = SCREEN_HEIGHT - 1;
            screen_scroll();
        }
    } else if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x--;
            VIDEO_MEMORY[cursor_y * SCREEN_WIDTH + cursor_x] = (0x07 << 8) | ' ';
        }
    } else {
        VIDEO_MEMORY[cursor_y * SCREEN_WIDTH + cursor_x] = (0x0F << 8) | c;
        cursor_x++;
        if (cursor_x >= SCREEN_WIDTH) {
            cursor_x = 0;
            cursor_y++;
            if (cursor_y >= SCREEN_HEIGHT) {
                cursor_y = SCREEN_HEIGHT - 1;
                screen_scroll();
            }
        }
    }
    update_cursor();
}
