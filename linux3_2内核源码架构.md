# Linux 3.2 内核源码架构组织

## 目录

- [架构设计图](#架构设计图)
- [模块化分层架构](#模块化分层架构)
- [核心子系统详解](#核心子系统详解)
  - [kernel/ - 内核核心](#kernel---内核核心)
  - [mm/ - 内存管理](#mm---内存管理)
  - [fs/ - 文件系统](#fs---文件系统)
  - [net/ - 网络子系统](#net---网络子系统)
  - [drivers/ - 设备驱动](#drivers---设备驱动)
  - [arch/ - 体系结构相关](#arch---体系结构相关)
  - [ipc/ - 进程间通信](#ipc---进程间通信)
  - [init/ - 系统初始化](#init---系统初始化)
  - [block/ - 块设备层](#block---块设备层)
  - [security/ - 安全子系统](#security---安全子系统)
  - [crypto/ - 加密子系统](#crypto---加密子系统)
  - [lib/ - 内核库](#lib---内核库)
  - [include/ - 头文件](#include---头文件)
- [构建与配置系统](#构建与配置系统)
- [架构设计优点](#架构设计优点)

---

## 架构设计图

### 整体架构层次图

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                            用户空间 (User Space)                             │
│                     应用程序、Shell、库函数 (glibc)                           │
├─────────────────────────────────────────────────────────────────────────────┤
│                        系统调用接口 (System Call Interface)                   │
│                              arch/x86/kernel/entry_*.S                       │
├───────────────┬───────────────┬───────────────┬───────────────┬─────────────┤
│   进程管理     │    内存管理    │    文件系统    │   网络子系统   │    IPC      │
│   kernel/     │     mm/       │     fs/       │     net/      │    ipc/     │
│               │               │               │               │             │
│  - 进程调度    │  - 页面分配    │  - VFS 抽象   │  - TCP/IP     │  - 消息队列  │
│  - 进程创建    │  - 内存映射    │  - 具体FS     │  - Socket     │  - 信号量    │
│  - 信号处理    │  - 页面回收    │  - 缓存管理    │  - 协议栈     │  - 共享内存  │
├───────────────┴───────────────┴───────────────┴───────────────┴─────────────┤
│                         虚拟文件系统 VFS (Virtual File System)                │
│                              fs/inode.c, fs/file.c, fs/namei.c               │
├─────────────────────────────────────────────────────────────────────────────┤
│                           通用块层 (Generic Block Layer)                      │
│                                    block/                                    │
├─────────────────────────────────────────────────────────────────────────────┤
│                            设备驱动层 (Device Drivers)                        │
│                                   drivers/                                   │
│     ┌─────────┬─────────┬─────────┬─────────┬─────────┬─────────┐           │
│     │ 字符设备 │ 块设备  │ 网络设备 │   USB   │   PCI   │  GPU等  │           │
│     │  char/  │ block/  │  net/   │  usb/   │  pci/   │  gpu/   │           │
│     └─────────┴─────────┴─────────┴─────────┴─────────┴─────────┘           │
├─────────────────────────────────────────────────────────────────────────────┤
│                        体系结构抽象层 (Architecture Abstraction)              │
│                                    arch/                                     │
│     ┌─────────┬─────────┬─────────┬─────────┬─────────┬─────────┐           │
│     │   x86   │   ARM   │  MIPS   │ PowerPC │  SPARC  │  其他... │           │
│     └─────────┴─────────┴─────────┴─────────┴─────────┴─────────┘           │
├─────────────────────────────────────────────────────────────────────────────┤
│                              硬件层 (Hardware)                               │
│                        CPU、内存、磁盘、网卡、外设等                           │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 内核子系统交互图

```
                                    ┌──────────────┐
                                    │   用户进程    │
                                    └──────┬───────┘
                                           │ 系统调用
                    ┌──────────────────────┼──────────────────────┐
                    │                      ▼                      │
                    │              ┌──────────────┐               │
                    │              │  系统调用层   │               │
                    │              └──────┬───────┘               │
                    │                     │                       │
          ┌─────────┼─────────┬───────────┼───────────┬───────────┤
          ▼         ▼         ▼           ▼           ▼           ▼
    ┌──────────┐ ┌──────┐ ┌──────┐  ┌──────────┐ ┌────────┐ ┌────────┐
    │进程管理   │ │内存  │ │ IPC  │  │   VFS    │ │ 网络   │ │ 安全   │
    │kernel/   │ │mm/   │ │ipc/  │  │   fs/    │ │ net/   │ │security│
    └────┬─────┘ └──┬───┘ └──────┘  └────┬─────┘ └───┬────┘ └────────┘
         │          │                    │           │
         │          │              ┌─────┴─────┐     │
         │          │              ▼           ▼     │
         │          │        ┌──────────┐ ┌────────┐ │
         │          │        │具体文件系统│ │ 块层   │ │
         │          │        │ext4,nfs..│ │ block/ │ │
         │          │        └────┬─────┘ └───┬────┘ │
         │          │             │           │      │
         └──────────┴─────────────┴───────────┴──────┘
                                  │
                           ┌──────┴──────┐
                           ▼             ▼
                    ┌──────────┐   ┌──────────┐
                    │设备驱动层 │   │ 网络驱动  │
                    │ drivers/ │   │drivers/  │
                    │          │   │net/      │
                    └────┬─────┘   └────┬─────┘
                         │              │
                    ┌────┴──────────────┴────┐
                    ▼                        ▼
              ┌──────────┐            ┌──────────┐
              │体系结构层 │            │ 体系结构层│
              │arch/x86  │            │ arch/arm │
              └────┬─────┘            └────┬─────┘
                   │                       │
                   └───────────┬───────────┘
                               ▼
                        ┌──────────┐
                        │   硬件    │
                        └──────────┘
```

---

## 模块化分层架构

### 顶层目录结构

| 目录 | 功能描述 | 代码性质 |
|------|----------|----------|
| `arch/` | 体系结构相关代码 | 平台相关 |
| `block/` | 块设备 I/O 层 | 平台无关 |
| `crypto/` | 加密算法实现 | 平台无关 |
| `Documentation/` | 内核文档 | 文档 |
| `drivers/` | 设备驱动程序 | 平台相关/无关混合 |
| `firmware/` | 设备固件 | 二进制数据 |
| `fs/` | 文件系统 | 平台无关 |
| `include/` | 头文件 | 接口定义 |
| `init/` | 内核初始化 | 平台无关 |
| `ipc/` | 进程间通信 | 平台无关 |
| `kernel/` | 内核核心 | 平台无关 |
| `lib/` | 内核库函数 | 平台无关 |
| `mm/` | 内存管理 | 平台无关 |
| `net/` | 网络协议栈 | 平台无关 |
| `samples/` | 示例代码 | 示例 |
| `scripts/` | 构建脚本 | 构建工具 |
| `security/` | 安全模块 | 平台无关 |
| `sound/` | 音频子系统 | 平台相关/无关混合 |
| `tools/` | 内核工具 | 用户空间工具 |
| `usr/` | 早期用户空间 | initramfs |
| `virt/` | 虚拟化支持 | KVM 等 |

---

## 核心子系统详解

### kernel/ - 内核核心

内核最核心的功能实现，包括进程管理、调度、同步、时间管理等。

#### 进程管理

| 文件 | 功能 |
|------|------|
| `fork.c` | 进程创建 (fork, vfork, clone 系统调用) |
| `exit.c` | 进程退出和资源回收 |
| `exec_domain.c` | 执行域管理 |
| `pid.c` | 进程 ID 管理 |
| `pid_namespace.c` | PID 命名空间 |

#### 进程调度

| 文件 | 功能 |
|------|------|
| `sched.c` | 调度器核心，CFS (完全公平调度器) |
| `sched_fair.c` | CFS 公平调度类实现 |
| `sched_rt.c` | 实时调度类实现 |
| `sched_idletask.c` | 空闲任务调度类 |
| `sched_stoptask.c` | 停止任务调度类 |
| `sched_cpupri.c` | CPU 优先级管理 |
| `sched_autogroup.c` | 自动分组调度 |
| `sched_clock.c` | 调度时钟 |
| `sched_debug.c` | 调度调试信息 |

#### 信号处理

| 文件 | 功能 |
|------|------|
| `signal.c` | 信号发送和处理 |
| `ptrace.c` | 进程跟踪 (调试支持) |

#### 同步机制

| 文件 | 功能 |
|------|------|
| `mutex.c` | 互斥锁实现 |
| `mutex-debug.c` | 互斥锁调试 |
| `semaphore.c` | 信号量实现 |
| `spinlock.c` | 自旋锁实现 |
| `rwsem.c` | 读写信号量 |
| `rtmutex.c` | 实时互斥锁 |
| `futex.c` | 快速用户空间互斥锁 |
| `lockdep.c` | 锁依赖检测 (死锁检测) |

#### 时间管理

| 文件 | 功能 |
|------|------|
| `timer.c` | 内核定时器 |
| `hrtimer.c` | 高精度定时器 |
| `time.c` | 时间相关系统调用 |
| `itimer.c` | 间隔定时器 |
| `posix-timers.c` | POSIX 定时器 |
| `posix-cpu-timers.c` | POSIX CPU 定时器 |

#### 中断与软中断

| 文件 | 功能 |
|------|------|
| `softirq.c` | 软中断处理 |
| `workqueue.c` | 工作队列 |
| `irq_work.c` | 中断工作 |

#### RCU (Read-Copy-Update)

| 文件 | 功能 |
|------|------|
| `rcupdate.c` | RCU 核心 |
| `rcutree.c` | 树形 RCU |
| `rcutiny.c` | 小型 RCU (嵌入式) |
| `srcu.c` | 可睡眠 RCU |

#### 模块与内核符号

| 文件 | 功能 |
|------|------|
| `module.c` | 内核模块加载/卸载 |
| `kallsyms.c` | 内核符号表 |
| `kprobes.c` | 内核探针 (动态追踪) |
| `params.c` | 模块参数 |

#### 电源管理

| 文件/目录 | 功能 |
|-----------|------|
| `power/` | 电源管理子目录 |
| `cpu.c` | CPU 热插拔 |
| `cpu_pm.c` | CPU 电源管理 |

#### 其他核心功能

| 文件 | 功能 |
|------|------|
| `printk.c` | 内核打印 |
| `panic.c` | 内核崩溃处理 |
| `sys.c` | 系统调用实现 |
| `sysctl.c` | /proc/sys 接口 |
| `kthread.c` | 内核线程 |
| `cgroup.c` | 控制组 |
| `nsproxy.c` | 命名空间代理 |
| `user_namespace.c` | 用户命名空间 |
| `cred.c` | 进程凭证 |
| `capability.c` | 权能机制 |
| `audit.c` | 审计系统 |
| `notifier.c` | 通知链 |
| `relay.c` | 中继通道 |

---

### mm/ - 内存管理

#### 页面分配

| 文件 | 功能 |
|------|------|
| `page_alloc.c` | **伙伴系统** (Buddy System) 页面分配器核心 |
| `bootmem.c` | 引导期内存分配器 |
| `nobootmem.c` | 无引导内存分配器 |
| `memblock.c` | 内存块分配器 |
| `mmzone.c` | 内存区域管理 |

#### 对象分配器

| 文件 | 功能 |
|------|------|
| `slab.c` | **SLAB 分配器** (经典实现) |
| `slub.c` | **SLUB 分配器** (默认，更简洁高效) |
| `slob.c` | **SLOB 分配器** (嵌入式系统，极小内存占用) |

#### 内存映射

| 文件 | 功能 |
|------|------|
| `mmap.c` | **内存映射**，VMA 管理 |
| `mremap.c` | 重新映射内存 |
| `mprotect.c` | 内存保护属性修改 |
| `mlock.c` | 内存锁定 |
| `madvise.c` | 内存建议 |
| `mincore.c` | 页面驻留查询 |

#### 虚拟内存

| 文件 | 功能 |
|------|------|
| `memory.c` | **页表管理**，缺页处理 |
| `vmalloc.c` | 虚拟连续内存分配 |
| `pgtable-generic.c` | 通用页表操作 |

#### 页面回收与交换

| 文件 | 功能 |
|------|------|
| `vmscan.c` | **页面扫描和回收** (kswapd) |
| `swap.c` | 交换核心 |
| `swapfile.c` | 交换文件/分区管理 |
| `swap_state.c` | 交换缓存 |
| `page_io.c` | 页面 I/O |

#### 页面写回

| 文件 | 功能 |
|------|------|
| `page-writeback.c` | 脏页写回 |
| `filemap.c` | 文件页面缓存 |
| `truncate.c` | 页面截断 |
| `readahead.c` | 预读 |

#### 反向映射

| 文件 | 功能 |
|------|------|
| `rmap.c` | **反向映射** (找出映射某页的所有进程) |

#### 大页支持

| 文件 | 功能 |
|------|------|
| `hugetlb.c` | 大页 (HugeTLB) 支持 |
| `huge_memory.c` | 透明大页 (THP) |

#### 内存控制

| 文件 | 功能 |
|------|------|
| `memcontrol.c` | 内存 cgroup 控制 |
| `page_cgroup.c` | 页面 cgroup |
| `oom_kill.c` | **OOM Killer** (内存不足时杀进程) |

#### 内存热插拔

| 文件 | 功能 |
|------|------|
| `memory_hotplug.c` | 内存热插拔 |
| `memory-failure.c` | 内存故障处理 |

#### 特殊功能

| 文件 | 功能 |
|------|------|
| `ksm.c` | 内核同页合并 (去重) |
| `migrate.c` | 页面迁移 |
| `compaction.c` | 内存压缩 |
| `shmem.c` | 共享内存/tmpfs |
| `percpu.c` | Per-CPU 变量分配 |

---

### fs/ - 文件系统

#### VFS 核心 (虚拟文件系统)

| 文件 | 功能 |
|------|------|
| `inode.c` | **inode 管理** (文件元数据) |
| `file.c` | **文件对象操作** |
| `super.c` | **超级块管理** |
| `namei.c` | **路径名解析** |
| `dcache.c` | **目录项缓存** (dentry cache) |
| `namespace.c` | 挂载命名空间 |
| `filesystems.c` | 文件系统注册 |

#### 文件操作

| 文件 | 功能 |
|------|------|
| `open.c` | open 系统调用 |
| `read_write.c` | read/write 系统调用 |
| `fcntl.c` | fcntl 系统调用 |
| `ioctl.c` | ioctl 系统调用 |
| `stat.c` | stat 系统调用 |
| `readdir.c` | 目录读取 |
| `select.c` | select/poll 系统调用 |
| `locks.c` | 文件锁 |
| `aio.c` | 异步 I/O |
| `splice.c` | splice/tee 系统调用 |
| `pipe.c` | 管道 |

#### 具体文件系统

| 目录 | 文件系统类型 |
|------|-------------|
| `ext2/` | Ext2 文件系统 |
| `ext3/` | Ext3 文件系统 (日志) |
| `ext4/` | **Ext4 文件系统** (现代默认) |
| `btrfs/` | **Btrfs** (B-tree 文件系统) |
| `xfs/` | XFS 高性能文件系统 |
| `reiserfs/` | ReiserFS |
| `jfs/` | JFS (IBM 日志文件系统) |
| `fat/` | FAT12/16/32 |
| `ntfs/` | NTFS (只读) |
| `hfs/`, `hfsplus/` | macOS 文件系统 |
| `isofs/` | ISO 9660 (CD-ROM) |
| `udf/` | UDF (DVD) |

#### 网络文件系统

| 目录 | 功能 |
|------|------|
| `nfs/` | NFS 客户端 |
| `nfsd/` | NFS 服务器 |
| `cifs/` | SMB/CIFS 客户端 |
| `ceph/` | Ceph 分布式文件系统 |
| `afs/` | AFS 客户端 |
| `9p/` | Plan 9 文件系统 |

#### 虚拟/特殊文件系统

| 目录 | 功能 |
|------|------|
| `proc/` | **/proc 文件系统** (进程信息) |
| `sysfs/` | **/sys 文件系统** (设备模型) |
| `debugfs/` | 调试文件系统 |
| `devpts/` | 伪终端 |
| `tmpfs/` | 内存文件系统 (通过 shmem.c) |
| `ramfs/` | RAM 文件系统 |
| `hugetlbfs/` | 大页文件系统 |
| `configfs/` | 配置文件系统 |
| `fuse/` | 用户空间文件系统 |

#### 二进制格式

| 文件 | 功能 |
|------|------|
| `binfmt_elf.c` | **ELF 格式加载器** |
| `binfmt_script.c` | 脚本 (#!) 处理 |
| `binfmt_misc.c` | 其他格式 |
| `exec.c` | execve 系统调用 |

#### 块设备

| 文件 | 功能 |
|------|------|
| `block_dev.c` | 块设备操作 |
| `buffer.c` | 缓冲区头管理 |
| `bio.c` | Block I/O |
| `direct-io.c` | 直接 I/O |
| `mpage.c` | 多页 I/O |

---

### net/ - 网络子系统

#### 核心

| 文件/目录 | 功能 |
|-----------|------|
| `socket.c` | **Socket 系统调用入口** |
| `core/` | **网络核心** |
| `core/sock.c` | socket 核心 |
| `core/skbuff.c` | **sk_buff 管理** (网络数据包) |
| `core/dev.c` | **网络设备接口** |
| `core/dst.c` | 路由缓存 |
| `core/neighbour.c` | 邻居子系统 (ARP/ND) |
| `core/filter.c` | BPF 包过滤 |
| `core/rtnetlink.c` | 路由 netlink |

#### 协议族

| 目录 | 协议 |
|------|------|
| `ipv4/` | **IPv4 协议栈** |
| `ipv6/` | **IPv6 协议栈** |
| `unix/` | **Unix 域 socket** |
| `packet/` | 原始包 socket |
| `netlink/` | Netlink socket |

#### IPv4 核心文件

| 文件 | 功能 |
|------|------|
| `ipv4/ip_input.c` | IP 输入处理 |
| `ipv4/ip_output.c` | IP 输出处理 |
| `ipv4/ip_forward.c` | IP 转发 |
| `ipv4/ip_fragment.c` | IP 分片 |
| `ipv4/tcp.c` | **TCP 协议核心** |
| `ipv4/tcp_input.c` | TCP 输入处理 |
| `ipv4/tcp_output.c` | TCP 输出处理 |
| `ipv4/tcp_ipv4.c` | TCP over IPv4 |
| `ipv4/udp.c` | **UDP 协议** |
| `ipv4/icmp.c` | ICMP 协议 |
| `ipv4/arp.c` | ARP 协议 |
| `ipv4/route.c` | IPv4 路由 |
| `ipv4/fib_*.c` | 转发信息库 |
| `ipv4/inet_connection_sock.c` | 连接管理 |
| `ipv4/inet_hashtables.c` | socket 哈希表 |

#### 防火墙/过滤

| 目录 | 功能 |
|------|------|
| `netfilter/` | **Netfilter 框架** |
| `netfilter/nf_conntrack*.c` | 连接跟踪 |
| `netfilter/nf_nat*.c` | NAT |
| `netfilter/xt_*.c` | iptables 匹配/目标 |

#### 其他协议

| 目录 | 功能 |
|------|------|
| `sctp/` | SCTP 协议 |
| `dccp/` | DCCP 协议 |
| `l2tp/` | L2TP 协议 |
| `bridge/` | 网桥 |
| `8021q/` | VLAN |
| `wireless/` | 无线网络 |
| `mac80211/` | 802.11 MAC 层 |
| `bluetooth/` | 蓝牙 |
| `can/` | CAN 总线 |
| `xfrm/` | IPSec 框架 |

#### 流量控制

| 目录 | 功能 |
|------|------|
| `sched/` | **QoS/流量调度** |
| `sched/sch_*.c` | 队列调度算法 |
| `sched/cls_*.c` | 分类器 |

---

### drivers/ - 设备驱动

#### 驱动框架

| 目录 | 功能 |
|------|------|
| `base/` | **驱动核心框架** |
| `base/bus.c` | 总线抽象 |
| `base/driver.c` | 驱动抽象 |
| `base/class.c` | 设备类 |
| `base/core.c` | 设备核心 |
| `base/platform.c` | 平台设备 |

#### 总线驱动

| 目录 | 功能 |
|------|------|
| `pci/` | **PCI 总线** |
| `usb/` | **USB 总线** |
| `usb/core/` | USB 核心 |
| `usb/host/` | USB 主机控制器 |
| `i2c/` | I2C 总线 |
| `spi/` | SPI 总线 |
| `amba/` | AMBA 总线 (ARM) |

#### 块设备驱动

| 目录 | 功能 |
|------|------|
| `block/` | 块设备驱动 |
| `ata/` | SATA/PATA 驱动 |
| `scsi/` | SCSI 子系统 |
| `nvme/` (3.3+) | NVMe 驱动 |
| `md/` | 软 RAID/LVM |
| `mmc/` | SD/MMC 卡 |

#### 字符设备驱动

| 目录 | 功能 |
|------|------|
| `char/` | 字符设备 |
| `tty/` | **终端驱动** |
| `input/` | 输入设备 (键盘、鼠标) |
| `rtc/` | 实时时钟 |
| `watchdog/` | 看门狗 |

#### 网络驱动

| 目录 | 功能 |
|------|------|
| `net/` | **网卡驱动** |
| `net/ethernet/` | 以太网驱动 |
| `net/wireless/` | 无线网卡驱动 |

#### 图形显示

| 目录 | 功能 |
|------|------|
| `gpu/` | GPU 驱动 |
| `gpu/drm/` | **DRM (Direct Rendering Manager)** |
| `video/` | 帧缓冲 |

#### 电源管理

| 目录 | 功能 |
|------|------|
| `acpi/` | ACPI 电源管理 |
| `cpufreq/` | CPU 频率调节 |
| `cpuidle/` | CPU 空闲管理 |
| `thermal/` | 热管理 |

#### 虚拟化

| 目录 | 功能 |
|------|------|
| `virtio/` | VirtIO 设备 |
| `xen/` | Xen 驱动 |
| `hv/` | Hyper-V 驱动 |
| `vhost/` | vhost |

---

### arch/ - 体系结构相关

每个 CPU 架构有独立目录，内部结构统一。

#### 支持的架构

| 目录 | 架构 |
|------|------|
| `x86/` | **x86/x86_64** (Intel/AMD) |
| `arm/` | **ARM 32 位** |
| `mips/` | MIPS |
| `powerpc/` | PowerPC |
| `sparc/` | SPARC |
| `ia64/` | Intel Itanium |
| `s390/` | IBM System z |
| `alpha/` | DEC Alpha |
| `sh/` | SuperH |
| `m68k/` | Motorola 68k |

#### 架构目录内部结构 (以 x86 为例)

| 目录 | 功能 |
|------|------|
| `x86/boot/` | **引导代码** |
| `x86/kernel/` | **架构相关内核代码** |
| `x86/kernel/entry_32.S` | 32 位系统调用入口 |
| `x86/kernel/entry_64.S` | 64 位系统调用入口 |
| `x86/kernel/head_32.S` | 32 位启动 |
| `x86/kernel/head_64.S` | 64 位启动 |
| `x86/kernel/setup.c` | 硬件初始化 |
| `x86/kernel/process.c` | 进程切换 |
| `x86/kernel/signal.c` | 信号处理 |
| `x86/kernel/irq.c` | 中断处理 |
| `x86/kernel/smp.c` | 多处理器支持 |
| `x86/kernel/cpu/` | CPU 检测和初始化 |
| `x86/mm/` | **内存管理** |
| `x86/mm/init.c` | 内存初始化 |
| `x86/mm/fault.c` | **缺页处理** |
| `x86/mm/pageattr.c` | 页属性 |
| `x86/mm/tlb.c` | TLB 管理 |
| `x86/include/` | 头文件 |
| `x86/lib/` | 库函数 |
| `x86/pci/` | PCI 支持 |
| `x86/configs/` | 默认配置 |

---

### ipc/ - 进程间通信

| 文件 | 功能 |
|------|------|
| `msg.c` | **消息队列** |
| `sem.c` | **信号量** |
| `shm.c` | **共享内存** |
| `mqueue.c` | POSIX 消息队列 |
| `util.c` | IPC 通用工具 |
| `namespace.c` | IPC 命名空间 |
| `syscall.c` | IPC 系统调用 |

---

### init/ - 系统初始化

| 文件 | 功能 |
|------|------|
| `main.c` | **start_kernel()** 内核入口点 |
| `do_mounts.c` | 根文件系统挂载 |
| `do_mounts_initrd.c` | initrd 处理 |
| `initramfs.c` | initramfs 处理 |
| `calibrate.c` | BogoMIPS 校准 |
| `version.c` | 版本信息 |

#### start_kernel() 调用流程

```
start_kernel() [init/main.c]
    │
    ├── setup_arch()              // 架构初始化
    ├── mm_init()                 // 内存管理初始化
    ├── sched_init()              // 调度器初始化
    ├── init_IRQ()                // 中断初始化
    ├── time_init()               // 时钟初始化
    ├── console_init()            // 控制台初始化
    ├── vfs_caches_init()         // VFS 缓存初始化
    ├── signals_init()            // 信号初始化
    ├── ...
    └── rest_init()
            │
            ├── kernel_thread(kernel_init)   // 创建 init 进程 (PID 1)
            └── kernel_thread(kthreadd)      // 创建 kthreadd (PID 2)
```

---

### block/ - 块设备层

| 文件 | 功能 |
|------|------|
| `blk-core.c` | **块设备核心** |
| `blk-settings.c` | 块设备设置 |
| `blk-exec.c` | 请求执行 |
| `blk-merge.c` | 请求合并 |
| `blk-map.c` | 缓冲区映射 |
| `blk-barrier.c` | I/O 屏障 |
| `blk-flush.c` | 刷新处理 |
| `elevator.c` | **I/O 调度器框架** |
| `cfq-iosched.c` | CFQ 调度器 |
| `deadline-iosched.c` | Deadline 调度器 |
| `noop-iosched.c` | Noop 调度器 |
| `genhd.c` | 通用硬盘 |
| `partition-generic.c` | 分区解析 |
| `scsi_ioctl.c` | SCSI ioctl |

---

### security/ - 安全子系统

| 目录/文件 | 功能 |
|-----------|------|
| `security.c` | **LSM (Linux Security Module) 框架** |
| `capability.c` | POSIX 权能 |
| `selinux/` | **SELinux** |
| `smack/` | SMACK |
| `tomoyo/` | TOMOYO |
| `apparmor/` | AppArmor |
| `yama/` | Yama |
| `keys/` | 密钥管理 |
| `integrity/` | 完整性检测 |

---

### crypto/ - 加密子系统

| 文件 | 功能 |
|------|------|
| `api.c` | 加密 API |
| `algapi.c` | 算法 API |
| `aes_generic.c` | AES 算法 |
| `sha1_generic.c` | SHA1 算法 |
| `sha256_generic.c` | SHA256 算法 |
| `md5.c` | MD5 算法 |
| `des_generic.c` | DES 算法 |
| `crc32c.c` | CRC32 |
| `cryptd.c` | 异步加密守护 |

---

### lib/ - 内核库

| 文件 | 功能 |
|------|------|
| `string.c` | 字符串操作 |
| `vsprintf.c` | 格式化输出 |
| `bitmap.c` | 位图操作 |
| `list_sort.c` | 链表排序 |
| `rbtree.c` | **红黑树** |
| `radix-tree.c` | **基数树** |
| `idr.c` | ID 分配器 |
| `crc32.c` | CRC32 |
| `zlib_*.c` | zlib 压缩 |
| `lzo/` | LZO 压缩 |
| `kobject.c` | 内核对象 |
| `kref.c` | 引用计数 |

---

### include/ - 头文件

| 目录 | 功能 |
|------|------|
| `linux/` | **内核核心头文件** |
| `linux/sched.h` | 调度相关 |
| `linux/mm.h` | 内存管理 |
| `linux/fs.h` | 文件系统 |
| `linux/net.h` | 网络 |
| `linux/list.h` | 链表 |
| `linux/kernel.h` | 通用定义 |
| `asm-generic/` | 通用汇编头文件 |
| `net/` | 网络头文件 |
| `scsi/` | SCSI 头文件 |
| `sound/` | 音频头文件 |
| `drm/` | DRM 头文件 |
| `acpi/` | ACPI 头文件 |

---

## 构建与配置系统

### Kbuild 系统

| 文件 | 功能 |
|------|------|
| `Makefile` | 顶层 Makefile |
| `Kbuild` | Kbuild 配置 |
| `Kconfig` | 内核配置选项 |
| `scripts/` | 构建脚本 |
| `scripts/Makefile.*` | Makefile 库 |
| `scripts/kconfig/` | 配置工具 |

### 配置命令

| 命令 | 说明 |
|------|------|
| `make menuconfig` | 文本菜单配置 |
| `make xconfig` | Qt 图形配置 |
| `make defconfig` | 默认配置 |
| `make oldconfig` | 升级配置 |

---

## 架构设计优点

### 1. 模块化设计 (Modularity)

```
┌─────────────────────────────────────────────────────────────┐
│  每个子系统独立目录，代码边界清晰                              │
│  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐            │
│  │ kernel/ │ │   mm/   │ │   fs/   │ │   net/  │            │
│  └─────────┘ └─────────┘ └─────────┘ └─────────┘            │
│       独立开发        独立测试        独立维护                │
└─────────────────────────────────────────────────────────────┘
```

- 驱动可编译为模块 (`.ko`)，按需加载
- 子系统之间通过明确定义的接口交互
- 便于代码复用和维护

### 2. 可移植性 (Portability)

```
┌─────────────────────────────────────────────────────────────┐
│              平台无关代码 (95%+)                             │
│     kernel/  mm/  fs/  net/  ipc/  crypto/  lib/            │
├─────────────────────────────────────────────────────────────┤
│              平台相关代码 (arch/)                            │
│  ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐                │
│  │  x86   │ │  ARM   │ │  MIPS  │ │  ...   │                │
│  └────────┘ └────────┘ └────────┘ └────────┘                │
└─────────────────────────────────────────────────────────────┘
```

- `arch/` 隔离了所有硬件相关代码
- 移植到新平台只需实现 `arch/xxx/` 目录
- 核心代码保持不变

### 3. 可扩展性 (Extensibility)

```
添加新功能:
┌───────────────────────────────────────────────┐
│ 新文件系统 → fs/myfs/                          │
│ 新驱动程序 → drivers/xxx/                      │
│ 新网络协议 → net/myproto/                      │
│ 新架构支持 → arch/myarch/                      │
└───────────────────────────────────────────────┘
```

- 遵循统一的接口规范
- 不需要修改核心代码

### 4. 层次化抽象 (Layered Abstraction)

```
应用程序
    ↓ 系统调用
VFS (虚拟文件系统)     ← 统一接口
    ↓
具体文件系统 (ext4, xfs, nfs...)
    ↓
块设备层
    ↓
设备驱动
    ↓
硬件
```

- 每一层定义清晰的接口
- 上层不关心下层实现细节
- 易于替换和升级各层实现

### 5. 配置灵活 (Configurability)

- **Kconfig** 系统支持细粒度配置
- 可裁剪内核用于嵌入式设备 (最小可至 ~500KB)
- 可选编译功能为模块或内建
- 支持多种默认配置文件

### 6. 并行开发 (Parallel Development)

```
                     ┌────────────────┐
                     │    Linus       │
                     │   (主维护者)    │
                     └───────┬────────┘
                             │
        ┌────────────────────┼────────────────────┐
        ↓                    ↓                    ↓
┌───────────────┐    ┌───────────────┐    ┌───────────────┐
│ 网络子系统     │    │ 文件系统      │    │ 驱动子系统     │
│   维护者       │    │   维护者      │    │   维护者       │
└───────────────┘    └───────────────┘    └───────────────┘
```

- 全球数千开发者可同时工作
- 不同子系统有独立的维护者
- `MAINTAINERS` 文件定义责任人

---

## 阅读建议

1. **入口点**: `init/main.c` → `start_kernel()` 函数
2. **进程管理**: `kernel/fork.c`, `kernel/sched.c`
3. **内存管理**: `mm/page_alloc.c`, `mm/mmap.c`
4. **文件系统**: `fs/read_write.c`, `fs/open.c`
5. **驱动框架**: 从简单的 `drivers/char/` 开始
6. **网络**: `net/socket.c`, `net/ipv4/tcp.c`

---

*本文档基于 Linux 3.2 内核源码分析生成*

