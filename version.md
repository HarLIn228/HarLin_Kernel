# HarLin Kernel version.md

## v1.6.1(最新)

**新增**
- 构建：多余参数检测功能，Error报错信息所有文字都显示为红色
- 构建：添加了 `[warn]`
- 文档：新增 `docs/架构.md` 架构说明文档（14 章涵盖整体架构、内存布局、中断、系统调用、进程、文件系统、网络、SMP、ACPI 等）
- 文档：新增 `docs/实践.md` 实践教程文档（10 个由浅入深的上手教程）
- 文档：新增 `docs/Bochs.md` Bochs 模拟器使用指南
- 文档：新增项目 Logo `docs/images/logo.png`
- 文档：架构图 ASCII 艺术图替换为 Python 生成的 PNG 图片，共 9 张架构图（整体架构图、模块依赖图、中断流程图、系统调用路径图、进程状态图、启动序列图、内存布局图、网络协议栈图、内核入口图）

**修复**
- 引导：修复 Bochs 1.44MB floppy `int 0x13 ah=02` 装载 sector 2 失败（CF=0 但数据全零），改用 CD-ROM El Torito 启动
- 引导：修复 stage2 16 位模式下 `mov es, 0x1000` 直接使用立即数的编译错误，改为通过 AX 中转
- 引导：修复 stage2 `mov cx, 0x20000` 超出 16 位寄存器范围的编译错误
- 构建：修复 build.bat CRLF 换行符丢失导致脚本逐行解释执行失败
- 构建：修复 build.bat 多余参数检测逻辑，黄色 `[Warn]` 提示具体参数
- 构建：修复 build.bat 错误输出未着色问题，全部改为红色逐行输出

**变更**
- 目录：`src` 中制作了更详细的分类
- 目录: `Build` 在添加了 `Binary` 文件夹
- 文档：重写 `docs/手册.md` 为完整的手册（约 710 行，涵盖所有内核模块）
- 文档：重写 `docs/HarLin_Api.md` API 规范文档（25 节系统调用规范）
- 文档：Logo 插入到 `README.md` 顶部

---

## v1.6 - 通用设施更新

**新增**
- 引导：El Torito CD-ROM 启动支持，替代传统 1.44MB floppy 启动
- 引导：分段引导架构：boot.asm（512 字节引导）+ stage2.asm（扩展引导加载器）
- 引导：INT 0x15 E820 内存探测，支持 64GB+ 物理内存
- ACPI：SRAT/SLIT 表解析，支持 NUMA 节点拓扑发现
- PMM：物理内存管理器重构，移除硬编码内存限制，支持 64GB 以上内存
- 构建：同时生成 HarLin.iso（CD-ROM 镜像）和 HarLin.img（1.44MB 磁盘镜像）

---

## v1.5.4

**新增**
- FAT32：可重入锁，新增 `fat32_lock` 自旋锁与深度计数器 `fat32_lock_depth`
- 调度器：FS 段基址保存恢复，`struct process` 新增 `fs_base` 字段
- 自旋锁：中断保存/恢复接口 `spinlock_acquire_irqsave`、`spinlock_release_irqrestore`、`spinlock_try_acquire_irqsave`、`spinlock_release_from_try`
- 进程：用户页数组从 16 扩展至 64
- 图形：VGA 13h 模式字模扩展至 256 字符（Latin-1 扩展字符集）

