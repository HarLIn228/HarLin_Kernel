#include "harlin_API.h"
#include "mem_fs.h"
#include "printk.h"
#include "bug.h"

extern int process_create_elf(const void* elf_data, u64 elf_size);

int Harlin_ExecFromBuffer(const void* elf_data, u32 elf_size)
{
    if (!elf_data || elf_size == 0) return -1;
    return process_create_elf(elf_data, elf_size);
}

int Harlin_ExecFromPath(const char* path)
{
    if (!path) return -1;
    u8 buf[1024];
    int n = Harlin_MemFsRead(path, buf, sizeof(buf));
    if (n <= 0) return -1;
    return process_create_elf(buf, (u32)n);
}

void Harlin_ExecTest(void)
{
    u8 fake_elf[16];
    for (int i = 0; i < 16; i++) fake_elf[i] = 0;
    fake_elf[0] = 0x7F; fake_elf[1] = 'E'; fake_elf[2] = 'L'; fake_elf[3] = 'F';
    int rc = Harlin_ExecFromBuffer(fake_elf, 16);
    ASSERT(rc < 0);

    ASSERT(Harlin_MemFsInit() == 0);
    ASSERT(Harlin_MemFsCreate("/app.elf") == 0);
    ASSERT(Harlin_MemFsWrite("/app.elf", fake_elf, 16) == 16);
    int rc2 = Harlin_ExecFromPath("/app.elf");
    ASSERT(rc2 < 0);

    int rc3 = Harlin_ExecFromPath("/nonexistent");
    ASSERT(rc3 < 0);

    pr_info("exec: test OK (mock invalid elf correctly rejected)");
}
