# HarLin API 标准

## 1. 范围

本文档定义 HarLin 内核的应用程序编程接口，面向在 HarLin 之上开发软件的第三方开发者。

## 2. 约定

### 2.1 命名

- 公开的内核函数使用前缀 `Harlin_`。
- 公开的宏使用前缀 `HARLIN_`。
- 公开的类型使用小写短名：`u8`、`u16`、`u32`、`u64`、`s8`、`s16`、`s32`、`s64`。
- 常量使用大写和下划线。

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
void Harlin_ConClear(void);
void Harlin_ConPutChar(char c);
void Harlin_ConPrint(const char* str);
void Harlin_ConPrintHex(u64 val);
void Harlin_ConPrintDec(s32 val);
void Harlin_ConSetColor(u8 fg, u8 bg);
```

## 5. 显示 API

```c
#define HARLIN_DISP_VGA_TEXT 0
#define HARLIN_DISP_VGA_13H  1
#define HARLIN_DISP_VESA     2

int  Harlin_DisplaySetMode(int mode);
void Harlin_DisplayClear(unsigned char color);
void Harlin_DisplayPutPixel(int x, int y, unsigned char color);
void Harlin_DisplayPutString(int x, int y, const char* str, unsigned char color);
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
int  Harlin_KeyReady(void);
char Harlin_KeyGet(void);
```

## 9. 字符串与内存 API

```c
u32  Harlin_StrLen(const char* str);
void Harlin_StrCopy(char* dst, const char* src);
int  Harlin_StrCmp(const char* a, const char* b);
void Harlin_MemCopy(void* dst, const void* src, u32 n);
void Harlin_MemSet(void* dst, u8 val, u32 n);
s32  Harlin_StrToInt(const char* str);
void Harlin_IntToStr(s32 val, char* buf);
```

## 10. 网络 API

```c
int Harlin_NetInit(void);
int Harlin_HttpGet(const char* host, const char* path);
int Harlin_DNS(const char* domain, u8* out_ip);
```

## 11. 内存管理 API

```c
#define HARLIN_PAGE_SIZE 4096
#define HARLIN_VMM_PRESENT  0x001
#define HARLIN_VMM_WRITABLE 0x002
#define HARLIN_VMM_USER     0x004

void Harlin_PmmInit(void);
u64  Harlin_PmmAlloc(void);
void Harlin_PmmFree(u64 addr);

void Harlin_VmmInit(u64 pml4_phys);
void Harlin_VmmMap(u64 virt, u64 phys, u64 flags);
void Harlin_VmmUnmap(u64 virt);
u64  Harlin_VmmGetPhys(u64 virt);
```

## 12. 磁盘 API

```c
int Harlin_DiskInit(void);
int Harlin_DiskReadSector(u64 lba, u8 count, void* buf);
int Harlin_DiskWriteSector(u64 lba, u8 count, const void* buf);
```

## 13. 分区 API

```c
struct Harlin_PartitionInfo {
    u8  active;
    u8  type;
    u32 start_lba;
    u32 sector_count;
};

int Harlin_PartitionInit(void);
int Harlin_PartitionCount(void);
int Harlin_PartitionGet(int index, struct Harlin_PartitionInfo* out);
```

## 14. 文件系统 API

```c
struct Harlin_File {
    u32 start_cluster;
    u32 current_cluster;
    u32 position;
    u32 size;
};

#define HARLIN_FS_OK     0
#define HARLIN_FS_ERROR -1
#define HARLIN_FS_EOF   -2

int  Harlin_FsMount(u32 partition_lba);
int  Harlin_FsOpen(const char* name, struct Harlin_File* out);
int  Harlin_FsRead(struct Harlin_File* file, void* buf, u32 len);
u32  Harlin_FsSize(struct Harlin_File* file);
void Harlin_FsClose(struct Harlin_File* file);
```

## 15. 系统控制 API

```c
void Harlin_Boot(void);
void Harlin_Shutdown(void);
```

## 16. 系统调用接口

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
| 8 | sys_exec | 执行 .cx 程序 |
| 9 | sys_yield | 让出 CPU |
| 10 | sys_sleep | 休眠指定毫秒 |

完整的系统调用表在内核中定义，并通过 `harlin_API.h` 向用户空间导出。

## 17. 版本管理

API 版本跟随内核版本。主版本号仅在发生不兼容的 API 变更时增加，次版本号在向后兼容地增加功能时增加。

## 18. 安全模型

- 内核代码和数据运行在 ring 0。
- 用户进程运行在 ring 3。
- 用户态禁止直接进行端口 I/O。
- 物理页分配和页表操作仅内核可执行。
- 用户态内存通过独立页表进行隔离。
