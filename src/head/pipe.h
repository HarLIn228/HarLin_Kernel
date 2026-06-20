#ifndef PIPE_H
#define PIPE_H

#include "harlin_API.h"

int  pipe_init(void);
int  pipe_create(void);
int  pipe_read(int id, void* buf, u32 len);
int  pipe_write(int id, const void* buf, u32 len);
int  pipe_ready(int id);
void pipe_close(int id);

#endif
