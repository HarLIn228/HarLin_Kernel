# HarLin Kernel version.md

## v1.8 - 突破更新

**新增**
- 路径解析：新增 `Harlin_PathParse` / `Harlin_PathNormalize` 接口（`src/head/fs/path_walk.h` + `src/harlin/fs/path_walk.c`），按 `/` 拆分路径段为 `struct path_parts`，规范化处理 `.` 与 `..` 路径段，支持绝对路径与相对路径
- 内存虚拟文件系统：新增 mem_fs 节点池（`src/head/fs/mem_fs.h` + `src/harlin/fs/mem_fs.c`），提供 `Harlin_MemFsInit` / `Harlin_MemFsMkdir` / `Harlin_MemFsRmdir` / `Harlin_MemFsCreate` / `Harlin_MemFsRemove` / `Harlin_MemFsWrite` / `Harlin_MemFsRead` / `Harlin_MemFsLs` / `Harlin_MemFsLookupNode` 完整目录树增删查能力
- 文件权限位：新增 9 位 rwx 权限位（owner/group/other × read/write/execute）与 `Harlin_PermCheck` 检查函数（`src/head/fs/perm.h` + `src/harlin/fs/perm.c`），按 uid/gid 匹配 owner/group/other 权限位
- COW 文件系统：新增 `Harlin_CowFsInit` / `Harlin_CowSnapshot` / `Harlin_CowWrite` / `Harlin_CowReadAt` / `Harlin_CowFork`（`src/head/fs/cow_fs.h` + `src/harlin/fs/cow_fs.c`），基于 mem_fs 文件节点按版本号（`v000` ~ `v999`）维护多版本快照，条件写检查 `expected_ver` 解决写写冲突，跨文件节点版本全量克隆
- 进程：新增 `Harlin_Fork(parent_pid, out_child_pid)` 与 `Harlin_ForkTest`（`src/harlin/proc/scheduler.c`），按父进程描述符克隆 rip/rsp/页表/stack/handles/优先级/fair_key 字段
- 进程：新增 `Harlin_ExecFromBuffer(elf, size)` / `Harlin_ExecFromPath(path)` / `Harlin_ExecTest`（`src/harlin/proc/exec.c`），封装 `process_create_elf` 提供 elf 缓冲区与路径两种 exec 入口
- 进程：新增 wait 子进程退出表（`src/head/proc/wait.h` + `src/harlin/proc/wait.c`），`Harlin_NotifyExit` / `Harlin_Wait` / `Harlin_WaitAny` 三个 API + `Harlin_WaitTest`
- /proc 虚拟文件系统：新增 `Harlin_ProcFsInit` / `Harlin_ProcFsRead` / `Harlin_ProcFsLs` / `Harlin_ProcFsTest`（`src/head/fs/proc_fs.h` + `src/harlin/fs/proc_fs.c`），动态生成 `/proc/uptime` / `/proc/meminfo` / `/proc/cpuinfo` / `/proc/loadavg` 文本
- 启动流程：`Harlin_Boot` 自测段新增 `Harlin_ForkTest` / `Harlin_ExecTest` / `Harlin_WaitTest` / `Harlin_ProcFsTest` 调用（`src/harlin/syscall/harlin_API.c`）

