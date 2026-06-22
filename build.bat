@echo off
setlocal enabledelayedexpansion

cd /d "%~dp0"

set MSYS_PATH=D:\Code\Local_tool_library\msys
set NASM_PATH=D:\Code\Local_tool_library\nasm
set LLVM_PATH=D:\Code\Local_tool_library\LLVM\bin
set PATH=%NASM_PATH%;%MSYS_PATH%\mingw64\bin;%MSYS_PATH%\usr\bin;%LLVM_PATH%;%PATH%

for /F %%a in ('echo prompt $E ^| cmd') do set "ESC=%%a"

set GREEN=%ESC%[32m
set RED=%ESC%[31m
set RESET=%ESC%[0m

set SUCCESS=%GREEN%[Success]%RESET%
set FAILURE=%RED%[Failure]%RESET%
set ERR=%RED%[Error]%RESET%

set CC=D:\Code\Local_tool_library\LLVM\bin\clang.exe
set LD=D:\Code\Local_tool_library\LLVM\bin\ld.lld.exe
set CFLAGS=-ffreestanding -c -target x86_64-unknown-none-elf -O2 -Wall -Wextra -fno-exceptions -fno-stack-protector -fno-stack-check -fno-asynchronous-unwind-tables -fno-unwind-tables -nostdlib -nodefaultlibs -I src\head -I build -mno-sse -mno-mmx -mabi=sysv

set TOTAL=0
for %%f in (src\asm\*.asm) do set /a TOTAL+=1
for %%f in (src\harlin\*.c) do set /a TOTAL+=1
set COUNT=0

if exist build rmdir /s /q build
if not exist build mkdir build

echo Building kernel...

nasm -f bin src\asm\boot.asm -o build\boot.bin 2>build\error.log
if errorlevel 1 (
    echo %FAILURE% Compiling boot.asm
    echo %ERR%
    type build\error.log
    goto error
)
set /a COUNT+=1
echo %SUCCESS% [!COUNT!/%TOTAL%] Compiling boot.asm

set OBJS=
for %%f in (src\asm\*.asm) do (
    set FILENAME=%%~nf
    if /I "!FILENAME!" neq "boot" (
        if /I "!FILENAME!" neq "gdt" (
            nasm -f elf64 %%f -o build\asm_!FILENAME!.o 2>build\error.log
            if errorlevel 1 (
                echo %FAILURE% Compiling %%f
                echo %ERR%
                type build\error.log
                goto error
            )
            set /a COUNT+=1
            echo %SUCCESS% [!COUNT!/%TOTAL%] Compiling %%f
            set OBJS=!OBJS! build\asm_!FILENAME!.o
        )
    )
)

for %%f in (src\harlin\*.c) do (
    set FILENAME=%%~nf
    %CC% %CFLAGS% -o build\!FILENAME!.o %%f 2>build\error.log
    if errorlevel 1 (
        echo %FAILURE% Compiling %%f
        echo %ERR%
        type build\error.log
        goto error
    )
    set /a COUNT+=1
    echo %SUCCESS% [!COUNT!/%TOTAL%] Compiling %%f
    set OBJS=!OBJS! build\!FILENAME!.o
)

%LD% -T src\harlin\linker.ld -o build\kernel.tmp %OBJS% -m elf_x86_64 -nostdlib 2>build\error.log
if errorlevel 1 (
    echo %FAILURE% Linking kernel
    echo %ERR%
    type build\error.log
    goto error
)
echo %SUCCESS% Linking kernel

objcopy -O binary build\kernel.tmp build\kernel.bin 2>build\error.log
if errorlevel 1 (
    echo %FAILURE% Creating kernel.bin
    echo %ERR%
    type build\error.log
    goto error
)
echo %SUCCESS% Creating kernel.bin
copy /Y build\kernel.tmp build\kernel.elf >nul 2>&1
if errorlevel 1 (
    echo %FAILURE% Creating kernel.elf
    echo %ERR%
    type build\error.log
    goto error
)
echo %SUCCESS% Creating kernel.elf

for /f %%a in ('"%MSYS_PATH%\usr\bin\truncate.exe" -s %%512 build\kernel.bin 2^>^&1') do set TRUNC_OUT=%%a
if errorlevel 1 (
    echo %FAILURE% Padding kernel.bin
    echo %ERR%
    goto error
)

fsutil file createnew build\HarLin.img 1474560 >nul 2>&1
if errorlevel 1 (
    echo %FAILURE% Creating 1.44MB disk image
    echo %ERR%
    goto error
)

for /f %%a in ('powershell -NoProfile -Command "((Get-Item 'build\boot.bin').Length + (Get-Item 'build\kernel.bin').Length)"') do set IMG_OFFSET=%%a
set /a PAD_SIZE=1474560-!IMG_OFFSET!
fsutil file createnew build\pad.bin !PAD_SIZE! >nul 2>&1

copy /b build\boot.bin + build\kernel.bin + build\pad.bin build\HarLin.img >nul
if errorlevel 1 (
    echo %FAILURE% Creating disk image
    echo %ERR%
    type build\error.log
    goto error
)
del build\pad.bin 2>nul
echo %SUCCESS% Creating disk image

rem del build\kernel.tmp 2>nul
del build\asm_*.o 2>nul
del build\*.o 2>nul
if exist build\error.log del build\error.log
if exist build\shell_error.log del build\shell_error.log

echo.
echo %GREEN%Build successful%RESET%
echo.
exit /b 0

:error
echo.
echo %RED%Build failed%RESET%
exit /b 1

:end
endlocal
