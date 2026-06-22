# HarLin Kernel 版本历史

本项目采用 CalVer 版本号，格式为 `YY.D.X`：
## H26.3.1（最新）

- 应用目录重构：HarLin_App/ 存放系统应用，harlin.c 为默认启动程序，shell/ 为独立 Shell CHC 应用
- Shell 从内核源码移除外置为独立 CHC 应用，支持命令：help、say、end、exit、run、exec、pid、beep、sleep、time、clearkeys
- HCC 编译器工具链预编译二进制（bin/HCC.exe、bin/bin2h.exe）
- User_CHC/ 目录存放预编译 CHC 程序（harlin.chc、shell.chc）
- build.bat 更新为编译 CHC 应用的构建流程
- 移除内核内置 shell.c/shell.h，清理 harlin_API.h 中废弃声明
- Sound Blaster 16 音频驱动，支持 DMA 方式播放 16-bit PCM
- WAV 文件解析器
- 音频播放 API：Harlin_AudioInit、Harlin_AudioPlayWav、Harlin_AudioStop、Harlin_AudioIsPlaying
- PMM 支持连续物理页分配 pmm_alloc_contiguous
- FAT32 支持文件删除：Harlin_DeleteFile
- 更新 `README.md`、`手册.md`、`HarLin_API_Spec.md` 文档
- `.gitignore` 重写为符合项目需求

## H26.3

- 大版本发布：新增多核 SMP 启动、自旋锁/原子操作、per-cpu 变量、更完善的调度器、更多系统调用、动态内存分配 kmalloc、RTC 时钟、扩展键盘功能
- 新增多核 SMP 启动：Local APIC 初始化、IPI 发送、AP 引导 trampoline
- 新增 spinlock 与原子操作：xchg/add/sub/cmpxchg
- 新增 per-cpu 变量支持
- 调度器增强：优先级调度、时间片轮转、就绪/睡眠队列
- 新增系统调用：getpid、getcpu、time、beep、kmalloc、kfree、mmap、unmap、getkeystate、keyled、setpriority
- 新增动态内存分配器 kmalloc/kfree/krealloc/ksize
- 新增 RTC 时钟读取 API
- 扩展键盘功能：LED 控制、修饰键状态、缓冲区清空、扫描码集切换
- 新增 Sound Blaster 16 音频驱动，支持 WAV 播放
- 修复系统调用门使用陷阱门（0xEF），避免系统调用时清除中断标志
- 修复 SMP SIPI 向量传递错误
- 修复 SMP AP 启动时未设置 TSS RSP0 的问题，每个 CPU 使用独立 GDT/TSS
- 修复 pipe.c、kmalloc.c、network.c 中物理地址直接当作虚拟地址使用的问题
- 修复 PMM 位图未标记内核与位图自身占用内存的问题
- 修复 vmm_map 对用户态虚拟地址范围缺少校验的问题
- 修复 syscall.c 中 sys_kmalloc / sys_kfree 参数校验缺失
- 修复 FAT32 目录查找遇到空条目提前返回导致漏找文件的问题
- 从公开 API 中移除 Harlin_IntOn / Harlin_IntOff，避免用户态调用
- 更新 `HarLin_API_Spec.md`、`手册.md`、`HarLin_CHC_Format.md`、`HCC.md` 文档

## H26.2.1

- 项目根目录由 `HarLIn_Boot` 重命名为 `Kernel`
- 优化源码目录结构：`src/ASM` -> `src/asm`、`src/Sys_C` -> `src/harlin`、`tools` -> `hcc`
- 新增 `bin/` 目录存放可执行工具
- 修复了运行chc程序崩溃的问题
- 新增 HCC 编译器
- 新增 `bin2h` 工具，用于将二进制文件转换为 C 头文件数组
- 新增 `docs/HCC.md` 文档，说明 HCC 命令用法与示例
- 新增test文件夹

## H26.2