**变更**
- 命名：4.x 阶段所有公共 API 改为口语化命名 + `Harlin_` 前缀——`slab` 系列改为 `block_pool`（`slab` → `block_pool`、`slab_alloc` → `block_pool_alloc`、`slab_free` → `block_pool_free`、`slab.c` → `block_pool.c`），chunk 替代 slab 中的 object 语义术语，`canary` 改为 `guard`
- 命名：`page_cache` 整组改为 `read_cache`（`Harlin_ReadCacheInit` / `Harlin_ReadCacheGet` / `Harlin_ReadCachePut` / `Harlin_ReadCacheTest`）
- 命名：`signal` 整组改为 `event` / `notify`（`Harlin_NotifyInit` / `Harlin_NotifySend` / `Harlin_NotifyWait` / `Harlin_NotifyTest`）
- 命名：CFS 调度自测 `cfs_selftest` 改为 `Harlin_FairPickTest`
- COW：COW 文件系统内部 16 槽 `struct cow_version`（used / file_node / ver 字符串 / payload）改为按 `file_node` 分组的版本池；版本号生成走 `ver_counter` 全局计数 + `make_ver_name` 拼出 `v000`~`v999`
- 自测：阶段五全部自测（`Harlin_PathTest` / `Harlin_MemFsTest` / `Harlin_PermTest` / `Harlin_CowTest` / `Harlin_ForkTest` / `Harlin_ExecTest` / `Harlin_WaitTest` / `Harlin_ProcFsTest`）按顺序串联在 `Harlin_Boot` 早期阶段
- C++重写了`Build.exe`,`Build.bat`回归

**修复**
- 中断：缺页处理函数 `page_fault_handler` 改名为 `Harlin_PageFaultHandler`（声明与中断分发同步更新），消除与 `Harlin_` 公共 API 命名规范的脱节
- mem_fs：节点结构补齐 `mode` / `owner` / `group` 权限位字段，确保 `Harlin_PermCheck` 输入端字段齐全
- COW：修复 `Harlin_CowSnapshot` 直接返回空版本号导致 `Harlin_CowWrite` 找不到 base 版本的连锁错误，改为快照时分配 `ver_counter++` 唯一版本字符串
- COW：修复 `cow_fs.c` 直接调用 `mem_fs.c` 的 `static resolve_path` 链接失败，在 `mem_fs.c` 暴露 `Harlin_MemFsLookupNode` 公共 API
- 头：修复 `cow_fs.h` 引用 `MEM_FS_PAYLOAD` 宏未 include `mem_fs.h` 的间接依赖错误
- 函数签名：阶段五自测入口全部统一为 `void Test(void)` 返回值（`Harlin_ForkTest` / `Harlin_ExecTest` / `Harlin_WaitTest` / `Harlin_ProcFsTest`），与 `Harlin_Boot` 调用端类型一致
- 编译：修复 `process_create_elf` 链路 scheduler.c 未 include `harlin_API.h` 导致的 prototype 推断冲突
- 编译：修复 `Harlin_ForkTest` 残留 `return 0;` 与 `void` 返回类型不匹配

---

## v1.7.4

**新增**
- 引导：UEFI 引导应用 `loader.efi`（`src/uefi/efi_types.h` + `src/uefi/efi_main.c`），在 PE32+ 入口 `efi_main` 中通过 `EFI_BOOT_SERVICES` 定位 `EFI_SIMPLE_FILE_SYSTEM_PROTOCOL` 与 `EFI_LOADED_IMAGE_PROTOCOL`，读取启动盘根目录下的 `kernel.bin` 并跳转到内核入口
- 引导：UEFI 阶段获取内存映射（`EFI_BOOT_SERVICES.GetMemoryMap`），退出引导服务（`ExitBootServices`）后再交接给内核，并把内存描述符基址/大小写入 `EFI_BOOT_INFO` 由内核侧消费
- 引导：UEFI 阶段获取 `ACPI 2.0` 表 RSDP 并写入 `EFI_BOOT_INFO.acpi_rsdp`，供内核 ACPI 解析复用
- 中断：新增 `isr14_stub` 缺页异常处理桩（`src/asm/core/interrupt.asm`），保存通用寄存器、读取 CR2、通过 `page_fault_handler(fault_addr, error_code)` 分发
- 内存：新增 `page_fault_handler` / `page_fault_demand_mapping_install` / `page_fault_cow_resolve`（`src/head/mem/page_fault.h` + `src/harlin/mem/page_fault.c`），按需分页与 COW 写时复制实现
- 内存：按需分页在 `!PF_PRESENT` 首次访问未映射页时 `pmm_alloc` + 清零 + `vmm_map` + `invlpg`
- 内存：COW 在 `PF_PRESENT & PF_WRITE` 命中只读共享页时分配新页、复制 4 KiB 内容、`vmm_unmap` + `vmm_map` 可写页 + `invlpg`
- 存储：AHCI 驱动替换原 ATA 驱动（`src/harlin/drv/ahci.c`），按 HBA 端口映射、命令列表/FIS 结构体构建、PRDT 物理区域描述符构造、命令提交与状态轮询完整流程
- 存储：AHCI 内部使用本地 `ahci_memset` / `ahci_memcpy`（freestanding 环境下标准库不可用），并通过宏替换 `memset` / `memcpy` 避免链接缺失
- 构建：构建脚本新增 CLI 模式 `python tools\Build.py -cli`（或 `build.exe -cli`），直接以命令行方式运行完整构建并输出到控制台，成功退出 0、失败退出 1

