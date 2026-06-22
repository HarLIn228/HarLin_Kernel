#include "shell.h"
#include "harlin_API.h"
#include "chc_loader.h"
#include "scheduler.h"
#include "fat32.h"
#include "kmalloc.h"

#define SHELL_LINE_SIZE 256
#define SHELL_MAX_ARGS  16
#define SHELL_MAX_VARS  32
#define SHELL_MAX_CONSTS 16
#define SHELL_LABELS    32
#define SHELL_MAX_COMMANDS 64

struct shell_var {
    char name[32];
    char value[128];
};

struct shell_label {
    char name[32];
    int line;
};

static struct shell_var vars[SHELL_MAX_VARS];
static int var_count = 0;

static struct shell_var consts[SHELL_MAX_CONSTS];
static int const_count = 0;

static struct shell_label labels[SHELL_LABELS];
static int label_count = 0;

static char shell_script[4096];
static int shell_script_lines = 0;
static int shell_script_pos[SHELL_MAX_ARGS * 4];
static int shell_pc = 0;
static int shell_running = 0;

static char line_buf[SHELL_LINE_SIZE];
static char* argv[SHELL_MAX_ARGS];
static int argc;

static char current_dir[64] = "/";

static struct shell_command cmd_table[SHELL_MAX_COMMANDS];
static int cmd_count = 0;

int Harlin_ShellRegister(const struct shell_command* cmd)
{
    int i;
    if (!cmd || !cmd->name || !cmd->handler)
        return HARLIN_INVALID;
    if (cmd_count >= SHELL_MAX_COMMANDS)
        return HARLIN_NO_MEMORY;
    for (i = 0; i < cmd_count; i++) {
        if (Harlin_Compare(cmd_table[i].name, cmd->name) == 0)
            return HARLIN_INVALID;
    }
    cmd_table[cmd_count++] = *cmd;
    return HARLIN_OK;
}

static void shell_puts(const char* s)
{
    Harlin_Print(s);
}

static void shell_putln(const char* s)
{
    shell_puts(s);
    shell_puts("\n");
}

static int shell_atoi(const char* s)
{
    int r = 0;
    int sign = 1;
    if (*s == '-') {
        sign = -1;
        s++;
    }
    while (*s >= '0' && *s <= '9') {
        r = r * 10 + (*s - '0');
        s++;
    }
    return r * sign;
}

static void shell_itoa(int v, char* buf)
{
    int i = 0;
    int neg = 0;
    if (v < 0) {
        neg = 1;
        v = -v;
    }
    if (v == 0) {
        buf[0] = '0';
        buf[1] = 0;
        return;
    }
    while (v > 0) {
        buf[i++] = '0' + (v % 10);
        v /= 10;
    }
    if (neg)
        buf[i++] = '-';
    buf[i] = 0;
    for (int a = 0, b = i - 1; a < b; a++, b--) {
        char t = buf[a];
        buf[a] = buf[b];
        buf[b] = t;
    }
}

static int shell_find_var(const char* name)
{
    for (int i = 0; i < var_count; i++) {
        if (Harlin_Compare(vars[i].name, name) == 0)
            return i;
    }
    return -1;
}

static int shell_find_const(const char* name)
{
    for (int i = 0; i < const_count; i++) {
        if (Harlin_Compare(consts[i].name, name) == 0)
            return i;
    }
    return -1;
}

static const char* shell_get_value(const char* name)
{
    int i;
    if ((i = shell_find_var(name)) >= 0)
        return vars[i].value;
    if ((i = shell_find_const(name)) >= 0)
        return consts[i].value;
    return name;
}

static void shell_set_var(const char* name, const char* value)
{
    int i = shell_find_var(name);
    if (i >= 0) {
        Harlin_CopyStr(vars[i].value, value);
        return;
    }
    if (var_count >= SHELL_MAX_VARS) {
        shell_putln("variable table full");
        return;
    }
    Harlin_CopyStr(vars[var_count].name, name);
    Harlin_CopyStr(vars[var_count].value, value);
    var_count++;
}

static void shell_set_const(const char* name, const char* value)
{
    int i = shell_find_const(name);
    if (i >= 0) {
        shell_putln("constant already defined");
        return;
    }
    if (const_count >= SHELL_MAX_CONSTS) {
        shell_putln("constant table full");
        return;
    }
    Harlin_CopyStr(consts[const_count].name, name);
    Harlin_CopyStr(consts[const_count].value, value);
    const_count++;
}

