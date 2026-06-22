@echo off
setlocal

cd /d "%~dp0"

set BOCHS_DIR=D:\Bochs-3.0
set QEMU_DIR=D:\QEMU\qemu
set BOCHS_EXE=%BOCHS_DIR%\bochs.exe
set QEMU_EXE=%QEMU_DIR%\qemu-system-x86_64.exe

if not exist "build\HarLin.img" (
    echo Disk image not found, please build first
    exit /b 1
)

if not exist "build" mkdir build

REM 选择模拟器
set EMULATOR=bochs
if /I "%~1"=="qemu" set EMULATOR=qemu
if /I "%~1"=="bochs" set EMULATOR=bochs

if "%EMULATOR%"=="bochs" (
    if not exist "%BOCHS_EXE%" (
        echo Bochs not found at %BOCHS_EXE%
        echo Falling back to QEMU...
        set EMULATOR=qemu
    )
)

if "%EMULATOR%"=="bochs" (
    echo Starting Bochs...
    "%BOCHS_EXE%" -f bochsrc.txt -q
) else (
    echo Starting QEMU...
    "%QEMU_EXE%" -fda "build\HarLin.img" -boot a -m 32M -vga std -no-reboot -no-shutdown -serial file:build\serial.log -machine smm=off
)
