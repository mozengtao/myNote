# Linux Kernel Layered Architecture: Boundaries and Contracts

A systems-level explanation of how layers interact in the Linux kernel, with code examples from v3.2.

---

## Table of Contents

1. [What Are Layer Boundaries?](#1-what-are-layer-boundaries)
2. [The Five Interaction Types](#2-the-five-interaction-types)
3. [What Is Forbidden](#3-what-is-forbidden)
4. [Kernel Code Examples](#4-kernel-code-examples)
5. [Complete Userspace Examples](#5-complete-userspace-examples)
6. [Summary](#6-summary)

---

## 1. What Are Layer Boundaries?

### 1.1 Definition

```
╔═══════════════════════════════════════════════════════════════════════════════╗
║                    LAYER BOUNDARIES IN C                                      ║
╠═══════════════════════════════════════════════════════════════════════════════╣
║                                                                               ║
║   In C, a "layer boundary" is:                                                ║
║                                                                               ║
║   1. A CONTRACTUAL INTERFACE                                                  ║
║      - Defined by header files (struct, function prototypes)                  ║
║      - Operations structures (function pointer tables)                        ║
║      - Documented invariants and semantics                                    ║
║                                                                               ║
║   2. AN ABSTRACTION BARRIER                                                   ║
║      - Upper layer doesn't know lower layer's implementation                  ║
║      - Lower layer doesn't know who's calling it                              ║
║      - Changes in one layer don't break the other                             ║
║                                                                               ║
║   3. A SOCIAL CONTRACT                                                        ║
║      - "Thou shalt not access my internals"                                   ║
║      - "I promise to maintain this interface"                                 ║
║      - Enforced by code review, not compiler                                  ║
║                                                                               ║
╚═══════════════════════════════════════════════════════════════════════════════╝
```

**中文说明：**
在 C 语言中，层边界不是由语言强制的（不像 Java 的 private），而是通过以下方式建立：
- **契约接口**：头文件定义结构体和函数原型，操作结构体定义回调函数表
- **抽象屏障**：上层不知道下层如何实现，下层不知道谁在调用它
- **社会契约**：开发者约定不直接访问内部实现，通过代码审查而非编译器来强制执行

---

### 1.2 Linux Kernel Layer Stack

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              USER SPACE                                     │
│                                                                             │
│    Application ──► libc ──► syscall instruction ──► trap to kernel          │
│                                                                             │
└──────────────────────────────────┬──────────────────────────────────────────┘
                                   │ System Call Interface
                                   ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                           SYSTEM CALL LAYER                                 │
│                                                                             │
│    sys_read(), sys_write(), sys_open(), sys_socket(), sys_ioctl()...        │
│                                                                             │
└──────────────────────────────────┬──────────────────────────────────────────┘
                                   │ VFS Interface / Socket Layer
                                   ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                     GENERIC SUBSYSTEM LAYER                                 │
│                                                                             │
│  ┌─────────────┐  ┌──────────────┐  ┌──────────────┐  ┌─────────────────┐   │
│  │     VFS     │  │ Block Layer  │  │ Net Core     │  │  Device Model   │   │
│  │             │  │              │  │              │  │                 │   │
│  │ file_ops    │  │ request_queue│  │ proto_ops    │  │ bus/driver/dev  │   │
│  └──────┬──────┘  └───────┬──────┘  └───────┬──────┘  └────────┬────────┘   │
│         │                 │                 │                  │            │
└─────────┼─────────────────┼─────────────────┼──────────────────┼────────────┘
          │                 │                 │                  │
          ▼                 ▼                 ▼                  ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                      SPECIFIC IMPLEMENTATION LAYER                          │
│                                                                             │
│  ┌─────────────┐  ┌──────────────┐  ┌──────────────┐  ┌─────────────────┐   │
│  │  ext4_ops   │  │  scsi_ops    │  │ tcp_proto    │  │  e1000_driver   │   │
│  │  nfs_ops    │  │  nvme_ops    │  │ udp_proto    │  │  usb_driver     │   │
│  │  proc_ops   │  │  mmc_ops     │  │ raw_proto    │  │  pci_driver     │   │
│  └─────────────┘  └──────────────┘  └──────────────┘  └─────────────────┘   │
│                                                                             │
└──────────────────────────────────┬──────────────────────────────────────────┘
                                   │ Hardware Abstraction
                                   ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                           HARDWARE LAYER                                    │
│                                                                             │
│    Physical devices: disk, NIC, USB controllers, etc.                       │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

**中文说明：**
Linux 内核的分层架构从上到下分为：
1. **用户空间**：应用程序通过 libc 发起系统调用
2. **系统调用层**：统一的入口点（sys_read, sys_write 等）
3. **通用子系统层**：VFS、块层、网络核心、设备模型 —— 定义操作接口
4. **具体实现层**：ext4, SCSI, TCP, e1000 等 —— 实现具体操作
5. **硬件层**：物理设备

---

## 2. The Five Interaction Types

### 2.1 Overview

```
╔═══════════════════════════════════════════════════════════════════════════════╗
║                    FIVE LEGAL INTERACTION TYPES                               ║
╠═══════════════════════════════════════════════════════════════════════════════╣
║                                                                               ║
║   Type 1: DOWNWARD CALL (Upper calls Lower via function pointer)              ║
║           VFS calls ext4 via file_operations                                  ║
║                                                                               ║
║   Type 2: UPWARD CALLBACK (Lower notifies Upper via registered callback)      ║
║           Block layer calls filesystem's end_io callback                      ║
║                                                                               ║
║   Type 3: REGISTRATION (Lower registers with Upper framework)                 ║
║           Driver calls register_chrdev(), register_netdev()                   ║
║                                                                               ║
║   Type 4: SERVICE CALL (Layer calls shared service/utility)                   ║
║           Any layer calls kmalloc(), printk(), spin_lock()                    ║
║                                                                               ║
║   Type 5: EVENT/NOTIFICATION (Asynchronous notification via hooks)            ║
║           Notifier chains, netfilter hooks                                    ║
║                                                                               ║
╚═══════════════════════════════════════════════════════════════════════════════╝
```

**中文说明：**
内核中层之间有五种合法的交互方式：
1. **向下调用**：上层通过函数指针调用下层（如 VFS 调用 ext4）
2. **向上回调**：下层通过注册的回调通知上层（如 I/O 完成回调）
3. **注册机制**：下层向上层框架注册自己（如驱动注册）
4. **服务调用**：任何层调用共享服务（如 kmalloc）
5. **事件通知**：异步通知机制（如 notifier chain）

---

### 2.2 Type 1: Downward Call via Operations Structure

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                     TYPE 1: DOWNWARD CALL                                   │
│                                                                             │
│   ┌─────────────────┐                                                       │
│   │   VFS Layer     │                                                       │
│   │                 │                                                       │
│   │  vfs_read() {   │                                                       │
│   │    ...          │                                                       │
│   │    file->f_op->read(...)  ────────────┐                                 │
│   │    ...          │                     │                                 │
│   │  }              │                     │                                 │
│   └─────────────────┘                     │                                 │
│                                           │ Function pointer                │
│   ════════════════════════════════════════│══════════════════════════════   │
│                                           │ BOUNDARY: file_operations       │
│   ════════════════════════════════════════│══════════════════════════════   │
│                                           │                                 │
│                                           ▼                                 │
│   ┌─────────────────┐          ┌─────────────────┐                          │
│   │   ext4_ops      │          │    nfs_ops      │                          │
│   │                 │          │                 │                          │
│   │ .read = ext4_   │          │ .read = nfs_    │                          │
│   │        file_read│          │        file_read│                          │
│   └─────────────────┘          └─────────────────┘                          │
│                                                                             │
│   Key: Upper layer is UNAWARE of which implementation is called             │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

**中文说明：**
向下调用是最常见的交互类型。上层（VFS）定义操作结构体（file_operations），下层（ext4, NFS）实现具体函数并填充该结构体。上层通过函数指针调用下层，完全不知道具体调用的是哪个实现。这就是"多态"在 C 语言中的实现方式。

**Kernel Code: VFS calling filesystem** (fs/read_write.c):

```c
/*
 * vfs_read - VFS layer read function
 * 
 * This function is LAYER-AGNOSTIC. It doesn't know or care
 * whether the file is on ext4, NFS, or a character device.
 */
ssize_t vfs_read(struct file *file, char __user *buf, 
                 size_t count, loff_t *pos)
{
    ssize_t ret;

    /* Validation (layer's responsibility) */
    if (!(file->f_mode & FMODE_READ))
        return -EBADF;
    if (!file->f_op || (!file->f_op->read && !file->f_op->aio_read))
        return -EINVAL;

    ret = rw_verify_area(READ, file, pos, count);
    if (ret >= 0) {
        count = ret;
        
        /* THE BOUNDARY CROSSING: call through function pointer */
        if (file->f_op->read)
            ret = file->f_op->read(file, buf, count, pos);
        else
            ret = do_sync_read(file, buf, count, pos);
            
        /* Post-processing (layer's responsibility) */
        if (ret > 0) {
            fsnotify_access(file);
            add_rchar(current, ret);
        }
        inc_syscr(current);
    }

    return ret;
}
```

---

### 2.3 Type 2: Upward Callback

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                     TYPE 2: UPWARD CALLBACK                                 │
│                                                                             │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │   Filesystem Layer (ext4)                                           │   │
│   │                                                                     │   │
│   │   ext4_readpage() {                                                 │   │
│   │       bio = bio_alloc(...);                                         │   │
│   │       bio->bi_end_io = ext4_end_bio;  ◄─── Register callback        │   │
│   │       submit_bio(bio);                                              │   │
│   │   }                                                                 │   │
│   │                                                                     │   │
│   │   void ext4_end_bio(struct bio *bio) { ◄─── Called when I/O done    │   │
│   │       /* Handle completion */                                       │   │
│   │   }                                                                 │   │
│   └──────────────────────────────────────┬──────────────────────────────┘   │
│                                          │                                  │
│   ═══════════════════════════════════════│═══════════════════════════════   │
│                                          │ BOUNDARY: bi_end_io callback     │
│   ═══════════════════════════════════════│═══════════════════════════════   │
│                                          │                                  │
│                                          ▼                                  │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │   Block Layer                                                       │   │
│   │                                                                     │   │
│   │   bio_endio(bio, error) {                                           │   │
│   │       ...                                                           │   │
│   │       if (bio->bi_end_io)                                           │   │
│   │           bio->bi_end_io(bio, error);  ────► Calls upper layer      │   │
│   │   }                                                                 │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                                                             │
│   Key: Lower layer NOTIFIES upper layer when async operation completes      │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

**中文说明：**
向上回调用于异步操作完成时通知上层。上层（文件系统）注册一个回调函数，下层（块层）在 I/O 完成时调用该回调。这样下层不需要知道上层是谁，只需要调用注册的函数指针。这种模式在内核中非常常见，用于处理中断完成、定时器到期、网络包到达等事件。

**Kernel Code: Block I/O completion callback** (include/linux/bio.h):

```c
/*
 * struct bio - Block I/O descriptor
 * 
 * The bi_end_io callback allows the upper layer (filesystem)
 * to be notified when I/O completes, without the block layer
 * knowing anything about filesystems.
 */
struct bio {
    sector_t            bi_sector;      /* device address */
    struct bio         *bi_next;        /* request queue link */
    struct block_device *bi_bdev;
    unsigned long       bi_flags;       /* status, command, etc */
    unsigned long       bi_rw;          /* READ/WRITE */
    
    /* ... */
    
    bio_end_io_t       *bi_end_io;      /* ← THE CALLBACK */
    void               *bi_private;     /* ← Context for callback */
    
    /* ... */
};

/* Block layer calls this when I/O completes */
void bio_endio(struct bio *bio, int error)
{
    if (error)
        clear_bit(BIO_UPTODATE, &bio->bi_flags);
    else if (!test_bit(BIO_UPTODATE, &bio->bi_flags))
        error = -EIO;

    /* THE UPWARD CALLBACK */
    if (bio->bi_end_io)
        bio->bi_end_io(bio, error);
}
```

---

### 2.4 Type 3: Registration

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                     TYPE 3: REGISTRATION                                    │
│                                                                             │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │   Framework Layer (VFS/Block/Net Core)                              │   │
│   │                                                                     │   │
│   │   /* Global registry */                                             │   │
│   │   static struct list_head registered_drivers;                       │   │
│   │                                                                     │   │
│   │   int register_blkdev(unsigned int major, const char *name) {       │   │
│   │       /* Add to registry */                                         │   │
│   │       /* Now framework knows about this driver */                   │   │
│   │   }                                                                 │   │
│   │                                                                     │   │
│   │   int unregister_blkdev(unsigned int major, const char *name);      │   │
│   │                                                                     │   │
│   └──────────────────────────────────────────────▲──────────────────────┘   │
│                                                  │                          │
│   ═══════════════════════════════════════════════│═══════════════════════   │
│                                                  │ BOUNDARY: Registration API│
│   ═══════════════════════════════════════════════│═══════════════════════   │
│                                                  │                          │
│   ┌──────────────────────────────────────────────┴──────────────────────┐   │
│   │   Driver Layer                                                      │   │
│   │                                                                     │   │
│   │   static int __init my_driver_init(void) {                          │   │
│   │       /* Register with framework */                                 │   │
│   │       register_blkdev(MY_MAJOR, "myblock");                         │   │
│   │       register_netdev(my_netdev);                                   │   │
│   │       cdev_add(&my_cdev, dev, 1);                                   │   │
│   │   }                                                                 │   │
│   │                                                                     │   │
│   │   static void __exit my_driver_exit(void) {                         │   │
│   │       unregister_blkdev(MY_MAJOR, "myblock");                       │   │
│   │   }                                                                 │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                                                             │
│   Key: Lower layer ANNOUNCES itself to upper framework                      │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

**中文说明：**
注册机制让下层（驱动程序）向上层框架（VFS、块层、网络核心）宣告自己的存在。注册时，驱动提供操作函数表，框架将其保存起来。之后当用户空间访问设备时，框架根据设备号找到对应的驱动，并调用其操作函数。注册/注销必须配对，否则会导致悬挂引用。

**Kernel Code: Character device registration** (fs/char_dev.c):

```c
/*
 * cdev_add - Add a character device to the system
 * @p: the cdev structure for the device
 * @dev: the first device number
 * @count: the number of consecutive minor numbers
 *
 * This makes the device "visible" to the VFS layer.
 * After this call, open() on the device node will find this driver.
 */
int cdev_add(struct cdev *p, dev_t dev, unsigned count)
{
    p->dev = dev;
    p->count = count;
    
    /* Add to global map - now VFS can find us */
    return kobj_map(cdev_map, dev, count, NULL,
                    exact_match, exact_lock, p);
}

/* The operations structure provided at registration */
struct cdev {
    struct kobject kobj;
    struct module *owner;
    const struct file_operations *ops;  /* ← Driver provides this */
    struct list_head list;
    dev_t dev;
    unsigned int count;
};
```

---

### 2.5 Type 4: Service Call

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                     TYPE 4: SERVICE CALL                                    │
│                                                                             │
│                              ┌───────────────────────┐                      │
│                              │   Shared Services     │                      │
│                              │                       │                      │
│                              │ • Memory: kmalloc()   │                      │
│                              │ • Sync: spin_lock()   │                      │
│                              │ • Print: printk()     │                      │
│                              │ • Time: jiffies       │                      │
│                              │ • Work: schedule_work │                      │
│                              └───────────┬───────────┘                      │
│                                          │                                  │
│              ┌───────────────┬───────────┴───────────┬───────────────┐      │
│              │               │                       │               │      │
│              ▼               ▼                       ▼               ▼      │
│   ┌─────────────────┐ ┌─────────────┐ ┌─────────────────┐ ┌─────────────┐   │
│   │   Filesystem    │ │ Block Layer │ │   Network       │ │   Drivers   │   │
│   │                 │ │             │ │                 │ │             │   │
│   │ kmalloc(...)    │ │ spin_lock() │ │ alloc_skb()     │ │ printk()    │   │
│   │ kfree(...)      │ │ schedule()  │ │ kfree_skb()     │ │ msleep()    │   │
│   └─────────────────┘ └─────────────┘ └─────────────────┘ └─────────────┘   │
│                                                                             │
│   Key: Services are ORTHOGONAL to layers - any layer can use them           │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

**中文说明：**
服务调用是横向的，不遵循层级关系。内核提供一组共享服务（内存管理、同步原语、调试输出等），任何层都可以使用。这些服务构成了内核的"基础设施"，它们是所有子系统的公共依赖。关键点是：服务调用不属于任何特定的层级，它们是正交的。

**Kernel Code: Common services used across layers**:

```c
/* Memory allocation - used by all layers */
void *kmalloc(size_t size, gfp_t flags);
void kfree(const void *);

/* Synchronization - used by all layers */
void spin_lock(spinlock_t *lock);
void spin_unlock(spinlock_t *lock);
void mutex_lock(struct mutex *lock);
void mutex_unlock(struct mutex *lock);

/* Debugging - used by all layers */
int printk(const char *fmt, ...);

/* Scheduling - used by all layers */
void schedule(void);
void msleep(unsigned int msecs);

/* Work queues - used by all layers */
bool schedule_work(struct work_struct *work);
```

---

### 2.6 Type 5: Event/Notification

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                     TYPE 5: EVENT/NOTIFICATION                              │
│                                                                             │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │   Notifier Chain / Hook System                                      │   │
│   │                                                                     │   │
│   │   struct notifier_block {                                           │   │
│   │       int (*notifier_call)(struct notifier_block *, ...);           │   │
│   │       struct notifier_block *next;                                  │   │
│   │   };                                                                │   │
│   │                                                                     │   │
│   │   ┌──────────┐  ┌──────────┐  ┌──────────┐                          │   │
│   │   │Subscriber│──│Subscriber│──│Subscriber│── NULL                   │   │
│   │   │ A        │  │ B        │  │ C        │                          │   │
│   │   └──────────┘  └──────────┘  └──────────┘                          │   │
│   │        ▲              ▲              ▲                              │   │
│   │        │              │              │                              │   │
│   │        └──────────────┴──────────────┘                              │   │
│   │                       │                                             │   │
│   │   EVENT OCCURS:       │                                             │   │
│   │   ═════════════       ▼                                             │   │
│   │                                                                     │   │
│   │   notifier_call_chain() {                                           │   │
│   │       for each subscriber:                                          │   │
│   │           subscriber->notifier_call(event_data);                    │   │
│   │   }                                                                 │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                                                             │
│   Examples:                                                                 │
│   • reboot_notifier_list - Notify subsystems before reboot                  │
│   • netdev_chain - Network device state changes                             │
│   • inetaddr_chain - IP address changes                                     │
│   • Netfilter hooks - Packet processing pipeline                            │
│                                                                             │
│   Key: DECOUPLED notification - publisher doesn't know subscribers          │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

**中文说明：**
事件通知机制允许解耦的模块之间进行通信。发布者不知道有哪些订阅者，只是在事件发生时调用 notifier chain。这种模式用于：系统重启通知、网络设备状态变化、IP 地址变化、Netfilter 数据包处理等。它实现了"发布-订阅"模式，让独立开发的模块可以响应系统事件。

**Kernel Code: Notifier chain** (include/linux/notifier.h):

```c
struct notifier_block {
    int (*notifier_call)(struct notifier_block *nb,
                         unsigned long action, void *data);
    struct notifier_block __rcu *next;
    int priority;
};

/* Publisher calls this when event occurs */
int notifier_call_chain(struct notifier_block **nl,
                        unsigned long val, void *v)
{
    int ret = NOTIFY_DONE;
    struct notifier_block *nb, *next_nb;

    nb = rcu_dereference_raw(*nl);

    while (nb) {
        next_nb = rcu_dereference_raw(nb->next);
        /* Call each subscriber */
        ret = nb->notifier_call(nb, val, v);
        if (ret & NOTIFY_STOP_MASK)
            break;
        nb = next_nb;
    }
    return ret;
}

/* Example: Network device notification */
int register_netdevice_notifier(struct notifier_block *nb);
int unregister_netdevice_notifier(struct notifier_block *nb);
```

---

## 3. What Is Forbidden

### 3.1 Forbidden Interactions

```
╔═══════════════════════════════════════════════════════════════════════════════╗
║                    FORBIDDEN: LAYER VIOLATIONS                                ║
╠═══════════════════════════════════════════════════════════════════════════════╣
║                                                                               ║
║   ❌ VIOLATION 1: Direct Structure Access                                     ║
║   ─────────────────────────────────────────                                   ║
║   /* WRONG: VFS directly manipulating ext4 internals */                       ║
║   struct ext4_inode_info *ei = EXT4_I(inode);                                 ║
║   ei->i_data[0] = new_block;  /* Bypass ext4's logic! */                      ║
║                                                                               ║
║   ❌ VIOLATION 2: Bypassing the Interface                                     ║
║   ─────────────────────────────────────────                                   ║
║   /* WRONG: Direct call to lower layer without going through ops */           ║
║   ext4_file_read(...);  /* Should use file->f_op->read */                     ║
║                                                                               ║
║   ❌ VIOLATION 3: Upward Knowledge                                            ║
║   ─────────────────────────────────────────                                   ║
║   /* WRONG: Block layer knowing about specific filesystems */                 ║
║   if (is_ext4_bio(bio))                                                       ║
║       ext4_special_handling();                                                ║
║                                                                               ║
║   ❌ VIOLATION 4: Cross-Layer Global State                                    ║
║   ─────────────────────────────────────────                                   ║
║   /* WRONG: Driver setting global VFS state */                                ║
║   extern int vfs_cache_pressure;                                              ║
║   vfs_cache_pressure = 50;  /* Driver shouldn't touch this */                 ║
║                                                                               ║
║   ❌ VIOLATION 5: Layer Skip                                                  ║
║   ─────────────────────────────────────────                                   ║
║   /* WRONG: Syscall directly calling device driver */                         ║
║   sys_read() {                                                                ║
║       my_device_read();  /* Should go through VFS! */                         ║
║   }                                                                           ║
║                                                                               ║
╚═══════════════════════════════════════════════════════════════════════════════╝
```

**中文说明：**

违规类型及其危害：

1. **直接访问结构体内部**：绕过抽象层，当下层实现改变时会导致上层崩溃
2. **绕过接口**：直接调用具体实现函数而非通过函数指针，破坏多态性
3. **上层知识**：下层不应该知道上层是谁，否则无法被不同的上层复用
4. **跨层全局状态**：一层修改另一层的全局变量，导致耦合和难以调试的 bug
5. **跳层调用**：跳过中间层直接调用底层，破坏了层次结构的完整性

---

### 3.2 Why These Are Forbidden

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                     WHY VIOLATIONS ARE DANGEROUS                            │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   1. BREAKS ENCAPSULATION                                                   │
│      └─ Lower layer can't change internals without breaking upper layer     │
│      └─ Example: ext4 changes i_data layout → VFS code breaks               │
│                                                                             │
│   2. PREVENTS SUBSTITUTION                                                  │
│      └─ Can't swap implementations (ext4 ↔ btrfs)                           │
│      └─ Can't mock for testing                                              │
│      └─ Can't add new implementations without modifying framework           │
│                                                                             │
│   3. CREATES HIDDEN DEPENDENCIES                                            │
│      └─ Changes have unpredictable ripple effects                           │
│      └─ Hard to understand what depends on what                             │
│      └─ Refactoring becomes dangerous                                       │
│                                                                             │
│   4. VIOLATES SINGLE RESPONSIBILITY                                         │
│      └─ Each layer has a defined role                                       │
│      └─ Mixing responsibilities = unmaintainable code                       │
│                                                                             │
│   5. BREAKS TESTING                                                         │
│      └─ Can't test layers in isolation                                      │
│      └─ Need full system for any test                                       │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 4. Kernel Code Examples

### 4.1 Complete File I/O Path

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                   FILE READ: USER SPACE TO DISK                             │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   User:      read(fd, buf, 4096);                                           │
│                    │                                                        │
│                    ▼                                                        │
│   Syscall:   SYSCALL_DEFINE3(read, fd, buf, count)                          │
│              {                                                              │
│                  file = fget_light(fd, ...);                                │
│                  ret = vfs_read(file, buf, count, &pos);                    │
│              }                                                              │
│                    │                                                        │
│                    ▼                                                        │
│   VFS:       vfs_read()                                                     │
│              {                                                              │
│                  ret = file->f_op->read(...);  ──┐                          │
│              }                                   │                          │
│                    │                             │ file_operations          │
│                    │                             │                          │
│   ═══════════════════════════════════════════════│══════════════════════    │
│                                                  ▼                          │
│   Filesystem: ext4_file_read()                                              │
│               {                                                             │
│                   generic_file_aio_read();                                  │
│               }                                                             │
│                    │                                                        │
│                    ▼                                                        │
│   Page Cache: If not cached → trigger read from disk                        │
│               mpage_readpage()                                              │
│               {                                                             │
│                   bio = bio_alloc(...);                                     │
│                   bio->bi_end_io = mpage_end_io_read;                       │
│                   submit_bio(READ, bio);  ──┐                               │
│               }                             │                               │
│                    │                        │ block_device_operations       │
│   ════════════════════════════════════════════│═════════════════════════    │
│                                               ▼                             │
│   Block:     submit_bio()                                                   │
│              {                                                              │
│                  generic_make_request(bio);                                 │
│                  q->make_request_fn(q, bio);  ──┐                           │
│              }                                  │                           │
│                    │                            │ request_queue ops         │
│   ═════════════════════════════════════════════════│════════════════════    │
│                                                    ▼                        │
│   Driver:    scsi_request_fn() or nvme_make_rq()                            │
│              {                                                              │
│                  /* Build hardware commands */                              │
│                  /* Program DMA */                                          │
│                  /* Trigger hardware */                                     │
│              }                                                              │
│                    │                                                        │
│                    ▼                                                        │
│   Hardware:  [ DISK CONTROLLER ] ──► [ DISK PLATTERS ]                      │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

**中文说明：**
这是一个完整的文件读取路径，展示了数据如何从用户空间一直流向磁盘：
1. 用户程序调用 read()
2. 系统调用层获取文件对象，调用 vfs_read()
3. VFS 通过 file_operations 调用具体文件系统（ext4）
4. ext4 使用页缓存，如果不命中则提交 bio 到块层
5. 块层调用请求队列的 make_request_fn
6. 设备驱动构建硬件命令，编程 DMA，触发硬件

每一层都只通过定义好的接口与相邻层交互。

---

### 4.2 Network Packet Transmission Path

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                   NETWORK SEND: USER SPACE TO WIRE                          │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   User:      sendto(sock_fd, data, len, 0, &addr, sizeof(addr));            │
│                    │                                                        │
│                    ▼                                                        │
│   Syscall:   SYSCALL_DEFINE6(sendto, ...)                                   │
│              {                                                              │
│                  sock = sockfd_lookup_light(fd, ...);                       │
│                  sock_sendmsg(sock, &msg, len);                             │
│              }                                                              │
│                    │                                                        │
│                    ▼                                                        │
│   Socket:    sock_sendmsg()                                                 │
│              {                                                              │
│                  sock->ops->sendmsg(...);  ──┐                              │
│              }                               │ proto_ops                    │
│                    │                         │                              │
│   ════════════════════════════════════════════│═════════════════════════    │
│                                              ▼                              │
│   Protocol:  udp_sendmsg() or tcp_sendmsg()                                 │
│              {                                                              │
│                  skb = alloc_skb(...);                                      │
│                  /* Build UDP/TCP header */                                 │
│                  ip_send_skb(skb);                                          │
│              }                                                              │
│                    │                                                        │
│                    ▼                                                        │
│   IP:        ip_send_skb()                                                  │
│              {                                                              │
│                  /* Build IP header */                                      │
│                  /* Route lookup */                                         │
│                  ip_local_out(skb);                                         │
│                  dst_output(skb);  ──► ip_output()                          │
│              }                                                              │
│                    │                                                        │
│                    ▼                                                        │
│   Neighbor:  neigh_output() / arp_send()                                    │
│              {                                                              │
│                  /* Resolve MAC address */                                  │
│                  dev_queue_xmit(skb);                                       │
│              }                                                              │
│                    │                                                        │
│                    ▼                                                        │
│   Net Core:  dev_queue_xmit()                                               │
│              {                                                              │
│                  /* Traffic control (qdisc) */                              │
│                  dev_hard_start_xmit(skb, dev);                             │
│                  dev->netdev_ops->ndo_start_xmit(skb, dev);  ──┐            │
│              }                               │ net_device_ops               │
│                    │                         │                              │
│   ═══════════════════════════════════════════│══════════════════════════    │
│                                              ▼                              │
│   Driver:    e1000_xmit_frame()                                             │
│              {                                                              │
│                  /* Build TX descriptor */                                  │
│                  /* Setup DMA */                                            │
│                  /* Ring doorbell */                                        │
│              }                                                              │
│                    │                                                        │
│                    ▼                                                        │
│   Hardware:  [ NIC ] ──► [ ETHERNET WIRE ]                                  │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

**中文说明：**
网络发送路径同样遵循严格的分层：
1. 用户调用 sendto()
2. Socket 层通过 proto_ops 调用协议层
3. 协议层（UDP/TCP）构建传输层头，调用 IP 层
4. IP 层构建网络层头，进行路由查找
5. 邻居层解析 MAC 地址
6. 网络核心层进行流量控制，通过 net_device_ops 调用驱动
7. 驱动构建 DMA 描述符，触发硬件发送

每个边界都有明确的接口（proto_ops, net_device_ops）。

---

### 4.3 Socket Operations Structure

```c
/* include/linux/net.h */

/*
 * This is the contract between socket layer and protocol layer.
 * Each protocol (TCP, UDP, RAW, etc.) implements this interface.
 */
struct proto_ops {
    int        family;
    struct module    *owner;
    
    /* Connection lifecycle */
    int    (*release)   (struct socket *sock);
    int    (*bind)      (struct socket *sock, struct sockaddr *addr, int len);
    int    (*connect)   (struct socket *sock, struct sockaddr *addr, 
                         int len, int flags);
    int    (*accept)    (struct socket *sock, struct socket *newsock, int flags);
    int    (*listen)    (struct socket *sock, int len);
    
    /* Data transfer */
    int    (*sendmsg)   (struct kiocb *iocb, struct socket *sock,
                         struct msghdr *m, size_t len);
    int    (*recvmsg)   (struct kiocb *iocb, struct socket *sock,
                         struct msghdr *m, size_t len, int flags);
    
    /* Control */
    int    (*ioctl)     (struct socket *sock, unsigned int cmd, unsigned long arg);
    int    (*getname)   (struct socket *sock, struct sockaddr *addr,
                         int *len, int peer);
    unsigned int (*poll)(struct file *file, struct socket *sock,
                         struct poll_table_struct *wait);
    /* ... */
};

/* Example: UDP protocol operations */
const struct proto_ops inet_dgram_ops = {
    .family        = PF_INET,
    .owner         = THIS_MODULE,
    .release       = inet_release,
    .bind          = inet_bind,
    .connect       = inet_dgram_connect,
    .accept        = sock_no_accept,        /* UDP doesn't accept */
    .sendmsg       = inet_sendmsg,          /* → udp_sendmsg */
    .recvmsg       = inet_recvmsg,          /* → udp_recvmsg */
    /* ... */
};
```

---

## 5. Complete Userspace Examples

### 5.1 Example 1: File I/O (Exercising VFS Layer)

```c
/*
 * file_io_example.c - Demonstrates file I/O layer boundaries
 * 
 * This program exercises:
 *   User Space → System Call → VFS → Filesystem → Block Layer → Driver
 *
 * Compile: gcc -o file_io file_io_example.c
 * Run:     ./file_io
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <errno.h>

#define TEST_FILE "/tmp/layer_test.dat"
#define BUFFER_SIZE 4096

/*
 * Interaction Type 1: Downward Call
 * User space → Syscall → VFS → Filesystem
 */
void demonstrate_downward_call(void)
{
    int fd;
    char write_buf[BUFFER_SIZE];
    char read_buf[BUFFER_SIZE];
    ssize_t bytes;

    printf("\n=== Type 1: Downward Call (User → VFS → FS) ===\n");

    /* Fill buffer with pattern */
    memset(write_buf, 'A', BUFFER_SIZE);

    /*
     * open() syscall:
     *   → sys_open()
     *     → do_sys_open()
     *       → do_filp_open()
     *         → path_openat()
     *           → inode->i_fop->open()  [ext4_file_open]
     */
    fd = open(TEST_FILE, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("open");
        return;
    }
    printf("Opened file (fd=%d)\n", fd);

    /*
     * write() syscall:
     *   → sys_write()
     *     → vfs_write()
     *       → file->f_op->write()  [ext4_file_write / generic_file_aio_write]
     *         → Page cache operations
     *           → submit_bio()     [if writeback needed]
     */
    bytes = write(fd, write_buf, BUFFER_SIZE);
    if (bytes < 0) {
        perror("write");
        close(fd);
        return;
    }
    printf("Wrote %zd bytes\n", bytes);

    /* Reset file position */
    lseek(fd, 0, SEEK_SET);

    /*
     * read() syscall:
     *   → sys_read()
     *     → vfs_read()
     *       → file->f_op->read()  [ext4_file_read / generic_file_aio_read]
     *         → Page cache lookup
     *           → submit_bio()    [if cache miss]
     */
    bytes = read(fd, read_buf, BUFFER_SIZE);
    if (bytes < 0) {
        perror("read");
        close(fd);
        return;
    }
    printf("Read %zd bytes\n", bytes);

    /* Verify data integrity */
    if (memcmp(write_buf, read_buf, BUFFER_SIZE) == 0) {
        printf("Data verified successfully!\n");
    } else {
        printf("Data mismatch!\n");
    }

    close(fd);
}

/*
 * Interaction Type 3: Demonstrating Registration
 * (From userspace, we see the effect of driver registration)
 */
void demonstrate_registration_effect(void)
{
    int fd;
    struct stat st;

    printf("\n=== Type 3: Registration Effect ===\n");

    /*
     * When we open a device file, the kernel uses the registered
     * file_operations from the driver.
     *
     * The driver called cdev_add() during init:
     *   → kobj_map(cdev_map, dev, count, ..., cdev)
     *
     * Now when we open, VFS finds the cdev and uses its ops:
     *   → chrdev_open()
     *     → kobj_lookup(cdev_map, inode->i_rdev, ...)
     *       → inode->i_fop = cdev->ops
     */
    
    /* Example: /dev/null is a registered char device */
    fd = open("/dev/null", O_RDWR);
    if (fd < 0) {
        perror("open /dev/null");
        return;
    }

    if (fstat(fd, &st) == 0) {
        printf("/dev/null: major=%d, minor=%d\n",
               major(st.st_rdev), minor(st.st_rdev));
        printf("This device was registered with register_chrdev()\n");
    }

    /* Write to /dev/null - data disappears */
    char buf[] = "This goes nowhere";
    ssize_t n = write(fd, buf, sizeof(buf));
    printf("Wrote %zd bytes to /dev/null (discarded by driver)\n", n);

    close(fd);

    /* Example: /dev/zero is another registered device */
    fd = open("/dev/zero", O_RDONLY);
    if (fd >= 0) {
        char zero_buf[16];
        n = read(fd, zero_buf, sizeof(zero_buf));
        printf("Read %zd bytes from /dev/zero (all zeros from driver)\n", n);
        close(fd);
    }
}

/*
 * Interaction Type 4: Service Call
 * Demonstrating how userspace triggers kernel services
 */
void demonstrate_service_effect(void)
{
    int fd;
    
    printf("\n=== Type 4: Service Call Effect ===\n");

    /*
     * When we call mmap(), the kernel uses memory management services:
     *   → sys_mmap()
     *     → vm_mmap()
     *       → do_mmap()
     *         → kmalloc()        [service: memory allocation]
     *         → spin_lock()      [service: synchronization]
     *         → ...
     */
    
    fd = open(TEST_FILE, O_RDONLY);
    if (fd < 0) {
        perror("open for mmap");
        return;
    }

    /* Get file size */
    struct stat st;
    fstat(fd, &st);
    
    if (st.st_size > 0) {
        /* mmap triggers multiple kernel services */
        void *addr = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (addr != MAP_FAILED) {
            printf("mmap'd %ld bytes at %p\n", st.st_size, addr);
            printf("(Kernel used kmalloc, spin_lock, page table ops internally)\n");
            munmap(addr, st.st_size);
        }
    }

    close(fd);
}

/*
 * Demonstrating layer boundaries with ioctl
 */
void demonstrate_ioctl_boundary(void)
{
    int fd;
    
    printf("\n=== ioctl: Crossing Multiple Layers ===\n");

    /*
     * ioctl goes: syscall → VFS → driver/filesystem
     * Each layer may handle different ioctl commands
     */
    
    fd = open(TEST_FILE, O_RDONLY);
    if (fd < 0) {
        perror("open for ioctl");
        return;
    }

    /* FIONREAD: handled by VFS layer */
    int bytes_available;
    if (ioctl(fd, FIONREAD, &bytes_available) == 0) {
        printf("FIONREAD (VFS layer): %d bytes available\n", bytes_available);
    }

    /* Get filesystem block size */
    struct stat st;
    if (fstat(fd, &st) == 0) {
        printf("Filesystem block size: %ld\n", st.st_blksize);
    }

    close(fd);

    /* Block device ioctl example */
    fd = open("/dev/sda", O_RDONLY);
    if (fd >= 0) {
        unsigned long long size;
        if (ioctl(fd, BLKGETSIZE64, &size) == 0) {
            printf("/dev/sda size (block layer ioctl): %llu bytes\n", size);
        }
        close(fd);
    } else {
        printf("(Cannot open /dev/sda - need root)\n");
    }
}

int main(void)
{
    printf("========================================\n");
    printf("  Linux Layer Boundary Demonstration\n");
    printf("========================================\n");

    demonstrate_downward_call();
    demonstrate_registration_effect();
    demonstrate_service_effect();
    demonstrate_ioctl_boundary();

    /* Cleanup */
    unlink(TEST_FILE);

    printf("\n=== Summary ===\n");
    printf("Every syscall crosses multiple layer boundaries.\n");
    printf("Each boundary is defined by an operations structure.\n");
    printf("Layers communicate ONLY through these defined interfaces.\n");

    return 0;
}
```

---

### 5.2 Example 2: Network Socket (Exercising Protocol Layers)

```c
/*
 * network_layers_example.c - Demonstrates network layer boundaries
 * 
 * This program exercises:
 *   User Space → Socket Layer → Protocol (UDP/TCP) → IP → Driver
 *
 * Compile: gcc -o net_layers network_layers_example.c
 * Run:     ./net_layers
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <errno.h>

#define SERVER_PORT 12345
#define BUFFER_SIZE 1024

/*
 * Type 1: Downward Call through Protocol Layers
 */
void demonstrate_udp_layers(void)
{
    int sock;
    struct sockaddr_in addr;
    char message[] = "Hello through layers!";
    
    printf("\n=== UDP: Downward Call Through Layers ===\n");

    /*
     * socket() syscall:
     *   → sys_socket()
     *     → sock_create()
     *       → __sock_create()
     *         → pf->create()  [inet_create for PF_INET]
     *           → Socket layer sets up sock->ops = &inet_dgram_ops
     */
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return;
    }
    printf("Created UDP socket (fd=%d)\n", sock);
    printf("  Kernel: sock->ops = &inet_dgram_ops\n");

    /* Setup destination */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERVER_PORT);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    /*
     * sendto() syscall:
     *   → sys_sendto()
     *     → sock_sendmsg()
     *       → sock->ops->sendmsg()         [inet_sendmsg]
     *         → sk->sk_prot->sendmsg()     [udp_sendmsg]
     *           → ip_send_skb()
     *             → ip_local_out()
     *               → dst_output()
     *                 → ip_output()
     *                   → ip_finish_output()
     *                     → neigh_output()
     *                       → dev_queue_xmit()
     *                         → dev->netdev_ops->ndo_start_xmit()
     */
    printf("\nSending packet through layers:\n");
    printf("  sendto() → sys_sendto()\n");
    printf("    → sock->ops->sendmsg()  [Socket Layer: inet_dgram_ops]\n");
    printf("      → sk_prot->sendmsg()  [Protocol: udp_prot]\n");
    printf("        → ip_send_skb()     [IP Layer]\n");
    printf("          → dev_queue_xmit() [Net Core]\n");
    printf("            → ndo_start_xmit() [Driver]\n");
    
    ssize_t sent = sendto(sock, message, strlen(message), 0,
                          (struct sockaddr *)&addr, sizeof(addr));
    if (sent < 0) {
        /* Expected: no server listening */
        printf("  (sendto returned %zd, errno=%d - expected, no server)\n", 
               sent, errno);
    } else {
        printf("  Sent %zd bytes\n", sent);
    }

    close(sock);
}

/*
 * Type 1: TCP Connection - More Layers
 */
void demonstrate_tcp_layers(void)
{
    int sock;
    struct sockaddr_in addr;
    
    printf("\n=== TCP: Connection State Machine ===\n");

    /*
     * TCP socket creation uses different ops:
     *   sock->ops = &inet_stream_ops
     *   sk->sk_prot = &tcp_prot
     */
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return;
    }
    printf("Created TCP socket (fd=%d)\n", sock);
    printf("  Kernel: sock->ops = &inet_stream_ops\n");
    printf("  Kernel: sk->sk_prot = &tcp_prot\n");

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(80);  /* HTTP port */
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    /*
     * connect() for TCP:
     *   → sys_connect()
     *     → sock->ops->connect()           [inet_stream_connect]
     *       → sk->sk_prot->connect()       [tcp_v4_connect]
     *         → tcp_connect()
     *           → tcp_transmit_skb()       [send SYN]
     *             → ip_queue_xmit()
     *               → ... down to driver
     */
    printf("\nTCP connect traverses:\n");
    printf("  connect() → sock->ops->connect() [inet_stream_connect]\n");
    printf("    → sk_prot->connect()           [tcp_v4_connect]\n");
    printf("      → tcp_connect()              [TCP state machine]\n");
    printf("        → tcp_transmit_skb()       [Send SYN packet]\n");
    printf("          → ip_queue_xmit()        [IP layer]\n");

    /* Try to connect (will likely fail - no server) */
    int ret = connect(sock, (struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0) {
        printf("  connect() returned %d (errno=%d: %s)\n", 
               ret, errno, strerror(errno));
    }

    close(sock);
}

/*
 * Type 4: Network Service Calls
 */
void demonstrate_network_services(void)
{
    int sock;
    struct ifreq ifr;
    
    printf("\n=== Network Service Calls ===\n");

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return;
    }

    /*
     * ioctl for network info - demonstrates service layer
     *
     * SIOCGIFADDR:
     *   → sys_ioctl()
     *     → sock_ioctl()
     *       → dev_ioctl()
     *         → dev_ifsioc()
     *           → Accesses net_device structure
     */
    
    /* Get interface list */
    printf("Network interfaces (via ioctl service):\n");
    
    struct if_nameindex *if_ni, *i;
    if_ni = if_nameindex();
    if (if_ni != NULL) {
        for (i = if_ni; i->if_index != 0 && i->if_name != NULL; i++) {
            printf("  %d: %s", i->if_index, i->if_name);
            
            /* Get interface flags */
            strncpy(ifr.ifr_name, i->if_name, IFNAMSIZ - 1);
            if (ioctl(sock, SIOCGIFFLAGS, &ifr) == 0) {
                printf(" [%s%s]",
                       (ifr.ifr_flags & IFF_UP) ? "UP" : "DOWN",
                       (ifr.ifr_flags & IFF_LOOPBACK) ? ",LOOPBACK" : "");
            }
            
            /* Get MTU */
            if (ioctl(sock, SIOCGIFMTU, &ifr) == 0) {
                printf(" mtu=%d", ifr.ifr_mtu);
            }
            
            printf("\n");
        }
        if_freenameindex(if_ni);
    }

    close(sock);
}

/*
 * Type 2: Callback Effect - Demonstrating async notification
 */
void demonstrate_socket_options(void)
{
    int sock;
    int optval;
    socklen_t optlen = sizeof(optval);
    
    printf("\n=== Socket Options: Layer-Specific Settings ===\n");

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return;
    }

    /*
     * getsockopt/setsockopt:
     *   Different levels access different layers:
     *   - SOL_SOCKET:  Socket layer (generic)
     *   - IPPROTO_TCP: Protocol layer (TCP-specific)
     *   - IPPROTO_IP:  IP layer
     */
    
    /* Socket layer option */
    if (getsockopt(sock, SOL_SOCKET, SO_RCVBUF, &optval, &optlen) == 0) {
        printf("SOL_SOCKET / SO_RCVBUF = %d (socket layer)\n", optval);
    }

    /* TCP layer option */
    if (getsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &optval, &optlen) == 0) {
        printf("IPPROTO_TCP / TCP_NODELAY = %d (protocol layer)\n", optval);
    }

    /* Enable TCP_NODELAY - affects TCP layer behavior */
    optval = 1;
    if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval)) == 0) {
        printf("Set TCP_NODELAY=1 (disables Nagle algorithm in TCP layer)\n");
    }

    /* IP layer option - TTL */
    optlen = sizeof(optval);
    if (getsockopt(sock, IPPROTO_IP, IP_TTL, &optval, &optlen) == 0) {
        printf("IPPROTO_IP / IP_TTL = %d (IP layer)\n", optval);
    }

    close(sock);
}

/*
 * Raw socket: Bypassing protocol layer (needs root)
 */
void demonstrate_raw_socket(void)
{
    int sock;
    
    printf("\n=== Raw Socket: Direct IP Layer Access ===\n");

    /*
     * Raw socket bypasses TCP/UDP protocol layer:
     *   User → Socket Layer → IP Layer → Driver
     *
     * Requires CAP_NET_RAW capability
     */
    sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sock < 0) {
        printf("Raw socket requires root (errno=%d: %s)\n", 
               errno, strerror(errno));
        printf("  With raw socket, you write directly to IP layer\n");
        printf("  Bypassing TCP/UDP protocol processing\n");
        return;
    }

    printf("Created raw ICMP socket - direct IP layer access\n");
    close(sock);
}

int main(void)
{
    printf("==========================================\n");
    printf("  Network Layer Boundary Demonstration\n");
    printf("==========================================\n");

    demonstrate_udp_layers();
    demonstrate_tcp_layers();
    demonstrate_network_services();
    demonstrate_socket_options();
    demonstrate_raw_socket();

    printf("\n=== Summary ===\n");
    printf("Network stack has clear layer boundaries:\n");
    printf("  Socket Layer  → proto_ops (inet_dgram_ops, inet_stream_ops)\n");
    printf("  Protocol Layer → proto (tcp_prot, udp_prot)\n");
    printf("  IP Layer      → routing, fragmentation\n");
    printf("  Net Device    → net_device_ops\n");
    printf("Each layer only communicates through defined interfaces.\n");

    return 0;
}
```

---

### 5.3 Example 3: Character Device (Demonstrating Registration)

```c
/*
 * chardev_user_example.c - User space interaction with char devices
 * 
 * This demonstrates how user space interacts with registered drivers.
 *
 * Compile: gcc -o chardev_user chardev_user_example.c
 * Run:     ./chardev_user
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <linux/random.h>
#include <errno.h>

/*
 * Demonstrating /dev/random - a registered character device
 */
void demonstrate_random_device(void)
{
    int fd;
    unsigned char buf[16];
    int entropy_count;
    
    printf("\n=== /dev/random: Registered Char Device ===\n");

    /*
     * /dev/random was registered during kernel init:
     *   random_init()
     *     → misc_register(&random_dev)
     *       → Internally: cdev_add()
     *
     * When we open it:
     *   VFS looks up the device by major/minor
     *   Finds the registered cdev
     *   Uses cdev->ops (random_fops)
     */
    
    fd = open("/dev/random", O_RDONLY);
    if (fd < 0) {
        perror("open /dev/random");
        return;
    }

    struct stat st;
    fstat(fd, &st);
    printf("Device: major=%d, minor=%d\n", major(st.st_rdev), minor(st.st_rdev));
    printf("(Driver registered this major/minor at boot)\n");

    /*
     * read() on /dev/random:
     *   → vfs_read()
     *     → file->f_op->read  [random_read]
     *       → extract_entropy()
     */
    printf("\nReading random bytes:\n");
    ssize_t n = read(fd, buf, sizeof(buf));
    if (n > 0) {
        printf("  Got %zd bytes: ", n);
        for (int i = 0; i < n && i < 8; i++)
            printf("%02x ", buf[i]);
        printf("...\n");
    }

    /*
     * ioctl() on /dev/random:
     *   → vfs_ioctl()
     *     → file->f_op->unlocked_ioctl  [random_ioctl]
     *       → Handle RNDGETENTCNT etc.
     */
    if (ioctl(fd, RNDGETENTCNT, &entropy_count) == 0) {
        printf("  Entropy pool: %d bits\n", entropy_count);
        printf("  (Returned by driver's ioctl handler)\n");
    }

    close(fd);
}

/*
 * Demonstrating /dev/tty - terminal device layers
 */
void demonstrate_tty_device(void)
{
    int fd;
    struct stat st;
    
    printf("\n=== /dev/tty: TTY Subsystem Layers ===\n");

    /*
     * TTY has multiple layers:
     *   User → tty_fops → ldisc (line discipline) → tty_driver → hardware
     *
     * Each layer has its own operations structure.
     */
    
    fd = open("/dev/tty", O_RDWR);
    if (fd < 0) {
        perror("open /dev/tty");
        return;
    }

    fstat(fd, &st);
    printf("Device: major=%d, minor=%d\n", major(st.st_rdev), minor(st.st_rdev));

    printf("\nTTY Layer Stack:\n");
    printf("  User space (you)\n");
    printf("    ↓ open/read/write/ioctl\n");
    printf("  tty_fops (VFS interface)\n");
    printf("    ↓ tty->ops\n");
    printf("  Line Discipline (n_tty, etc.)\n");
    printf("    ↓ tty->ldisc->ops\n");
    printf("  TTY Driver (pty, serial, vt)\n");
    printf("    ↓ tty->driver->ops\n");
    printf("  Hardware (UART, pseudo-terminal)\n");

    /* TIOCGWINSZ - get window size (handled by tty layer) */
    struct winsize ws;
    if (ioctl(fd, TIOCGWINSZ, &ws) == 0) {
        printf("\nWindow size (from tty layer): %d rows × %d cols\n",
               ws.ws_row, ws.ws_col);
    }

    close(fd);
}

/*
 * Demonstrating device file creation
 */
void demonstrate_device_nodes(void)
{
    printf("\n=== Device Nodes: User Space View of Registration ===\n");

    /*
     * Device nodes in /dev are created by:
     * 1. devtmpfs (automatic, based on driver registration)
     * 2. udev rules (user space daemon)
     * 3. Manual mknod (legacy)
     *
     * The major/minor numbers link to registered drivers.
     */
    
    printf("Examining /dev entries:\n\n");
    
    struct {
        const char *path;
        const char *description;
    } devices[] = {
        {"/dev/null",    "Null device (discards writes, reads EOF)"},
        {"/dev/zero",    "Zero device (reads zeros)"},
        {"/dev/random",  "Random number generator"},
        {"/dev/urandom", "Non-blocking random"},
        {"/dev/tty",     "Current terminal"},
        {"/dev/console", "System console"},
        {NULL, NULL}
    };

    for (int i = 0; devices[i].path != NULL; i++) {
        struct stat st;
        if (stat(devices[i].path, &st) == 0) {
            printf("%-15s major=%3d minor=%3d  %s\n",
                   devices[i].path,
                   major(st.st_rdev),
                   minor(st.st_rdev),
                   devices[i].description);
        }
    }

    printf("\nMajor number identifies the driver.\n");
    printf("Minor number identifies specific device instance.\n");
    printf("Driver registration creates this mapping.\n");
}

/*
 * Demonstrating procfs and sysfs (virtual filesystems)
 */
void demonstrate_virtual_fs(void)
{
    int fd;
    char buf[256];
    ssize_t n;
    
    printf("\n=== Virtual Filesystems: Different file_operations ===\n");

    /*
     * /proc and /sys are not backed by disk storage.
     * They use different file_operations that generate data dynamically.
     */
    
    /* procfs: kernel/process information */
    fd = open("/proc/version", O_RDONLY);
    if (fd >= 0) {
        n = read(fd, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            printf("/proc/version:\n  %s", buf);
        }
        close(fd);
    }
    printf("  (Generated by proc_version_show, not read from disk)\n");

    /* sysfs: device model information */
    fd = open("/sys/class/tty/tty0/dev", O_RDONLY);
    if (fd >= 0) {
        n = read(fd, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            printf("\n/sys/class/tty/tty0/dev: %s", buf);
        }
        close(fd);
        printf("  (Generated by sysfs attribute show function)\n");
    }

    printf("\nVirtual FS layer boundaries:\n");
    printf("  VFS → procfs_ops → proc show functions\n");
    printf("  VFS → sysfs_ops  → kobject attribute functions\n");
}

int main(void)
{
    printf("==========================================\n");
    printf("  Character Device Layer Demonstration\n");
    printf("==========================================\n");

    demonstrate_random_device();
    demonstrate_tty_device();
    demonstrate_device_nodes();
    demonstrate_virtual_fs();

    printf("\n=== Key Takeaways ===\n");
    printf("1. Drivers register with the kernel (cdev_add, misc_register)\n");
    printf("2. Registration provides an operations structure\n");
    printf("3. Device nodes (major:minor) map to registered drivers\n");
    printf("4. VFS routes operations through the registered ops\n");
    printf("5. Each subsystem (tty, random, proc) has its own ops\n");

    return 0;
}
```

---

### 5.4 Example 4: Event Notification (poll/epoll)

```c
/*
 * event_notification_example.c - Demonstrates async notification layers
 * 
 * This shows how poll/epoll work through layer boundaries.
 *
 * Compile: gcc -o event_notify event_notification_example.c
 * Run:     ./event_notify
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <sys/eventfd.h>
#include <errno.h>

#define MAX_EVENTS 10

/*
 * Demonstrating epoll - event notification mechanism
 */
void demonstrate_epoll_layers(void)
{
    int epfd, timerfd, eventfd;
    struct epoll_event ev, events[MAX_EVENTS];
    struct itimerspec ts;
    
    printf("\n=== epoll: Event Notification Layers ===\n");

    /*
     * epoll architecture:
     *   
     *   User space          Kernel
     *   ──────────          ──────
     *   epoll_wait() ──────► eventpoll
     *                            │
     *                            ▼
     *                       Wait queue
     *                            │
     *         ┌──────────────────┼──────────────────┐
     *         ▼                  ▼                  ▼
     *   [timerfd poll]    [eventfd poll]    [socket poll]
     *         │                  │                  │
     *         ▼                  ▼                  ▼
     *   timer_settime     eventfd_write       network rx
     */
    
    /* Create epoll instance */
    epfd = epoll_create1(0);
    if (epfd < 0) {
        perror("epoll_create1");
        return;
    }
    printf("Created epoll fd=%d\n", epfd);

    /* Create timerfd - timer events */
    timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (timerfd < 0) {
        perror("timerfd_create");
        close(epfd);
        return;
    }
    printf("Created timerfd fd=%d\n", timerfd);

    /* Create eventfd - manual signaling */
    eventfd = eventfd(0, EFD_NONBLOCK);
    if (eventfd < 0) {
        perror("eventfd");
        close(timerfd);
        close(epfd);
        return;
    }
    printf("Created eventfd fd=%d\n", eventfd);

    /*
     * epoll_ctl() adds file to epoll:
     *   → sys_epoll_ctl()
     *     → ep_insert()
     *       → file->f_op->poll()  ← Type 1: Downward call
     *         → Each file type implements poll differently
     *         → Returns wait queue to watch
     */
    
    ev.events = EPOLLIN;
    ev.data.fd = timerfd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, timerfd, &ev) < 0) {
        perror("epoll_ctl timerfd");
    }
    printf("Added timerfd to epoll (calls timerfd->f_op->poll)\n");

    ev.data.fd = eventfd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, eventfd, &ev) < 0) {
        perror("epoll_ctl eventfd");
    }
    printf("Added eventfd to epoll (calls eventfd->f_op->poll)\n");

    /* Arm timer to fire in 100ms */
    ts.it_value.tv_sec = 0;
    ts.it_value.tv_nsec = 100000000;  /* 100ms */
    ts.it_interval.tv_sec = 0;
    ts.it_interval.tv_nsec = 0;
    timerfd_settime(timerfd, 0, &ts, NULL);
    printf("\nArmed timer for 100ms\n");

    /* Signal eventfd */
    uint64_t val = 1;
    write(eventfd, &val, sizeof(val));
    printf("Signaled eventfd\n");

    /*
     * epoll_wait():
     *   → sys_epoll_wait()
     *     → ep_poll()
     *       → Wait on epoll's wait queue
     *       → Type 2: Upward callback when events ready
     *         → ep_poll_callback() wakes us up
     */
    
    printf("\nCalling epoll_wait (waiting for events)...\n");
    int nfds = epoll_wait(epfd, events, MAX_EVENTS, 1000);
    
    printf("epoll_wait returned %d events:\n", nfds);
    for (int i = 0; i < nfds; i++) {
        if (events[i].data.fd == timerfd) {
            printf("  - timerfd ready (timer expired)\n");
            uint64_t exp;
            read(timerfd, &exp, sizeof(exp));
        } else if (events[i].data.fd == eventfd) {
            printf("  - eventfd ready (was signaled)\n");
            read(eventfd, &val, sizeof(val));
        }
    }

    printf("\nLayer interaction summary:\n");
    printf("  epoll_ctl() → file->f_op->poll() [get wait queue]\n");
    printf("  Event occurs → callback wakes epoll\n");
    printf("  epoll_wait() returns → user handles event\n");

    close(eventfd);
    close(timerfd);
    close(epfd);
}

/*
 * Demonstrating poll with pipes
 */
void demonstrate_pipe_poll(void)
{
    int pipefd[2];
    struct epoll_event ev, events[MAX_EVENTS];
    int epfd;
    
    printf("\n=== Pipe poll: Producer/Consumer Pattern ===\n");

    if (pipe(pipefd) < 0) {
        perror("pipe");
        return;
    }
    printf("Created pipe: read_fd=%d, write_fd=%d\n", pipefd[0], pipefd[1]);

    /* Make read end non-blocking */
    fcntl(pipefd[0], F_SETFL, O_NONBLOCK);

    epfd = epoll_create1(0);
    
    ev.events = EPOLLIN;
    ev.data.fd = pipefd[0];
    epoll_ctl(epfd, EPOLL_CTL_ADD, pipefd[0], &ev);

    /* Fork to demonstrate cross-process notification */
    pid_t pid = fork();
    if (pid == 0) {
        /* Child: write to pipe after delay */
        close(pipefd[0]);
        usleep(50000);  /* 50ms */
        write(pipefd[1], "Hello from child!", 17);
        close(pipefd[1]);
        exit(0);
    }

    /* Parent: wait for data */
    close(pipefd[1]);
    
    printf("Parent waiting for child to write...\n");
    
    /*
     * When child writes to pipe:
     *   write() → pipe_write()
     *     → wake_up_interruptible(&pipe->wait)
     *       → Type 2: Callback to epoll
     *         → ep_poll_callback()
     *           → Wakes parent's epoll_wait
     */
    
    int nfds = epoll_wait(epfd, events, MAX_EVENTS, 1000);
    if (nfds > 0) {
        char buf[64];
        ssize_t n = read(pipefd[0], buf, sizeof(buf) - 1);
        buf[n] = '\0';
        printf("Received: \"%s\"\n", buf);
    }

    close(pipefd[0]);
    close(epfd);
}

int main(void)
{
    printf("==========================================\n");
    printf("  Event Notification Layer Demonstration\n");
    printf("==========================================\n");

    demonstrate_epoll_layers();
    demonstrate_pipe_poll();

    printf("\n=== Key Insights ===\n");
    printf("1. epoll is a layer that aggregates events from many sources\n");
    printf("2. Each file type implements poll() differently\n");
    printf("3. Callbacks (Type 2) notify epoll when events occur\n");
    printf("4. User space sees unified interface regardless of source\n");

    return 0;
}
```

---

## 6. Summary

### 6.1 Layer Boundary Patterns

```
╔═══════════════════════════════════════════════════════════════════════════════╗
║                    SUMMARY: LAYER BOUNDARY PATTERNS                           ║
╠═══════════════════════════════════════════════════════════════════════════════╣
║                                                                               ║
║   PATTERN              MECHANISM                  KERNEL EXAMPLE              ║
║   ───────              ─────────                  ──────────────              ║
║                                                                               ║
║   Downward Call        Function pointer table     file_operations             ║
║                        (ops structure)            block_device_operations     ║
║                                                   net_device_ops              ║
║                                                   proto_ops                   ║
║                                                                               ║
║   Upward Callback      Callback in request        bio->bi_end_io              ║
║                        or context structure       sk_buff callbacks           ║
║                                                   timer_list->function        ║
║                                                                               ║
║   Registration         Framework register/        cdev_add()                  ║
║                        unregister functions       register_netdev()           ║
║                                                   register_filesystem()       ║
║                                                                               ║
║   Service Call         Exported kernel            kmalloc/kfree               ║
║                        functions                  spin_lock/unlock            ║
║                                                   printk                      ║
║                                                                               ║
║   Event Notification   Notifier chains,           notifier_call_chain()       ║
║                        hooks                      netfilter hooks             ║
║                                                   netdev_chain                ║
║                                                                               ║
╚═══════════════════════════════════════════════════════════════════════════════╝
```

**中文总结：**

Linux 内核的分层架构通过五种模式实现层间通信：

1. **向下调用**：通过函数指针表（如 file_operations）实现多态，上层不知道下层具体实现
2. **向上回调**：下层完成异步操作后通过回调通知上层（如 bio->bi_end_io）
3. **注册机制**：下层向上层框架注册自己（如 cdev_add），让框架知道如何路由请求
4. **服务调用**：横向调用共享服务（kmalloc, spin_lock），不遵循层级关系
5. **事件通知**：解耦的发布-订阅模式（notifier chain），发布者不知道订阅者

### 6.2 Design Principles

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    DESIGN PRINCIPLES                                         │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  1. INFORMATION HIDING                                                       │
│     • Internal structures are private                                        │
│     • Only expose what's necessary in headers                                │
│     • Use opaque handles where possible                                      │
│                                                                              │
│  2. DEPENDENCY INVERSION                                                     │
│     • High-level defines interfaces (file_operations)                        │
│     • Low-level implements interfaces (ext4_file_operations)                 │
│     • Both depend on abstraction, not each other                             │
│                                                                              │
│  3. SINGLE RESPONSIBILITY                                                    │
│     • VFS: namespace, permission, caching                                    │
│     • Filesystem: on-disk format, allocation                                 │
│     • Block: scheduling, merging                                             │
│     • Driver: hardware protocol                                              │
│                                                                              │
│  4. OPEN/CLOSED PRINCIPLE                                                    │
│     • Framework is closed for modification                                   │
│     • Open for extension via registration                                    │
│     • Add new filesystem without changing VFS                                │
│                                                                              │
│  5. INTERFACE SEGREGATION                                                    │
│     • Don't force implementing unused operations                             │
│     • NULL function pointers = not supported                                 │
│     • Caller checks before calling                                           │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

**最终总结（中文）：**

在 C 语言中实现分层架构需要程序员的自律和良好的设计：
- 头文件定义公开接口，实现文件隐藏细节
- 函数指针表实现多态，让上层与具体实现解耦
- 注册机制让框架保持稳定，新实现可以随时加入
- 遵守约定：不访问其他层的内部结构，不绕过接口直接调用

Linux 内核是 C 语言分层架构的典范。尽管语言本身不强制封装，但通过严格的代码审查和约定，内核维护了清晰的层次边界，使得这个数百万行的代码库能够持续演进和维护。

---

## 7. Userspace Simulation: The Five Interaction Types

This section provides complete, compilable userspace C programs that simulate each of the five interaction types. These examples help you understand the patterns without needing kernel development environment.

### 7.1 Type 1: Downward Call (Function Pointer Table)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                 SIMULATION: DOWNWARD CALL                                   │
│                                                                             │
│   ┌──────────────────────────────────────────────────────────────────────┐  │
│   │  Framework Layer (like VFS)                                          │  │
│   │                                                                      │  │
│   │  struct storage_ops {                                                │  │
│   │      int (*open)(struct storage *s);                                 │  │
│   │      int (*read)(struct storage *s, void *buf, size_t len);          │  │
│   │      int (*write)(struct storage *s, const void *buf, size_t len);   │  │
│   │      void (*close)(struct storage *s);                               │  │
│   │  };                                                                  │  │
│   │                                                                      │  │
│   │  /* Framework code - doesn't know implementation */                  │  │
│   │  int storage_read(struct storage *s, void *buf, size_t len) {        │  │
│   │      return s->ops->read(s, buf, len);  ◄─── Downward call           │  │
│   │  }                                                                   │  │
│   └────────────────────────────────┬─────────────────────────────────────┘  │
│                                    │                                        │
│   ═════════════════════════════════│════════════════════════════════════    │
│                                    │ BOUNDARY: storage_ops                  │
│   ═════════════════════════════════│════════════════════════════════════    │
│                                    │                                        │
│          ┌─────────────────────────┴─────────────────────────┐              │
│          │                                                   │              │
│          ▼                                                   ▼              │
│   ┌─────────────────────┐                     ┌─────────────────────┐       │
│   │  Memory Backend     │                     │  File Backend       │       │
│   │                     │                     │                     │       │
│   │  .read = mem_read   │                     │  .read = file_read  │       │
│   │  .write = mem_write │                     │  .write = file_write│       │
│   └─────────────────────┘                     └─────────────────────┘       │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

**中文说明：**
这个示例模拟了 VFS 的 file_operations 模式。框架层定义操作接口（storage_ops），不同的后端（内存、文件）实现该接口。框架通过函数指针调用具体实现，完全不知道底层是什么。这就是 C 语言中的"多态"。

```c
/*
 * type1_downward_call.c - Simulates VFS file_operations pattern
 *
 * This demonstrates how upper layer calls lower layer through
 * function pointer tables, achieving polymorphism in C.
 *
 * Compile: gcc -o type1 type1_downward_call.c -lpthread
 * Run:     ./type1
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * ═══════════════════════════════════════════════════════════════════
 * LAYER 1: INTERFACE DEFINITION (like include/linux/fs.h)
 * This is the CONTRACT between layers
 * ═══════════════════════════════════════════════════════════════════
 */

/* Forward declaration - implementation is hidden */
struct storage;

/* 
 * Operations structure - THE BOUNDARY CONTRACT
 * Similar to struct file_operations in Linux kernel
 */
struct storage_ops {
    const char *name;                                          /* Backend name */
    int (*open)(struct storage *s);                            /* Open storage */
    ssize_t (*read)(struct storage *s, void *buf, size_t len, size_t offset);
    ssize_t (*write)(struct storage *s, const void *buf, size_t len, size_t offset);
    void (*close)(struct storage *s);                          /* Close storage */
};

/* 
 * Storage handle - contains ops pointer
 * Similar to struct file in Linux kernel
 */
struct storage {
    const struct storage_ops *ops;    /* ← Points to implementation's ops */
    void *private_data;               /* ← Implementation-specific data */
    size_t size;                      /* Total size */
};

/*
 * ═══════════════════════════════════════════════════════════════════
 * LAYER 2: FRAMEWORK (like fs/read_write.c - VFS layer)
 * Framework code that is UNAWARE of specific implementations
 * ═══════════════════════════════════════════════════════════════════
 */

/* 
 * Framework function - similar to vfs_read()
 * Note: It doesn't know if it's reading from memory, file, or network!
 */
ssize_t storage_read(struct storage *s, void *buf, size_t len, size_t offset)
{
    /* Validation - framework's responsibility */
    if (!s || !s->ops) {
        fprintf(stderr, "storage_read: invalid storage\n");
        return -1;
    }
    
    if (!s->ops->read) {
        fprintf(stderr, "storage_read: read not supported\n");
        return -1;  /* Operation not supported */
    }
    
    /* ══════════════════════════════════════════════════════════════
     * THE DOWNWARD CALL - crossing the layer boundary
     * This is equivalent to: file->f_op->read(file, buf, count, pos)
     * Framework doesn't know which implementation will handle this
     * ══════════════════════════════════════════════════════════════ */
    return s->ops->read(s, buf, len, offset);
}

/* Framework function - similar to vfs_write() */
ssize_t storage_write(struct storage *s, const void *buf, size_t len, size_t offset)
{
    if (!s || !s->ops || !s->ops->write) {
        return -1;
    }
    
    /* THE DOWNWARD CALL */
    return s->ops->write(s, buf, len, offset);
}

/*
 * ═══════════════════════════════════════════════════════════════════
 * LAYER 3A: MEMORY BACKEND IMPLEMENTATION (like a ramfs)
 * One possible implementation of the storage_ops interface
 * ═══════════════════════════════════════════════════════════════════
 */

/* Private data for memory backend */
struct mem_storage_data {
    char *buffer;
    size_t capacity;
};

static int mem_open(struct storage *s)
{
    printf("  [mem_backend] Opening memory storage\n");
    return 0;
}

static ssize_t mem_read(struct storage *s, void *buf, size_t len, size_t offset)
{
    struct mem_storage_data *data = s->private_data;
    
    printf("  [mem_backend] Reading %zu bytes at offset %zu\n", len, offset);
    
    if (offset >= data->capacity)
        return 0;  /* EOF */
    
    if (offset + len > data->capacity)
        len = data->capacity - offset;
    
    /* Copy from memory buffer */
    memcpy(buf, data->buffer + offset, len);
    return len;
}

static ssize_t mem_write(struct storage *s, const void *buf, size_t len, size_t offset)
{
    struct mem_storage_data *data = s->private_data;
    
    printf("  [mem_backend] Writing %zu bytes at offset %zu\n", len, offset);
    
    if (offset + len > data->capacity) {
        len = data->capacity - offset;
    }
    
    memcpy(data->buffer + offset, buf, len);
    return len;
}

static void mem_close(struct storage *s)
{
    printf("  [mem_backend] Closing memory storage\n");
}

/* Memory backend's operations - filled function pointer table */
static const struct storage_ops mem_storage_ops = {
    .name  = "memory",
    .open  = mem_open,
    .read  = mem_read,     /* ← Points to mem_read function */
    .write = mem_write,    /* ← Points to mem_write function */
    .close = mem_close,
};

/* Factory function to create memory storage */
struct storage *create_mem_storage(size_t size)
{
    struct storage *s = malloc(sizeof(*s));
    struct mem_storage_data *data = malloc(sizeof(*data));
    
    data->buffer = calloc(1, size);
    data->capacity = size;
    
    s->ops = &mem_storage_ops;    /* ← Set operations table */
    s->private_data = data;
    s->size = size;
    
    return s;
}

/*
 * ═══════════════════════════════════════════════════════════════════
 * LAYER 3B: FILE BACKEND IMPLEMENTATION (like ext4)
 * Another implementation of the same interface
 * ═══════════════════════════════════════════════════════════════════
 */

struct file_storage_data {
    FILE *fp;
    char filename[256];
};

static int file_open(struct storage *s)
{
    struct file_storage_data *data = s->private_data;
    printf("  [file_backend] Opening file: %s\n", data->filename);
    
    data->fp = fopen(data->filename, "w+b");
    if (!data->fp) {
        perror("fopen");
        return -1;
    }
    return 0;
}

static ssize_t file_read(struct storage *s, void *buf, size_t len, size_t offset)
{
    struct file_storage_data *data = s->private_data;
    
    printf("  [file_backend] Reading %zu bytes at offset %zu from file\n", len, offset);
    
    fseek(data->fp, offset, SEEK_SET);
    return fread(buf, 1, len, data->fp);
}

static ssize_t file_write(struct storage *s, const void *buf, size_t len, size_t offset)
{
    struct file_storage_data *data = s->private_data;
    
    printf("  [file_backend] Writing %zu bytes at offset %zu to file\n", len, offset);
    
    fseek(data->fp, offset, SEEK_SET);
    return fwrite(buf, 1, len, data->fp);
}

static void file_close(struct storage *s)
{
    struct file_storage_data *data = s->private_data;
    printf("  [file_backend] Closing file: %s\n", data->filename);
    if (data->fp) {
        fclose(data->fp);
        data->fp = NULL;
    }
}

/* File backend's operations */
static const struct storage_ops file_storage_ops = {
    .name  = "file",
    .open  = file_open,
    .read  = file_read,
    .write = file_write,
    .close = file_close,
};

struct storage *create_file_storage(const char *filename)
{
    struct storage *s = malloc(sizeof(*s));
    struct file_storage_data *data = malloc(sizeof(*data));
    
    strncpy(data->filename, filename, sizeof(data->filename) - 1);
    data->fp = NULL;
    
    s->ops = &file_storage_ops;    /* ← Different operations table */
    s->private_data = data;
    s->size = 0;
    
    return s;
}

/*
 * ═══════════════════════════════════════════════════════════════════
 * APPLICATION CODE - Uses framework, unaware of backends
 * ═══════════════════════════════════════════════════════════════════
 */

/* 
 * This function works with ANY storage backend
 * It only knows about the storage_ops interface
 */
void test_storage(struct storage *s, const char *test_name)
{
    char write_data[] = "Hello, layered architecture!";
    char read_buf[64] = {0};
    
    printf("\n=== Testing %s backend ===\n", test_name);
    
    /* Open - calls s->ops->open (don't know which one!) */
    s->ops->open(s);
    
    /* Write through framework - THE DOWNWARD CALL */
    printf("Framework: calling storage_write()...\n");
    ssize_t written = storage_write(s, write_data, strlen(write_data), 0);
    printf("Framework: wrote %zd bytes\n", written);
    
    /* Read through framework - THE DOWNWARD CALL */
    printf("Framework: calling storage_read()...\n");
    ssize_t bytes_read = storage_read(s, read_buf, sizeof(read_buf) - 1, 0);
    printf("Framework: read %zd bytes: \"%s\"\n", bytes_read, read_buf);
    
    /* Close */
    s->ops->close(s);
}

int main(void)
{
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║  Type 1: Downward Call via Function Pointer Table          ║\n");
    printf("║  Simulating: file->f_op->read() pattern                    ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    
    /* Create memory-backed storage */
    struct storage *mem_store = create_mem_storage(1024);
    test_storage(mem_store, "MEMORY");
    
    /* Create file-backed storage - SAME interface, DIFFERENT implementation */
    struct storage *file_store = create_file_storage("/tmp/type1_test.dat");
    test_storage(file_store, "FILE");
    
    printf("\n=== KEY INSIGHT ===\n");
    printf("The framework (storage_read/storage_write) used the SAME code\n");
    printf("to operate on DIFFERENT backends. This is polymorphism in C.\n");
    printf("The boundary is the storage_ops structure.\n");
    
    /* Cleanup */
    free(mem_store->private_data);
    free(mem_store);
    free(file_store->private_data);
    free(file_store);
    unlink("/tmp/type1_test.dat");
    
    return 0;
}
```

---

### 7.2 Type 2: Upward Callback (Completion Notification)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                 SIMULATION: UPWARD CALLBACK                                 │
│                                                                             │
│   ┌──────────────────────────────────────────────────────────────────────┐  │
│   │  Upper Layer (like Filesystem)                                       │  │
│   │                                                                      │  │
│   │  void my_completion_handler(struct request *req, int status) {       │  │
│   │      printf("I/O completed with status %d\n", status);               │  │
│   │  }                                                                   │  │
│   │                                                                      │  │
│   │  /* Submit request with callback */                                  │  │
│   │  req->complete = my_completion_handler;  ◄─── Register callback      │  │
│   │  submit_request(req);                                                │  │
│   │                                                                      │  │
│   └──────────────────────────────────────┬───────────────────────────────┘  │
│                                          │                                  │
│   ═══════════════════════════════════════│═══════════════════════════════   │
│                                          │ BOUNDARY: completion callback    │
│   ═══════════════════════════════════════│═══════════════════════════════   │
│                                          │                                  │
│                                          ▼                                  │
│   ┌──────────────────────────────────────────────────────────────────────┐  │
│   │  Lower Layer (like Block Layer / Worker Thread)                      │  │
│   │                                                                      │  │
│   │  void process_request(struct request *req) {                         │  │
│   │      /* Do async work... */                                          │  │
│   │      int result = do_io(req);                                        │  │
│   │                                                                      │  │
│   │      /* UPWARD CALLBACK - notify upper layer */                      │  │
│   │      if (req->complete)                                              │  │
│   │          req->complete(req, result);  ◄─── Call upper layer back     │  │
│   │  }                                                                   │  │
│   │                                                                      │  │
│   └──────────────────────────────────────────────────────────────────────┘  │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

**中文说明：**
这个示例模拟了块层的 bio->bi_end_io 回调模式。上层（文件系统）在提交 I/O 请求时注册一个完成回调。下层（块层/工作线程）在 I/O 完成后调用该回调通知上层。下层完全不知道上层是谁，只是调用注册的函数指针。这种模式用于所有异步操作的完成通知。

```c
/*
 * type2_upward_callback.c - Simulates bio->bi_end_io callback pattern
 *
 * This demonstrates how lower layer notifies upper layer
 * when async operations complete, without knowing who the upper layer is.
 *
 * Compile: gcc -o type2 type2_upward_callback.c -lpthread
 * Run:     ./type2
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

/*
 * ═══════════════════════════════════════════════════════════════════
 * REQUEST STRUCTURE (like struct bio in Linux kernel)
 * Contains the callback pointer for completion notification
 * ═══════════════════════════════════════════════════════════════════
 */

struct io_request;  /* Forward declaration */

/* Completion callback type - like bio_end_io_t */
typedef void (*completion_fn_t)(struct io_request *req, int error);

/* 
 * I/O Request structure - like struct bio
 * The 'complete' callback is the upward notification mechanism
 */
struct io_request {
    int id;                      /* Request ID */
    void *buffer;                /* Data buffer */
    size_t length;               /* Request length */
    size_t offset;               /* Offset in device */
    int is_write;                /* Write or read? */
    
    /* ══════════════════════════════════════════════════════════════
     * THE CALLBACK - this is how lower layer notifies upper layer
     * Equivalent to bio->bi_end_io in Linux kernel
     * Upper layer sets this before submitting request
     * ══════════════════════════════════════════════════════════════ */
    completion_fn_t complete;    /* ← Completion callback */
    void *private;               /* ← Upper layer's context */
};

/*
 * ═══════════════════════════════════════════════════════════════════
 * LOWER LAYER: I/O Processing (like block layer / device driver)
 * Processes requests asynchronously, calls back when done
 * ═══════════════════════════════════════════════════════════════════
 */

/* Simulated I/O queue */
static struct {
    struct io_request *requests[16];
    int head, tail;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    int running;
} io_queue = {
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .cond = PTHREAD_COND_INITIALIZER,
    .running = 1
};

/* Worker thread - processes I/O requests (like block layer softirq) */
void *io_worker_thread(void *arg)
{
    printf("[IO_WORKER] Worker thread started\n");
    
    while (io_queue.running) {
        struct io_request *req = NULL;
        
        pthread_mutex_lock(&io_queue.lock);
        while (io_queue.head == io_queue.tail && io_queue.running) {
            pthread_cond_wait(&io_queue.cond, &io_queue.lock);
        }
        
        if (!io_queue.running) {
            pthread_mutex_unlock(&io_queue.lock);
            break;
        }
        
        /* Dequeue request */
        req = io_queue.requests[io_queue.head];
        io_queue.head = (io_queue.head + 1) % 16;
        pthread_mutex_unlock(&io_queue.lock);
        
        /* Simulate I/O processing */
        printf("[IO_WORKER] Processing request #%d (%s, %zu bytes)\n",
               req->id, req->is_write ? "WRITE" : "READ", req->length);
        
        usleep(100000);  /* Simulate I/O delay (100ms) */
        
        int result = 0;  /* Success */
        if (req->length > 4096) {
            result = -1;  /* Simulate error for large requests */
        }
        
        printf("[IO_WORKER] Request #%d completed, result=%d\n", req->id, result);
        
        /* ══════════════════════════════════════════════════════════════
         * THE UPWARD CALLBACK - notify upper layer that I/O is done
         * This is equivalent to bio_endio(bio, error) in Linux kernel
         * Worker doesn't know WHO registered this callback!
         * ══════════════════════════════════════════════════════════════ */
        if (req->complete) {
            printf("[IO_WORKER] Calling completion callback...\n");
            req->complete(req, result);    /* ← UPWARD CALL */
        }
    }
    
    printf("[IO_WORKER] Worker thread exiting\n");
    return NULL;
}

/* Submit request to I/O queue (like submit_bio) */
void submit_io_request(struct io_request *req)
{
    printf("[BLOCK_LAYER] Submitting request #%d to queue\n", req->id);
    
    pthread_mutex_lock(&io_queue.lock);
    io_queue.requests[io_queue.tail] = req;
    io_queue.tail = (io_queue.tail + 1) % 16;
    pthread_cond_signal(&io_queue.cond);
    pthread_mutex_unlock(&io_queue.lock);
}

/*
 * ═══════════════════════════════════════════════════════════════════
 * UPPER LAYER A: Filesystem (like ext4)
 * Submits I/O requests, provides completion callback
 * ═══════════════════════════════════════════════════════════════════
 */

/* Context for filesystem I/O */
struct fs_io_context {
    const char *fs_name;
    int completed;
    int error;
    pthread_mutex_t lock;
    pthread_cond_t cond;
};

/* 
 * Filesystem's completion handler
 * Called by lower layer when I/O completes
 */
void fs_io_complete(struct io_request *req, int error)
{
    struct fs_io_context *ctx = req->private;
    
    /* ══════════════════════════════════════════════════════════════
     * This function is called from the I/O worker thread!
     * The worker doesn't know this is a filesystem callback.
     * ══════════════════════════════════════════════════════════════ */
    
    printf("  [%s] I/O completion callback invoked!\n", ctx->fs_name);
    printf("  [%s] Request #%d finished with error=%d\n", 
           ctx->fs_name, req->id, error);
    
    if (error) {
        printf("  [%s] ERROR: I/O failed, need to handle error!\n", ctx->fs_name);
    } else {
        printf("  [%s] SUCCESS: Data ready, can continue processing\n", ctx->fs_name);
    }
    
    /* Signal completion to waiting thread */
    pthread_mutex_lock(&ctx->lock);
    ctx->completed = 1;
    ctx->error = error;
    pthread_cond_signal(&ctx->cond);
    pthread_mutex_unlock(&ctx->lock);
}

/* Filesystem read function - submits I/O with callback */
void filesystem_read(const char *fs_name, size_t length)
{
    struct fs_io_context ctx = {
        .fs_name = fs_name,
        .completed = 0,
        .error = 0,
        .lock = PTHREAD_MUTEX_INITIALIZER,
        .cond = PTHREAD_COND_INITIALIZER
    };
    
    static int req_id = 0;
    struct io_request *req = malloc(sizeof(*req));
    
    /* Setup request */
    req->id = ++req_id;
    req->buffer = malloc(length);
    req->length = length;
    req->offset = 0;
    req->is_write = 0;
    
    /* ══════════════════════════════════════════════════════════════
     * REGISTER CALLBACK - tell lower layer how to notify us
     * This is like: bio->bi_end_io = ext4_end_bio;
     * ══════════════════════════════════════════════════════════════ */
    req->complete = fs_io_complete;    /* ← Set our callback */
    req->private = &ctx;               /* ← Our context */
    
    printf("\n[%s] Submitting read request #%d (%zu bytes)\n", 
           fs_name, req->id, length);
    
    /* Submit to lower layer */
    submit_io_request(req);
    
    /* Wait for completion */
    pthread_mutex_lock(&ctx.lock);
    while (!ctx.completed) {
        pthread_cond_wait(&ctx.cond, &ctx.lock);
    }
    pthread_mutex_unlock(&ctx.lock);
    
    printf("[%s] Read complete, error=%d\n", fs_name, ctx.error);
    
    free(req->buffer);
    free(req);
}

/*
 * ═══════════════════════════════════════════════════════════════════
 * UPPER LAYER B: Database (another user of block layer)
 * Shows same lower layer can notify different upper layers
 * ═══════════════════════════════════════════════════════════════════
 */

struct db_io_context {
    const char *db_name;
    int completed;
    pthread_mutex_t lock;
    pthread_cond_t cond;
};

/* Database has its OWN completion handler */
void db_io_complete(struct io_request *req, int error)
{
    struct db_io_context *ctx = req->private;
    
    printf("  [%s] Database I/O callback invoked!\n", ctx->db_name);
    printf("  [%s] Request #%d: %s\n", ctx->db_name, req->id,
           error ? "FAILED - will retry" : "SUCCESS - commit possible");
    
    pthread_mutex_lock(&ctx->lock);
    ctx->completed = 1;
    pthread_cond_signal(&ctx->cond);
    pthread_mutex_unlock(&ctx->lock);
}

void database_write(const char *db_name, size_t length)
{
    struct db_io_context ctx = {
        .db_name = db_name,
        .completed = 0,
        .lock = PTHREAD_MUTEX_INITIALIZER,
        .cond = PTHREAD_COND_INITIALIZER
    };
    
    static int req_id = 100;
    struct io_request *req = malloc(sizeof(*req));
    
    req->id = ++req_id;
    req->buffer = malloc(length);
    req->length = length;
    req->is_write = 1;
    
    /* Database uses DIFFERENT callback than filesystem */
    req->complete = db_io_complete;    /* ← Different callback! */
    req->private = &ctx;
    
    printf("\n[%s] Submitting write request #%d (%zu bytes)\n",
           db_name, req->id, length);
    
    submit_io_request(req);
    
    pthread_mutex_lock(&ctx.lock);
    while (!ctx.completed) {
        pthread_cond_wait(&ctx.cond, &ctx.lock);
    }
    pthread_mutex_unlock(&ctx.lock);
    
    printf("[%s] Write complete\n", db_name);
    
    free(req->buffer);
    free(req);
}

/*
 * ═══════════════════════════════════════════════════════════════════
 * MAIN - Demonstrates the pattern
 * ═══════════════════════════════════════════════════════════════════
 */

int main(void)
{
    pthread_t worker;
    
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║  Type 2: Upward Callback (Completion Notification)         ║\n");
    printf("║  Simulating: bio->bi_end_io() pattern                      ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    
    /* Start I/O worker thread (like block layer softirq) */
    pthread_create(&worker, NULL, io_worker_thread, NULL);
    usleep(50000);  /* Let worker start */
    
    /* Filesystem submits I/O - gets notified via fs_io_complete */
    filesystem_read("EXT4_FS", 1024);
    
    /* Database submits I/O - gets notified via db_io_complete */
    database_write("POSTGRES", 2048);
    
    /* Submit a large request - will fail */
    filesystem_read("XFS_FS", 8192);
    
    printf("\n=== KEY INSIGHT ===\n");
    printf("The I/O worker thread called DIFFERENT callbacks for each request.\n");
    printf("It doesn't know if the callback is from filesystem, database, or anything else.\n");
    printf("The boundary is the completion_fn_t callback pointer in the request.\n");
    
    /* Shutdown */
    io_queue.running = 0;
    pthread_cond_signal(&io_queue.cond);
    pthread_join(worker, NULL);
    
    return 0;
}
```

---

### 7.3 Type 3: Registration (Framework Registration)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                 SIMULATION: REGISTRATION                                    │
│                                                                             │
│   ┌──────────────────────────────────────────────────────────────────────┐  │
│   │  Framework Layer (like VFS / Device Core)                            │  │
│   │                                                                      │  │
│   │  /* Global registry - framework maintains this */                    │  │
│   │  static struct driver_entry registry[MAX_DRIVERS];                   │  │
│   │                                                                      │  │
│   │  int register_driver(int id, struct driver_ops *ops) {               │  │
│   │      registry[id].ops = ops;   /* Store driver's ops */              │  │
│   │      return 0;                                                       │  │
│   │  }                                                                   │  │
│   │                                                                      │  │
│   │  int call_driver(int id, ...) {                                      │  │
│   │      return registry[id].ops->operation(...);  /* Route to driver */ │  │
│   │  }                                                                   │  │
│   │                                                                      │  │
│   └────────────────────────────────────────▲─────────────────────────────┘  │
│                                            │                                │
│   ═════════════════════════════════════════│════════════════════════════    │
│                                            │ BOUNDARY: register/unregister  │
│   ═════════════════════════════════════════│════════════════════════════    │
│                                            │                                │
│   ┌────────────────────────────────────────┴─────────────────────────────┐  │
│   │  Driver Layer                                                        │  │
│   │                                                                      │  │
│   │  static struct driver_ops my_ops = { .read = my_read, ... };         │  │
│   │                                                                      │  │
│   │  int init(void) {                                                    │  │
│   │      register_driver(MY_ID, &my_ops);  ◄─── Announce to framework    │  │
│   │  }                                                                   │  │
│   │                                                                      │  │
│   │  void exit(void) {                                                   │  │
│   │      unregister_driver(MY_ID);         ◄─── Remove from framework    │  │
│   │  }                                                                   │  │
│   │                                                                      │  │
│   └──────────────────────────────────────────────────────────────────────┘  │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

**中文说明：**
这个示例模拟了 cdev_add() 和 register_netdev() 的模式。框架（VFS/网络核心）维护一个驱动注册表。驱动在初始化时调用注册函数，将自己的操作表登记到框架中。之后用户空间访问设备时，框架根据设备号/ID 找到对应的驱动并调用其函数。注册和注销必须配对，否则会导致悬挂引用。

```c
/*
 * type3_registration.c - Simulates cdev_add() / register_netdev() pattern
 *
 * This demonstrates how drivers register with frameworks,
 * and how frameworks route operations to registered drivers.
 *
 * Compile: gcc -o type3 type3_registration.c
 * Run:     ./type3
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * ═══════════════════════════════════════════════════════════════════
 * DRIVER OPERATIONS INTERFACE (like file_operations)
 * ═══════════════════════════════════════════════════════════════════
 */

struct device_ops {
    const char *name;
    int (*open)(void *private_data);
    int (*read)(void *private_data, char *buf, size_t len);
    int (*write)(void *private_data, const char *buf, size_t len);
    int (*ioctl)(void *private_data, int cmd, void *arg);
    void (*close)(void *private_data);
};

/*
 * ═══════════════════════════════════════════════════════════════════
 * FRAMEWORK LAYER: Device Registry (like cdev_map in VFS)
 * Maintains registered drivers and routes operations
 * ═══════════════════════════════════════════════════════════════════
 */

#define MAX_DEVICES 256
#define MAJOR_SHIFT 8
#define MAKE_DEV(major, minor) (((major) << MAJOR_SHIFT) | (minor))
#define DEV_MAJOR(dev) ((dev) >> MAJOR_SHIFT)
#define DEV_MINOR(dev) ((dev) & 0xFF)

/* Registry entry - what framework stores about each driver */
struct device_entry {
    int in_use;
    int major;
    int minor;
    const struct device_ops *ops;    /* ← Driver's operations */
    void *driver_data;               /* ← Driver's private data */
};

/* Global registry - like cdev_map */
static struct device_entry device_registry[MAX_DEVICES];
static int registry_count = 0;

/* 
 * Register a device driver - like cdev_add() or register_chrdev()
 * Driver calls this to announce itself to the framework
 */
int register_device(int major, int minor, const struct device_ops *ops, void *data)
{
    int dev_id = MAKE_DEV(major, minor);
    
    if (dev_id >= MAX_DEVICES) {
        fprintf(stderr, "register_device: invalid device number\n");
        return -1;
    }
    
    if (device_registry[dev_id].in_use) {
        fprintf(stderr, "register_device: device %d:%d already registered\n",
                major, minor);
        return -1;
    }
    
    /* ══════════════════════════════════════════════════════════════
     * REGISTRATION - Store driver's ops in the framework's registry
     * This is equivalent to kobj_map() in Linux kernel
     * After this, framework can route operations to this driver
     * ══════════════════════════════════════════════════════════════ */
    device_registry[dev_id].in_use = 1;
    device_registry[dev_id].major = major;
    device_registry[dev_id].minor = minor;
    device_registry[dev_id].ops = ops;      /* ← Save operations table */
    device_registry[dev_id].driver_data = data;
    registry_count++;
    
    printf("[FRAMEWORK] Registered device %d:%d (%s)\n", 
           major, minor, ops->name);
    return 0;
}

/* Unregister a device - like cdev_del() */
int unregister_device(int major, int minor)
{
    int dev_id = MAKE_DEV(major, minor);
    
    if (!device_registry[dev_id].in_use) {
        return -1;
    }
    
    printf("[FRAMEWORK] Unregistered device %d:%d (%s)\n",
           major, minor, device_registry[dev_id].ops->name);
    
    device_registry[dev_id].in_use = 0;
    device_registry[dev_id].ops = NULL;
    registry_count--;
    return 0;
}

/* 
 * Framework operation: open device
 * Routes to registered driver - like chrdev_open()
 */
int device_open(int major, int minor)
{
    int dev_id = MAKE_DEV(major, minor);
    
    if (!device_registry[dev_id].in_use) {
        fprintf(stderr, "[FRAMEWORK] No driver for device %d:%d\n", major, minor);
        return -1;
    }
    
    /* ══════════════════════════════════════════════════════════════
     * LOOKUP & DISPATCH - Find registered driver and call its open()
     * Framework doesn't know what driver does, just routes the call
     * ══════════════════════════════════════════════════════════════ */
    struct device_entry *entry = &device_registry[dev_id];
    printf("[FRAMEWORK] Opening device %d:%d, routing to '%s' driver\n",
           major, minor, entry->ops->name);
    
    if (entry->ops->open) {
        return entry->ops->open(entry->driver_data);
    }
    return 0;
}

/* Framework operation: read from device */
int device_read(int major, int minor, char *buf, size_t len)
{
    int dev_id = MAKE_DEV(major, minor);
    struct device_entry *entry = &device_registry[dev_id];
    
    if (!entry->in_use || !entry->ops->read)
        return -1;
    
    printf("[FRAMEWORK] Read from %d:%d, routing to '%s'\n",
           major, minor, entry->ops->name);
    return entry->ops->read(entry->driver_data, buf, len);
}

/* Framework operation: write to device */
int device_write(int major, int minor, const char *buf, size_t len)
{
    int dev_id = MAKE_DEV(major, minor);
    struct device_entry *entry = &device_registry[dev_id];
    
    if (!entry->in_use || !entry->ops->write)
        return -1;
    
    printf("[FRAMEWORK] Write to %d:%d, routing to '%s'\n",
           major, minor, entry->ops->name);
    return entry->ops->write(entry->driver_data, buf, len);
}

/* List all registered devices */
void list_devices(void)
{
    printf("\n[FRAMEWORK] Registered devices:\n");
    for (int i = 0; i < MAX_DEVICES; i++) {
        if (device_registry[i].in_use) {
            printf("  %d:%d - %s\n",
                   device_registry[i].major,
                   device_registry[i].minor,
                   device_registry[i].ops->name);
        }
    }
}

/*
 * ═══════════════════════════════════════════════════════════════════
 * DRIVER A: Null Device (like /dev/null)
 * ═══════════════════════════════════════════════════════════════════
 */

static int null_open(void *data) 
{
    printf("  [null_driver] opened\n");
    return 0;
}

static int null_read(void *data, char *buf, size_t len)
{
    printf("  [null_driver] read returns 0 (EOF)\n");
    return 0;  /* Always EOF */
}

static int null_write(void *data, const char *buf, size_t len)
{
    printf("  [null_driver] write discards %zu bytes\n", len);
    return len;  /* Accept all, discard */
}

static void null_close(void *data)
{
    printf("  [null_driver] closed\n");
}

/* Null device operations table */
static const struct device_ops null_ops = {
    .name  = "null",
    .open  = null_open,
    .read  = null_read,
    .write = null_write,
    .close = null_close,
};

/* Driver init function - like module_init() */
int null_driver_init(void)
{
    printf("[null_driver] Initializing...\n");
    /* Register with major=1, minor=3 (like real /dev/null) */
    return register_device(1, 3, &null_ops, NULL);
}

void null_driver_exit(void)
{
    printf("[null_driver] Exiting...\n");
    unregister_device(1, 3);
}

/*
 * ═══════════════════════════════════════════════════════════════════
 * DRIVER B: Zero Device (like /dev/zero)
 * ═══════════════════════════════════════════════════════════════════
 */

static int zero_read(void *data, char *buf, size_t len)
{
    printf("  [zero_driver] filling %zu bytes with zeros\n", len);
    memset(buf, 0, len);
    return len;
}

static int zero_write(void *data, const char *buf, size_t len)
{
    printf("  [zero_driver] discarding %zu bytes\n", len);
    return len;
}

static const struct device_ops zero_ops = {
    .name  = "zero",
    .open  = null_open,
    .read  = zero_read,
    .write = zero_write,
    .close = null_close,
};

int zero_driver_init(void)
{
    printf("[zero_driver] Initializing...\n");
    return register_device(1, 5, &zero_ops, NULL);
}

void zero_driver_exit(void)
{
    printf("[zero_driver] Exiting...\n");
    unregister_device(1, 5);
}

/*
 * ═══════════════════════════════════════════════════════════════════
 * DRIVER C: Memory Device (like ramdisk)
 * ═══════════════════════════════════════════════════════════════════
 */

struct mem_device_data {
    char buffer[1024];
    size_t data_len;
};

static int mem_open(void *data)
{
    printf("  [mem_driver] opened\n");
    return 0;
}

static int mem_read(void *data, char *buf, size_t len)
{
    struct mem_device_data *mem = data;
    if (len > mem->data_len)
        len = mem->data_len;
    printf("  [mem_driver] reading %zu bytes from buffer\n", len);
    memcpy(buf, mem->buffer, len);
    return len;
}

static int mem_write(void *data, const char *buf, size_t len)
{
    struct mem_device_data *mem = data;
    if (len > sizeof(mem->buffer))
        len = sizeof(mem->buffer);
    printf("  [mem_driver] writing %zu bytes to buffer\n", len);
    memcpy(mem->buffer, buf, len);
    mem->data_len = len;
    return len;
}

static const struct device_ops mem_ops = {
    .name  = "mem",
    .open  = mem_open,
    .read  = mem_read,
    .write = mem_write,
    .close = null_close,
};

static struct mem_device_data mem_data;

int mem_driver_init(void)
{
    printf("[mem_driver] Initializing...\n");
    memset(&mem_data, 0, sizeof(mem_data));
    return register_device(1, 1, &mem_ops, &mem_data);
}

void mem_driver_exit(void)
{
    printf("[mem_driver] Exiting...\n");
    unregister_device(1, 1);
}

/*
 * ═══════════════════════════════════════════════════════════════════
 * USER APPLICATION - Uses framework to access devices
 * ═══════════════════════════════════════════════════════════════════
 */

int main(void)
{
    char buf[64];
    
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║  Type 3: Registration (Framework Registration)             ║\n");
    printf("║  Simulating: cdev_add() / register_chrdev() pattern        ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n\n");
    
    printf("=== PHASE 1: Driver Registration ===\n");
    /* Drivers register themselves (like module init) */
    null_driver_init();
    zero_driver_init();
    mem_driver_init();
    
    list_devices();
    
    printf("\n=== PHASE 2: Using Devices via Framework ===\n");
    
    /* Use null device (1:3) */
    printf("\n--- Accessing /dev/null (1:3) ---\n");
    device_open(1, 3);
    device_write(1, 3, "Hello", 5);
    int n = device_read(1, 3, buf, sizeof(buf));
    printf("[USER] Read returned %d bytes\n", n);
    
    /* Use zero device (1:5) */
    printf("\n--- Accessing /dev/zero (1:5) ---\n");
    device_open(1, 5);
    n = device_read(1, 5, buf, 10);
    printf("[USER] Read %d bytes, first byte = %d\n", n, buf[0]);
    
    /* Use memory device (1:1) */
    printf("\n--- Accessing /dev/mem (1:1) ---\n");
    device_open(1, 1);
    device_write(1, 1, "Stored Data!", 12);
    n = device_read(1, 1, buf, sizeof(buf));
    buf[n] = '\0';
    printf("[USER] Read %d bytes: \"%s\"\n", n, buf);
    
    /* Try unregistered device */
    printf("\n--- Accessing unregistered device (2:0) ---\n");
    device_open(2, 0);  /* Should fail */
    
    printf("\n=== PHASE 3: Driver Unregistration ===\n");
    /* Drivers unregister (like module exit) */
    mem_driver_exit();
    
    printf("\n--- Try to access unregistered mem device ---\n");
    n = device_read(1, 1, buf, sizeof(buf));  /* Should fail now */
    printf("[USER] Read returned %d\n", n);
    
    list_devices();
    
    /* Cleanup remaining drivers */
    null_driver_exit();
    zero_driver_exit();
    
    printf("\n=== KEY INSIGHT ===\n");
    printf("Drivers register themselves with the framework at init time.\n");
    printf("Framework routes operations to registered drivers by device number.\n");
    printf("The boundary is the register/unregister API.\n");
    printf("Framework doesn't care what drivers do, just maintains the registry.\n");
    
    return 0;
}
```

---

### 7.4 Type 4: Service Call (Shared Utilities)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                 SIMULATION: SERVICE CALL                                    │
│                                                                             │
│                         ┌─────────────────────────────┐                     │
│                         │    SHARED SERVICES          │                     │
│                         │                             │                     │
│                         │  • Memory: alloc/free       │                     │
│                         │  • Logging: log_msg()       │                     │
│                         │  • Locking: lock/unlock     │                     │
│                         │  • Time: get_timestamp()    │                     │
│                         └──────────────┬──────────────┘                     │
│                                        │                                    │
│               ┌────────────────────────┼────────────────────────┐           │
│               │                        │                        │           │
│               ▼                        ▼                        ▼           │
│   ┌───────────────────┐    ┌───────────────────┐    ┌───────────────────┐   │
│   │   Module A        │    │   Module B        │    │   Module C        │   │
│   │   (Filesystem)    │    │   (Network)       │    │   (Driver)        │   │
│   │                   │    │                   │    │                   │   │
│   │  alloc_memory()   │    │  alloc_memory()   │    │  alloc_memory()   │   │
│   │  log_msg()        │    │  log_msg()        │    │  acquire_lock()   │   │
│   │  acquire_lock()   │    │  get_timestamp()  │    │  log_msg()        │   │
│   └───────────────────┘    └───────────────────┘    └───────────────────┘   │
│                                                                             │
│   Services are ORTHOGONAL to layers - any module can use them               │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

**中文说明：**
服务调用是横向的，不遵循层级关系。内核提供共享服务（内存管理 kmalloc、日志 printk、同步原语 spin_lock 等），任何模块都可以使用。这些服务构成了系统的"基础设施"。关键点是：服务调用不属于任何特定层级，它们是正交的工具函数。

```c
/*
 * type4_service_call.c - Simulates kmalloc() / spin_lock() / printk() pattern
 *
 * This demonstrates shared services that any layer can use.
 * Services are orthogonal to the layer hierarchy.
 *
 * Compile: gcc -o type4 type4_service_call.c -lpthread
 * Run:     ./type4
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

/*
 * ═══════════════════════════════════════════════════════════════════
 * SERVICE 1: Memory Allocation (like kmalloc/kfree)
 * Can be called from ANY module/layer
 * ═══════════════════════════════════════════════════════════════════
 */

/* Track allocations for debugging */
static struct {
    size_t total_allocated;
    size_t allocation_count;
    pthread_mutex_t lock;
} mem_stats = { .lock = PTHREAD_MUTEX_INITIALIZER };

/* 
 * Allocate memory - like kmalloc()
 * Any layer can call this service
 */
void *service_alloc(size_t size, const char *caller)
{
    void *ptr = malloc(size);
    
    if (ptr) {
        pthread_mutex_lock(&mem_stats.lock);
        mem_stats.total_allocated += size;
        mem_stats.allocation_count++;
        pthread_mutex_unlock(&mem_stats.lock);
        
        printf("[MEM_SERVICE] %s allocated %zu bytes at %p\n", caller, size, ptr);
    }
    return ptr;
}

/* Free memory - like kfree() */
void service_free(void *ptr, const char *caller)
{
    if (ptr) {
        pthread_mutex_lock(&mem_stats.lock);
        mem_stats.allocation_count--;
        pthread_mutex_unlock(&mem_stats.lock);
        
        printf("[MEM_SERVICE] %s freed memory at %p\n", caller, ptr);
        free(ptr);
    }
}

void service_mem_stats(void)
{
    pthread_mutex_lock(&mem_stats.lock);
    printf("[MEM_SERVICE] Stats: %zu allocations, %zu bytes total\n",
           mem_stats.allocation_count, mem_stats.total_allocated);
    pthread_mutex_unlock(&mem_stats.lock);
}

/*
 * ═══════════════════════════════════════════════════════════════════
 * SERVICE 2: Logging (like printk)
 * Centralized logging used by all modules
 * ═══════════════════════════════════════════════════════════════════
 */

typedef enum {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR
} log_level_t;

static const char *level_str[] = { "DEBUG", "INFO", "WARN", "ERROR" };
static log_level_t current_log_level = LOG_INFO;
static pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;

/* 
 * Log message - like printk()
 * Any module can use this for logging
 */
void service_log(log_level_t level, const char *module, const char *fmt, ...)
{
    if (level < current_log_level)
        return;
    
    pthread_mutex_lock(&log_lock);
    
    /* Get timestamp */
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_buf[20];
    strftime(time_buf, sizeof(time_buf), "%H:%M:%S", tm_info);
    
    /* Print log message */
    printf("[%s] [%5s] [%s] ", time_buf, level_str[level], module);
    
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    
    printf("\n");
    
    pthread_mutex_unlock(&log_lock);
}

#define LOG_DBG(module, fmt, ...) service_log(LOG_DEBUG, module, fmt, ##__VA_ARGS__)
#define LOG_INF(module, fmt, ...) service_log(LOG_INFO, module, fmt, ##__VA_ARGS__)
#define LOG_WRN(module, fmt, ...) service_log(LOG_WARNING, module, fmt, ##__VA_ARGS__)
#define LOG_ERR(module, fmt, ...) service_log(LOG_ERROR, module, fmt, ##__VA_ARGS__)

/*
 * ═══════════════════════════════════════════════════════════════════
 * SERVICE 3: Locking (like spin_lock/mutex)
 * Synchronization primitives for all modules
 * ═══════════════════════════════════════════════════════════════════
 */

typedef struct {
    pthread_mutex_t mutex;
    const char *name;
    int held;
    const char *holder;
} service_lock_t;

/* Initialize a lock - like spin_lock_init() */
void service_lock_init(service_lock_t *lock, const char *name)
{
    pthread_mutex_init(&lock->mutex, NULL);
    lock->name = name;
    lock->held = 0;
    lock->holder = NULL;
    printf("[LOCK_SERVICE] Initialized lock '%s'\n", name);
}

/* Acquire lock - like spin_lock() */
void service_lock_acquire(service_lock_t *lock, const char *caller)
{
    printf("[LOCK_SERVICE] %s acquiring '%s'...\n", caller, lock->name);
    pthread_mutex_lock(&lock->mutex);
    lock->held = 1;
    lock->holder = caller;
    printf("[LOCK_SERVICE] %s acquired '%s'\n", caller, lock->name);
}

/* Release lock - like spin_unlock() */
void service_lock_release(service_lock_t *lock, const char *caller)
{
    printf("[LOCK_SERVICE] %s releasing '%s'\n", caller, lock->name);
    lock->held = 0;
    lock->holder = NULL;
    pthread_mutex_unlock(&lock->mutex);
}

/*
 * ═══════════════════════════════════════════════════════════════════
 * SERVICE 4: Time (like jiffies / ktime)
 * Time-related functions for all modules
 * ═══════════════════════════════════════════════════════════════════
 */

/* Get current time in milliseconds - like jiffies */
unsigned long service_get_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/* Sleep for milliseconds - like msleep() */
void service_sleep_ms(unsigned int ms, const char *caller)
{
    printf("[TIME_SERVICE] %s sleeping for %u ms\n", caller, ms);
    usleep(ms * 1000);
}

/*
 * ═══════════════════════════════════════════════════════════════════
 * MODULE A: Filesystem Module
 * Uses various services to implement functionality
 * ═══════════════════════════════════════════════════════════════════
 */

static service_lock_t fs_lock;

struct fs_buffer {
    char *data;
    size_t size;
};

struct fs_buffer *fs_alloc_buffer(size_t size)
{
    /* ══════════════════════════════════════════════════════════════
     * SERVICE CALLS - Filesystem uses shared services
     * These are orthogonal to the layer hierarchy
     * ══════════════════════════════════════════════════════════════ */
    
    LOG_INF("FS", "Allocating buffer of size %zu", size);
    
    struct fs_buffer *buf = service_alloc(sizeof(*buf), "FS");
    if (!buf) {
        LOG_ERR("FS", "Failed to allocate buffer struct");
        return NULL;
    }
    
    buf->data = service_alloc(size, "FS");
    if (!buf->data) {
        LOG_ERR("FS", "Failed to allocate buffer data");
        service_free(buf, "FS");
        return NULL;
    }
    
    buf->size = size;
    return buf;
}

void fs_free_buffer(struct fs_buffer *buf)
{
    LOG_INF("FS", "Freeing buffer");
    service_free(buf->data, "FS");
    service_free(buf, "FS");
}

void fs_write_data(struct fs_buffer *buf, const char *data)
{
    service_lock_acquire(&fs_lock, "FS");
    
    LOG_INF("FS", "Writing data to buffer");
    strncpy(buf->data, data, buf->size - 1);
    
    service_lock_release(&fs_lock, "FS");
}

/*
 * ═══════════════════════════════════════════════════════════════════
 * MODULE B: Network Module
 * Also uses the same services
 * ═══════════════════════════════════════════════════════════════════
 */

static service_lock_t net_lock;

struct net_packet {
    char *payload;
    size_t length;
    unsigned long timestamp;
};

struct net_packet *net_create_packet(size_t size)
{
    /* Same services, different module */
    LOG_INF("NET", "Creating packet of size %zu", size);
    
    struct net_packet *pkt = service_alloc(sizeof(*pkt), "NET");
    pkt->payload = service_alloc(size, "NET");
    pkt->length = size;
    pkt->timestamp = service_get_time_ms();  /* Use time service */
    
    return pkt;
}

void net_send_packet(struct net_packet *pkt)
{
    service_lock_acquire(&net_lock, "NET");
    
    LOG_INF("NET", "Sending packet (timestamp=%lu)", pkt->timestamp);
    
    /* Simulate network delay */
    service_sleep_ms(50, "NET");
    
    unsigned long now = service_get_time_ms();
    LOG_INF("NET", "Packet sent (latency=%lu ms)", now - pkt->timestamp);
    
    service_lock_release(&net_lock, "NET");
}

void net_free_packet(struct net_packet *pkt)
{
    LOG_INF("NET", "Freeing packet");
    service_free(pkt->payload, "NET");
    service_free(pkt, "NET");
}

/*
 * ═══════════════════════════════════════════════════════════════════
 * MODULE C: Driver Module
 * Yet another user of the same services
 * ═══════════════════════════════════════════════════════════════════
 */

static service_lock_t drv_lock;

void driver_do_io(void)
{
    LOG_INF("DRV", "Starting I/O operation");
    
    service_lock_acquire(&drv_lock, "DRV");
    
    void *dma_buf = service_alloc(4096, "DRV");
    
    unsigned long start = service_get_time_ms();
    service_sleep_ms(100, "DRV");  /* Simulate I/O */
    unsigned long elapsed = service_get_time_ms() - start;
    
    LOG_INF("DRV", "I/O completed in %lu ms", elapsed);
    
    service_free(dma_buf, "DRV");
    
    service_lock_release(&drv_lock, "DRV");
}

/*
 * ═══════════════════════════════════════════════════════════════════
 * MAIN - Demonstrates services used across modules
 * ═══════════════════════════════════════════════════════════════════
 */

int main(void)
{
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║  Type 4: Service Call (Shared Utilities)                   ║\n");
    printf("║  Simulating: kmalloc() / printk() / spin_lock() pattern    ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n\n");
    
    /* Initialize locks */
    service_lock_init(&fs_lock, "fs_lock");
    service_lock_init(&net_lock, "net_lock");
    service_lock_init(&drv_lock, "drv_lock");
    
    printf("\n=== Filesystem Module Using Services ===\n");
    struct fs_buffer *fs_buf = fs_alloc_buffer(256);
    fs_write_data(fs_buf, "Hello from filesystem!");
    
    printf("\n=== Network Module Using Services ===\n");
    struct net_packet *pkt = net_create_packet(1500);
    net_send_packet(pkt);
    
    printf("\n=== Driver Module Using Services ===\n");
    driver_do_io();
    
    printf("\n=== Memory Statistics ===\n");
    service_mem_stats();
    
    printf("\n=== Cleanup ===\n");
    fs_free_buffer(fs_buf);
    net_free_packet(pkt);
    
    service_mem_stats();
    
    printf("\n=== KEY INSIGHT ===\n");
    printf("All three modules (FS, NET, DRV) use the SAME services:\n");
    printf("  - Memory allocation (service_alloc/service_free)\n");
    printf("  - Logging (service_log / LOG_* macros)\n");
    printf("  - Locking (service_lock_acquire/release)\n");
    printf("  - Time (service_get_time_ms/service_sleep_ms)\n");
    printf("\nServices are ORTHOGONAL to layers - they're shared infrastructure.\n");
    printf("Any module can call any service without crossing layer boundaries.\n");
    
    return 0;
}
```

---

### 7.5 Type 5: Event Notification (Notifier Chain)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                 SIMULATION: EVENT NOTIFICATION                              │
│                                                                             │
│   ┌──────────────────────────────────────────────────────────────────────┐  │
│   │  Event Publisher (doesn't know subscribers)                          │  │
│   │                                                                      │  │
│   │  /* Notify all subscribers about event */                            │  │
│   │  void notify_event(int event_type, void *data) {                     │  │
│   │      for each subscriber in chain:                                   │  │
│   │          subscriber->callback(event_type, data);                     │  │
│   │  }                                                                   │  │
│   │                                                                      │  │
│   └────────────────────────────────────┬─────────────────────────────────┘  │
│                                        │                                    │
│                                        │  event notification                │
│                                        │                                    │
│          ┌─────────────────────────────┼─────────────────────────────┐      │
│          │                             │                             │      │
│          ▼                             ▼                             ▼      │
│   ┌─────────────┐              ┌─────────────┐              ┌─────────────┐ │
│   │ Subscriber A│──────────────│ Subscriber B│──────────────│ Subscriber C│ │
│   │ (registered │              │ (registered │              │ (registered │ │
│   │  callback)  │              │  callback)  │              │  callback)  │ │
│   └─────────────┘              └─────────────┘              └─────────────┘ │
│                                                                             │
│   Publisher and subscribers are COMPLETELY DECOUPLED                        │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

**中文说明：**
这个示例模拟了 notifier_call_chain 模式。事件发布者维护一个订阅者链表，当事件发生时遍历链表调用每个订阅者的回调。发布者完全不知道有哪些订阅者，订阅者也不知道彼此。这实现了完全的解耦，让独立开发的模块可以响应系统事件（如网络设备状态变化、系统重启通知等）。

```c
/*
 * type5_event_notification.c - Simulates notifier_call_chain() pattern
 *
 * This demonstrates the publish-subscribe pattern where
 * publishers and subscribers are completely decoupled.
 *
 * Compile: gcc -o type5 type5_event_notification.c -lpthread
 * Run:     ./type5
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

/*
 * ═══════════════════════════════════════════════════════════════════
 * EVENT TYPES (like network device events)
 * ═══════════════════════════════════════════════════════════════════
 */

typedef enum {
    EVENT_DEVICE_REGISTER,      /* Device added to system */
    EVENT_DEVICE_UNREGISTER,    /* Device removed from system */
    EVENT_DEVICE_UP,            /* Device brought up */
    EVENT_DEVICE_DOWN,          /* Device brought down */
    EVENT_CONFIG_CHANGE,        /* Configuration changed */
    EVENT_SHUTDOWN,             /* System shutting down */
} event_type_t;

static const char *event_names[] = {
    "DEVICE_REGISTER",
    "DEVICE_UNREGISTER", 
    "DEVICE_UP",
    "DEVICE_DOWN",
    "CONFIG_CHANGE",
    "SHUTDOWN"
};

/*
 * ═══════════════════════════════════════════════════════════════════
 * NOTIFIER BLOCK (like struct notifier_block)
 * Each subscriber uses this to register with the notifier chain
 * ═══════════════════════════════════════════════════════════════════
 */

/* Return values from notifier callbacks */
#define NOTIFY_OK       0    /* Continue calling other subscribers */
#define NOTIFY_DONE     1    /* Don't call remaining subscribers */
#define NOTIFY_STOP     2    /* Stop chain, event handled */

struct notifier_block;  /* Forward declaration */

/* Callback function type */
typedef int (*notifier_fn_t)(struct notifier_block *nb, 
                             event_type_t event, 
                             void *data);

/* Notifier block - subscriber registers this with the chain */
struct notifier_block {
    notifier_fn_t notifier_call;      /* ← Callback function */
    struct notifier_block *next;       /* ← Next in chain */
    int priority;                      /* ← Higher = called first */
    const char *name;                  /* ← For debugging */
};

/*
 * ═══════════════════════════════════════════════════════════════════
 * NOTIFIER CHAIN (like struct notifier_head)
 * Publisher maintains this, subscribers register with it
 * ═══════════════════════════════════════════════════════════════════
 */

struct notifier_chain {
    struct notifier_block *head;
    pthread_mutex_t lock;
    const char *name;
};

/* Initialize a notifier chain */
void notifier_chain_init(struct notifier_chain *chain, const char *name)
{
    chain->head = NULL;
    pthread_mutex_init(&chain->lock, NULL);
    chain->name = name;
    printf("[NOTIFIER] Initialized chain '%s'\n", name);
}

/* 
 * Register a subscriber - like register_netdevice_notifier()
 * Subscriber calls this to receive events
 */
int notifier_chain_register(struct notifier_chain *chain,
                            struct notifier_block *nb)
{
    pthread_mutex_lock(&chain->lock);
    
    /* Insert sorted by priority (higher priority = earlier in list) */
    struct notifier_block **p = &chain->head;
    while (*p && (*p)->priority >= nb->priority) {
        p = &(*p)->next;
    }
    nb->next = *p;
    *p = nb;
    
    pthread_mutex_unlock(&chain->lock);
    
    printf("[NOTIFIER] '%s' registered with chain '%s' (priority=%d)\n",
           nb->name, chain->name, nb->priority);
    return 0;
}

/*
 * Unregister a subscriber - like unregister_netdevice_notifier()
 */
int notifier_chain_unregister(struct notifier_chain *chain,
                              struct notifier_block *nb)
{
    pthread_mutex_lock(&chain->lock);
    
    struct notifier_block **p = &chain->head;
    while (*p) {
        if (*p == nb) {
            *p = nb->next;
            break;
        }
        p = &(*p)->next;
    }
    
    pthread_mutex_unlock(&chain->lock);
    
    printf("[NOTIFIER] '%s' unregistered from chain '%s'\n",
           nb->name, chain->name);
    return 0;
}

/*
 * Call all subscribers - like notifier_call_chain()
 * Publisher calls this when event occurs
 */
int notifier_call_chain(struct notifier_chain *chain,
                        event_type_t event,
                        void *data)
{
    int ret = NOTIFY_OK;
    
    printf("\n[NOTIFIER] === Event %s on chain '%s' ===\n",
           event_names[event], chain->name);
    
    pthread_mutex_lock(&chain->lock);
    
    /* ══════════════════════════════════════════════════════════════
     * NOTIFY ALL SUBSCRIBERS
     * Publisher doesn't know who these subscribers are!
     * It just walks the chain and calls each callback.
     * ══════════════════════════════════════════════════════════════ */
    struct notifier_block *nb = chain->head;
    while (nb) {
        printf("[NOTIFIER] Calling subscriber '%s'...\n", nb->name);
        
        ret = nb->notifier_call(nb, event, data);    /* ← Call subscriber */
        
        if (ret == NOTIFY_STOP) {
            printf("[NOTIFIER] '%s' returned STOP, halting chain\n", nb->name);
            break;
        }
        
        nb = nb->next;
    }
    
    pthread_mutex_unlock(&chain->lock);
    
    printf("[NOTIFIER] === Event notification complete ===\n");
    return ret;
}

/*
 * ═══════════════════════════════════════════════════════════════════
 * GLOBAL NOTIFIER CHAINS (like netdev_chain, reboot_notifier_list)
 * ═══════════════════════════════════════════════════════════════════
 */

/* Device event notification chain */
static struct notifier_chain device_notifier;

/* System event notification chain */
static struct notifier_chain system_notifier;

/*
 * ═══════════════════════════════════════════════════════════════════
 * SUBSCRIBER A: Logging Module
 * Logs all events it receives
 * ═══════════════════════════════════════════════════════════════════
 */

static int logging_notifier_callback(struct notifier_block *nb,
                                     event_type_t event,
                                     void *data)
{
    printf("  [LOGGING] Received event: %s", event_names[event]);
    if (data)
        printf(" (data: %s)", (char *)data);
    printf("\n");
    printf("  [LOGGING] Event logged to /var/log/events.log\n");
    
    return NOTIFY_OK;  /* Continue to next subscriber */
}

static struct notifier_block logging_nb = {
    .notifier_call = logging_notifier_callback,
    .priority = 0,    /* Low priority - log after others process */
    .name = "logging"
};

/*
 * ═══════════════════════════════════════════════════════════════════
 * SUBSCRIBER B: Network Manager
 * Reacts to device events
 * ═══════════════════════════════════════════════════════════════════
 */

static int netmgr_notifier_callback(struct notifier_block *nb,
                                    event_type_t event,
                                    void *data)
{
    printf("  [NETMGR] Received event: %s\n", event_names[event]);
    
    switch (event) {
    case EVENT_DEVICE_UP:
        printf("  [NETMGR] Configuring routes for device\n");
        printf("  [NETMGR] Starting DHCP client\n");
        break;
    case EVENT_DEVICE_DOWN:
        printf("  [NETMGR] Removing routes for device\n");
        printf("  [NETMGR] Stopping DHCP client\n");
        break;
    case EVENT_DEVICE_REGISTER:
        printf("  [NETMGR] New device detected, scanning...\n");
        break;
    default:
        break;
    }
    
    return NOTIFY_OK;
}

static struct notifier_block netmgr_nb = {
    .notifier_call = netmgr_notifier_callback,
    .priority = 10,    /* Higher priority - process before logging */
    .name = "netmgr"
};

/*
 * ═══════════════════════════════════════════════════════════════════
 * SUBSCRIBER C: Firewall
 * Applies security rules when devices change
 * ═══════════════════════════════════════════════════════════════════
 */

static int firewall_notifier_callback(struct notifier_block *nb,
                                      event_type_t event,
                                      void *data)
{
    printf("  [FIREWALL] Received event: %s\n", event_names[event]);
    
    switch (event) {
    case EVENT_DEVICE_UP:
        printf("  [FIREWALL] Applying firewall rules to device\n");
        printf("  [FIREWALL] Enabling packet filtering\n");
        break;
    case EVENT_DEVICE_DOWN:
        printf("  [FIREWALL] Removing firewall rules for device\n");
        break;
    case EVENT_SHUTDOWN:
        printf("  [FIREWALL] Saving firewall state before shutdown\n");
        return NOTIFY_STOP;  /* Don't let others process shutdown */
    default:
        break;
    }
    
    return NOTIFY_OK;
}

static struct notifier_block firewall_nb = {
    .notifier_call = firewall_notifier_callback,
    .priority = 20,    /* Highest priority - security first */
    .name = "firewall"
};

/*
 * ═══════════════════════════════════════════════════════════════════
 * SUBSCRIBER D: Power Manager (for system events)
 * ═══════════════════════════════════════════════════════════════════
 */

static int powermgr_notifier_callback(struct notifier_block *nb,
                                      event_type_t event,
                                      void *data)
{
    printf("  [POWERMGR] Received event: %s\n", event_names[event]);
    
    if (event == EVENT_SHUTDOWN) {
        printf("  [POWERMGR] Saving state to disk...\n");
        printf("  [POWERMGR] Notifying hardware to prepare for shutdown...\n");
    }
    
    return NOTIFY_OK;
}

static struct notifier_block powermgr_nb = {
    .notifier_call = powermgr_notifier_callback,
    .priority = 100,
    .name = "powermgr"
};

/*
 * ═══════════════════════════════════════════════════════════════════
 * PUBLISHER: Device Subsystem
 * Publishes events without knowing who's listening
 * ═══════════════════════════════════════════════════════════════════
 */

void device_register(const char *name)
{
    printf("\n>>> DEVICE '%s' BEING REGISTERED <<<\n", name);
    
    /* ══════════════════════════════════════════════════════════════
     * PUBLISH EVENT - Publisher doesn't know subscribers!
     * This is like call_netdevice_notifiers() in Linux kernel
     * ══════════════════════════════════════════════════════════════ */
    notifier_call_chain(&device_notifier, EVENT_DEVICE_REGISTER, (void *)name);
}

void device_up(const char *name)
{
    printf("\n>>> DEVICE '%s' GOING UP <<<\n", name);
    notifier_call_chain(&device_notifier, EVENT_DEVICE_UP, (void *)name);
}

void device_down(const char *name)
{
    printf("\n>>> DEVICE '%s' GOING DOWN <<<\n", name);
    notifier_call_chain(&device_notifier, EVENT_DEVICE_DOWN, (void *)name);
}

void system_shutdown(void)
{
    printf("\n>>> SYSTEM SHUTDOWN INITIATED <<<\n");
    notifier_call_chain(&system_notifier, EVENT_SHUTDOWN, NULL);
}

/*
 * ═══════════════════════════════════════════════════════════════════
 * MAIN - Demonstrates the notification pattern
 * ═══════════════════════════════════════════════════════════════════
 */

int main(void)
{
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║  Type 5: Event Notification (Notifier Chain)               ║\n");
    printf("║  Simulating: notifier_call_chain() pattern                 ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n\n");
    
    /* Initialize notification chains */
    printf("=== Initializing Notifier Chains ===\n");
    notifier_chain_init(&device_notifier, "device_chain");
    notifier_chain_init(&system_notifier, "system_chain");
    
    /* Register subscribers */
    printf("\n=== Registering Subscribers ===\n");
    printf("(Note: Higher priority subscribers are called first)\n\n");
    
    /* Device event subscribers */
    notifier_chain_register(&device_notifier, &logging_nb);
    notifier_chain_register(&device_notifier, &netmgr_nb);
    notifier_chain_register(&device_notifier, &firewall_nb);
    
    /* System event subscribers */
    notifier_chain_register(&system_notifier, &logging_nb);
    notifier_chain_register(&system_notifier, &powermgr_nb);
    notifier_chain_register(&system_notifier, &firewall_nb);
    
    /* Simulate device lifecycle */
    printf("\n=== Simulating Device Lifecycle ===\n");
    
    device_register("eth0");
    device_up("eth0");
    device_down("eth0");
    
    /* Unregister one subscriber */
    printf("\n=== Unregistering Network Manager ===\n");
    notifier_chain_unregister(&device_notifier, &netmgr_nb);
    
    /* Another device - netmgr won't be notified */
    device_up("wlan0");
    
    /* System shutdown */
    printf("\n=== System Shutdown ===\n");
    system_shutdown();
    
    printf("\n=== KEY INSIGHT ===\n");
    printf("The device subsystem (publisher) doesn't know:\n");
    printf("  - How many subscribers there are\n");
    printf("  - Who the subscribers are\n");
    printf("  - What subscribers do with the events\n");
    printf("\nSubscribers don't know:\n");
    printf("  - Who else is subscribed\n");
    printf("  - When events will be published\n");
    printf("\nThis is COMPLETE DECOUPLING via the notifier chain pattern.\n");
    printf("The boundary is the notifier_chain register/unregister API.\n");
    
    return 0;
}
```

---

### 7.6 Summary: Interaction Types Comparison

```
╔════════════════════════════════════════════════════════════════════════════════╗
║               COMPARISON OF ALL FIVE INTERACTION TYPES                         ║
╠═══════════════╦═══════════════════════╦═══════════════════╦════════════════════╣
║ Type          ║ Direction             ║ Coupling          ║ When to Use        ║
╠═══════════════╬═══════════════════════╬═══════════════════╬════════════════════╣
║               ║                       ║                   ║                    ║
║ 1. Downward   ║ Upper → Lower         ║ Loose             ║ Normal operations  ║
║    Call       ║ via function pointer  ║ (through ops)     ║ read/write/ioctl   ║
║               ║                       ║                   ║                    ║
╠═══════════════╬═══════════════════════╬═══════════════════╬════════════════════╣
║               ║                       ║                   ║                    ║
║ 2. Upward     ║ Lower → Upper         ║ Loose             ║ Async completion   ║
║    Callback   ║ via registered cb     ║ (through cb ptr)  ║ I/O done, timeout  ║
║               ║                       ║                   ║                    ║
╠═══════════════╬═══════════════════════╬═══════════════════╬════════════════════╣
║               ║                       ║                   ║                    ║
║ 3. Registra-  ║ Lower → Upper         ║ Lifecycle         ║ Driver init/exit   ║
║    tion       ║ (announce existence)  ║ binding           ║ Add new capability ║
║               ║                       ║                   ║                    ║
╠═══════════════╬═══════════════════════╬═══════════════════╬════════════════════╣
║               ║                       ║                   ║                    ║
║ 4. Service    ║ Any → Service         ║ None              ║ Common utilities   ║
║    Call       ║ (orthogonal)          ║ (shared infra)    ║ Alloc, log, lock   ║
║               ║                       ║                   ║                    ║
╠═══════════════╬═══════════════════════╬═══════════════════╬════════════════════╣
║               ║                       ║                   ║                    ║
║ 5. Event      ║ Publisher → Many      ║ None              ║ System events      ║
║    Notify     ║ (broadcast)           ║ (pub-sub)         ║ State changes      ║
║               ║                       ║                   ║                    ║
╚═══════════════╩═══════════════════════╩═══════════════════╩════════════════════╝
```

**中文总结：**

| 类型        | 方向               | 耦合度                | 使用场景 |
|------       |------             |--------               |----------|
| **向下调用** | 上层调用下层       | 松耦合（通过 ops）     | 正常操作：read/write/ioctl |
| **向上回调** | 下层通知上层       | 松耦合（通过回调指针）  | 异步完成：I/O 完成、超时 |
| **注册机制** | 下层向上层宣告     | 生命周期绑定           | 驱动初始化/退出 |
| **服务调用** | 任何层调用服务     | 无耦合（共享基础设施）  | 通用工具：分配、日志、锁 |
| **事件通知** | 发布者广播给订阅者 | 无耦合（发布-订阅）     | 系统事件、状态变化 |

---

**Document Version**: 1.0  
**Based on**: Linux Kernel v3.2  
**Author**: Systems Architecture Analysis

