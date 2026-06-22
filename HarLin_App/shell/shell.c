#include "harlin.h"

#define SHELL_BUF_SIZE 256
#define SHELL_ARGS_MAX 16
#define SHELL_PROMPT  "HarLin> "

static void shell_print(const char* s)
{
    harlin_print(s);
}

static void shell_print_dec(harlin_u64 val)
{
    char buf[21];
    int pos = 20;
    buf[20] = '\0';
    if (val == 0) {
        shell_print("0");
        return;
    }
    while (val > 0 && pos > 0) {
        pos--;
        buf[pos] = '0' + (val % 10);
        val /= 10;
    }
    shell_print(&buf[pos]);
}

static int shell_read_line(char* buf, int max)
{
    int pos = 0;
    char c;
    for (;;) {
        c = harlin_getc();
        if (c == '\r' || c == '\n') {
            buf[pos] = '\0';
            shell_print("\r\n");
            return pos;
        }
        if (c == '\b' || c == 0x7F) {
            if (pos > 0) {
                pos--;
                shell_print("\b \b");
            }
            continue;
        }
        if (pos < max - 1) {
            buf[pos++] = c;
            { char tmp[2]; tmp[0] = c; tmp[1] = '\0'; shell_print(tmp); }
        }
    }
}

static int shell_parse(char* line, char** args)
{
    int count = 0;
    char* p = line;
    while (*p) {
        while (*p == ' ') p++;
        if (*p == '\0') break;
        args[count++] = p;
        while (*p && *p != ' ') p++;
        if (*p) { *p = '\0'; p++; }
        if (count >= SHELL_ARGS_MAX) break;
    }
    return count;
}

static int cmd_help(int argc, char** argv)
{
    (void)argc; (void)argv;
    shell_print("HarLin Shell commands:\r\n");
    shell_print("  help             显示帮助\r\n");
    shell_print("  say <text>       回显文本\r\n");
    shell_print("  end              退出Shell\r\n");
    shell_print("  exit             退出Shell\r\n");
    shell_print("  run <file>       运行CHC程序\r\n");
    shell_print("  exec <file>      运行CHC程序\r\n");
    shell_print("  pid              显示进程ID\r\n");
    shell_print("  beep <freq> <ms> 蜂鸣\r\n");
    shell_print("  sleep <ms>       休眠\r\n");
    shell_print("  time             显示时间\r\n");
    shell_print("  clearkeys        清除键盘缓冲区\r\n");
    return 0;
}

static int cmd_say(int argc, char** argv)
{
    int i;
    for (i = 1; i < argc; i++) {
        if (i > 1) shell_print(" ");
        shell_print(argv[i]);
    }
    shell_print("\r\n");
    return 0;
}

static int cmd_end(int argc, char** argv)
{
    (void)argc; (void)argv;
    shell_print("Shell exiting.\r\n");
    harlin_exit(0);
    return 0;
}

static int cmd_run(int argc, char** argv)
{
    if (argc < 2) {
        shell_print("Usage: run <file.chc>\r\n");
        return -1;
    }
    if (harlin_exec(argv[1]) < 0) {
        shell_print("Failed to run: ");
        shell_print(argv[1]);
        shell_print("\r\n");
        return -1;
    }
    return 0;
}

static int cmd_pid(int argc, char** argv)
{
    (void)argc; (void)argv;
    shell_print("PID: ");
    shell_print_dec((harlin_u64)harlin_getpid());
    shell_print("\r\n");
    return 0;
}

static int cmd_beep(int argc, char** argv)
{
    harlin_u64 freq = 1000, ms = 200;
    if (argc > 1) {
        freq = 0;
        while (*argv[1]) {
            freq = freq * 10 + (*argv[1] - '0');
            argv[1]++;
        }
    }
    if (argc > 2) {
        ms = 0;
        while (*argv[2]) {
            ms = ms * 10 + (*argv[2] - '0');
            argv[2]++;
        }
    }
    harlin_beep((harlin_u32)freq, (harlin_u32)ms);
    return 0;
}

static int cmd_sleep(int argc, char** argv)
{
    harlin_u64 ms = 1000;
    if (argc > 1) {
        ms = 0;
        while (*argv[1]) {
            ms = ms * 10 + (*argv[1] - '0');
            argv[1]++;
        }
    }
    harlin_sleep((harlin_u32)ms);
    return 0;
}

static int cmd_time(int argc, char** argv)
{
    struct harlin_rtc_time t;
    (void)argc; (void)argv;
    harlin_time(&t);
    shell_print_dec(t.year);
    shell_print("-");
    if (t.month < 10) shell_print("0");
    shell_print_dec(t.month);
    shell_print("-");
    if (t.day < 10) shell_print("0");
    shell_print_dec(t.day);
    shell_print(" ");
    if (t.hour < 10) shell_print("0");
    shell_print_dec(t.hour);
    shell_print(":");
    if (t.minute < 10) shell_print("0");
    shell_print_dec(t.minute);
    shell_print(":");
    if (t.second < 10) shell_print("0");
    shell_print_dec(t.second);
    shell_print("\r\n");
    return 0;
}

static int cmd_clearkeys(int argc, char** argv)
{
    (void)argc; (void)argv;
    while (harlin_getc() >= 0);
    shell_print("Keyboard buffer cleared.\r\n");
    return 0;
}

struct shell_cmd {
    const char* name;
    int (*handler)(int argc, char** argv);
};

static const struct shell_cmd commands[] = {
    {"help",      cmd_help},
    {"say",       cmd_say},
    {"end",       cmd_end},
    {"exit",      cmd_end},
    {"run",       cmd_run},
    {"exec",      cmd_run},
    {"pid",       cmd_pid},
    {"beep",      cmd_beep},
    {"sleep",     cmd_sleep},
    {"time",      cmd_time},
    {"clearkeys", cmd_clearkeys},
    {0, 0}
};

static int shell_dispatch(int argc, char** argv)
{
    int i;
    for (i = 0; commands[i].name; i++) {
        const char* a = commands[i].name;
        const char* b = argv[0];
        int match = 1;
        while (*a && *b) {
            if (*a != *b) { match = 0; break; }
            a++; b++;
        }
        if (match && *a == '\0' && *b == '\0') {
            return commands[i].handler(argc, argv);
        }
    }
    shell_print("Unknown command: ");
    shell_print(argv[0]);
    shell_print("\r\n");
    return -1;
}

void _start(void)
{
    char line[SHELL_BUF_SIZE];
    char* args[SHELL_ARGS_MAX];
    int argc;

    shell_print("HarLin Shell v1.0\r\n");
    shell_print("Type 'help' for commands.\r\n");

    for (;;) {
        shell_print(SHELL_PROMPT);
        if (shell_read_line(line, SHELL_BUF_SIZE) <= 0)
            continue;
        argc = shell_parse(line, args);
        if (argc == 0)
            continue;
        shell_dispatch(argc, args);
    }
}
