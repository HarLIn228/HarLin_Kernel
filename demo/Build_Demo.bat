@echo off
setlocal enabledelayedexpansion

cd /d "%~dp0"

set LLVM_PATH=D:\Code\Local_tool_library\LLVM\bin
set PATH=%LLVM_PATH%;%PATH%

for /F %%a in ('echo prompt $E ^| cmd') do set "ESC=%%a"

set GREEN=%ESC%[32m
set RED=%ESC%[31m
set RESET=%ESC%[0m

if not "%1%2%3"=="" (
    echo %RED%[Error] This shouldn't be here: %*%RESET%
    exit /b 1
)

set SUCCESS=%GREEN%[Success]%RESET%
set FAILURE=%RED%[Failure]%RESET%

set CC=D:\Code\Local_tool_library\LLVM\bin\clang.exe
set LD=D:\Code\Local_tool_library\LLVM\bin\ld.lld.exe
set CFLAGS=-ffreestanding -c -target x86_64-unknown-none-elf -O2 -mno-sse -mno-mmx

echo Building demo...

%CC% %CFLAGS% -o Hello.o Hello.c 2>build_demo_err.log
if errorlevel 1 (
    echo %FAILURE% Compiling Hello.c
    for /f "delims=" %%a in (build_demo_err.log) do echo %RED%%%a%RESET%
    goto error
)
echo %SUCCESS% Compiling Hello.c

%LD% -Ttext 0x400000 -m elf_x86_64 -nostdlib -o Hello.elf Hello.o 2>>build_demo_err.log
if errorlevel 1 (
    echo %FAILURE% Linking Hello.elf
    for /f "delims=" %%a in (build_demo_err.log) do echo %RED%%%a%RESET%
    goto error
)
echo %SUCCESS% Creating Hello.elf

del Hello.o 2>nul
del build_demo_err.log 2>nul

echo.
echo %GREEN%Demo build successful%RESET%
echo.
exit /b 0

:error
echo.
echo %RED%Demo build failed%RESET%
exit /b 1

:end
endlocal
