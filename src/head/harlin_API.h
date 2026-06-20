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
#define HARLIN_SYS_YIELD        9
#define HARLIN_SYS_SLEEP        10
#define HARLIN_SYS_KEYOVERFLOW  11
#define HARLIN_SYS_PIPE_CREATE  12
#define HARLIN_SYS_PIPE_READ    13
#define HARLIN_SYS_PIPE_WRITE   14
#define HARLIN_SYS_PIPE_CLOSE   15
#define HARLIN_SYS_PIPE_READY   16

void Harlin_Boot(void);
void Harlin_Shutdown(void);

void Harlin_Clear(void);
void Harlin_PutChar(char c);
void Harlin_Print(const char* str);
void Harlin_PrintHex(u64 val);
void Harlin_PrintDec(s32 val);
void Harlin_SetColor(u8 fg, u8 bg);

int  Harlin_SetMode(int mode);
void Harlin_ClearScreen(unsigned char color);
void Harlin_PutPixel(int x, int y, unsigned char color);
void Harlin_PutString(int x, int y, const char* str, unsigned char color);

u8   Harlin_PortIn8(u16 port);
u16  Harlin_PortIn16(u16 port);
u32  Harlin_PortIn32(u16 port);
void Harlin_PortOut8(u16 port, u8 data);
void Harlin_PortOut16(u16 port, u16 data);
void Harlin_PortOut32(u16 port, u32 data);

void Harlin_IntOn(void);
void Harlin_IntOff(void);

int  Harlin_KeyAvail(void);
char Harlin_GetKey(void);
int  Harlin_KeyOverflow(void);

u32  Harlin_Len(const char* str);
void Harlin_CopyStr(char* dst, const char* src);
int  Harlin_Compare(const char* a, const char* b);
void Harlin_Copy(void* dst, const void* src, u32 n);
void Harlin_Fill(void* dst, u8 val, u32 n);
s32  Harlin_ToInt(const char* str);
void Harlin_FromInt(s32 val, char* buf);

int Harlin_InitNet(void);
int Harlin_HttpGet(const char* host, const char* path);
int Harlin_Resolve(const char* domain, u8* out_ip);

void Harlin_InitPmm(void);
u64  Harlin_AllocPage(void);
void Harlin_FreePage(u64 addr);

void Harlin_InitVmm(u64 pml4_phys);
void Harlin_Map(u64 virt, u64 phys, u64 flags);
void Harlin_Unmap(u64 virt);
u64  Harlin_ToPhys(u64 virt);

int Harlin_InitDisk(void);
int Harlin_ReadSectors(u64 lba, u8 count, void* buf);
int Harlin_WriteSectors(u64 lba, u8 count, const void* buf);

struct Harlin_Part {
    u8  active;
    u8  type;
    u32 start_lba;
    u32 sector_count;
};

int Harlin_InitPart(void);
int Harlin_PartCount(void);
int Harlin_GetPart(int index, struct Harlin_Part* out);

struct Harlin_File {
    u32 start_cluster;
    u32 current_cluster;
    u32 position;
    u32 size;
    u32 dir_cluster;
    u32 dir_offset;
};

struct Harlin_Pipe {
    int id;
};

int  Harlin_Mount(u32 partition_lba);
int  Harlin_Open(const char* name, struct Harlin_File* out);
int  Harlin_Create(const char* name, struct Harlin_File* out);
int  Harlin_Read(struct Harlin_File* file, void* buf, u32 len);
int  Harlin_Write(struct Harlin_File* file, const void* buf, u32 len);
u32  Harlin_Size(struct Harlin_File* file);
void Harlin_Close(struct Harlin_File* file);

int  Harlin_CreatePipe(struct Harlin_Pipe* pipe);
int  Harlin_ReadPipe(struct Harlin_Pipe* pipe, void* buf, u32 len);
int  Harlin_WritePipe(struct Harlin_Pipe* pipe, const void* buf, u32 len);
int  Harlin_ReadyPipe(struct Harlin_Pipe* pipe);
void Harlin_ClosePipe(struct Harlin_Pipe* pipe);

#endif
