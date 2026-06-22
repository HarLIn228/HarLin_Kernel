# Bochs 安装说明

## 下载地址

1. **SourceForge (推荐)**
   https://sourceforge.net/projects/bochs/files/bochs/
   - 选择 `bochs-2.x.y.tar.gz` 或 Windows 预编译版

2. **GitHub Releases**
   https://github.com/bochs-emu/Bochs/releases

## 安装路径

将 Bochs 解压到 `D:\Bochs-3.0\`，最终结构：
```
D:\Bochs-3.0\
├── bochs.exe              # 主程序
├── bochsdbg.exe           # 调试版
├── BIOS-bochs-latest      # BIOS 镜像
├── VGABIOS-lgpl-latest    # VGA BIOS 镜像
├── keymaps\               # 键盘映射
│   └── xtus.kmap
└── ...
```

## 所需文件

如果压缩包中没有 BIOS 镜像，可以从以下地址单独下载：
- BIOS: https://sourceforge.net/projects/bochs/files/BIOS%20Images/
- VGABIOS: https://sourceforge.net/projects/bochs/files/VGABIOS/

需要的文件：
- `BIOS-bochs-latest` 或 `bios.bin`
- `VGABIOS-lgpl-latest` 或 `vgabios.bin`

## 启动

```bash
# 默认启动 (自动选择 Bochs，如果已安装)
.\run.bat

# 显式指定 Bochs
.\run.bat bochs

# 使用 QEMU
.\run.bat qemu
```

## 调试

Bochs 启动后按 `Ctrl+C` 进入调试器：
- `help` - 查看命令
- `r` - 查看寄存器
- `s` - 单步
- `c` - 继续
- `b addr` - 断点
- `info gdt` / `info idt` / `info cr0/cr3/cr4`
- `dump_cpu` - 完整 CPU 状态
- `xp /128bx 0x10000` - 查看物理内存
- `set $reg=val` - 修改寄存器

启动时如果 `magic_break: enabled=1`，MBR 中的 `xchg bx,bx` / `xchg ebx,ebx` 会触发调试器断点。

## 串口输出

串口 (COM1) 输出到 `build\serial.log`，内核可通过 0x3F8 端口输出调试信息。

## 常见问题

1. **找不到 bochs.exe** - 检查路径是否在 `D:\QEMU\bochs\`
2. **BIOS 加载失败** - 确保 BIOS-bochs-latest 在正确路径
3. **键盘无响应** - 检查 `display_library: win32` 设置