**变更**
- IDT：`isr_stubs[14]` 由通用 `isr14` 切到缺页专用 `isr14_stub`，沿用 `GDT_KERNEL_CODE` 0x8E 中断门
- Build：`run_command` 同时支持 list 形参（避免 shell 重解析）与 str 形参；xorriso 调用改为 list 模式，并以 `cwd=iso_root` + 相对 `boot.img` 路径解决 mkisofs 路径解析
- Build：`root_dir` 判定从 `os.path.dirname(...)` 改为 `os.path.dirname(os.path.dirname(...))`，dev 模式（`tools/Build.py`）与 frozen 模式（`tools/build.exe`）都能正确指向项目根
- 启动：ISO 镜像内同时含 BIOS 阶段（`boot.img`）与 UEFI 阶段（`EFI/BOOT/BOOTX64.EFI`），并通过 `-eltorito-alt-boot` 走 UEFI eltorito 入口
- 内核：启动魔数检测加入 UEFI 引导魔数（`EFI_BOOT_INFO` 标记），与 BIOS 启动路径共享同一 `kernel_main` 入口

**修复**
- 中断：修复 `isr14_stub` 在 `iretq` 前 `add rsp, 16` 多跳一格导致返回用户态崩溃，改为 `add rsp, 8` 仅跳过 CPU 压入的 error_code
- UEFI：修复 `efi_types.h` 缺少 `UINTN` 类型导致 `LocateProtocol` 编译失败，新增 `typedef unsigned long UINTN`
- UEFI：修复 `EFI_BOOT_SERVICES` 结构体字段偏移计算错误导致 `LocateProtocol` 等调用走飞指针，改为按函数指针偏移直接定位调用
- 构建：修复 `Build.exe` 用 `--noconsole` 打包导致编译信息无法在控制台显示，`log()` 直接 `print` 到 stdout；后续 `--onefile --console` 重新打包后 CLI 模式可见
- 构建：修复 `xorriso` shell 模式下 `-eltorito-alt-boot` 被解析为 ISO 路径导致 abort，改用 list 形参 + `cwd=iso_root` 显式工作目录

---

## v1.7.3

**新增**
- 构建：GUI 编译工具（`Build.exe`），含参数控制器（12 个编译选项）和工具控制器（6 个工具路径配置，支持 Path/本地路径模式）
- 工具控制器：支持组装器、C 编译器、链接器、objcopy、truncate、xorriso 路径配置；Path 模式自动用 `where.exe` 检测，找不到则切回本地路径
- 参数控制器：调试信息、优化等级、LTO、GC 节区、错误信息显示、ISO/磁盘映像生成等开关
- 网络：新增 `icmp_ping(dest_ip, ident, seq, timeout, payload_size, out)` 接口（`src/head/net/network.h` + `src/harlin/net/network.c`），按 RFC 792 构造 ICMPv4 echo request（type=8）并通过 `ip_send(proto=IP_PROTO_ICMP)` 发送
- 网络：新增 `struct icmp_ping_result`（`received / rtt_ms / reply_id / reply_seq / reply_ttl / reply_ip`），回复后回填耗时与 TTL 等信息
- 网络：新增本地 `rtl_get_ticks()`，读取 PIT 通道 0 计数器，转换为单调递增 tick（1.193 MHz），作为 RTT 计量
- 网络：新增 `icmp_echo_checksum()`，按标准 ones-complement 算法对 ICMP 头+payload 计算校验和
- 网络：常量 `IP_PROTO_ICMP = 1`、`ICMP_ECHO_REQUEST = 8`、`ICMP_ECHO_REPLY = 0`、`ICMP_HEADER_SIZE = 8`
- API：新增 `Harlin_IcmpPing` 公开宏别名