**修复**
- VMM：修复 `vmm_get_phys` 拆分为持锁版本 `vmm_get_phys_locked` 和无锁版本，修复直接将物理地址当作虚拟地址解引用的 bug
- VMM：修复用户态拷贝操作 `copy_from_user` / `copy_to_user` / `strncpy_from_user` 添加 `vmm_lock` 保护，多核环境下防止临时映射窗口竞争
- FAT32：修复 `find_entry_in_dir` 等访问 `sector_buf` / `cluster_buf` 的函数均加锁保护；修复 `Harlin_Mkdir` 重复写入 0x1A 偏移的父目录簇号错误
- 汇编：修复 `gdt_flush.asm` 中 `jump_to_user` 函数未正确传递第三个参数到 `rdi` 的问题
- SMP：修复 AP 启动栈从 2 页扩展至 3 页（`pmm_alloc_contiguous(3)`，`stack_top = phys + 0x3000`）；修复未设置 TSS RSP0 的隐患
- ATA：修复 `ata_irq_handler` 添加 `ata_irq_lock` 保护，使用 IRQ-save 自旋锁防止中断嵌套
- 网络：修复新增 `tcp_table_lock` 保护 TCP 连接表分配/释放/发送/接收；`net_irq_handler` 加锁
- USB：修复 UHCI 控制传输 `qh_virt` 映射失败时正确释放 qh 物理页与其他已分配资源
- CHC：修复加载器 / 用户态内存分配数组越界
- 引导：修复 boot.asm 磁盘读取单扇区失败最多重试 3 次，重置磁盘后重试；跨磁道 CH 递增读覆盖更大内核镜像；修复因 `setup_vesa` 占用空间导致超过 510 字节的编译错误
- 音频：修复 speaker.asm 16 位模式下 word 溢出警告，循环计数器拆为 3 段
- PMM：修复 `pmm_alloc` / `pmm_free` / `pmm_alloc_contiguous` 等位图操作添加 `pmm_lock` 保护
- 系统调用：修复 `syscall.c` 与 `chc_loader.c` 同步扩容上限至 64

**移除**
- 引导：移除 boot.asm 中未使用的 VESA 初始化代码

---

## v1.5.3

**新增**
- 构建：Bochs 调试支持（`bochsrc.txt`、`run-bochs.bat`）
- 图形：VGA 13h Shell GLI（`shell.c`）

**变更**
- 构建：内核格式从 PE 改为 ELF 标准
- 构建：集成 LLVM 工具链（clang + ld.lld）
- 构建：链接脚本调整为 ELF PHDRS 结构
- 构建：软盘镜像填充至完整 1.44MB

**修复**
- 引导：修复 boot.asm 栈错误与磁盘读取逻辑

**保留**
- 系统调用：HarLinAPI 系统调用接口

**移除**
- 工具：CHC 自定义格式及 HCC 编译器

---

## v1.5.2

**新增**
- ACPI：电源管理，RSDP 扫描、FADT 解析、`acpi_power_off()` 硬件关机、`acpi_reboot()` 系统重启
- 网络：协议栈增强，TCP 多连接支持（连接表管理，`tcp_connect_remote`/`tcp_send`/`tcp_recv`/`tcp_close_conn`）
- 网络：DHCP 客户端，支持四步握手（Discover→Offer→Request→Ack），自动获取 IP/网关/DNS；`network_init()` 默认启用 DHCP，失败则回退静态配置
- 系统调用：动态链接器集成，`HARLIN_SYS_DLOPEN(40)`、`HARLIN_SYS_DLSYM(41)`、`HARLIN_SYS_DLCLOSE(42)`
- PMM：连续物理页分配 `pmm_alloc_contiguous`

**修复**
- 编译：修复 `display.h` 缺少 `#include "harlin_API.h"`；`ipc.h` 缺少 `ipc_init()` 声明
- USB：修复 UHCI 驱动 SETUP 包未复制到 TD 缓冲区导致控制器发送垃圾数据；DMA 缓冲区使用 `pmm_alloc` 可能分配高于 4GB 物理地址被截断，改用 `pmm_alloc_contiguous_low`
- 网络：修复 DHCP 发送期间 `local_ip` 被清零导致 ARP 响应污染 ARP 表，添加 `dhcp_in_progress` 标志屏蔽 ARP 响应；修复 `dhcp_build_common` 被调用两次的冗余问题
- 内存：修复 `sys_mmap`/`sys_munmap` 预检与映射间竞态条件（SMP 多线程），添加自旋锁保护
- 网络：修复 TCP 连接复用后 `remote_mac` 残留旧数据，`tcp_alloc_conn` 改为清零整个结构体
- ACPI：修复 RSDP 扫描缺失 EBDA 区域，导致部分 BIOS 无法发现 RSDP
- 系统调用：修复 `sys_kfree` 仅校验魔数未校验 `used` 字段，增强安全检查
- 内核：修复 `Harlin_Shutdown` 中 `acpi_power_off` 失败无提示输出

---

## v1.5.1

