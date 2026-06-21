#include "display.h"
#include "io.h"
#include "vmm.h"

static int current_mode = HARLIN_DISP_VGA_TEXT;
static int vesa_width = 0;
static int vesa_height = 0;
static int vesa_bpp = 0;
static unsigned long vesa_lfb = 0;

#define VESA_LFB_VIRTUAL 0xFFFF800000000000

#define VBE_MODE_INFO_ADDR 0x7000
#define VBE_MODE_INFO_XRES  0x12
#define VBE_MODE_INFO_YRES  0x14
#define VBE_MODE_INFO_BPP   0x13
#define VBE_MODE_INFO_LFB   0x22

static unsigned short read_mode_info_le16(int offset)
{
    unsigned char* p = (unsigned char*)VBE_MODE_INFO_ADDR;
    return (unsigned short)(p[offset] | (p[offset + 1] << 8));
}

static unsigned long read_mode_info_le32(int offset)
{
    unsigned char* p = (unsigned char*)VBE_MODE_INFO_ADDR;
    return (unsigned long)(p[offset] | (p[offset + 1] << 8) |
                           (p[offset + 2] << 16) | (p[offset + 3] << 24));
}

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

static void vga_set_palette(void);

static void vga_set_mode_text(void)
{
    int i;

    outb(0x3C2, 0x67);

    outb(0x3C4, 0x00); outb(0x3C5, 0x03);
    outb(0x3C4, 0x01); outb(0x3C5, 0x00);
    outb(0x3C4, 0x02); outb(0x3C5, 0x03);
    outb(0x3C4, 0x03); outb(0x3C5, 0x00);
    outb(0x3C4, 0x04); outb(0x3C5, 0x02);

    outb(0x3D4, 0x11);
    outb(0x3D5, inb(0x3D5) & 0x7F);

    outb(0x3D4, 0x00); outb(0x3D5, 0x5F);
    outb(0x3D4, 0x01); outb(0x3D5, 0x4F);
    outb(0x3D4, 0x02); outb(0x3D5, 0x50);
    outb(0x3D4, 0x03); outb(0x3D5, 0x82);
    outb(0x3D4, 0x04); outb(0x3D5, 0x55);
    outb(0x3D4, 0x05); outb(0x3D5, 0x81);
    outb(0x3D4, 0x06); outb(0x3D5, 0xBF);
    outb(0x3D4, 0x07); outb(0x3D5, 0x1F);
    outb(0x3D4, 0x08); outb(0x3D5, 0x00);
    outb(0x3D4, 0x09); outb(0x3D5, 0x4F);
    outb(0x3D4, 0x0A); outb(0x3D5, 0x0E);
    outb(0x3D4, 0x0B); outb(0x3D5, 0x0F);
    outb(0x3D4, 0x0C); outb(0x3D5, 0x00);
    outb(0x3D4, 0x0D); outb(0x3D5, 0x00);
    outb(0x3D4, 0x0E); outb(0x3D5, 0x07);
    outb(0x3D4, 0x0F); outb(0x3D5, 0x80);
    outb(0x3D4, 0x10); outb(0x3D5, 0x9C);
    outb(0x3D4, 0x11); outb(0x3D5, 0x8E);
    outb(0x3D4, 0x12); outb(0x3D5, 0x8F);
    outb(0x3D4, 0x13); outb(0x3D5, 0x28);
    outb(0x3D4, 0x14); outb(0x3D5, 0x1F);
    outb(0x3D4, 0x15); outb(0x3D5, 0x96);
    outb(0x3D4, 0x16); outb(0x3D5, 0xB9);
    outb(0x3D4, 0x17); outb(0x3D5, 0xA3);
    outb(0x3D4, 0x18); outb(0x3D5, 0xFF);

    outb(0x3CE, 0x00); outb(0x3CF, 0x00);
    outb(0x3CE, 0x01); outb(0x3CF, 0x00);
    outb(0x3CE, 0x02); outb(0x3CF, 0x00);
    outb(0x3CE, 0x03); outb(0x3CF, 0x00);
    outb(0x3CE, 0x04); outb(0x3CF, 0x00);
    outb(0x3CE, 0x05); outb(0x3CF, 0x10);
    outb(0x3CE, 0x06); outb(0x3CF, 0x0E);
    outb(0x3CE, 0x07); outb(0x3CF, 0x00);
    outb(0x3CE, 0x08); outb(0x3CF, 0xFF);

    inb(0x3DA);
    for (i = 0; i < 16; i++) {
        outb(0x3C0, (u8)i);
        outb(0x3C0, (u8)i);
    }
    outb(0x3C0, 0x10); outb(0x3C0, 0x0C);
    outb(0x3C0, 0x11); outb(0x3C0, 0x00);
    outb(0x3C0, 0x12); outb(0x3C0, 0x0F);
    outb(0x3C0, 0x13); outb(0x3C0, 0x08);
    outb(0x3C0, 0x14); outb(0x3C0, 0x00);
    outb(0x3C0, 0x20);
}

