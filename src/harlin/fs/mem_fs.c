#include "mem_fs.h"
#include "path_walk.h"
#include "perm.h"
#include "printk.h"
#include "bug.h"

static struct mem_fs_node nodes[MEM_FS_NODES];

static u32 alloc_node(void)
{
    for (u32 i = 0; i < MEM_FS_NODES; i++) {
        if (!nodes[i].used) {
            nodes[i].used = 1;
            nodes[i].type = 0;
            nodes[i].name_len = 0;
            nodes[i].parent = (u32)-1;
            nodes[i].first_child = (u32)-1;
            nodes[i].next_sibling = (u32)-1;
            nodes[i].mode = 0;
            nodes[i].owner = 0;
            nodes[i].group = 0;
            nodes[i].payload_size = 0;
            return i;
        }
    }
    return (u32)-1;
}

static void free_node(u32 idx)
{
    if (idx >= MEM_FS_NODES) return;
    nodes[idx].used = 0;
    nodes[idx].type = 0;
    nodes[idx].name_len = 0;
    nodes[idx].parent = (u32)-1;
    nodes[idx].first_child = (u32)-1;
    nodes[idx].next_sibling = (u32)-1;
    nodes[idx].payload_size = 0;
}

static int name_eq(const char* a, u32 a_len, const char* b, u32 b_len)
{
    if (a_len != b_len) return 0;
    for (u32 i = 0; i < a_len; i++) {
        if (a[i] != b[i]) return 0;
    }
    return 1;
}

static u32 find_child(u32 parent, const char* name, u32 name_len)
{
    u32 c = nodes[parent].first_child;
    while (c != (u32)-1) {
        if (name_eq(nodes[c].name, nodes[c].name_len, name, name_len)) {
            return c;
        }
        c = nodes[c].next_sibling;
    }
    return (u32)-1;
}

static u32 resolve_path(const char* path)
{
    struct path_parts p;
    if (Harlin_PathParse(path, &p) != 0) return (u32)-1;
    u32 cur = 0;
    if (!p.is_absolute) {
        cur = 0;
    }
    for (u32 i = 0; i < p.count; i++) {
        u32 n = find_child(cur, p.seg[i], p.seg_len[i]);
        if (n == (u32)-1) return (u32)-1;
        cur = n;
    }
    return cur;
}

u32 Harlin_MemFsLookupNode(const char* path)
{
    return resolve_path(path);
}

static u32 resolve_parent_create(const char* path, const char** out_name, u32* out_name_len)
{
    struct path_parts p;
    if (Harlin_PathParse(path, &p) != 0) return (u32)-1;
    if (p.count == 0) return (u32)-1;
    u32 cur = 0;
    if (!p.is_absolute) cur = 0;
    for (u32 i = 0; i + 1 < p.count; i++) {
        u32 n = find_child(cur, p.seg[i], p.seg_len[i]);
        if (n == (u32)-1) return (u32)-1;
        cur = n;
    }
    if (p.seg_len[p.count - 1] >= MEM_FS_NAME_MAX) return (u32)-1;
    *out_name = p.seg[p.count - 1];
    *out_name_len = p.seg_len[p.count - 1];
    return cur;
}

int Harlin_MemFsInit(void)
{
    for (u32 i = 0; i < MEM_FS_NODES; i++) free_node(i);
    u32 root = alloc_node();
    if (root != 0) {
        pr_err("mem_fs: root alloc failed");
        return -1;
    }
    nodes[root].type = MEM_FS_DIR;
    nodes[root].name[0] = '/';
    nodes[root].name_len = 1;
    nodes[root].parent = root;
    nodes[root].mode = PERM_DEFAULT_DIR;
    nodes[root].owner = 0;
    nodes[root].group = 0;
    pr_info("mem_fs: %d nodes ready, root=0", MEM_FS_NODES);
    return 0;
}

int Harlin_MemFsMkdir(const char* path)
{
    const char* name;
    u32 name_len;
    u32 parent = resolve_parent_create(path, &name, &name_len);
    if (parent == (u32)-1) return -1;
    if (nodes[parent].type != MEM_FS_DIR) return -1;
    if (find_child(parent, name, name_len) != (u32)-1) return -1;
    u32 n = alloc_node();
    if (n == (u32)-1) return -1;
    nodes[n].type = MEM_FS_DIR;
    nodes[n].name_len = (u8)name_len;
    for (u32 i = 0; i < name_len; i++) nodes[n].name[i] = name[i];
    nodes[n].parent = parent;
    nodes[n].next_sibling = nodes[parent].first_child;
    nodes[parent].first_child = n;
    nodes[n].mode = PERM_DEFAULT_DIR;
    return 0;
}

int Harlin_MemFsRmdir(const char* path)
{
    u32 n = resolve_path(path);
    if (n == (u32)-1 || n == 0) return -1;
    if (nodes[n].type != MEM_FS_DIR) return -1;
    if (nodes[n].first_child != (u32)-1) return -1;
    u32 parent = nodes[n].parent;
    if (nodes[parent].first_child == n) {
        nodes[parent].first_child = nodes[n].next_sibling;
    } else {
        u32 c = nodes[parent].first_child;
        while (c != (u32)-1 && nodes[c].next_sibling != n) {
            c = nodes[c].next_sibling;
        }
        if (c != (u32)-1) nodes[c].next_sibling = nodes[n].next_sibling;
    }
    free_node(n);
    return 0;
}

