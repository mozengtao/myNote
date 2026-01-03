# WHERE｜源码地图与阅读策略

## 1. 内核源码树的心智地图

```
LINUX KERNEL SOURCE TREE MAP (v3.2)
+=============================================================================+
|                                                                              |
|  linux-3.2/                                                                  |
|  ├── arch/                 ARCHITECTURE-SPECIFIC CODE                        |
|  │   ├── x86/             ★ x86/x86_64 architecture                         |
|  │   │   ├── kernel/      - syscall entry, irq, smp                         |
|  │   │   ├── mm/          - page tables, fault handling                     |
|  │   │   └── boot/        - early boot code                                 |
|  │   ├── arm/             ARM 32-bit                                        |
|  │   └── ...              20+ more architectures                            |
|  │                                                                          |
|  ├── kernel/              CORE KERNEL INFRASTRUCTURE                        |
|  │   ├── sched/           ★ Scheduler (CFS, RT, deadline)                   |
|  │   │   ├── core.c       - schedule(), context_switch()                    |
|  │   │   ├── fair.c       - CFS implementation                              |
|  │   │   └── rt.c         - Real-time scheduler                             |
|  │   ├── fork.c           ★ Process creation (fork, clone)                  |
|  │   ├── exit.c           Process termination                               |
|  │   ├── signal.c         Signal delivery                                   |
|  │   ├── time/            Timekeeping, timers                               |
|  │   ├── irq/             IRQ handling infrastructure                       |
|  │   ├── workqueue.c      Deferred work processing                          |
|  │   └── trace/           ★ Tracing infrastructure (ftrace, kprobes)        |
|  │                                                                          |
|  ├── mm/                  MEMORY MANAGEMENT                                  |
|  │   ├── memory.c         ★ Page fault handling                             |
|  │   ├── mmap.c           ★ Memory mapping (mmap, munmap)                   |
|  │   ├── page_alloc.c     Buddy allocator                                   |
|  │   ├── slub.c           ★ SLUB slab allocator                             |
|  │   ├── vmalloc.c        Non-contiguous allocation                         |
|  │   ├── swap.c           Swap management                                   |
|  │   ├── vmscan.c         Page reclaim                                      |
|  │   └── rmap.c           Reverse mapping                                   |
|  │                                                                          |
|  ├── fs/                  VFS AND FILESYSTEMS                                |
|  │   ├── namei.c          ★ Path lookup                                     |
|  │   ├── open.c           File open/close                                   |
|  │   ├── read_write.c     ★ vfs_read(), vfs_write()                         |
|  │   ├── file_table.c     File descriptor management                        |
|  │   ├── dcache.c         ★ Dentry cache                                    |
|  │   ├── inode.c          ★ Inode management                                |
|  │   ├── super.c          Superblock operations                             |
|  │   ├── ext4/            ★ ext4 filesystem                                 |
|  │   ├── xfs/             XFS filesystem                                    |
|  │   ├── nfs/             NFS client                                        |
|  │   ├── proc/            /proc filesystem                                  |
|  │   └── sysfs/           /sys filesystem                                   |
|  │                                                                          |
|  ├── net/                 NETWORKING STACK                                   |
|  │   ├── core/            ★ Core networking                                 |
|  │   │   ├── dev.c        - netif_receive_skb(), dev_queue_xmit()           |
|  │   │   ├── sock.c       - Socket layer                                    |
|  │   │   └── skbuff.c     - sk_buff operations                              |
|  │   ├── socket.c         ★ Socket syscalls                                 |
|  │   ├── ipv4/            ★ IPv4 stack                                      |
|  │   │   ├── tcp.c        - TCP socket operations                           |
|  │   │   ├── tcp_input.c  - TCP receive                                     |
|  │   │   ├── tcp_output.c - TCP send                                        |
|  │   │   ├── ip_input.c   - ip_rcv()                                        |
|  │   │   └── ip_output.c  - ip_queue_xmit()                                 |
|  │   ├── ipv6/            IPv6 stack                                        |
|  │   ├── netfilter/       Packet filtering framework                        |
|  │   └── sched/           Traffic control (qdisc)                           |
|  │                                                                          |
|  ├── drivers/             DEVICE DRIVERS                                     |
|  │   ├── base/            ★ Driver model core                               |
|  │   │   ├── core.c       - struct device management                        |
|  │   │   ├── driver.c     - struct device_driver                            |
|  │   │   └── bus.c        - struct bus_type                                 |
|  │   ├── pci/             PCI bus driver                                    |
|  │   ├── usb/             USB subsystem                                     |
|  │   ├── net/             Network drivers                                   |
|  │   │   └── ethernet/    Ethernet drivers (e1000e, ixgbe, ...)            |
|  │   ├── block/           Block device drivers                              |
|  │   ├── char/            Character device drivers                          |
|  │   ├── gpu/             GPU/DRM drivers                                   |
|  │   └── ...              1000+ more driver directories                     |
|  │                                                                          |
|  ├── security/            SECURITY FRAMEWORK                                 |
|  │   ├── security.c       ★ LSM hook dispatch                               |
|  │   ├── selinux/         SELinux implementation                            |
|  │   ├── apparmor/        AppArmor implementation                           |
|  │   └── capability.c     POSIX capabilities                                |
|  │                                                                          |
|  ├── block/               BLOCK LAYER                                        |
|  │   ├── blk-core.c       ★ Block I/O core                                  |
|  │   ├── elevator.c       I/O schedulers                                    |
|  │   └── bio.c            bio structure management                          |
|  │                                                                          |
|  ├── include/             HEADER FILES                                       |
|  │   ├── linux/           ★ Core kernel headers                             |
|  │   │   ├── sched.h      - task_struct, scheduler                          |
|  │   │   ├── fs.h         - file, inode, dentry                             |
|  │   │   ├── mm.h         - memory management                               |
|  │   │   ├── skbuff.h     - sk_buff                                         |
|  │   │   ├── netdevice.h  - net_device                                      |
|  │   │   └── device.h     - device, driver                                  |
|  │   ├── net/             Networking headers                                |
|  │   ├── asm-generic/     Generic arch headers                              |
|  │   └── uapi/            Userspace API headers                             |
|  │                                                                          |
|  ├── lib/                 KERNEL LIBRARY                                     |
|  │   ├── string.c         String functions                                  |
|  │   ├── vsprintf.c       printf implementation                             |
|  │   ├── rbtree.c         Red-black tree                                    |
|  │   └── radix-tree.c     Radix tree                                        |
|  │                                                                          |
|  ├── init/                KERNEL INITIALIZATION                              |
|  │   └── main.c           ★ start_kernel(), boot sequence                   |
|  │                                                                          |
|  └── Documentation/       DOCUMENTATION                                      |
|      ├── filesystems/     Filesystem documentation                          |
|      ├── networking/      Networking documentation                          |
|      └── ...              Per-subsystem docs                                |
|                                                                              |
|  ★ = Primary entry point for understanding                                   |
|                                                                              |
+=============================================================================+
```

