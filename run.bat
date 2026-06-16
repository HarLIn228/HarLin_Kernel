@echo off

if not exist "build\HarLin.img" (
    echo Disk image not found, please build first
    exit /b 1
)

echo Starting QEMU with debug output...
"D:\QEMU\qemu\qemu-system-i386.exe" -fda "build\HarLin.img" -boot a -m 32M -vga std -d int,cpu_reset,guest_errors -no-reboot -no-shutdown
