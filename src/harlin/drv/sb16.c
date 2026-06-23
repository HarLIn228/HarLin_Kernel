#include "sb16.h"
#include "io.h"
#include "interrupt.h"
#include "pmm.h"
#include "vmm.h"
#include "pci.h"

static u16 sb16_base = 0x220;
static u8  sb16_irq = 5;
static u8  sb16_dma = 5;
static int sb16_present = 0;

static const u8 sb16_irq_options[] = { 5, 7, 10, 11 };
static const u8 sb16_dma_options[] = { 5, 6, 7 };

#define SB_MIXER_ADDR   (sb16_base + 0x04)
#define SB_MIXER_DATA   (sb16_base + 0x05)
#define SB_RESET        (sb16_base + 0x06)
#define SB_READ         (sb16_base + 0x0A)
#define SB_WRITE        (sb16_base + 0x0C)
#define SB_READ_STATUS  (sb16_base + 0x0E)
#define SB_ACK_16       (sb16_base + 0x0F)

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

static const u16 sb16_probe_bases[] = { 0x220, 0x240, 0x260, 0x280 };

static int sb16_probe(u16 base)
{
    u8 a, b;
    int i;
    u16 saved_base = sb16_base;
    sb16_base = base;
    outb(SB_RESET, 1);
    for (i = 0; i < 1000; i++) inb(SB_RESET);
    outb(SB_RESET, 0);
    {
        u32 t;
        for (t = 0; t < 1000000; t++) {
            if (inb(SB_READ_STATUS) & 0x80) break;
        }
    }
    a = inb(SB_READ);
    b = inb(SB_READ);
    if (a == 0xAA || b == 0xAA) {
        return 1;
    }
    sb16_base = saved_base;
    return 0;
}

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
    outb(SB_MIXER_DATA, (u8)(((u16)sb16_irq << 4) | (sb16_dma & 0x07)));
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

    outb(DMA16_MASK, sb16_dma | 0x04);
    outb(DMA16_CLEAR, 0);
    outb(DMA16_MODE, 0x49 | (sb16_dma & 0x03));
    outb(DMA16_ADDR, word_addr & 0xFF);
    outb(DMA16_ADDR, (word_addr >> 8) & 0xFF);
    outb(DMA16_PAGE, page);
    outb(DMA16_COUNT, count & 0xFF);
    outb(DMA16_COUNT, (count >> 8) & 0xFF);
    outb(DMA16_MASK, sb16_dma & 0x03);
}

int sb16_init(void)
{
    u32 i;
    int p;
    int found = 0;
    struct pci_device dev;
    int q;
    int has_native = 0;
    if (pci_find_class(0x04, 0x01, &dev, 0) >= 0) {
        pci_enable_busmaster(&dev);
        if (dev.irq_line != 0 && dev.irq_line != 0xFF) {
            int matched = 0;
            for (q = 0; q < (int)(sizeof(sb16_irq_options) / sizeof(sb16_irq_options[0])); q++) {
                if (sb16_irq_options[q] == dev.irq_line) {
                    matched = 1;
                    break;
                }
            }
            if (matched) {
                sb16_irq = dev.irq_line;
                has_native = 1;
            }
        }
    }
    (void)has_native;
    for (p = 0; p < (int)(sizeof(sb16_probe_bases) / sizeof(sb16_probe_bases[0])); p++) {
        if (sb16_probe(sb16_probe_bases[p])) {
            found = 1;
            break;
        }
    }
    if (!found)
        return HARLIN_UNSUPPORTED;

    sb16_set_irq_dma();
    sb16_dsp_write(0xD1);

    irq_register(sb16_irq, sb16_irq_handler);

    dma_buffer_phys = pmm_alloc_contiguous_low(SB16_DMA_BUFFER_PAGES);
    if (!dma_buffer_phys)
        return HARLIN_NO_MEMORY;

    for (i = 0; i < SB16_DMA_BUFFER_PAGES; i++) {
        vmm_map((u64)(dma_buffer_virt + i * 4096),
                dma_buffer_phys + i * 4096,
                VMM_PRESENT | VMM_WRITABLE);
    }

    sb16_present = 1;
    return HARLIN_OK;
}

int sb16_is_present(void)
{
    return sb16_present;
}

int sb16_play(const u8* data, u32 len, u16 sample_rate, int stereo)
{
    u32 len_words;
    u8 mode;

    if (!data || len == 0 || len > SB16_DMA_BUFFER_SIZE)
        return HARLIN_INVALID;

    if (sb16_is_playing())
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