static int shell_expand(const char* src, char* dst, int dst_size)
{
    int i = 0;
    int j = 0;
    while (src[i] && j < dst_size - 1) {
        if (src[i] == '$' && (src[i + 1] == '_' || (src[i + 1] >= 'a' && src[i + 1] <= 'z') || (src[i + 1] >= 'A' && src[i + 1] <= 'Z'))) {
            char name[32];
            int k = 0;
            i++;
            while ((src[i] == '_' || (src[i] >= 'a' && src[i] <= 'z') || (src[i] >= 'A' && src[i] <= 'Z') || (src[i] >= '0' && src[i] <= '9')) && k < 31) {
                name[k++] = src[i++];
            }
            name[k] = 0;
            const char* v = shell_get_value(name);
            while (*v && j < dst_size - 1)
                dst[j++] = *v++;
        } else {
            dst[j++] = src[i++];
        }
    }
    dst[j] = 0;
    return j;
}

static void shell_read_line(void)
{
    int i = 0;
    char c;
    shell_puts("harlin> ");
    while (i < SHELL_LINE_SIZE - 1) {
        c = Harlin_GetKey();
        if (c == '\n' || c == '\r') {
            shell_puts("\n");
            break;
        }
        if (c == '\b' && i > 0) {
            i--;
            shell_puts("\b \b");
            continue;
        }
        if (c >= 0x20 && c < 0x7F) {
            line_buf[i++] = c;
            Harlin_PutChar(c);
        }
    }
    line_buf[i] = 0;
}

static void shell_parse_args(void)
{
    int i = 0;
    int in_arg = 0;
    argc = 0;
    while (line_buf[i] && argc < SHELL_MAX_ARGS) {
        if (line_buf[i] == ' ' || line_buf[i] == '\t') {
            line_buf[i] = 0;
            in_arg = 0;
        } else if (!in_arg) {
            argv[argc++] = &line_buf[i];
            in_arg = 1;
        }
        i++;
    }
}

static int shell_find_label(const char* name)
{
    for (int i = 0; i < label_count; i++) {
        if (Harlin_Compare(labels[i].name, name) == 0)
            return labels[i].line;
    }
    return -1;
}

static int shell_file_copy(const char* src, const char* dst)
{
    struct Harlin_File sf, df;
    u8* buf;
    u32 size;
    int r;

    if (Harlin_Open(src, &sf) != HARLIN_FS_OK)
        return -1;
    size = Harlin_Size(&sf);
    buf = (u8*)Harlin_Kmalloc(size + 1);
    if (!buf) {
        Harlin_Close(&sf);
        return -1;
    }
    if (Harlin_Read(&sf, buf, size) != (int)size) {
        Harlin_Kfree(buf);
        Harlin_Close(&sf);
        return -1;
    }
    Harlin_Close(&sf);

    if (Harlin_Create(dst, &df) != HARLIN_FS_OK) {
        Harlin_Kfree(buf);
        return -1;
    }
    r = Harlin_Write(&df, buf, size);
    Harlin_Close(&df);
    Harlin_Kfree(buf);
    return (r == (int)size) ? 0 : -1;
}

static int shell_file_delete(const char* name)
{
    if (Harlin_DeleteFile(name) == HARLIN_FS_OK)
        return 0;
    return -1;
}

static int shell_run_program(const char* name)
{
    struct Harlin_File file;
    void* buf;
    u32 size;
    int r;
    char path[64];

    shell_expand(name, path, sizeof(path));

    if (Harlin_Open(path, &file) != HARLIN_FS_OK) {
        int len = Harlin_Len(path);
        if (len + 4 >= (int)sizeof(path))
            return -1;
        path[len] = '.';
        path[len + 1] = 'c';
        path[len + 2] = 'h';
        path[len + 3] = 'c';
        path[len + 4] = 0;
        if (Harlin_Open(path, &file) != HARLIN_FS_OK)
            return -1;
    }

    size = Harlin_Size(&file);
    if (size == 0 || size > 65536) {
        Harlin_Close(&file);
        return -1;
    }

    buf = (void*)Harlin_Kmalloc(size);
    if (!buf) {
        Harlin_Close(&file);
        return -1;
    }

    r = Harlin_Read(&file, buf, size);
    Harlin_Close(&file);
    if (r != (int)size) {
        Harlin_Kfree(buf);
        return -1;
    }

    r = chc_load(buf, size);
    Harlin_Kfree(buf);

    if (r >= 0) {
        scheduler_add_ready(r);
        shell_running = 0;
    }
    return r;
}

