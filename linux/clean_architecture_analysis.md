# Linux Kernel v3.2 — Clean Architecture Analysis

> A comprehensive analysis of how Clean Architecture principles manifest in the Linux Kernel's
> major subsystems, with concrete source code references, dependency flow diagrams, and
> execution flow traces.

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [Methodology](#2-methodology)
3. [The Kernel's Universal Abstraction Pattern](#3-the-kernels-universal-abstraction-pattern)
4. [Subsystem Analysis](#4-subsystem-analysis)
   - 4.1 [Process Management (Scheduler)](#41-process-management-scheduler)
   - 4.2 [Memory Management](#42-memory-management)
   - 4.3 [Virtual File System (VFS)](#43-virtual-file-system-vfs)
   - 4.4 [Network Stack](#44-network-stack)
   - 4.5 [Device Driver Model](#45-device-driver-model)
   - 4.6 [Security (Linux Security Modules)](#46-security-linux-security-modules)
   - 4.7 [Interrupt Handling](#47-interrupt-handling)
   - 4.8 [Time Management](#48-time-management)
5. [Cross-Subsystem Comparison](#5-cross-subsystem-comparison)
6. [Dependency Violations and Trade-offs](#6-dependency-violations-and-trade-offs)
7. [Key Insights and Conclusions](#7-key-insights-and-conclusions)

---

## 1. Executive Summary

The Linux Kernel v3.2 naturally embodies many Clean Architecture principles, despite predating
Uncle Bob's formal articulation by decades. The kernel achieves this through a consistent
**ops-structure pattern** — structs filled with function pointers that serve as abstract
interfaces, enabling dependency inversion between stable core logic and volatile hardware
implementations.

**Key findings:**

- **7 out of 8** subsystems analyzed follow strict dependency inversion: outer layers
  (drivers, filesystems, security modules) depend on inner layers (core abstractions),
  never the reverse.
- The kernel defines **20+ major ops-structures** acting as interface contracts.
- Execution flow and dependency flow consistently run in **opposite directions** —
  the hallmark of Clean Architecture.
- The **VFS layer** is the purest Clean Architecture implementation, with five distinct
  ops-structures and zero reverse dependencies.
- The **scheduler** is the notable exception: `kernel/sched.c` `#include`s implementation
  files directly, creating a dependency from core to concrete implementations.

---

## 2. Methodology

### 2.1 Analysis Framework

Each subsystem was analyzed along five dimensions:

| Dimension | Question |
|-----------|----------|
| **Layer Identification** | What are the entities, use cases, adapters, and outer implementations? |
| **Dependency Direction** | Do dependencies point inward (outer → inner)? |
| **Execution vs. Dependency** | Do execution flow and dependency flow run opposite? |
| **Abstraction Interfaces** | What ops-structures define the contracts? |
| **Inversion of Control** | How are implementations registered and dispatched? |

### 2.2 Clean Architecture Layer Mapping

```
+------------------------------------------------------------------+
|  Outer Ring: Volatile / Changes Often                            |
|  (drivers/, arch/, fs/ext4/, security/selinux/, net/ipv4/)       |
+------------------------------------------------------------------+
|  Middle Ring: Interface Adapters                                 |
|  (VFS dispatch, socket layer, IRQ generic layer)                 |
+------------------------------------------------------------------+
|  Inner Ring: Core Entities & Use Cases (Stable)                  |
|  (include/linux/*.h abstractions, core algorithms)               |
+------------------------------------------------------------------+
```

---

## 3. The Kernel's Universal Abstraction Pattern

The Linux kernel uses a single, consistent pattern across all subsystems to achieve
dependency inversion. This is the **ops-structure pattern**:

### 3.1 Pattern Definition

```c
/* Step 1: Define abstract interface (inner layer) */
struct xxx_operations {
    int (*open)(...);
    int (*read)(...);
    int (*write)(...);
};

/* Step 2: Implement interface (outer layer) */
static const struct xxx_operations my_ops = {
    .open  = my_open,
    .read  = my_read,
    .write = my_write,
};

/* Step 3: Register implementation */
register_xxx(&my_ops);

/* Step 4: Core dispatches through interface */
ops->read(...);  /* Core never knows the concrete type */
```

### 3.2 Ops-Structures Inventory

| Subsystem | Interface Structure | Location | ~Hooks |
|-----------|-------------------|----------|--------|
| **VFS** | `file_operations` | `include/linux/fs.h:1223` | 25 |
| **VFS** | `inode_operations` | `include/linux/fs.h:1249` | 23 |
| **VFS** | `super_operations` | `include/linux/fs.h:1291` | 22 |
| **VFS** | `dentry_operations` | `include/linux/dcache.h:158` | 10 |
| **VFS** | `address_space_operations` | `include/linux/fs.h:419` | 17 |
| **Scheduler** | `sched_class` | `include/linux/sched.h:834` | 24 |
| **MM** | `vm_operations_struct` | `include/linux/mm.h:204` | 8 |
| **Network** | `proto_ops` | `include/linux/net.h:161` | 20 |
| **Network** | `net_device_ops` | `include/linux/netdevice.h:612` | 40+ |
| **Network** | `proto` | `include/net/sock.h:545` | 30+ |
| **Block** | `block_device_operations` | `include/linux/blkdev.h:951` | 11 |
| **Driver Model** | `bus_type` | `include/linux/device.h:86` | 7 |
| **Security** | `security_operations` | `include/linux/security.h:1380` | ~185 |
| **IRQ** | `irq_chip` | `include/linux/irq.h:295` | 21 |
| **Time** | `clocksource` | `include/linux/clocksource.h:166` | 5 |
| **Time** | `clock_event_device` | `include/linux/clockchips.h:82` | 5 |

---

## 4. Subsystem Analysis

---

### 4.1 Process Management (Scheduler)

#### Clean Architecture Layers

| Layer | Components |
|-------|-----------|
| **Entities** | `task_struct` (`include/linux/sched.h:924-1210`), `sched_entity` (`:899-923`), `sched_rt_entity` (`:925-939`) |
| **Use Cases** | `schedule()`, `__schedule()`, `pick_next_task()`, `context_switch()` in `kernel/sched.c` |
| **Interface Adapter** | `struct sched_class` (`include/linux/sched.h:834-877`) |
| **Implementations** | CFS (`kernel/sched_fair.c`), RT (`kernel/sched_rt.c`), Idle (`kernel/sched_idletask.c`), Stop (`kernel/sched_stoptask.c`) |

#### The `sched_class` Interface

The scheduler's key abstraction is `struct sched_class` with 24 function pointers:

| Hook | Purpose |
|------|---------|
| `enqueue_task` | Add task to runqueue |
| `dequeue_task` | Remove task from runqueue |
| `pick_next_task` | Choose next task to run |
| `put_prev_task` | Put previous task back |
| `check_preempt_curr` | Check preemption of current task |
| `yield_task` | Voluntary yield |
| `task_tick` | Timer tick handling |
| `task_fork` | Fork-time setup |
| `set_curr_task` | Mark task as current |
| `switched_to` / `switched_from` | Task class migration |
| `select_task_rq` | CPU selection (SMP) |

**Implementation chain (priority order):**

```
stop_sched_class  -->  rt_sched_class  -->  fair_sched_class  -->  idle_sched_class
(sched_stoptask.c)    (sched_rt.c)        (sched_fair.c)        (sched_idletask.c)
      next -->              next -->             next -->              next = NULL
```

#### Dependency Flow

```
                    include/linux/sched.h
                    (defines sched_class)
                           ^    ^
                          /      \
                         /        \
   kernel/sched_fair.c --          -- kernel/sched_rt.c
   (implements fair)                  (implements RT)
         ^                                  ^
         |                                  |
   kernel/sched.c -------------------------+
   (core: #includes implementations)
```

**Note:** The dependency from `kernel/sched.c` to implementation files is a **violation** — see
Section 6.

#### Execution Flow

```
sys_sched_yield()                           [kernel/sched.c:5710]
    |
    +-> current->sched_class->yield_task()  [policy-specific callback]
    |
    +-> schedule()                          [kernel/sched.c:4486]
        |
        +-> __schedule()                    [kernel/sched.c:4395]
            |
            +-> put_prev_task(rq, prev)     -> prev->sched_class->put_prev_task()
            |
            +-> pick_next_task(rq)          -> for_each_class: class->pick_next_task()
            |                                  [stop -> rt -> fair -> idle]
            +-> context_switch(rq, prev, next)  [kernel/sched.c:3287]
                |
                +-> switch_mm()             [arch-specific]
                +-> switch_to(prev, next)   [arch-specific]
```

#### Execution vs. Dependency Comparison

```
Execution:   syscall --> sched core --> sched_class callback --> implementation --> CPU
                 |            |                |                       |
Dependency:      |            |                |                       |
                 v            v                v                       v
             outer         middle          interface               outer
                                           (stable)             (volatile)

Execution flows OUTWARD:   core ---------> implementation
Dependency flows INWARD:   implementation ---------> core interface
```

---

### 4.2 Memory Management

#### Clean Architecture Layers

| Layer | Components |
|-------|-----------|
| **Entities** | `struct page` (`include/linux/mm_types.h:39`), `struct vm_area_struct` (`:200`), `struct mm_struct` (`:287`), `struct zone` (`include/linux/mmzone.h:297`) |
| **Use Cases** | Page allocator (`mm/page_alloc.c`), Slab allocator (`mm/slub.c`), Page fault handler (`mm/memory.c`), mmap (`mm/mmap.c`) |
| **Interface Adapters** | `vm_operations_struct` (`include/linux/mm.h:204`), `address_space_operations` (`include/linux/fs.h:419`) |
| **Implementations** | Generic file VM ops (`mm/filemap.c`), shmem ops (`mm/shmem.c`), driver-specific VM ops |

#### Key Abstraction: `vm_operations_struct`

| Hook | Purpose |
|------|---------|
| `open` | VMA created/duplicated |
| `close` | VMA removed |
| `fault` | **Page fault handler** — the central dispatch point |
| `page_mkwrite` | Page becoming writable |
| `access` | `access_process_vm` fallback |
| `set_policy` / `get_policy` | NUMA policy |
| `migrate` | NUMA page migration |

#### Allocator Layering

```
+-----------------------------------------------------------+
| Application Layer                                         |
|   kmalloc() / kmem_cache_alloc()                          |
|   [mm/slub.c or mm/slab.c]                                |
+-----------------------------------------------------------+
                          |
                          v  alloc_pages() for backing
+-----------------------------------------------------------+
| Page Allocator                                            |
|   __alloc_pages_nodemask()  [mm/page_alloc.c:2255]        |
|   get_page_from_freelist()  [mm/page_alloc.c:1644]        |
|   __alloc_pages_slowpath()  [mm/page_alloc.c:2076]        |
+-----------------------------------------------------------+
                          |
                          v  zone->free_area[order]
+-----------------------------------------------------------+
| Buddy System                                              |
|   __rmqueue_smallest()  [mm/page_alloc.c:801]             |
+-----------------------------------------------------------+
```

#### Dependency Flow

```
                  include/linux/mm.h
               (defines vm_operations_struct)
                     ^           ^
                    /             \
                   /               \
    mm/filemap.c --                 -- drivers/ (GPU, etc.)
    (generic_file_vm_ops)              (driver-specific vm_ops)
         ^
         |
    mm/memory.c
    (page fault handler, calls vma->vm_ops->fault)
```

`include/linux/mm.h` does **not** include `fs.h` or any driver headers. It uses only forward
declarations for cross-subsystem types.

#### Execution Flow: Page Fault

```
Hardware exception (#PF on x86)
    |
    +-> do_page_fault()                  [arch/x86/mm/fault.c:994]
        |  read_cr2(), find_vma()
        |
        +-> handle_mm_fault()            [mm/memory.c:3442]
            |  pgd/pud/pmd walk
            |
            +-> handle_pte_fault()       [mm/memory.c:3386]
                |
                +-- pte_none?
                |   +-> do_linear_fault()
                |       |
                |       +-> vma->vm_ops->fault()   <-- DISPATCH POINT
                |           |
                |           +-> filemap_fault()    [mm/filemap.c:1655]
                |               |
                |               +-> mapping->a_ops->readpage()  <-- FS DISPATCH
                |
                +-- swap entry?
                |   +-> do_swap_page()
                |
                +-- write fault?
                    +-> do_wp_page()     [copy-on-write]
```

#### Execution vs. Dependency

```
Execution:   hardware --> arch --> mm/memory.c --> vm_ops->fault() --> filemap --> a_ops --> FS
Dependency:  FS driver --> a_ops interface <-- mm core <-- arch code
                                    ^
                          (defined in include/linux/fs.h)
```

---

### 4.3 Virtual File System (VFS)

> The VFS is the **purest** Clean Architecture implementation in the kernel.

#### Clean Architecture Layers

| Layer | Components |
|-------|-----------|
| **Entities** | `struct inode` (`include/linux/fs.h:569`), `struct file` (`:758`), `struct dentry` (`include/linux/dcache.h:115`), `struct super_block` (`include/linux/fs.h:1110`) |
| **Use Cases** | `vfs_read()`, `vfs_write()` (`fs/read_write.c`), `do_sys_open()` (`fs/open.c`), path resolution (`fs/namei.c`) |
| **Interface Adapters** | 5 ops-structures + `file_system_type` (see below) |
| **Implementations** | ext4 (`fs/ext4/`), NFS (`fs/nfs/`), tmpfs (`mm/shmem.c`), procfs (`fs/proc/`), etc. |

#### Five Abstraction Interfaces

**1. `file_operations`** (`include/linux/fs.h:1223-1247`) — 25 hooks

Core file I/O: `llseek`, `read`, `write`, `aio_read`, `aio_write`, `readdir`, `poll`,
`unlocked_ioctl`, `mmap`, `open`, `flush`, `release`, `fsync`, `fasync`, `lock`,
`sendpage`, `splice_write`, `splice_read`, `fallocate`, ...

**2. `inode_operations`** (`include/linux/fs.h:1249-1275`) — 23 hooks

Namespace operations: `lookup`, `create`, `link`, `unlink`, `symlink`, `mkdir`, `rmdir`,
`mknod`, `rename`, `permission`, `setattr`, `getattr`, `setxattr`, `getxattr`, ...

**3. `super_operations`** (`include/linux/fs.h:1291-1319`) — 22 hooks

Filesystem lifecycle: `alloc_inode`, `destroy_inode`, `dirty_inode`, `write_inode`,
`drop_inode`, `evict_inode`, `put_super`, `sync_fs`, `statfs`, `remount_fs`, ...

**4. `dentry_operations`** (`include/linux/dcache.h:158-172`) — 10 hooks

Name cache: `d_revalidate`, `d_hash`, `d_compare`, `d_delete`, `d_release`, `d_iput`,
`d_automount`, ...

**5. `address_space_operations`** (`include/linux/fs.h:419-449`) — 17 hooks

Page cache I/O: `writepage`, `readpage`, `writepages`, `readpages`, `write_begin`,
`write_end`, `set_page_dirty`, `direct_IO`, `bmap`, ...

**6. `file_system_type`** (`include/linux/fs.h:1369-1385`) — registration structure

`mount`, `kill_sb` — how filesystems are mounted and unmounted.

#### ext4 Implementation Map

| VFS Interface | ext4 Symbol | File |
|--------------|-------------|------|
| `file_operations` | `ext4_file_operations` | `fs/ext4/file.c:231` |
| `inode_operations` (file) | `ext4_file_inode_operations` | `fs/ext4/file.c:250` |
| `inode_operations` (dir) | `ext4_dir_inode_operations` | `fs/ext4/namei.c:2571` |
| `super_operations` | `ext4_sops` | `fs/ext4/super.c:1271` |
| `address_space_operations` | `ext4_da_aops` (+ 3 others) | `fs/ext4/inode.c:3024-3082` |
| `file_system_type` | `ext4_fs_type` | `fs/ext4/super.c:5012` |

#### Dependency Flow

```
                    include/linux/fs.h
          (file_operations, inode_operations,
           super_operations, address_space_operations)
                 ^          ^          ^
                /            |          \
               /             |           \
    fs/ext4/ --      fs/nfs/ --     fs/proc/ --
    (ext4_sops,      (nfs_sops,     (proc_fops,
     ext4_file_ops)   nfs_file_ops)  proc_iops)
               \             |           /
                \            |          /
                 v           v         v
              fs/open.c, fs/read_write.c, fs/namei.c
              (VFS core: vfs_read, do_sys_open, ...)
```

**Critical proof:** `include/linux/fs.h` includes **zero** filesystem-specific headers (no
ext4, no nfs, no proc). The only noted coupling is a minor `#include <linux/nfs_fs_i.h>`
for a lock-info struct in the `file_lock` union — commented as "that will die."

All ext4 files include `<linux/fs.h>` — dependencies point strictly **inward**.

#### Execution Flow: `sys_open()`

```
sys_open()
    |
    +-> do_sys_open()                        [fs/open.c:973]
        |
        +-> do_filp_open()                   [fs/namei.c:2347]
            |
            +-> path_openat()                [fs/namei.c:2289]
                |
                +-> link_path_walk()         [path resolution]
                |   |
                |   +-> inode->i_op->lookup()  <-- inode_operations dispatch
                |
                +-> do_last()                [fs/namei.c:2084]
                    |
                    +-> nameidata_to_filp()  [fs/open.c:789]
                        |
                        +-> __dentry_open()  [fs/open.c:647]
                            |
                            +-> f->f_op = fops_get(inode->i_fop)
                            +-> f->f_op->open(inode, f)  <-- file_operations dispatch
                                |
                                +-> ext4_file_open()  [fs/ext4/file.c]
```

#### VFS Dispatch Mechanism

The VFS achieves polymorphic dispatch through a two-step process:
1. **Binding**: When an inode is created, the filesystem sets `inode->i_fop` and `inode->i_op`
   to its own ops tables.
2. **Dispatch**: VFS core functions (e.g., `vfs_read`) call through the ops pointers
   without knowing the concrete filesystem type.

```c
/* In __dentry_open() — fs/open.c:681 */
f->f_op = fops_get(inode->i_fop);    /* Bind ops from inode */
open = f->f_op->open;
open(inode, f);                        /* Dispatch to filesystem */

/* In vfs_read() — fs/read_write.c:377 */
ret = file->f_op->read(file, buf, count, pos);  /* Polymorphic call */
```

#### Execution vs. Dependency

```
Execution:   userspace -> syscall -> VFS core -> ops dispatch -> ext4 -> block layer -> disk
                                                     |
Dependency:                ext4 ---> VFS interfaces <--- VFS core
                          (outer)     (inner/stable)    (middle)
```

---

### 4.4 Network Stack

#### Clean Architecture Layers

| Layer | Components |
|-------|-----------|
| **Entities** | `struct socket` (`include/linux/net.h:127`), `struct sock` (`include/net/sock.h:236`), `struct sk_buff` (`include/linux/skbuff.h:364`), `struct net_device` (`include/linux/netdevice.h:682`) |
| **Use Cases** | Socket creation (`net/socket.c`), packet routing (`net/core/dev.c`), TCP state machine (`net/ipv4/tcp.c`) |
| **Interface Adapters** | `proto_ops`, `net_device_ops`, `proto`, `packet_type`, `net_proto_family` |
| **Implementations** | IPv4 (`net/ipv4/`), drivers (`drivers/net/`), protocol families |

#### Three-Layer Abstraction

```
+-------------------------------------------------+
| Socket Layer                                     |
|   net/socket.c                                   |
|   Interface: struct proto_ops (20 hooks)          |
|   Interface: struct net_proto_family              |
+-------------------------------------------------+
                      |
                      v
+-------------------------------------------------+
| Protocol Layer                                   |
|   net/ipv4/, net/ipv6/                           |
|   Interface: struct proto (30+ hooks)             |
|   Interface: struct packet_type                   |
+-------------------------------------------------+
                      |
                      v
+-------------------------------------------------+
| Device Layer                                     |
|   net/core/dev.c                                 |
|   Interface: struct net_device_ops (40+ hooks)    |
+-------------------------------------------------+
                      |
                      v
+-------------------------------------------------+
| Driver Layer                                     |
|   drivers/net/                                   |
|   Implements: net_device_ops                      |
+-------------------------------------------------+
```

#### Key Interfaces

**`proto_ops`** (`include/linux/net.h:161-208`) — socket-to-protocol adapter:
`release`, `bind`, `connect`, `accept`, `listen`, `sendmsg`, `recvmsg`, `poll`, ...

**`net_device_ops`** (`include/linux/netdevice.h:612-678`) — device abstraction:
`ndo_open`, `ndo_stop`, `ndo_start_xmit` (required), `ndo_set_mac_address`,
`ndo_change_mtu`, `ndo_tx_timeout`, `ndo_get_stats64`, ...

**`proto`** (`include/net/sock.h:545-600`) — transport protocol:
`close`, `connect`, `accept`, `sendmsg`, `recvmsg`, `bind`, `backlog_rcv`, `hash`, ...

#### Dependency Flow

```
drivers/net/loopback.c  ---+
drivers/net/.../e1000   ---+--->  include/linux/netdevice.h  (net_device_ops)
                           |              ^
                           |              |
                     net/ipv4/af_inet.c --+--->  include/linux/net.h  (proto_ops)
                     net/ipv4/tcp.c     --+--->  include/net/sock.h   (proto)
                                          |              ^
                                          |              |
                                   net/socket.c ---------+
```

`include/linux/netdevice.h` includes **no** driver-specific headers. Network drivers
(e.g., `drivers/net/ethernet/intel/e1000/`) include `linux/netdevice.h`, `linux/pci.h`,
etc. — dependencies point inward.

#### Execution Flow: Packet Reception

```
NIC hardware interrupt
    |
    +-> Driver NAPI poll
        |
        +-> netif_receive_skb(skb)         [net/core/dev.c:3364]
            |
            +-> __netif_receive_skb()      [net/core/dev.c:3224]
                |
                +-- ptype_all handlers     [raw sockets, tcpdump]
                |
                +-- rx_handler             [bridge, bonding]
                |
                +-> ptype_base[hash]       [protocol dispatch]
                    |
                    +-> ip_rcv()           [net/ipv4/ip_input.c:375]
                        |                  (registered as ETH_P_IP handler)
                        +-> ip_local_deliver()
                            |
                            +-> tcp_v4_rcv() or udp_rcv()
                                |
                                +-> sock_queue_rcv_skb()
                                    |
                                    +-> sk->sk_data_ready()  [wake socket]
```

#### Execution Flow: Packet Transmission

```
sendmsg() syscall
    |
    +-> sock->ops->sendmsg()              [proto_ops dispatch]
        |
        +-> sk->sk_prot->sendmsg()        [proto dispatch, e.g. tcp_sendmsg]
            |
            +-> ip_queue_xmit()
                |
                +-> dev_queue_xmit(skb)    [net/core/dev.c:2488]
                    |
                    +-> dev_hard_start_xmit()
                        |
                        +-> dev->netdev_ops->ndo_start_xmit()  [driver dispatch]
```

#### Registration Mechanisms

| Function | Purpose |
|----------|---------|
| `sock_register(&inet_family_ops)` | Register AF_INET protocol family |
| `dev_add_pack(&ip_packet_type)` | Register ETH_P_IP receive handler |
| `register_netdev(dev)` | Register network device |
| `proto_register(&tcp_prot)` | Register TCP protocol |

#### Dependency Violation

`struct net_device` contains protocol-specific pointers (`ip_ptr`, `ip6_ptr`, `dn_ptr`,
`atalk_ptr`) at `include/linux/netdevice.h:816-826`. This mixes the device abstraction
layer with protocol knowledge — a mild Clean Architecture violation trading purity for
performance.

---

### 4.5 Device Driver Model

#### Clean Architecture Layers

| Layer | Components |
|-------|-----------|
| **Entities** | `struct device` (`include/linux/device.h:413`), `struct kobject`, `struct cdev` (`include/linux/cdev.h:12`) |
| **Use Cases** | Device registration, probe/remove lifecycle, sysfs exposure |
| **Interface Adapters** | `struct bus_type` (`:86`), `struct device_driver` (`:192`), `struct file_operations` |
| **Implementations** | PCI drivers, platform drivers, character drivers, block drivers |

#### Hierarchical Abstraction

```
+-------------------------------------------+
| Generic Driver Model                      |
|   struct device_driver  (probe, remove)   |
|   struct bus_type       (match, probe)    |
+-------------------------------------------+
              |
              v
+-------------------------------------------+
| Bus-Specific Wrappers                     |
|   struct pci_driver     [include/linux/pci.h:551]           |
|   struct platform_driver [include/linux/platform_device.h:164] |
|   struct usb_driver                       |
+-------------------------------------------+
              |
              v
+-------------------------------------------+
| Concrete Drivers                          |
|   e1000, gpio, i2c devices, etc.         |
+-------------------------------------------+
```

Bus-specific drivers (e.g., `pci_driver`) **embed** `struct device_driver`, achieving
composition over inheritance:

```c
/* include/linux/pci.h:551 */
struct pci_driver {
    const char *name;
    const struct pci_device_id *id_table;
    int  (*probe)(struct pci_dev *, const struct pci_device_id *);
    void (*remove)(struct pci_dev *);
    /* ... */
    struct device_driver driver;     /* <-- embedded base */
};
```

#### Character Device Flow

```
userspace read(fd, buf, count)
    |
    +-> SYSCALL_DEFINE3(read)              [fs/read_write.c:460]
        |
        +-> vfs_read(file, buf, count)     [fs/read_write.c:364]
            |
            +-> file->f_op->read()         [VFS dispatches to driver]
```

The connection is made during `open()`:
1. `chrdev_open()` looks up `cdev` via `kobj_lookup(cdev_map, inode->i_rdev)`
2. Sets `filp->f_op = fops_get(cdev->ops)` — binding driver's `file_operations`
3. All subsequent `read`/`write`/`ioctl` go through the driver's ops table

#### `bus_type` Function Pointers

| Hook | Purpose |
|------|---------|
| `match` | Match device to driver |
| `uevent` | Generate uevent environment |
| `probe` | Probe device |
| `remove` | Remove device |
| `shutdown` | Shutdown device |
| `suspend` / `resume` | Power management |

#### Dependency Flow

```
drivers/net/e1000/ ---+
drivers/char/mem.c  --+--->  include/linux/device.h  (device_driver, bus_type)
drivers/pci/*.c     --+              ^
                                     |
                            include/linux/pci.h  (pci_driver embeds device_driver)
                                     ^
                                     |
                            drivers/base/driver.c  (driver_register)
```

`include/linux/device.h` includes only generic kernel headers (`ioport.h`, `kobject.h`,
`mutex.h`, `pm.h`). Verified: **zero** driver-specific includes.

#### Registration Cascade

```
module_init(e1000_init)
    |
    +-> pci_register_driver(&e1000_driver)   [include/linux/pci.h:940]
        |
        +-> __pci_register_driver()
            |  drv->driver.bus = &pci_bus_type
            |
            +-> driver_register(&drv->driver)  [drivers/base/driver.c:222]
                |
                +-> bus_add_driver(drv)
```

---

### 4.6 Security (Linux Security Modules)

#### Clean Architecture Layers

| Layer | Components |
|-------|-----------|
| **Entities** | Credentials (`struct cred`), security labels, audit data |
| **Use Cases** | Permission checks, access control decisions |
| **Interface Adapter** | `struct security_operations` (`include/linux/security.h:1380-1655`) — **~185 hooks** |
| **Implementations** | SELinux (`security/selinux/`), AppArmor (`security/apparmor/`), SMACK (`security/smack/`), TOMOYO (`security/tomoyo/`), Capabilities (`security/capability.c`) |

#### The `security_operations` Interface

The largest ops-structure in the kernel (~185 function pointers), organized by category:

| Category | Example Hooks | Count |
|----------|--------------|-------|
| **Inode** | `inode_permission`, `inode_create`, `inode_link`, `inode_setxattr`, ... | ~28 |
| **File** | `file_permission`, `file_mmap`, `file_ioctl`, `dentry_open`, ... | ~12 |
| **Task/Cred** | `task_create`, `task_kill`, `cred_prepare`, `task_setscheduler`, ... | ~25 |
| **Socket/Network** | `socket_create`, `socket_bind`, `socket_sendmsg`, `sk_alloc_security`, ... | ~30 |
| **Superblock** | `sb_mount`, `sb_umount`, `sb_statfs`, ... | ~12 |
| **Path** | `path_mknod`, `path_mkdir`, `path_chmod`, ... | ~11 |
| **IPC** | `ipc_permission`, `msg_queue_*`, `shm_*`, `sem_*`, ... | ~15 |
| **Capabilities** | `capable`, `capget`, `capset` | 3 |

#### Hook Dispatch Mechanism

```c
/* security/security.c:418 — wrapper function */
int security_inode_permission(struct inode *inode, int mask)
{
    if (unlikely(IS_PRIVATE(inode)))
        return 0;
    return security_ops->inode_permission(inode, mask);
}
```

All hooks follow this pattern: a thin wrapper in `security/security.c` dispatches through
the global `security_ops` pointer.

#### Implementation Map

| Module | Ops Symbol | File | Registration |
|--------|-----------|------|-------------|
| SELinux | `selinux_ops` | `security/selinux/hooks.c:5453` | `register_security(&selinux_ops)` at `:5674` |
| AppArmor | `apparmor_ops` | `security/apparmor/lsm.c:624` | `register_security(&apparmor_ops)` at `:922` |
| SMACK | `smack_ops` | `security/smack/smack_lsm.c:3484` | `register_security(&smack_ops)` at `:3674` |
| TOMOYO | `tomoyo_security_ops` | `security/tomoyo/tomoyo.c` | `register_security()` at `:554` |
| Capabilities | default via `security_fixup_ops()` | `security/capability.c` | Fills NULL hooks with `cap_*` defaults |

#### Boot-time Registration

```c
/* security/security.c:57 */
int __init security_init(void)
{
    security_fixup_ops(&default_security_ops);  /* Fill defaults */
    security_ops = &default_security_ops;
    do_security_initcalls();                    /* Run LSM inits */
    return 0;
}

/* security/security.c:111 */
int __init register_security(struct security_operations *ops)
{
    if (security_ops != &default_security_ops)
        return -EAGAIN;          /* Only one LSM allowed */
    security_ops = ops;
    return 0;
}
```

#### Dependency Flow

```
                  include/linux/security.h
                  (defines security_operations)
                     ^        ^        ^
                    /          |        \
    security/selinux/  security/apparmor/  security/smack/
    (selinux_ops)      (apparmor_ops)      (smack_ops)

                  include/linux/security.h
                           ^
                           |
              +------------+-------------+
              |            |             |
         fs/namei.c   fs/open.c    net/socket.c
         (callers: security_inode_permission, etc.)
```

`include/linux/security.h` includes **zero** LSM-specific headers. All LSM implementations
include `<linux/security.h>`. Dependencies point strictly **inward**.

#### Execution Flow: File Open Security Check

```
open() syscall
    |
    +-> do_sys_open()
        |
        +-> path_openat()
            |
            +-> do_last()
                |
                +-> inode_permission()
                |   |
                |   +-> security_inode_permission()     [security/security.c:418]
                |       |
                |       +-> security_ops->inode_permission()
                |           |
                |           +-> selinux_inode_permission()  [security/selinux/hooks.c:2656]
                |
                +-> __dentry_open()
                    |
                    +-> security_dentry_open()           [security/security.c:547]
                        |
                        +-> security_ops->dentry_open()
                            |
                            +-> selinux_dentry_open()    [security/selinux/hooks.c:3205]
```

---

### 4.7 Interrupt Handling

#### Clean Architecture Layers

| Layer | Components |
|-------|-----------|
| **Entities** | `struct irq_desc` (`include/linux/irqdesc.h:40`), `struct irqaction` (`include/linux/interrupt.h:110`) |
| **Use Cases** | IRQ dispatch (`kernel/irq/handle.c`), IRQ management (`kernel/irq/manage.c`) |
| **Interface Adapter** | `struct irq_chip` (`include/linux/irq.h:295`) — 21 hooks |
| **Implementations** | I/O APIC (`arch/x86/kernel/apic/io_apic.c`), i8259 PIC (`arch/x86/kernel/i8259.c`), ARM GIC, etc. |

#### `irq_chip` Interface

| Hook | Purpose |
|------|---------|
| `irq_startup` / `irq_shutdown` | Start/stop interrupt |
| `irq_enable` / `irq_disable` | Enable/disable interrupt |
| `irq_ack` | Acknowledge interrupt |
| `irq_mask` / `irq_unmask` | Mask/unmask interrupt line |
| `irq_eoi` | End of interrupt |
| `irq_set_affinity` | Set CPU affinity (SMP) |
| `irq_set_type` | Set trigger type (level/edge) |
| `irq_set_wake` | Configure wake-on-IRQ |

#### Dual-Level Handler Architecture

```
+------------------------------------------+
| irq_chip  (hardware-level)               |
|   - ack, mask, unmask, eoi               |
|   - Abstracts interrupt controller HW    |
+------------------------------------------+
              |
              v
+------------------------------------------+
| irq_flow_handler_t  (flow-level)         |
|   - handle_level_irq                     |
|   - handle_edge_irq                      |
|   - handle_fasteoi_irq                   |
|   - Manages IRQ flow semantics           |
+------------------------------------------+
              |
              v
+------------------------------------------+
| irqaction->handler  (device-level)       |
|   - Driver's interrupt handler           |
|   - Registered via request_irq()         |
+------------------------------------------+
```

#### Dependency Flow

```
arch/x86/kernel/apic/io_apic.c  ---+
arch/x86/kernel/i8259.c          --+--->  include/linux/irq.h  (irq_chip)
drivers/*  (request_irq)          --+             ^
                                                  |
                                         kernel/irq/chip.c
                                         kernel/irq/handle.c
                                         kernel/irq/manage.c
                                         (generic IRQ layer)
```

`include/linux/irq.h` includes **no** arch-specific interrupt controller headers.

#### Execution Flow

```
Hardware interrupt
    |
    +-> CPU vector entry                   [arch/x86/kernel/entry_64.S]
        |
        +-> do_IRQ(regs)                   [arch/x86/kernel/irq.c:176]
            |
            +-> handle_irq(irq, regs)      [arch/x86/kernel/irq_64.c:55]
                |
                +-> generic_handle_irq_desc(irq, desc)
                    |
                    +-> desc->handle_irq(irq, desc)     <-- flow handler dispatch
                        |
                        e.g. handle_level_irq()         [kernel/irq/chip.c:344]
                             |
                             +-> chip->irq_mask()        <-- irq_chip dispatch
                             +-> handle_irq_event(desc)  [kernel/irq/handle.c:166]
                                 |
                                 +-> action->handler(irq, dev_id)  <-- driver handler
                             +-> chip->irq_unmask()
```

---

### 4.8 Time Management

#### Clean Architecture Layers

| Layer | Components |
|-------|-----------|
| **Entities** | `struct timer_list` (`include/linux/timer.h:12`), `struct hrtimer` (`include/linux/hrtimer.h:107`) |
| **Use Cases** | Timekeeping (`kernel/time/timekeeping.c`), tick management (`kernel/time/tick-common.c`), timer wheel (`kernel/timer.c`) |
| **Interface Adapters** | `struct clocksource` (`include/linux/clocksource.h:166`), `struct clock_event_device` (`include/linux/clockchips.h:82`) |
| **Implementations** | TSC (`arch/x86/kernel/tsc.c`), HPET (`arch/x86/kernel/hpet.c`), Local APIC timer, PIT, `drivers/clocksource/` |

#### `clocksource` Interface (Free-Running Counters)

| Hook | Purpose |
|------|---------|
| `read` | Read current cycle count |
| `enable` / `disable` | Enable/disable clocksource |
| `suspend` / `resume` | Power management |

#### `clock_event_device` Interface (Programmable Timers)

| Hook | Purpose |
|------|---------|
| `event_handler` | Called on timer event |
| `set_next_event` | Program next event (cycles) |
| `set_next_ktime` | Program next event (ktime) |
| `set_mode` | Set operating mode |
| `broadcast` | Broadcast to CPUs |

#### Dependency Flow

```
arch/x86/kernel/tsc.c    ---+
arch/x86/kernel/hpet.c   --+--->  include/linux/clocksource.h  (clocksource)
drivers/clocksource/*.c  --+      include/linux/clockchips.h   (clock_event_device)
                                           ^
                                           |
                                  kernel/time/clocksource.c
                                  kernel/time/clockevents.c
                                  kernel/time/tick-common.c
                                  (generic time layer)
```

#### Execution Flow: Timer Tick

```
Clock hardware (Local APIC timer)
    |
    +-> smp_apic_timer_interrupt()       [arch/x86/kernel/apic/apic.c:865]
        |
        +-> local_apic_timer_interrupt() [arch/x86/kernel/apic/apic.c:826]
            |
            +-> evt->event_handler(evt)  <-- clock_event_device dispatch
                |
                +-> tick_handle_periodic()  [kernel/time/tick-common.c:82]
                    |
                    +-> tick_periodic()     [kernel/time/tick-common.c:63]
                        |
                        +-> do_timer()             [update jiffies, wall time]
                        +-> update_process_times()  [kernel/timer.c:1286]
                            |
                            +-> scheduler_tick()   [kernel/sched.c]
                            +-> run_local_timers() [fire expired timers]
                            +-> run_posix_cpu_timers()
```

---

## 5. Cross-Subsystem Comparison

### 5.1 Abstraction Purity Ranking

| Rank | Subsystem | Purity | Rationale |
|------|-----------|--------|-----------|
| 1 | **VFS** | Excellent | 5 separate ops-structures, zero reverse dependencies, clean dispatch |
| 2 | **Security (LSM)** | Excellent | Single large interface, zero reverse dependencies, clean hooks |
| 3 | **IRQ** | Excellent | Clean chip abstraction, dual-level handlers, no HW coupling in headers |
| 4 | **Time** | Excellent | Clean clocksource/clockevent split, no HW coupling |
| 5 | **Device Model** | Very Good | Hierarchical bus/driver/device, clean registration cascade |
| 6 | **Memory Management** | Very Good | Clean VM ops, but tightly coupled allocator internals |
| 7 | **Network** | Good | Multi-layer abstractions, but protocol pointers leak into `net_device` |
| 8 | **Scheduler** | Moderate | Good `sched_class` interface, but core `#include`s implementations |

### 5.2 Execution Flow vs. Dependency Flow Summary

All subsystems exhibit the Clean Architecture hallmark: **execution and dependency flow in
opposite directions**.

```
+------------------+------------------------------------+------------------------------------+
| Subsystem        | Execution Flow (outward)           | Dependency Flow (inward)           |
+------------------+------------------------------------+------------------------------------+
| VFS              | syscall -> VFS core -> ext4 -> HW  | ext4 -> VFS interface <- VFS core  |
| Scheduler        | syscall -> sched core -> CFS -> HW | CFS -> sched_class <- sched core   |
| Memory Mgmt      | #PF -> mm core -> vm_ops -> FS     | FS -> vm_ops iface <- mm core      |
| Network (RX)     | HW -> driver -> core -> proto      | driver -> netdev_ops <- core       |
| Network (TX)     | socket -> proto -> core -> driver  | driver -> netdev_ops <- core       |
| Device Drivers   | syscall -> VFS -> cdev -> driver   | driver -> device.h <- VFS          |
| Security         | VFS -> security_*(wrapper) -> LSM  | LSM -> security_ops <- VFS         |
| IRQ              | HW -> arch -> generic -> handler   | arch -> irq_chip <- generic        |
| Time             | HW -> arch -> tick -> scheduler    | arch -> clocksource <- tick mgmt   |
+------------------+------------------------------------+------------------------------------+
```

### 5.3 Interface Size Comparison

```
security_operations  ████████████████████████████████████████  ~185 hooks
net_device_ops       ████████                                  ~40 hooks
proto                ██████                                    ~30 hooks
file_operations      █████                                     25 hooks
sched_class          █████                                     24 hooks
inode_operations     █████                                     23 hooks
super_operations     ████                                      22 hooks
irq_chip             ████                                      21 hooks
proto_ops            ████                                      20 hooks
address_space_ops    ███                                       17 hooks
block_device_ops     ██                                        11 hooks
dentry_operations    ██                                        10 hooks
vm_operations_struct █                                         8 hooks
bus_type             █                                         7 hooks
clocksource          █                                         5 hooks
clock_event_device   █                                         5 hooks
```

---

## 6. Dependency Violations and Trade-offs

### 6.1 Confirmed Violations

| # | Subsystem | Violation | Severity | File |
|---|-----------|-----------|----------|------|
| 1 | **Scheduler** | `kernel/sched.c` `#include`s `sched_fair.c`, `sched_rt.c`, etc. | **High** | `kernel/sched.c:2196-2203` |
| 2 | **Scheduler** | Core references concrete class names (`fair_sched_class`, `stop_sched_class`) | **Medium** | `kernel/sched.c:1909,4378,3012` |
| 3 | **Network** | `net_device` has protocol-specific pointers (`ip_ptr`, `ip6_ptr`) | **Low** | `include/linux/netdevice.h:816-826` |
| 4 | **VFS** | `include/linux/fs.h` includes `linux/nfs_fs_i.h` for lock info | **Minimal** | `include/linux/fs.h:1123` |

### 6.2 Why the Scheduler Violates

The scheduler's `#include` of `.c` files is a deliberate trade-off:

- **Performance**: Inlining across scheduler classes enables critical-path optimization.
  `pick_next_task()` on the fast path directly references `fair_sched_class` to avoid
  iterating the class chain.
- **Compilation unit**: All scheduler code compiles into a single `sched.o`, enabling
  the compiler to optimize across class boundaries.
- **Trade-off**: Architecture purity is sacrificed for microsecond-level scheduling latency.

Note: This was refactored in later kernels (3.10+) to move scheduler code into
`kernel/sched/` with separate `.c` files compiled independently but with shared
internal headers.

### 6.3 Why `net_device` Leaks Protocol Knowledge

Having `ip_ptr`, `ip6_ptr` directly in `struct net_device` avoids an extra lookup for
every packet processed. In a high-throughput network stack processing millions of
packets per second, this saves significant overhead.

---

## 7. Key Insights and Conclusions

### 7.1 How the Kernel Separates "What Changes" from "What Doesn't"

| What Doesn't Change (Inner/Stable) | What Changes (Outer/Volatile) |
|-------------------------------------|-------------------------------|
| `struct file_operations` interface | ext4, NFS, procfs implementations |
| `struct sched_class` interface | CFS, RT, deadline scheduler policies |
| `struct irq_chip` interface | I/O APIC, GIC, PIC implementations |
| `struct net_device_ops` interface | e1000, rtl8139, virtio-net drivers |
| `struct security_operations` interface | SELinux, AppArmor, SMACK policies |
| `struct clocksource` interface | TSC, HPET, PIT hardware drivers |
| `struct vm_operations_struct` interface | file-backed, anonymous, shmem, GPU mappings |

### 7.2 The Dependency Rule

In 7 of 8 subsystems analyzed, the dependency rule holds strictly:

> **Outer layers (drivers, filesystems, security modules) depend on inner layers
> (core interfaces in `include/linux/`). Inner layers never depend on outer layers.**

This is enforced through:
- Header file organization: interfaces in `include/linux/`, implementations in `drivers/`, `fs/`, `security/`
- Forward declarations instead of direct includes
- The ops-structure pattern providing the inversion mechanism

### 7.3 Inversion of Control

The kernel achieves IoC through **registration + callback dispatch**:

1. **At init time**: Implementations register themselves (`register_filesystem`,
   `register_netdev`, `register_security`, `clocksource_register`, `request_irq`,
   `cdev_add`, `pci_register_driver`)
2. **At runtime**: Core dispatches through ops pointers without knowing concrete types

This is functionally equivalent to Dependency Injection in object-oriented frameworks,
achieved in C through function pointer tables.

### 7.4 Comparison with Uncle Bob's Clean Architecture

| Clean Architecture Principle | Kernel Implementation | Assessment |
|------------------------------|----------------------|------------|
| **Dependency Rule** | Header hierarchy, ops-structures | Strong adherence |
| **Entities** | Core data structures (`task_struct`, `inode`, `sk_buff`) | Well-defined |
| **Use Cases** | Core subsystem logic (`schedule()`, `vfs_read()`) | Clear separation |
| **Interface Adapters** | Ops-structures as contracts | Extensive use |
| **Frameworks & Drivers** | `drivers/`, `fs/ext4/`, `security/selinux/` | Cleanly separated |
| **Independent of UI** | Kernel has no UI; userspace boundary via syscalls | By design |
| **Independent of Database** | N/A; but filesystem abstraction analogous | VFS as the "repository interface" |
| **Testability** | Can swap scheduler policies, filesystems, security modules independently | Pluggable |

### 7.5 Architectural Pattern Summary

The Linux kernel's architecture can be summarized as a **layered onion** with
**ops-structure seams**:

```
+============================================================+
|                                                            |
|   OUTER: Hardware & External Systems                       |
|   (NIC, disk, interrupt controller, clock hardware)        |
|                                                            |
+------------------------------------------------------------+
|                                                            |
|   DRIVERS & IMPLEMENTATIONS                                |
|   (e1000, ext4, SELinux, io_apic, tsc, CFS scheduler)     |
|   Dependencies: INWARD only                                |
|                                                            |
+------------------------------------------------------------+
|                                                            |
|   INTERFACE LAYER (ops-structures)                         |
|   file_operations, net_device_ops, irq_chip,               |
|   sched_class, security_operations, clocksource,           |
|   vm_operations_struct, bus_type, proto_ops                 |
|   Location: include/linux/*.h                              |
|                                                            |
+------------------------------------------------------------+
|                                                            |
|   CORE LOGIC (Use Cases)                                   |
|   VFS dispatch, scheduler core, MM page fault handler,     |
|   generic IRQ layer, tick management, driver model         |
|   Dependencies: INWARD only (to interfaces)                |
|                                                            |
+------------------------------------------------------------+
|                                                            |
|   ENTITIES (Core Data Structures)                          |
|   task_struct, inode, file, dentry, sk_buff, page,         |
|   irq_desc, net_device, device, cred                       |
|   Location: include/linux/*.h                              |
|                                                            |
+============================================================+
```

---

*Analysis performed on Linux Kernel v3.2.0 ("Saber-toothed Squirrel").*
*All file paths and line numbers reference the source tree at `/home/morrism/repos/linux`.*
