# HarLin Kernel

HarLin Kernel 是由 HarLin228 Studio 独立开发的 x86_64 通用操作系统内核。

本项目采用统一 API 层设计，提供基础硬件抽象与驱动接口，便于第三方开发者基于本内核进行二次开发、功能扩展与再发布。

### 注意：HarLin Kernel 为独立操作系统内核，不基于 Linux，也不兼容 Linux 可执行文件、Linux 系统调用或 Linux 驱动程序

## 快速开始

### 环境要求

- Windows 10/11
- NASM 2.14+
- MinGW-w64 x86_64 GCC
- GNU Binutils（ld、objcopy）
- QEMU（可选，用于运行调试）

将工具链加入系统 PATH，或在 `build.bat` 与 `run.bat` 中修改工具路径。

### 构建与运行

```powershell
cd Kernel
.\build.bat
.\run.bat
```

构建成功后会生成 `build/HarLin.img`，`run.bat` 使用 QEMU 启动。

## 文档

- `docs/手册.md`：小白上手指册
- `docs/HarLin_API_Spec.md`：API 规范
- `docs/HarLin_CHC_Format.md`：.chc 可执行格式说明
- `docs/HCC.md`：HCC 编译器使用说明
- `docs/version.md`：版本更新记录

## 开源协议

本项目基于 MIT 开源协议发布。

© 2026 HarLin228 Studio
