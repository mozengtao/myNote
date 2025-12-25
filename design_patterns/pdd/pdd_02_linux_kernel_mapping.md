# PDD Mapping to Linux Kernel Subsystems (v3.2)

## Introduction

```
+------------------------------------------------------------------+
|  THE LINUX KERNEL IS A PDD MASTERCLASS                           |
+------------------------------------------------------------------+

    The Linux kernel embodies PDD principles through:
    
    1. SYSCALL LAYER (Presentation)
       - Translates user-space calls to internal operations
       - Validates parameters, copies from user space
       
    2. SUBSYSTEM CORE (Domain)
       - VFS, network stack, scheduler, memory manager
       - Defines policies, invariants, algorithms
       
    3. DRIVERS & HARDWARE ABSTRACTION (Data)
       - Device drivers, filesystem implementations
       - Hardware-specific mechanisms

    KEY INSIGHT:
    The kernel achieves this separation WITHOUT classes or inheritance.
    It uses ops tables (function pointer structs) as interfaces.
```

**中文解释：**
- Linux 内核通过三层体现 PDD 原则
- **系统调用层**：将用户空间调用转换为内部操作
- **子系统核心**：VFS、网络栈、调度器——定义策略和不变量
- **驱动和硬件抽象**：设备驱动、文件系统实现——硬件特定机制

---

## Subsystem 1: VFS (Virtual Filesystem Switch)

### 1.1 Overview

```
+------------------------------------------------------------------+
|  VFS: THE CANONICAL PDD EXAMPLE                                  |
+------------------------------------------------------------------+

    PURPOSE:
    Provide unified file operations regardless of underlying storage
    
    SCALE:
    - Supports 60+ filesystem types
    - Handles billions of file operations daily
    - Zero filesystem-specific code in syscall handlers
```

### 1.2 Presentation Layer Components

```
+------------------------------------------------------------------+
|  VFS PRESENTATION LAYER                                          |
+------------------------------------------------------------------+

    LOCATION: fs/read_write.c, fs/open.c, fs/ioctl.c
    
    ┌─────────────────────────────────────────────────────────────┐
    │  SYSCALL HANDLERS:                                          │
    │                                                              │
    │  SYSCALL_DEFINE3(read, fd, buf, count)                      │
    │  SYSCALL_DEFINE3(write, fd, buf, count)                     │
    │  SYSCALL_DEFINE3(open, filename, flags, mode)               │
    │  SYSCALL_DEFINE3(ioctl, fd, cmd, arg)                       │
    └─────────────────────────────────────────────────────────────┘

    RESPONSIBILITIES:
    - Validate user pointers (access_ok)
    - Copy data from/to user space (copy_from_user, copy_to_user)
    - Convert fd to struct file
    - Dispatch to file_operations
```

```c
/* fs/read_write.c - PRESENTATION layer */

SYSCALL_DEFINE3(read, unsigned int, fd, char __user *, buf, size_t, count)
{
    struct file *file;
    ssize_t ret = -EBADF;
    int fput_needed;

    /* PRESENTATION: Convert fd to internal representation */
    file = fget_light(fd, &fput_needed);
    if (file) {
        /* PRESENTATION: Validate position */
        loff_t pos = file_pos_read(file);
        
        /* DISPATCH TO DOMAIN */
        ret = vfs_read(file, buf, count, &pos);
        
        /* PRESENTATION: Update user-visible position */
        file_pos_write(file, pos);
        fput_light(file, fput_needed);
    }
    return ret;
}
```

**Key Ops Table: `file_operations`**

```c
/* include/linux/fs.h lines 1583-1611 */
struct file_operations {
    struct module *owner;
    loff_t (*llseek) (struct file *, loff_t, int);
    ssize_t (*read) (struct file *, char __user *, size_t, loff_t *);
    ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *);
    int (*open) (struct inode *, struct file *);
    int (*release) (struct inode *, struct file *);
    /* ... per-file presentation handlers ... */
};
```