**中文说明：**

内核源码树按功能划分：
- **`arch/`**：架构特定代码（x86、ARM 等），包含系统调用入口、页表管理
- **`kernel/`**：核心基础设施——调度器、进程管理、信号、中断
- **`mm/`**：内存管理——页故障、mmap、slab 分配器、交换
- **`fs/`**：VFS 和文件系统——路径查找、inode、dcache、具体 FS 实现
- **`net/`**：网络栈——socket、TCP/IP、Netfilter
- **`drivers/`**：设备驱动——驱动模型核心、PCI/USB 等总线驱动、各类设备驱动
- **`security/`**：安全框架——LSM、SELinux、AppArmor
- **`block/`**：块层——I/O 调度、bio 管理
- **`include/`**：头文件——核心数据结构定义

---

## 2. 跨子系统的架构锚点结构

```
CROSS-SUBSYSTEM ARCHITECTURAL ANCHORS
+=============================================================================+
|                                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  task_struct: THE CENTRAL HUB                                            │ |
|  │  ─────────────────────────────                                           │ |
|  │                                                                          │ |
|  │  Connects:                                                               │ |
|  │  • Scheduler (sched_entity, sched_class)                                │ |
|  │  • Memory (mm_struct)                                                   │ |
|  │  • Files (files_struct, fs_struct)                                      │ |
|  │  • Signals (signal_struct, sighand_struct)                              │ |
|  │  • Credentials (cred)                                                   │ |
|  │  • Namespaces (nsproxy)                                                 │ |
|  │                                                                          │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐   │ |
|  │  │                                                                    │   │ |
|  │  │                      task_struct                                   │   │ |
|  │  │                           │                                        │   │ |
|  │  │    ┌──────────┬──────────┼──────────┬──────────┬──────────┐       │   │ |
|  │  │    ▼          ▼          ▼          ▼          ▼          ▼       │   │ |
|  │  │  sched     mm_struct  files     signal     cred      nsproxy      │   │ |
|  │  │  entity               struct     struct                           │   │ |
|  │  │    │          │          │          │          │          │       │   │ |
|  │  │    ▼          ▼          ▼          ▼          ▼          ▼       │   │ |
|  │  │  CFS RB   vm_area    file      sighand    uid/gid   net_ns       │   │ |
|  │  │  tree     list       array     struct     caps      mnt_ns       │   │ |
|  │  │                        │                             pid_ns       │   │ |
|  │  │                        ▼                                          │   │ |
|  │  │                     dentry/inode                                   │   │ |
|  │  │                                                                    │   │ |
|  │  └──────────────────────────────────────────────────────────────────┘   │ |
|  │                                                                          │ |
|  │  Location: include/linux/sched.h                                        │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  struct page: PHYSICAL MEMORY ANCHOR                                     │ |
|  │  ───────────────────────────────────                                     │ |
|  │                                                                          │ |
|  │  Connects:                                                               │ |
|  │  • Page cache (address_space)                                           │ |
|  │  • Slab allocator                                                       │ |
|  │  • Anonymous memory (swap)                                              │ |
|  │  • DMA (device drivers)                                                 │ |
|  │  • Page reclaim (LRU lists)                                             │ |
|  │                                                                          │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐   │ |
|  │  │                                                                    │   │ |
|  │  │                       struct page                                  │   │ |
|  │  │                           │                                        │   │ |
|  │  │    ┌──────────────────────┼──────────────────────┐                │   │ |
|  │  │    │                      │                      │                │   │ |
|  │  │    ▼                      ▼                      ▼                │   │ |
|  │  │  mapping ──► address_space    OR    s_mem ──► slab object        │   │ |
|  │  │    │              │                                               │   │ |
|  │  │    ▼              ▼                                               │   │ |
|  │  │  inode      page cache tree                                       │   │ |
|  │  │    │                                                              │   │ |
|  │  │    ▼                                                              │   │ |
|  │  │  filesystem                                                       │   │ |
|  │  │                                                                    │   │ |
|  │  └──────────────────────────────────────────────────────────────────┘   │ |
|  │                                                                          │ |
|  │  Location: include/linux/mm_types.h                                     │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  struct inode: VFS ANCHOR                                                │ |
|  │  ────────────────────────                                                │ |
|  │                                                                          │ |
|  │  Connects:                                                               │ |
|  │  • VFS (file_operations, inode_operations)                              │ |
|  │  • Page cache (address_space)                                           │ |
|  │  • Filesystem specific data (i_private)                                 │ |
|  │  • Security (i_security for LSM)                                        │ |
|  │  • Block devices (i_bdev for block special files)                       │ |
|  │                                                                          │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐   │ |
|  │  │                                                                    │   │ |
|  │  │                       struct inode                                 │   │ |
|  │  │                           │                                        │   │ |
|  │  │    ┌──────────┬──────────┼──────────┬──────────┬──────────┐       │   │ |
|  │  │    ▼          ▼          ▼          ▼          ▼          ▼       │   │ |
|  │  │  i_op      i_fop    i_mapping    i_sb      i_security  i_private │   │ |
|  │  │    │          │          │          │          │          │       │   │ |
|  │  │    ▼          ▼          ▼          ▼          ▼          ▼       │   │ |
|  │  │  inode    file     address    super    SELinux   ext4_inode      │   │ |
|  │  │  ops      ops      space      block    context   info            │   │ |
|  │  │                                                                    │   │ |
|  │  └──────────────────────────────────────────────────────────────────┘   │ |
|  │                                                                          │ |
|  │  Location: include/linux/fs.h                                           │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  struct sk_buff: NETWORKING ANCHOR                                       │ |
|  │  ─────────────────────────────────                                       │ |
|  │                                                                          │ |
|  │  Connects:                                                               │ |
|  │  • Socket (sk)                                                          │ |
|  │  • Network device (dev)                                                 │ |
|  │  • Protocol headers (transport, network, mac)                           │ |
|  │  • Routing (dst_entry)                                                  │ |
|  │  • Netfilter (nf_bridge, nf_trace)                                      │ |
|  │                                                                          │ |
|  │  Location: include/linux/skbuff.h                                       │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  struct device: DRIVER MODEL ANCHOR                                      │ |
|  │  ──────────────────────────────────                                      │ |
|  │                                                                          │ |
|  │  Connects:                                                               │ |
|  │  • Bus (bus_type)                                                       │ |
|  │  • Driver (device_driver)                                               │ |
|  │  • Parent/child hierarchy                                               │ |
|  │  • Power management (pm_ops)                                            │ |
|  │  • sysfs representation                                                 │ |
|  │                                                                          │ |
|  │  Location: include/linux/device.h                                       │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

跨子系统的架构锚点结构：

1. **`task_struct`**：中央枢纽，连接调度器、内存、文件、信号、凭证、命名空间

2. **`struct page`**：物理内存锚点，连接页缓存、slab 分配器、匿名内存、DMA、页回收

3. **`struct inode`**：VFS 锚点，连接文件操作、inode 操作、页缓存、文件系统特定数据、安全上下文

4. **`struct sk_buff`**：网络锚点，连接 socket、网络设备、协议头、路由、Netfilter

5. **`struct device`**：驱动模型锚点，连接总线、驱动、设备层次、电源管理、sysfs

---

## 3. 系统级控制枢纽函数

```
SYSTEM-LEVEL CONTROL HUB FUNCTIONS
+=============================================================================+
|                                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  schedule() - SCHEDULER ENTRY POINT                                      │ |
|  │  ──────────────────────────────────                                      │ |
|  │                                                                          │ |
|  │  Location: kernel/sched/core.c                                          │ |
|  │                                                                          │ |
|  │  Purpose: Select next task to run and switch to it                      │ |
|  │                                                                          │ |
|  │  Key operations:                                                         │ |
|  │  • Disable preemption                                                   │ |
|  │  • Call pick_next_task() through sched_class chain                     │ |
|  │  • Perform context_switch() if different task selected                  │ |
|  │                                                                          │ |
|  │  Called from:                                                            │ |
|  │  • Voluntarily: schedule() directly, wait_event(), mutex_lock()         │ |
|  │  • Involuntarily: preemption at syscall return, interrupt return        │ |
|  │                                                                          │ |
|  │  Reading strategy:                                                       │ |
|  │  1. Start at __schedule() in kernel/sched/core.c                        │ |
|  │  2. Follow pick_next_task() → sched_class->pick_next_task()             │ |
|  │  3. Trace context_switch() → switch_mm(), switch_to()                   │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  do_page_fault() - PAGE FAULT HANDLER                                    │ |
|  │  ────────────────────────────────────                                    │ |
|  │                                                                          │ |
|  │  Location: arch/x86/mm/fault.c                                          │ |
|  │                                                                          │ |
|  │  Purpose: Handle CPU page faults                                         │ |
|  │                                                                          │ |
|  │  Key operations:                                                         │ |
|  │  • Determine if address is valid (in VMA)                               │ |
|  │  • Check access permissions                                              │ |
|  │  • Call handle_mm_fault() for actual page handling                      │ |
|  │  • Send SIGSEGV if invalid                                              │ |
|  │                                                                          │ |
|  │  Called from:                                                            │ |
|  │  • CPU exception handler (IDT entry 14)                                 │ |
|  │                                                                          │ |
|  │  Reading strategy:                                                       │ |
|  │  1. Start at do_page_fault() in arch/x86/mm/fault.c                    │ |
|  │  2. Follow __do_page_fault() for main logic                            │ |
|  │  3. Trace handle_mm_fault() in mm/memory.c                             │ |
|  │  4. Understand fast path (filemap_fault) vs slow path (readpage)       │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  vfs_read() / vfs_write() - VFS ENTRY POINTS                             │ |
|  │  ───────────────────────────────────────────                             │ |
|  │                                                                          │ |
|  │  Location: fs/read_write.c                                              │ |
|  │                                                                          │ |
|  │  Purpose: Central dispatch for all file I/O                              │ |
|  │                                                                          │ |
|  │  Key operations:                                                         │ |
|  │  • Security check (security_file_permission)                            │ |
|  │  • Dispatch to file->f_op->read/write                                   │ |
|  │  • Handle return values and errors                                      │ |
|  │                                                                          │ |
|  │  Called from:                                                            │ |
|  │  • sys_read(), sys_write()                                              │ |
|  │  • pread/pwrite variants                                                │ |
|  │  • splice, sendfile                                                     │ |
|  │                                                                          │ |
|  │  Reading strategy:                                                       │ |
|  │  1. Start at vfs_read() in fs/read_write.c                             │ |
|  │  2. Follow file->f_op->read() dispatch                                 │ |
|  │  3. Pick one filesystem (ext4) and trace ext4_file_read()              │ |
|  │  4. Understand page cache involvement (do_generic_file_read)            │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  netif_receive_skb() - NETWORK RECEIVE ENTRY                             │ |
|  │  ───────────────────────────────────────────                             │ |
|  │                                                                          │ |
|  │  Location: net/core/dev.c                                               │ |
|  │                                                                          │ |
|  │  Purpose: Entry point for all received packets                          │ |
|  │                                                                          │ |
|  │  Key operations:                                                         │ |
|  │  • RPS/RFS processing (multi-queue distribution)                        │ |
|  │  • Protocol type dispatch (ETH_P_IP → ip_rcv)                           │ |
|  │  • Netfilter hook invocation                                            │ |
|  │                                                                          │ |
|  │  Called from:                                                            │ |
|  │  • Driver NAPI poll functions                                           │ |
|  │                                                                          │ |
|  │  Reading strategy:                                                       │ |
|  │  1. Start at netif_receive_skb() in net/core/dev.c                     │ |
|  │  2. Follow __netif_receive_skb_core() for protocol dispatch            │ |
|  │  3. Trace to ip_rcv() in net/ipv4/ip_input.c                           │ |
|  │  4. Follow tcp_v4_rcv() in net/ipv4/tcp_ipv4.c                         │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  OTHER KEY FUNCTIONS                                                     │ |
|  │  ───────────────────                                                     │ |
|  │                                                                          │ |
|  │  start_kernel()      - Boot sequence (init/main.c)                      │ |
|  │  do_fork()           - Process creation (kernel/fork.c)                 │ |
|  │  do_exit()           - Process termination (kernel/exit.c)              │ |
|  │  do_sys_open()       - File open (fs/open.c)                            │ |
|  │  path_lookupat()     - Path resolution (fs/namei.c)                     │ |
|  │  __alloc_pages_*     - Page allocation (mm/page_alloc.c)                │ |
|  │  kmem_cache_alloc()  - Slab allocation (mm/slub.c)                      │ |
|  │  sys_socket()        - Socket creation (net/socket.c)                   │ |
|  │  tcp_sendmsg()       - TCP send (net/ipv4/tcp.c)                        │ |
|  │  dev_queue_xmit()    - Network transmit (net/core/dev.c)                │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

