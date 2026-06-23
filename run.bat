@echo off
setlocal

cd /d "%~dp0"

set BOCHS_DIR=D:\Bochs-3.0
set BOCHS_EXE=%BOCHS_DIR%\bochs.exe

if not exist "%BOCHS_EXE%" (
    echo Bochs not found at %BOCHS_EXE%
    echo Please install Bochs to %BOCHS_DIR%
    echo Download: https://sourceforge.net/projects/bochs/files/bochs/
    echo Or: https://github.com/bochs-emu/Bochs/releases
    exit /b 1
)

if not exist "build\HarLin.img" (
    echo Disk image not found, please build first
    exit /b 1
)

if not exist "build" mkdir build

echo Starting Bochs...
"%BOCHS_EXE%" -f bochsrc.txt -q