### 1.3 Domain Layer Components

```
+------------------------------------------------------------------+
|  VFS DOMAIN LAYER                                                |
+------------------------------------------------------------------+

    LOCATION: fs/namei.c, fs/inode.c, fs/dcache.c, fs/super.c
    
    ┌─────────────────────────────────────────────────────────────┐
    │  DOMAIN POLICIES:                                           │
    │                                                              │
    │  • Path resolution algorithm (namei.c)                      │
    │  • Permission checking (inode->i_op->permission)            │
    │  • Inode lifecycle management (inode.c)                     │
    │  • Dentry cache policies (dcache.c)                         │
    │  • Mount policies (namespace.c)                             │
    └─────────────────────────────────────────────────────────────┘
```

**Key Domain Structs:**

```c
/* include/linux/fs.h - DOMAIN layer interfaces */

/* Inode operations - domain policies for file objects */
struct inode_operations {
    /* POLICY: How to resolve a name in this directory */
    struct dentry * (*lookup) (struct inode *, struct dentry *, 
                               struct nameidata *);
    
    /* POLICY: How to check if operation is permitted */
    int (*permission) (struct inode *, int);
    
    /* POLICY: How to create a new file */
    int (*create) (struct inode *, struct dentry *, int, struct nameidata *);
    
    /* POLICY: How to handle attribute changes */
    int (*setattr) (struct dentry *, struct iattr *);
};

/* Super operations - filesystem-wide domain policies */
struct super_operations {
    /* POLICY: How to allocate inodes */
    struct inode *(*alloc_inode)(struct super_block *sb);
    
    /* POLICY: What to do when inode becomes dirty */
    void (*dirty_inode) (struct inode *, int flags);
    
    /* POLICY: How to sync the filesystem */
    int (*sync_fs)(struct super_block *sb, int wait);
};
```

**Domain Invariants:**

```c
/* Domain enforces these invariants */

/* i_nlink modification MUST use these domain functions */
static inline void inc_nlink(struct inode *inode) {
    inode->__i_nlink++;
}

static inline void drop_nlink(struct inode *inode) {
    inode->__i_nlink--;  /* Domain tracks write events */
}

/* Permission check is a DOMAIN decision */
int inode_permission(struct inode *inode, int mask)
{
    /* Domain decides whether access is allowed */
    if (inode->i_op->permission)
        return inode->i_op->permission(inode, mask);
    return generic_permission(inode, mask);
}
```

### 1.4 Data Layer Components

```
+------------------------------------------------------------------+
|  VFS DATA LAYER                                                  |
+------------------------------------------------------------------+

    LOCATION: fs/ext4/, fs/xfs/, fs/nfs/, drivers/block/
    
    ┌─────────────────────────────────────────────────────────────┐
    │  DATA MECHANISMS:                                           │
    │                                                              │
    │  • Filesystem implementations (ext4, xfs, btrfs)            │
    │  • Block device drivers                                      │
    │  • Page cache mechanics (address_space_operations)          │
    │  • I/O scheduling (block/elevator.c)                        │
    └─────────────────────────────────────────────────────────────┘
```

**Key Data Interface:**

```c
/* address_space_operations - DATA layer contract */
struct address_space_operations {
    /* DATA: How to write page to storage */
    int (*writepage)(struct page *page, struct writeback_control *wbc);
    
    /* DATA: How to read page from storage */
    int (*readpage)(struct file *, struct page *);
    
    /* DATA: How to handle direct I/O */
    ssize_t (*direct_IO)(int, struct kiocb *, const struct iovec *iov,
                         loff_t offset, unsigned long nr_segs);
};

/* Example: ext4 implements DATA layer */
const struct address_space_operations ext4_aops = {
    .readpage       = ext4_readpage,
    .writepage      = ext4_writepage,
    .direct_IO      = ext4_direct_IO,
    /* ... ext4-specific storage mechanisms ... */
};
```

