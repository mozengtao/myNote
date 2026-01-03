# WHAT｜内核中的具体架构形态

## 1. 反复出现的架构模式

```
RECURRING ARCHITECTURAL PATTERNS
+=============================================================================+
|                                                                              |
|  PATTERN 1: LAYERED ARCHITECTURE (分层架构)                                  |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  VFS LAYERING                                                            │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  User Space                                                      │    │ |
|  │  │  ─────────────────────────────────────────────────────────────── │    │ |
|  │  │                          │ syscall                               │    │ |
|  │  │                          ▼                                       │    │ |
|  │  │  ┌─────────────────────────────────────────────────────────────┐│    │ |
|  │  │  │                  VFS LAYER                                   ││    │ |
|  │  │  │  • Path lookup (namei)                                       ││    │ |
|  │  │  │  • File descriptor management                                ││    │ |
|  │  │  │  • Common file operations                                    ││    │ |
|  │  │  └─────────────────────────────────────────────────────────────┘│    │ |
|  │  │                          │ file_operations                       │    │ |
|  │  │                          ▼                                       │    │ |
|  │  │  ┌─────────────────────────────────────────────────────────────┐│    │ |
|  │  │  │              FILESYSTEM LAYER                                ││    │ |
|  │  │  │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐            ││    │ |
|  │  │  │  │  ext4   │ │   xfs   │ │  btrfs  │ │   nfs   │            ││    │ |
|  │  │  │  └─────────┘ └─────────┘ └─────────┘ └─────────┘            ││    │ |
|  │  │  └─────────────────────────────────────────────────────────────┘│    │ |
|  │  │                          │ address_space_operations              │    │ |
|  │  │                          ▼                                       │    │ |
|  │  │  ┌─────────────────────────────────────────────────────────────┐│    │ |
|  │  │  │                PAGE CACHE LAYER                              ││    │ |
|  │  │  │  • Read/write buffering                                      ││    │ |
|  │  │  │  • Writeback                                                 ││    │ |
|  │  │  │  • Memory mapping                                            ││    │ |
|  │  │  └─────────────────────────────────────────────────────────────┘│    │ |
|  │  │                          │ bio / make_request                    │    │ |
|  │  │                          ▼                                       │    │ |
|  │  │  ┌─────────────────────────────────────────────────────────────┐│    │ |
|  │  │  │                  BLOCK LAYER                                 ││    │ |
|  │  │  │  • Request queue                                             ││    │ |
|  │  │  │  • I/O scheduling                                            ││    │ |
|  │  │  │  • Request merging                                           ││    │ |
|  │  │  └─────────────────────────────────────────────────────────────┘│    │ |
|  │  │                          │ block_device_operations               │    │ |
|  │  │                          ▼                                       │    │ |
|  │  │  ┌─────────────────────────────────────────────────────────────┐│    │ |
|  │  │  │              DEVICE DRIVER LAYER                             ││    │ |
|  │  │  │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐            ││    │ |
|  │  │  │  │  SCSI   │ │  NVMe   │ │ virtio  │ │  AHCI   │            ││    │ |
|  │  │  │  └─────────┘ └─────────┘ └─────────┘ └─────────┘            ││    │ |
|  │  │  └─────────────────────────────────────────────────────────────┘│    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  Each layer adds one specific concern:                                   │ |
|  │  VFS: naming, permissions, fd management                                │ |
|  │  FS: on-disk layout, journaling                                         │ |
|  │  Page Cache: buffering, writeback                                       │ |
|  │  Block: queuing, scheduling                                             │ |
|  │  Driver: hardware specifics                                             │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  PATTERN 2: OPS-TABLE-BASED POLYMORPHISM (基于操作表的多态)                  |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  struct file_operations {     // "Interface" definition          │    │ |
|  │  │      loff_t (*llseek)(...);                                      │    │ |
|  │  │      ssize_t (*read)(...);                                       │    │ |
|  │  │      ssize_t (*write)(...);                                      │    │ |
|  │  │      int (*mmap)(...);                                           │    │ |
|  │  │      int (*open)(...);                                           │    │ |
|  │  │      int (*release)(...);                                        │    │ |
|  │  │      int (*fsync)(...);                                          │    │ |
|  │  │      // ... 30+ more operations                                  │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  // "Implementation" - each filesystem provides its own          │    │ |
|  │  │  const struct file_operations ext4_file_operations = {           │    │ |
|  │  │      .llseek  = ext4_llseek,                                     │    │ |
|  │  │      .read    = do_sync_read,                                    │    │ |
|  │  │      .aio_read = ext4_file_read,                                 │    │ |
|  │  │      .write   = do_sync_write,                                   │    │ |
|  │  │      .mmap    = ext4_file_mmap,                                  │    │ |
|  │  │      .open    = ext4_file_open,                                  │    │ |
|  │  │      .fsync   = ext4_sync_file,                                  │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  // Polymorphic dispatch                                         │    │ |
|  │  │  ssize_t vfs_read(struct file *file, ...) {                      │    │ |
|  │  │      if (file->f_op->read)                                       │    │ |
|  │  │          return file->f_op->read(file, buf, count, pos);         │    │ |
|  │  │      return -EINVAL;                                             │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  This pattern appears in:                                                │ |
|  │  • file_operations      (VFS → filesystem)                              │ |
|  │  • inode_operations     (VFS → filesystem)                              │ |
|  │  • address_space_ops    (page cache → filesystem)                       │ |
|  │  • net_device_ops       (network core → driver)                         │ |
|  │  • proto_ops            (socket → protocol)                             │ |
|  │  • block_device_ops     (block layer → driver)                          │ |
|  │  • sched_class          (scheduler → policy)                            │ |
|  │  • security_operations  (VFS/net/... → LSM)                             │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  PATTERN 3: EVENT AND HOOK SYSTEMS (事件和钩子系统)                          |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  NOTIFIER CHAINS                                                         │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Publisher:                                                      │    │ |
|  │  │  blocking_notifier_call_chain(&cpu_chain, CPU_ONLINE, hcpu);    │    │ |
|  │  │                                                                  │    │ |
|  │  │  Subscribers:                                                    │    │ |
|  │  │  ┌──────────────────────────────────────────────────────────┐   │    │ |
|  │  │  │  cpu_chain                                                │   │    │ |
|  │  │  │  ────────────────────────────────────────────────────────│   │    │ |
|  │  │  │  [sched_cpu_notifier] → [workqueue_cpu_notifier] →       │   │    │ |
|  │  │  │  [perf_cpu_notifier]  → [rcu_cpu_notify] → ...           │   │    │ |
|  │  │  └──────────────────────────────────────────────────────────┘   │    │ |
|  │  │                                                                  │    │ |
|  │  │  Common notifier chains:                                         │    │ |
|  │  │  • cpu_chain        - CPU hotplug events                        │    │ |
|  │  │  • netdev_chain     - network device events                     │    │ |
|  │  │  • inetaddr_chain   - IP address changes                        │    │ |
|  │  │  • reboot_notifier  - system shutdown                           │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  NETFILTER HOOKS                                                         │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Packet flow with hooks:                                         │    │ |
|  │  │                                                                  │    │ |
|  │  │  IN ──► [PRE_ROUTING] ──► Routing ──┬──► [LOCAL_IN] ──► Local  │    │ |
|  │  │                                     │                           │    │ |
|  │  │                                     └──► [FORWARD] ──┐          │    │ |
|  │  │                                                      │          │    │ |
|  │  │  Local ──► [LOCAL_OUT] ──► Routing ──► [POST_ROUTING] ◄─┘ ──► OUT│    │ |
|  │  │                                                                  │    │ |
|  │  │  At each hook point:                                             │    │ |
|  │  │  NF_ACCEPT  - continue to next hook                              │    │ |
|  │  │  NF_DROP    - discard packet                                     │    │ |
|  │  │  NF_QUEUE   - pass to userspace                                  │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  PATTERN 4: PIPELINE-BASED PROCESSING (基于管道的处理)                       |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  NETWORK RECEIVE PIPELINE                                                │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  NIC Hardware                                                    │    │ |
|  │  │      │                                                           │    │ |
|  │  │      ▼ IRQ                                                       │    │ |
|  │  │  ┌──────────┐                                                    │    │ |
|  │  │  │ Driver   │ NAPI poll, alloc skb                               │    │ |
|  │  │  └────┬─────┘                                                    │    │ |
|  │  │       ▼                                                          │    │ |
|  │  │  ┌──────────┐                                                    │    │ |
|  │  │  │netif_    │ RPS/RFS distribution                               │    │ |
|  │  │  │receive_  │                                                    │    │ |
|  │  │  │skb       │                                                    │    │ |
|  │  │  └────┬─────┘                                                    │    │ |
|  │  │       ▼ protocol dispatch                                        │    │ |
|  │  │  ┌──────────┐                                                    │    │ |
|  │  │  │ ip_rcv   │ IP layer processing                                │    │ |
|  │  │  └────┬─────┘                                                    │    │ |
|  │  │       ▼ NF_INET_PRE_ROUTING hook                                 │    │ |
|  │  │  ┌──────────┐                                                    │    │ |
|  │  │  │ routing  │ Local vs forward decision                          │    │ |
|  │  │  └────┬─────┘                                                    │    │ |
|  │  │       ▼                                                          │    │ |
|  │  │  ┌──────────┐                                                    │    │ |
|  │  │  │tcp_v4_rcv│ TCP state machine                                  │    │ |
|  │  │  └────┬─────┘                                                    │    │ |
|  │  │       ▼                                                          │    │ |
|  │  │  ┌──────────┐                                                    │    │ |
|  │  │  │ socket   │ Deliver to user                                    │    │ |
|  │  │  │ queue    │                                                    │    │ |
|  │  │  └──────────┘                                                    │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  Each stage can:                                                         │ |
|  │  • Transform the packet (add/remove headers)                            │ |
|  │  • Drop the packet                                                      │ |
|  │  • Queue for later processing                                           │ |
|  │  • Redirect to different path                                           │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

四种反复出现的架构模式：

1. **分层架构**：VFS 展示了完美的分层——每层只关注一个问题（VFS：命名/权限，文件系统：磁盘布局，页缓存：缓冲，块层：调度，驱动：硬件）

2. **基于操作表的多态**：`file_operations`、`net_device_ops` 等使用函数指针表实现 C 语言多态，是内核中最普遍的模式

3. **事件和钩子系统**：通知链用于订阅系统事件（CPU 热插拔、网络设备变化），Netfilter 钩子用于数据包过滤

4. **基于管道的处理**：网络接收路径展示了数据如何流经多个处理阶段，每阶段可以转换、丢弃、排队或重定向

---

## 2. 定义架构的核心数据结构

```
ARCHITECTURE-DEFINING DATA STRUCTURES
+=============================================================================+
|                                                                              |
|  PROCESS/EXECUTION CONTEXT                                                   |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  struct task_struct (include/linux/sched.h)                              │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  THE central data structure - represents a thread of execution   │    │ |
|  │  │                                                                  │    │ |
|  │  │  struct task_struct {                                            │    │ |
|  │  │      // SCHEDULING                                               │    │ |
|  │  │      volatile long state;           // TASK_RUNNING, etc.        │    │ |
|  │  │      int prio, static_prio;         // priority                  │    │ |
|  │  │      struct sched_entity se;        // CFS scheduling entity     │    │ |
|  │  │      struct sched_rt_entity rt;     // RT scheduling entity      │    │ |
|  │  │      const struct sched_class *sched_class; // scheduler policy  │    │ |
|  │  │                                                                  │    │ |
|  │  │      // MEMORY                                                   │    │ |
|  │  │      struct mm_struct *mm;          // address space             │    │ |
|  │  │      struct mm_struct *active_mm;   // active mm for kernel      │    │ |
|  │  │                                                                  │    │ |
|  │  │      // FILES                                                    │    │ |
|  │  │      struct files_struct *files;    // open file table           │    │ |
|  │  │      struct fs_struct *fs;          // cwd, root                 │    │ |
|  │  │                                                                  │    │ |
|  │  │      // SIGNALS                                                  │    │ |
|  │  │      struct signal_struct *signal;  // shared by thread group    │    │ |
|  │  │      struct sighand_struct *sighand;// signal handlers           │    │ |
|  │  │                                                                  │    │ |
|  │  │      // CREDENTIALS                                              │    │ |
|  │  │      const struct cred *cred;       // uid, gid, capabilities    │    │ |
|  │  │                                                                  │    │ |
|  │  │      // NAMESPACES (for containers)                              │    │ |
|  │  │      struct nsproxy *nsproxy;       // namespace references      │    │ |
|  │  │                                                                  │    │ |
|  │  │      // HIERARCHY                                                │    │ |
|  │  │      struct task_struct *parent;    // parent process            │    │ |
|  │  │      struct list_head children;     // child processes           │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  Size: ~1.5KB (varies with CONFIG options)                       │    │ |
|  │  │  Allocation: kernel stack + task_struct from slab                │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  MEMORY MANAGEMENT                                                           |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  struct mm_struct / vm_area_struct (include/linux/mm_types.h)           │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  mm_struct: One per process address space                        │    │ |
|  │  │                                                                  │    │ |
|  │  │  struct mm_struct {                                              │    │ |
|  │  │      struct vm_area_struct *mmap;   // VMA list                  │    │ |
|  │  │      struct rb_root mm_rb;          // VMA red-black tree        │    │ |
|  │  │      pgd_t *pgd;                    // page directory            │    │ |
|  │  │      atomic_t mm_users;             // process references        │    │ |
|  │  │      atomic_t mm_count;             // total references          │    │ |
|  │  │      unsigned long start_code, end_code;                         │    │ |
|  │  │      unsigned long start_brk, brk;  // heap                      │    │ |
|  │  │      unsigned long start_stack;                                  │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  vm_area_struct: One per mapped region                           │    │ |
|  │  │                                                                  │    │ |
|  │  │  struct vm_area_struct {                                         │    │ |
|  │  │      unsigned long vm_start, vm_end;  // virtual addresses       │    │ |
|  │  │      pgprot_t vm_page_prot;           // protection flags        │    │ |
|  │  │      struct file *vm_file;            // backing file            │    │ |
|  │  │      const struct vm_operations_struct *vm_ops;                  │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  struct page (include/linux/mm_types.h)                                  │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Describes ONE physical page (4KB typically)                     │    │ |
|  │  │                                                                  │    │ |
|  │  │  struct page {                                                   │    │ |
|  │  │      unsigned long flags;           // PG_locked, PG_dirty, etc. │    │ |
|  │  │      atomic_t _count;               // reference count           │    │ |
|  │  │      union {                                                     │    │ |
|  │  │          struct address_space *mapping;  // page cache owner    │    │ |
|  │  │          void *s_mem;               // slab first object        │    │ |
|  │  │      };                                                          │    │ |
|  │  │      pgoff_t index;                 // offset in mapping         │    │ |
|  │  │      struct list_head lru;          // for page reclaim          │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  Size: ~64 bytes - CRITICAL to keep small (millions of pages)    │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  VFS / FILE SYSTEMS                                                          |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  struct file, inode, dentry (include/linux/fs.h)                        │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  VFS Object Relationships:                                       │    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌──────────────────┐                                            │    │ |
|  │  │  │   struct file    │  Per open file descriptor                  │    │ |
|  │  │  │  ─────────────── │                                            │    │ |
|  │  │  │  f_pos           │  current position                          │    │ |
|  │  │  │  f_flags         │  O_RDONLY, O_NONBLOCK, etc.                │    │ |
|  │  │  │  f_op            │  → file_operations                         │    │ |
|  │  │  └────────┬─────────┘                                            │    │ |
|  │  │           │ references                                           │    │ |
|  │  │           ▼                                                      │    │ |
|  │  │  ┌──────────────────┐                                            │    │ |
|  │  │  │  struct dentry   │  Per pathname component (cached)           │    │ |
|  │  │  │  ─────────────── │                                            │    │ |
|  │  │  │  d_name          │  component name                            │    │ |
|  │  │  │  d_parent        │  parent directory                          │    │ |
|  │  │  │  d_inode         │  → inode                                   │    │ |
|  │  │  └────────┬─────────┘                                            │    │ |
|  │  │           │ references                                           │    │ |
|  │  │           ▼                                                      │    │ |
|  │  │  ┌──────────────────┐                                            │    │ |
|  │  │  │  struct inode    │  Per file (unique in filesystem)           │    │ |
|  │  │  │  ─────────────── │                                            │    │ |
|  │  │  │  i_mode          │  permissions, file type                    │    │ |
|  │  │  │  i_size          │  file size                                 │    │ |
|  │  │  │  i_op            │  → inode_operations                        │    │ |
|  │  │  │  i_fop           │  → file_operations                         │    │ |
|  │  │  │  i_mapping       │  → address_space (page cache)              │    │ |
|  │  │  └──────────────────┘                                            │    │ |
|  │  │                                                                  │    │ |
|  │  │  Multiple files → one dentry → one inode (hard links)            │    │ |
|  │  │  Same file opened twice → two files → one dentry → one inode     │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  NETWORKING                                                                  |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  struct sk_buff, net_device (include/linux/skbuff.h, netdevice.h)       │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  sk_buff: THE packet buffer                                      │    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌─────────────────────────────────────────────────────────┐    │    │ |
|  │  │  │  struct sk_buff                                          │    │    │ |
|  │  │  │  ────────────────                                        │    │    │ |
|  │  │  │  head ──────────► [buffer start    ]                     │    │    │ |
|  │  │  │  data ──────────► [  ethernet hdr  ]                     │    │    │ |
|  │  │  │                   [   IP header    ]                     │    │    │ |
|  │  │  │                   [  TCP header    ]                     │    │    │ |
|  │  │  │                   [    payload     ]                     │    │    │ |
|  │  │  │  tail ──────────► [               ]                      │    │    │ |
|  │  │  │  end  ──────────► [buffer end      ]                     │    │    │ |
|  │  │  │                                                          │    │    │ |
|  │  │  │  sk ──────────────► struct sock (owning socket)          │    │    │ |
|  │  │  │  dev ─────────────► struct net_device                    │    │    │ |
|  │  │  │  protocol ────────► ETH_P_IP, ETH_P_ARP, etc.            │    │    │ |
|  │  │  │  users ───────────► reference count                      │    │    │ |
|  │  │  │                                                          │    │    │ |
|  │  │  └─────────────────────────────────────────────────────────┘    │    │ |
|  │  │                                                                  │    │ |
|  │  │  net_device: Network interface abstraction                       │    │ |
|  │  │                                                                  │    │ |
|  │  │  struct net_device {                                             │    │ |
|  │  │      char name[IFNAMSIZ];           // "eth0"                    │    │ |
|  │  │      unsigned int flags;            // IFF_UP, IFF_RUNNING       │    │ |
|  │  │      unsigned int mtu;                                           │    │ |
|  │  │      const struct net_device_ops *netdev_ops;                    │    │ |
|  │  │      struct netdev_queue *_tx;      // transmit queues           │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  DEVICE MODEL                                                                |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  struct device, driver (include/linux/device.h)                         │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Device Model Hierarchy:                                         │    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌────────────────┐                                              │    │ |
|  │  │  │   struct bus   │  PCI, USB, platform, I2C, SPI                │    │ |
|  │  │  └───────┬────────┘                                              │    │ |
|  │  │          │                                                       │    │ |
|  │  │     ┌────┴────┐                                                  │    │ |
|  │  │     │         │                                                  │    │ |
|  │  │     ▼         ▼                                                  │    │ |
|  │  │  ┌────────┐ ┌────────┐                                           │    │ |
|  │  │  │device  │ │driver  │                                           │    │ |
|  │  │  │        │ │        │                                           │    │ |
|  │  │  │ bus ───┼─┼→ bus   │                                           │    │ |
|  │  │  │ driver ◄┼─┼─      │                                           │    │ |
|  │  │  └────────┘ └────────┘                                           │    │ |
|  │  │                                                                  │    │ |
|  │  │  struct device {                                                 │    │ |
|  │  │      struct device *parent;                                      │    │ |
|  │  │      struct device_driver *driver;                               │    │ |
|  │  │      struct bus_type *bus;                                       │    │ |
|  │  │      void *platform_data;           // device-specific data      │    │ |
|  │  │      struct device_node *of_node;   // device tree node          │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  struct device_driver {                                          │    │ |
|  │  │      const char *name;                                           │    │ |
|  │  │      struct bus_type *bus;                                       │    │ |
|  │  │      int (*probe)(struct device *dev);                           │    │ |
|  │  │      int (*remove)(struct device *dev);                          │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

定义架构的核心数据结构：

1. **`task_struct`**：最核心的结构，代表一个执行线程，包含调度、内存、文件、信号、凭证、命名空间等所有信息

2. **`mm_struct` / `vm_area_struct` / `page`**：
   - `mm_struct`：进程地址空间
   - `vm_area_struct`：每个映射区域
   - `page`：每个物理页（~64 字节，因为有数百万页必须保持小）

3. **`file` / `dentry` / `inode`**：
   - `file`：每个打开的文件描述符
   - `dentry`：路径组件缓存
   - `inode`：文件元数据，唯一标识一个文件

4. **`sk_buff` / `net_device`**：
   - `sk_buff`：网络数据包容器，包含 head/data/tail/end 指针
   - `net_device`：网络接口抽象

5. **`device` / `driver`**：
   - 设备模型层次：bus → device ↔ driver
   - 支持热插拔、电源管理

---

## 3. 控制流如何组织

```
CONTROL FLOW ORGANIZATION
+=============================================================================+
|                                                                              |
|  SCHEDULER DECISION PATH (调度器决策路径)                                    |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Scheduling Entry Points:                                        │    │ |
|  │  │                                                                  │    │ |
|  │  │  1. Voluntary: schedule() called directly                        │    │ |
|  │  │     • wait_event(), mutex_lock(), I/O blocking                   │    │ |
|  │  │                                                                  │    │ |
|  │  │  2. Preemption: need_resched flag set                           │    │ |
|  │  │     • Timer tick, higher priority wakeup                        │    │ |
|  │  │                                                                  │    │ |
|  │  │  schedule() Flow:                                                │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  schedule()                                                 │ │    │ |
|  │  │  │      │                                                      │ │    │ |
|  │  │  │      ├── put_prev_task(rq, prev)                            │ │    │ |
|  │  │  │      │       └── sched_class->put_prev_task()               │ │    │ |
|  │  │  │      │                                                      │ │    │ |
|  │  │  │      ├── pick_next_task(rq)                                 │ │    │ |
|  │  │  │      │       ├── stop_sched_class  (highest priority)       │ │    │ |
|  │  │  │      │       ├── rt_sched_class    (real-time)              │ │    │ |
|  │  │  │      │       ├── fair_sched_class  (CFS - normal tasks)     │ │    │ |
|  │  │  │      │       └── idle_sched_class  (lowest priority)        │ │    │ |
|  │  │  │      │                                                      │ │    │ |
|  │  │  │      └── context_switch(rq, prev, next)                     │ │    │ |
|  │  │  │              ├── switch_mm()      (address space)           │ │    │ |
|  │  │  │              └── switch_to()      (registers, stack)        │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  PAGE FAULT HANDLING (页故障处理)                                            |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  CPU triggers #PF exception                                      │    │ |
|  │  │      │                                                           │    │ |
|  │  │      ▼                                                           │    │ |
|  │  │  do_page_fault() [arch/x86/mm/fault.c]                           │    │ |
|  │  │      │                                                           │    │ |
|  │  │      ├── Is address valid? (in VMA?)                             │    │ |
|  │  │      │      NO → send SIGSEGV                                    │    │ |
|  │  │      │                                                           │    │ |
|  │  │      ├── Is access permitted? (write to read-only?)              │    │ |
|  │  │      │      NO → send SIGSEGV (or COW)                           │    │ |
|  │  │      │                                                           │    │ |
|  │  │      └── handle_mm_fault()                                       │    │ |
|  │  │              │                                                   │    │ |
|  │  │              ├── FAST PATH: page in page cache                   │    │ |
|  │  │              │      └── just update PTE, done                    │    │ |
|  │  │              │                                                   │    │ |
|  │  │              └── SLOW PATH:                                      │    │ |
|  │  │                      ├── Anonymous page? allocate_page()         │    │ |
|  │  │                      ├── File-backed? read from disk             │    │ |
|  │  │                      ├── Swap? read from swap device             │    │ |
|  │  │                      └── Update page tables                      │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  VFS OPERATION DISPATCH (VFS 操作分发)                                       |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  User calls read(fd, buf, count)                                 │    │ |
|  │  │      │                                                           │    │ |
|  │  │      ▼                                                           │    │ |
|  │  │  sys_read() [fs/read_write.c]                                    │    │ |
|  │  │      │                                                           │    │ |
|  │  │      ├── file = fget(fd)        // fd → struct file              │    │ |
|  │  │      │                                                           │    │ |
|  │  │      └── vfs_read(file, ...)                                     │    │ |
|  │  │              │                                                   │    │ |
|  │  │              ├── security_file_permission()  // LSM check        │    │ |
|  │  │              │                                                   │    │ |
|  │  │              └── file->f_op->read()  // polymorphic dispatch     │    │ |
|  │  │                      │                                           │    │ |
|  │  │                      ├── ext4: ext4_file_read()                  │    │ |
|  │  │                      │      └── generic_file_aio_read()          │    │ |
|  │  │                      │              └── do_generic_file_read()   │    │ |
|  │  │                      │                      └── a_ops->readpage()│    │ |
|  │  │                      │                                           │    │ |
|  │  │                      ├── nfs: nfs_file_read()                    │    │ |
|  │  │                      │      └── nfs_readpage()                   │    │ |
|  │  │                      │              └── RPC to server            │    │ |
|  │  │                      │                                           │    │ |
|  │  │                      └── proc: proc_file_read()                  │    │ |
|  │  │                             └── generates data on-the-fly        │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  PACKET RECEIVE/TRANSMIT (数据包收发)                                        |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  RECEIVE PATH                           TRANSMIT PATH                    │ |
|  │  ┌────────────────────────────┐        ┌────────────────────────────┐   │ |
|  │  │                            │        │                            │   │ |
|  │  │  NIC → Hardware IRQ        │        │  send() syscall            │   │ |
|  │  │         │                  │        │         │                  │   │ |
|  │  │         ▼                  │        │         ▼                  │   │ |
|  │  │  napi_schedule()           │        │  tcp_sendmsg()             │   │ |
|  │  │         │                  │        │         │                  │   │ |
|  │  │         ▼ (softirq)        │        │         ▼                  │   │ |
|  │  │  driver->napi_poll()       │        │  tcp_write_xmit()          │   │ |
|  │  │         │                  │        │         │                  │   │ |
|  │  │         ▼                  │        │         ▼                  │   │ |
|  │  │  netif_receive_skb()       │        │  ip_queue_xmit()           │   │ |
|  │  │         │                  │        │         │                  │   │ |
|  │  │         ▼                  │        │         ▼                  │   │ |
|  │  │  ip_rcv()                  │        │  dev_queue_xmit()          │   │ |
|  │  │         │                  │        │         │                  │   │ |
|  │  │         ▼                  │        │         ▼                  │   │ |
|  │  │  tcp_v4_rcv()              │        │  qdisc->enqueue()          │   │ |
|  │  │         │                  │        │         │                  │   │ |
|  │  │         ▼                  │        │         ▼                  │   │ |
|  │  │  sk_receive_queue          │        │  driver->ndo_start_xmit()  │   │ |
|  │  │         │                  │        │         │                  │   │ |
|  │  │         ▼                  │        │         ▼                  │   │ |
|  │  │  wake up recv()            │        │  DMA to NIC                │   │ |
|  │  │                            │        │                            │   │ |
|  │  └────────────────────────────┘        └────────────────────────────┘   │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

控制流组织的四个关键路径：

1. **调度器决策路径**：
   - 入口：自愿调用 `schedule()` 或抢占（`need_resched` 标志）
   - 流程：`put_prev_task` → `pick_next_task`（按优先级检查各调度类）→ `context_switch`

2. **页故障处理**：
   - CPU 触发 #PF 异常
   - 检查地址有效性和权限
   - 快速路径：页在缓存中，只需更新 PTE
   - 慢速路径：分配页、从磁盘/swap 读取

3. **VFS 操作分发**：
   - `sys_read()` → `vfs_read()` → `file->f_op->read()`
   - 多态分发到具体文件系统（ext4、nfs、proc 等）

4. **网络数据包收发**：
   - 接收：IRQ → NAPI poll → netif_receive_skb → IP → TCP → socket 队列
   - 发送：tcp_sendmsg → IP → dev_queue_xmit → qdisc → driver

---

## 4. 扩展点与约束，模式的代价与边界

```
EXTENSION POINTS, CONSTRAINTS, AND COSTS
+=============================================================================+
|                                                                              |
|  EXTENSION POINTS AND CONSTRAINTS (扩展点与约束)                             |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Extension Point         Constraint                                      │ |
|  │  ─────────────────────   ──────────────────────────────────────────────  │ |
|  │                                                                          │ |
|  │  file_operations         Must implement at least: read/write or mmap    │ |
|  │                          Must handle all error cases                     │ |
|  │                          Cannot change core struct file layout           │ |
|  │                                                                          │ |
|  │  net_device_ops          Must implement: ndo_start_xmit, ndo_open       │ |
|  │                          Must support NAPI if high performance          │ |
|  │                          Cannot bypass netfilter hooks                   │ |
|  │                                                                          │ |
|  │  sched_class             Must fit in class priority chain               │ |
|  │                          Cannot break O(1) or O(log n) guarantees       │ |
|  │                          Must handle migration, load balancing          │ |
|  │                                                                          │ |
|  │  security_operations     All hooks are optional but called in order     │ |
|  │                          Cannot grant more access than DAC allows        │ |
|  │                          Must be fast (on every syscall!)               │ |
|  │                                                                          │ |
|  │  Netfilter hooks         Priority determines ordering                   │ |
|  │                          Cannot hold locks across hooks                 │ |
|  │                          Must return verdict quickly                    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  PATTERN COSTS AND LIMITS (模式代价与边界)                                   |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  1. INDIRECTION OVERHEAD (间接调用开销)                                  │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Direct call:    3-5 cycles                                      │    │ |
|  │  │  Indirect call:  10-15 cycles (function pointer)                 │    │ |
|  │  │  With cache miss: 100+ cycles                                    │    │ |
|  │  │                                                                  │    │ |
|  │  │  Every ops table dispatch is an indirect call                    │    │ |
|  │  │  VFS read: at least 3 indirect calls                             │    │ |
|  │  │    vfs_read → f_op->read → a_ops->readpage                       │    │ |
|  │  │                                                                  │    │ |
|  │  │  Mitigation:                                                     │    │ |
|  │  │  • Inline fast paths                                             │    │ |
|  │  │  • Branch prediction hints                                       │    │ |
|  │  │  • Keep hot function pointers together                           │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  2. CACHE LOCALITY TRADE-OFFS (缓存局部性权衡)                           │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Layered architecture problem:                                   │    │ |
|  │  │                                                                  │    │ |
|  │  │  Layer 1 data ────► Layer 2 data ────► Layer 3 data              │    │ |
|  │  │      │                  │                  │                     │    │ |
|  │  │      └── cache line A   └── cache line B   └── cache line C      │    │ |
|  │  │                                                                  │    │ |
|  │  │  Each layer touch → potential cache miss                         │    │ |
|  │  │                                                                  │    │ |
|  │  │  Mitigation:                                                     │    │ |
|  │  │  • Embed frequently accessed data in parent struct               │    │ |
|  │  │  • Use slab caches to keep similar objects together              │    │ |
|  │  │  • Careful struct layout (hot fields first)                      │    │ |
|  │  │                                                                  │    │ |
|  │  │  Example: sk_buff embeds most-used fields, rarely uses fragments │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  3. HISTORICAL COMPATIBILITY BURDENS (历史兼容性负担)                    │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Syscall ABI frozen forever:                                     │    │ |
|  │  │                                                                  │    │ |
|  │  │  • stat() → stat64() → fstatat() evolution                       │    │ |
|  │  │  • select() → poll() → epoll() evolution                         │    │ |
|  │  │  • Still must support all old versions                           │    │ |
|  │  │                                                                  │    │ |
|  │  │  Struct padding for future expansion:                            │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │  struct inode {                                             │ │    │ |
|  │  │  │      // ... active fields ...                               │ │    │ |
|  │  │  │      void *i_private;    // "just in case" pointer          │ │    │ |
|  │  │  │  };                                                         │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  │  Backward compatible extensions:                                 │    │ |
|  │  │  • CONFIG_COMPAT for 32-bit syscalls on 64-bit                  │    │ |
|  │  │  • Multiple versions of same structure (compat_*)               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  4. COMPLEXITY COST (复杂度成本)                                         │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Layered architecture adds:                                      │    │ |
|  │  │  • More code to understand                                       │    │ |
|  │  │  • More places bugs can hide                                     │    │ |
|  │  │  • Harder to trace execution                                     │    │ |
|  │  │                                                                  │    │ |
|  │  │  VFS read path crosses ~10 functions                             │    │ |
|  │  │  Network receive path crosses ~20 functions                      │    │ |
|  │  │                                                                  │    │ |
|  │  │  Debugging tools needed:                                         │    │ |
|  │  │  • ftrace for function call tracing                              │    │ |
|  │  │  • kprobes for dynamic instrumentation                           │    │ |
|  │  │  • perf for performance analysis                                 │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**扩展点与约束**：
- 每个扩展点都有必须满足的约束（如 `net_device_ops` 必须实现 `ndo_start_xmit`）
- 不能绕过核心安全检查（如 Netfilter 钩子）
- 必须保持性能保证（如调度器不能破坏 O(1) 保证）

**模式代价**：

1. **间接调用开销**：每次 ops 表分发是间接调用（10-15 周期 vs 直接调用 3-5 周期）

2. **缓存局部性权衡**：分层导致跨多个缓存行访问，通过嵌入热字段和 slab 缓存缓解

3. **历史兼容性负担**：系统调用 ABI 永远冻结，必须支持所有旧版本，导致代码膨胀

4. **复杂度成本**：VFS 读路径跨越 ~10 个函数，网络接收 ~20 个函数，需要 ftrace/kprobes 等工具调试