int Harlin_MemFsCreate(const char* path)
{
    const char* name;
    u32 name_len;
    u32 parent = resolve_parent_create(path, &name, &name_len);
    if (parent == (u32)-1) return -1;
    if (nodes[parent].type != MEM_FS_DIR) return -1;
    if (find_child(parent, name, name_len) != (u32)-1) return -1;
    u32 n = alloc_node();
    if (n == (u32)-1) return -1;
    nodes[n].type = MEM_FS_FILE;
    nodes[n].name_len = (u8)name_len;
    for (u32 i = 0; i < name_len; i++) nodes[n].name[i] = name[i];
    nodes[n].parent = parent;
    nodes[n].next_sibling = nodes[parent].first_child;
    nodes[parent].first_child = n;
    nodes[n].mode = PERM_DEFAULT_FILE;
    return 0;
}

int Harlin_MemFsRemove(const char* path)
{
    u32 n = resolve_path(path);
    if (n == (u32)-1 || n == 0) return -1;
    u32 parent = nodes[n].parent;
    if (nodes[parent].first_child == n) {
        nodes[parent].first_child = nodes[n].next_sibling;
    } else {
        u32 c = nodes[parent].first_child;
        while (c != (u32)-1 && nodes[c].next_sibling != n) {
            c = nodes[c].next_sibling;
        }
        if (c != (u32)-1) nodes[c].next_sibling = nodes[n].next_sibling;
    }
    free_node(n);
    return 0;
}

int Harlin_MemFsWrite(const char* path, const void* data, u32 size)
{
    u32 n = resolve_path(path);
    if (n == (u32)-1) return -1;
    if (nodes[n].type != MEM_FS_FILE) return -1;
    if (size > MEM_FS_PAYLOAD) size = MEM_FS_PAYLOAD;
    const u8* s = (const u8*)data;
    for (u32 i = 0; i < size; i++) nodes[n].payload[i] = s[i];
    nodes[n].payload_size = size;
    return (int)size;
}

int Harlin_MemFsRead(const char* path, void* buf, u32 size)
{
    u32 n = resolve_path(path);
    if (n == (u32)-1) return -1;
    if (nodes[n].type != MEM_FS_FILE) return -1;
    if (size > nodes[n].payload_size) size = nodes[n].payload_size;
    u8* d = (u8*)buf;
    for (u32 i = 0; i < size; i++) d[i] = nodes[n].payload[i];
    return (int)size;
}

int Harlin_MemFsLs(const char* path, struct mem_fs_dir_entry* out, u32 max)
{
    u32 n = resolve_path(path);
    if (n == (u32)-1) return -1;
    if (nodes[n].type != MEM_FS_DIR) return -1;
    u32 count = 0;
    u32 c = nodes[n].first_child;
    while (c != (u32)-1 && count < max) {
        if (out) {
            u32 l = nodes[c].name_len;
            if (l >= MEM_FS_NAME_MAX) l = MEM_FS_NAME_MAX - 1;
            for (u32 i = 0; i < l; i++) out[count].name[i] = nodes[c].name[i];
            out[count].name[l] = 0;
            out[count].name_len = (u8)l;
            out[count].type = nodes[c].type;
            out[count].node = c;
        }
        count++;
        c = nodes[c].next_sibling;
    }
    return (int)count;
}

int Harlin_MemFsTest(void)
{
    ASSERT(Harlin_MemFsInit() == 0);

    ASSERT(Harlin_MemFsMkdir("/a") == 0);
    ASSERT(Harlin_MemFsMkdir("/a/b") == 0);
    ASSERT(Harlin_MemFsMkdir("/a/c") == 0);
    ASSERT(Harlin_MemFsMkdir("/d") == 0);

    struct mem_fs_dir_entry ents[8];
    int n = Harlin_MemFsLs("/", ents, 8);
    ASSERT(n == 2);

    n = Harlin_MemFsLs("/a", ents, 8);
    ASSERT(n == 2);

    ASSERT(Harlin_MemFsCreate("/a/b/file1") == 0);
    ASSERT(Harlin_MemFsCreate("/a/b/file2") == 0);

    const char* hello = "hello world";
    ASSERT(Harlin_MemFsWrite("/a/b/file1", hello, 12) == 12);
    char buf[32];
    ASSERT(Harlin_MemFsRead("/a/b/file1", buf, 32) == 12);
    for (int i = 0; i < 12; i++) ASSERT(buf[i] == hello[i]);

    ASSERT(Harlin_MemFsMkdir("/a/b/file1") != 0);
    ASSERT(Harlin_MemFsRemove("/a/b/file1") == 0);
    ASSERT(Harlin_MemFsRemove("/a/b/file1") != 0);

    ASSERT(Harlin_MemFsRmdir("/a/b") != 0);
    ASSERT(Harlin_MemFsRemove("/a/b/file2") == 0);
    ASSERT(Harlin_MemFsRmdir("/a/b") == 0);

    n = Harlin_MemFsLs("/", ents, 8);
    ASSERT(n == 2);

    pr_info("mem_fs: test OK (ls root=%d)", n);
    return 0;
}