**新增**
- 音频：Sound Blaster 16 驱动，支持 DMA 方式播放 16-bit PCM
- 音频：WAV 文件解析器
- 音频：播放 API `Harlin_AudioInit`、`Harlin_AudioPlayWav`、`Harlin_AudioStop`、`Harlin_AudioIsPlaying`
- FAT32：文件删除支持 `Harlin_DeleteFile`

**变更**
- 用户态：应用目录重构，`HarLin_App/` 存放系统应用，`harlin.c` 为默认启动程序，`shell/` 为独立 Shell CHC 应用
- 构建：Shell 从内核源码移除外置为独立 CHC 应用，命令：`help`、`say`、`end`、`exit`、`run`、`exec`、`pid`、`beep`、`sleep`、`time`、`clearkeys`
- 工具：HCC 编译器工具链预编译二进制（`bin/HCC.exe`、`bin/bin2h.exe`）
- 构建：`User_CHC/` 目录存放预编译 CHC 程序（`harlin.chc`、`shell.chc`）
- 构建：`build.bat` 更新为编译 CHC 应用的构建流程
- 文档：更新 `README.md`、`手册.md`、`HarLin_API_Spec.md`
- 构建：`.gitignore` 重写为符合项目需求

**修复**
- 系统调用：修复使用陷阱门（0xEF）后，部分系统调用返回值传递错误
- SMP：修复 AP 启动时每个 CPU 使用独立 GDT/TSS，但 TSS 初始化遗漏 RSP0 设置
- 内存：修复 `pipe.c`、`kmalloc.c`、`network.c` 中物理地址直接当作虚拟地址使用的问题
- PMM：修复位图未标记内核与位图自身占用内存的问题
- VMM：修复 `vmm_map` 对用户态虚拟地址范围缺少校验
- 系统调用：修复 `syscall.c` 中 `sys_kmalloc`/`sys_kfree` 参数校验缺失
- FAT32：修复目录查找遇到空条目提前返回导致漏找文件

**移除**
- 内核：移除内置 `shell.c`/`shell.h`
- 系统调用：移除 `harlin_API.h` 中废弃声明
- 系统调用：从公开 API 中移除 `Harlin_IntOn`/`Harlin_IntOff`，避免用户态调用

---

## v1.5 - 多核更新

**新增**
- SMP：多核 SMP 启动，Local APIC 初始化、IPI 发送、AP 引导 trampoline
- 自旋锁：自旋锁与原子操作 `xchg`/`add`/`sub`/`cmpxchg`
- SMP：per-cpu 变量支持
- 调度器：增强，优先级调度、时间片轮转、就绪/睡眠队列
- 内存：动态内存分配器 `kmalloc`/`kfree`/`krealloc`/`ksize`
- 系统调用：RTC 时钟读取 API
- 键盘：扩展功能，LED 控制、修饰键状态、缓冲区清空、扫描码集切换
- 音频：Sound Blaster 16 驱动，支持 WAV 播放
- 系统调用：新增 `getpid`、`getcpu`、`time`、`beep`、`kmalloc`、`kfree`、`mmap`、`unmap`、`getkeystate`、`keyled`、`setpriority`

---

## v1.4 - 工具更新

**新增**
- 工具：HCC 编译器
- 工具：`bin2h`，用于将二进制文件转换为 C 头文件数组
- 构建：`test/` 文件夹
- 文档：`docs/HCC.md`，说明 HCC 命令用法与示例

**变更**
- 构建：项目根目录由 `HarLIn_Boot` 重命名为 `Kernel`
- 构建：优化源码目录结构 `src/ASM` → `src/asm`、`src/Sys_C` → `src/harlin`、`tools` → `hcc`
- 构建：新增 `bin/` 目录存放可执行工具

---

## v1.3.2

**变更**
- 系统调用：简化 HarLin_API 命名（如 `Harlin_ConPrint` → `Harlin_Print`、`Harlin_FsOpen` → `Harlin_Open`）
- 工具：将 CX 用户态可执行格式升级为 CHC，魔数改为 `HARLINCHC`
- CHC：增强加载器健壮性，严格校验头部边界、段大小、重定位对齐，失败时自动回滚已映射页面
- 文档：更新 API 规范和 CHC 格式文档

