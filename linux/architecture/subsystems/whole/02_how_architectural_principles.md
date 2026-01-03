# HOW｜核心架构原则与控制机制

## 1. 内核的高层架构原则

```
HIGH-LEVEL ARCHITECTURAL PRINCIPLES
+=============================================================================+
|                                                                              |
|  PRINCIPLE 1: MECHANISM VS POLICY SEPARATION (机制与策略分离)                |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  MECHANISM (What CAN be done)      POLICY (What SHOULD be done)  │    │ |
|  │  │  ─────────────────────────────     ───────────────────────────── │    │ |
|  │  │                                                                  │    │ |
|  │  │  Scheduler CAN:                    CFS policy DECIDES:           │    │ |
|  │  │  • context switch                  • which task runs next        │    │ |
|  │  │  • preempt running task            • for how long                │    │ |
|  │  │  • migrate tasks                   • on which CPU                │    │ |
|  │  │                                                                  │    │ |
|  │  │  VFS CAN:                          Filesystem policy DECIDES:    │    │ |
|  │  │  • open/close files                • how data is stored          │    │ |
|  │  │  • read/write                      • caching strategy            │    │ |
|  │  │  • cache pages                     • journaling mode             │    │ |
|  │  │                                                                  │    │ |
|  │  │  Memory CAN:                       Allocator policy DECIDES:     │    │ |
|  │  │  • map pages                       • which page to reclaim       │    │ |
|  │  │  • swap out                        • when to swap                │    │ |
|  │  │  • reclaim                         • watermark thresholds        │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  Why this matters:                                                       │ |
|  │  • Mechanism code is stable (rarely changes)                            │ |
|  │  • Policy can be swapped (CFS vs RT vs deadline)                        │ |
|  │  • Testing: mechanism tested once, policies tested separately           │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  PRINCIPLE 2: RUNTIME POLYMORPHISM (运行时多态 - ops tables)                 |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  C lacks classes/interfaces. Linux uses "ops tables":            │    │ |
|  │  │                                                                  │    │ |
|  │  │  struct file_operations {        // "interface" definition       │    │ |
|  │  │      ssize_t (*read)(...);                                       │    │ |
|  │  │      ssize_t (*write)(...);                                      │    │ |
|  │  │      int (*open)(...);                                           │    │ |
|  │  │      int (*release)(...);                                        │    │ |
|  │  │      // ... 30+ more operations                                  │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  static const struct file_operations ext4_file_ops = {           │    │ |
|  │  │      .read  = ext4_file_read,    // ext4 implementation          │    │ |
|  │  │      .write = ext4_file_write,                                   │    │ |
|  │  │      .open  = ext4_file_open,                                    │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  static const struct file_operations nfs_file_ops = {            │    │ |
|  │  │      .read  = nfs_file_read,     // NFS implementation           │    │ |
|  │  │      .write = nfs_file_write,                                    │    │ |
|  │  │      .open  = nfs_file_open,                                     │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  Usage (polymorphic dispatch):                                   │    │ |
|  │  │  file->f_op->read(file, buf, count, pos);                        │    │ |
|  │  │  // Calls ext4_file_read or nfs_file_read based on file type     │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  Key ops tables in kernel:                                               │ |
|  │  • file_operations     (VFS)                                            │ |
|  │  • inode_operations    (VFS)                                            │ |
|  │  • address_space_operations (page cache)                                │ |
|  │  • net_device_ops      (networking)                                     │ |
|  │  • proto_ops           (sockets)                                        │ |
|  │  • block_device_operations (block layer)                                │ |
|  │  • sched_class         (scheduler)                                      │ |
|  │  • security_operations (LSM)                                            │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  PRINCIPLE 3: FAST PATH VS SLOW PATH SEPARATION (快速/慢速路径分离)          |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Example: Page Fault Handling                                    │    │ |
|  │  │                                                                  │    │ |
|  │  │  Page Fault ───────┬─────────────────────────────────────────    │    │ |
|  │  │                    │                                             │    │ |
|  │  │         ┌──────────┴──────────┐                                  │    │ |
|  │  │         ▼                     ▼                                  │    │ |
|  │  │  ┌─────────────────┐   ┌─────────────────┐                       │    │ |
|  │  │  │   FAST PATH     │   │   SLOW PATH     │                       │    │ |
|  │  │  │   (~90% cases)  │   │   (~10% cases)  │                       │    │ |
|  │  │  ├─────────────────┤   ├─────────────────┤                       │    │ |
|  │  │  │ • Page in cache │   │ • Read from disk│                       │    │ |
|  │  │  │ • COW (no copy) │   │ • Allocate page │                       │    │ |
|  │  │  │ • Just map PTE  │   │ • Handle swap   │                       │    │ |
|  │  │  ├─────────────────┤   ├─────────────────┤                       │    │ |
|  │  │  │ Optimized for:  │   │ Handles:        │                       │    │ |
|  │  │  │ • Minimal locks │   │ • Complex cases │                       │    │ |
|  │  │  │ • No I/O        │   │ • I/O wait      │                       │    │ |
|  │  │  │ • Inlined code  │   │ • Error paths   │                       │    │ |
|  │  │  │ • likely() hints│   │ • Full logging  │                       │    │ |
|  │  │  └─────────────────┘   └─────────────────┘                       │    │ |
|  │  │                                                                  │    │ |
|  │  │  Code pattern:                                                   │    │ |
|  │  │  if (likely(page_in_cache)) {                                    │    │ |
|  │  │      // fast path: ~50 cycles                                    │    │ |
|  │  │      pte_set(pte, page);                                         │    │ |
|  │  │      return;                                                     │    │ |
|  │  │  }                                                               │    │ |
|  │  │  // slow path: may take millions of cycles                       │    │ |
|  │  │  return handle_mm_fault_slow(mm, vma, address, flags);           │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  Other examples:                                                         │ |
|  │  • Syscall: cached vs full path lookup                                  │ |
|  │  • Networking: established connection vs state machine                  │ |
|  │  • Memory: SLUB fast path vs full allocation                            │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

三大高层架构原则：

1. **机制与策略分离**：
   - 机制提供"能做什么"（如调度器能切换上下文）
   - 策略决定"应该怎么做"（如 CFS 决定运行哪个任务）
   - 好处：机制稳定，策略可替换，测试可分离

2. **运行时多态（ops 表）**：
   - C 语言没有类/接口，Linux 使用函数指针表实现多态
   - 核心 ops 表：`file_operations`、`inode_operations`、`net_device_ops`、`sched_class` 等
   - 调用方式：`file->f_op->read()` 根据文件类型分发到不同实现

3. **快速/慢速路径分离**：
   - 快速路径处理 90% 的常见情况：最少锁、无 I/O、内联代码
   - 慢速路径处理复杂情况：完整验证、I/O 等待、错误处理
   - 使用 `likely()`/`unlikely()` 提示编译器优化分支预测

---

## 2. 内核如何将复杂度划分到子系统

```
SUBSYSTEM PARTITIONING
+=============================================================================+
|                                                                              |
|  LINUX KERNEL SUBSYSTEM MAP                                                  |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │                         ┌───────────────────────┐                        │ |
|  │                         │    SYSTEM CALLS       │                        │ |
|  │                         │  (arch/x86/entry/)    │                        │ |
|  │                         └───────────┬───────────┘                        │ |
|  │                                     │                                    │ |
|  │  ┌──────────────────────────────────┼──────────────────────────────────┐│ |
|  │  │                                  ▼                                   ││ |
|  │  │  ┌───────────────────────────────────────────────────────────────┐  ││ |
|  │  │  │              CORE KERNEL INFRASTRUCTURE                        │  ││ |
|  │  │  ├───────────────────────────────────────────────────────────────┤  ││ |
|  │  │  │                                                                │  ││ |
|  │  │  │  ┌─────────────────────────────────────────────────────────┐  │  ││ |
|  │  │  │  │  PROCESS MANAGEMENT (kernel/sched/, kernel/fork.c)      │  │  ││ |
|  │  │  │  │  ─────────────────────────────────────────────────────  │  │  ││ |
|  │  │  │  │  • task_struct: execution context                       │  │  ││ |
|  │  │  │  │  • Scheduler classes: CFS, RT, Deadline                 │  │  ││ |
|  │  │  │  │  • Process creation/destruction                         │  │  ││ |
|  │  │  │  │  • Signals, wait queues                                 │  │  ││ |
|  │  │  │  └─────────────────────────────────────────────────────────┘  │  ││ |
|  │  │  │                                                                │  ││ |
|  │  │  │  ┌─────────────────────────────────────────────────────────┐  │  ││ |
|  │  │  │  │  MEMORY MANAGEMENT (mm/)                                │  │  ││ |
|  │  │  │  │  ─────────────────────────────────────────────────────  │  │  ││ |
|  │  │  │  │  • mm_struct: address space                             │  │  ││ |
|  │  │  │  │  • Page tables, page fault handling                     │  │  ││ |
|  │  │  │  │  • Slab allocator (SLUB)                                │  │  ││ |
|  │  │  │  │  • Page reclaim, swap                                   │  │  ││ |
|  │  │  │  └─────────────────────────────────────────────────────────┘  │  ││ |
|  │  │  │                                                                │  ││ |
|  │  │  └───────────────────────────────────────────────────────────────┘  ││ |
|  │  │                                  │                                   ││ |
|  │  │                                  ▼                                   ││ |
|  │  │  ┌───────────────────────────────────────────────────────────────┐  ││ |
|  │  │  │                    I/O SUBSYSTEMS                              │  ││ |
|  │  │  ├───────────────────────────────────────────────────────────────┤  ││ |
|  │  │  │                                                                │  ││ |
|  │  │  │  ┌─────────────────────────────────────────────────────────┐  │  ││ |
|  │  │  │  │  VFS AND FILE SYSTEMS (fs/)                             │  │  ││ |
|  │  │  │  │  ─────────────────────────────────────────────────────  │  │  ││ |
|  │  │  │  │  • VFS layer: file, inode, dentry                       │  │  ││ |
|  │  │  │  │  • Page cache                                           │  │  ││ |
|  │  │  │  │  • Filesystems: ext4, xfs, btrfs, nfs                   │  │  ││ |
|  │  │  │  └─────────────────────────────────────────────────────────┘  │  ││ |
|  │  │  │                                                                │  ││ |
|  │  │  │  ┌─────────────────────────────────────────────────────────┐  │  ││ |
|  │  │  │  │  NETWORKING STACK (net/)                                │  │  ││ |
|  │  │  │  │  ─────────────────────────────────────────────────────  │  │  ││ |
|  │  │  │  │  • Socket layer                                         │  │  ││ |
|  │  │  │  │  • TCP/UDP/IP                                           │  │  ││ |
|  │  │  │  │  • Netfilter, routing                                   │  │  ││ |
|  │  │  │  │  • sk_buff, net_device                                  │  │  ││ |
|  │  │  │  └─────────────────────────────────────────────────────────┘  │  ││ |
|  │  │  │                                                                │  ││ |
|  │  │  │  ┌─────────────────────────────────────────────────────────┐  │  ││ |
|  │  │  │  │  BLOCK LAYER (block/)                                   │  │  ││ |
|  │  │  │  │  ─────────────────────────────────────────────────────  │  │  ││ |
|  │  │  │  │  • Request queue, I/O schedulers                        │  │  ││ |
|  │  │  │  │  • bio structure                                        │  │  ││ |
|  │  │  │  │  • Multiqueue support                                   │  │  ││ |
|  │  │  │  └─────────────────────────────────────────────────────────┘  │  ││ |
|  │  │  │                                                                │  ││ |
|  │  │  └───────────────────────────────────────────────────────────────┘  ││ |
|  │  │                                  │                                   ││ |
|  │  │                                  ▼                                   ││ |
|  │  │  ┌───────────────────────────────────────────────────────────────┐  ││ |
|  │  │  │                 HARDWARE ABSTRACTION                           │  ││ |
|  │  │  ├───────────────────────────────────────────────────────────────┤  ││ |
|  │  │  │                                                                │  ││ |
|  │  │  │  ┌─────────────────────────────────────────────────────────┐  │  ││ |
|  │  │  │  │  DEVICE DRIVERS AND DRIVER MODEL (drivers/)             │  │  ││ |
|  │  │  │  │  ─────────────────────────────────────────────────────  │  │  ││ |
|  │  │  │  │  • struct device, driver, bus                           │  │  ││ |
|  │  │  │  │  • Platform, PCI, USB, I2C buses                        │  │  ││ |
|  │  │  │  │  • 10,000+ device drivers                               │  │  ││ |
|  │  │  │  └─────────────────────────────────────────────────────────┘  │  ││ |
|  │  │  │                                                                │  ││ |
|  │  │  └───────────────────────────────────────────────────────────────┘  ││ |
|  │  │                                  │                                   ││ |
|  │  │                                  ▼                                   ││ |
|  │  │  ┌───────────────────────────────────────────────────────────────┐  ││ |
|  │  │  │                 CROSS-CUTTING CONCERNS                         │  ││ |
|  │  │  ├───────────────────────────────────────────────────────────────┤  ││ |
|  │  │  │                                                                │  ││ |
|  │  │  │  ┌────────────────┐ ┌────────────────┐ ┌────────────────────┐ │  ││ |
|  │  │  │  │ SECURITY (LSM) │ │ TRACING       │ │ POWER MANAGEMENT   │ │  ││ |
|  │  │  │  │ security/      │ │ kernel/trace/ │ │ drivers/cpufreq/   │ │  ││ |
|  │  │  │  │                │ │               │ │                    │ │  ││ |
|  │  │  │  │ • SELinux      │ │ • ftrace      │ │ • cpufreq          │ │  ││ |
|  │  │  │  │ • AppArmor     │ │ • perf events │ │ • cpuidle          │ │  ││ |
|  │  │  │  │ • Capabilities │ │ • kprobes     │ │ • suspend/resume   │ │  ││ |
|  │  │  │  └────────────────┘ └────────────────┘ └────────────────────┘ │  ││ |
|  │  │  │                                                                │  ││ |
|  │  │  └───────────────────────────────────────────────────────────────┘  ││ |
|  │  │                                                                      ││ |
|  │  └──────────────────────────────────────────────────────────────────────┘│ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