### 1.5 Module Boundaries

```
+------------------------------------------------------------------+
|  VFS MODULE BOUNDARY DIAGRAM                                     |
+------------------------------------------------------------------+

    User Space
    ────────────────────────────────────────────────────────────
        │ read(fd, buf, count)
        ▼
    ┌─────────────────────────────────────────────────────────────┐
    │  PRESENTATION: fs/read_write.c                              │
    │  ┌─────────────────────────────────────────────────────┐    │
    │  │ sys_read() → fget() → vfs_read()                    │    │
    │  └─────────────────────────────────────────────────────┘    │
    └─────────────────────────────────────────────────────────────┘
        │ file_operations.read()
        ▼
    ┌─────────────────────────────────────────────────────────────┐
    │  DOMAIN: fs/inode.c, fs/namei.c                             │
    │  ┌─────────────────────────────────────────────────────┐    │
    │  │ permission check → read validation → page request   │    │
    │  └─────────────────────────────────────────────────────┘    │
    └─────────────────────────────────────────────────────────────┘
        │ address_space_operations.readpage()
        ▼
    ┌─────────────────────────────────────────────────────────────┐
    │  DATA: fs/ext4/inode.c                                      │
    │  ┌─────────────────────────────────────────────────────┐    │
    │  │ ext4_readpage() → block mapping → submit_bio()      │    │
    │  └─────────────────────────────────────────────────────┘    │
    └─────────────────────────────────────────────────────────────┘
        │
        ▼
    Hardware (disk)
```

---

## Subsystem 2: TCP/IP Networking Stack

### 2.1 Overview

```
+------------------------------------------------------------------+
|  NETWORKING: LAYERED PROTOCOLS AS PDD                            |
+------------------------------------------------------------------+

    The network stack maps naturally to PDD:
    
    PRESENTATION: Socket interface, protocol handlers
    DOMAIN:       TCP state machine, IP routing decisions
    DATA:         Network device drivers, hardware queues
```

### 2.2 Presentation Layer

```
+------------------------------------------------------------------+
|  NETWORK PRESENTATION LAYER                                      |
+------------------------------------------------------------------+

    LOCATION: net/socket.c, include/linux/net.h
    
    ┌─────────────────────────────────────────────────────────────┐
    │  SOCKET SYSCALLS:                                           │
    │                                                              │
    │  sys_socket()   - Create socket                             │
    │  sys_bind()     - Bind to address                           │
    │  sys_connect()  - Connect to peer                           │
    │  sys_sendto()   - Send data                                 │
    │  sys_recvfrom() - Receive data                              │
    └─────────────────────────────────────────────────────────────┘
```

```c
/* net/socket.c - PRESENTATION layer */
SYSCALL_DEFINE3(socket, int, family, int, type, int, protocol)
{
    int retval;
    struct socket *sock;
    int flags;

    /* PRESENTATION: Parse user flags */
    flags = type & ~SOCK_TYPE_MASK;
    type &= SOCK_TYPE_MASK;

    /* DISPATCH TO DOMAIN: Create protocol-specific socket */
    retval = sock_create(family, type, protocol, &sock);
    if (retval < 0)
        goto out;

    /* PRESENTATION: Map to file descriptor for user */
    retval = sock_map_fd(sock, flags & (O_CLOEXEC | O_NONBLOCK));
    
out:
    return retval;
}
```

**Socket Ops Table:**

```c
/* include/linux/net.h - Presentation interface */
struct proto_ops {
    int family;
    struct module *owner;
    
    /* PRESENTATION: Protocol-specific syscall handlers */
    int (*bind)(struct socket *, struct sockaddr *, int);
    int (*connect)(struct socket *, struct sockaddr *, int, int);
    int (*accept)(struct socket *, struct socket *, int);
    int (*sendmsg)(struct kiocb *, struct socket *, struct msghdr *, size_t);
    int (*recvmsg)(struct kiocb *, struct socket *, struct msghdr *, size_t, int);
};
```