**修复**
- 内核：修复启动崩溃 bug
- CHC：修复运行 CHC 程序崩溃的问题
- FAT32：修复 `Harlin_FsCreate` 不检查文件已存在的问题，避免同名目录项冲突
- IPC：修复 `sys_pipe_create` 返回值设计缺陷，改为返回管道 ID
- 系统调用：修复 `harlin_API.c` 中 `Harlin_FsRead`/`FsWrite`/`FsSize`/`FsClose` 重复定义导致的无限递归
- 图形：修复 VESA LFB 映射与内核低地址冲突，统一映射到 `0xFFFF800000000000`
- FAT32：修复在 `Harlin_File` 中记录目录项位置，写入后自动更新文件大小

---

## v1.3.1

**修复**
- 中断：修复 `idt_load` 汇编函数错误地从栈读取参数，改为从 RDI 寄存器读取
- 中断：修复因 IDT 未正确加载导致的定时器中断 Triple Fault 崩溃

**变更**
- 引导：Bootloader 精简启动输出，保持纯黑屏启动

---

## v1.3 - 图形与丰富更新

**新增**
- 图形：VGA 13H 图形模式（320×200，256 色），支持 `set_mode`/`clear`/`put_pixel`
- 图形：VESA 800×600×24 图形模式，Bootloader 通过标准 VBE INT 10h 设置模式并保存模式信息
- 网络：HTTP Chunked 传输编码流式解码状态机，支持任意大小响应
- 网络：HTTP 请求升级到 HTTP/1.1
- FAT32：文件写入支持（`Harlin_FsWrite`）
- FAT32：空闲簇查找、簇分配、簇链扩展与 FAT 表双副本同步写入
- FAT32：文件创建（`Harlin_FsCreate`），支持 8.3 短文件名
- IPC：进程间通信管道（Pipe）内核对象
- 系统调用：新增 `PIPE_CREATE`/`PIPE_READ`/`PIPE_WRITE`/`PIPE_CLOSE`/`PIPE_READY`
- 系统调用：新增用户态 API `Harlin_PipeCreate`/`Read`/`Write`/`Ready`/`Close`

---

## v1.2.1

**新增**
- 系统调用：`HARLIN_SYS_KEYOVERFLOW`，暴露键盘溢出计数
- 系统调用：统一字符串比较函数，删除重复的 `string.c`/`string.h`，全面使用 `Harlin_StrCmp`

**修复**
- 中断：修复 RTC 中断风暴导致的系统崩溃，禁用 CMOS 周期性中断与 Local APIC
- 中断：修复 PIC 初始化时序，添加 ICW 命令间延迟
- ATA：修复 IRQ 竞态与无限循环卡死，删除写操作后错误的 0xE7 命令
- 键盘：修复驱动中断状态保存错误（`cli`/`sti` 替换为 `pushf`/`popf`）
- 网络：修复驱动全局静态缓冲区重入问题，改为局部变量
- 网络：修复网卡 DMA 传递虚拟地址问题，改为分配物理内存发送
- FAT32：修复 `cluster_buf` 硬编码大小导致的溢出风险
- FAT32：修复 `sectors_per_cluster` 上限检查，拒绝异常分区
- CHC：修复 `cx_loader` `map_user_pages` 失败时的内存泄漏，添加回滚逻辑
- 调度器：修复 `timer_handler` 的 frame 索引与中断栈帧布局不匹配
- 系统调用：修复 `sys_exec` 调用 `schedule` 后无法返回的问题
- 调度器：修复 `schedule` 只调度一次的缺陷，支持重复抢占切换
- 调度器：修复竞态条件，使用 `pushf`/`popf` 保存中断状态
- GDT：修复用户代码/数据段 32 位描述符错误，修正为 64 位
- VMM：修复大页映射不兼容用户 4KB 页，实现 2MB 大页拆分
- VMM：修复空指针解引用风险，处理分配失败回滚
- 系统调用：修复 `sys_alloc` 返回物理地址的安全漏洞，改为返回用户虚拟地址
- 系统调用：修复 `sys_free` 可释放任意物理页的漏洞，限制只释放进程拥有的页
- 系统调用：修复 `sys_print`/`sys_open`/`sys_read` 等未验证用户指针的问题
- 系统调用：修复 `sys_alloc` 从 `0x500000` 开始扫描避免覆盖代码段
- 系统调用：修复 `sys_alloc` `next_free_virt` 跨进程共享的碎片问题，改为每个进程独立
- 进程：修复 `process_exit` 页面泄漏，添加 `vmm_unmap` 并记录虚拟地址
- 进程：修复 `process_exit` 死循环问题，标记为 `noreturn` 并使用 `__builtin_unreachable`
- 调度器：修复无就绪进程时直接返回导致的卡死，改为 `sti; hlt; cli` 等待
- 系统调用：修复 `sys_yield` 空函数问题，调用 `schedule` 真正让出 CPU
- 网络：修复 DNS 解析越界读取与无限循环风险
- 键盘：修复 Shift 键状态错误，使用 `shift_count` 计数器支持双 Shift
- 键盘：修复缓冲区满时静默丢弃按键，添加溢出计数器
- 图形：修复 `Harlin_ConSetColor` 错误实现，正确遍历 VGA 属性字节
- 图形：修复 `Harlin_ConPrint` 在真实硬件上可能出错的 QEMU 0xE9 调试端口输出
- 系统调用：修复 `syscall_stub` 缺少 `noreturn` 标记
- 链接：修复 `__chkstk_ms` 未定义
- 编译：修复 `harlin_API.c` 中 `outb`/`inb` 函数未声明的编译错误
- 启动：修复网络初始化顺序错误，确保 PMM/VMM 初始化后再初始化网络
- 构建：修复 QEMU 命令行 `-no-hpet` 参数错误
- 启动：修复文件系统初始化顺序，先 `DiskInit`/`PartitionInit` 再挂载 FAT32 分区
- 图形：修复内核启动后显示模式被 Bootloader VBE 改变，强制回到 VGA 文本模式
- 网络：修复初始化 ARP 查询超时过长导致启动卡住

