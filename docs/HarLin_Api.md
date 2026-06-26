# HarLin API 标准

## 1. 范围

本文档定义 HarLin 内核的应用程序编程接口，面向在 HarLin 之上开发软件的第三方开发者。

## 2. 约定

### 2.1 命名

- 公开的内核函数使用前缀 `Harlin_`。
- 公开的宏使用前缀 `HARLIN_`。
- 公开的类型使用小写短名：`u8`、`u16`、`u32`、`u64`、`s8`、`s16`、`s32`、`s64`。
- 常量使用大写和下划线。
- API 名称追求简短好记，避免晦涩缩写。

### 2.2 调用约定

- 所有 HarLin API 函数使用 System V AMD64 ABI。
- 前六个整数或指针参数通过 RDI、RSI、RDX、RCX、R8、R9 传递。
- 返回值放在 RAX 中。
- 内核在 API 调用过程中保存所有被调用者保存寄存器。

### 2.3 错误处理

- 可能失败的函数返回有符号 32 位整数。
- 返回值 `0` 表示成功。
- 负值表示错误。
- 常见错误码如下。

| 编码 | 名称 | 含义 |
|------|------|------|
| 0 | HARLIN_OK | 成功 |
| -1 | HARLIN_ERROR | 通用错误 |
| -2 | HARLIN_INVALID | 无效参数 |
| -3 | HARLIN_NO_MEMORY | 内存不足 |
| -4 | HARLIN_NOT_FOUND | 未找到资源 |
| -5 | HARLIN_TIMEOUT | 操作超时 |
| -6 | HARLIN_IO_ERROR | 输入输出错误 |
| -7 | HARLIN_UNSUPPORTED | 操作不支持 |
| -8 | HARLIN_ACCESS_DENIED | 权限不足 |
| -9 | HARLIN_BUSY | 资源忙 |

## 3. 类型定义

```c
typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;
typedef signed char        s8;
typedef signed short       s16;
typedef signed int         s32;
typedef signed long long   s64;
```

## 4. 控制台 API

```c
void Harlin_Clear(void);
void Harlin_PutChar(char c);
void Harlin_Print(const char* str);
void Harlin_PrintHex(u64 val);
void Harlin_PrintDec(s32 val);
void Harlin_SetColor(u8 fg, u8 bg);
```

## 5. 显示 API

```c
#define HARLIN_DISP_VGA_TEXT 0
#define HARLIN_DISP_VGA_13H  1
#define HARLIN_DISP_VESA     2

int  Harlin_SetMode(int mode);
void Harlin_ClearScreen(unsigned char color);
void Harlin_PutPixel(int x, int y, unsigned char color);
void Harlin_PutString(int x, int y, const char* str, unsigned char color);
```

## 6. 端口 I/O API

```c
u8   Harlin_PortIn8(u16 port);
u16  Harlin_PortIn16(u16 port);
u32  Harlin_PortIn32(u16 port);
void Harlin_PortOut8(u16 port, u8 data);
void Harlin_PortOut16(u16 port, u16 data);
void Harlin_PortOut32(u16 port, u32 data);
```

## 7. 中断控制 API

```c
void Harlin_IntOn(void);
void Harlin_IntOff(void);
```

## 8. 键盘 API

```c
#define HARLIN_KEY_SHIFT  0x01
#define HARLIN_KEY_CTRL   0x02
#define HARLIN_KEY_ALT    0x04
#define HARLIN_KEY_CAPS   0x08
#define HARLIN_KEY_NUM    0x10
#define HARLIN_KEY_SCROLL 0x20

#define HARLIN_LED_SCROLL 0x01
#define HARLIN_LED_NUM    0x02
#define HARLIN_LED_CAPS   0x04

int  Harlin_KeyAvail(void);
char Harlin_GetKey(void);
int  Harlin_KeyOverflow(void);
void Harlin_KeyFlush(void);
u8   Harlin_KeyState(void);
void Harlin_KeyLed(u8 leds);
```

## 9. 字符串与内存 API