### 2.3 Domain Layer

```
+------------------------------------------------------------------+
|  NETWORK DOMAIN LAYER                                            |
+------------------------------------------------------------------+

    LOCATION: net/ipv4/tcp*.c, net/ipv4/ip*.c, net/core/
    
    ┌─────────────────────────────────────────────────────────────┐
    │  DOMAIN POLICIES:                                           │
    │                                                              │
    │  • TCP state machine (ESTABLISHED, FIN_WAIT, etc.)         │
    │  • Congestion control algorithms (Reno, CUBIC)              │
    │  • IP routing decisions                                      │
    │  • Packet filtering (netfilter)                             │
    │  • Connection tracking                                       │
    └─────────────────────────────────────────────────────────────┘
```

```c
/* net/ipv4/tcp.c - DOMAIN layer */

/* TCP state machine - pure domain logic */
static const unsigned char new_state[16] = {
    /* Current state + event → new state */
    [TCP_ESTABLISHED]   = TCP_FIN_WAIT1 | TCP_ACTION_FIN,
    [TCP_SYN_SENT]     = TCP_CLOSE,
    [TCP_SYN_RECV]     = TCP_FIN_WAIT1 | TCP_ACTION_FIN,
    /* ... state transition table ... */
};

/* Domain policy: when to send ACK */
static void tcp_send_delayed_ack(struct sock *sk)
{
    struct inet_connection_sock *icsk = inet_csk(sk);
    int ato = icsk->icsk_ack.ato;
    
    /* DOMAIN POLICY: Calculate optimal ACK delay */
    if (ato > TCP_DELACK_MIN) {
        /* ... policy logic ... */
    }
}
```

**Protocol Ops (Domain Interface):**

```c
/* include/net/tcp.h - Domain operations */
struct tcp_congestion_ops {
    /* DOMAIN: Congestion control algorithm */
    void (*cong_avoid)(struct sock *sk, u32 ack, u32 in_flight);
    
    /* DOMAIN: Slow start threshold calculation */
    u32  (*ssthresh)(struct sock *sk);
    
    /* DOMAIN: RTT measurement policy */
    void (*pkts_acked)(struct sock *sk, u32 num_acked, s32 rtt_us);
};

/* Multiple domain policies can be selected */
struct tcp_congestion_ops tcp_reno = { ... };
struct tcp_congestion_ops tcp_cubic = { ... };
```

### 2.4 Data Layer

```
+------------------------------------------------------------------+
|  NETWORK DATA LAYER                                              |
+------------------------------------------------------------------+

    LOCATION: drivers/net/, net/core/dev.c
    
    ┌─────────────────────────────────────────────────────────────┐
    │  DATA MECHANISMS:                                           │
    │                                                              │
    │  • Network device drivers (e1000, ixgbe, virtio-net)       │
    │  • DMA ring buffers                                         │
    │  • Hardware offload (TSO, checksum)                         │
    │  • Queue management                                          │
    └─────────────────────────────────────────────────────────────┘
```

```c
/* include/linux/netdevice.h - DATA layer interface */
struct net_device_ops {
    /* DATA: Open/close hardware */
    int (*ndo_open)(struct net_device *dev);
    int (*ndo_stop)(struct net_device *dev);
    
    /* DATA: Transmit packet to hardware */
    netdev_tx_t (*ndo_start_xmit)(struct sk_buff *skb,
                                   struct net_device *dev);
    
    /* DATA: Configure hardware address */
    int (*ndo_set_mac_address)(struct net_device *dev, void *addr);
    
    /* DATA: Hardware statistics */
    struct net_device_stats* (*ndo_get_stats)(struct net_device *dev);
};
```

### 2.5 Dependency Flow

