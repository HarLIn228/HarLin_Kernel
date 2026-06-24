#ifndef EFI_TYPES_H
#define EFI_TYPES_H

typedef unsigned long long UINT64;
typedef unsigned long      SIZE_T;
typedef unsigned long      UINTN;
typedef signed long long   INT64;
typedef unsigned int       UINT32;
typedef signed int         INT32;
typedef unsigned short     UINT16;
typedef signed short       INT16;
typedef unsigned char      UINT8;
typedef signed char        INT8;
typedef UINT64             EFI_STATUS;
typedef void*              EFI_HANDLE;
typedef UINT64             EFI_PHYSICAL_ADDRESS;
typedef UINT64             EFI_VIRTUAL_ADDRESS;

typedef struct {
    UINT32 Data1;
    UINT16 Data2;
    UINT16 Data3;
    UINT8  Data4[8];
} EFI_GUID;

typedef struct {
    UINT64 Signature;
    UINT32 Revision;
    UINT32 HeaderSize;
    UINT32 CRC32;
    UINT32 Reserved;
} EFI_TABLE_HEADER;

typedef EFI_STATUS (*EFI_LOCATE_PROTOCOL)(
    EFI_GUID* Protocol,
    void* Registration,
    void** Interface);

typedef EFI_STATUS (*EFI_GET_MEMORY_MAP)(
    UINT64* MemoryMapSize,
    void* MemoryMap,
    UINT64* MapKey,
    UINT32* DescriptorSize,
    UINT32* DescriptorVersion);

typedef EFI_STATUS (*EFI_EXIT_BOOT_SERVICES)(
    EFI_HANDLE ImageHandle,
    UINT64 MapKey);

typedef struct {
    UINT32 MaxMode;
    UINT32 Mode;
    UINT32 Attribute;
    UINT32 FrameBufferFormat;
    UINT64 FrameBufferBase;
    UINT64 FrameBufferSize;
} EFI_GRAPHICS_OUTPUT_MODE_INFORMATION;

typedef struct {
    UINT32 ResolutionX;
    UINT32 ResolutionY;
    UINT32 PixelFormat;
    UINT32 PixelInformation;
    UINT32 PixelsPerScanLine;
    UINT64 FrameBufferBase;
    UINT64 FrameBufferSize;
} EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE;

typedef struct {
    void* QueryMode;
    void* SetMode;
    void* Blt;
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE* Mode;
} EFI_GRAPHICS_OUTPUT_PROTOCOL;

typedef struct _EFI_FILE_HANDLE* EFI_FILE_HANDLE;

typedef struct {
    UINT64 Revision;
    void* Open;
    void* Close;
    void* Delete;
    void* Read;
    void* GetPosition;
    void* SetPosition;
    void* GetInfo;
    void* SetInfo;
    void* Flush;
} EFI_FILE_PROTOCOL;

typedef EFI_FILE_PROTOCOL EFI_FILE;

typedef struct {
    UINT64 Revision;
    void* OpenVolume;
} EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;

typedef EFI_STATUS (*EFI_LOCATE_PROTOCOL)(
    EFI_GUID* Protocol,
    void* Registration,
    void** Interface);

typedef EFI_STATUS (*EFI_GET_MEMORY_MAP)(
    UINT64* MemoryMapSize,
    void* MemoryMap,
    UINT64* MapKey,
    UINT32* DescriptorSize,
    UINT32* DescriptorVersion);

typedef EFI_STATUS (*EFI_EXIT_BOOT_SERVICES)(
    EFI_HANDLE ImageHandle,
    UINT64 MapKey);

typedef EFI_STATUS (*EFI_FILE_OPEN)(
    EFI_FILE_PROTOCOL* File,
    EFI_FILE_PROTOCOL** NewHandle,
    UINT16* FileName,
    UINT64 OpenMode,
    UINT64 Attributes);

typedef EFI_STATUS (*EFI_FILE_READ)(
    EFI_FILE_PROTOCOL* File,
    UINT64* BufferSize,
    void* Buffer);

typedef EFI_STATUS (*EFI_FILE_CLOSE)(
    EFI_FILE_PROTOCOL* File);

typedef EFI_STATUS (*EFI_SIMPLE_FILE_OPEN_VOLUME)(
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* This,
    EFI_FILE_PROTOCOL** Root);
typedef struct {
    UINT64 Magic;
    UINT64 FrameBufferBase;
    UINT64 FrameBufferSize;
    UINT32 HoriResolution;
    UINT32 VertResolution;
    UINT32 PixelsPerScanLine;
    UINT32 PixelFormat;
    UINT64 RsdpAddress;
    UINT64 KernelEntryPhys;
} EFI_BOOT_INFO;

#define EFI_BOOT_INFO_MAGIC 0x484C4E45ULL

#endif