Linux 内核子系统划分：

**核心基础设施**：
- **进程管理**：`task_struct`、调度器类（CFS/RT/Deadline）、信号、等待队列
- **内存管理**：`mm_struct`、页表、页故障处理、SLUB 分配器、页面回收

**I/O 子系统**：
- **VFS 和文件系统**：VFS 抽象层、页面缓存、ext4/xfs/btrfs 等
- **网络栈**：Socket 层、TCP/UDP/IP、Netfilter、路由
- **块层**：请求队列、I/O 调度器、bio 结构

**硬件抽象**：
- **设备驱动和驱动模型**：device/driver/bus 结构、各种总线（PCI/USB/I2C）

**横切关注点**：
- **安全**：LSM 框架、SELinux、AppArmor、Capabilities
- **追踪**：ftrace、perf events、kprobes
- **电源管理**：cpufreq、cpuidle、suspend/resume

---

## 3. 子系统边界如何在没有硬隔离的情况下执行

```
SUBSYSTEM BOUNDARY ENFORCEMENT
+=============================================================================+
|                                                                              |
|  MECHANISM 1: OPAQUE CORE STRUCTS (不透明核心结构)                           |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  task_struct (defined in include/linux/sched.h)                  │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │  struct task_struct {                                       │ │    │ |
|  │  │  │      // Only scheduler code touches these:                  │ │    │ |
|  │  │  │      int prio, static_prio, normal_prio;                    │ │    │ |
|  │  │  │      struct sched_entity se;                                │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │      // Only mm code touches these:                         │ │    │ |
|  │  │  │      struct mm_struct *mm, *active_mm;                      │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │      // Only signal code touches these:                     │ │    │ |
|  │  │  │      struct signal_struct *signal;                          │ │    │ |
|  │  │  │      struct sighand_struct *sighand;                        │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │      // Only fs code touches these:                         │ │    │ |
|  │  │  │      struct fs_struct *fs;                                  │ │    │ |
|  │  │  │      struct files_struct *files;                            │ │    │ |
|  │  │  │  };                                                         │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  │  Enforcement: Social contract + code review                      │    │ |
|  │  │  • Scheduler maintainer reviews scheduler field changes          │    │ |
|  │  │  • MM maintainer reviews mm field changes                        │    │ |
|  │  │  • Cross-subsystem access requires explicit justification        │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  MECHANISM 2: CONTROLLED CALLBACK INTERFACES (受控回调接口)                  |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  VFS → Filesystem Interface                                      │    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌────────────────┐      ┌────────────────────────────────────┐ │    │ |
|  │  │  │                │      │                                    │ │    │ |
|  │  │  │   VFS Core     │      │    Filesystem (ext4, xfs, ...)    │ │    │ |
|  │  │  │                │      │                                    │ │    │ |
|  │  │  │  vfs_read()  ──┼─────►│ file_operations.read               │ │    │ |
|  │  │  │  vfs_write() ──┼─────►│ file_operations.write              │ │    │ |
|  │  │  │  lookup()    ──┼─────►│ inode_operations.lookup            │ │    │ |
|  │  │  │                │      │                                    │ │    │ |
|  │  │  │  Owns:         │      │  Implements:                       │ │    │ |
|  │  │  │  • dcache      │      │  • On-disk format                  │ │    │ |
|  │  │  │  • inode cache │      │  • Journaling                      │ │    │ |
|  │  │  │  • file table  │      │  • Block allocation                │ │    │ |
|  │  │  │                │      │                                    │ │    │ |
|  │  │  └────────────────┘      └────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  │  Contract:                                                       │    │ |
|  │  │  • VFS never looks at ext4-specific fields                      │    │ |
|  │  │  • ext4 never manipulates dcache directly                       │    │ |
|  │  │  • All interaction through defined ops                          │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  MECHANISM 3: OWNERSHIP AND LIFECYCLE RULES (所有权和生命周期规则)           |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  OWNERSHIP HIERARCHY                                             │    │ |
|  │  │                                                                  │    │ |
|  │  │  task_struct                                                     │    │ |
|  │  │      │ owns (via refcount)                                       │    │ |
|  │  │      ├── mm_struct ────────────────────────────────────────────  │    │ |
|  │  │      │       │ owns                                              │    │ |
|  │  │      │       └── vm_area_struct (VMA list)                       │    │ |
|  │  │      │               │ references                                │    │ |
|  │  │      │               └── struct page (via page tables)           │    │ |
|  │  │      │                                                           │    │ |
|  │  │      ├── files_struct ────────────────────────────────────────── │    │ |
|  │  │      │       │ owns                                              │    │ |
|  │  │      │       └── struct file (fd table)                          │    │ |
|  │  │      │               │ references                                │    │ |
|  │  │      │               ├── dentry                                  │    │ |
|  │  │      │               └── inode                                   │    │ |
|  │  │      │                                                           │    │ |
|  │  │      └── signal_struct                                           │    │ |
|  │  │              │ owns                                              │    │ |
|  │  │              └── sighand_struct                                  │    │ |
|  │  │                                                                  │    │ |
|  │  │  Lifecycle rules:                                                │    │ |
|  │  │  • Parent refcount must be held before accessing child          │    │ |
|  │  │  • Teardown happens in reverse order of creation                │    │ |
|  │  │  • get/put functions enforce refcounting                        │    │ |
|  │  │                                                                  │    │ |
|  │  │  Example:                                                        │    │ |
|  │  │  mm = get_task_mm(task);  // increments mm->mm_users             │    │ |
|  │  │  // ... use mm ...                                               │    │ |
|  │  │  mmput(mm);               // decrements, frees if 0              │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  MECHANISM 4: EXPORT_SYMBOL CONTROLS (符号导出控制)                          |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  EXPORT_SYMBOL(func);         // Exported to all modules        │    │ |
|  │  │  EXPORT_SYMBOL_GPL(func);     // Exported only to GPL modules   │    │ |
|  │  │  /* no export */              // Internal to subsystem          │    │ |
|  │  │                                                                  │    │ |
|  │  │  Example from mm/:                                               │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  // Public API - exported                                   │ │    │ |
|  │  │  │  EXPORT_SYMBOL(get_user_pages);                             │ │    │ |
|  │  │  │  EXPORT_SYMBOL(vmalloc);                                    │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  // GPL-only API - some internal details exposed            │ │    │ |
|  │  │  │  EXPORT_SYMBOL_GPL(page_cache_sync_readahead);              │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  // Internal - no export, subsystem-private                 │ │    │ |
|  │  │  │  static void __free_pages_ok(...);                          │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  │  Enforcement: link-time error if using unexported symbol        │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

子系统边界执行机制：

1. **不透明核心结构**：
   - `task_struct` 包含多个子系统的字段
   - 每个子系统只触碰自己的字段
   - 通过代码审查和社会契约执行

2. **受控回调接口**：
   - VFS 只通过 `file_operations` 和 `inode_operations` 调用文件系统
   - 文件系统从不直接操作 dcache
   - 所有交互通过定义的 ops 表

3. **所有权和生命周期规则**：
   - 明确的所有权层次结构
   - 必须先持有父级引用才能访问子级
   - get/put 函数强制引用计数

4. **EXPORT_SYMBOL 控制**：
   - 无导出 = 子系统内部
   - `EXPORT_SYMBOL_GPL` = 仅 GPL 模块可用
   - `EXPORT_SYMBOL` = 公共 API

---

## 4. 内核如何管理控制流、状态和资源生命周期

```
CONTROL FLOW, STATE, AND RESOURCE MANAGEMENT
+=============================================================================+
|                                                                              |
|  CONTROL FLOW MANAGEMENT (控制流管理)                                        |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  PATTERN 1: CALLBACKS (回调)                                             │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  struct file_operations ext4_ops = {                             │    │ |
|  │  │      .read = ext4_file_read,                                     │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  // VFS calls through function pointer                           │    │ |
|  │  │  file->f_op->read(file, buf, count, pos);                        │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  PATTERN 2: PIPELINES (管道处理)                                         │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Network Receive Pipeline:                                       │    │ |
|  │  │                                                                  │    │ |
|  │  │  NIC → driver → netif_receive_skb → ip_rcv → tcp_v4_rcv → sock  │    │ |
|  │  │   │                                                              │    │ |
|  │  │   └──► Netfilter hooks at each stage can intercept/modify       │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  PATTERN 3: HOOKS (钩子系统)                                             │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Notifier chains:                                                │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  register_netdevice_notifier(&my_nb);  // subscribe        │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  // When network device state changes:                      │ │    │ |
|  │  │  │  call_netdevice_notifiers(NETDEV_UP, dev);                  │ │    │ |
|  │  │  │      │                                                      │ │    │ |
|  │  │  │      ├── subscriber 1: routing table update                 │ │    │ |
|  │  │  │      ├── subscriber 2: ARP cache clear                      │ │    │ |
|  │  │  │      └── subscriber 3: bonding driver update                │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  │  LSM hooks:                                                      │    │ |
|  │  │  security_file_permission(file, mask);  // every file access    │    │ |
|  │  │  security_socket_create();              // every socket create  │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  SHARED STATE MANAGEMENT (共享状态管理)                                      |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  TECHNIQUE 1: LOCKS (锁)                                                 │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  spinlock_t lock;                                                │    │ |
|  │  │  spin_lock(&lock);                                               │    │ |
|  │  │  // critical section - can't sleep                               │    │ |
|  │  │  spin_unlock(&lock);                                             │    │ |
|  │  │                                                                  │    │ |
|  │  │  struct mutex lock;                                              │    │ |
|  │  │  mutex_lock(&lock);                                              │    │ |
|  │  │  // critical section - can sleep                                 │    │ |
|  │  │  mutex_unlock(&lock);                                            │    │ |
|  │  │                                                                  │    │ |
|  │  │  rwlock_t lock;                                                  │    │ |
|  │  │  read_lock(&lock);  // multiple readers                          │    │ |
|  │  │  write_lock(&lock); // exclusive writer                          │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  TECHNIQUE 2: PER-CPU DATA (Per-CPU 数据)                                │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  DEFINE_PER_CPU(int, counter);                                   │    │ |
|  │  │                                                                  │    │ |
|  │  │  CPU 0        CPU 1        CPU 2        CPU 3                    │    │ |
|  │  │  ┌────────┐   ┌────────┐   ┌────────┐   ┌────────┐               │    │ |
|  │  │  │counter │   │counter │   │counter │   │counter │               │    │ |
|  │  │  │  = 5   │   │  = 3   │   │  = 7   │   │  = 2   │               │    │ |
|  │  │  └────────┘   └────────┘   └────────┘   └────────┘               │    │ |
|  │  │                                                                  │    │ |
|  │  │  No sharing = no locking needed!                                 │    │ |
|  │  │  Used for: statistics, caches, per-CPU work queues               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  TECHNIQUE 3: RCU (Read-Copy-Update)                                     │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Reader:                      Writer:                            │    │ |
|  │  │  rcu_read_lock();             new = kmalloc(...);                │    │ |
|  │  │  p = rcu_dereference(ptr);    // initialize new                  │    │ |
|  │  │  // use p                     rcu_assign_pointer(ptr, new);      │    │ |
|  │  │  rcu_read_unlock();           synchronize_rcu(); // wait readers │    │ |
|  │  │                               kfree(old);                        │    │ |
|  │  │                                                                  │    │ |
|  │  │  Readers: nearly free (no locks, no atomics)                     │    │ |
|  │  │  Writers: pay synchronization cost                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  Used for: routing tables, fd table, module list                 │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  RESOURCE LIFETIME MANAGEMENT (资源生命周期管理)                             |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  TECHNIQUE 1: REFERENCE COUNTING (引用计数)                              │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  struct kref {                                                   │    │ |
|  │  │      atomic_t refcount;                                          │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  kref_init(&obj->kref);       // refcount = 1                    │    │ |
|  │  │  kref_get(&obj->kref);        // refcount++                      │    │ |
|  │  │  kref_put(&obj->kref, release_fn);  // refcount--, free if 0    │    │ |
|  │  │                                                                  │    │ |
|  │  │  Examples:                                                       │    │ |
|  │  │  • struct file → f_count                                         │    │ |
|  │  │  • struct inode → i_count                                        │    │ |
|  │  │  • struct dentry → d_count                                       │    │ |
|  │  │  • struct sk_buff → users                                        │    │ |
|  │  │  • struct page → _count                                          │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  TECHNIQUE 2: EXPLICIT TEARDOWN PATHS (显式清理路径)                     │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  int my_init(void)                                               │    │ |
|  │  │  {                                                               │    │ |
|  │  │      ret = alloc_a();                                            │    │ |
|  │  │      if (ret) goto fail_a;                                       │    │ |
|  │  │                                                                  │    │ |
|  │  │      ret = alloc_b();                                            │    │ |
|  │  │      if (ret) goto fail_b;                                       │    │ |
|  │  │                                                                  │    │ |
|  │  │      ret = alloc_c();                                            │    │ |
|  │  │      if (ret) goto fail_c;                                       │    │ |
|  │  │                                                                  │    │ |
|  │  │      return 0;                                                   │    │ |
|  │  │                                                                  │    │ |
|  │  │  fail_c:                                                         │    │ |
|  │  │      free_b();                                                   │    │ |
|  │  │  fail_b:                                                         │    │ |
|  │  │      free_a();                                                   │    │ |
|  │  │  fail_a:                                                         │    │ |
|  │  │      return ret;                                                 │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  Cleanup in reverse order of allocation                          │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**控制流管理**：
- **回调**：通过函数指针表（ops）实现多态调用
- **管道**：数据包经过多阶段处理，每阶段可插入钩子
- **钩子**：notifier chains 和 LSM hooks 实现事件订阅

**共享状态管理**：
- **锁**：spinlock（不可睡眠）、mutex（可睡眠）、rwlock
- **Per-CPU 数据**：每个 CPU 有自己的副本，无需锁
- **RCU**：读者几乎免费，写者承担同步成本

**资源生命周期**：
- **引用计数**：`kref_get()`/`kref_put()` 管理对象生命周期
- **显式清理路径**：使用 goto 链实现反向顺序清理

---

## 5. 哪些部分是可扩展的，哪些是刻意刚性的

```
EXTENSIBILITY VS RIGIDITY
+=============================================================================+
|                                                                              |
|  INTENTIONALLY EXTENSIBLE (刻意可扩展)                                       |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  1. OPS TABLES (操作表)                                                  │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  // Add a new filesystem                                         │    │ |
|  │  │  static struct file_system_type my_fs = {                        │    │ |
|  │  │      .name    = "myfs",                                          │    │ |
|  │  │      .mount   = myfs_mount,                                      │    │ |
|  │  │      .kill_sb = myfs_kill_sb,                                    │    │ |
|  │  │  };                                                              │    │ |
|  │  │  register_filesystem(&my_fs);                                    │    │ |
|  │  │                                                                  │    │ |
|  │  │  // Add a new scheduler class                                    │    │ |
|  │  │  static const struct sched_class my_sched_class = {              │    │ |
|  │  │      .enqueue_task   = my_enqueue,                               │    │ |
|  │  │      .dequeue_task   = my_dequeue,                               │    │ |
|  │  │      .pick_next_task = my_pick_next,                             │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  2. NOTIFIER CHAINS (通知链)                                             │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  // Subscribe to CPU hotplug events                              │    │ |
|  │  │  static struct notifier_block my_nb = {                          │    │ |
|  │  │      .notifier_call = my_cpu_callback,                           │    │ |
|  │  │  };                                                              │    │ |
|  │  │  register_cpu_notifier(&my_nb);                                  │    │ |
|  │  │                                                                  │    │ |
|  │  │  // Subscribe to memory pressure events                          │    │ |
|  │  │  register_shrinker(&my_shrinker);                                │    │ |
|  │  │                                                                  │    │ |
|  │  │  // Subscribe to network device events                           │    │ |
|  │  │  register_netdevice_notifier(&my_notifier);                      │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  3. PLUGIN SUBSYSTEMS (插件子系统)                                       │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Netfilter:                                                      │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │  PRE_ROUTING → FORWARD → POST_ROUTING                      │ │    │ |
|  │  │  │       │            │           │                           │ │    │ |
|  │  │  │       ▼            ▼           ▼                           │ │    │ |
|  │  │  │  [hook1]       [hook2]     [hook3]                         │ │    │ |
|  │  │  │  [hook4]                   [hook5]                         │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  nf_register_hook(&my_hook_ops);                            │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  │  LSM (Linux Security Modules):                                   │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │  security_file_open() ──► SELinux check                     │ │    │ |
|  │  │  │                       ──► AppArmor check                    │ │    │ |
|  │  │  │                       ──► SMACK check                       │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  │  TCP Congestion Control:                                         │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │  tcp_register_congestion_control(&my_cc);                   │ │    │ |
|  │  │  │  // Now "my_cc" can be set via setsockopt                   │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  INTENTIONALLY RIGID (刻意刚性)                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  1. CORE DATA STRUCTURES (核心数据结构)                                  │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  These structures are NOT extensible:                            │    │ |
|  │  │                                                                  │    │ |
|  │  │  • task_struct      - process representation                     │    │ |
|  │  │  • mm_struct        - address space                              │    │ |
|  │  │  • page             - physical page metadata                     │    │ |
|  │  │  • sk_buff          - network packet buffer                      │    │ |
|  │  │                                                                  │    │ |
|  │  │  Why? Changing layout breaks:                                    │    │ |
|  │  │  • Cache alignment assumptions                                   │    │ |
|  │  │  • Per-CPU data layouts                                          │    │ |
|  │  │  • Binary compatibility                                          │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  2. SYSCALL INTERFACE (系统调用接口)                                     │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  System calls are stable ABI:                                    │    │ |
|  │  │                                                                  │    │ |
|  │  │  • open(), read(), write(), close()                              │    │ |
|  │  │  • fork(), exec(), wait()                                        │    │ |
|  │  │  • mmap(), munmap(), mprotect()                                  │    │ |
|  │  │                                                                  │    │ |
|  │  │  "We don't break userspace" - Linus Torvalds                     │    │ |
|  │  │                                                                  │    │ |
|  │  │  Once a syscall exists, it exists forever with same semantics    │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  3. MEMORY LAYOUT CONTRACTS (内存布局契约)                               │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Fixed contracts:                                                │    │ |
|  │  │                                                                  │    │ |
|  │  │  • PAGE_SIZE (usually 4KB)                                       │    │ |
|  │  │  • Kernel/user split (KERNEL_OFFSET)                             │    │ |
|  │  │  • Process address space layout                                  │    │ |
|  │  │                                                                  │    │ |
|  │  │  Why? Hardware depends on these, can't change without           │    │ |
|  │  │  breaking all existing software                                  │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**刻意可扩展的部分**：
1. **Ops 表**：添加新文件系统、新调度器类，只需填充操作表并注册
2. **通知链**：订阅 CPU 热插拔、内存压力、网络设备事件
3. **插件子系统**：Netfilter 钩子、LSM 安全模块、TCP 拥塞控制算法

**刻意刚性的部分**：
1. **核心数据结构**：`task_struct`、`mm_struct`、`page`、`sk_buff` 不可扩展，因为影响缓存对齐和二进制兼容
2. **系统调用接口**："不破坏用户空间"原则，系统调用一旦存在就永远保持相同语义
3. **内存布局契约**：PAGE_SIZE、内核/用户分割等硬件依赖的契约