- 大版本发布：内核启动崩溃 bug 已修复，API 与可执行格式完成重构
- 简化 HarLin_API 命名，让 API 更短更易记（如 `Harlin_ConPrint` -> `Harlin_Print`、`Harlin_FsOpen` -> `Harlin_Open`）
- 将 CX 用户态可执行格式升级为 CHC，魔数改为 `HARLINCHC`
- 增强 CHC 加载器健壮性：严格校验头部边界、段大小、重定位对齐，失败时自动回滚已映射页面
- 更新 API 规范和 CHC 格式文档

## H26.1.6

- 修复 idt_load 汇编函数错误地从栈读取参数的问题，改为从 RDI 寄存器读取
- 修复因 IDT 未正确加载导致的定时器中断 Triple Fault 崩溃
- Bootloader 精简启动输出，保持纯黑屏启动

## H26.1.5

- 实现 VGA 13H 图形模式（320x200，256 色），支持 set_mode/clear/put_pixel
- 初始化 VGA 13H 标准 16 色调色板与灰度扩展色
- 实现 VESA 800x600x24 图形模式，Bootloader 通过标准 VBE INT 10h 设置模式并保存模式信息
- 修复 Bootloader setup_vesa 寄存器覆盖问题，调整 VBE 信息获取顺序
- display.c 读取 VBE 模式信息获取 LFB 地址、分辨率与色深，支持 24bpp 与 32bpp
- 修复 VESA LFB 映射到与内核低地址冲突的问题，统一映射到 0xFFFF800000000000
- VESA 模式设置失败时自动回退到 VGA 文本模式
- 实现 HTTP Chunked 传输编码流式解码状态机，支持任意大小响应
- HTTP 请求升级到 HTTP/1.1，保持 Connection: close 兼容性
- 实现 FAT32 文件写入支持（Harlin_FsWrite）
- 实现 FAT32 空闲簇查找、簇分配、簇链扩展与 FAT 表双副本同步写入
- 实现 FAT32 文件创建（Harlin_FsCreate），支持 8.3 短文件名
- 修复 Harlin_FsCreate 不检查文件已存在的问题，避免同名目录项冲突
- 在 Harlin_File 中记录目录项位置，写入后自动更新文件大小
- 实现进程间通信管道（Pipe）内核对象
- 新增 PIPE_CREATE / PIPE_READ / PIPE_WRITE / PIPE_CLOSE / PIPE_READY 系统调用
- 新增 Harlin_PipeCreate / Read / Write / Ready / Close 用户态 API
- 修复 sys_pipe_create 返回值设计缺陷，返回管道 ID
- 修复 harlin_API.c 中 Harlin_FsRead/FsWrite/FsSize/FsClose 重复定义导致的无限递归
- 内核改为纯黑屏启动，不显示任何输出
- 修复内核启动后显示模式被 Bootloader VBE 改变的问题，强制回到 VGA 文本模式
- 移除 Bootloader 默认 VBE 调用，默认以 VGA 文本模式启动
- 重写 `docs/手册.md` 为小白上手版本，包含逐行修改位置与常见坑解决
- 修复网络初始化 ARP 查询超时过长导致启动卡住的问题
- 默认启动不初始化网络模块，保持内核地基稳定启动
- 在 `docs/手册.md` 中新增网络模块启用说明

## H26.1.4

