#include "audio.h"
#include "sb16.h"
#include "wav.h"

int Harlin_AudioInit(void)
{
    return sb16_init();
}

int Harlin_AudioPlayWav(const u8* data, u32 len)
{
    struct wav_header hdr;
    const u8* pcm;
    u32 pcm_len;
    int r;

    r = wav_parse(data, len, &hdr, &pcm, &pcm_len);
    if (r != HARLIN_OK)
        return r;

    if (hdr.bits_per_sample != 16)
        return HARLIN_UNSUPPORTED;

    return sb16_play(pcm, pcm_len, (u16)hdr.sample_rate, hdr.num_channels == 2);
}

void Harlin_AudioStop(void)
{
    sb16_stop();
}

int Harlin_AudioIsPlaying(void)
{
    return sb16_is_playing();
}
