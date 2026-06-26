#include "cow_fs.h"
#include "mem_fs.h"
#include "printk.h"
#include "bug.h"

static struct cow_version versions[COW_MAX_VERSIONS];
static u64 ver_counter;

static u32 find_ver(u32 file_node, const char* ver)
{
    for (u32 i = 0; i < COW_MAX_VERSIONS; i++) {
        if (versions[i].used && versions[i].file_node == file_node) {
            u32 j = 0;
            while (j < COW_VER_STR_LEN && versions[i].ver[j] == ver[j]) {
                if (ver[j] == 0) break;
                j++;
            }
            if (versions[i].ver[j] == ver[j]) return i;
        }
    }
    return (u32)-1;
}

static u32 alloc_ver(u32 file_node)
{
    for (u32 i = 0; i < COW_MAX_VERSIONS; i++) {
        if (!versions[i].used) {
            versions[i].used = 1;
            versions[i].file_node = file_node;
            versions[i].payload_size = 0;
            for (u32 j = 0; j < COW_VER_STR_LEN; j++) versions[i].ver[j] = 0;
            return i;
        }
    }
    return (u32)-1;
}

static void make_ver_name(char* dst, u64 n)
{
    dst[0] = 'v';
    dst[1] = (char)('0' + (int)(n % 10));
    n /= 10;
    dst[2] = (char)('0' + (int)(n % 10));
    n /= 10;
    dst[3] = (char)('0' + (int)(n % 10));
    dst[4] = 0;
}

int Harlin_CowFsInit(void)
{
    for (u32 i = 0; i < COW_MAX_VERSIONS; i++) {
        versions[i].used = 0;
        versions[i].file_node = (u32)-1;
        versions[i].payload_size = 0;
    }
    ver_counter = 0;
    pr_info("cow_fs: %d version slots", COW_MAX_VERSIONS);
    return 0;
}

int Harlin_CowSnapshot(const char* path, char* out_ver)
{
    u32 n = Harlin_MemFsLookupNode(path);
    if (n == (u32)-1) return -1;

    ver_counter++;
    u32 v = alloc_ver(n);
    if (v == (u32)-1) return -1;
    make_ver_name(versions[v].ver, ver_counter);

    for (u32 i = 0; i < COW_VER_STR_LEN; i++) out_ver[i] = versions[v].ver[i];
    return 0;
}

int Harlin_CowWrite(const char* path, const char* expected_ver,
                    const void* data, u32 size)
{
    u32 n = Harlin_MemFsLookupNode(path);
    if (n == (u32)-1) return -1;

    u32 slot = (u32)-1;
    if (expected_ver && expected_ver[0] != 0) {
        slot = find_ver(n, expected_ver);
        if (slot == (u32)-1) return -1;
    } else {
        slot = alloc_ver(n);
        if (slot == (u32)-1) return -1;
        ver_counter++;
        make_ver_name(versions[slot].ver, ver_counter);
    }

    if (size > MEM_FS_PAYLOAD) size = MEM_FS_PAYLOAD;
    const u8* s = (const u8*)data;
    for (u32 i = 0; i < size; i++) versions[slot].payload[i] = s[i];
    versions[slot].payload_size = size;
    versions[slot].file_node = n;
    return (int)size;
}

int Harlin_CowReadAt(const char* path, const char* ver, void* buf, u32 size)
{
    u32 n = Harlin_MemFsLookupNode(path);
    if (n == (u32)-1) return -1;
    u32 slot = find_ver(n, ver);
    if (slot == (u32)-1) return -1;
    if (size > versions[slot].payload_size) size = versions[slot].payload_size;
    u8* d = (u8*)buf;
    for (u32 i = 0; i < size; i++) d[i] = versions[slot].payload[i];
    return (int)size;
}

int Harlin_CowFork(const char* src_path, const char* dst_path)
{
    u32 src = Harlin_MemFsLookupNode(src_path);
    if (src == (u32)-1) return -1;
    if (Harlin_MemFsCreate(dst_path) != 0) return -1;
    u32 dst = Harlin_MemFsLookupNode(dst_path);
    if (dst == (u32)-1) return -1;

    for (u32 i = 0; i < COW_MAX_VERSIONS; i++) {
        if (versions[i].used && versions[i].file_node == src) {
            u32 nv = alloc_ver(dst);
            if (nv == (u32)-1) return -1;
            for (u32 k = 0; k < COW_VER_STR_LEN; k++) versions[nv].ver[k] = versions[i].ver[k];
            versions[nv].payload_size = versions[i].payload_size;
            for (u32 k = 0; k < versions[i].payload_size; k++) {
                versions[nv].payload[k] = versions[i].payload[k];
            }
            versions[nv].file_node = dst;
        }
    }
    return 0;
}

int Harlin_CowTest(void)
{
    ASSERT(Harlin_MemFsInit() == 0);
    ASSERT(Harlin_CowFsInit() == 0);

    ASSERT(Harlin_MemFsCreate("/file") == 0);

    char v0[COW_VER_STR_LEN];
    ASSERT(Harlin_CowSnapshot("/file", v0) == 0);

    const char* data1 = "first version content";
    ASSERT(Harlin_CowWrite("/file", v0, data1, 21) == 21);

    char v1[COW_VER_STR_LEN];
    ASSERT(Harlin_CowSnapshot("/file", v1) == 0);

    char buf[64];
    ASSERT(Harlin_CowReadAt("/file", v0, buf, 64) == 0);
    ASSERT(Harlin_CowReadAt("/file", v1, buf, 64) == 21);
    for (int i = 0; i < 21; i++) ASSERT(buf[i] == data1[i]);

    ASSERT(Harlin_CowWrite("/file", v0, "stale", 5) != 21);

    const char* data2 = "second version";
    ASSERT(Harlin_CowWrite("/file", v1, data2, 14) == 14);

    char v2[COW_VER_STR_LEN];
    ASSERT(Harlin_CowSnapshot("/file", v2) == 0);
    ASSERT(Harlin_CowReadAt("/file", v2, buf, 64) == 14);
    for (int i = 0; i < 14; i++) ASSERT(buf[i] == data2[i]);

    ASSERT(Harlin_CowFork("/file", "/file_clone") == 0);
    char buf2[64];
    ASSERT(Harlin_CowReadAt("/file_clone", v2, buf2, 64) == 14);
    for (int i = 0; i < 14; i++) ASSERT(buf2[i] == data2[i]);

    const char* data3 = "forked write";
    ASSERT(Harlin_CowWrite("/file_clone", v2, data3, 12) == 12);
    ASSERT(Harlin_CowReadAt("/file", v2, buf, 64) == 14);
    for (int i = 0; i < 14; i++) ASSERT(buf[i] == data2[i]);

    pr_info("cow_fs: test OK");
    return 0;
}
