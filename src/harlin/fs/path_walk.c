#include "path_walk.h"
#include "printk.h"
#include "bug.h"

static u32 my_strlen(const char* s)
{
    u32 n = 0;
    while (s[n] && n < 4096) n++;
    return n;
}

static void my_cpy(char* dst, const char* src, u32 n)
{
    for (u32 i = 0; i < n; i++) dst[i] = src[i];
    dst[n] = 0;
}

static int my_eq(const char* a, const char* b, u32 n)
{
    for (u32 i = 0; i < n; i++) {
        if (a[i] != b[i]) return 0;
    }
    return 1;
}

int Harlin_PathParse(const char* in, struct path_parts* out)
{
    if (!in || !out) return -1;
    u32 len = my_strlen(in);
    if (len == 0 || len >= PATH_MAX_CHARS) return -1;
    my_cpy(out->buf, in, len);
    out->count = 0;
    out->is_absolute = (in[0] == '/');

    u32 i = 0;
    if (out->is_absolute) i = 1;
    while (i < len && out->count < PATH_MAX_SEGMENTS) {
        u32 start = i;
        while (i < len && out->buf[i] != '/') i++;
        u32 seg_len = i - start;
        if (seg_len > 0) {
            u32 idx = out->count;
            out->seg[idx] = &out->buf[start];
            out->seg_len[idx] = seg_len;
            out->count++;
        }
        if (i < len && out->buf[i] == '/') i++;
    }
    return 0;
}

int Harlin_PathNormalize(const char* in, char* out, u32 out_size)
{
    if (!in || !out || out_size < 2) return -1;
    struct path_parts p;
    if (Harlin_PathParse(in, &p) != 0) return -1;

    const char* stack[PATH_MAX_SEGMENTS];
    u32 stack_len[PATH_MAX_SEGMENTS];
    u32 top = 0;

    for (u32 i = 0; i < p.count; i++) {
        if (p.seg_len[i] == 1 && p.seg[i][0] == '.') {
            continue;
        }
        if (p.seg_len[i] == 2 && p.seg[i][0] == '.' && p.seg[i][1] == '.') {
            if (top > 0) top--;
            continue;
        }
        if (top >= PATH_MAX_SEGMENTS) return -1;
        stack[top] = p.seg[i];
        stack_len[top] = p.seg_len[i];
        top++;
    }

    u32 pos = 0;
    if (p.is_absolute) {
        if (pos + 1 >= out_size) return -1;
        out[pos++] = '/';
    }
    for (u32 i = 0; i < top; i++) {
        if (i > 0) {
            if (pos + 1 >= out_size) return -1;
            out[pos++] = '/';
        }
        if (pos + stack_len[i] >= out_size) return -1;
        my_cpy(out + pos, stack[i], stack_len[i]);
        pos += stack_len[i];
    }
    if (pos == 0) {
        if (out_size < 1) return -1;
        out[pos++] = '.';
    }
    out[pos] = 0;
    return (int)pos;
}

int Harlin_PathTest(void)
{
    char out[PATH_MAX_CHARS];
    int n;

    n = Harlin_PathNormalize("/a/b/c", out, sizeof(out));
    ASSERT(n > 0);
    ASSERT(out[0] == '/');
    ASSERT(my_eq(out, "/a/b/c", 6));

    n = Harlin_PathNormalize("/a/./b/../c", out, sizeof(out));
    ASSERT(n > 0);
    ASSERT(my_eq(out, "/a/c", 4));

    n = Harlin_PathNormalize("a/b", out, sizeof(out));
    ASSERT(n > 0);
    ASSERT(my_eq(out, "a/b", 3));

    n = Harlin_PathNormalize("/", out, sizeof(out));
    ASSERT(n == 1);
    ASSERT(out[0] == '/');

    n = Harlin_PathNormalize(".", out, sizeof(out));
    ASSERT(n == 1);
    ASSERT(out[0] == '.');

    n = Harlin_PathNormalize("a/../b", out, sizeof(out));
    ASSERT(n > 0);
    ASSERT(my_eq(out, "b", 1));

    struct path_parts pp;
    Harlin_PathParse("/x/y/z", &pp);
    ASSERT(pp.count == 3);
    ASSERT(pp.is_absolute == 1);
    ASSERT(pp.seg_len[0] == 1);

    pr_info("path_walk: test OK");
    return 0;
}
