#ifndef HARLIN_API_H
#define HARLIN_API_H

typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;
typedef signed char        s8;
typedef signed short       s16;
typedef signed int         s32;
typedef signed long long   s64;

#define HARLIN_OK              0
#define HARLIN_ERROR          -1
#define HARLIN_INVALID        -2
#define HARLIN_NO_MEMORY      -3
#define HARLIN_NOT_FOUND      -4
#define HARLIN_TIMEOUT        -5
#define HARLIN_IO_ERROR       -6
#define HARLIN_UNSUPPORTED    -7
#define HARLIN_ACCESS_DENIED  -8
#define HARLIN_BUSY           -9

#define HARLIN_PAGE_SIZE 4096
#define HARLIN_VMM_PRESENT  0x001
#define HARLIN_VMM_WRITABLE 0x002
#define HARLIN_VMM_USER     0x004

#define HARLIN_DISP_VGA_TEXT 0
#define HARLIN_DISP_VGA_13H  1
#define HARLIN_DISP_VESA     2

#define HARLIN_FS_OK     0
#define HARLIN_FS_ERROR -1
#define HARLIN_FS_EOF   -2

#define HARLIN_SYS_EXIT   0
#define HARLIN_SYS_PRINT  1
#define HARLIN_SYS_GETC   2
#define HARLIN_SYS_ALLOC  3
#define HARLIN_SYS_FREE   4
#define HARLIN_SYS_OPEN   5
#define HARLIN_SYS_READ   6
#define HARLIN_SYS_CLOSE  7
#define HARLIN_SYS_EXEC   8
#define HARLIN_SYS_YIELD  9
#define HARLIN_SYS_SLEEP  10

void Harlin_Boot(void);
void Harlin_Shutdown(void);

void Harlin_ConClear(void);
void Harlin_ConPutChar(char c);
void Harlin_ConPrint(const char* str);
void Harlin_ConPrintHex(u64 val);
void Harlin_ConPrintDec(s32 val);
void Harlin_ConSetColor(u8 fg, u8 bg);

int  Harlin_DisplaySetMode(int mode);
void Harlin_DisplayClear(unsigned char color);
void Harlin_DisplayPutPixel(int x, int y, unsigned char color);
void Harlin_DisplayPutString(int x, int y, const char* str, unsigned char color);

u8   Harlin_PortIn8(u16 port);
u16  Harlin_PortIn16(u16 port);
u32  Harlin_PortIn32(u16 port);
void Harlin_PortOut8(u16 port, u8 data);
void Harlin_PortOut16(u16 port, u16 data);
void Harlin_PortOut32(u16 port, u32 data);

void Harlin_IntOn(void);
void Harlin_IntOff(void);

int  Harlin_KeyReady(void);
char Harlin_KeyGet(void);

u32  Harlin_StrLen(const char* str);
void Harlin_StrCopy(char* dst, const char* src);
int  Harlin_StrCmp(const char* a, const char* b);
void Harlin_MemCopy(void* dst, const void* src, u32 n);
void Harlin_MemSet(void* dst, u8 val, u32 n);
s32  Harlin_StrToInt(const char* str);
void Harlin_IntToStr(s32 val, char* buf);

int Harlin_NetInit(void);
int Harlin_HttpGet(const char* host, const char* path);
int Harlin_DNS(const char* domain, u8* out_ip);

void Harlin_PmmInit(void);
u64  Harlin_PmmAlloc(void);
void Harlin_PmmFree(u64 addr);

void Harlin_VmmInit(u64 pml4_phys);
void Harlin_VmmMap(u64 virt, u64 phys, u64 flags);
void Harlin_VmmUnmap(u64 virt);
u64  Harlin_VmmGetPhys(u64 virt);

int Harlin_DiskInit(void);
int Harlin_DiskReadSector(u64 lba, u8 count, void* buf);
int Harlin_DiskWriteSector(u64 lba, u8 count, const void* buf);

struct Harlin_PartitionInfo {
    u8  active;
    u8  type;
    u32 start_lba;
    u32 sector_count;
};

int Harlin_PartitionInit(void);
int Harlin_PartitionCount(void);
int Harlin_PartitionGet(int index, struct Harlin_PartitionInfo* out);

struct Harlin_File {
    u32 start_cluster;
    u32 current_cluster;
    u32 position;
    u32 size;
};

int  Harlin_FsMount(u32 partition_lba);
int  Harlin_FsOpen(const char* name, struct Harlin_File* out);
int  Harlin_FsRead(struct Harlin_File* file, void* buf, u32 len);
u32  Harlin_FsSize(struct Harlin_File* file);
void Harlin_FsClose(struct Harlin_File* file);

#endif
