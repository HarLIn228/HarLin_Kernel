#ifndef PIPE_H
#define PIPE_H

#include "harlin_API.h"

int  pipe_init(void);
int  pipe_create(void);
int  pipe_read(int id, void* buf, u32 len);
int  pipe_write(int id, const void* buf, u32 len);
int  pipe_ready(int id);
int  pipe_space(int id);
int  pipe_read_blocking(int id, void* buf, u32 len);
int  pipe_write_blocking(int id, const void* buf, u32 len);
void pipe_close(int id);

#define Harlin_PipeInit               pipe_init
#define Harlin_PipeCreate             pipe_create
#define Harlin_PipeRead               pipe_read
#define Harlin_PipeWrite              pipe_write
#define Harlin_PipeReady              pipe_ready
#define Harlin_PipeSpace              pipe_space
#define Harlin_PipeReadBlocking       pipe_read_blocking
#define Harlin_PipeWriteBlocking      pipe_write_blocking
#define Harlin_PipeClose              pipe_close

#endif
