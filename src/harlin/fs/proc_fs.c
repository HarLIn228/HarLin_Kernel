#include "harlin_API.h"
#include "proc_fs.h"
#include "printk.h"
#include "bug.h"
#include "smp.h"
#include "pmm.h"

static u64 boot_ticks;

int Harlin_ProcFsInit(void)
{
    boot_ticks = 0;
    return 0;
}

void Harlin_ProcFsTick(void)
{
    boot_ticks++;
}

static int streq(const char* a, const char* b)
{
    while (*a && *b) {
        if (*a != *b) return 0;
        a++; b++;
    }
    return *a == 0 && *b == 0;
}

static int append_num(char* out, u32 out_size, int i, u64 n)
{
    if (n == 0) {
        if (i + 1 >= (int)out_size) return -1;
        out[i++] = '0';
        return i;
    }
    char tmp[24]; int t = 0;
    while (n > 0 && t < 23) {
        tmp[t++] = (char)('0' + (int)(n % 10));
        n /= 10;
    }
    while (t > 0) {
        if (i + 1 >= (int)out_size) return -1;
        out[i++] = tmp[--t];
    }
    return i;
}

static int append_str(char* out, u32 out_size, int i, const char* s)
{
    while (*s) {
        if (i + 1 >= (int)out_size) return -1;
        out[i++] = *s++;
    }
    return i;
}

static int read_uptime(char* out, u32 out_size)
{
    int i = 0;
    i = append_str(out, out_size, i, "uptime_ticks=");
    if (i < 0) return -1;
    i = append_num(out, out_size, i, boot_ticks);
    if (i < 0) return -1;
    if (i >= (int)out_size) return -1;
    out[i] = 0;
    return i;
}

static int read_meminfo(char* out, u32 out_size)
{
    int i = 0;
    i = append_str(out, out_size, i, "free=");
    if (i < 0) return -1;
    i = append_num(out, out_size, i, Harlin_PmmTotalPages());
    if (i < 0) return -1;
    i = append_str(out, out_size, i, " used=");
    if (i < 0) return -1;
    i = append_num(out, out_size, i, 0);
    if (i < 0) return -1;
    if (i >= (int)out_size) return -1;
    out[i] = 0;
    return i;
}

static int read_cpuinfo(char* out, u32 out_size)
{
    int i = 0;
    i = append_str(out, out_size, i, "cpus=");
    if (i < 0) return -1;
    i = append_num(out, out_size, i, (u64)Harlin_SmpCpuCount());
    if (i < 0) return -1;
    if (i >= (int)out_size) return -1;
    out[i] = 0;
    return i;
}

static int read_loadavg(char* out, u32 out_size)
{
    int i = 0;
    i = append_str(out, out_size, i, "load=0.0");
    if (i < 0) return -1;
    if (i >= (int)out_size) return -1;
    out[i] = 0;
    return i;
}

int Harlin_ProcFsRead(const char* path, char* out, u32 out_size)
{
    if (!path || !out || out_size == 0) return -1;
    if (path[0] == '/') path++;
    if (streq(path, "proc/uptime"))   return read_uptime(out, out_size);
    if (streq(path, "proc/meminfo"))  return read_meminfo(out, out_size);
    if (streq(path, "proc/cpuinfo"))  return read_cpuinfo(out, out_size);
    if (streq(path, "proc/loadavg"))  return read_loadavg(out, out_size);
    return -1;
}

int Harlin_ProcFsLs(const char* path, char* out, u32 out_size)
{
    if (!path || !out || out_size == 0) return -1;
    if (streq(path, "/proc") || streq(path, "proc")) {
        int i = 0;
        i = append_str(out, out_size, i, "uptime meminfo cpuinfo loadavg");
        if (i < 0) return -1;
        out[i] = 0;
        return i;
    }
    return -1;
}

void Harlin_ProcFsTest(void)
{
    char buf[64];
    int n;

    Harlin_ProcFsInit();
    n = Harlin_ProcFsRead("/proc/uptime", buf, sizeof(buf));
    ASSERT(n > 0);
    ASSERT(buf[0] == 'u');

    n = Harlin_ProcFsRead("/proc/meminfo", buf, sizeof(buf));
    ASSERT(n > 0);

    n = Harlin_ProcFsRead("/proc/cpuinfo", buf, sizeof(buf));
    ASSERT(n > 0);

    n = Harlin_ProcFsRead("/proc/loadavg", buf, sizeof(buf));
    ASSERT(n > 0);

    n = Harlin_ProcFsRead("/proc/nonexistent", buf, sizeof(buf));
    ASSERT(n < 0);

    n = Harlin_ProcFsLs("/proc", buf, sizeof(buf));
    ASSERT(n > 0);

    pr_info("proc_fs: test OK");
}