```c
u32  Harlin_Len(const char* str);
void Harlin_CopyStr(char* dst, const char* src);
int  Harlin_Compare(const char* a, const char* b);
void Harlin_Copy(void* dst, const void* src, u32 n);
void Harlin_Fill(void* dst, u8 val, u32 n);
s32  Harlin_ToInt(const char* str);
void Harlin_FromInt(s32 val, char* buf);
```

## 10. 网络 API

```c
int Harlin_InitNet(void);
int Harlin_HttpGet(const char* host, const char* path);
int Harlin_Resolve(const char* domain, u8* out_ip);
```

`Harlin_InitNet` 初始化网卡并启动 DHCP 自动获取 IP 地址（超时约 2 秒）。DHCP 失败后回退到静态 IP（192.168.0.100）。`Harlin_HttpGet` 使用 HTTP/1.1 和 Chunked 传输编码。`Harlin_Resolve` 使用 DNS 协议进行域名解析。

## 11. 内存管理 API

```c
#define HARLIN_PAGE_SIZE 4096
#define HARLIN_VMM_PRESENT  0x001
#define HARLIN_VMM_WRITABLE 0x002
#define HARLIN_VMM_USER     0x004

void Harlin_InitPmm(void);
u64  Harlin_AllocPage(void);
void Harlin_FreePage(u64 addr);

void Harlin_InitVmm(u64 pml4_phys);
void Harlin_Map(u64 virt, u64 phys, u64 flags);
void Harlin_Unmap(u64 virt);
u64  Harlin_ToPhys(u64 virt);

void Harlin_InitKmalloc(void);
void* Harlin_Kmalloc(u64 size);
void  Harlin_Kfree(void* ptr);
void* Harlin_Krealloc(void* ptr, u64 size);
u64   Harlin_Ksize(void* ptr);
```

## 12. 磁盘 API

```c
int Harlin_InitDisk(void);
int Harlin_ReadSectors(u64 lba, u8 count, void* buf);
int Harlin_WriteSectors(u64 lba, u8 count, const void* buf);
```

## 13. 分区 API

```c
struct Harlin_Part {
    u8  active;
    u8  type;
    u32 start_lba;
    u32 sector_count;
};

int Harlin_InitPart(void);
int Harlin_PartCount(void);
int Harlin_GetPart(int index, struct Harlin_Part* out);
```

## 14. 文件系统 API

```c
struct Harlin_File {
    u32 start_cluster;
    u32 current_cluster;
    u32 position;
    u32 size;
    u32 dir_cluster;
    u32 dir_offset;
};

#define HARLIN_FS_OK     0
#define HARLIN_FS_ERROR -1
#define HARLIN_FS_EOF   -2

int  Harlin_Mount(u32 partition_lba);
int  Harlin_Open(const char* name, struct Harlin_File* out);
int  Harlin_Create(const char* name, struct Harlin_File* out);
int  Harlin_Read(struct Harlin_File* file, void* buf, u32 len);
int  Harlin_Write(struct Harlin_File* file, const void* buf, u32 len);
u32  Harlin_Size(struct Harlin_File* file);
void Harlin_Close(struct Harlin_File* file);
```

## 15. 管道 API

```c
struct Harlin_Pipe {
    int id;
};

int  Harlin_CreatePipe(struct Harlin_Pipe* pipe);
int  Harlin_ReadPipe(struct Harlin_Pipe* pipe, void* buf, u32 len);
int  Harlin_WritePipe(struct Harlin_Pipe* pipe, const void* buf, u32 len);
int  Harlin_ReadyPipe(struct Harlin_Pipe* pipe);
void Harlin_ClosePipe(struct Harlin_Pipe* pipe);
```

## 16. SMP API

```c
void Harlin_InitSmp(void);
int  Harlin_CpuCount(void);
int  Harlin_CurrentCpu(void);
void Harlin_SendIpi(int cpu, u8 vector);
```

## 17. 自旋锁 API

```c
struct Harlin_Spinlock {
    volatile u32 lock;
};

void Harlin_SpinlockInit(struct Harlin_Spinlock* lk);
void Harlin_SpinlockAcquire(struct Harlin_Spinlock* lk);
void Harlin_SpinlockRelease(struct Harlin_Spinlock* lk);
```