static int shell_cmd_say(void)
{
    char out[SHELL_LINE_SIZE];
    if (argc < 2) {
        shell_putln("usage: say <text>");
        return 0;
    }
    shell_expand(argv[1], out, sizeof(out));
    shell_putln(out);
    return 0;
}

static int shell_cmd_help(void)
{
    shell_putln("HarLin Shell commands:");
    shell_putln("  say <text>        print text");
    shell_putln("  copy <src> <dst>  copy file");
    shell_putln("  setup <name> ...  create file or variable");
    shell_putln("  delete <name>     delete file or variable");
    shell_putln("  cd <path>         change directory");
    shell_putln("  set <var> <val>   set variable");
    shell_putln("  variable <n> <v>  define variable");
    shell_putln("  constant <n> <v>  define constant");
    shell_putln("  if <v> <op> <v>   conditional");
    shell_putln("  else              conditional else");
    shell_putln("  calculate <v> <e> compute expression");
    shell_putln("  goto <label>      jump to label");
    shell_putln("  run <program>     execute .chc program");
    shell_putln("  end               end script or shell");
    return 0;
}

static int shell_cmd_copy(void)
{
    char src[64], dst[64];
    if (argc < 3) {
        shell_putln("usage: copy <src> <dst>");
        return 0;
    }
    shell_expand(argv[1], src, sizeof(src));
    shell_expand(argv[2], dst, sizeof(dst));
    if (shell_file_copy(src, dst) < 0)
        shell_putln("copy failed");
    return 0;
}

static int shell_cmd_setup(void)
{
    struct Harlin_File f;
    char name[64];
    if (argc < 2) {
        shell_putln("usage: setup <file> or setup <var> <value>");
        return 0;
    }
    shell_expand(argv[1], name, sizeof(name));
    if (argc >= 3) {
        char val[128];
        shell_expand(argv[2], val, sizeof(val));
        shell_set_var(name, val);
        return 0;
    }
    if (Harlin_Create(name, &f) == HARLIN_FS_OK) {
        Harlin_Close(&f);
    } else {
        shell_putln("setup failed");
    }
    return 0;
}

static int shell_cmd_delete(void)
{
    char name[64];
    if (argc < 2) {
        shell_putln("usage: delete <file|var>");
        return 0;
    }
    shell_expand(argv[1], name, sizeof(name));
    int i = shell_find_var(name);
    if (i >= 0) {
        for (int j = i; j < var_count - 1; j++)
            vars[j] = vars[j + 1];
        var_count--;
        return 0;
    }
    if (shell_file_delete(name) < 0)
        shell_putln("delete failed");
    return 0;
}

static int shell_cmd_end(void)
{
    shell_running = 0;
    return 0;
}

static int shell_cmd_start(void)
{
    var_count = 0;
    const_count = 0;
    label_count = 0;
    shell_pc = 0;
    shell_script_lines = 0;
    shell_putln("shell state reset");
    return 0;
}

static int shell_cmd_cd(void)
{
    if (argc < 2) {
        shell_putln(current_dir);
        return 0;
    }
    Harlin_CopyStr(current_dir, argv[1]);
    return 0;
}

static int shell_cmd_set(void)
{
    char val[128];
    if (argc < 3) {
        shell_putln("usage: set <var> <value>");
        return 0;
    }
    shell_expand(argv[2], val, sizeof(val));
    shell_set_var(argv[1], val);
    return 0;
}

static int shell_cmd_variable(void)
{
    char val[128];
    if (argc < 3) {
        shell_putln("usage: variable <name> <value>");
        return 0;
    }
    shell_expand(argv[2], val, sizeof(val));
    shell_set_var(argv[1], val);
    return 0;
}

static int shell_cmd_constant(void)
{
    char val[128];
    if (argc < 3) {
        shell_putln("usage: constant <name> <value>");
        return 0;
    }
    shell_expand(argv[2], val, sizeof(val));
    shell_set_const(argv[1], val);
    return 0;
}

static int shell_cmd_calculate(void)
{
    int a, b, r;
    char buf[32];
    if (argc < 4) {
        shell_putln("usage: calculate <var> <a> <op> <b>");
        return 0;
    }
    a = shell_atoi(shell_get_value(argv[2]));
    b = shell_atoi(shell_get_value(argv[4]));
    if (Harlin_Compare(argv[3], "+") == 0) r = a + b;
    else if (Harlin_Compare(argv[3], "-") == 0) r = a - b;
    else if (Harlin_Compare(argv[3], "*") == 0) r = a * b;
    else if (Harlin_Compare(argv[3], "/") == 0) r = b ? a / b : 0;
    else {
        shell_putln("unknown operator");
        return 0;
    }
    shell_itoa(r, buf);
    shell_set_var(argv[1], buf);
    return 0;
}

