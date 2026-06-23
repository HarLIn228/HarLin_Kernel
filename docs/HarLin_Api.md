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

## 23. 系统调用接口

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

## 24. 版本管理

API 版本跟随内核版本。主版本号仅在发生不兼容的 API 变更时增加，次版本号在向后兼容地增加功能时增加。

## 25. 安全模型

- 内核代码和数据运行在 ring 0。
- 用户进程运行在 ring 3。
- 用户态禁止直接进行端口 I/O。
- 物理页分配和页表操作仅内核可执行。
- 用户态内存通过独立页表进行隔离。
