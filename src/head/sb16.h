#ifndef SB16_H
#define SB16_H

#include "harlin_API.h"

#define SB16_BASE       0x220
#define SB16_IRQ        5
#define SB16_DMA_16BIT  5

int  sb16_init(void);
int  sb16_play(const u8* data, u32 len, u16 sample_rate, int stereo);
void sb16_stop(void);
int  sb16_is_playing(void);

#endif
