#include "shell.h"
#include "keyboard.h"
#include "screen.h"
#include "string.h"
#include "io.h"
#include "network.h"

#define INPUT_MAX 256
#define PROMPT "HarLin> "
#define MAX_VARS 64
#define VAR_NAME_LEN 16

static char input_buf[INPUT_MAX];
static int input_pos = 0;

static char var_names[MAX_VARS][VAR_NAME_LEN];
static int var_values[MAX_VARS];
static int var_count = 0;

static void exec_cmd(void);

static int find_var(const char* name, int len)
{
    int i;
    for (i = 0; i < var_count; i++) {
        int j;
        for (j = 0; j < len && var_names[i][j]; j++) {
            if (var_names[i][j] != name[j]) break;
        }
        if (j == len && var_names[i][j] == 0) return i;
    }
    return -1;
}

static int get_var_value(const char* name, int len)
{
    int idx = find_var(name, len);
    if (idx >= 0) return var_values[idx];
    return 0;
}

static void set_var(const char* name, int len, int value)
{
    int idx = find_var(name, len);
    if (idx >= 0) {
        var_values[idx] = value;
    } else if (var_count < MAX_VARS) {
        int i;
        for (i = 0; i < len && i < VAR_NAME_LEN - 1; i++) {
            var_names[var_count][i] = name[i];
        }
        var_names[var_count][i] = 0;
        var_values[var_count] = value;
        var_count++;
    }
}

static int is_digit(char c)
{
    return c >= '0' && c <= '9';
}



static int parse_number(const char** p)
{
    int num = 0;
    while (is_digit(**p)) {
        num = num * 10 + (**p - '0');
        (*p)++;
    }
    return num;
}

static int parse_expr(const char** p);