```
+------------------------------------------------------------------+
|  NETWORK STACK DEPENDENCY DIAGRAM                                |
+------------------------------------------------------------------+

    Application: send(sockfd, buf, len, 0)
    ────────────────────────────────────────────────────────────
        │
        ▼
    ┌─────────────────────────────────────────────────────────────┐
    │  PRESENTATION: net/socket.c                                 │
    │  ┌─────────────────────────────────────────────────────┐    │
    │  │ sys_send() → sock_sendmsg() → proto_ops->sendmsg()  │    │
    │  └─────────────────────────────────────────────────────┘    │
    └─────────────────────────────────────────────────────────────┘
        │
        ▼
    ┌─────────────────────────────────────────────────────────────┐
    │  DOMAIN: net/ipv4/tcp.c, net/ipv4/ip_output.c              │
    │  ┌─────────────────────────────────────────────────────┐    │
    │  │ tcp_sendmsg() → TCP state machine → ip_queue_xmit() │    │
    │  │                  ↓                                   │    │
    │  │            routing decision → netfilter             │    │
    │  └─────────────────────────────────────────────────────┘    │
    └─────────────────────────────────────────────────────────────┘
        │
        ▼
    ┌─────────────────────────────────────────────────────────────┐
    │  DATA: net/core/dev.c → driver                             │
    │  ┌─────────────────────────────────────────────────────┐    │
    │  │ dev_queue_xmit() → qdisc → ndo_start_xmit()         │    │
    │  └─────────────────────────────────────────────────────┘    │
    └─────────────────────────────────────────────────────────────┘
        │
        ▼
    Hardware (NIC)
```

---

## Subsystem 3: Block Layer

### 3.1 Overview

```
+------------------------------------------------------------------+
|  BLOCK LAYER: I/O REQUEST MANAGEMENT                             |
+------------------------------------------------------------------+

    PURPOSE:
    Abstract block device access, optimize I/O patterns
    
    PDD MAPPING:
    - Presentation: Block device file interface
    - Domain: I/O scheduling, request merging, plugging
    - Data: Storage drivers (SATA, NVMe, SCSI)
```

### 3.2 Layer Components

```c
/* PRESENTATION: include/linux/fs.h */
/* Block device file operations */
const struct file_operations def_blk_fops = {
    .open       = blkdev_open,
    .release    = blkdev_close,
    .read       = do_sync_read,
    .write      = do_sync_write,
    .fsync      = blkdev_fsync,
    /* Translates file I/O to block I/O */
};

/* DOMAIN: include/linux/blkdev.h */
struct request_queue {
    /* Domain policy: request ordering */
    struct elevator_queue *elevator;
    
    /* Domain policy: request merging */
    merge_bvec_fn *merge_bvec_fn;
    
    /* Domain policy: queue limits */
    struct queue_limits limits;
    
    /* Domain state: plugging for batching */
    struct blk_plug *plug;
};

/* I/O Scheduler - Pure domain policy */
struct elevator_ops {
    /* DOMAIN: Decide next request to dispatch */
    elevator_dispatch_fn *elevator_dispatch_fn;
    
    /* DOMAIN: Decide if requests can merge */
    elevator_allow_merge_fn *elevator_allow_merge_fn;
    
    /* DOMAIN: Add request to queue */
    elevator_add_req_fn *elevator_add_req_fn;
};

/* DATA: Block device operations */
struct block_device_operations {
    /* DATA: Open/close device */
    int (*open) (struct block_device *, fmode_t);
    int (*release) (struct gendisk *, fmode_t);
    
    /* DATA: Device-specific commands */
    int (*ioctl) (struct block_device *, fmode_t, 
                  unsigned, unsigned long);
};
```

### 3.3 Request Flow