## 18. RTC API

```c
struct Harlin_RtcTime {
    u8 second;
    u8 minute;
    u8 hour;
    u8 day;
    u8 month;
    u16 year;
};

void Harlin_InitRtc(void);
void Harlin_RtcRead(struct Harlin_RtcTime* out);
u64  Harlin_RtcBootSeconds(void);

## 19. 音频 API

```c
int  Harlin_AudioInit(void);
int  Harlin_AudioPlayWav(const u8* data, u32 len);
void Harlin_AudioStop(void);
int  Harlin_AudioIsPlaying(void);
```

`Harlin_AudioInit` 初始化 Sound Blaster 16 声卡。
`Harlin_AudioPlayWav` 播放一段内存中的 WAV 数据，目前支持 16-bit PCM。
`Harlin_AudioStop` 立即停止当前播放。
`Harlin_AudioIsPlaying` 返回当前是否正在播放。

## 20. Shell API

```c
struct shell_command {
    const char* name;
    int (*handler)(int argc, char** argv);
};

void shell_run(void);
int  Harlin_ShellRegister(const struct shell_command* cmd);
```

`shell_run` 启动 HarLin Shell。Shell 在内核初始化完成后调用，提供交互式命令行。

`Harlin_ShellRegister` 向 Shell 注册一个外部命令。注册后，用户在 Shell 中输入该命令名即可调用对应的处理函数。命令名不能重复，不能与内置命令冲突。

## 21. 系统控制 API

```c
void Harlin_Boot(void);
void Harlin_Shutdown(void);
```

`Harlin_Shutdown` 优先通过 ACPI PM1 控制寄存器触发 S5 软关机，失败后回退到 CLI + HLT。`Harlin_Boot` 在 initialization 尾声自动执行 ACPI 初始化。

## 22. ACPI 电源管理 API

```c
int  acpi_init(void);
void acpi_power_off(void);
void acpi_reboot(void);
```

`acpi_init` 扫描 BIOS 内存区域查找 RSDP，通过 RSDT 定位 FADT 并从 DSDT 解析 \_S5 关机参数。`acpi_power_off` 向 PM1a_CNT 寄存器写入 SLP_TYPa | SLP_EN 触发 S5 状态。`acpi_reboot` 优先使用 FADT RESET_REG（I/O 端口），失败则回退到键盘控制器复位。

## 26. 块池 API
```c
void Harlin_BlockPoolInit(void);
void* Harlin_BlockPoolAlloc(u32 size);
int  Harlin_BlockPoolFree(void* ptr);
u32  Harlin_BlockPoolUsed(void);
```

`Harlin_BlockPoolAlloc` 按请求大小向上对齐到最近的块大小档位并分配，块大小共 6 档（最小 16 字节）。每块带 `guard` 校验位，越界时 `pr_err` 报警。`Harlin_BlockPoolFree` 释放块并清零 guard。

## 27. 读缓存 API

```c
void Harlin_ReadCacheInit(void);
int  Harlin_ReadCacheGet(u32 key, void* buf, u32 len);
int  Harlin_ReadCachePut(u32 key, const void* data, u32 len);
u32  Harlin_ReadCacheHit(void);
u32  Harlin_ReadCacheMiss(void);
```

`Harlin_ReadCacheGet` 按 key 查缓存；命中返回 1 并复制内容，未命中返回 0。`Harlin_ReadCachePut` 写入或更新条目。`Harlin_ReadCacheHit` / `Harlin_ReadCacheMiss` 给出累计命中/未命中统计。

## 28. 公平选 API

```c
void Harlin_FairPickInit(void);
int  Harlin_FairPick(int* keys, int count);
void Harlin_FairPickTest(void);
```

`Harlin_FairPick` 从一组 key 中按累计运行时间反比例选出一个，返回选中下标；keys 为 0/负值/重复时按确定顺序轮转。

## 29. 事件通知 API

```c
void Harlin_NotifyInit(void);
int  Harlin_NotifySend(int to_pid, int event);
int  Harlin_NotifyWait(int pid, int* out_event);
int  Harlin_NotifyPending(int pid);
```

`Harlin_NotifySend` 将事件投递到目标进程队列；满则丢弃并返回 -1。`Harlin_NotifyWait` 从当前进程队列取出一条事件；空返回 -1。`Harlin_NotifyPending` 查看队列中待处理事件数量。

## 30. 路径解析 API

```c
struct path_parts {
    u32 count;
    const char* seg[PATH_MAX_SEGMENTS];
    u32  seg_len[PATH_MAX_SEGMENTS];
    int  is_absolute;
    char buf[PATH_MAX_CHARS];
};