**变更**
- 目录：`src/asm/` 子目录合并，audio/speaker.asm → drv/speaker.asm；gdt/intr/smp/sync/util 合并为 core/
- 目录：`src/harlin/` 移除 acpi/、sec/、shell/ 顶层目录，boot/ 新增（cr4_features、feature_probe）；shell/ → proc/shell、elf/ → proc/elf
- 目录：`docs/` 新增默认硬件指南、第三方驱动接入教程
- 构建：构建脚本由 `build.bat` 迁移为 Python GUI 程序 `Build.py`，支持可视化参数配置
- ping 走现有 `arp_resolve` + `ip_send` 路径，仅在 `gateway_resolved` 之后再行包发送；目标不可达返回 `-1`，超时返回 `-2`
- 接收侧单次会话轮询 `net_wait_packet`，按 ETHERTYPE_IP + IP_PROTO_ICMP + ICMP_ECHO_REPLY + 源 IP 严格匹配，过滤其他 ICMP 流量
- 网络：`net_wait_packet` 忙等循环插入 `pause` 指令，降低自旋功耗
- 进程：`process_create_elf` 解除 ELF 加载期间对 `scheduler_lock` 的长持锁，slot 预占为 `PROC_STATE_BLOCKED` 后释放锁调 `elf_load_exec`，完成后再获取锁校验 slot 仍有效再置 `PROC_STATE_READY`
- 进程：移除 `processes[slot].page_count = ctx.mapped_pages + ELF_USER_STACK_PAGES` 的二次覆盖，改为复用 `elf_alloc_user_page_cb` 与用户栈循环累加的 `page_count`，仅做 `> 64` 上限钳制

**修复**
- 网络：修复 `net_wait_packet` 100000 次空轮询无 `pause`，降低自旋耗电

---

## v1.7.2

**新增**
- PCI：新增 `pci_init_with_ecam(ecam_base, bus_start, bus_end)` 入口（`src/head/drv/pci.h` + `src/harlin/drv/pci.c`），把 ECAM 基址和总线范围挂到 PCI 子系统
- PCI：新增 `pci_ecam_available` / `pci_ecam_base` / `pci_ecam_bus_start` / `pci_ecam_bus_end` 查询接口
- PCI：新增 `pci_read_ext(bus, dev, func, off)` / `pci_write_ext(...)`，按 PCI Express 4 KB/设备 扩展配置空间访问规范实现 MMIO 读写（地址 = base + bus*1MB + dev*32KB + func*4KB + off）
- PCI：原 `pci_read` / `pci_write` 自动检测目标总线是否在 ECAM 范围，命中走 MMIO、不命中回退到 I/O 端口 `0xCF8/0xCFC`
- API：新增 `Harlin_PciInitWithEcam` / `Harlin_PciEcamAvailable` / `Harlin_PciEcamBase` / `Harlin_PciEcamBusStart` / `Harlin_PciEcamBusEnd` / `Harlin_PciReadExt` / `Harlin_PciWriteExt` 公开宏别名

**变更**
- `pci_init_with_ecam(0, 0, 0)` 行为等价于旧 `pci_init`，不会启用 ECAM
- 旧 I/O 路径在 ECAM 关闭时仍为唯一访问手段，已是生产分支行为
- ECAM 地址空间按调用方传入的 `ecam_base` 直接解引用，未做虚拟映射；调用方须保证 ECAM 物理页已在分页中建立