static void vga_set_mode_13h(void)
{
    outb(0x3C2, 0x63);
    outb(0x3D4, 0x00); outb(0x3D5, 0x5F);
    outb(0x3D4, 0x01); outb(0x3D5, 0x4F);
    outb(0x3D4, 0x02); outb(0x3D5, 0x50);
    outb(0x3D4, 0x03); outb(0x3D5, 0x82);
    outb(0x3D4, 0x04); outb(0x3D5, 0x54);
    outb(0x3D4, 0x05); outb(0x3D5, 0x80);
    outb(0x3D4, 0x06); outb(0x3D5, 0xBF);
    outb(0x3D4, 0x07); outb(0x3D5, 0x1F);
    outb(0x3D4, 0x08); outb(0x3D5, 0x00);
    outb(0x3D4, 0x09); outb(0x3D5, 0x41);
    outb(0x3D4, 0x10); outb(0x3D5, 0x9C);
    outb(0x3D4, 0x11); outb(0x3D5, 0x0E);
    outb(0x3D4, 0x12); outb(0x3D5, 0x8F);
    outb(0x3D4, 0x13); outb(0x3D5, 0x28);
    outb(0x3D4, 0x14); outb(0x3D5, 0x1F);
    outb(0x3D4, 0x15); outb(0x3D5, 0x96);
    outb(0x3D4, 0x16); outb(0x3D5, 0xB9);
    outb(0x3D4, 0x17); outb(0x3D5, 0xA3);
    outb(0x3D4, 0x18); outb(0x3D5, 0xFF);
    outb(0x3C4, 0x01); outb(0x3C5, 0x01);
    outb(0x3C4, 0x02); outb(0x3C5, 0x0F);
    outb(0x3C4, 0x04); outb(0x3C5, 0x0E);
    outb(0x3CE, 0x05); outb(0x3CF, 0x40);
    outb(0x3CE, 0x06); outb(0x3CF, 0x05);
    outb(0x3C4, 0x00); outb(0x3C5, 0x03);
    outb(0x3C0, 0x10); outb(0x3C0, 0x41);
    outb(0x3C0, 0x13); outb(0x3C0, 0x00);
    outb(0x3C6, 0xFF);
    vga_set_palette();
}

static void vga_set_palette(void)
{
    static const unsigned char colors[16][3] = {
        {0, 0, 0}, {0, 0, 42}, {0, 42, 0}, {0, 42, 42},
        {42, 0, 0}, {42, 0, 42}, {42, 21, 0}, {42, 42, 42},
        {21, 21, 21}, {21, 21, 63}, {21, 63, 21}, {21, 63, 63},
        {63, 21, 21}, {63, 21, 63}, {63, 63, 21}, {63, 63, 63}
    };
    int i, j;

    outb(0x3C8, 0);
    for (i = 0; i < 16; i++) {
        for (j = 0; j < 3; j++) {
            outb(0x3C9, colors[i][j]);
        }
    }
    for (i = 16; i < 256; i++) {
        outb(0x3C9, (unsigned char)i >> 2);
        outb(0x3C9, (unsigned char)i >> 2);
        outb(0x3C9, (unsigned char)i >> 2);
    }
}