int Harlin_PathParse(const char* in, struct path_parts* out);
int Harlin_PathNormalize(const char* in, char* out, u32 out_size);
```

`Harlin_PathParse` 按 `/` 拆分路径为 `seg` 段并写入 `buf`；`is_absolute` 反映是否以 `/` 开头。`Harlin_PathNormalize` 在 `Harlin_PathParse` 基础上规范化 `.` 和 `..` 后输出到 `out`。

## 31. 内存虚拟文件系统 API

```c
int  Harlin_MemFsInit(void);
int  Harlin_MemFsMkdir(const char* path);
int  Harlin_MemFsRmdir(const char* path);
int  Harlin_MemFsCreate(const char* path);
int  Harlin_MemFsRemove(const char* path);
int  Harlin_MemFsWrite(const char* path, const void* data, u32 len);
int  Harlin_MemFsRead(const char* path, void* buf, u32 len);
int  Harlin_MemFsLs(const char* path, struct mem_fs_dir_entry* out, u32 max);
u32  Harlin_MemFsLookupNode(const char* path);
```

基于节点池的目录树结构，节点数 `MEM_FS_MAX_NODES`（默认 64）。每个文件内嵌 `payload[MEM_FS_PAYLOAD]` 字节。`Harlin_MemFsLs` 将目录项以 `struct mem_fs_dir_entry { char name[32]; u8 type; }` 形式填充到 `out`，返回条目数。

## 32. 文件权限位 API

```c
#define PERM_OWN_R (1u << 8)
#define PERM_OWN_W (1u << 7)
#define PERM_OWN_X (1u << 6)
#define PERM_GRP_R (1u << 5)
#define PERM_GRP_W (1u << 4)
#define PERM_GRP_X (1u << 3)
#define PERM_OTH_R (1u << 2)
#define PERM_OTH_W (1u << 1)
#define PERM_OTH_X (1u << 0)

int Harlin_PermCheck(u16 mode, u16 file_owner, u16 file_group,
                     u16 uid, u16 gid, int want);
```

`Harlin_PermCheck` 比较 `uid/gid` 与文件 `owner/group`，按 `want`（任一 rwx 位的或组合）检查权限，返回 1 表示通过、0 表示拒绝。

## 33. COW 文件系统 API

```c
int  Harlin_CowFsInit(void);
int  Harlin_CowSnapshot(const char* path, char* out_ver);
int  Harlin_CowWrite(const char* path, const char* expected_ver,
                     const void* data, u32 size);
int  Harlin_CowReadAt(const char* path, const char* ver,
                      u32 offset, void* buf, u32 len);
