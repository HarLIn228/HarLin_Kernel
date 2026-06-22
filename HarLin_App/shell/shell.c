#include "harlin.h"

#define SHELL_BUF_SIZE 512
#define SHELL_ARGS_MAX 16
#define SHELL_PROMPT  "HarLin> "
#define MAX_VARS 64
#define MAX_FUNCS 16
#define MAX_LINES 256
#define MAX_LINE_LEN 256
#define MAX_SCOPE_DEPTH 8

static char var_names[MAX_VARS][32];
static char var_values[MAX_VARS][SHELL_BUF_SIZE];

static char func_names[MAX_FUNCS][32];
static int func_line_start[MAX_FUNCS];
static int func_line_count[MAX_FUNCS];
static int func_count;

static char line_storage[MAX_LINES][MAX_LINE_LEN];
static int line_count;

static int scope_stack[MAX_SCOPE_DEPTH];
static int scope_depth;

static int local_base;

static void shell_print(const char* s);
static void shell_print_dec(harlin_u64 val);
static int shell_read_line(char* buf, int max);
static int shell_parse(char* line, char** args);
static int str_eq(const char* a, const char* b);
static int str_startswith(const char* s, const char* prefix);
static int set_variable(const char* name, const char* value);
static const char* get_variable(const char* name);
static int is_number(const char* s);
static int to_number(const char* s);
static void expand_vars(const char* input, char* output, int max);
static int split_assignment(const char* line, char* var, int var_max, char* val, int val_max);
static int parse_condition(const char* str, char* left, int lmax, char* op, int omax, char* right, int rmax);
static int eval_condition(const char* cond);
static int find_matching_brace(int start);
static int find_func(const char* name);
static int exec_block(int start_line, int end_line);
static int exec_command_block(const char* line);
static int exec_command(const char* line);
static void skip_whitespace(const char** p);

static int str_eq(const char* a, const char* b)
{
    while (*a && *b) {
        if (*a != *b) return 0;
        a++; b++;
    }
    return *a == *b;
}

static int str_startswith(const char* s, const char* prefix)
{
    while (*prefix) {
        if (*s != *prefix) return 0;
        s++; prefix++;
    }
    return 1;
}

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

static int is_number(const char* s)
{
    if (!s || !*s) return 0;
    if (*s == '-') s++;
    if (!*s) return 0;
    while (*s) {
        if (*s < '0' || *s > '9') return 0;
        s++;
    }
    return 1;
}

static int to_number(const char* s)
{
    int val = 0, sign = 1;
    if (*s == '-') { sign = -1; s++; }
    while (*s >= '0' && *s <= '9') {
        val = val * 10 + (*s - '0');
        s++;
    }
    return val * sign;
}

static void skip_whitespace(const char** p)
{
    while (**p == ' ') (*p)++;
}

static int set_variable(const char* name, const char* value)
{
    int i, empty = -1;
    for (i = 0; i < MAX_VARS; i++) {
        if (var_names[i][0] == '\0' && empty < 0)
            empty = i;
        if (var_names[i][0] != '\0') {
            if (str_eq(var_names[i], name)) {
                int j;
                for (j = 0; value[j] && j < SHELL_BUF_SIZE - 1; j++)
                    var_values[i][j] = value[j];
                var_values[i][j] = '\0';
                return 0;
            }
        }
    }
    if (empty < 0) return -1;
    {
        int j;
        for (j = 0; name[j] && j < 31; j++)
            var_names[empty][j] = name[j];
        var_names[empty][j] = '\0';
    }
    {
        int j;
        for (j = 0; value[j] && j < SHELL_BUF_SIZE - 1; j++)
            var_values[empty][j] = value[j];
        var_values[empty][j] = '\0';
    }
    return 0;
}

static const char* get_variable(const char* name)
{
    int i;
    for (i = 0; i < MAX_VARS; i++) {
        if (var_names[i][0] != '\0' && str_eq(var_names[i], name))
            return var_values[i];
    }
    return "";
}

