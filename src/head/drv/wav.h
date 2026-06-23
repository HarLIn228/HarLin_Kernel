#ifndef WAV_H
#define WAV_H

#include "harlin_API.h"

struct wav_header {
    u32 chunk_id;
    u32 chunk_size;
    u32 format;
    u32 subchunk1_id;
    u32 subchunk1_size;
    u16 audio_format;
    u16 num_channels;
    u32 sample_rate;
    u32 byte_rate;
    u16 block_align;
    u16 bits_per_sample;
    u32 subchunk2_id;
    u32 subchunk2_size;
};

int wav_parse(const u8* data, u32 len, struct wav_header* out, const u8** pcm, u32* pcm_len);

#endif
