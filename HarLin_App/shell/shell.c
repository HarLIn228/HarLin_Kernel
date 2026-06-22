#include "harlin.h"

#define SHELL_WIN_X 8
#define SHELL_WIN_Y 8
#define SHELL_WIN_W 304
#define SHELL_WIN_H 184
#define SHELL_TITLE_H 16
#define SHELL_STATUS_H 16
#define SHELL_INPUT_H 16
#define SHELL_OUT_Y   (SHELL_WIN_Y + SHELL_TITLE_H)
#define SHELL_OUT_H   (SHELL_WIN_H - SHELL_TITLE_H - SHELL_STATUS_H - SHELL_INPUT_H)
#define SHELL_INPUT_Y (SHELL_WIN_Y + SHELL_WIN_H - SHELL_STATUS_H - SHELL_INPUT_H)
#define SHELL_STATUS_Y (SHELL_WIN_Y + SHELL_WIN_H - SHELL_STATUS_H)
#define SHELL_CHAR_W  8
#define SHELL_CHAR_H  16
#define SHELL_COLS    (SHELL_WIN_W / SHELL_CHAR_W)
#define SHELL_ROWS    (SHELL_OUT_H / SHELL_CHAR_H)
#define SHELL_BUF     128

#define COL_BLACK     0
#define COL_BLUE      1
#define COL_WHITE     15
#define COL_GRAY      8
#define COL_DARKGRAY  7
#define COL_LIGHTGRAY 7
#define COL_TITLEFG   15
#define COL_BORDER    0
#define COL_STATUSBG  7
#define COL_STATUSFG  0
#define COL_INPUFG    0
#define COL_OUTFG     8

static char out_buf[SHELL_ROWS][SHELL_COLS + 2];
static int  out_rows;
static int  out_scroll;
static char in_buf[SHELL_BUF];
static int  in_len;
static int  in_pos;

static int str_len(const char* s)
{
    int n = 0;
    while (s[n]) n++;
    return n;
}

static void gui_clear_region(int x, int y, int w, int h, int color)
{
    harlin_draw_rect(x, y, w, h, (harlin_u32)color);
}

static void gui_draw_text(int x, int y, const char* s, int fg, int bg)
{
    harlin_draw_string(x, y, s, (harlin_u32)fg, (harlin_u32)bg);
}

static void gui_draw_char_at(int x, int y, char c, int fg, int bg)
{
    harlin_draw_char(x, y, c, (harlin_u32)fg, (harlin_u32)bg);
}

static void gui_redraw_output(void)
{
    int r;
    int x = SHELL_WIN_X + 4;
    int y = SHELL_OUT_Y;
    gui_clear_region(x, y, SHELL_WIN_W - 8, SHELL_OUT_H, COL_WHITE);
    for (r = 0; r < SHELL_ROWS; r++) {
        int idx = r + out_scroll;
        if (idx < 0 || idx >= out_rows) continue;
        gui_draw_text(x, y + r * SHELL_CHAR_H, out_buf[idx], COL_OUTFG, COL_WHITE);
    }
}

static void gui_redraw_status(void)
{
    char timebuf[32];
    struct harlin_rtc_time t;
    int x = SHELL_WIN_X;
    harlin_time(&t);
    timebuf[0] = '0' + (t.hour >> 4);
    timebuf[1] = '0' + (t.hour & 0xF);
    timebuf[2] = ':';
    timebuf[3] = '0' + (t.minute >> 4);
    timebuf[4] = '0' + (t.minute & 0xF);
    timebuf[5] = ':';
    timebuf[6] = '0' + (t.second >> 4);
    timebuf[7] = '0' + (t.second & 0xF);
    timebuf[8] = 0;
    gui_clear_region(x, SHELL_STATUS_Y, SHELL_WIN_W, SHELL_STATUS_H, COL_STATUSBG);
    gui_draw_text(x + 4, SHELL_STATUS_Y, timebuf, COL_STATUSFG, COL_STATUSBG);
    {
        int tw = 0;
        int i = 0;
        const char* label = "HarLin v1.0";
        while (label[tw]) tw++;
        tw *= SHELL_CHAR_W;
        gui_draw_text(x + SHELL_WIN_W - tw - 4, SHELL_STATUS_Y, label, COL_STATUSFG, COL_STATUSBG);
        (void)i;
    }
}

