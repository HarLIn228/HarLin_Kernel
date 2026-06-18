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

set CFLAGS=-ffreestanding -c -m32 -O2 -Wall -Wextra -fno-exceptions -fno-stack-protector -fno-stack-check -nostdlib -nodefaultlibs -I src\head -mno-sse -mno-mmx

if not exist build mkdir build

nasm -f bin src\ASM\boot.asm -o build\boot.bin 2>build\error.log
if errorlevel 1 (
    echo %FAILURE% Compiling boot.asm
    echo %ERR%
    type build\error.log
    goto error
)
echo %SUCCESS% Compiling boot.asm

set OBJS=
for %%f in (src\Sys_C\*.c) do (
    set FILENAME=%%~nf
    gcc %CFLAGS% -o build\!FILENAME!.o %%f 2>build\error.log
    if errorlevel 1 (
        echo %FAILURE% Compiling %%f
        echo %ERR%
        type build\error.log
        goto error
    )
    echo %SUCCESS% Compiling %%f
    set OBJS=!OBJS! build\!FILENAME!.o
)

ld -T src\Sys_C\linker.ld -o build\kernel.tmp %OBJS% -m i386pe -nostdlib 2>build\error.log
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

copy /b build\boot.bin + build\kernel.bin build\HarLin.img >nul
if errorlevel 1 (
    echo %FAILURE% Creating disk image
    echo %ERR%
    type build\error.log
    goto error
)
echo %SUCCESS% Creating disk image

del build\kernel.tmp 2>nul
if exist build\error.log del build\error.log

echo.
echo %GREEN%Build successful%RESET%
echo.
set /p WAIT=Press Y to exit...
if /i "%WAIT%"=="Y" goto end
goto end

:error
if exist build\error.log del build\error.log
echo.
echo %RED%Build failed%RESET%
exit /b 1

:end
endlocal
