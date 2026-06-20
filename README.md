# HarLin Kernel

HarLin Kernel 是由 HarLin228 Studio 独立开发的 x86_64 通用操作系统内核。

本项目采用统一 API 层设计，提供基础硬件抽象与驱动接口，便于第三方开发者基于本内核进行二次开发、功能扩展与再发布。

## 主要特性

- x86_64 64位长模式
- MBR 引导扇区
- 统一 API 层：harlin_API.h / harlin_API.c
- IDT / PIC / 键盘中断支持
- VGA 03h 文本模式输出
- VGA 13h / VESA 显示模式接口（由第三方驱动实现）
- 基础物理内存管理（PMM）
- 基础虚拟内存管理（VMM）
- ATA PIO 磁盘 I/O
- MBR 分区表解析
- FAT32 只读文件系统
- RTL8139 网络接口支持

## 开源协议

本项目基于 MIT 开源协议发布。

(C) 2026 HarLin228 Studio