static void gui_redraw_input(void)
{
    int x = SHELL_WIN_X;
    int y = SHELL_INPUT_Y;
    int i;
    int prompt_cols = 2;
    gui_clear_region(x, y, SHELL_WIN_W, SHELL_INPUT_H, COL_WHITE);
    gui_draw_text(x + 4, y, "> ", COL_INPUFG, COL_WHITE);
    for (i = 0; i < in_len; i++) {
        if (i < SHELL_COLS - prompt_cols)
            gui_draw_char_at(x + 4 + (i + prompt_cols) * SHELL_CHAR_W, y, in_buf[i], COL_INPUFG, COL_WHITE);
    }
    {
        int cur_col = in_pos + prompt_cols;
        if (cur_col < SHELL_COLS)
            gui_draw_char_at(x + 4 + cur_col * SHELL_CHAR_W, y, '_', COL_INPUFG, COL_WHITE);
    }
}

static void gui_redraw_title(void)
{
    const char* title = "HarLin Shell";
    int tw = str_len(title) * SHELL_CHAR_W;
    int tx = SHELL_WIN_X + (SHELL_WIN_W - tw) / 2;
    gui_clear_region(SHELL_WIN_X, SHELL_WIN_Y, SHELL_WIN_W, SHELL_TITLE_H, COL_BLUE);
    gui_draw_text(tx, SHELL_WIN_Y, title, COL_TITLEFG, COL_BLUE);
}

static void gui_redraw_border(void)
{
    harlin_draw_rect(SHELL_WIN_X - 1, SHELL_WIN_Y - 1, SHELL_WIN_W + 2, 1, (harlin_u32)COL_BORDER);
    harlin_draw_rect(SHELL_WIN_X - 1, SHELL_WIN_Y + SHELL_WIN_H, SHELL_WIN_W + 2, 1, (harlin_u32)COL_BORDER);
    harlin_draw_rect(SHELL_WIN_X - 1, SHELL_WIN_Y - 1, 1, SHELL_WIN_H + 2, (harlin_u32)COL_BORDER);
    harlin_draw_rect(SHELL_WIN_X + SHELL_WIN_W, SHELL_WIN_Y - 1, 1, SHELL_WIN_H + 2, (harlin_u32)COL_BORDER);
}

static void gui_redraw_all(void)
{
    gui_redraw_title();
    gui_redraw_output();
    gui_redraw_input();
    gui_redraw_status();
    gui_redraw_border();
}

static void shell_out_push(const char* s)
{
    int row = 0;
    int col = 0;
    int len;
    int i;
    if (!s) return;
    len = str_len(s);
    for (i = 0; i < len; i++) {
        char c = s[i];
        if (c == '\n' || col >= SHELL_COLS) {
            while (col < SHELL_COLS) out_buf[out_rows][col++] = ' ';
            out_buf[out_rows][col] = 0;
            out_rows++;
            if (out_rows >= SHELL_ROWS) {
                int k;
                for (k = 0; k < SHELL_ROWS - 1; k++) {
                    int j;
                    for (j = 0; j < SHELL_COLS; j++) out_buf[k][j] = out_buf[k + 1][j];
                    out_buf[k][SHELL_COLS] = 0;
                }
                out_rows = SHELL_ROWS - 1;
            }
            col = 0;
            if (c == '\n') continue;
        }
        out_buf[out_rows][col++] = c;
    }
    if (col > 0 || out_rows == 0) {
        while (col < SHELL_COLS) out_buf[out_rows][col++] = ' ';
        out_buf[out_rows][col] = 0;
        out_rows++;
        if (out_rows >= SHELL_ROWS) {
            int k;
            for (k = 0; k < SHELL_ROWS - 1; k++) {
                int j;
                for (j = 0; j < SHELL_COLS; j++) out_buf[k][j] = out_buf[k + 1][j];
                out_buf[k][SHELL_COLS] = 0;
            }
            out_rows = SHELL_ROWS - 1;
        }
    }
    (void)row;
}

static void shell_out(const char* s)
{
    shell_out_push(s);
    gui_redraw_output();
}

static int match_key(const char* line, const char* prefix)
{
    int i = 0;
    while (prefix[i] && line[i] == prefix[i]) i++;
    return prefix[i] == 0 && (line[i] == 0 || line[i] == ' ');
}

static void cmd_help(void)
{
    shell_out("Commands:");
    shell_out("  help     - show this help");
    shell_out("  about    - about HarLin");
    shell_out("  time     - show time");
    shell_out("  cpu      - current CPU id");
    shell_out("  pid      - process id");
    shell_out("  cls      - clear screen");
    shell_out("  echo ... - print text");
    shell_out("  exit     - return to kernel");
}

static void cmd_about(void)
{
    shell_out("HarLin Kernel v1.0");
    shell_out("A small x86_64 hobby OS");
}