static int is_var_char(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

static int parse_factor(const char** p)
{
    while (**p == ' ') (*p)++;
    
    if (**p == '(') {
        (*p)++;
        int val = parse_expr(p);
        if (**p == ')') (*p)++;
        return val;
    }
    
    if (**p == '-') {
        (*p)++;
        return -parse_factor(p);
    }
    
    if (is_digit(**p)) {
        return parse_number(p);
    }
    
    if ((**p >= 'a' && **p <= 'z') || (**p >= 'A' && **p <= 'Z') || **p == '_') {
        const char* start = *p;
        while (is_var_char(**p)) (*p)++;
        return get_var_value(start, *p - start);
    }
    
    return 0;
}

static int parse_term(const char** p)
{
    int val = parse_factor(p);
    while (1) {
        while (**p == ' ') (*p)++;
        if (**p == '*') {
            (*p)++;
            val *= parse_factor(p);
        } else if (**p == '/') {
            (*p)++;
            int div = parse_factor(p);
            if (div != 0) val /= div;
        } else {
            break;
        }
    }
    return val;
}

static int parse_expr(const char** p)
{
    int val = parse_term(p);
    while (1) {
        while (**p == ' ') (*p)++;
        if (**p == '+') {
            (*p)++;
            val += parse_term(p);
        } else if (**p == '-') {
            (*p)++;
            val -= parse_term(p);
        } else {
            break;
        }
    }
    return val;
}

static int eval_condition(const char** p)
{
    int left, right;
    int op;
    
    while (**p == ' ') (*p)++;
    left = parse_expr(p);
    while (**p == ' ') (*p)++;
    
    op = **p;
    if (op == '=' || op == '!' || op == '<' || op == '>') {
        (*p)++;
        if (**p == '=') {
            op = (op << 8) | '=';
            (*p)++;
        }
    } else {
        return left != 0;
    }
    
    while (**p == ' ') (*p)++;
    right = parse_expr(p);
    
    if (op == (('=' << 8) | '=') || op == '=') return left == right;
    if (op == (('!' << 8) | '=')) return left != right;
    if (op == (('<' << 8) | '=')) return left <= right;
    if (op == (('>' << 8) | '=')) return left >= right;
    if (op == '<') return left < right;
    if (op == '>') return left > right;
    
    return 0;
}

static const char* skip_block(const char* p)
{
    int depth = 1;
    p++;
    while (*p && depth > 0) {
        if (*p == '{') depth++;
        if (*p == '}') depth--;
        if (depth > 0) p++;
    }
    return p;
}

static void exec_block(const char* block);

static void exec_if(const char** p)
{
    int condition_true;
    const char* block_start;
    const char* block_end;
    
    (*p) += 2;
    while (**p == ' ') (*p)++;
    
    if (**p != '(') return;
    (*p)++;
    
    condition_true = eval_condition(p);
    
    while (**p == ' ') (*p)++;
    if (**p != ')') return;
    (*p)++;
    while (**p == ' ') (*p)++;
    
    if (**p != '{') return;
    block_start = *p + 1;
    block_end = skip_block(*p);
    
    if (condition_true) {
        char saved = *block_end;
        char* temp = (char*)block_end;
        *temp = 0;
        exec_block(block_start);
        *temp = saved;
    }
    
    *p = block_end;
    if (**p == '}') (*p)++;
    
    while (**p == ' ') (*p)++;
    
    if (**p == 'e' && (*p)[1] == 'l' && (*p)[2] == 's' && (*p)[3] == 'e') {
        int else_condition;
        (*p) += 4;
        while (**p == ' ') (*p)++;
        
        if (**p == '(') {
            (*p)++;
            else_condition = eval_condition(p);
            while (**p == ' ') (*p)++;
            if (**p == ')') (*p)++;
        } else {
            else_condition = 1;
        }
        
        while (**p == ' ') (*p)++;
        if (**p == '{') {
            block_start = *p + 1;
            block_end = skip_block(*p);
            
            if (!condition_true && else_condition) {
                char saved = *block_end;
                char* temp = (char*)block_end;
                *temp = 0;
                exec_block(block_start);
                *temp = saved;
            }
            
            *p = block_end;
        }
    }
}

static void exec_set(const char** p)
{
    const char* var_start;
    int var_len;
    int val;
    
    (*p) += 3;
    while (**p == ' ') (*p)++;
    
    if (!(**p >= 'a' && **p <= 'z') && !(**p >= 'A' && **p <= 'Z') && **p != '_') return;
    var_start = *p;
    while (is_var_char(**p)) (*p)++;
    var_len = *p - var_start;
    
    while (**p == ' ') (*p)++;
    if (**p == '=') (*p)++;
    while (**p == ' ') (*p)++;
    
    val = parse_expr(p);
    set_var(var_start, var_len, val);
}

static void exec_calc(const char** p)
{
    int left_val, right_val, result;
    char op;
    
    (*p) += 4;
    while (**p == ' ') (*p)++;
    if (**p != '(') return;
    (*p)++;
    
    left_val = parse_expr(p);
    
    while (**p == ' ') (*p)++;
    if (**p != ')') return;
    (*p)++;
    while (**p == ' ') (*p)++;
    
    if (**p != '{') return;
    (*p)++;
    op = **p;
    if (op != '+' && op != '-' && op != '*' && op != '/') return;
    (*p)++;
    if (**p != '}') return;
    (*p)++;
    
    while (**p == ' ') (*p)++;
    if (**p != '(') return;
    (*p)++;
    
    right_val = parse_expr(p);
    
    while (**p == ' ') (*p)++;
    if (**p != ')') return;
    (*p)++;
    
    switch (op) {
        case '+': result = left_val + right_val; break;
        case '-': result = left_val - right_val; break;
        case '*': result = left_val * right_val; break;
        case '/': result = right_val != 0 ? left_val / right_val : 0; break;
        default: result = 0; break;
    }
    
    while (**p == ' ') (*p)++;
    if (**p == '{') {
        (*p)++;
        while (**p == ' ') (*p)++;
        if (**p == '=') (*p)++;
        while (**p == ' ') (*p)++;
        if ((**p >= 'a' && **p <= 'z') || (**p >= 'A' && **p <= 'Z') || **p == '_') {
            const char* var_start = *p;
            while (is_var_char(**p)) (*p)++;
            set_var(var_start, *p - var_start, result);
        }
        while (**p == ' ') (*p)++;
        if (**p == '}') (*p)++;
    }
    
    while (**p == ' ') (*p)++;
    if (**p == '{') {
        const char* block_start = *p + 1;
        const char* block_end = skip_block(*p);
        char saved = *block_end;
        char* temp = (char*)block_end;
        *temp = 0;
        exec_block(block_start);
        *temp = saved;
        *p = block_end;
    }
}

static void exec_block(const char* block)
{
    const char* p = block;
    char cmd_buf[64];
    int cmd_len;
    
    while (*p) {
        while (*p == ' ' || *p == ';') p++;
        if (*p == 0 || *p == '}') break;
        
        if (p[0] == 'i' && p[1] == 'f' && (p[2] == '(' || p[2] == ' ')) {
            exec_if(&p);
            continue;
        }
        
        if (p[0] == 's' && p[1] == 'e' && p[2] == 't' && (p[3] == ' ' || p[3] == '(')) {
            exec_set(&p);
            continue;
        }
        
        if (p[0] == 'c' && p[1] == 'a' && p[2] == 'l' && p[3] == 'c' && (p[4] == '(' || p[4] == ' ')) {
            exec_calc(&p);
            continue;
        }
        
        if (p[0] == 'u' && p[1] == 'r' && p[2] == 'l' && (p[3] == ' ' || p[3] == 0)) {
            cmd_len = 0;
            while (*p && *p != ';' && *p != '}' && cmd_len < 63) {
                cmd_buf[cmd_len++] = *p++;
            }
            cmd_buf[cmd_len] = 0;
            if (cmd_len > 0) {
                int i2;
                for (i2 = 0; i2 <= cmd_len && i2 < INPUT_MAX - 1; i2++) {
                    input_buf[i2] = cmd_buf[i2];
                }
                exec_cmd();
                input_buf[0] = 0;
            }
            continue;
        }

        if (p[0] == 'c' && p[1] == 'o' && p[2] == 'n' && p[3] == 'n'
            && p[4] == 'e' && p[5] == 'c' && p[6] == 't' && (p[7] == ' ' || p[7] == 0)) {
            cmd_len = 0;
            while (*p && *p != ';' && *p != '}' && cmd_len < 63) {
                cmd_buf[cmd_len++] = *p++;
            }
            cmd_buf[cmd_len] = 0;
            if (cmd_len > 0) {
                int i2;
                for (i2 = 0; i2 <= cmd_len && i2 < INPUT_MAX - 1; i2++) {
                    input_buf[i2] = cmd_buf[i2];
                }
                exec_cmd();
                input_buf[0] = 0;
            }
            continue;
        }
        
        cmd_len = 0;
        while (*p && *p != ';' && *p != '}' && cmd_len < 63) {
            cmd_buf[cmd_len++] = *p++;
        }
        cmd_buf[cmd_len] = 0;
        
        if (cmd_len > 0) {
            int i;
            for (i = 0; i <= cmd_len && i < INPUT_MAX - 1; i++) {
                input_buf[i] = cmd_buf[i];
            }
            exec_cmd();
            input_buf[0] = 0;
        }
    }
}

static void shell_prompt(void)
{
    int i;
    for (i = 0; PROMPT[i] != 0; i++) {
        screen_put_char(PROMPT[i]);
    }
}

static void read_line(void)
{
    unsigned char scancode;
    unsigned char last = 0;
    input_pos = 0;
    input_buf[0] = 0;

    while (1) {
        scancode = keyboard_poll();
        if (scancode == 0 || scancode == last) {
            last = scancode;
            continue;
        }
        last = scancode;
        if (scancode >= 128) {
            keyboard_scancode_to_ascii(scancode);
            continue;
        }
        if (scancode == 0x1C) {
            screen_put_char('\n');
            input_buf[input_pos] = 0;
            return;
        }
        if (scancode == 0x0E) {
            if (input_pos > 0) {
                input_pos--;
                screen_put_char('\b');
            }
            continue;
        }
        if (input_pos >= INPUT_MAX - 1) continue;
        char c = keyboard_scancode_to_ascii(scancode);
        if (c == 0) continue;
        input_buf[input_pos++] = c;
        screen_put_char(c);
    }
}

static void cmd_say(const char* args)
{
    while (*args == ' ') args++;
    while (*args) {
        screen_put_char(*args);
        args++;
    }
    screen_put_char('\n');
}

static void cmd_ver(void)
{
    int i;
    const char* s = "HarLin";
    for (i = 0; s[i]; i++) screen_put_char(s[i]);
    screen_put_char('\n');
}

static void cmd_info(void)
{
    int i;
    const char* s;
    s = "HarLin Shell V.H26.1";
    for (i = 0; s[i]; i++) screen_put_char(s[i]);
    screen_put_char('\n');
}

static void cmd_reboot(void)
{
    int i;
    const char* s = "Rebooting...";
    for (i = 0; s[i]; i++) screen_put_char(s[i]);
    screen_put_char('\n');
    outb(0x64, 0xFE);
}

static void cmd_show(const char* args)
{
    const char* p = args;
    int val;
    int neg = 0;
    char buf[12];
    int i = 0;
    
    while (*p == ' ') p++;
    
    if (*p == 0) {
        int j;
        for (j = 0; j < var_count; j++) {
            int k;
            for (k = 0; var_names[j][k]; k++) {
                screen_put_char(var_names[j][k]);
            }
            screen_put_char('=');
            val = var_values[j];
            if (val < 0) {
                neg = 1;
                val = -val;
            }
            if (val == 0) buf[i++] = '0';
            while (val > 0) {
                buf[i++] = '0' + (val % 10);
                val /= 10;
            }
            if (neg) buf[i++] = '-';
            while (i > 0) screen_put_char(buf[--i]);
            screen_put_char('\n');
        }
        return;
    }
    
    if (!(p[0] >= 'a' && p[0] <= 'z') && !(p[0] >= 'A' && p[0] <= 'Z') && p[0] != '_') {
        screen_put_char('?');
        screen_put_char('\n');
        return;
    }
    
    const char* start = p;
    while (is_var_char(*p)) p++;
    val = get_var_value(start, p - start);
    
    if (val < 0) {
        neg = 1;
        val = -val;
    }
    if (val == 0) buf[i++] = '0';
    while (val > 0) {
        buf[i++] = '0' + (val % 10);
        val /= 10;
    }
    if (neg) buf[i++] = '-';
    while (i > 0) screen_put_char(buf[--i]);
    screen_put_char('\n');
}

static unsigned char read_cmos(int reg)
{
    outb(0x70, reg);
    return inb(0x71);
}

static int bcd_to_int(unsigned char bcd)
{
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

static void cmd_date(void)
{
    int year, month, day, hour, minute, second;
    char buf[5];
    int i;
    
    second = bcd_to_int(read_cmos(0x00));
    minute = bcd_to_int(read_cmos(0x02));
    hour = bcd_to_int(read_cmos(0x04));
    day = bcd_to_int(read_cmos(0x07));
    month = bcd_to_int(read_cmos(0x08));
    year = bcd_to_int(read_cmos(0x09)) + 2000;
    
    i = 0;
    buf[i++] = '0' + (year / 1000);
    buf[i++] = '0' + ((year / 100) % 10);
    buf[i++] = '0' + ((year / 10) % 10);
    buf[i++] = '0' + (year % 10);
    buf[i] = 0;
    for (i = 0; buf[i]; i++) screen_put_char(buf[i]);
    screen_put_char('/');
    
    screen_put_char('0' + (month / 10));
    screen_put_char('0' + (month % 10));
    screen_put_char('/');
    
    screen_put_char('0' + (day / 10));
    screen_put_char('0' + (day % 10));
    screen_put_char('/');
    
    screen_put_char('0' + (hour / 10));
    screen_put_char('0' + (hour % 10));
    screen_put_char('/');
    
    screen_put_char('0' + (minute / 10));
    screen_put_char('0' + (minute % 10));
    screen_put_char('/');
    
    screen_put_char('0' + (second / 10));
    screen_put_char('0' + (second % 10));
    screen_put_char('\n');
}

static void exec_calc(const char** p);

static void cmd_calc(const char* args)
{
    const char* p = args;
    exec_calc(&p);
}

static void cmd_url(void)
{
    const char* s = "This feature is under development\n";
    int k = 0;
    while (s[k]) screen_put_char(s[k++]);
}

static void cmd_connect(void)
{
    const char* s = "https://harread.surge.sh\n";
    int k = 0;
    while (s[k]) screen_put_char(s[k++]);
}

static void cmd_help(void)
{
    int i;
    const char* s;
    s = "\nHarLin Shell\n\n";
    for (i = 0; s[i]; i++) screen_put_char(s[i]);
    s = "help    - Show commands\n";
    for (i = 0; s[i]; i++) screen_put_char(s[i]);
    s = "cls     - Clear screen\n";
    for (i = 0; s[i]; i++) screen_put_char(s[i]);
    s = "say     - Print text\n";
    for (i = 0; s[i]; i++) screen_put_char(s[i]);
    s = "ver     - Version\n";
    for (i = 0; s[i]; i++) screen_put_char(s[i]);
    s = "info    - System info\n";
    for (i = 0; s[i]; i++) screen_put_char(s[i]);
    s = "reboot  - Reboot\n";
    for (i = 0; s[i]; i++) screen_put_char(s[i]);
    s = "set     - Set variable\n";
    for (i = 0; s[i]; i++) screen_put_char(s[i]);
    s = "show    - Show variable\n";
    for (i = 0; s[i]; i++) screen_put_char(s[i]);
    s = "date    - Show date/time\n";
    for (i = 0; s[i]; i++) screen_put_char(s[i]);
    s = "calc    - Calculate\n";
    for (i = 0; s[i]; i++) screen_put_char(s[i]);
    s = "if      - Condition\n";
    for (i = 0; s[i]; i++) screen_put_char(s[i]);
    s = "url     - HTTP GET\n";
    for (i = 0; s[i]; i++) screen_put_char(s[i]);
    s = "connect - Connect to server\n";
    for (i = 0; s[i]; i++) screen_put_char(s[i]);
    screen_put_char('\n');
}

static int cmd_match(const char* input, const char* cmd)
{
    while (*cmd) {
        if (*input != *cmd) return 0;
        input++;
        cmd++;
    }
    return (*input == 0 || *input == ' ');
}

static const char* cmd_args(const char* input)
{
    while (*input && *input != ' ') input++;
    while (*input == ' ') input++;
    return input;
}

static void exec_cmd(void)
{
    const char* p = input_buf;
    while (*p == ' ') p++;
    if (*p == 0) return;

    if (p[0] == 'i' && p[1] == 'f' && (p[2] == '(' || p[2] == ' ')) {
        exec_if(&p);
        return;
    }

    if (p[0] == 's' && p[1] == 'e' && p[2] == 't' && (p[3] == ' ' || p[3] == '(')) {
        exec_set(&p);
        return;
    }

    if (cmd_match(p, "help")) { cmd_help(); }
    else if (cmd_match(p, "cls")) { screen_clear(); }
    else if (cmd_match(p, "ver")) { cmd_ver(); }
    else if (cmd_match(p, "info")) { cmd_info(); }
    else if (cmd_match(p, "reboot")) { cmd_reboot(); }
    else if (cmd_match(p, "say")) { cmd_say(cmd_args(p)); }
    else if (cmd_match(p, "show")) { cmd_show(cmd_args(p)); }
    else if (cmd_match(p, "date")) { cmd_date(); }
    else if (cmd_match(p, "calc")) { cmd_calc(cmd_args(p)); }
    else if (cmd_match(p, "url")) { cmd_url(); }
    else if (cmd_match(p, "connect")) { cmd_connect(); }
    else {
        int i;
        const char* s = "Unknown: ";
        for (i = 0; s[i]; i++) screen_put_char(s[i]);
        for (i = 0; p[i] && p[i] != ' '; i++) screen_put_char(p[i]);
        screen_put_char('\n');
    }
}

void shell_run(void)
{
    screen_clear();
    cmd_info();
    screen_put_char('\n');

    while (1) {
        shell_prompt();
        read_line();
        exec_cmd();
    }
}