**修复**
- 网络：修复 `icmp_ping` 在 `arp_resolve` 之前计算 RTT，导致 RTT 包含 ARP 解析等待时间的错误
- 网络：修复 `icmp_ping` 毫秒换算用 `/ 1000` 偏差过大（PIT 1.193 MHz），改用 `rtl_pit_to_ms(ticks) = ticks / 1193`
- 网络：修复 `rtl_get_ticks` 首次 `raw > last_ticks` 在初始化阶段误增 `wrap` 计数；新增 `initialized` 标志，首轮直接返回 0
- 网络：修复 `icmp_ping` 超时按 `tries` 计数硬凑 `timeout * 1000` 实际等待时间远超预期，改为 PIT 计数 `deadline_ticks = now + timeout * 1193`
- 网络：清理 `icmp_ping` 中 `elapsed` 死代码

---

## v1.7.1

**新增**
- PMM：新增 `pmm_init_from_e820(top_usable_addr)` 入口（`src/head/mem/pmm.h` + `src/harlin/mem/pmm.c`），按 `top_usable_addr - PMM_BASE_ADDR` 推算 `total_pages`
- PMM：新增 `pmm_total_pages` / `pmm_total_bytes` / `pmm_top_addr` 状态查询
- PMM：`PMM_MAX_PAGES` 上限设为 `0x100000`（4 GiB / 4 KiB）；下界 `PMM_TOTAL_PAGES_MIN = 32000` 保持旧行为兼容
- PMM：bitmap 范围 `[PMM_BITMAP_ADDR, PMM_BITMAP_ADDR + bitmap_bytes)`，布局校验保证不撞 `PMM_BASE_ADDR`
- API：新增 `Harlin_PmmInitFromE820` / `Harlin_PmmTotalPages` / `Harlin_PmmTotalBytes` / `Harlin_PmmTopAddr` 公开宏别名

**变更**
- 移除旧版 `PMM_TOTAL_PAGES` 宏对所有分配函数的硬编码依赖，所有路径改用 `g_pmm_total_pages` 全局
- 旧 `pmm_init` 改为 `pmm_init_from_e820(PMM_BASE_ADDR + PMM_TOTAL_PAGES_MIN * PAGE_SIZE)` 包装，行为向后兼容

**修复**
- ELF：修复 `elf.c` 加载 PT_LOAD 段时未校验 `p_offset` / `p_filesz` 是否在 `size` 范围内，新增 `if (off_in_file > size || segs[i].p_filesz > size - off_in_file) return -1` 越界检查
- 进程：修复 `process_create_elf` 在 `elf_load_exec` 期间使用 `Harlin_Copy` 在不同 PML4 页表间直接拷贝导致崩溃，改为 `Harlin_CopyToUser` 通过用户虚拟地址映射安全传输数据

---

## v1.7 - 现代更新

**新增**
- ELF：新增 `elf_load_exec` / `elf_load_exec_simple` / `Harlin_ElfLoadExec` 接口（`src/head/elf/elf.h` + `src/harlin/elf/elf.c`），回调式完成 PT_LOAD 段解析、地址范围统计、入口点暴露
- ELF：新增 `struct elf_exec_info` 结构体（`entry / phdr_vaddr / phdr_memsz / load_bias / lowest_vaddr / highest_vaddr / is_64 / loaded_segments / total_segments`），为进程加载器提供完整加载元信息
- 进程管理：新增 `process_create_elf(elf_data, elf_size)` 入口（`src/head/proc/scheduler.h` + `src/harlin/proc/scheduler.c`），通过 `vmm_clone_kernel_pml4` 复制内核 PML4 副本、切换后映射 PT_LOAD 段、分配 4 页内核栈 + 4 页用户栈、记录到 `user_vaddrs` / `user_pages` 跟踪表、回切 PML4 后将进程置为 `PROC_STATE_READY`
- 进程管理：进程描述符新增 `pml4_phys` / `kernel_stack_top` / `kernel_stack_base` / `kernel_stack_pages_count` / `kernel_stack_pages_phys[4]` / `rflags` 字段
- 系统调用：重建 `sys_exec`（`src/harlin/syscall/syscall.c`），流程为：用户路径名拷贝 → `Harlin_Open` + `Harlin_Size` + `Harlin_Read` 读入 kmalloc 缓冲区 → `Harlin_ElfCheckMagic` 校验 → `process_create_elf` 加载 → 返回新建 pid

