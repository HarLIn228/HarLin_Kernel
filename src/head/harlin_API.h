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
#define HARLIN_SYS_GETPID       17
#define HARLIN_SYS_GETCPU       18
#define HARLIN_SYS_TIME         19
#define HARLIN_SYS_BEEP         20
#define HARLIN_SYS_KMALLOC      21
#define HARLIN_SYS_KFREE        22
#define HARLIN_SYS_MMAP         23
#define HARLIN_SYS_UNMAP        24
#define HARLIN_SYS_GETKEYSTATE  25
#define HARLIN_SYS_KEYLED       26
#define HARLIN_SYS_SETPRIORITY  27
#define HARLIN_SYS_CD           28
#define HARLIN_SYS_MKDIR        29
#define HARLIN_SYS_RMDIR        30
#define HARLIN_SYS_GETCWD       31
#define HARLIN_SYS_MSGCREATE    32
#define HARLIN_SYS_MSGSEND      33
#define HARLIN_SYS_MSGRECV      34
#define HARLIN_SYS_MSGDESTROY   35
#define HARLIN_SYS_SEMCREATE    36
#define HARLIN_SYS_SEMWAIT      37
#define HARLIN_SYS_SEMPOST      38
#define HARLIN_SYS_SEMDESTROY   39
#define HARLIN_SYS_DLOPEN       40
#define HARLIN_SYS_DLSYM        41
#define HARLIN_SYS_DLCLOSE      42

#define HARLIN_KEY_SHIFT  0x01
#define HARLIN_KEY_CTRL   0x02
#define HARLIN_KEY_ALT    0x04
#define HARLIN_KEY_CAPS   0x08
#define HARLIN_KEY_NUM    0x10
#define HARLIN_KEY_SCROLL 0x20

#define HARLIN_LED_SCROLL 0x01
#define HARLIN_LED_NUM    0x02
#define HARLIN_LED_CAPS   0x04

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



int  Harlin_KeyAvail(void);
char Harlin_GetKey(void);
int  Harlin_KeyOverflow(void);
void Harlin_KeyFlush(void);
u8   Harlin_KeyState(void);
void Harlin_KeyLed(u8 leds);

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

void Harlin_InitKmalloc(void);
void* Harlin_Kmalloc(u64 size);
void  Harlin_Kfree(void* ptr);
void* Harlin_Krealloc(void* ptr, u64 size);
u64   Harlin_Ksize(void* ptr);

void Harlin_InitSmp(void);
int  Harlin_CpuCount(void);
int  Harlin_CurrentCpu(void);
void Harlin_SendIpi(int cpu, u8 vector);

struct Harlin_Spinlock {
    volatile u32 lock;
};

void Harlin_SpinlockInit(struct Harlin_Spinlock* lk);
void Harlin_SpinlockAcquire(struct Harlin_Spinlock* lk);
void Harlin_SpinlockRelease(struct Harlin_Spinlock* lk);

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

int  Harlin_AudioInit(void);
int  Harlin_AudioPlayWav(const u8* data, u32 len);
void Harlin_AudioStop(void);
int  Harlin_AudioIsPlaying(void);


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
int  Harlin_DeleteFile(const char* name);

int  Harlin_Cd(const char* path);
int  Harlin_Mkdir(const char* path);
int  Harlin_Rmdir(const char* path);
int  Harlin_GetCwd(char* buf, u32 size);

int  Harlin_MsgCreate(void);
int  Harlin_MsgSend(int qid, u32 type, const void* data, u32 len);
int  Harlin_MsgRecv(int qid, u32* type, void* buf, u32 len, u32 expected_type);
int  Harlin_MsgDestroy(int qid);

int  Harlin_SemCreate(int initial);
int  Harlin_SemWait(int sid);
int  Harlin_SemPost(int sid);
int  Harlin_SemDestroy(int sid);

int  Harlin_DlOpen(const char* path);
void* Harlin_DlSym(int lib_id, const char* name);
int  Harlin_DlClose(int lib_id);

int  Harlin_CreatePipe(struct Harlin_Pipe* pipe);
int  Harlin_ReadPipe(struct Harlin_Pipe* pipe, void* buf, u32 len);
int  Harlin_WritePipe(struct Harlin_Pipe* pipe, const void* buf, u32 len);
int  Harlin_ReadyPipe(struct Harlin_Pipe* pipe);
void Harlin_ClosePipe(struct Harlin_Pipe* pipe);

int  Harlin_GetPid(void);
void Harlin_SetPriority(u32 priority);
void Harlin_Beep(u32 freq, u32 ms);

#endif