**变更**
- 图形：VESA 模式设置失败时自动回退到 VGA 文本模式
- 引导：移除 Bootloader 默认 VBE 调用，默认以 VGA 文本模式启动
- 启动：内核改为纯黑屏启动
- 网络：默认启动不初始化网络模块，保持内核地基稳定启动
- 文档：重写 `docs/手册.md` 为小白上手版本
- 文档：在 `docs/手册.md` 中新增网络模块启用说明

---

## v1.2 - 用户更新

**新增**
- GDT：64 位 GDT/TSS 与用户态 ring3 切换支持
- 系统调用：`int 0x80` 系统调用入口与基础系统调用表
- CHC：HarLin .cx 可执行文件加载器
- 调度器：基础进程调度框架

---

## v1.1 - 转移更新

**新增**
- GDT：64 位 GDT、IDT 与中断处理程序
- 系统调用：统一 API 层 `harlin_API.h`/`harlin_API.c`
- PMM：物理内存管理器
- VMM：虚拟内存管理器
- ATA：PIO LBA28 磁盘 I/O 驱动
- FAT32：MBR 分区表解析
- FAT32：只读文件系统支持（短文件名）

---

## v1.0.1

**修复**
- 内存：修复变量名越界
- 编译：修复 const 修饰导致的未定义行为
- 内存：修复整数溢出
- 内存：修复缓冲区溢出
- 网络：修复字节序错误
- 编译：修复未初始化变量

**变更**
- 构建：更新构建输出

---

## v1.0 - 初始更新

- 项目：初始版本发布
- 引导：MBR 引导扇区
- 内核：x86 32 位保护模式内核
- 图形：VGA 文本模式与 VESA 图形模式支持
- 键盘：中断与轮询输入
- 用户态：内置 Shell
- 网络：RTL8139 网卡基础支持
- 音频：蜂鸣器驱动
- 工具：基础字符串与屏幕输出工具

## v0.9 -beta
- 构建：创建了 Gitee 仓库
- 内核：优化了代码
- 文档：新增 手册.md

## v0.8 -beta
- 项目：项目上线准备中
- 文档：优化了文档

## v0.7 -beta
- 用户态：增加了 shell

## v0.6 -beta
- 编译：修复了定义问题

## v0.5 -beta
- 内核：功能持续开发中

## v0.4 -beta
- 引导：修复了 boot.asm 跳转失败的问题

## v0.3 -beta
- 构建：添加了 QEMU 硬件调试

## v0.2 -beta
- 内核：功能持续开发中

## v0.1 -beta
- 构建：创建了项目基础目录

## v0 -beta
- 项目：项目正在规划中
