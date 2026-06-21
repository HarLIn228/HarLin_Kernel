@echo off

if not exist "build\HarLin.img" (
    echo Disk image not found, please build first
    exit /b 1
)

echo Starting QEMU with debug output...
"D:\QEMU\qemu\qemu-system-x86_64.exe" -fda "build\HarLin.img" -boot a -m 32M -vga std -netdev user,id=n0 -device rtl8139,netdev=n0 -d int,cpu_reset,guest_errors -no-reboot -no-shutdown -debugcon stdio
