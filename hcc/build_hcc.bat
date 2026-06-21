@echo off
setlocal

cd /d "%~dp0"

set CC=x86_64-w64-mingw32-gcc
set CFLAGS=-O2 -Wall -Wextra -std=c99 -static

if not exist ..\bin mkdir ..\bin

%CC% %CFLAGS% hcc.c -o ..\bin\HCC.exe >nul 2>&1
if errorlevel 1 (
    echo [Failure] Building HCC
    exit /b 1
)

echo [Success] [1/2] Built bin\HCC.exe

%CC% %CFLAGS% bin2h.c -o ..\bin\bin2h.exe >nul 2>&1
if errorlevel 1 (
    echo [Failure] Building bin2h
    exit /b 1
)

echo [Success] [2/2] Built bin\bin2h.exe
