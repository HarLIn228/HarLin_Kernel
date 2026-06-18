#ifndef HARLIN_API_H
#define HARLIN_API_H

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;
typedef signed char    s8;
typedef signed short   s16;
typedef signed int     s32;

void Harlin_Boot(void);
void Harlin_Shutdown(void);

void Harlin_ConClear(void);
void Harlin_ConPutChar(char c);
void Harlin_ConPrint(const char* str);
void Harlin_ConPrintHex(u32 val);
void Harlin_ConPrintDec(s32 val);
void Harlin_ConSetColor(u8 fg, u8 bg);

#define HARLIN_DISP_VGA_TEXT 0
#define HARLIN_DISP_VGA_13H  1
#define HARLIN_DISP_VESA     2

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

int  Harlin_NetInit(void);
int  Harlin_HttpGet(const char* host, const char* path);
int  Harlin_DNS(const char* domain, u8* out_ip);

#endif