static void expand_vars(const char* input, char* output, int max)
{
    int o = 0;
    while (*input && o < max - 1) {
        if (*input == '$') {
            input++;
            if (*input == '{') {
                input++;
                {
                    char vname[32];
                    int v = 0;
                    while (*input && *input != '}' && v < 31)
                        vname[v++] = *input++;
                    vname[v] = '\0';
                    if (*input == '}') input++;
                    {
                        const char* val = get_variable(vname);
                        while (*val && o < max - 1)
                            output[o++] = *val++;
                    }
                }
            } else if (*input >= '1' && *input <= '9') {
                char pname[3];
                pname[0] = *input++;
                pname[1] = '\0';
                {
                    const char* val = get_variable(pname);
                    while (*val && o < max - 1)
                        output[o++] = *val++;
                }
            } else if (*input == '0') {
                input++;
                {
                    const char* val = get_variable("0");
                    while (*val && o < max - 1)
                        output[o++] = *val++;
                }
            } else if (*input == '?') {
                input++;
                {
                    const char* r = get_variable("?");
                    if (!r || r[0] == '\0') {
                        if (o < max - 1) output[o++] = '0';
                    } else {
                        while (*r && o < max - 1)
                            output[o++] = *r++;
                    }
                }
            } else if (*input == '$') {
                output[o++] = '$';
                input++;
            } else if (*input == '#') {
                input++;
                {
                    const char* val = get_variable("#");
                    if (!val || val[0] == '\0')
                        val = "0";
                    while (*val && o < max - 1)
                        output[o++] = *val++;
                }
            } else if (*input == '@' || *input == '*') {
                input++;
                {
                    char all_args[SHELL_BUF_SIZE];
                    int ai = 0;
                    int pi;
                    for (pi = 1; pi < 10; pi++) {
                        char pn[3];
                        pn[0] = '0' + pi;
                        pn[1] = '\0';
                        {
                            const char* val = get_variable(pn);
                            if (val[0]) {
                                if (ai > 0) all_args[ai++] = ' ';
                                while (*val && ai < SHELL_BUF_SIZE - 1)
                                    all_args[ai++] = *val++;
                            }
                        }
                    }
                    all_args[ai] = '\0';
                    {
                        const char* vp = all_args;
                        while (*vp && o < max - 1)
                            output[o++] = *vp++;
                    }
                }
            } else {
                char vname[32];
                int v = 0;
                while (*input && *input != ' ' && *input != '\t' && v < 31)
                    vname[v++] = *input++;
                vname[v] = '\0';
                {
                    const char* val = get_variable(vname);
                    while (*val && o < max - 1)
                        output[o++] = *val++;
                }
            }
        } else {
            output[o++] = *input++;
        }
    }
    output[o] = '\0';
}

static int split_assignment(const char* line, char* var, int var_max, char* val, int val_max)
{
    const char* p = line;
    int vp = 0;
    while (*p == ' ') p++;
    while (*p && *p != '=' && vp < var_max - 1)
        var[vp++] = *p++;
    var[vp] = '\0';
    if (*p != '=') return -1;
    p++;
    {
        int vvp = 0;
        while (*p == ' ') p++;
        if (*p == '"') {
            p++;
            while (*p && *p != '"' && vvp < val_max - 1)
                val[vvp++] = *p++;
            if (*p == '"') p++;
        } else if (*p == '\'') {
            p++;
            while (*p && *p != '\'' && vvp < val_max - 1)
                val[vvp++] = *p++;
            if (*p == '\'') p++;
        } else {
            while (*p && *p != ' ' && vvp < val_max - 1)
                val[vvp++] = *p++;
        }
        val[vvp] = '\0';
    }
    return 0;
}

static int parse_condition(const char* str, char* left, int lmax, char* op, int omax, char* right, int rmax)
{
    const char* p = str;
    int lp = 0, opi = 0, rp = 0;
    while (*p == ' ') p++;
    while (*p && *p != ' ' && lp < lmax - 1)
        left[lp++] = *p++;
    left[lp] = '\0';
    expand_vars(left, left, lmax);
    while (*p == ' ') p++;
    while (*p && *p != ' ' && opi < omax - 1)
        op[opi++] = *p++;
    op[opi] = '\0';
    while (*p == ' ') p++;
    while (*p && rp < rmax - 1)
        right[rp++] = *p++;
    right[rp] = '\0';
    expand_vars(right, right, rmax);
    return 0;
}

