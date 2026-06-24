# Bochs 安装与使用说明

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

## 项目配置

Bochs 配置文件位于项目根目录 `bochsrc.txt`。默认配置如下：

```
romimage: file=D:\Bochs-3.0\BIOS-bochs-latest, options=fastboot
vgaromimage: file=D:\Bochs-3.0\VGABIOS-lgpl-latest.bin

cpu: model=core2_penryn_t9600, count=1, ips=50000000, reset_on_triple_fault=1, ignore_bad_msrs=1
cpu: cpuid_limit_winnt=0

memory: guest=512, host=256

ata0: enabled=1, ioaddr1=0x1f0, ioaddr2=0x3f0, irq=14
ata0-master: type=cdrom, path="build\HarLin.iso", status=inserted

boot: cdrom

floppy_bootsig_check: disabled=0

log: build\bochs.log

magic_break: enabled=1

pci: enabled=1, chipset=i440fx

port_e9_hack: enabled=1
```

### 配置项说明

| 配置项 | 说明 |
|--------|------|
| `romimage` | BIOS 镜像路径 |
| `vgaromimage` | VGA BIOS 镜像路径 |
| `cpu: model=...` | CPU 型号、核心数、指令速度 |
| `memory: guest=N` | 客户机内存大小（MB） |
| `ata0-master: type=cdrom` | CD-ROM 镜像挂载 |
| `boot: cdrom` | 从光盘启动 |
| `log: build\bochs.log` | Bochs 日志输出路径 |
| `magic_break: enabled=1` | 启用魔术断点（`xchg bx,bx` 触发） |
| `port_e9_hack: enabled=1` | 启用 0xE9 调试端口输出 |

## 启动

### 标准启动

```powershell
.\run.bat
```

### 调试启动

```powershell
bochsdbg.exe -q -f bochsrc.txt
```

或直接双击 `bochsdbg.exe` 并在配置界面指定配置文件。

## 调试命令

Bochs 启动后按 `Ctrl+C` 进入调试器（仅 `bochsdbg.exe` 支持）。以下为常用命令：

### 基本调试

| 命令 | 说明 |
|------|------|
| `help` | 查看所有命令 |
| `c` | 继续执行 |
| `q` | 退出 |
| `s` | 单步执行（step） |
| `n` | 单步跳过（next，不进入 call） |

### 断点

| 命令 | 说明 |
|------|------|
| `b addr` | 设置物理地址断点，如 `b 0x7C00` |
| `lb addr` | 设置线性地址断点 |
| `pb addr` | 设置页地址断点 |
| `blist` | 列出所有断点 |
| `bpd N` | 删除第 N 个断点 |
| `d N` | 同上，删除断点 |
| `u` | 设置 I/O 断点 |

### 查看状态

| 命令 | 说明 |
|------|------|
| `r` | 查看通用寄存器 |
| `fp` | 查看浮点寄存器 |
| `mmx` | 查看 MMX 寄存器 |
| `sse` | 查看 SSE 寄存器 |
| `info gdt` | 查看 GDT 表 |
| `info idt` | 查看 IDT 表 |
| `info tss` | 查看 TSS |
| `info cr0` | 查看 CR0 寄存器 |
| `info cr3` | 查看 CR3 寄存器（页表基址） |
| `info cr4` | 查看 CR4 寄存器 |
| `info eflags` | 查看 EFLAGS |
| `dump_cpu` | 完整 CPU 状态 |
| `events` | 查看当前中断/异常事件 |
| `ptime` | 查看开机后 CPU 时间 |

### 查看内存

| 命令 | 说明 |
|------|------|
| `xp /Nxf addr` | 查看物理内存，如 `xp /8bx 0x7C00` |
| `x /Nxf addr` | 查看线性地址内存 |
| `page addr` | 查看指定线性地址的页表映射 |
| `setpmem addr len val` | 修改物理内存 |
| `crc addr len` | 计算内存段 CRC32 |
| `info pb` | 查看页表信息 |
| `info dirty` | 查看脏页 |

### 魔术断点

`bochsrc.txt` 中设置 `magic_break: enabled=1` 后，在汇编代码中插入：

```asm
xchg bx, bx        ; 16 位模式下触发断点
```

或

```asm
xchg ebx, ebx      ; 32/64 位模式下触发断点
```

执行到该指令时会自动暂停进入调试器。

### 反汇编

| 命令 | 说明 |
|------|------|
| `disasm start end` | 反汇编指定内存范围的指令 |
| `u /N` | 反汇编后面 N 条指令 |
| `u` | 反汇编下一条指令 |

## 串口输出

Bochs 默认将串口 COM1 输出重定向到文件。在内核代码中使用串口输出：

```c
void serial_write(const char* str) {
    while (*str) {
        outb(0x3F8, *str++);
    }
}
```

调试时使用串口输出比屏幕输出更可靠（不受显示模式切换影响）。日志文件位置在 `build\bochs.log`，若配置文件指定了串口输出文件，也可配置到独立文件：

```
com1: enabled=1, mode=file, dev=build\serial.log
```

## 网络配置

如需在 Bochs 中使用 RTL8139 网卡测试网络功能，在 `bochsrc.txt` 中添加：

```
pci: enabled=1, chipset=i440fx
ne2k: enabled=0
i440fxsb: enabled=0
e1000: enabled=0

# RTL8139 网卡
pci: slot=5, vendor=0x10ec, device=0x8139
```

注：Bochs 的网络后端配置较为复杂，建议网络相关开发使用 QEMU。

## 常见问题

### 1. 找不到 bochs.exe
检查路径是否与 `bochsrc.txt` 中的配置一致。确认 `D:\Bochs-3.0\` 存在且文件完整。

### 2. BIOS 加载失败
确保 `BIOS-bochs-latest` 在 `D:\Bochs-3.0\` 目录下。检查文件名是否完全匹配（包括大小写）。

### 3. 键盘无响应
检查 `bochsrc.txt` 中 `display_library: win32` 设置。如果使用其他显示库，键盘映射可能异常。

### 4. 启动后黑屏但无崩溃
这是正常现象——HarLin 内核默认纯黑屏启动。这是预期行为，表示内核已成功启动并进入主循环。如需显示输出，请参考手册中添加显示代码的章节。

### 5. 启动时报 "TIMES value is negative"
Bootloader 体积超过 512 字节限制。检查 `boot.asm` 中的代码和数据体积，减少不必要的字符串或代码。

### 6. Bochs 启动时闪退
检查日志文件 `build\bochs.log` 查看错误信息。常见原因：
- BIOS 或 VGABIOS 路径错误
- 镜像文件 `build\HarLin.iso` 不存在（需先执行 `Build.exe`）
- 内存配置超出主机可用范围

### 7. 调试时中断未触发
确认 `bochsrc.txt` 中 `magic_break: enabled=1`，且代码中的 `xchg bx,bx` 在实模式下执行。

### 8. 镜像更新后 Bochs 仍运行旧代码
Bochs 可能缓存了 ISO 镜像。重启 Bochs 即可加载新镜像。如果问题持续，删除 `build\HarLin.iso` 后重新执行 `Build.exe`。

## 串口与 0xE9 调试端口

Bochs 的 `port_e9_hack` 是一种高效的调试输出方式。在内核中使用：

```asm
mov dx, 0xE9
mov al, 'A'
out dx, al
```

输出会显示在 Bochs 控制台或日志中，不依赖显示模式。建议在早期启动阶段使用该方法调试。
