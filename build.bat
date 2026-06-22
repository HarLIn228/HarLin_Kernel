@echo off
setlocal enabledelayedexpansion

cd /d "%~dp0"

set MSYS_PATH=D:\Code\Local_tool_library\msys
set NASM_PATH=D:\Code\Local_tool_library\nasm
set PATH=%NASM_PATH%;%MSYS_PATH%\mingw64\bin;%MSYS_PATH%\usr\bin;%PATH%

for /F %%a in ('echo prompt $E ^| cmd') do set "ESC=%%a"

set GREEN=%ESC%[32m
set RED=%ESC%[31m
set RESET=%ESC%[0m

set SUCCESS=%GREEN%[Success]%RESET%
set FAILURE=%RED%[Failure]%RESET%
set ERR=%RED%[Error]%RESET%

set CC=x86_64-w64-mingw32-gcc
set LD=ld
set CFLAGS=-ffreestanding -c -m64 -O2 -Wall -Wextra -fno-exceptions -fno-stack-protector -fno-stack-check -fno-asynchronous-unwind-tables -fno-unwind-tables -nostdlib -nodefaultlibs -I src\head -I build -mno-sse -mno-mmx -mabi=sysv

set TOTAL=0
for %%f in (src\asm\*.asm) do set /a TOTAL+=1
for %%f in (src\harlin\*.c) do set /a TOTAL+=1
set COUNT=0

if exist build rmdir /s /q build
if not exist build mkdir build
if not exist User_CHC mkdir User_CHC

.\bin\HCC.exe HarLin_App\harlin.c -o User_CHC\harlin.chc >nul 2>build\error.log
if errorlevel 1 (
    echo %FAILURE% Creating harlin.chc
    echo %ERR%
    type build\error.log
    goto error
)
.\bin\bin2h.exe User_CHC\harlin.chc build\harlin_chc.h harlin_chc_data >nul 2>build\error.log
if errorlevel 1 (
    echo %FAILURE% Creating harlin_chc.h
    echo %ERR%
    type build\error.log
    goto error
)

echo %SUCCESS% Created User_CHC\harlin.chc

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
        nasm -f win64 %%f -o build\asm_!FILENAME!.o 2>build\error.log
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

%LD% -T src\harlin\linker.ld -o build\kernel.tmp %OBJS% -m i386pep -nostdlib 2>build\error.log
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

for /f %%a in ('"%MSYS_PATH%\usr\bin\truncate.exe" -s %%512 build\kernel.bin 2^>^&1') do set TRUNC_OUT=%%a
if errorlevel 1 (
    echo %FAILURE% Padding kernel.bin
    echo %ERR%
    goto error
)

copy /b build\boot.bin + build\kernel.bin build\HarLin.img >nul
if errorlevel 1 (
    echo %FAILURE% Creating disk image
    echo %ERR%
    type build\error.log
    goto error
)
echo %SUCCESS% Creating disk image

echo(
echo Building shell.chc...
.\bin\HCC.exe HarLin_App\shell\shell.c -o User_CHC\shell.chc >nul 2>build\shell_error.log
if errorlevel 1 (
    echo [Warning] shell.chc build failed (non-critical)
    type build\shell_error.log
) else (
    echo Created User_CHC\shell.chc
)

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