```
+------------------------------------------------------------------+
|  BLOCK LAYER REQUEST FLOW                                        |
+------------------------------------------------------------------+

    write(fd, buf, 4096)  // File on block device
    ────────────────────────────────────────────────────────────
        │
        ▼
    ┌─────────────────────────────────────────────────────────────┐
    │  PRESENTATION: VFS write path                               │
    │  ┌─────────────────────────────────────────────────────┐    │
    │  │ generic_file_aio_write() → __block_write_begin()    │    │
    │  └─────────────────────────────────────────────────────┘    │
    └─────────────────────────────────────────────────────────────┘
        │
        ▼
    ┌─────────────────────────────────────────────────────────────┐
    │  DOMAIN: Block layer                                        │
    │  ┌─────────────────────────────────────────────────────┐    │
    │  │ submit_bio() → generic_make_request()               │    │
    │  │      ↓                                               │    │
    │  │ I/O scheduler: merge? reorder? batch?               │    │
    │  │      ↓                                               │    │
    │  │ __elv_add_request() → elevator policy              │    │
    │  └─────────────────────────────────────────────────────┘    │
    └─────────────────────────────────────────────────────────────┘
        │
        ▼
    ┌─────────────────────────────────────────────────────────────┐
    │  DATA: Storage driver                                       │
    │  ┌─────────────────────────────────────────────────────┐    │
    │  │ scsi_dispatch_cmd() → hardware queue → DMA         │    │
    │  └─────────────────────────────────────────────────────┘    │
    └─────────────────────────────────────────────────────────────┘
```

---

## Subsystem 4: Character Device Framework

### 4.1 Overview

```
+------------------------------------------------------------------+
|  CHAR DEVICES: SIMPLEST PDD EXAMPLE                              |
+------------------------------------------------------------------+

    Character devices show PDD at its cleanest:
    
    PRESENTATION: /dev/xxx file interface
    DOMAIN:       Device-specific logic
    DATA:         Hardware access
```

### 4.2 Layer Mapping

```c
/* PRESENTATION: drivers/char/misc.c */
static const struct file_operations misc_fops = {
    .owner = THIS_MODULE,
    .open  = misc_open,  /* Dispatch to device-specific fops */
};

/* misc_open dispatches to device's own file_operations */
static int misc_open(struct inode *inode, struct file *file)
{
    int minor = iminor(inode);
    struct miscdevice *c;
    
    /* Find registered device */
    list_for_each_entry(c, &misc_list, list) {
        if (c->minor == minor) {
            /* DISPATCH: Replace with device-specific ops */
            file->f_op = c->fops;
            /* Call device's open if present */
            if (file->f_op->open)
                return file->f_op->open(inode, file);
            return 0;
        }
    }
    return -ENODEV;
}

/* DOMAIN: Device-specific logic example (RNG) */
/* drivers/char/random.c */
static const struct file_operations random_fops = {
    .read  = random_read,   /* Domain: entropy pool management */
    .write = random_write,  /* Domain: add entropy */
    .poll  = random_poll,   /* Domain: entropy availability */
};

/* DATA: Hardware access */
/* Architecture-specific RNG hardware access */
static void get_hardware_random(void *buf, size_t len)
{
    /* Direct hardware register reads */
    arch_get_random_seed_long((unsigned long *)buf);
}
```

### 4.3 Registration Pattern

```
+------------------------------------------------------------------+
|  CHAR DEVICE REGISTRATION AS PDD                                 |
+------------------------------------------------------------------+

    Driver Registration:
    ┌─────────────────────────────────────────────────────────────┐
    │  1. Define file_operations (PRESENTATION handlers)         │
    │  2. Implement domain logic in fops callbacks               │
    │  3. Register with char subsystem                            │
    │  4. Kernel creates /dev entry (PRESENTATION)               │
    └─────────────────────────────────────────────────────────────┘

    User Access:
    ┌─────────────────────────────────────────────────────────────┐
    │  1. open("/dev/mydev") → PRESENTATION                      │
    │  2. Dispatch to driver's fops → DOMAIN                     │
    │  3. Driver accesses hardware → DATA                        │
    └─────────────────────────────────────────────────────────────┘
```

---

## Subsystem 5: Process Scheduler

### 5.1 Overview

