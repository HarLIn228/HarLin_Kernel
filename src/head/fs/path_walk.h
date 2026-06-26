#ifndef PATH_WALK_H
#define PATH_WALK_H

#include "harlin_API.h"

#define PATH_MAX_SEGMENTS  8
#define PATH_MAX_CHARS    128

struct path_parts {
    u32 count;
    const char* seg[PATH_MAX_SEGMENTS];
    u32  seg_len[PATH_MAX_SEGMENTS];
    int  is_absolute;
    char buf[PATH_MAX_CHARS];
};

int  Harlin_PathParse(const char* in, struct path_parts* out);
int  Harlin_PathNormalize(const char* in, char* out, u32 out_size);
int  Harlin_PathTest(void);

#endif
