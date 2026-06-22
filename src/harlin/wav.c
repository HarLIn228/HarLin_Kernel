#include "wav.h"

#define RIFF_ID 0x46464952
#define WAVE_ID 0x45564157
#define FMT_ID  0x20746D66
#define DATA_ID 0x61746164

static u32 read_u32(const u8* p)
{
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

static u16 read_u16(const u8* p)
{
    return (u16)p[0] | ((u16)p[1] << 8);
}

int wav_parse(const u8* data, u32 len, struct wav_header* out, const u8** pcm, u32* pcm_len)
{
    const u8* p;
    u32 id;
    u32 size;
    u32 skip;

    if (!data || len < 44 || !out || !pcm || !pcm_len)
        return HARLIN_INVALID;

    p = data;

    out->chunk_id     = read_u32(p); p += 4;
    out->chunk_size   = read_u32(p); p += 4;
    out->format       = read_u32(p); p += 4;

    if (out->chunk_id != RIFF_ID || out->format != WAVE_ID)
        return HARLIN_UNSUPPORTED;

    out->subchunk1_id     = read_u32(p); p += 4;
    out->subchunk1_size   = read_u32(p); p += 4;
    out->audio_format     = read_u16(p); p += 2;
    out->num_channels     = read_u16(p); p += 2;
    out->sample_rate      = read_u32(p); p += 4;
    out->byte_rate        = read_u32(p); p += 4;
    out->block_align      = read_u16(p); p += 2;
    out->bits_per_sample  = read_u16(p); p += 2;

    if (out->subchunk1_id != FMT_ID || out->audio_format != 1)
        return HARLIN_UNSUPPORTED;

    skip = out->subchunk1_size;
    if (skip >= 16 && skip < 64)
        p += (skip - 16);

    while ((u32)(p - data) < len - 8) {
        id   = read_u32(p); p += 4;
        size = read_u32(p); p += 4;
        if (id == DATA_ID) {
            *pcm = p;
            *pcm_len = size;
            out->subchunk2_id   = id;
            out->subchunk2_size = size;
            return HARLIN_OK;
        }
        p += size;
    }

    return HARLIN_NOT_FOUND;
}