```
+------------------------------------------------------------------+
|  SCHEDULER: POLICY vs MECHANISM SEPARATION                       |
+------------------------------------------------------------------+

    The scheduler is a textbook policy/mechanism example:
    
    PRESENTATION: System calls (nice, setpriority, sched_*)
    DOMAIN:       Scheduling policies (CFS, RT, DEADLINE)
    DATA:         CPU/timer hardware, runqueue data structures
```

### 5.2 Layer Components

```c
/* PRESENTATION: kernel/sched/core.c */
SYSCALL_DEFINE1(nice, int, increment)
{
    /* PRESENTATION: Validate and adapt user input */
    int nice = task_nice(current) + increment;
    
    if (nice < MIN_NICE) nice = MIN_NICE;
    if (nice > MAX_NICE) nice = MAX_NICE;
    
    /* DISPATCH TO DOMAIN */
    set_user_nice(current, nice);
    return 0;
}

/* DOMAIN: Scheduling class interface */
struct sched_class {
    const struct sched_class *next;
    
    /* DOMAIN POLICY: Add task to runqueue */
    void (*enqueue_task)(struct rq *rq, struct task_struct *p, int flags);
    
    /* DOMAIN POLICY: Remove task from runqueue */  
    void (*dequeue_task)(struct rq *rq, struct task_struct *p, int flags);
    
    /* DOMAIN POLICY: Pick next task to run */
    struct task_struct *(*pick_next_task)(struct rq *rq);
    
    /* DOMAIN POLICY: Preemption decision */
    void (*check_preempt_curr)(struct rq *rq, struct task_struct *p, int flags);
};

/* CFS scheduler class - domain policy implementation */
const struct sched_class fair_sched_class = {
    .next           = &idle_sched_class,
    .enqueue_task   = enqueue_task_fair,
    .dequeue_task   = dequeue_task_fair,
    .pick_next_task = pick_next_task_fair,
    /* CFS-specific policy decisions */
};

/* RT scheduler class - different domain policy */
const struct sched_class rt_sched_class = {
    .next           = &fair_sched_class,
    .enqueue_task   = enqueue_task_rt,
    .dequeue_task   = dequeue_task_rt,
    .pick_next_task = pick_next_task_rt,
    /* Real-time policy decisions */
};
```

### 5.3 Scheduler Architecture

```
+------------------------------------------------------------------+
|  SCHEDULER PDD ARCHITECTURE                                      |
+------------------------------------------------------------------+

    User: nice(-5) or sched_setscheduler(SCHED_FIFO)
    ────────────────────────────────────────────────────────────
        │
        ▼
    ┌─────────────────────────────────────────────────────────────┐
    │  PRESENTATION: kernel/sched/core.c                          │
    │  ┌─────────────────────────────────────────────────────┐    │
    │  │ sys_nice() / sys_sched_setscheduler()               │    │
    │  │ Validate parameters, check permissions               │    │
    │  └─────────────────────────────────────────────────────┘    │
    └─────────────────────────────────────────────────────────────┘
        │
        ▼
    ┌─────────────────────────────────────────────────────────────┐
    │  DOMAIN: sched_class implementations                        │
    │  ┌───────────────────┬───────────────────────────────┐      │
    │  │ CFS (fair_sched)  │ RT (rt_sched_class)           │      │
    │  │ - vruntime        │ - fixed priority              │      │
    │  │ - red-black tree  │ - FIFO/RR                     │      │
    │  │ - load balancing  │ - strict preemption           │      │
    │  └───────────────────┴───────────────────────────────┘      │
    └─────────────────────────────────────────────────────────────┘
        │
        ▼
    ┌─────────────────────────────────────────────────────────────┐
    │  DATA: Hardware/arch                                        │
    │  ┌─────────────────────────────────────────────────────┐    │
    │  │ Per-CPU runqueues, timer hardware, context switch   │    │
    │  │ arch/x86/kernel/process.c: __switch_to()           │    │
    │  └─────────────────────────────────────────────────────┘    │
    └─────────────────────────────────────────────────────────────┘
```

