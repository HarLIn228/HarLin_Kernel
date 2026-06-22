#ifndef AUDIO_H
#define AUDIO_H

#include "harlin_API.h"

int  Harlin_AudioInit(void);
int  Harlin_AudioPlayWav(const u8* data, u32 len);
void Harlin_AudioStop(void);
int  Harlin_AudioIsPlaying(void);

#endif