static int eval_condition(const char* cond)
{
    char left[64], op[8], right[64];
    int lnum, rnum, l_is_num, r_is_num;

    if (!cond || !*cond) return 1;

    parse_condition(cond, left, 64, op, 8, right, 64);

    l_is_num = is_number(left);
    r_is_num = is_number(right);
    lnum = l_is_num ? to_number(left) : 0;
    rnum = r_is_num ? to_number(right) : 0;

    if (op[0] == '\0')
        return left[0] != '\0';

    if (str_eq(op, "==")) {
        if (l_is_num && r_is_num) return lnum == rnum;
        return str_eq(left, right);
    }
    if (str_eq(op, "!=")) {
        if (l_is_num && r_is_num) return lnum != rnum;
        return !str_eq(left, right);
    }
    if (l_is_num && r_is_num) {
        if (str_eq(op, "<"))  return lnum < rnum;
        if (str_eq(op, ">"))  return lnum > rnum;
        if (str_eq(op, "<=")) return lnum <= rnum;
        if (str_eq(op, ">=")) return lnum >= rnum;
    }
    if (str_eq(op, "-eq")) return lnum == rnum;
    if (str_eq(op, "-ne")) return lnum != rnum;
    if (str_eq(op, "-lt")) return lnum < rnum;
    if (str_eq(op, "-gt")) return lnum > rnum;
    if (str_eq(op, "-le")) return lnum <= rnum;
    if (str_eq(op, "-ge")) return lnum >= rnum;

    return 0;
}

static int find_matching_brace(int start)
{
    int depth = 1;
    int i;
    for (i = start; i < line_count; i++) {
        const char* p = line_storage[i];
        skip_whitespace(&p);
        if (p[0] == '{') depth++;
        else if (p[0] == '}') {
            depth--;
            if (depth == 0) return i;
        }
    }
    return -1;
}

static int find_func(const char* name)
{
    int i;
    for (i = 0; i < func_count; i++) {
        if (str_eq(func_names[i], name))
            return i;
    }
    return -1;
}