---

## Subsystem 6: Memory Management

### 6.1 Overview

```
+------------------------------------------------------------------+
|  MEMORY MANAGEMENT: MULTI-LAYER PDD                              |
+------------------------------------------------------------------+

    PRESENTATION: mmap, brk, mlock syscalls
    DOMAIN:       Page allocation policies, NUMA, OOM killer
    DATA:         Page tables, physical memory, swap devices
```

### 6.2 Key Structures

```c
/* PRESENTATION: Memory syscall interface */
/* mm/mmap.c */
SYSCALL_DEFINE6(mmap_pgoff, unsigned long, addr, unsigned long, len,
                unsigned long, prot, unsigned long, flags,
                unsigned long, fd, unsigned long, pgoff)
{
    /* PRESENTATION: Validate parameters */
    if (offset_in_page(addr))
        return -EINVAL;
    
    /* DISPATCH TO DOMAIN */
    return do_mmap_pgoff(file, addr, len, prot, flags, pgoff);
}

/* DOMAIN: VM area operations */
struct vm_operations_struct {
    /* DOMAIN: Page fault handling policy */
    int (*fault)(struct vm_area_struct *vma, struct vm_fault *vmf);
    
    /* DOMAIN: Page access tracking */
    int (*page_mkwrite)(struct vm_area_struct *vma, struct vm_fault *vmf);
};

/* DOMAIN: Memory allocation policy */
struct mempolicy {
    atomic_t refcnt;
    unsigned short mode;    /* MPOL_PREFERRED, MPOL_INTERLEAVE, etc. */
    nodemask_t nodes;       /* NUMA nodes */
};

/* DATA: Physical memory zones */
struct zone {
    unsigned long       watermark[NR_WMARK];
    unsigned long       managed_pages;
    struct free_area    free_area[MAX_ORDER];
    /* Physical memory management */
};
```

---

## Summary: PDD Patterns in Kernel

```
+------------------------------------------------------------------+
|  COMMON KERNEL PDD PATTERNS                                      |
+------------------------------------------------------------------+

    PATTERN 1: OPS TABLE AS INTERFACE
    ┌─────────────────────────────────────────────────────────────┐
    │  struct xxx_operations {                                    │
    │      int (*method1)(...);                                   │
    │      int (*method2)(...);                                   │
    │  };                                                         │
    │                                                              │
    │  - Domain defines the struct                                │
    │  - Data layer implements it                                 │
    │  - Presentation calls through it                            │
    └─────────────────────────────────────────────────────────────┘

    PATTERN 2: REGISTRATION / DISPATCH
    ┌─────────────────────────────────────────────────────────────┐
    │  register_xxx(&my_ops);    // Data registers with Domain   │
    │  dispatch_xxx(key);        // Presentation uses Domain     │
    └─────────────────────────────────────────────────────────────┘

    PATTERN 3: CONTEXT OBJECTS
    ┌─────────────────────────────────────────────────────────────┐
    │  struct file { ... void *private_data; };                   │
    │                                                              │
    │  - Presentation creates context                             │
    │  - private_data links to Domain/Data state                 │
    └─────────────────────────────────────────────────────────────┘

    PATTERN 4: LAYERED STRUCTS
    ┌─────────────────────────────────────────────────────────────┐
    │  struct socket → struct sock → device-specific             │
    │  Presentation   Domain        Data                          │
    └─────────────────────────────────────────────────────────────┘
```

**中文总结：**
- **VFS**：最典型的 PDD 示例——系统调用→VFS 策略→文件系统实现
- **网络栈**：socket 接口→TCP 状态机→网卡驱动
- **块层**：块设备文件→I/O 调度→存储驱动
- **字符设备**：/dev 文件→设备逻辑→硬件访问
- **调度器**：系统调用→调度策略类→CPU/定时器硬件
- **内存管理**：mmap 等调用→分配策略→页表/物理内存