系统级控制枢纽函数：

1. **`schedule()`**：调度器入口，选择下一个任务并切换
   - 阅读策略：`__schedule()` → `pick_next_task()` → `context_switch()`

2. **`do_page_fault()`**：页故障处理器
   - 阅读策略：`do_page_fault()` → `handle_mm_fault()` → 快速/慢速路径

3. **`vfs_read()` / `vfs_write()`**：VFS 入口
   - 阅读策略：`vfs_read()` → `f_op->read()` → 页缓存

4. **`netif_receive_skb()`**：网络接收入口
   - 阅读策略：`netif_receive_skb()` → 协议分发 → `ip_rcv()` → `tcp_v4_rcv()`

---

## 4. 如何通过源码验证架构意图

```
VALIDATING ARCHITECTURAL INTENT THROUGH CODE
+=============================================================================+
|                                                                              |
|  TECHNIQUE 1: IDENTIFY OWNERSHIP BOUNDARIES (识别所有权边界)                 |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Look for refcounting patterns:                                          │ |
|  │                                                                          │ |
|  │  grep -rn "get_task_mm\|mmput" mm/                                      │ |
|  │  grep -rn "fget\|fput" fs/                                              │ |
|  │  grep -rn "kref_get\|kref_put" drivers/                                 │ |
|  │                                                                          │ |
|  │  Example from fs/file_table.c:                                           │ |
|  │  ┌────────────────────────────────────────────────────────────────┐     │ |
|  │  │  struct file *fget(unsigned int fd)                             │     │ |
|  │  │  {                                                              │     │ |
|  │  │      struct file *file;                                         │     │ |
|  │  │      rcu_read_lock();                                           │     │ |
|  │  │      file = fcheck(fd);                                         │     │ |
|  │  │      if (file) {                                                │     │ |
|  │  │          if (!atomic_long_inc_not_zero(&file->f_count))         │     │ |
|  │  │              file = NULL;                                       │     │ |
|  │  │      }                                                          │     │ |
|  │  │      rcu_read_unlock();                                         │     │ |
|  │  │      return file;                                               │     │ |
|  │  │  }                                                              │     │ |
|  │  │                                                                  │     │ |
|  │  │  // Every fget() MUST have matching fput()                       │     │ |
|  │  │  // This is the ownership contract                               │     │ |
|  │  └────────────────────────────────────────────────────────────────┘     │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  TECHNIQUE 2: TRACE OPS-TABLE DISPATCH (追踪 ops 表分发)                     |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Find the ops table definition:                                          │ |
|  │  grep -rn "struct file_operations" include/linux/fs.h                   │ |
|  │                                                                          │ |
|  │  Find implementations:                                                   │ |
|  │  grep -rn "\.read\s*=" fs/ext4/                                         │ |
|  │  grep -rn "static.*file_operations" fs/                                 │ |
|  │                                                                          │ |
|  │  Trace the dispatch:                                                     │ |
|  │  ┌────────────────────────────────────────────────────────────────┐     │ |
|  │  │  // In fs/read_write.c                                          │     │ |
|  │  │  ssize_t vfs_read(struct file *file, ...)                       │     │ |
|  │  │  {                                                              │     │ |
|  │  │      if (file->f_op->read)                                      │     │ |
|  │  │          ret = file->f_op->read(file, buf, count, pos);         │     │ |
|  │  │                    ▲                                            │     │ |
|  │  │                    │                                            │     │ |
|  │  │          This points to ext4_file_read, nfs_file_read, etc.     │     │ |
|  │  │          depending on what filesystem the file is on            │     │ |
|  │  │  }                                                              │     │ |
|  │  └────────────────────────────────────────────────────────────────┘     │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  TECHNIQUE 3: FOLLOW LIFECYCLE TRANSITIONS (跟踪生命周期转换)                |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Example: Socket lifecycle                                               │ |
|  │                                                                          │ |
|  │  ┌────────────────────────────────────────────────────────────────┐     │ |
|  │  │                                                                  │     │ |
|  │  │  Creation:                                                       │     │ |
|  │  │  sys_socket() → sock_create() → protocol->create()              │     │ |
|  │  │                                                                  │     │ |
|  │  │  Use:                                                            │     │ |
|  │  │  sys_connect() → sock->ops->connect()                           │     │ |
|  │  │  sys_sendto() → sock->ops->sendmsg()                            │     │ |
|  │  │                                                                  │     │ |
|  │  │  Destruction:                                                    │     │ |
|  │  │  sys_close() → sock_release() → sock->ops->release()            │     │ |
|  │  │                                                                  │     │ |
|  │  │  Find with:                                                      │     │ |
|  │  │  grep -rn "\.create\s*=" net/ipv4/af_inet.c                     │     │ |
|  │  │  grep -rn "\.release\s*=" net/ipv4/af_inet.c                    │     │ |
|  │  │                                                                  │     │ |
|  │  └────────────────────────────────────────────────────────────────┘     │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  TECHNIQUE 4: USE TRACING TOOLS (使用追踪工具)                               |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ftrace for function call chains:                                        │ |
|  │  ┌────────────────────────────────────────────────────────────────┐     │ |
|  │  │  # Enable function graph tracer                                  │     │ |
|  │  │  echo function_graph > /sys/kernel/debug/tracing/current_tracer│     │ |
|  │  │  echo vfs_read > /sys/kernel/debug/tracing/set_graph_function   │     │ |
|  │  │  cat /sys/kernel/debug/tracing/trace                            │     │ |
|  │  │                                                                  │     │ |
|  │  │  Output shows exact call chain:                                  │     │ |
|  │  │  vfs_read() {                                                    │     │ |
|  │  │      security_file_permission();                                 │     │ |
|  │  │      ext4_file_read() {                                          │     │ |
|  │  │          generic_file_aio_read() {                               │     │ |
|  │  │              ...                                                 │     │ |
|  │  │          }                                                       │     │ |
|  │  │      }                                                           │     │ |
|  │  │  }                                                               │     │ |
|  │  └────────────────────────────────────────────────────────────────┘     │ |
|  │                                                                          │ |
|  │  perf for hot paths:                                                     │ |
|  │  ┌────────────────────────────────────────────────────────────────┐     │ |
|  │  │  perf record -g ./my_workload                                   │     │ |
|  │  │  perf report                                                    │     │ |
|  │  │                                                                  │     │ |
|  │  │  Shows which functions consume most time                        │     │ |
|  │  └────────────────────────────────────────────────────────────────┘     │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

验证架构意图的技术：

1. **识别所有权边界**：搜索引用计数模式（`get/put`、`kref_get/put`）

2. **追踪 ops 表分发**：找到 ops 表定义和各实现，跟踪分发代码

3. **跟踪生命周期转换**：从创建到使用到销毁的完整路径

4. **使用追踪工具**：ftrace 显示调用链，perf 显示热点路径

---

## 5. 建议阅读顺序

```
SUGGESTED READING ORDER (MACRO TO MICRO)
+=============================================================================+
|                                                                              |
|  PHASE 1: CORE ABSTRACTIONS (核心抽象)                                       |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  1. include/linux/                                                       │ |
|  │     ├── sched.h         task_struct, scheduler                          │ |
|  │     ├── fs.h            file, inode, dentry, file_operations            │ |
|  │     ├── mm_types.h      mm_struct, vm_area_struct, page                  │ |
|  │     ├── skbuff.h        sk_buff                                         │ |
|  │     ├── netdevice.h     net_device, net_device_ops                      │ |
|  │     └── device.h        device, driver, bus                              │ |
|  │                                                                          │ |
|  │  Goal: Understand the contracts between subsystems                       │ |
|  │  Time: 2-4 hours                                                         │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  PHASE 2: EXECUTION MODEL (执行模型)                                         |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  2. kernel/                                                              │ |
|  │     ├── sched/core.c    schedule(), context_switch()                    │ |
|  │     ├── sched/fair.c    CFS implementation                              │ |
|  │     ├── fork.c          do_fork(), copy_process()                       │ |
|  │     └── exit.c          do_exit()                                        │ |
|  │                                                                          │ |
|  │  Goal: Understand how tasks run and switch                              │ |
|  │  Time: 3-5 hours                                                         │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  PHASE 3: MEMORY MANAGEMENT (内存管理)                                       |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  3. mm/                                                                  │ |
|  │     ├── memory.c        handle_mm_fault(), page fault handling          │ |
|  │     ├── mmap.c          do_mmap(), vm_area management                   │ |
|  │     ├── slub.c          slab allocator                                  │ |
|  │     └── page_alloc.c    buddy allocator                                 │ |
|  │                                                                          │ |
|  │  Goal: Understand virtual → physical mapping                            │ |
|  │  Time: 4-6 hours                                                         │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  PHASE 4: VFS (虚拟文件系统)                                                 |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  4. fs/                                                                  │ |
|  │     ├── read_write.c    vfs_read(), vfs_write()                         │ |
|  │     ├── namei.c         path_lookupat(), path resolution                │ |
|  │     ├── open.c          do_sys_open()                                   │ |
|  │     ├── dcache.c        dentry cache                                    │ |
|  │     └── ext4/           pick one concrete filesystem                    │ |
|  │                                                                          │ |
|  │  Goal: Understand file operations dispatch                              │ |
|  │  Time: 4-6 hours                                                         │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  PHASE 5: NETWORKING (网络)                                                  |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  5. net/                                                                 │ |
|  │     ├── socket.c        sys_socket(), socket layer                      │ |
|  │     ├── core/dev.c      netif_receive_skb(), dev_queue_xmit()           │ |
|  │     ├── ipv4/tcp.c      tcp_sendmsg(), tcp_recvmsg()                    │ |
|  │     └── ipv4/ip_input.c ip_rcv()                                        │ |
|  │                                                                          │ |
|  │  Goal: Understand packet flow through layers                            │ |
|  │  Time: 5-8 hours                                                         │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  PHASE 6: DRIVERS (驱动)                                                     |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  6. drivers/                                                             │ |
|  │     ├── base/core.c     device model                                    │ |
|  │     ├── base/driver.c   driver registration                            │ |
|  │     └── net/ethernet/intel/e1000e/  pick one concrete driver            │ |
|  │                                                                          │ |
|  │  Goal: Understand hw abstraction and driver model                       │ |
|  │  Time: 3-5 hours                                                         │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  PHASE 7: SECURITY (安全)                                                    |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  7. security/                                                            │ |
|  │     ├── security.c      LSM framework                                   │ |
|  │     └── selinux/        one concrete implementation                     │ |
|  │                                                                          │ |
|  │  Goal: Understand security hook architecture                            │ |
|  │  Time: 2-3 hours                                                         │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  PHASE 8: TRACING (追踪)                                                     |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  8. kernel/trace/                                                        │ |
|  │     ├── ftrace.c        function tracer                                 │ |
|  │     └── trace_events.c  tracepoints                                     │ |
|  │                                                                          │ |
|  │  Goal: Understand observability infrastructure                          │ |
|  │  Time: 2-3 hours                                                         │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  TOTAL ESTIMATED TIME: 25-40 hours for architectural understanding          |
|                                                                              |
+=============================================================================+
```

**中文说明：**

建议阅读顺序（从宏观到微观）：

1. **核心抽象**（2-4 小时）：头文件中的核心数据结构
2. **执行模型**（3-5 小时）：调度器、fork、exit
3. **内存管理**（4-6 小时）：页故障、mmap、分配器
4. **VFS**（4-6 小时）：读写、路径解析、dentry 缓存
5. **网络**（5-8 小时）：socket、TCP/IP、设备层
6. **驱动**（3-5 小时）：驱动模型、具体驱动
7. **安全**（2-3 小时）：LSM 框架
8. **追踪**（2-3 小时）：ftrace、tracepoints

**总计**：25-40 小时可建立架构级理解