- 修复 RTC 中断风暴导致的系统崩溃，禁用 CMOS 周期性中断与 Local APIC
- 修复 PIC 初始化时序，添加 ICW 命令间延迟
- 修复 ATA IRQ 竞态与无限循环卡死，删除写操作后错误的 0xE7 命令
- 修复键盘驱动中断状态保存错误（cli/sti 替换为 pushf/popf）
- 修复网络驱动全局静态缓冲区重入问题，改为局部变量
- 修复网卡 DMA 传递虚拟地址问题，改为分配物理内存发送
- 修复 FAT32 cluster_buf 硬编码大小导致的溢出风险
- 修复 FAT32 sectors_per_cluster 上限检查，拒绝异常分区
- 修复 cx_loader map_user_pages 失败时的内存泄漏，添加回滚逻辑
- 修复 scheduler timer_handler 的 frame 索引与中断栈帧布局不匹配
- 修复 sys_exec 调用 schedule 后无法返回的问题
- 修复 schedule 只调度一次的缺陷，支持重复抢占切换
- 修复调度器竞态条件，使用 pushf/popf 保存中断状态
- 修复 GDT 用户代码/数据段 32 位描述符错误，修正为 64 位
- 修复 VMM 大页映射不兼容用户 4KB 页的问题，实现 2MB 大页拆分
- 修复 VMM 空指针解引用风险，处理分配失败回滚
- 修复 sys_alloc 返回物理地址的安全漏洞，改为返回用户虚拟地址
- 修复 sys_free 可释放任意物理页的漏洞，限制只释放进程拥有的页
- 修复 sys_print/sys_open/sys_read 等系统调用未验证用户指针的问题
- 修复 sys_alloc 从 0x500000 开始扫描避免覆盖代码段
- 修复 sys_alloc next_free_virt 跨进程共享的碎片问题，改为每个进程独立
- 修复 process_exit 页面泄漏，添加 vmm_unmap 并记录虚拟地址
- 修复 process_exit 死循环问题，标记为 noreturn 并使用 __builtin_unreachable
- 修复 schedule 无就绪进程时直接返回导致的卡死，改为 sti; hlt; cli 等待
- 修复 sys_yield 空函数问题，调用 schedule 真正让出 CPU
- 修复 DNS 解析越界读取与无限循环风险
- 修复键盘 Shift 键状态错误，使用 shift_count 计数器支持双 Shift
- 修复键盘缓冲区满时静默丢弃按键的问题，添加溢出计数器
- 修复 Harlin_ConSetColor 错误实现，正确遍历 VGA 属性字节
- 修复 Harlin_ConPrint 在真实硬件上可能出错的 QEMU 0xE9 调试端口输出
- 修复 syscall_stub 缺少 noreturn 标记的问题
- 新增 HARLIN_SYS_KEYOVERFLOW 系统调用，暴露键盘溢出计数
- 统一字符串比较函数，删除重复的 string.c/string.h，全面使用 Harlin_StrCmp
- 修复链接错误 ___chkstk_ms 未定义的问题
- 修复 harlin_API.c 中 outb/inb 函数未声明的编译错误
- 修复启动时网络初始化顺序错误，确保 PMM/VMM 初始化后再初始化网络
- 修复 QEMU 命令行 -no-hpet 参数错误
- 修复启动时文件系统初始化顺序，先 DiskInit/PartitionInit 再挂载 FAT32 分区

## H26.1.3

- 将键盘、ATA 磁盘、RTL8139 网络改为中断驱动，移除轮询
- 修复 PIC 中断完成信号（EOI）导致 CPU 休眠的问题
- 添加 64 位 GDT/TSS 与用户态 ring3 切换支持
- 添加 `int 0x80` 系统调用入口与基础系统调用表
- 设计并实现 HarLin .cx 可执行文件加载器
- 添加基础进程调度框架
- 修复并发访问与缓冲区安全 bug
- 优化项目结构，文档统一归集到 docs/ 目录
- 重写二次开发手册

## H26.1.2

- 将内核从 x86 32 位保护模式迁移至 x86_64 64 位长模式
- 重写引导程序，启用 PAE 分页与 PML4 页表机制
- 更新 64 位 GDT、IDT 与中断处理程序
- 构建脚本升级为 MinGW-w64 x86_64 工具链
- 添加统一 API 层 `harlin_API.h` / `harlin_API.c`
- 添加物理内存管理器（PMM）与虚拟内存管理器（VMM）
- 添加 ATA PIO LBA28 磁盘 I/O 驱动
- 添加 MBR 分区表解析
- 添加 FAT32 只读文件系统支持（短文件名）
- 移除内置 Shell，仅保留接口供第三方扩展
- 更新手册与 README

## H26.1.1

- 修复源代码安全隐患：
  - 变量名越界
  - const 修饰导致的未定义行为
  - 整数溢出
  - 缓冲区溢出
  - 字节序错误
  - 未初始化变量
- 更新构建输出

## H26.1

- 初始版本发布
- MBR 引导扇区
- x86 32 位保护模式内核
- VGA 文本模式与 VESA 图形模式支持
- 键盘中断与轮询输入
- 内置 Shell
- RTL8139 网卡基础支持
- 蜂鸣器驱动
- 基础字符串与屏幕输出工具