static int shell_cmd_if(void)
{
    int a, b;
    int cond = 0;
    if (argc < 5) {
        shell_putln("usage: if <var> <op> <val> : <command>");
        return 0;
    }
    a = shell_atoi(shell_get_value(argv[1]));
    b = shell_atoi(shell_get_value(argv[3]));
    if (Harlin_Compare(argv[2], "==") == 0) cond = (a == b);
    else if (Harlin_Compare(argv[2], "!=") == 0) cond = (a != b);
    else if (Harlin_Compare(argv[2], ">") == 0) cond = (a > b);
    else if (Harlin_Compare(argv[2], "<") == 0) cond = (a < b);
    else {
        shell_putln("unknown comparator");
        return 0;
    }

    if (!cond) {
        int depth = 1;
        while (shell_pc < shell_script_lines && depth > 0) {
            shell_pc++;
            Harlin_CopyStr(line_buf, shell_script + shell_script_pos[shell_pc]);
            shell_parse_args();
            if (argc > 0) {
                if (Harlin_Compare(argv[0], "if") == 0) depth++;
                else if (Harlin_Compare(argv[0], "else") == 0 && depth == 1) break;
                else if (Harlin_Compare(argv[0], "end") == 0) depth--;
            }
        }
    }
    return 0;
}

static int shell_cmd_else(void)
{
    int depth = 1;
    while (shell_pc < shell_script_lines && depth > 0) {
        shell_pc++;
        Harlin_CopyStr(line_buf, shell_script + shell_script_pos[shell_pc]);
        shell_parse_args();
        if (argc > 0) {
            if (Harlin_Compare(argv[0], "if") == 0) depth++;
            else if (Harlin_Compare(argv[0], "end") == 0) depth--;
        }
    }
    return 0;
}

static int shell_cmd_goto(void)
{
    int line;
    if (argc < 2) {
        shell_putln("usage: goto <label>");
        return 0;
    }
    line = shell_find_label(argv[1]);
    if (line < 0) {
        shell_putln("label not found");
        return 0;
    }
    shell_pc = line;
    return 0;
}

static int shell_cmd_run(void)
{
    char name[64];
    if (argc < 2) {
        shell_putln("usage: run <program>");
        return 0;
    }
    shell_expand(argv[1], name, sizeof(name));
    if (shell_run_program(name) < 0)
        shell_putln("run failed");
    return 0;
}

static int shell_dispatch(void)
{
    int i;
    if (argc == 0)
        return 0;
    for (i = 0; i < cmd_count; i++) {
        if (Harlin_Compare(argv[0], cmd_table[i].name) == 0)
            return cmd_table[i].handler(argc, argv);
    }
    if (Harlin_Compare(argv[0], "say") == 0)       return shell_cmd_say();
    if (Harlin_Compare(argv[0], "copy") == 0)      return shell_cmd_copy();
    if (Harlin_Compare(argv[0], "setup") == 0)     return shell_cmd_setup();
    if (Harlin_Compare(argv[0], "delete") == 0)    return shell_cmd_delete();
    if (Harlin_Compare(argv[0], "end") == 0)       return shell_cmd_end();
    if (Harlin_Compare(argv[0], "start") == 0)     return shell_cmd_start();
    if (Harlin_Compare(argv[0], "cd") == 0)        return shell_cmd_cd();
    if (Harlin_Compare(argv[0], "help") == 0)      return shell_cmd_help();
    if (Harlin_Compare(argv[0], "set") == 0)       return shell_cmd_set();
    if (Harlin_Compare(argv[0], "variable") == 0)  return shell_cmd_variable();
    if (Harlin_Compare(argv[0], "constant") == 0)  return shell_cmd_constant();
    if (Harlin_Compare(argv[0], "if") == 0)        return shell_cmd_if();
    if (Harlin_Compare(argv[0], "else") == 0)      return shell_cmd_else();
    if (Harlin_Compare(argv[0], "calculate") == 0) return shell_cmd_calculate();
    if (Harlin_Compare(argv[0], "goto") == 0)      return shell_cmd_goto();
    if (Harlin_Compare(argv[0], "run") == 0)       return shell_cmd_run();

    shell_puts("unknown command: ");
    shell_putln(argv[0]);
    return 0;
}

void shell_run(void)
{
    shell_running = 1;
    shell_putln("HarLin Shell ready. Type 'help' for commands.");
    while (shell_running) {
        shell_read_line();
        shell_parse_args();
        shell_dispatch();
    }
}