int  Harlin_CowFork(const char* src_path, const char* dst_path);
```

`Harlin_CowSnapshot` 给指定文件拍一个版本快照，版本号格式 `v000`~`v999`，写入 `out_ver`（至少 5 字节）。`Harlin_CowWrite` 在文件当前最新版本为 `expected_ver` 时写入新数据，否则返回 `HARLIN_BUSY`（冲突）。`Harlin_CowReadAt` 按版本号 + 偏移读取。`Harlin_CowFork` 克隆另一文件节点的所有版本到新路径。

## 34. 进程控制 API

```c
int  Harlin_Fork(int parent_pid, int* out_child_pid);
int  Harlin_ExecFromPath(const char* path);
int  Harlin_ExecFromBuffer(const void* elf_data, u32 elf_size);
int  Harlin_NotifyExit(int child_pid, int code);
int  Harlin_Wait(int parent_pid, int child_pid, int* out_code);
int  Harlin_WaitAny(int parent_pid, int* out_child_pid, int* out_code);
```

`Harlin_Fork` 复制父进程的 rip / rsp / 页表 / stack / handles / 优先级 / fair_key，返回 0 表示成功、子 pid 通过 `out_child_pid` 返回。`Harlin_ExecFromPath` 从 mem_fs 读取 elf 并加载为新进程；`Harlin_ExecFromBuffer` 直接从内存缓冲区加载。`Harlin_NotifyExit` 登记子进程退出码；`Harlin_Wait` 等待指定子进程退出；`Harlin_WaitAny` 等待任意一个子进程退出。退出码回收后该子进程槽位可被复用。

## 35. /proc 虚拟文件系统 API

```c
int Harlin_ProcFsRead(const char* path, char* out, u32 out_size);
int Harlin_ProcFsLs(const char* path, char* out, u32 out_size);
```

`Harlin_ProcFsRead` 支持的路径：`/proc/uptime`（内核启动滴答计数）、`/proc/meminfo`（free/used 物理页数）、`/proc/cpuinfo`（CPU 数）、`/proc/loadavg`（负载占位 0.0）。`Harlin_ProcFsLs("/proc", ...)` 返回 `uptime meminfo cpuinfo loadavg` 空格分隔的列表。

## 36. 系统调用接口

用户态程序通过软件中断 0x80 调用内核服务。系统调用号放在 RAX 中，参数遵循 System V AMD64 ABI。

| 编号 | 名称 | 说明 |
|------|------|------|
| 0 | sys_exit | 终止当前进程 |
| 1 | sys_print | 打印以空字符结尾的字符串 |
| 2 | sys_getc | 从键盘读取一个字符 |
| 3 | sys_alloc | 分配物理页 |
| 4 | sys_free | 释放物理页 |
| 5 | sys_open | 打开文件 |
| 6 | sys_read | 从文件读取 |
| 7 | sys_close | 关闭文件 |
| 8 | sys_exec | 已废除（保留入口返回 -1） |
| 9 | sys_yield | 让出 CPU |
| 10 | sys_sleep | 休眠指定毫秒 |
| 11 | sys_key_overflow | 获取键盘缓冲区溢出计数 |
| 12 | sys_pipe_create | 创建管道 |
| 13 | sys_pipe_read | 从管道读取 |
| 14 | sys_pipe_write | 向管道写入 |
| 15 | sys_pipe_close | 关闭管道 |
| 16 | sys_pipe_ready | 检查管道是否有数据可读 |
| 17 | sys_getpid | 获取当前进程 ID |
| 18 | sys_getcpu | 获取当前 CPU 编号 |
| 19 | sys_time | 读取 RTC 时间 |
| 20 | sys_beep | 蜂鸣器发声 |
| 21 | sys_kmalloc | 内核堆内存分配 |
| 22 | sys_kfree | 释放内核堆内存 |
| 23 | sys_mmap | 映射一段用户内存 |
| 24 | sys_unmap | 解除用户内存映射 |
| 25 | sys_getkeystate | 获取键盘修饰键状态 |
| 26 | sys_keyled | 设置键盘 LED |
| 27 | sys_setpriority | 设置当前进程优先级 |
| 40 | sys_dlopen | 加载动态链接库 |
| 41 | sys_dlsym | 查找动态库符号 |
| 42 | sys_dlclose | 卸载动态链接库 |

完整的系统调用表在内核中定义，并通过 `harlin_API.h` 向用户空间导出。

## 37. 版本管理

API 版本跟随内核版本。主版本号仅在发生不兼容的 API 变更时增加，次版本号在向后兼容地增加功能时增加。

## 38. 安全模型

- 内核代码和数据运行在 ring 0。
- 用户进程运行在 ring 3。
- 用户态禁止直接进行端口 I/O。
- 物理页分配和页表操作仅内核可执行。
- 用户态内存通过独立页表进行隔离。
