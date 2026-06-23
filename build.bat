@echo off
setlocal enabledelayedexpansion

cd /d "%~dp0"

set MSYS_PATH=D:\Code\Local_tool_library\msys
set NASM_PATH=D:\Code\Local_tool_library\nasm
set LLVM_PATH=D:\Code\Local_tool_library\LLVM\bin
set PATH=%NASM_PATH%;%MSYS_PATH%\mingw64\bin;%MSYS_PATH%\usr\bin;%LLVM_PATH%;%PATH%

for /F %%a in ('echo prompt $E ^| cmd') do set "ESC=%%a"

set GREEN=%ESC%[32m
set YELLOW=%ESC%[33m
set RED=%ESC%[31m
set RESET=%ESC%[0m

if not "%1%2%3"=="" (
    echo %YELLOW%[Warn] This shouldn't be here: %*%RESET%
    exit /b 1
)

set SUCCESS=%GREEN%[Success]%RESET%
set FAILURE=%RED%[Failure]%RESET%
set ERR=%RED%[Error]%RESET%

set CC=D:\Code\Local_tool_library\LLVM\bin\clang.exe
set LD=D:\Code\Local_tool_library\LLVM\bin\ld.lld.exe
set CFLAGS=-ffreestanding -c -target x86_64-unknown-none-elf -O2 -Wall -Wextra -fno-exceptions -fno-stack-protector -fno-stack-check -fno-asynchronous-unwind-tables -fno-unwind-tables -nostdlib -nodefaultlibs -I src\head -I src\head\mem -I src\head\fs -I src\head\net -I src\head\drv -I src\head\acpi -I src\head\gpu -I src\head\syscall -I src\head\proc -I src\head\shell -I src\head\core -I build -mno-sse -mno-mmx -mabi=sysv

set TOTAL=0
for /r src\asm %%f in (*.asm) do set /a TOTAL+=1
for /r src\harlin %%f in (*.c) do set /a TOTAL+=1
set COUNT=0

if exist build rmdir /s /q build
if not exist build mkdir build
if not exist build\Binary mkdir build\Binary

echo Building kernel...

nasm -O0 -w-orphan-labels -w-label-redef-late -f bin src\asm\boot\boot.asm -o build\Binary\boot.bin 2>build\error.log
if errorlevel 1 (
    echo %RED%[Failure] Compiling boot.asm%RESET%
    for /f "delims=" %%a in (build\error.log) do echo %RED%%%a%RESET%
    goto error
)
set /a COUNT+=1
echo %SUCCESS% [!COUNT!/%TOTAL%] Compiling boot.asm

nasm -O0 -w-orphan-labels -w-label-redef-late -f bin src\asm\boot\stage2.asm -o build\Binary\stage2.bin 2>build\error.log
if errorlevel 1 (
    echo %RED%[Failure] Compiling stage2.asm%RESET%
    for /f "delims=" %%a in (build\error.log) do echo %RED%%%a%RESET%
    goto error
)
set /a COUNT+=1
echo %SUCCESS% [!COUNT!/%TOTAL%] Compiling stage2.asm

set OBJS=
for /r src\asm %%f in (*.asm) do (
    set "FPATH=%%f"
    set "EXCLUDE="
    if not "!FPATH:\boot\=!"=="!FPATH!" set "EXCLUDE=1"
    set "FNAME=%%~nf"
    if /I "!FNAME!"=="gdt" set "EXCLUDE=1"
    if not defined EXCLUDE (
        set FILENAME=%%~nf
        nasm -f elf64 %%f -o build\asm_!FILENAME!.o 2>build\error.log
        if errorlevel 1 (
            echo %RED%[Failure] Compiling %%f%RESET%
            for /f "delims=" %%a in (build\error.log) do echo %RED%%%a%RESET%
            goto error
        )
        set /a COUNT+=1
        echo %SUCCESS% [!COUNT!/%TOTAL%] Compiling %%f
        set OBJS=!OBJS! build\asm_!FILENAME!.o
    )
)

for /r src\harlin %%f in (*.c) do (
    set FILENAME=%%~nf
    %CC% %CFLAGS% -o build\!FILENAME!.o %%f 2>build\error.log
    if errorlevel 1 (
        echo %RED%[Failure] Compiling %%f%RESET%
        for /f "delims=" %%a in (build\error.log) do echo %RED%%%a%RESET%
        goto error
    )
    set /a COUNT+=1
    echo %SUCCESS% [!COUNT!/%TOTAL%] Compiling %%f
    set OBJS=!OBJS! build\!FILENAME!.o
)

%LD% -T src\harlin\linker.ld -o build\kernel.tmp %OBJS% -m elf_x86_64 -nostdlib 2>build\error.log
if errorlevel 1 (
    echo %RED%[Failure] Linking kernel%RESET%
    for /f "delims=" %%a in (build\error.log) do echo %RED%%%a%RESET%
    goto error
)
echo %SUCCESS% Linking kernel

objcopy -O binary build\kernel.tmp build\Binary\kernel.bin 2>build\error.log
if errorlevel 1 (
    echo %RED%[Failure] Creating kernel.bin%RESET%
    for /f "delims=" %%a in (build\error.log) do echo %RED%%%a%RESET%
    goto error
)
echo %SUCCESS% Creating kernel.bin
copy /Y build\kernel.tmp build\kernel.elf >nul 2>&1
if errorlevel 1 (
    echo %RED%[Failure] Creating kernel.elf%RESET%
    for /f "delims=" %%a in (build\error.log) do echo %RED%%%a%RESET%
    goto error
)
echo %SUCCESS% Creating kernel.elf

for /f %%a in ('"%MSYS_PATH%\usr\bin\truncate.exe" -s %%512 build\Binary\kernel.bin 2^>^&1') do set TRUNC_OUT=%%a
if errorlevel 1 (
    echo %RED%[Failure] Padding kernel.bin%RESET%
    goto error
)

copy /b build\Binary\boot.bin + build\Binary\stage2.bin + build\Binary\kernel.bin build\boot.img >nul
if errorlevel 1 (
    echo %RED%[Failure] Creating boot image%RESET%
    for /f "delims=" %%a in (build\error.log) do echo %RED%%%a%RESET%
    goto error
)
echo %SUCCESS% Creating boot image

mkdir build\iso_root 2>nul
copy /Y build\boot.img build\iso_root\boot.img >nul

set XORRISO=%MSYS_PATH%\usr\bin\xorriso.exe
for /f %%a in ('powershell -NoProfile -Command "[Math]::Ceiling((Get-Item 'build\boot.img').Length / 2048)"') do set LOAD_SIZ=%%a
"%XORRISO%" -as mkisofs -o build\HarLin.iso -b boot.img -no-emul-boot -boot-load-size !LOAD_SIZ! --sort-weight 1 boot.img build\iso_root 2>build\error.log
if errorlevel 1 (
    echo %RED%[Failure] Creating ISO image%RESET%
    for /f "delims=" %%a in (build\error.log) do echo %RED%%%a%RESET%
    goto error
)
echo %SUCCESS% Creating ISO image
rmdir /s /q build\iso_root

fsutil file createnew build\HarLin.img 1474560 >nul 2>&1
if errorlevel 1 (
    echo %RED%[Failure] Creating 1.44MB disk image%RESET%
    goto error
)

for /f %%a in ('powershell -NoProfile -Command "((Get-Item 'build\Binary\boot.bin').Length + (Get-Item 'build\Binary\stage2.bin').Length + (Get-Item 'build\Binary\kernel.bin').Length)"') do set IMG_OFFSET=%%a
set /a PAD_SIZE=1474560-!IMG_OFFSET!
if !PAD_SIZE! lss 0 set PAD_SIZE=0
fsutil file createnew build\pad.bin !PAD_SIZE! >nul 2>&1

copy /b build\Binary\boot.bin + build\Binary\stage2.bin + build\Binary\kernel.bin + build\pad.bin build\HarLin.img >nul
if errorlevel 1 (
    echo %RED%[Failure] Creating disk image%RESET%
    for /f "delims=" %%a in (build\error.log) do echo %RED%%%a%RESET%
    goto error
)
del build\pad.bin 2>nul
echo %SUCCESS% Creating disk image

rem del build\kernel.tmp 2>nul
del build\asm_*.o 2>nul
del build\*.o 2>nul
if exist build\error.log del build\error.log
if exist build\shell_error.log del build\shell_error.log
if exist build\boot.img del build\boot.img
if exist build\kernel.tmp del build\kernel.tmp
if exist build\kernel.elf del build\kernel.elf

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