static int exec_block(int start_line, int end_line)
{
    int i = start_line;
    int last_ret = 0;
    while (i <= end_line) {
        const char* p = line_storage[i];
        skip_whitespace(&p);
        if (p[0] == '\0' || p[0] == '#') { i++; continue; }
        if (p[0] == '}' || p[0] == '{') { i++; continue; }

        if (str_startswith(p, "while") && (p[5] == ' ' || p[5] == '\t')) {
            const char* cp = p + 6;
            char cond[SHELL_BUF_SIZE];
            int ci = 0;
            skip_whitespace(&cp);
            {
                int in_brace = 0;
                const char* brace_chk = cp;
                while (*brace_chk) {
                    if (*brace_chk == '{') { in_brace = 1; break; }
                    brace_chk++;
                }
                if (!in_brace) {
                    int bi = i + 1;
                    while (bi <= end_line) {
                        const char* bp = line_storage[bi];
                        skip_whitespace(&bp);
                        if (bp[0] == '}') break;
                        bi++;
                    }
                    while (eval_condition(cp)) {
                        int ji;
                        for (ji = i + 1; ji < bi; ji++)
                            exec_command(line_storage[ji]);
                    }
                    i = bi;
                    i++;
                    continue;
                }
            }
            while (*cp && *cp != '{' && ci < SHELL_BUF_SIZE - 1)
                cond[ci++] = *cp++;
            cond[ci] = '\0';
            {
                int body_start = i + 1;
                int body_end = find_matching_brace(body_start);
                if (body_end < 0) {
                    shell_print("Syntax error: no matching }\r\n");
                    return -1;
                }
                while (eval_condition(cond)) {
                    int r = exec_block(body_start, body_end - 1);
                    if (r < 0) return r;
                }
                i = body_end;
            }
            i++;
            continue;
        }

        if (str_startswith(p, "for") && (p[3] == ' ' || p[3] == '\t')) {
            const char* cp = p + 4;
            while (*cp == ' ') cp++;
            {
                char init[256], cond[256], inc[256];
                int phase = 0;
                int pi = 0, ci = 0, ii = 0;
                int paren_depth = 0;

                if (*cp == '(') {
                    cp++;
                    while (*cp && phase < 3) {
                        if (*cp == '(') paren_depth++;
                        if (*cp == ')' && paren_depth == 0) { cp++; break; }
                        if (*cp == ')' && paren_depth > 0) { paren_depth--; }
                        if (*cp == ';' && paren_depth == 0) { phase++; cp++; continue; }
                        if (phase == 0 && pi < 255) init[pi++] = *cp;
                        if (phase == 1 && ci < 255) cond[ci++] = *cp;
                        if (phase == 2 && ii < 255) inc[ii++] = *cp;
                        cp++;
                    }
                } else {
                    while (*cp && *cp != '{' && ci < SHELL_BUF_SIZE - 1)
                        cond[ci++] = *cp++;
                }
                init[pi] = '\0';
                cond[ci] = '\0';
                inc[ii] = '\0';
                if (*cp == ' ') cp++;
                {
                    int body_start = i + 1;
                    int body_end = -1;
                    if (*cp == '{')
                        body_end = find_matching_brace(body_start);
                    if (body_end < 0) body_end = i;
                    if (init[0]) exec_command_block(init);
                    while (1) {
                        char expanded_cond[256];
                        expand_vars(cond, expanded_cond, 256);
                        if (expanded_cond[0] && !eval_condition(expanded_cond))
                            break;
                        if (body_end > i)
                            exec_block(body_start, body_end - 1);
                        else {
                            int ji;
                            for (ji = i + 1; ji <= end_line; ji++) {
                                const char* jp = line_storage[ji];
                                skip_whitespace(&jp);
                                if (jp[0] == '}') break;
                                exec_command(line_storage[ji]);
                            }
                        }
                        if (inc[0]) exec_command_block(inc);
                    }
                    if (body_end > i) i = body_end;
                }
            }
            i++;
            continue;
        }

        if (str_startswith(p, "if") && (p[2] == ' ' || p[2] == '\t')) {
            const char* cp = p + 3;
            char cond[SHELL_BUF_SIZE];
            int ci = 0;
            skip_whitespace(&cp);
            while (*cp && *cp != '{' && ci < SHELL_BUF_SIZE - 1)
                cond[ci++] = *cp++;
            cond[ci] = '\0';
            {
                int if_body_start = i + 1;
                int if_body_end = find_matching_brace(if_body_start);
                int has_else = 0;
                int else_line = -1;
                int ji;
                if (if_body_end < 0) { shell_print("Syntax error\r\n"); return -1; }
                for (ji = if_body_end + 1; ji <= end_line; ji++) {
                    const char* ep = line_storage[ji];
                    skip_whitespace(&ep);
                    if (ep[0] == '\0') continue;
                    if (str_startswith(ep, "else")) {
                        const char* ep2 = ep + 4;
                        skip_whitespace(&ep2);
                        if (*ep2 == '\0' || *ep2 == '{') {
                            has_else = 1;
                            else_line = ji;
                            break;
                        }
                    }
                    break;
                }
                if (eval_condition(cond)) {
                    exec_block(if_body_start, if_body_end - 1);
                } else if (has_else) {
                    const char* ep = line_storage[else_line] + 4;
                    skip_whitespace(&ep);
                    if (*ep == '{') {
                        int else_body_start = else_line + 1;
                        int else_body_end = find_matching_brace(else_body_start);
                        if (else_body_end >= 0)
                            exec_block(else_body_start, else_body_end - 1);
                        i = else_body_end;
                    }
                }
                i = if_body_end;
                if (has_else && else_line > i) i = else_line;
            }
            i++;
            continue;
        }

        if (str_startswith(p, "func") && (p[4] == ' ' || p[4] == '\t')) {
            if (func_count >= MAX_FUNCS) {
                shell_print("Too many functions\r\n");
                return -1;
            }
            {
                const char* fp = p + 5;
                int fn = 0;
                skip_whitespace(&fp);
                while (*fp && *fp != ' ' && *fp != '{' && fn < 31)
                    func_names[func_count][fn++] = *fp++;
                func_names[func_count][fn] = '\0';
                func_line_start[func_count] = i + 1;
                {
                    int body_end = find_matching_brace(func_line_start[func_count]);
                    if (body_end < 0) {
                        shell_print("Syntax error in function body\r\n");
                        return -1;
                    }
                    func_line_count[func_count] = body_end - func_line_start[func_count];
                }
                func_count++;
                {
                    int body_end = func_line_start[func_count - 1] + func_line_count[func_count - 1];
                    i = body_end;
                }
            }
            i++;
            continue;
        }

        {
            int r = exec_command(line_storage[i]);
            if (r < 0) return r;
            last_ret = r;
        }
        i++;
    }
    return last_ret;
}