**变更**
- 仅支持 x86_64 ET_EXEC / ET_DYN，不支持静态 PIE 之外的 32 位 ELF（`info.entry < ELF_USER_LOAD_BASE` 直接拒绝）
- 用户地址空间范围 `0x400000 - 0x800000`，用户栈顶固定 `0x7FFFF000`
- 单个 ELF 文件最大 16 MiB，超出直接拒绝
- ELF 头不在已加载 PT_LOAD 段中时，回调 `src_phys == 0`，加载器自行 `pmm_alloc` 并清零
- ELF：回调签名改为显式 `unsigned long long` 形参，避免与项目 `u64` 在不同平台下长度不一致导致的隐式类型不匹配
- ELF：`elf.h` 增加 `Harlin_ElfLoadExec` / `Harlin_ElfCheckMagic` 公开宏别名

---

## v1.6.2

**新增**
- 驱动层：新增 `drv_loader` 驱动注册/加载框架（`src/head/drv/drv_loader.h` + `src/harlin/drv/drv_loader.c`），支持 32 个子系统分类（对齐主流：storage/net/usb/display/audio/input/timer/power/bus/serio/tty/rtc/watchdog/thermal/gpio/crypto/blk/hid/fs/mfd/clk/pwm/net_wireless/sound/acpi/iio/platform/scsi/mtd/virtio/drm）、优先级、`probe/init/remove/suspend/resume` 标准入口与 `provider` 探测；缺位驱动自动回退默认实现
- 硬件能力探测：新增 `feature_probe` 模块（`src/head/boot/feature_probe.h` + `src/harlin/boot/feature_probe.c`），通过 CPUID 探测 SMEP / SMAP / PCID / FSGSBASE / RDRAND / RDSEED / TSC invariant / x2APIC / APIC / NX；探测 HPET 基址；估算 TSC 频率；提供 `feature_rdrand()` 与 `feature_tsc()` 辅助函数
- 安全：新增 `cr4_features` 模块（`src/head/sec/cr4_features.h` + `src/harlin/sec/cr4_features.c`），按 CPUID 结果置位 `CR4.PCID / SMEP / SMAP`；提供 `copy_from_user_safe` / `copy_to_user_safe` 安全拷贝封装
- API：新增 `Harlin_UserPtrValid(addr, len)` 公开用户指针校验入口
- API：新增 `Harlin_HttpsGet(host, path)` HTTPS 入口（v1.6.2 阶段回退到 HTTP，留作第三方 crypto 驱动接入点）
- 启动流程：`Harlin_Boot` 在 SMP 初始化后依次调用 `feature_probe_init` → `cr4_features_enable` → `drv_loader_init` → `drv_load_all`
- 文档：新增 `docs/默认硬件指南.md`（说明 v1.6.2 默认硬件基线、内置驱动集、规划中驱动、CR4 安全默认）
- 文档：新增 `docs/第三方驱动接入教程.md`（含驱动模型、ops 结构、provider 探测器、loopback 虚拟网卡示例、NVMe 驱动骨架、调试技巧、最佳实践、API 速查）

