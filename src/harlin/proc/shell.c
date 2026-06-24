#include "display.h"
#include "keyboard.h"
#include "harlin_API.h"
#include "io.h"

#define COLOR_BLACK  0
#define COLOR_BLUE   1
#define COLOR_GRAY   8
#define COLOR_WHITE  15

#define TITLE_BAR_H 16
#define WIN_X 4
#define WIN_Y 4
#define WIN_W 312
#define WIN_H 192
#define OUTPUT_X 8
#define OUTPUT_Y 24
#define OUTPUT_W 304
#define OUTPUT_H 152
#define INPUT_Y 184

#define CMD_BUF_SIZE 128

static char cmd_buf[CMD_BUF_SIZE];
static int cmd_len = 0;
static int cursor_blink = 0;

static void put_string_color(int x, int y, const char* s, u32 fg, u32 bg)
{
    display_draw_string(x, y, s, fg, bg);
}

static int str_eq(const char* a, const char* b)
{
    while (*a && *b) {
        if (*a != *b) return 0;
        a++; b++;
    }
    return *a == 0 && *b == 0;
}

static void sys_print(const char* s)
{
    static int line = 0;
    int max_lines = OUTPUT_H / 16;
    if (line >= max_lines) {
        display_draw_rect(OUTPUT_X, OUTPUT_Y, OUTPUT_W, OUTPUT_H, COLOR_WHITE);
        line = 0;
    }
    put_string_color(OUTPUT_X, OUTPUT_Y + line * 16, s, COLOR_GRAY, COLOR_WHITE);
    line++;
}

static void draw_title_bar(void)
{
    display_draw_rect(WIN_X, WIN_Y, WIN_W, TITLE_BAR_H, COLOR_BLUE);
    put_string_color(WIN_X + 8, WIN_Y + 0, "HarLin OS Shell v1.0", COLOR_WHITE, COLOR_BLUE);
}

static void draw_window(void)
{
    display_draw_rect(WIN_X, WIN_Y + TITLE_BAR_H, WIN_W, WIN_H - TITLE_BAR_H, COLOR_WHITE);
    display_draw_rect(OUTPUT_X, OUTPUT_Y, OUTPUT_W, 2, COLOR_GRAY);
}

static void draw_prompt(void)
{
    char buf[CMD_BUF_SIZE + 4];
    int i;
    buf[0] = '>';
    buf[1] = ' ';
    for (i = 0; i < cmd_len && i < CMD_BUF_SIZE; i++) {
        buf[2 + i] = cmd_buf[i];
    }
    buf[2 + i] = 0;
    display_draw_rect(OUTPUT_X, INPUT_Y, OUTPUT_W, 16, COLOR_WHITE);
    put_string_color(OUTPUT_X, INPUT_Y, buf, COLOR_BLACK, COLOR_WHITE);
    if (cursor_blink) {
        display_draw_rect(OUTPUT_X + (2 + cmd_len) * 8, INPUT_Y, 8, 16, COLOR_BLACK);
    }
}

static void show_banner(void)
{
    sys_print("HarLin OS Shell v1.0");
    sys_print("Type 'help' for commands.");
    sys_print("");
}

static void cmd_help(void)
{
    sys_print("Commands:");
    sys_print("  help   - show this help");
    sys_print("  about  - about HarLin OS");
    sys_print("  clear  - clear screen");
    sys_print("  info   - system info");
    sys_print("  beep   - play a beep");
    sys_print("  halt   - halt system");
}

static void cmd_about(void)
{
    sys_print("HarLin OS");
    sys_print("A minimal x86_64 operating system");
    sys_print("Kernel: ELF format");
    sys_print("Display: VGA 13h (320x200 256-color)");
}

static void cmd_info(void)
{
    sys_print("CPU: x86_64");
    sys_print("Mode: Long mode (64-bit)");
    sys_print("Display: 320x200 256 colors");
    sys_print("HarLinAPI: v1.0");
}

static void cmd_beep(void)
{
    Harlin_Beep(440, 100);
    sys_print("Beep!");
}

static void cmd_halt(void)
{
    sys_print("Halting...");
    Harlin_Shutdown();
}

static void execute_command(void)
{
    if (cmd_len == 0) return;

    sys_print(cmd_buf);

    if (str_eq(cmd_buf, "help")) {
        cmd_help();
    } else if (str_eq(cmd_buf, "about")) {
        cmd_about();
    } else if (str_eq(cmd_buf, "info")) {
        cmd_info();
    } else if (str_eq(cmd_buf, "clear")) {
        display_draw_rect(OUTPUT_X, OUTPUT_Y, OUTPUT_W, OUTPUT_H - 2, COLOR_WHITE);
    } else if (str_eq(cmd_buf, "beep")) {
        cmd_beep();
    } else if (str_eq(cmd_buf, "halt")) {
        cmd_halt();
    } else {
        sys_print("Unknown command. Type 'help'.");
    }
    sys_print("");

    cmd_len = 0;
}

static char read_key(void)
{
    u8 sc;
    char c;
    if (!keyboard_has_data()) return 0;
    sc = keyboard_poll();
    if (sc == 0) return 0;
    c = keyboard_scancode_to_ascii(sc);
    return c;
}

void Harlin_Shell(void)
{
    Harlin_SetMode(HARLIN_DISP_VGA_13H);
    display_clear(COLOR_BLACK);
    draw_title_bar();
    draw_window();
    show_banner();
    draw_prompt();

    for (;;) {
        char c = read_key();
        if (c) {
            if (c == '\n' || c == '\r') {
                execute_command();
                draw_prompt();
            } else if (c == '\b') {
                if (cmd_len > 0) {
                    cmd_len--;
                    draw_prompt();
                }
            } else if (cmd_len < CMD_BUF_SIZE - 1) {
                cmd_buf[cmd_len++] = c;
                cmd_buf[cmd_len] = 0;
                draw_prompt();
            }
        } else {
            cursor_blink = !cursor_blink;
            draw_prompt();
            for (volatile int i = 0; i < 1000000; i++);
        }
    }
}
