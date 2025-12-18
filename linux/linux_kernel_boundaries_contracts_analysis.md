# Linux Kernel v3.2 Architecture: Boundaries and Contracts Analysis

A professional architecture review of the Linux kernel's boundary design, contract enforcement, and architectural patterns.

---

## Table of Contents

1. [Major Architectural Boundaries](#step-1--major-architectural-boundaries)
2. [Mapping Boundaries to Code](#step-2--mapping-boundaries-to-code-locations)
3. [Contracts at Each Boundary](#step-3--contracts-enforced-at-each-boundary)
4. [Dependency Rules & Direction](#step-4--dependency-rules--direction)
5. [How Violations Creep In](#step-5--how-violations-creep-in-architecture-decay)
6. [Existing Safeguards](#step-6--existing-safeguards-or-lack-thereof)
7. [Stability vs Volatility](#step-7--stability-vs-volatility-analysis)
8. [Stress-Testing the Architecture](#step-8--stress-test-the-architecture)
9. [Architecture Lessons](#step-9--architecture-lessons-extracted)

---

## Step 1 — Major Architectural Boundaries

### Overview Diagram

```
+===========================================================================+
|                          USER SPACE (Ring 3)                               |
|                                                                            |
|    Applications    |    Libraries (glibc)    |    System Utilities         |
+===========================================================================+
                     |           |           |
                     |  System Call Interface (syscall)
                     v           v           v
+===========================================================================+
|                          KERNEL SPACE (Ring 0)                             |
|                                                                            |
|  +---------------------------------------------------------------------+   |
|  |                    SYSTEM CALL LAYER                                |   |
|  |         sys_read(), sys_write(), sys_socket(), ...                  |   |
|  +---------------------------------------------------------------------+   |
|                     |           |           |                              |
|         +-----------+-----------+-----------+-----------+                  |
|         v           v           v           v           v                  |
|  +----------+ +----------+ +----------+ +----------+ +----------+          |
|  |   VFS    | | Network  | |  Memory  | | Process  | | Security |          |
|  |  Layer   | |  Stack   | |   Mgmt   | |  Mgmt    | |  Modules |          |
|  +----------+ +----------+ +----------+ +----------+ +----------+          |
|         |           |           |           |           |                  |
|         v           v           v           v           v                  |
|  +---------------------------------------------------------------------+   |
|  |              CORE KERNEL SERVICES                                   |   |
|  |   Scheduler | IRQ | Timers | Workqueues | RCU | Memory Allocator    |   |
|  +---------------------------------------------------------------------+   |
|         |           |           |           |                              |
|         v           v           v           v                              |
|  +---------------------------------------------------------------------+   |
|  |              DEVICE DRIVER FRAMEWORK                                |   |
|  |        Block Layer | Network Drivers | Char Drivers | USB           |   |
|  +---------------------------------------------------------------------+   |
|         |           |           |           |                              |
|         v           v           v           v                              |
|  +---------------------------------------------------------------------+   |
|  |              ARCHITECTURE ABSTRACTION LAYER                         |   |
|  |              (arch/x86, arch/arm, ...)                              |   |
|  +---------------------------------------------------------------------+   |
|                              |                                             |
+===========================================================================+
                               v
+===========================================================================+
|                          HARDWARE                                          |
+===========================================================================+
```

**说明 (Chinese Explanation):**
- 用户空间 (User Space): 运行在 Ring 3，不能直接访问硬件
- 系统调用层: 用户空间进入内核的唯一合法入口
- VFS/网络栈/内存管理: 子系统层，提供核心功能抽象
- 核心内核服务: 调度器、中断、定时器等基础设施
- 设备驱动框架: 统一的硬件抽象接口
- 架构抽象层: 隐藏 CPU 架构差异

### Major Boundaries Table

| Boundary Name | Left Side | Right Side | Purpose | Change Absorbed |
|---------------|-----------|------------|---------|-----------------|
| **User/Kernel** | User applications | Kernel subsystems | Security, privilege isolation | Application changes, new syscalls |
| **VFS/Filesystem** | Generic file operations | Specific filesystem (ext4, xfs) | Filesystem independence | New filesystems without VFS changes |
| **Socket/Protocol** | BSD socket API | Protocol implementations (TCP, UDP) | Protocol independence | New protocols without socket changes |
| **Protocol/Device** | Network protocols | Network device drivers | Hardware independence | New NICs without protocol changes |
| **Block Layer/Driver** | Block I/O requests | Block device drivers | Storage device independence | New storage devices |
| **Subsystem/Arch** | Generic kernel code | Architecture-specific code | CPU architecture independence | New architectures |
| **Driver/Hardware** | Driver software | Physical hardware | Hardware abstraction | Hardware revisions |

---

## Step 2 — Mapping Boundaries to Code Locations

### 2.1 VFS (Virtual File System) Boundary

```
VFS BOUNDARY STRUCTURE:

include/linux/fs.h                 <-- CONTRACT DEFINITION
      |
      | defines interfaces
      v
+------------------+------------------+------------------+
|   fs/read_write.c |   fs/namei.c    |   fs/open.c     |
|   fs/file.c       |   fs/inode.c    |   fs/super.c    |
+------------------+------------------+------------------+
      |
      | VFS calls filesystem through operations tables
      v
+------------------+------------------+------------------+
|   fs/ext4/       |   fs/xfs/       |   fs/nfs/       |
|   fs/btrfs/      |   fs/fat/       |   fs/proc/      |
+------------------+------------------+------------------+
```

**说明:**
- `include/linux/fs.h`: 定义 VFS 的核心契约（`file_operations`, `inode_operations` 等）
- `fs/*.c`: VFS 通用实现，调用具体文件系统
- `fs/ext4/`, `fs/xfs/`: 具体文件系统实现操作表

**Key Contract Structures:**

| Structure | Location | Purpose |
|-----------|----------|---------|
| `struct file_operations` | `include/linux/fs.h:1583` | File I/O operations contract |
| `struct inode_operations` | `include/linux/fs.h:1613` | Inode manipulation contract |
| `struct super_operations` | `include/linux/fs.h:1658` | Superblock/filesystem contract |
| `struct file_system_type` | `include/linux/fs.h:1859` | Filesystem registration contract |
| `struct address_space_operations` | `include/linux/fs.h` | Page cache I/O contract |

**Code Example - file_operations contract:**

```c
/* include/linux/fs.h:1583-1611 */
struct file_operations {
    struct module *owner;
    loff_t (*llseek) (struct file *, loff_t, int);
    ssize_t (*read) (struct file *, char __user *, size_t, loff_t *);
    ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *);
    ssize_t (*aio_read) (struct kiocb *, const struct iovec *, unsigned long, loff_t);
    ssize_t (*aio_write) (struct kiocb *, const struct iovec *, unsigned long, loff_t);
    int (*readdir) (struct file *, void *, filldir_t);
    unsigned int (*poll) (struct file *, struct poll_table_struct *);
    long (*unlocked_ioctl) (struct file *, unsigned int, unsigned long);
    int (*mmap) (struct file *, struct vm_area_struct *);
    int (*open) (struct inode *, struct file *);
    int (*flush) (struct file *, fl_owner_t id);
    int (*release) (struct inode *, struct file *);
    int (*fsync) (struct file *, loff_t, loff_t, int datasync);
    /* ... more operations ... */
};
```

### 2.2 Network Stack Boundary

```
NETWORK STACK BOUNDARIES:

include/linux/net.h                <-- Socket layer contract
include/net/sock.h                 <-- Socket internals contract  
include/net/tcp.h                  <-- TCP protocol contract
include/linux/netdevice.h          <-- Device driver contract
      |
      v
+------------------------------------------------------------------+
|                         net/socket.c                              |
|                    (Socket system calls)                          |
+------------------------------------------------------------------+
      |
      | struct proto_ops (protocol family operations)
      v
+------------------------------------------------------------------+
|  net/ipv4/af_inet.c   |   net/ipv6/af_inet6.c   |  net/unix/     |
|  (IPv4 family)        |   (IPv6 family)          |  (Unix domain) |
+------------------------------------------------------------------+
      |
      | struct proto (transport protocol operations)
      v
+------------------------------------------------------------------+
|  net/ipv4/tcp.c       |   net/ipv4/udp.c        |  net/sctp/     |
|  net/ipv4/tcp_*.c     |   net/ipv4/raw.c        |                |
+------------------------------------------------------------------+
      |
      | struct net_device_ops (device operations)
      v
+------------------------------------------------------------------+
|  drivers/net/ethernet/ |  drivers/net/wireless/ |  drivers/net/* |
+------------------------------------------------------------------+
```

**说明:**
- `include/linux/net.h`: 定义 socket 层和协议族之间的契约
- `include/net/sock.h`: 定义 socket 内部表示和传输协议契约
- `include/linux/netdevice.h`: 定义网络协议和设备驱动之间的契约
- 每一层通过操作表（`proto_ops`, `proto`, `net_device_ops`）与下层通信

**Key Contract Structures:**

| Structure | Location | Purpose |
|-----------|----------|---------|
| `struct socket` | `include/linux/net.h:138` | BSD socket representation |
| `struct proto_ops` | `include/linux/net.h:161` | Protocol family operations |
| `struct sock` | `include/net/sock.h:238` | Internal socket representation |
| `struct proto` | `include/net/sock.h` | Transport protocol operations |
| `struct net_device_ops` | `include/linux/netdevice.h:859` | Network device operations |
| `struct net_device` | `include/linux/netdevice.h:963` | Network device representation |

### 2.3 Block Layer Boundary

```
BLOCK LAYER BOUNDARIES:

include/linux/blkdev.h             <-- Block device contract
include/linux/genhd.h              <-- Disk representation
      |
      v
+------------------------------------------------------------------+
|                         block/                                    |
|   blk-core.c | blk-merge.c | elevator.c | genhd.c                |
+------------------------------------------------------------------+
      |
      | struct block_device_operations
      v
+------------------------------------------------------------------+
|  drivers/block/       |  drivers/scsi/      |  drivers/nvme/     |
|  drivers/ata/         |  drivers/mmc/       |                    |
+------------------------------------------------------------------+
```

### 2.4 Directory Structure as Boundary Expression

| Directory | Boundary Role | Includes From |
|-----------|---------------|---------------|
| `include/linux/` | Public kernel API | Standard library |
| `include/net/` | Network subsystem internal | `linux/`, `asm/` |
| `include/asm-generic/` | Architecture abstractions | `linux/` |
| `arch/*/include/` | Architecture-specific | `asm-generic/` |
| `fs/` | VFS and filesystems | `linux/`, `asm/` |
| `net/` | Network stack | `linux/`, `net/` |
| `drivers/` | Device drivers | `linux/` |
| `kernel/` | Core kernel services | `linux/`, `asm/` |

---

## Step 3 — Contracts Enforced at Each Boundary

### 3.1 VFS Contracts

#### API Contract: file_operations

```c
/* The VFS calls filesystem through function pointers.
 * Filesystem MUST implement required operations.
 * VFS guarantees certain preconditions.
 */

/* CONTRACT: read operation */
ssize_t (*read)(struct file *file, char __user *buf, size_t count, loff_t *pos);

/* Preconditions (VFS guarantees):
 * - file is valid and opened for reading
 * - buf points to user-space buffer
 * - pos points to valid file position
 * 
 * Postconditions (filesystem must ensure):
 * - Returns bytes read, 0 for EOF, negative errno on error
 * - *pos updated to new position
 * - buf filled with file data
 * 
 * Ownership:
 * - file: VFS owns, filesystem borrows
 * - buf: user owns, filesystem copies to
 * - pos: VFS owns, filesystem updates
 */
```

#### Data Ownership Contract

```c
/* include/linux/fs.h - inode lifecycle contract */

struct super_operations {
    /* VFS calls to CREATE inode - filesystem allocates */
    struct inode *(*alloc_inode)(struct super_block *sb);
    
    /* VFS calls to DESTROY inode - filesystem deallocates */
    void (*destroy_inode)(struct inode *);
    
    /* VFS calls when inode is dirty - filesystem persists */
    void (*dirty_inode)(struct inode *, int flags);
    
    /* VFS calls to write inode - filesystem writes to storage */
    int (*write_inode)(struct inode *, struct writeback_control *wbc);
};

/* OWNERSHIP INVARIANT:
 * - VFS manages inode reference counting (i_count)
 * - Filesystem manages on-disk representation
 * - VFS calls destroy_inode when i_count reaches 0
 * - Filesystem must not free inode directly
 */
```

#### Control Flow Contract

```c
/* Registration contract - filesystem announces itself */
int register_filesystem(struct file_system_type *fs);

struct file_system_type {
    const char *name;
    int fs_flags;
    
    /* VFS calls this to mount - filesystem creates super_block */
    struct dentry *(*mount)(struct file_system_type *, int,
                            const char *, void *);
    
    /* VFS calls this to unmount - filesystem cleans up */
    void (*kill_sb)(struct super_block *);
    
    struct module *owner;
    struct file_system_type *next;    /* VFS manages linked list */
};

/* CONTROL FLOW:
 * 1. Filesystem calls register_filesystem() at init
 * 2. User mounts -> VFS calls fs->mount()
 * 3. User unmounts -> VFS calls fs->kill_sb()
 * 4. Filesystem calls unregister_filesystem() at exit
 */
```

### 3.2 Network Stack Contracts

#### Socket/Protocol Contract

```c
/* include/linux/net.h:161-209 */
struct proto_ops {
    int family;
    struct module *owner;
    
    /* Socket lifecycle */
    int (*release)(struct socket *sock);
    int (*bind)(struct socket *sock, struct sockaddr *myaddr, int sockaddr_len);
    int (*connect)(struct socket *sock, struct sockaddr *vaddr, 
                   int sockaddr_len, int flags);
    int (*accept)(struct socket *sock, struct socket *newsock, int flags);
    
    /* Data transfer */
    int (*sendmsg)(struct kiocb *iocb, struct socket *sock,
                   struct msghdr *m, size_t total_len);
    int (*recvmsg)(struct kiocb *iocb, struct socket *sock,
                   struct msghdr *m, size_t total_len, int flags);
};

/* CONTRACT:
 * - Socket layer calls proto_ops for all socket operations
 * - Protocol family implements proto_ops completely
 * - Protocol must handle all socket states correctly
 * - Error returns follow errno convention
 */
```

#### Protocol/Device Contract

```c
/* include/linux/netdevice.h:859-951 */
struct net_device_ops {
    int (*ndo_init)(struct net_device *dev);
    void (*ndo_uninit)(struct net_device *dev);
    int (*ndo_open)(struct net_device *dev);
    int (*ndo_stop)(struct net_device *dev);
    
    /* THE CRITICAL HOT PATH CONTRACT */
    netdev_tx_t (*ndo_start_xmit)(struct sk_buff *skb, struct net_device *dev);
    
    /* XMIT CONTRACT:
     * - Returns NETDEV_TX_OK: driver consumed skb
     * - Returns NETDEV_TX_BUSY: driver rejected, caller retries
     * - Driver MUST NOT free skb on NETDEV_TX_BUSY
     * - Driver MUST free skb after successful transmission
     */
    
    void (*ndo_tx_timeout)(struct net_device *dev);
    void (*ndo_set_rx_mode)(struct net_device *dev);
    int (*ndo_set_mac_address)(struct net_device *dev, void *addr);
    int (*ndo_change_mtu)(struct net_device *dev, int new_mtu);
};
```

#### sk_buff Ownership Contract

```c
/* The sk_buff (socket buffer) ownership is critical */

/* RULE 1: Creator owns until explicit transfer */
struct sk_buff *skb = alloc_skb(size, GFP_KERNEL);  /* Caller owns */

/* RULE 2: Passing to transmit transfers ownership */
dev_queue_xmit(skb);  /* Network stack now owns skb */

/* RULE 3: Receiver must consume or free */
/* In driver receive path: */
netif_rx(skb);  /* Ownership transferred to network stack */
/* OR */
kfree_skb(skb); /* Driver frees if not forwarding */

/* RULE 4: Clone for multiple consumers */
struct sk_buff *clone = skb_clone(skb, GFP_ATOMIC);
/* Now both skb and clone can be independently consumed */
```

### 3.3 Performance Contracts

```c
/* Hot path vs cold path distinction */

/* HOT PATH - Called per packet, must be fast */
netdev_tx_t (*ndo_start_xmit)(struct sk_buff *skb, struct net_device *dev);
int netif_rx(struct sk_buff *skb);
ssize_t (*read)(struct file *, char __user *, size_t, loff_t *);

/* COLD PATH - Called rarely, can be slower */
int (*ndo_open)(struct net_device *dev);
int register_filesystem(struct file_system_type *fs);
int (*mount)(struct file_system_type *, int, const char *, void *);

/* Performance contract implications:
 * - Hot paths must not sleep (use GFP_ATOMIC)
 * - Hot paths must not acquire contended locks
 * - Cold paths may sleep, do I/O, acquire locks
 * - Hot path must be O(1) or O(log n) at worst
 */
```

### 3.4 Error Propagation Contract

```c
/* Standard error convention throughout kernel */

/* Return value convention:
 * Success: 0 or positive value
 * Failure: negative errno value
 */

int vfs_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
    /* ... */
    if (!file->f_op->read)
        return -EINVAL;
    
    ret = file->f_op->read(file, buf, count, pos);
    if (ret < 0)
        return ret;  /* Propagate filesystem error */
    
    return ret;  /* Return bytes read */
}

/* For pointers: */
struct inode *inode = iget_locked(sb, ino);
if (IS_ERR(inode))
    return PTR_ERR(inode);  /* Convert to errno */
```

---

## Step 4 — Dependency Rules & Direction

### 4.1 Dependency Diagram

```
KERNEL DEPENDENCY DIRECTION (Allowed: Down and Same Level)

+=============+     +=============+     +=============+
| User Space  |     | User Space  |     | User Space  |
+======+======+     +======+======+     +======+======+
       |                   |                   |
       v                   v                   v
+======+==========================================+======+
|                SYSTEM CALL LAYER                       |
|        (Entry point, parameter validation)             |
+=======================+================================+
                        |
    +-------------------+-------------------+
    v                   v                   v
+========+        +========+        +========+
|  VFS   |------->| Network|        |  MM    |
+===+====+        +===+====+        +===+====+
    |                 |                 |
    v                 v                 v
+========+        +========+        +========+
|  FS    |        |Protocol|        | SLAB   |
| (ext4) |        | (TCP)  |        |        |
+===+====+        +===+====+        +========+
    |                 |
    v                 v
+========+        +========+
| Block  |        | NetDev |
| Layer  |        | Layer  |
+===+====+        +===+====+
    |                 |
    v                 v
+========+        +========+
| Block  |        | NIC    |
| Driver |        | Driver |
+===+====+        +===+====+
    |                 |
    v                 v
+========================================+
|            HARDWARE                    |
+========================================+

FORBIDDEN: Any arrow pointing upward
```

**说明:**
- 依赖方向始终向下或同层
- VFS 调用文件系统，文件系统不能调用 VFS 的内部函数
- 网络协议调用设备驱动，驱动不能调用协议层
- 驱动向上通知通过回调机制，不是直接调用

### 4.2 Include Rules

| Source Layer | Allowed Includes | Forbidden Includes |
|--------------|------------------|---------------------|
| User space | UAPI headers only | Any internal kernel headers |
| System calls | `linux/*.h`, `asm/*.h` | `*_internal.h` |
| VFS | `linux/fs.h`, `linux/dcache.h` | Specific filesystem headers |
| Filesystem | VFS headers, block headers | Other filesystem internals |
| Block layer | `linux/blkdev.h` | Driver-specific headers |
| Drivers | Layer-specific headers | Subsystem internals |

### 4.3 Why Reversing Dependencies Breaks Things

```c
/* VIOLATION EXAMPLE: Filesystem calling VFS internals */

/* fs/ext4/inode.c - HYPOTHETICAL VIOLATION */
#include "../internal.h"  /* FORBIDDEN: VFS internal header */

int ext4_some_operation(struct inode *inode)
{
    /* VIOLATION: Calling VFS internal function */
    __some_vfs_internal_function(inode);  
}

/* CONSEQUENCES:
 * 1. VFS cannot refactor internal functions
 * 2. ext4 breaks when VFS changes internals
 * 3. Other filesystems might not have this assumption
 * 4. Testing VFS changes becomes impossible
 * 5. ext4 cannot be a loadable module (unresolved symbol)
 */
```

### 4.4 Dependency Inversion via Callbacks

```c
/* CORRECT: Lower layer notifies upper layer via registered callback */

/* Network device signals link state change */
void netif_carrier_on(struct net_device *dev);
void netif_carrier_off(struct net_device *dev);

/* Driver calls these - they notify registered listeners */
/* Driver doesn't know WHO is listening */
/* Driver doesn't include protocol headers */

/* Registration (by upper layers): */
register_netdevice_notifier(&my_notifier);

/* This is NOT a dependency violation because:
 * 1. Driver doesn't know about notifier internals
 * 2. Driver just calls generic netif_* functions
 * 3. Registration is done by upper layer
 */
```

---

## Step 5 — How Violations Creep In (Architecture Decay)

### 5.1 Convenience-Driven Shortcuts

```c
/* VIOLATION: Direct access to internal structure */

/* fs/some_fs/file.c */
int some_fs_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
    struct inode *inode = file->f_path.dentry->d_inode;
    
    /* SHORTCUT: Directly accessing internal scheduling state */
    if (current->state != TASK_RUNNING) {
        /* Checking scheduler internals */
    }
    
    /* SHORTCUT: Direct memory mapping manipulation */
    if (file->f_mapping->nrpages > SOME_THRESHOLD) {
        /* Bypassing proper page cache API */
        struct page *page = file->f_mapping->page_tree.rnode;
    }
}

/* WHY IT SEEMS HARMLESS:
 * - "It's just reading a value"
 * - "This is faster than calling an API"
 * - "The internal structure is stable"
 *
 * LONG-TERM DAMAGE:
 * - page_tree changed to xarray in later kernels
 * - Internal structure layouts changed for cache optimization
 * - All such shortcuts became bugs
 */
```

### 5.2 Performance-Driven Exceptions

```c
/* VIOLATION: Bypassing layer for performance */

/* net/ipv4/tcp_output.c - HYPOTHETICAL */
int tcp_send_skb(struct sock *sk, struct sk_buff *skb)
{
    struct net_device *dev = skb->dev;
    
    /* SHORTCUT: Skip qdisc for "fast path" */
    if (dev->flags & IFF_LOOPBACK) {
        /* Direct call to driver, bypassing traffic control */
        return dev->netdev_ops->ndo_start_xmit(skb, dev);  /* VIOLATION */
    }
    
    return dev_queue_xmit(skb);  /* Correct path */
}

/* WHY IT SEEMS HARMLESS:
 * - "Loopback doesn't need traffic control"
 * - "This saves CPU cycles"
 * - "The optimization is well-contained"
 *
 * LONG-TERM DAMAGE:
 * - Traffic control features don't work on loopback
 * - Debugging tools can't observe loopback traffic
 * - Network namespaces may not be respected
 * - BPF programs attached to qdisc are bypassed
 */
```

### 5.3 Debugging/Logging Leaks

```c
/* VIOLATION: Debug code reaching across layers */

/* drivers/net/ethernet/some_driver.c */
#include <net/tcp.h>  /* VIOLATION: Driver including transport header */

int some_driver_xmit(struct sk_buff *skb, struct net_device *dev)
{
    /* Debug code that reaches into TCP */
    if (skb->protocol == htons(ETH_P_IP)) {
        struct tcphdr *th = tcp_hdr(skb);
        pr_debug("TCP seq=%u, ack=%u\n", 
                 ntohl(th->seq), ntohl(th->ack_seq));  /* VIOLATION */
    }
    
    /* Actual transmission */
    return do_xmit(skb, dev);
}

/* WHY IT SEEMS HARMLESS:
 * - "It's just debug logging"
 * - "It's behind #ifdef DEBUG"
 * - "It helps troubleshoot issues"
 *
 * LONG-TERM DAMAGE:
 * - Driver now depends on TCP header layout
 * - Driver must be rebuilt when TCP headers change
 * - Encourages more cross-layer dependencies
 * - Debug code often becomes permanent
 */
```

### 5.4 Feature Creep

```c
/* VIOLATION: Feature that spans multiple layers */

/* A feature that requires coordinated changes */

/* include/linux/fs.h */
struct file_operations {
    /* ... existing operations ... */
    
    /* NEW: Feature-specific operation added to VFS */
    int (*custom_feature)(struct file *, struct custom_params *);
};

/* fs/ext4/file.c */
static int ext4_custom_feature(struct file *file, struct custom_params *params)
{
    /* Implementation requires knowledge of:
     * - VFS internals
     * - Block layer internals  
     * - Memory management internals
     */
    struct address_space *mapping = file->f_mapping;
    
    /* VIOLATION: Direct block allocation bypassing proper API */
    ext4_get_blocks_direct(inode, params);  /* Internal function */
}

/* WHY IT SEEMS HARMLESS:
 * - "The feature needs tight integration"
 * - "Performance requires direct access"
 * - "We control all the layers"
 *
 * LONG-TERM DAMAGE:
 * - Feature cannot be disabled without breaking ABI
 * - Other filesystems must implement or stub the operation
 * - Testing matrix explodes
 * - Maintenance burden across multiple subsystems
 */
```

### 5.5 Team/Ownership Changes

```c
/* VIOLATION: Knowledge loss leading to inappropriate changes */

/* Original design (documented):
 * - inode->i_private is for filesystem-specific data
 * - Only the owning filesystem should access it
 */

/* After team change, new developer adds: */

/* fs/some_other_fs/inode.c */
int some_other_fs_do_thing(struct inode *inode)
{
    /* VIOLATION: Assuming i_private format from another fs */
    struct ext4_inode_info *ei = inode->i_private;
    
    /* "ext4 stores something useful here" */
    if (ei->i_flags & EXT4_SOME_FLAG) {
        /* ... */
    }
}

/* WHY IT SEEMS HARMLESS:
 * - "I saw ext4 does this"
 * - "The struct layout is public"
 * - "It works in my testing"
 *
 * LONG-TERM DAMAGE:
 * - Works only when underlying fs is ext4
 * - Silent corruption on other filesystems
 * - Debugging nightmare
 * - ext4 cannot change its i_private layout
 */
```

### 5.6 Summary: Violation Patterns

| Violation Type | Code Smell | Initial Justification | Long-term Damage |
|----------------|------------|----------------------|------------------|
| Convenience shortcut | Direct struct access | "Just reading" | API changes break it |
| Performance bypass | Layer skipping | "Faster" | Features don't work |
| Debug leak | Cross-layer includes | "Just logging" | Build dependencies |
| Feature creep | Multi-layer changes | "Tight integration" | Maintenance nightmare |
| Knowledge loss | Wrong assumptions | "I saw it work" | Silent bugs |

---

## Step 6 — Existing Safeguards (or Lack Thereof)

### 6.1 Coding Conventions (Documentation/CodingStyle)

```
SAFEGUARD: Naming conventions signal intent

/* Public API */
int vfs_read(struct file *, char __user *, size_t, loff_t *);
int register_filesystem(struct file_system_type *);

/* Internal functions (double underscore prefix) */
int __mark_inode_dirty(struct inode *, int);
void __destroy_inode(struct inode *);

/* Static functions (file scope) */
static int validate_superblock(struct super_block *sb);

/* ENFORCEMENT: Convention only, compiler doesn't check */
```

### 6.2 File Layout Discipline

```
SAFEGUARD: Internal headers in specific locations

fs/
├── internal.h          <-- VFS-internal, not installed
├── read_write.h        <-- Shared within fs/
├── pnode.h             <-- Namespace-internal
└── ext4/
    ├── ext4.h          <-- ext4-internal
    └── ext4_jbd2.h     <-- ext4-jbd2 bridge

include/linux/
├── fs.h                <-- Public VFS API
└── dcache.h            <-- Public dentry API

/* ENFORCEMENT: Not installed = not available to modules */
/* WEAKNESS: In-tree code can still include ../internal.h */
```

### 6.3 EXPORT_SYMBOL Discipline

```c
/* SAFEGUARD: Only exported symbols available to modules */

/* Exported - modules can use */
EXPORT_SYMBOL(vfs_read);
EXPORT_SYMBOL(register_filesystem);
EXPORT_SYMBOL_GPL(generic_file_read_iter);  /* GPL-only modules */

/* Not exported - in-tree only */
static int some_internal_function(void);  /* Not even linkable */
int another_internal_function(void);       /* Linkable but discouraged */

/* ENFORCEMENT: 
 * - Modules fail to load with unresolved symbols
 * - EXPORT_SYMBOL_GPL requires GPL module license
 * 
 * WEAKNESS:
 * - In-tree code can still call non-exported functions
 * - No enforcement of "should not use" vs "cannot use"
 */
```

### 6.4 Comments and Documentation

```c
/* SAFEGUARD: Explicit warnings in code */

/* include/linux/fs.h:1459-1462 */
/*
 * The next field is for VFS *only*. No filesystems have any business
 * even looking at it. You had been warned.
 */
struct mutex s_vfs_rename_mutex;    /* Kludge */

/* include/net/sock.h:66 */
/*
 * This structure really needs to be cleaned up.
 * Most of it is for TCP, and not used by any of
 * the other protocols.
 */

/* ENFORCEMENT: None. Relies on developers reading comments. */
```

### 6.5 Review Culture (Implied)

```
SAFEGUARD: Patch review catches violations

Linux kernel development process:
1. Patch submitted to mailing list
2. Subsystem maintainer reviews
3. Cross-subsystem changes require multiple acks
4. Violations caught by experienced reviewers

/* ENFORCEMENT:
 * - Human review, not automated
 * - Depends on reviewer knowledge
 * - Historical violations exist in codebase
 */
```

### 6.6 What is NOT Protected

| Aspect | Status | Risk |
|--------|--------|------|
| In-tree code accessing internals | UNPROTECTED | High - nothing prevents it |
| Internal header includes | UNPROTECTED | Medium - convention only |
| Cross-subsystem assumptions | UNPROTECTED | High - no automated check |
| Performance shortcuts | UNPROTECTED | High - tempting optimization |
| Struct layout assumptions | UNPROTECTED | High - changes break silently |
| Callback contract compliance | UNPROTECTED | Medium - tested at runtime |

---

## Step 7 — Stability vs Volatility Analysis

### 7.1 Stability Spectrum

```
MOST STABLE <<<-------------------------------->>> MOST VOLATILE

+------------+------------+------------+------------+------------+
|  System    |  Core      |  Major     |  Device    |  Config    |
|  Call ABI  |  Data      |  Subsystem |  Drivers   |  Options   |
|            |  Structs   |  APIs      |            |            |
+------------+------------+------------+------------+------------+
    |             |             |             |             |
    |             |             |             |             |
 Never          Rarely        Sometimes     Often       Frequently
 changes       changes        changes      changes      changes
```

**说明:**
- 系统调用 ABI: 几乎永不改变，用户空间兼容性是最高优先级
- 核心数据结构: 很少改变，但会添加新字段
- 主要子系统 API: 有时改变，但有弃用周期
- 设备驱动: 经常改变，因为是内核内部接口
- 配置选项: 频繁改变，允许新功能和实验

### 7.2 Stable Boundaries

```c
/* STABLE: System Call Interface */
/* include/linux/syscalls.h */

/* These signatures are frozen - changing them breaks userspace */
asmlinkage long sys_read(unsigned int fd, char __user *buf, size_t count);
asmlinkage long sys_write(unsigned int fd, const char __user *buf, size_t count);
asmlinkage long sys_open(const char __user *filename, int flags, umode_t mode);

/* STABLE: Core VFS operations structure layout */
/* New fields added at END only, existing fields never removed */
struct file_operations {
    struct module *owner;           /* Position 0 - never moves */
    loff_t (*llseek)(struct file *, loff_t, int);  /* Position 1 */
    ssize_t (*read)(struct file *, char __user *, size_t, loff_t *);
    /* ... */
    /* NEW operations added HERE at end */
};

/* STABLE: Socket API */
struct sockaddr {
    sa_family_t sa_family;          /* Address family - stable */
    char sa_data[14];               /* Address data - stable size */
};
```

### 7.3 Volatile Boundaries

```c
/* VOLATILE: Driver APIs change frequently */

/* Between 2.6 and 3.x, network driver API changed: */

/* OLD (2.6): */
int (*hard_start_xmit)(struct sk_buff *skb, struct net_device *dev);

/* NEW (3.x): */
netdev_tx_t (*ndo_start_xmit)(struct sk_buff *skb, struct net_device *dev);

/* The entire net_device_ops structure was introduced in 2.6.29
 * Before that, operations were directly in struct net_device
 */

/* VOLATILE: Internal allocation APIs */
/* These change with memory management improvements */

/* kmalloc flags have evolved */
/* GFP_ATOMIC, GFP_KERNEL stable */
/* GFP_NOFS, GFP_NOIO meanings refined over time */
```

### 7.4 How Contracts Protect Stable Parts

```c
/* The operations table pattern protects stability */

/* VFS doesn't care about inode internals */
struct inode *inode = iget_locked(sb, ino);

/* VFS accesses inode ONLY through:
 * 1. Public fields defined in struct inode
 * 2. Operations in inode->i_op
 * 3. Operations in inode->i_fop
 */

/* Filesystem can change internal representation freely */
struct ext4_inode_info {
    struct inode vfs_inode;  /* Must be included */
    /* ext4-specific fields - can change freely */
    __le32 i_data[15];
    __u32 i_flags;
    /* ... */
};

/* Contract: VFS uses container_of to get ext4_inode_info
 * but never accesses ext4-specific fields */
```

### 7.5 Where Instability Leaks

```c
/* LEAK: Performance optimizations expose internals */

/* The page cache uses radix trees internally */
/* fs/inode.c and other VFS code sometimes assumes this */

/* When radix tree was replaced with xarray:
 * - All code assuming radix_tree_* broke
 * - Even though it was "internal"
 */

/* LEAK: Debug/tracing code */

/* Tracepoints expose internal state */
trace_block_rq_insert(rq);  /* Exposes request structure */

/* Change request structure -> tracepoint users affected */
/* Even though they "shouldn't depend on format" */

/* LEAK: /proc and /sys interfaces */

/* /proc/net/tcp exposes internal socket state */
/* Format becomes de-facto ABI even though undocumented */
```

---

## Step 8 — Stress-Test the Architecture

### 8.1 Adding a Major New Feature

**Scenario: Add a new network protocol (QUIC in kernel)**

```
BOUNDARY IMPACT ANALYSIS:

+------------------------------------------------------------------+
| Socket Layer     | HOLDS - proto_ops abstraction sufficient      |
|                  | New protocol registers with sock_register()   |
+------------------------------------------------------------------+
| Transport Layer  | STRESSED - QUIC has different semantics       |
|                  | May need new proto operations or extensions   |
+------------------------------------------------------------------+
| Network Layer    | HOLDS - IP layer unchanged                    |
|                  | QUIC just another protocol number             |
+------------------------------------------------------------------+
| Device Layer     | HOLDS - No changes needed                     |
|                  | QUIC packets are just UDP at wire level       |
+------------------------------------------------------------------+
```

**Required Changes:**
- New `net/quic/` directory
- New `proto_ops` implementation  
- Registration with `sock_register()`
- Possible new socket options

**Boundaries Respected:**
- No changes to lower layers
- New protocol isolated in own directory
- Uses existing registration mechanisms

### 8.2 Optimizing Performance on Hot Path

**Scenario: Optimize TCP fast path**

```
BOUNDARY STRESS ANALYSIS:

Optimization: Bypass netfilter for established connections

+------------------------------------------------------------------+
| Netfilter Boundary | BROKEN - Optimization bypasses hooks        |
|                    | Security policies won't apply               |
+------------------------------------------------------------------+
| Routing Boundary   | STRESSED - May need to cache route          |
|                    | Route changes might not be noticed          |
+------------------------------------------------------------------+
| Driver Boundary    | HOLDS - Still goes through ndo_start_xmit   |
+------------------------------------------------------------------+
```

**Refactoring Required:**
```c
/* Instead of bypassing netfilter, add fast-path WITHIN netfilter */

/* net/netfilter/nf_conntrack_core.c */
static inline bool nf_conntrack_fastpath_ok(struct sk_buff *skb)
{
    /* Check if we can take fast path */
    struct nf_conn *ct = nf_ct_get(skb, &ctinfo);
    return ct && ct->status & IPS_CONFIRMED;
}

/* This respects the boundary while enabling optimization */
```

### 8.3 Removing/Replacing a Subsystem

**Scenario: Replace block I/O scheduler**

```
BOUNDARY ANALYSIS:

+------------------------------------------------------------------+
| Filesystems      | HOLDS - Use block_device abstraction          |
|                  | Don't know about scheduler internals          |
+------------------------------------------------------------------+
| Block Layer API  | MOSTLY HOLDS - submit_bio() unchanged         |
|                  | But internal elevator_* APIs change           |
+------------------------------------------------------------------+
| Scheduler        | REPLACED - New BFQ replaces CFQ               |
|                  | Internal implementation completely different   |
+------------------------------------------------------------------+
| Drivers          | HOLDS - request structure stable enough       |
|                  | queue management abstracted                   |
+------------------------------------------------------------------+
```

**Actual Example: BFQ Scheduler Addition**

The boundary worked because:
1. Filesystems use `submit_bio()` - unchanged
2. Drivers implement `request_fn` - unchanged  
3. Only `block/` internals changed
4. Scheduler selection is policy (user config)

### 8.4 Handing Project to New Team

**Scenario: New maintainer takes over ext4**

```
BOUNDARY BENEFITS:

+------------------------------------------------------------------+
| What New Team Must Understand:                                    |
|   - ext4 internal structures in fs/ext4/                         |
|   - VFS contract (file_operations, inode_operations, etc.)       |
|   - Block layer API (submit_bio, etc.)                           |
|   - JBD2 journaling interface                                    |
+------------------------------------------------------------------+
| What New Team Does NOT Need to Understand:                        |
|   - Other filesystems (btrfs, xfs, etc.)                         |
|   - Network stack                                                 |
|   - Memory management internals                                   |
|   - Scheduler internals                                          |
|   - Driver specifics                                             |
+------------------------------------------------------------------+
```

**Boundaries that Help:**
1. `fs/ext4/` is self-contained
2. Public contracts documented in `include/linux/fs.h`
3. ext4 can be understood without kernel-wide knowledge

**Boundaries at Risk:**
1. Undocumented assumptions about VFS behavior
2. Performance tricks that depend on MM internals
3. Historical workarounds for other subsystem bugs

---

## Step 9 — Architecture Lessons Extracted

### Lesson 1: Operations Tables as Contracts

**Principle:** Use tables of function pointers to define contracts between layers.

**Linux Application:**
```c
struct file_operations {
    ssize_t (*read)(struct file *, char __user *, size_t, loff_t *);
    ssize_t (*write)(struct file *, const char __user *, size_t, loff_t *);
    /* ... */
};
```

**How to Apply:**
```c
/* Define contract as operations structure */
struct my_storage_ops {
    int (*read)(struct storage_context *, void *buf, size_t len);
    int (*write)(struct storage_context *, const void *buf, size_t len);
    int (*sync)(struct storage_context *);
};

/* Upper layer stores pointer, never knows implementation */
struct my_file {
    struct my_storage_ops *ops;
    struct storage_context *ctx;
};

/* Call through contract, not directly */
int my_read(struct my_file *f, void *buf, size_t len) {
    return f->ops->read(f->ctx, buf, len);
}
```

### Lesson 2: Registration Patterns for Extensibility

**Principle:** Lower layers should not know about higher layers. Use registration.

**Linux Application:**
```c
/* Filesystem registers itself */
static struct file_system_type ext4_fs_type = {
    .name = "ext4",
    .mount = ext4_mount,
    .kill_sb = kill_block_super,
};

static int __init ext4_init(void)
{
    return register_filesystem(&ext4_fs_type);
}
```

**How to Apply:**
```c
/* Define registration interface */
struct protocol_handler {
    const char *name;
    int (*handle)(struct message *msg);
    struct list_head list;
};

static LIST_HEAD(protocol_handlers);

int register_protocol(struct protocol_handler *handler) {
    list_add(&handler->list, &protocol_handlers);
    return 0;
}

/* Core doesn't include handler headers */
/* Core just iterates registered handlers */
```

### Lesson 3: Reference Counting for Ownership

**Principle:** Explicit reference counting prevents ownership ambiguity.

**Linux Application:**
```c
/* Every major kernel object has reference count */
struct inode {
    atomic_t i_count;  /* Reference count */
    /* ... */
};

struct inode *iget_locked(struct super_block *sb, unsigned long ino);  /* +1 ref */
void iput(struct inode *inode);  /* -1 ref, may free */
```

**How to Apply:**
```c
struct my_resource {
    atomic_t refcount;
    /* resource data */
};

struct my_resource *my_resource_get(struct my_resource *res) {
    atomic_inc(&res->refcount);
    return res;
}

void my_resource_put(struct my_resource *res) {
    if (atomic_dec_and_test(&res->refcount))
        my_resource_free(res);
}
```

### Lesson 4: Internal vs External Headers

**Principle:** Separate public API from internal implementation.

**Linux Application:**
```
include/linux/fs.h      <- Public, stable
fs/internal.h           <- Private, can change
```

**How to Apply:**
```
include/mylib.h         <- Public API
src/internal.h          <- Implementation details
src/mylib.c             <- Implementation

/* Public header */
typedef struct my_handle my_handle_t;  /* Opaque */
my_handle_t *my_create(void);
void my_destroy(my_handle_t *h);

/* Internal header */
struct my_handle {
    int internal_field;
    void *private_data;
};
```

### Lesson 5: Naming Conventions Signal Intent

**Principle:** Use naming to indicate scope and stability.

**Linux Application:**
```c
vfs_read()              /* Public VFS API */
__mark_inode_dirty()    /* Internal, use with care */
do_sys_open()           /* System call implementation */
```

**How to Apply:**
```c
/* Public API */
int mymod_operation(struct mymod *m);

/* Module internal */
int mymod__internal_helper(struct mymod *m);

/* File-local */
static int validate_params(struct mymod *m);
```

### Lesson 6: Error Handling Must Be Contractual

**Principle:** Define error representation at boundaries.

**Linux Application:**
```c
/* Universal convention: negative errno on error */
ssize_t vfs_read(...) {
    if (error_condition)
        return -EINVAL;
    return bytes_read;  /* Positive = success */
}
```

**How to Apply:**
```c
/* Document error contract explicitly */
/**
 * my_operation - does something
 * @param: input parameter
 *
 * Return: 0 on success, negative errno on failure
 *         -EINVAL: invalid parameter
 *         -ENOMEM: allocation failed
 *         -EIO: I/O error
 */
int my_operation(int param);
```

### Lesson 7: Hot Path vs Cold Path Separation

**Principle:** Performance-critical paths have different rules.

**Linux Application:**
```c
/* Hot path - cannot sleep, must be fast */
netdev_tx_t ndo_start_xmit(struct sk_buff *skb, struct net_device *dev);

/* Cold path - can sleep, can do I/O */
int ndo_open(struct net_device *dev);
```

**How to Apply:**
```c
/* Document path type */
/**
 * HOT PATH - Called per request
 * Must not sleep or block
 * Must complete in bounded time
 */
int process_request(struct request *req);

/**
 * COLD PATH - Called once at startup
 * May sleep, may do I/O
 */
int initialize_system(struct config *cfg);
```

### Lesson Summary Table

| Lesson | Kernel Example | Application Guideline |
|--------|----------------|----------------------|
| Operations tables | `file_operations` | Define contracts as function pointer structs |
| Registration | `register_filesystem()` | Lower layers don't know upper layers |
| Reference counting | `iget()`/`iput()` | Explicit ownership with refcounts |
| Header separation | `fs.h` vs `internal.h` | Public vs private headers |
| Naming conventions | `vfs_*`, `__*` | Names signal scope and stability |
| Error contracts | `-EINVAL`, `-ENOMEM` | Consistent error representation |
| Hot/cold paths | `ndo_start_xmit` vs `ndo_open` | Different rules for different paths |

---

## Appendix: Quick Reference

### VFS Contract Quick Reference

```c
/* Filesystem must implement: */
struct file_system_type {
    .mount = my_mount,      /* Create superblock */
    .kill_sb = kill_sb,     /* Destroy superblock */
};

struct super_operations {
    .alloc_inode = my_alloc_inode,    /* Optional: custom inode */
    .destroy_inode = my_destroy_inode,
    .write_inode = my_write_inode,    /* Persist inode */
    .statfs = my_statfs,              /* Filesystem stats */
};

struct inode_operations {
    .lookup = my_lookup,    /* Find child dentry */
    .create = my_create,    /* Create file */
    .mkdir = my_mkdir,      /* Create directory */
    .unlink = my_unlink,    /* Remove file */
};

struct file_operations {
    .read = my_read,        /* Read file content */
    .write = my_write,      /* Write file content */
    .open = my_open,        /* Open file */
    .release = my_release,  /* Close file */
};
```

### Network Protocol Quick Reference

```c
/* Protocol family must implement: */
struct net_proto_family {
    .family = AF_MY_PROTOCOL,
    .create = my_proto_create,  /* Create socket */
};

struct proto_ops {
    .release = my_release,      /* Close socket */
    .bind = my_bind,            /* Bind address */
    .connect = my_connect,      /* Connect to peer */
    .accept = my_accept,        /* Accept connection */
    .sendmsg = my_sendmsg,      /* Send data */
    .recvmsg = my_recvmsg,      /* Receive data */
};

/* Network device must implement: */
struct net_device_ops {
    .ndo_open = my_open,            /* Enable interface */
    .ndo_stop = my_stop,            /* Disable interface */
    .ndo_start_xmit = my_xmit,      /* Transmit packet */
    .ndo_set_mac_address = my_set_mac,
};
```

### Boundary Review Checklist (Linux-Specific)

| Check | Good | Bad |
|-------|------|-----|
| **Includes** | `#include <linux/fs.h>` | `#include "../internal.h"` |
| **Function calls** | `vfs_read()` | `__some_internal_func()` |
| **Struct access** | `file->f_op->read()` | `file->f_mapping->page_tree` |
| **Registration** | `register_filesystem()` | Static initialization |
| **Error returns** | `return -EINVAL` | `return 1` for error |
| **Ownership** | `iget()`/`iput()` pairs | Direct free without ref check |

---

*This document analyzes the Linux kernel v3.2 architecture. Later versions may have evolved these patterns, but the principles remain applicable.*