static int exec_command_block(const char* line)
{
    char expanded[SHELL_BUF_SIZE];
    char* args[SHELL_ARGS_MAX];
    int argc;
    char var_name[32], var_val[SHELL_BUF_SIZE];

    expand_vars(line, expanded, SHELL_BUF_SIZE);

    if (split_assignment(expanded, var_name, 32, var_val, SHELL_BUF_SIZE) == 0) {
        int valid = 1;
        const char* vp = var_name;
        while (*vp) {
            if (!((*vp >= 'a' && *vp <= 'z') || (*vp >= 'A' && *vp <= 'Z') ||
                  (*vp >= '0' && *vp <= '9') || *vp == '_')) {
                valid = 0;
                break;
            }
            vp++;
        }
        if (valid) {
            set_variable(var_name, var_val);
            return 0;
        }
    }

    argc = shell_parse(expanded, args);
    if (argc == 0) return 0;

    {
        int fi = find_func(args[0]);
        if (fi >= 0) {
            int pi;
            if (scope_depth < MAX_SCOPE_DEPTH) {
                int base_idx = -1;
                int vi;
                for (vi = 0; vi < MAX_VARS; vi++) {
                    if (var_names[vi][0] == '\0' && base_idx < 0)
                        base_idx = vi;
                }
                scope_stack[scope_depth] = local_base;
                local_base = base_idx;
                scope_depth++;
            }
            {
                char old_params[10][SHELL_BUF_SIZE];
                for (pi = 0; pi < 10; pi++) {
                    char pn[3];
                    pn[0] = '0' + pi;
                    pn[1] = '\0';
                    {
                        const char* old = get_variable(pn);
                        int oi;
                        for (oi = 0; old[oi] && oi < SHELL_BUF_SIZE - 1; oi++)
                            old_params[pi][oi] = old[oi];
                        old_params[pi][oi] = '\0';
                    }
                }
                {
                    char all_args[SHELL_BUF_SIZE];
                    int ai = 0;
                    for (pi = 1; pi < argc; pi++) {
                        if (pi > 1) all_args[ai++] = ' ';
                        {
                            int si;
                            for (si = 0; args[pi][si] && ai < SHELL_BUF_SIZE - 1; si++)
                                all_args[ai++] = args[pi][si];
                        }
                    }
                    all_args[ai] = '\0';
                    set_variable("0", all_args);
                }
                for (pi = 1; pi < argc && pi < 10; pi++) {
                    char pn[3];
                    pn[0] = '0' + pi;
                    pn[1] = '\0';
                    set_variable(pn, args[pi]);
                }
                for (pi = argc; pi < 10; pi++) {
                    char pn[3];
                    pn[0] = '0' + pi;
                    pn[1] = '\0';
                    set_variable(pn, "");
                }
                {
                    char argc_str[4];
                    int ai;
                    int av = argc - 1;
                    if (av < 0) av = 0;
                    for (ai = 0; ai < 3 && av > 0; ai++) {
                        argc_str[ai] = '0' + (av % 10);
                        av /= 10;
                    }
                    argc_str[ai] = '\0';
                    {
                        char rev[4];
                        int ri;
                        for (ri = 0; ri < ai; ri++)
                            rev[ri] = argc_str[ai - 1 - ri];
                        rev[ri] = '\0';
                        set_variable("#", rev);
                    }
                }
                set_variable("?", "0");
                exec_block(func_line_start[fi], func_line_start[fi] + func_line_count[fi] - 1);
                for (pi = 0; pi < 10; pi++) {
                    char pn[3];
                    pn[0] = '0' + pi;
                    pn[1] = '\0';
                    set_variable(pn, old_params[pi]);
                }
            }
            if (scope_depth > 0) {
                scope_depth--;
                local_base = scope_stack[scope_depth];
            }
            return 0;
        }
    }

    return 0;
}