**修复**
- USB：UHCI `uhci_control_transfer` 增加 `data_len ∈ [0, 4096]` 边界检查与 `data` 非空校验，防止描述符/数据阶段内核堆溢出
- 网络：HTTP 客户端 `network_http_get` 移除栈上 4096 字节定长 `rx_buf_data`，改用 `kmalloc` 堆分配，函数出口与所有提前 `return` 统一经 `cleanup` 标签 `kfree`，消除大响应截断与栈溢出风险
- 网络：HTTP 客户端升级协议解析，新增响应状态行解析（`HTTP/<v> <code>`）、`Content-Length` 提取与基于长度的提前终止、新增 `User-Agent: HarLin-Kernel/1.6.2` 与 `Accept: */*` 请求头、状态码返回（2xx/3xx 视为成功）
- 文件系统：FAT32 `write_cluster` 增加 3 次重试 + 写后回读逐字节校验，写入失败时返回 `-1`，避免数据静默损坏
- 中断：PIC `pic_init` 末尾对主片 IMR 寄存器做回读校验，若未稳定在 `0xF8`（屏蔽 IRQ5/6/7）则重写一次，避免启动期 IRQ 屏蔽丢失
- 安全：`user_ptr_valid` 由 `syscall.c` 内的 `static` 提升为全局，便于 `cr4_features` 与其他模块复用，统一用户指针校验入口
- 文档：清理 `docs/手册.md` 中 v1.6.1 已废除的 CHC 章节（13.x `run` 命令、17.3 `CHC 用户程序`、变量/条件/跳转示例段），与当前内置图形 Shell（`help/about/clear/info/beep/halt`）保持一致
- 文档：`docs/HarLin_Api.md` 网络 API 段补充 `Harlin_HttpsGet`，系统调用表 `sys_exec` 标记为「已废除（保留入口返回 -1）」

**变更**
- 构建：`build.bat` CFLAGS 增加 `-I src\head\boot -I src\head\sec`，覆盖新增的 `feature_probe` / `cr4_features` 头文件目录
- 文档：`README.md` 文档列表新增 `docs/默认硬件指南.md` 与 `docs/第三方驱动接入.md` 入口
- 网络：`network_https_get` 暂为 HTTP 回退，签名与 `network_http_get` 一致，留作第三方 crypto 驱动对接钩子
- 驱动框架：`enum drv_subsys` 由 8 扩到 32，参考 Linux 主要子系统但按 HarLin 习惯命名（`DRV_SUBSYS_MAX` 由 8 调为 32）：详见下方「驱动框架重命名」段
- 驱动框架：新增 `drv_subsys_name(enum drv_subsys)` 公开接口（供日志与默认硬件指南使用）
- 驱动框架：扩展 fallback 实现，新增 `drv_fallback_input` / `drv_fallback_audio` / `drv_fallback_rtc` / `drv_fallback_watchdog` / `drv_fallback_bus` 对应缺位回退提示
- 驱动框架：fallback 输出统一改为 `[drv: fallback xxx]` 统一格式（重命名后为 media/link/screen/input/sound/rtc/guard/bridge）
- 文档：`docs/第三方驱动接入.md` 子系统分类表与 API 速查段同步到 32 项
- 头文件：新增 `src/head/stdlib/c.h` 聚合头，专供应用开发者（`#include <c.h>`），汇总 freestanding C 标准子集与 HarLin 常用类型/宏/错误码；内核本身不使用
- ELF 完善：新增 `src/head/elf/elf.h` + `src/harlin/elf/elf.c` 完整解析器，覆盖 ELF32/ELF64、ET_REL/ET_EXEC/ET_DYNAMIC 加载，Program Header 全部类型（含 `PT_GNU_STACK/PT_GNU_EH_FRAME/PT_GNU_RELRO/PT_GNU_PROPERTY`），Section 头表与 `.symtab`/`.dynsym`/`.strtab`/`.rela*` 查找，Dynamic 段（`DT_*` 全部常见项）解析与重定位应用（`R_X86_64_NONE/64/PC32/32/32S/16/8/PC16/PC8/GLOB_DAT/JUMP_SLOT/RELATIVE/GOTPCREL`），所有公开 API 同步 `Harlin_Elf*` 前缀宏别名

---

## v1.6.1

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