static int vesa_set_mode(void)
{
    unsigned long lfb;
    int width, height, bpp;

    lfb = read_mode_info_le32(VBE_MODE_INFO_LFB);
    if (!lfb)
        return -1;

    width = (int)read_mode_info_le16(VBE_MODE_INFO_XRES);
    height = (int)read_mode_info_le16(VBE_MODE_INFO_YRES);
    bpp = (int)((unsigned char*)VBE_MODE_INFO_ADDR)[VBE_MODE_INFO_BPP];

    if (bpp != 24 && bpp != 32)
        return -1;
    if (width <= 0 || height <= 0)
        return -1;
    if (lfb >= 0x100000000)
        return -1;

    vmm_map(VESA_LFB_VIRTUAL, lfb, VMM_PRESENT | VMM_WRITABLE);

    vesa_lfb = VESA_LFB_VIRTUAL;
    vesa_width = width;
    vesa_height = height;
    vesa_bpp = bpp;
    return 0;
}

int display_set_mode(int mode)
{
    switch (mode) {
        case HARLIN_DISP_VGA_TEXT:
            vga_set_mode_text();
            current_mode = mode;
            return 0;
        case HARLIN_DISP_VGA_13H:
            vga_set_mode_13h();
            current_mode = mode;
            return 0;
        case HARLIN_DISP_VESA:
            if (vesa_set_mode() != 0) {
                current_mode = HARLIN_DISP_VGA_TEXT;
                return -1;
            }
            current_mode = mode;
            return 0;
        default:
            return -1;
    }
}

void display_clear(unsigned char color)
{
    if (current_mode == HARLIN_DISP_VGA_TEXT) {
        clear_03h(color);
    } else if (current_mode == HARLIN_DISP_VGA_13H) {
        unsigned char* vga = (unsigned char*)0xA0000;
        int i;
        for (i = 0; i < 320 * 200; i++) {
            vga[i] = color;
        }
    } else if (current_mode == HARLIN_DISP_VESA) {
        int i;
        int count = vesa_width * vesa_height;
        if (vesa_bpp == 32) {
            unsigned int* lfb = (unsigned int*)vesa_lfb;
            unsigned int c = ((unsigned int)color << 16) | ((unsigned int)color << 8) | color;
            for (i = 0; i < count; i++) {
                lfb[i] = c;
            }
        } else {
            unsigned char* lfb = (unsigned char*)vesa_lfb;
            for (i = 0; i < count; i++) {
                lfb[i * 3] = color;
                lfb[i * 3 + 1] = color;
                lfb[i * 3 + 2] = color;
            }
        }
    }
}

void display_put_pixel(int x, int y, unsigned char color)
{
    if (current_mode == HARLIN_DISP_VGA_TEXT) {
        unsigned short* vm = (unsigned short*)0xB8000;
        if (x < 0 || x >= 80) return;
        if (y < 0 || y >= 25) return;
        vm[y * 80 + x] = ((unsigned short)color << 8) | 0xDB;
    } else if (current_mode == HARLIN_DISP_VGA_13H) {
        unsigned char* vga = (unsigned char*)0xA0000;
        if (x < 0 || x >= 320) return;
        if (y < 0 || y >= 200) return;
        vga[y * 320 + x] = color;
    } else if (current_mode == HARLIN_DISP_VESA) {
        if (x < 0 || x >= vesa_width) return;
        if (y < 0 || y >= vesa_height) return;
        if (vesa_bpp == 32) {
            unsigned int* lfb = (unsigned int*)vesa_lfb;
            lfb[y * vesa_width + x] = ((unsigned int)color << 16) | ((unsigned int)color << 8) | color;
        } else {
            unsigned char* lfb = (unsigned char*)vesa_lfb;
            int off = (y * vesa_width + x) * 3;
            lfb[off] = color;
            lfb[off + 1] = color;
            lfb[off + 2] = color;
        }
    }
}

void display_put_string(int x, int y, const char* str, unsigned char color)
{
    if (current_mode == HARLIN_DISP_VGA_TEXT) {
        putstring_03h(x, y, str, color);
    }
}