static int exec_command(const char* line)
{
    char expanded[SHELL_BUF_SIZE];
    char* args[SHELL_ARGS_MAX];
    int argc;
    char var_name[32], var_val[SHELL_BUF_SIZE];

    expand_vars(line, expanded, SHELL_BUF_SIZE);

    if (split_assignment(expanded, var_name, 32, var_val, SHELL_BUF_SIZE) == 0) {
        int valid = 1;
        const char* vp = var_name;
        while (*vp) {
            if (!((*vp >= 'a' && *vp <= 'z') || (*vp >= 'A' && *vp <= 'Z') ||
                  (*vp >= '0' && *vp <= '9') || *vp == '_')) {
                valid = 0;
                break;
            }
            vp++;
        }
        if (valid) {
            set_variable(var_name, var_val);
            return 0;
        }
    }

    argc = shell_parse(expanded, args);
    if (argc == 0) return 0;

    {
        int fi = find_func(args[0]);
        if (fi >= 0) {
            int pi;
            if (scope_depth < MAX_SCOPE_DEPTH) {
                scope_stack[scope_depth] = local_base;
                {
                    int vi;
                    int base_idx = -1;
                    for (vi = 0; vi < MAX_VARS; vi++) {
                        if (var_names[vi][0] == '\0' && base_idx < 0)
                            base_idx = vi;
                    }
                    local_base = base_idx;
                }
                scope_depth++;
            }
            {
                char old_params[10][SHELL_BUF_SIZE];
                for (pi = 0; pi < 10; pi++) {
                    char pn[3];
                    pn[0] = '0' + pi;
                    pn[1] = '\0';
                    {
                        const char* old = get_variable(pn);
                        int oi;
                        for (oi = 0; old[oi] && oi < SHELL_BUF_SIZE - 1; oi++)
                            old_params[pi][oi] = old[oi];
                        old_params[pi][oi] = '\0';
                    }
                }
                {
                    char all_args[SHELL_BUF_SIZE];
                    int ai = 0;
                    for (pi = 1; pi < argc; pi++) {
                        if (pi > 1) all_args[ai++] = ' ';
                        {
                            int si;
                            for (si = 0; args[pi][si] && ai < SHELL_BUF_SIZE - 1; si++)
                                all_args[ai++] = args[pi][si];
                        }
                    }
                    all_args[ai] = '\0';
                    set_variable("0", all_args);
                }
                for (pi = 1; pi < argc && pi < 10; pi++) {
                    char pn[3];
                    pn[0] = '0' + pi;
                    pn[1] = '\0';
                    set_variable(pn, args[pi]);
                }
                for (pi = argc; pi < 10; pi++) {
                    char pn[3];
                    pn[0] = '0' + pi;
                    pn[1] = '\0';
                    set_variable(pn, "");
                }
                {
                    char argc_str[4];
                    int ai;
                    int av = argc - 1;
                    if (av < 0) av = 0;
                    for (ai = 0; ai < 3 && av > 0; ai++) {
                        argc_str[ai] = '0' + (av % 10);
                        av /= 10;
                    }
                    argc_str[ai] = '\0';
                    {
                        char rev[4];
                        int ri;
                        for (ri = 0; ri < ai; ri++)
                            rev[ri] = argc_str[ai - 1 - ri];
                        rev[ri] = '\0';
                        set_variable("#", rev);
                    }
                }
                set_variable("?", "0");
                exec_block(func_line_start[fi], func_line_start[fi] + func_line_count[fi] - 1);
                for (pi = 0; pi < 10; pi++) {
                    char pn[3];
                    pn[0] = '0' + pi;
                    pn[1] = '\0';
                    set_variable(pn, old_params[pi]);
                }
            }
            if (scope_depth > 0) {
                scope_depth--;
                local_base = scope_stack[scope_depth];
            }
            return 0;
        }
    }

    if (str_eq(args[0], "help")) {
        shell_print("HarLin Shell commands:\r\n");
        shell_print("  help             显示帮助\r\n");
        shell_print("  say/echo <text>  回显文本(支持$var)\r\n");
        shell_print("  end/exit         退出Shell\r\n");
        shell_print("  run/exec <file>  运行CHC程序\r\n");
        shell_print("  pid              显示进程ID\r\n");
        shell_print("  beep <freq> <ms> 蜂鸣\r\n");
        shell_print("  sleep <ms>       休眠\r\n");
        shell_print("  time             显示时间\r\n");
        shell_print("  clearkeys        清除键盘缓冲区\r\n");
        shell_print("  set [var=val]    设置/显示变量\r\n");
        shell_print("  unset <var>      删除变量\r\n");
        shell_print("  source <file>    执行脚本文件\r\n");
        shell_print("  while/for/if/func 控制流\r\n");
        return 0;
    }
    if (str_eq(args[0], "say") || str_eq(args[0], "echo")) {
        int i;
        for (i = 1; i < argc; i++) {
            if (i > 1) shell_print(" ");
            shell_print(args[i]);
        }
        shell_print("\r\n");
        return 0;
    }
    if (str_eq(args[0], "end") || str_eq(args[0], "exit")) {
        shell_print("Shell exiting.\r\n");
        harlin_exit(0);
        return 0;
    }
    if (str_eq(args[0], "run") || str_eq(args[0], "exec")) {
        if (argc < 2) {
            shell_print("Usage: run <file.chc>\r\n");
            return -1;
        }
        if (harlin_exec(args[1]) < 0) {
            shell_print("Failed to run: ");
            shell_print(args[1]);
            shell_print("\r\n");
            return -1;
        }
        return 0;
    }
    if (str_eq(args[0], "pid")) {
        shell_print("PID: ");
        shell_print_dec((harlin_u64)harlin_getpid());
        shell_print("\r\n");
        return 0;
    }
    if (str_eq(args[0], "beep")) {
        harlin_u64 freq = 1000, ms = 200;
        if (argc > 1) {
            freq = 0;
            while (*args[1]) {
                freq = freq * 10 + (*args[1] - '0');
                args[1]++;
            }
        }
        if (argc > 2) {
            ms = 0;
            while (*args[2]) {
                ms = ms * 10 + (*args[2] - '0');
                args[2]++;
            }
        }
        harlin_beep((harlin_u32)freq, (harlin_u32)ms);
        return 0;
    }
    if (str_eq(args[0], "sleep")) {
        harlin_u64 ms = 1000;
        if (argc > 1) {
            ms = 0;
            while (*args[1]) {
                ms = ms * 10 + (*args[1] - '0');
                args[1]++;
            }
        }
        harlin_sleep((harlin_u32)ms);
        return 0;
    }
    if (str_eq(args[0], "time")) {
        struct harlin_rtc_time t;
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
    if (str_eq(args[0], "clearkeys")) {
        while (harlin_getc() >= 0);
        shell_print("Keyboard buffer cleared.\r\n");
        return 0;
    }
    if (str_eq(args[0], "source") || str_eq(args[0], ".")) {
        if (argc < 2) {
            shell_print("Usage: source <file>\r\n");
            return -1;
        }
        {
            int fd = harlin_open(args[1]);
            if (fd < 0) {
                shell_print("Failed to open: ");
                shell_print(args[1]);
                shell_print("\r\n");
                return -1;
            }
            {
                char script_buf[4096];
                int nread = harlin_read(fd, script_buf, 4096);
                harlin_close(fd);
                if (nread > 0) {
                    int lnum;
                    char* slines[MAX_LINES];
                    int sc = 1;
                    slines[0] = script_buf;
                    for (lnum = 0; lnum < nread; lnum++) {
                        if (script_buf[lnum] == '\n' || script_buf[lnum] == '\r') {
                            script_buf[lnum] = '\0';
                            if (lnum + 1 < nread &&
                                ((script_buf[lnum + 1] == '\n') || (script_buf[lnum + 1] == '\r')) &&
                                script_buf[lnum + 1] != script_buf[lnum])
                                lnum++;
                            if (lnum + 1 < nread && sc < MAX_LINES)
                                slines[sc++] = script_buf + lnum + 1;
                        }
                    }
                    {
                        int saved_line_count = line_count;
                        int si;
                        for (si = 0; si < sc && line_count < MAX_LINES; si++) {
                            int c;
                            for (c = 0; slines[si][c] && c < MAX_LINE_LEN - 1; c++)
                                line_storage[line_count][c] = slines[si][c];
                            line_storage[line_count][c] = '\0';
                            line_count++;
                        }
                        exec_block(saved_line_count, line_count - 1);
                        line_count = saved_line_count;
                    }
                }
            }
        }
        return 0;
    }
    if (str_eq(args[0], "set")) {
        if (argc >= 3) {
            set_variable(args[1], args[2]);
        } else if (argc == 2) {
            const char* p = args[1];
            char vname[32];
            int vp = 0;
            while (*p && *p != '=' && vp < 31)
                vname[vp++] = *p++;
            vname[vp] = '\0';
            if (*p == '=') {
                p++;
                set_variable(vname, p);
            } else {
                const char* val = get_variable(args[1]);
                shell_print(args[1]);
                shell_print("=");
                shell_print(val);
                shell_print("\r\n");
            }
        } else {
            int i;
            for (i = 0; i < MAX_VARS; i++) {
                if (var_names[i][0]) {
                    shell_print(var_names[i]);
                    shell_print("=");
                    shell_print(var_values[i]);
                    shell_print("\r\n");
                }
            }
        }
        return 0;
    }
    if (str_eq(args[0], "unset")) {
        if (argc >= 2) {
            int i;
            for (i = 0; i < MAX_VARS; i++) {
                if (str_eq(var_names[i], args[1])) {
                    var_names[i][0] = '\0';
                    var_values[i][0] = '\0';
                    break;
                }
            }
        }
        return 0;
    }

    shell_print("Unknown command: ");
    shell_print(args[0]);
    shell_print("\r\n");
    return -1;
}

void _start(void)
{
    char line[SHELL_BUF_SIZE];
    int i;

    for (i = 0; i < MAX_VARS; i++) {
        var_names[i][0] = '\0';
        var_values[i][0] = '\0';
    }
    for (i = 0; i < MAX_FUNCS; i++) {
        func_names[i][0] = '\0';
        func_line_start[i] = 0;
        func_line_count[i] = 0;
    }
    func_count = 0;
    line_count = 0;
    scope_depth = 0;
    local_base = 0;

    shell_print("HarLin Shell v2.0\r\n");
    shell_print("while/for/if/func, 变量, 参数\r\n");
    shell_print("Type 'help' for commands.\r\n");

    for (;;) {
        shell_print(SHELL_PROMPT);
        if (shell_read_line(line, SHELL_BUF_SIZE) <= 0)
            continue;

        if (line_count < MAX_LINES) {
            int c;
            for (c = 0; line[c] && c < MAX_LINE_LEN - 1; c++)
                line_storage[line_count][c] = line[c];
            line_storage[line_count][c] = '\0';
            line_count++;
        }

        {
            int has_control = 0;
            for (i = 0; i < line_count; i++) {
                const char* p = line_storage[i];
                while (*p == ' ') p++;
                if ((str_startswith(p, "while") && (p[5] == ' ' || p[5] == '\t' || p[5] == '\0')) ||
                    (str_startswith(p, "for") && (p[3] == ' ' || p[3] == '\t' || p[3] == '\0')) ||
                    (str_startswith(p, "func") && (p[4] == ' ' || p[4] == '\t' || p[4] == '\0')) ||
                    (str_startswith(p, "if") && (p[2] == ' ' || p[2] == '\t' || p[2] == '\0'))) {
                    has_control = 1;
                    break;
                }
            }
            if (!has_control) {
                for (i = 0; i < line_count; i++)
                    exec_command(line_storage[i]);
                line_count = 0;
            } else {
                exec_block(0, line_count - 1);
                line_count = 0;
            }
        }
    }
}
