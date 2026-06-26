#include "printk.h"
#include "harlin_API.h"

#define MAX_FRAMES 16

static u64 read_ul(const u64* p)
{
    return *p;
}

void show_stack(unsigned long max_frames)
{
    u64 rbp;
    u64 ret;
    u64 frames;

    asm volatile ("mov %%rbp, %0" : "=r"(rbp));

    if (max_frames == 0) max_frames = MAX_FRAMES;
    if (max_frames > MAX_FRAMES) max_frames = MAX_FRAMES;

    pr_info("stack backtrace (rbp=%p):", (void*)rbp);
    for (frames = 0; frames < max_frames; frames++) {
        if (rbp == 0) break;
        if (rbp < 0x1000) break;
        if ((rbp & 0x7) != 0) {
            pr_info("  [%2llu] <unaligned rbp=%p>", (unsigned long long)frames, (void*)rbp);
            break;
        }
        ret = read_ul((u64*)(rbp + 8));
        pr_info("  [%2llu] ret=%p rbp=%p", (unsigned long long)frames, (void*)ret, (void*)rbp);
        rbp = read_ul((u64*)rbp);
    }
}
