#include "sb16.h"
#include "io.h"
#include "interrupt.h"
#include "pmm.h"
#include "vmm.h"

#define SB_MIXER_ADDR   (SB16_BASE + 0x04)
#define SB_MIXER_DATA   (SB16_BASE + 0x05)
#define SB_RESET        (SB16_BASE + 0x06)
#define SB_READ         (SB16_BASE + 0x0A)
#define SB_WRITE        (SB16_BASE + 0x0C)
#define SB_READ_STATUS  (SB16_BASE + 0x0E)
#define SB_ACK_16       (SB16_BASE + 0x0F)

#define DMA16_MASK      0xD4
#define DMA16_CLEAR     0xD8
#define DMA16_MODE      0xD6
#define DMA16_ADDR      0xC4
#define DMA16_COUNT     0xC6
#define DMA16_PAGE      0x8B

#define SB16_DMA_BUFFER_PAGES 16
#define SB16_DMA_BUFFER_SIZE  (SB16_DMA_BUFFER_PAGES * 4096)
#define SB16_DMA_VIRT_BASE    0xFFFF800010000000

static volatile int sb16_playing = 0;
static u64 dma_buffer_phys = 0;
static u8* dma_buffer_virt = (u8*)SB16_DMA_VIRT_BASE;

static void sb16_dsp_write(u8 val)
{
    while ((inb(SB_WRITE) & 0x80));
    outb(SB_WRITE, val);
}

static u8 sb16_dsp_read(void)
{
    while (!(inb(SB_READ_STATUS) & 0x80));
    return inb(SB_READ);
}

static void sb16_reset(void)
{
    int i;
    outb(SB_RESET, 1);
    for (i = 0; i < 1000; i++) inb(SB_RESET);
    outb(SB_RESET, 0);
}

static void sb16_set_irq_dma(void)
{
    outb(SB_MIXER_ADDR, 0x80);
    outb(SB_MIXER_DATA, (SB16_IRQ << 4) | (SB16_DMA_16BIT & 0x07));
}

static void sb16_set_sample_rate(u16 rate)
{
    sb16_dsp_write(0x41);
    sb16_dsp_write((rate >> 8) & 0xFF);
    sb16_dsp_write(rate & 0xFF);
}

static void sb16_irq_handler(void)
{
    inb(SB_ACK_16);
    sb16_playing = 0;
}

static void dma16_setup(u64 phys, u32 len_words)
{
    u16 word_addr;
    u16 count;
    u8 page;

    word_addr = (u16)((phys >> 1) & 0xFFFF);
    count = (u16)((len_words - 1) & 0xFFFF);
    page = (u8)((phys >> 16) & 0xFF);

    outb(DMA16_MASK, SB16_DMA_16BIT | 0x04);
    outb(DMA16_CLEAR, 0);
    outb(DMA16_MODE, 0x49 | (SB16_DMA_16BIT & 0x03));
    outb(DMA16_ADDR, word_addr & 0xFF);
    outb(DMA16_ADDR, (word_addr >> 8) & 0xFF);
    outb(DMA16_PAGE, page);
    outb(DMA16_COUNT, count & 0xFF);
    outb(DMA16_COUNT, (count >> 8) & 0xFF);
    outb(DMA16_MASK, SB16_DMA_16BIT & 0x03);
}

int sb16_init(void)
{
    u32 i;

    sb16_reset();
    if (sb16_dsp_read() != 0xAA)
        return HARLIN_UNSUPPORTED;

    sb16_set_irq_dma();
    sb16_dsp_write(0xD1);

    irq_register(SB16_IRQ, sb16_irq_handler);

    dma_buffer_phys = pmm_alloc_contiguous(SB16_DMA_BUFFER_PAGES);
    if (!dma_buffer_phys)
        return HARLIN_NO_MEMORY;

    for (i = 0; i < SB16_DMA_BUFFER_PAGES; i++) {
        vmm_map((u64)(dma_buffer_virt + i * 4096),
                dma_buffer_phys + i * 4096,
                VMM_PRESENT | VMM_WRITABLE);
    }

    return HARLIN_OK;
}

int sb16_play(const u8* data, u32 len, u16 sample_rate, int stereo)
{
    u32 len_words;
    u8 mode;

    if (!data || len == 0 || len > SB16_DMA_BUFFER_SIZE)
        return HARLIN_INVALID;

    if (len & 1)
        len++;

    sb16_stop();

    Harlin_Copy(dma_buffer_virt, data, len);

    len_words = len / 2;
    mode = 0xB8;
    if (stereo)
        mode |= 0x04;

    sb16_set_sample_rate(sample_rate);
    dma16_setup(dma_buffer_phys, len_words);

    sb16_playing = 1;
    sb16_dsp_write(mode);
    sb16_dsp_write(0x00);
    sb16_dsp_write((len_words - 1) & 0xFF);
    sb16_dsp_write(((len_words - 1) >> 8) & 0xFF);

    return HARLIN_OK;
}

void sb16_stop(void)
{
    sb16_dsp_write(0xD5);
    sb16_playing = 0;
}

int sb16_is_playing(void)
{
    return sb16_playing;
}
