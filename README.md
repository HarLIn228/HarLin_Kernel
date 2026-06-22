# HarLin Kernel

HarLin Kernel 是由 HarLin228 Studio 独立开发的 x86_64 通用操作系统内核。

本项目采用统一 API 层设计，提供基础硬件抽象与驱动接口，便于第三方开发者基于本内核进行二次开发、功能扩展与再发布。

### 注意：HarLin Kernel 为独立操作系统内核，不基于 Linux，也不兼容 Linux 可执行文件、Linux 系统调用或 Linux 驱动程序

## 功能与特性

### 内核基础
- x86_64 长模式，PAE 分页（PML4）
- 物理内存管理器（PMM）：页分配/释放、连续页分配、ISA DMA 低位内存分配
- 虚拟内存管理器（VMM）：页映射/取消映射、2MB 大页自动拆分
- 动态内存分配器（kmalloc）：支持 kmalloc/kfree/krealloc/ksize
- 多核 SMP 启动：Local APIC 初始化、IPI 发送、AP 引导
- 自旋锁与原子操作
- per-cpu 变量

### 中断与调度
- IDT 中断描述符表
- PIC 与 Local APIC 中断控制器
- 优先级调度、时间片轮转、就绪/睡眠队列
- 用户态 ring3 切换与系统调用（int 0x80）

### 驱动
- 键盘：中断驱动、LED 控制、修饰键状态、缓冲区管理
- ATA PIO LBA28：磁盘读写
- RTL8139：PCI 网卡驱动
- Sound Blaster 16：DMA 方式 16-bit PCM 播放
- USB UHCI：控制传输基础框架
- PCI 总线枚举
- 鼠标驱动
- RTC 时钟

### 文件系统
- MBR 分区表解析
- FAT32：文件读写、创建、删除、目录遍历

### 网络协议栈
- ARP：地址解析协议
- IP：网络层收发
- ICMP：Ping 响应
- UDP：用户数据报协议
- TCP：多连接管理、收发窗口
- DHCP：四步握手自动获取 IP/网关/DNS
- DNS：域名解析
- HTTP 1.1 客户端

### 进程间通信
- 管道（Pipe）：创建、读写、关闭

### 可执行格式
- CHC 格式加载器：段加载、重定位、动态链接
- 动态链接器：dlopen/dlsym/dlclose 系统调用

### 图形与显示
- VGA 文本模式（80x25）
- VGA 13H 图形模式（320x200，256 色）
- VESA 图形模式（800x600x24/32）
- 8x16 字体渲染

### 电源管理
- ACPI：RSDP 扫描、FADT 解析、DSDT S5 解析
- acpi_power_off()：硬件关机
- acpi_reboot()：系统重启

### 音频
- WAV 文件解析
- Harlin_AudioInit / PlayWav / Stop / IsPlaying API

## 文档

- `docs/手册.md`：小白上手指册
- `docs/HarLin_API_Spec.md`：API 规范
- `docs/HarLin_CHC_Format.md`：.chc 可执行格式说明
- `docs/HCC.md`：HCC 编译器使用说明
- `docs/version.md`：版本更新记录

## 鸣谢
感谢所有使用HarLin Kernel的用户 开发者 宣传者.

## 开源协议

本项目基于 MIT 开源协议发布。

© 2026 HarLin228 Studio