static void cmd_time(void)
{
    struct harlin_rtc_time t;
    char buf[32];
    harlin_time(&t);
    buf[0] = '0' + (t.hour >> 4);
    buf[1] = '0' + (t.hour & 0xF);
    buf[2] = ':';
    buf[3] = '0' + (t.minute >> 4);
    buf[4] = '0' + (t.minute & 0xF);
    buf[5] = ':';
    buf[6] = '0' + (t.second >> 4);
    buf[7] = '0' + (t.second & 0xF);
    buf[8] = 0;
    shell_out(buf);
}

static void cmd_cpu(void)
{
    char buf[16];
    int n = harlin_getcpu();
    int i = 0;
    if (n == 0) {
        shell_out("0");
        return;
    }
    while (n > 0 && i < 14) {
        buf[i++] = '0' + (n % 10);
        n /= 10;
    }
    {
        char rev[16];
        int j;
        for (j = 0; j < i; j++) rev[j] = buf[i - 1 - j];
        rev[i] = 0;
        shell_out(rev);
    }
}

static void cmd_pid(void)
{
    int n = harlin_getpid();
    char buf[16];
    int i = 0;
    if (n == 0) {
        shell_out("0");
        return;
    }
    while (n > 0 && i < 14) {
        buf[i++] = '0' + (n % 10);
        n /= 10;
    }
    {
        char rev[16];
        int j;
        for (j = 0; j < i; j++) rev[j] = buf[i - 1 - j];
        rev[i] = 0;
        shell_out(rev);
    }
}

static void cmd_echo(const char* s)
{
    while (*s == ' ') s++;
    shell_out(s);
}

static void cmd_run(const char* line)
{
    if (match_key(line, "help"))   cmd_help();
    else if (match_key(line, "about"))  cmd_about();
    else if (match_key(line, "time"))   cmd_time();
    else if (match_key(line, "cpu"))    cmd_cpu();
    else if (match_key(line, "pid"))    cmd_pid();
    else if (match_key(line, "cls")) {
        out_rows = 0;
        gui_redraw_output();
    }
    else if (match_key(line, "echo"))   cmd_echo(line + 4);
    else if (match_key(line, "exit"))   harlin_exit(0);
    else if (line[0] == 0) ;
    else {
        char msg[SHELL_COLS + 1];
        const char* p = "unknown: ";
        int i = 0;
        while (*p) msg[i++] = *p++;
        for (p = line; *p && i < SHELL_COLS; p++) msg[i++] = *p;
        msg[i] = 0;
        shell_out(msg);
    }
}

static void show_prompt(void)
{
    gui_redraw_input();
}

static void input_backspace(void)
{
    if (in_pos > 0) {
        int i;
        for (i = in_pos; i < in_len; i++) in_buf[i - 1] = in_buf[i];
        in_len--;
        in_pos--;
        in_buf[in_len] = 0;
    }
}

static void input_insert(char c)
{
    if (in_len < SHELL_BUF - 1) {
        int i;
        for (i = in_len; i > in_pos; i--) in_buf[i] = in_buf[i - 1];
        in_buf[in_pos] = c;
        in_len++;
        in_pos++;
        in_buf[in_len] = 0;
    }
}

static void input_commit(void)
{
    char echo[SHELL_COLS + 1];
    int i;
    for (i = 0; i < in_len; i++) echo[i] = in_buf[i];
    echo[in_len] = 0;
    shell_out(echo);
    cmd_run(in_buf);
    in_len = 0;
    in_pos = 0;
    in_buf[0] = 0;
    gui_redraw_input();
    gui_redraw_status();
}

void _start(void)
{
    harlin_set_mode(HARLIN_MODE_13H);
    harlin_clear_screen(7);
    in_len = 0;
    in_pos = 0;
    in_buf[0] = 0;
    out_rows = 0;
    out_scroll = 0;
    gui_redraw_all();
    shell_out("HarLin Shell v1.0 ready.");
    shell_out("Type 'help' for commands.");
    show_prompt();
    {
        int last_sec = -1;
        for (;;) {
            char c = harlin_keypoll();
            if (c == 0) {
                struct harlin_rtc_time t;
                harlin_time(&t);
                if (t.second != last_sec) {
                    last_sec = t.second;
                    gui_redraw_status();
                }
                harlin_yield();
                continue;
            }
            if (c == '\r' || c == '\n') {
                input_commit();
            } else if (c == '\b' || c == 0x7F) {
                input_backspace();
                gui_redraw_input();
            } else if (c >= 32 && c < 127) {
                input_insert(c);
                gui_redraw_input();
            }
        }
    }
}
