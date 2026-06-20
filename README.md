# HarLin Kernel

HarLin Kernel 是由 HarLin228 Studio 独立开发的 x86_64 通用操作系统内核。

本项目采用统一 API 层设计，提供基础硬件抽象与驱动接口，便于第三方开发者基于本内核进行二次开发、功能扩展与再发布。

详细更新内容 /docs/version.md

本项目采取覆盖式提交 不保留旧版本代码 但在Version中记录

## 主要特性

- x86_64 64 位长模式
- MBR 引导扇区
- 统一 API 层：`harlin_API.h` / `harlin_API.c`
- IDT / PIC / 键盘中断支持
- 显示模式：
  - VGA 03h 文本模式（默认启动）
  - VGA 13h 320x200x256 图形模式
  - VESA 800x600x24 图形模式（需 Bootloader 开启）
- 基础物理内存管理（PMM）
- 基础虚拟内存管理（VMM）
- ATA PIO 磁盘 I/O
- MBR 分区表解析
- FAT32 文件系统（读取、写入、创建）
- 进程间通信管道（Pipe）
- RTL8139 网络接口支持（默认不启用）
- HTTP/1.1 客户端，支持 Chunked 传输编码

## 快速开始

### 环境要求

- Windows 10/11
- NASM 2.14+
- MinGW-w64 x86_64 GCC
- GNU Binutils（ld、objcopy）
- QEMU（可选，用于运行调试）

工具链路径已预设为 `D:\Code\Local_tool_library\`，可按实际环境修改 `build.bat` 与 `run.bat`。

### 构建与运行

```powershell
cd D:\Code\Code\HarLIn_Boot
.\build.bat
.\run.bat
```

构建成功后会生成 `build/HarLin.img`，`run.bat` 使用 QEMU 启动。

内核默认纯黑屏启动。如需添加启动画面或图形界面，请参考 `docs/手册.md`。

## 文档

- `docs/手册.md`：小白上手指册，包含逐行修改教程与常见坑解决
- `docs/HarLin_API_Spec.md`：API 规范
- `docs/HarLin_CXC_Format.md`：.cxc 用户态可执行格式说明
- `docs/version.md`：版本更新记录

## 开源协议

本项目基于 MIT 开源协议发布。

© 2026 HarLin228 Studio
