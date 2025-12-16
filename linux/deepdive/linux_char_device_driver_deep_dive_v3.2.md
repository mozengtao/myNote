# Linux Kernel Character Device Driver Deep Dive (v3.2)
## A Code-Level Walkthrough of the Character Device Subsystem

---

## Table of Contents
1. [Subsystem Context (Big Picture)](#1-subsystem-context-big-picture)
2. [Directory & File Map](#2-directory--file-map)
3. [Core Data Structures](#3-core-data-structures)
4. [Entry Points & Call Paths](#4-entry-points--call-paths)
5. [Core Workflows](#5-core-workflows)
6. [Important Algorithms & Mechanisms](#6-important-algorithms--mechanisms)
7. [Concurrency & Synchronization](#7-concurrency--synchronization)
8. [Performance Considerations](#8-performance-considerations)
9. [Common Pitfalls & Bugs](#9-common-pitfalls--bugs)
10. [How to Read This Code Yourself](#10-how-to-read-this-code-yourself)
11. [Summary & Mental Model](#11-summary--mental-model)
12. [What to Study Next](#12-what-to-study-next)

---

## 1. Subsystem Context (Big Picture)

### What Kernel Subsystem Are We Studying?

The **Character Device Driver** subsystem provides the infrastructure for drivers that handle data as a stream of bytes (characters), without block structure. These devices are accessed via special files in `/dev` and support operations like `open()`, `read()`, `write()`, `ioctl()`, and `close()`.

### What Problem Does It Solve?

1. **Device Abstraction**: Provides unified file I/O interface for diverse hardware
2. **Namespace Management**: Manages major/minor number allocation
3. **Driver Dispatch**: Routes user operations to correct driver code
4. **Module Integration**: Supports loadable kernel modules for drivers
5. **Device Registration**: Links `/dev` entries to driver implementations

### Where It Sits in the Overall Kernel Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              USER SPACE                                      │
│  ┌─────────────────────────────────────────────────────────────────────────┐│
│  │  Application                                                             ││
│  │  fd = open("/dev/mydev", O_RDWR);                                       ││
│  │  read(fd, buf, size);                                                   ││
│  │  write(fd, data, len);                                                  ││
│  │  ioctl(fd, cmd, arg);                                                   ││
│  │  close(fd);                                                             ││
│  └────────────────────────────────────┬────────────────────────────────────┘│
└───────────────────────────────────────┼─────────────────────────────────────┘
                                        │ System Call Interface
                                        ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                              KERNEL SPACE                                    │
│                                                                              │
│  ┌──────────────────────────────────────────────────────────────────────┐   │
│  │                    VFS (Virtual File System)                          │   │
│  │  ┌─────────┐     ┌─────────┐     ┌─────────┐     ┌──────────────┐   │   │
│  │  │ struct  │ ──▶ │ struct  │ ──▶ │ struct  │ ──▶ │    struct    │   │   │
│  │  │  file   │     │ dentry  │     │  inode  │     │     cdev     │   │   │
│  │  │ (f_op)  │     │         │     │ (i_cdev)│     │    (ops)     │   │   │
│  │  └─────────┘     └─────────┘     └─────────┘     └──────────────┘   │   │
│  └──────────────────────────────────────────────────────────────────────┘   │
│                                        │                                     │
│                                        │ def_chr_fops.open → chrdev_open()   │
│                                        ▼                                     │
│  ┌──────────────────────────────────────────────────────────────────────┐   │
│  │              CHARACTER DEVICE SUBSYSTEM (fs/char_dev.c)               │   │
│  │                                                                        │   │
│  │   ┌────────────────┐         ┌────────────────────────────────────┐  │   │
│  │   │  chrdevs[]     │         │         cdev_map (kobj_map)        │  │   │
│  │   │  Hash Table    │         │   ┌──────────────────────────┐     │  │   │
│  │   │ ┌────────────┐ │         │   │ probes[255] hash array   │     │  │   │
│  │   │ │ major=4    │ │         │   │ ┌────────────────────┐   │     │  │   │
│  │   │ │ name="tty" │ │         │   │ │ dev_t → kobject    │   │     │  │   │
│  │   │ │ baseminor=0│ │         │   │ │ (MAJOR:MINOR→cdev) │   │     │  │   │
│  │   │ └────────────┘ │         │   │ └────────────────────┘   │     │  │   │
│  │   │      ↓         │         │   └──────────────────────────┘     │  │   │
│  │   │ ┌────────────┐ │         └────────────────────────────────────┘  │   │
│  │   │ │ major=1    │ │                        │                         │   │
│  │   │ │ name="mem" │ │                        │ kobj_lookup()           │   │
│  │   │ │ minorct=16 │ │                        ▼                         │   │
│  │   │ └────────────┘ │           ┌─────────────────────────────┐       │   │
│  │   └────────────────┘           │      struct cdev            │       │   │
│  │                                │  ┌────────────────────────┐ │       │   │
│  │                                │  │ kobject kobj           │ │       │   │
│  │                                │  │ module *owner          │ │       │   │
│  │                                │  │ file_operations *ops ──┼─┼──┐    │   │
│  │                                │  │ dev_t dev (major:minor)│ │  │    │   │
│  │                                │  │ unsigned int count     │ │  │    │   │
│  │                                │  └────────────────────────┘ │  │    │   │
│  │                                └─────────────────────────────┘  │    │   │
│  └─────────────────────────────────────────────────────────────────┼────┘   │
│                                                                    │        │
│                                                                    ▼        │
│  ┌──────────────────────────────────────────────────────────────────────┐   │
│  │                    YOUR CHARACTER DEVICE DRIVER                       │   │
│  │  ┌──────────────────────────────────────────────────────────────────┐│   │
│  │  │  static const struct file_operations my_fops = {                 ││   │
│  │  │      .owner   = THIS_MODULE,                                     ││   │
│  │  │      .open    = my_open,       // Called on open()               ││   │
│  │  │      .release = my_release,    // Called on close()              ││   │
│  │  │      .read    = my_read,       // Called on read()               ││   │
│  │  │      .write   = my_write,      // Called on write()              ││   │
│  │  │      .unlocked_ioctl = my_ioctl, // Called on ioctl()            ││   │
│  │  │  };                                                              ││   │
│  │  └──────────────────────────────────────────────────────────────────┘│   │
│  │                               │                                       │   │
│  └───────────────────────────────┼───────────────────────────────────────┘   │
│                                  │                                           │
│                                  ▼                                           │
│  ┌──────────────────────────────────────────────────────────────────────┐   │
│  │                           HARDWARE                                    │   │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐             │   │
│  │  │  Serial  │  │   TTY    │  │  GPIO    │  │ Sensors  │  ...        │   │
│  │  │   Port   │  │ Console  │  │   Pins   │  │          │             │   │
│  │  └──────────┘  └──────────┘  └──────────┘  └──────────┘             │   │
│  └──────────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────────┘
```

**图解说明：**
- 用户空间应用程序通过标准文件操作(open/read/write/ioctl/close)访问字符设备
- VFS层将文件描述符映射到具体的inode，对于字符设备，inode包含指向cdev的指针
- 字符设备子系统维护两个核心数据结构：chrdevs[]哈希表（记录主设备号注册信息）和cdev_map（用于查找具体的cdev结构）
- struct cdev是核心抽象，它将设备号与file_operations关联
- 驱动开发者实现file_operations中的具体操作函数，处理实际的硬件I/O

### How This Subsystem Interacts with Others

| Subsystem | Interaction |
|-----------|-------------|
| **VFS** | Receives file operations from VFS via `def_chr_fops`, dispatches to driver's `file_operations` |
| **Device Model (sysfs)** | Uses `struct device` and `struct class` for `/sys` representation and udev integration |
| **Module Subsystem** | `cdev->owner` tracks module reference count, prevents unload while device is open |
| **Memory Management** | `mmap()` implementation maps device memory to user space; `copy_to/from_user()` for data transfer |
| **Interrupt Subsystem** | Drivers register IRQ handlers; use wait queues for blocking I/O |
| **udev/devtmpfs** | Automatically creates `/dev` nodes based on `device_create()` calls |

---

## 2. Directory & File Map

### Main Files Involved

```
linux-3.2/
├── fs/
│   └── char_dev.c              → Core char device management (registration, lookup)
│
├── include/
│   └── linux/
│       ├── cdev.h              → struct cdev definition and API
│       ├── kdev_t.h            → MAJOR(), MINOR(), MKDEV() macros
│       ├── fs.h                → struct file_operations, struct file, struct inode
│       ├── miscdevice.h        → Misc device (simplified char device) API
│       ├── kobj_map.h          → Device number to kobject mapping API
│       └── major.h             → Pre-defined major number constants
│
├── drivers/
│   ├── base/
│   │   └── map.c               → kobj_map implementation (dev_t lookup)
│   │
│   └── char/
│       ├── mem.c               → /dev/null, /dev/zero, /dev/mem implementation
│       ├── misc.c              → Misc device subsystem (major 10)
│       ├── random.c            → /dev/random, /dev/urandom
│       └── [device].c          → Various character device drivers
│
└── Documentation/
    └── devices.txt             → Official device number assignments
```

### File Responsibilities

| File | Responsibility |
|------|----------------|
| `fs/char_dev.c` | Device number registration, cdev management, `chrdev_open()` dispatch |
| `include/linux/cdev.h` | `struct cdev` definition, `cdev_init()`, `cdev_add()`, `cdev_del()` API |
| `include/linux/kdev_t.h` | Macros for device number manipulation (12-bit major, 20-bit minor) |
| `include/linux/fs.h` | `struct file_operations` (driver callback table), `struct file`, `struct inode` |
| `drivers/base/map.c` | `kobj_map` - hash table mapping dev_t ranges to kobjects (cdev) |
| `drivers/char/misc.c` | Simplified registration for single-minor devices (major 10) |
| `drivers/char/mem.c` | Reference implementation: /dev/null, /dev/zero, /dev/mem |

### Why Code is Split This Way

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          CODE ORGANIZATION RATIONALE                         │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   fs/char_dev.c                                                             │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │  Generic infrastructure shared by ALL character device drivers       │   │
│   │  - Device number registration (major/minor management)               │   │
│   │  - cdev lifecycle (alloc, init, add, del)                           │   │
│   │  - Open dispatch (chrdev_open → find cdev → call driver's open)     │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                    ▲                                         │
│                                    │ Uses                                    │
│   drivers/base/map.c              │                                         │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │  Device number lookup mechanism (also used by block devices)         │   │
│   │  - Hash table: dev_t → kobject (cdev or gendisk)                    │   │
│   │  - Supports ranges of device numbers                                │   │
│   │  - Module reference counting integration                            │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                                                              │
│   drivers/char/misc.c                                                       │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │  Simplified API for "one-off" character devices                      │   │
│   │  - All misc devices share major 10                                   │   │
│   │  - Automatic minor allocation                                        │   │
│   │  - Reduces boilerplate for simple drivers                            │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                                                              │
│   drivers/char/*.c (mem.c, tty/*.c, etc.)                                   │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │  Actual device drivers implementing specific functionality           │   │
│   │  - Hardware-specific code                                            │   │
│   │  - Device-specific file_operations                                   │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

**代码分离原因说明：**
- `fs/char_dev.c` 提供所有字符设备驱动共享的基础设施，遵循"不重复自己"(DRY)原则
- `drivers/base/map.c` 实现通用的设备号查找机制，被字符设备和块设备共用
- `drivers/char/misc.c` 为简单驱动提供便捷API，减少重复代码
- 具体驱动分散在`drivers/char/`下，实现各自的硬件特定逻辑

---

## 3. Core Data Structures

### 3.1 Device Number (dev_t)

```c
/* include/linux/kdev_t.h */

#define MINORBITS    20
#define MINORMASK    ((1U << MINORBITS) - 1)

#define MAJOR(dev)   ((unsigned int) ((dev) >> MINORBITS))
#define MINOR(dev)   ((unsigned int) ((dev) & MINORMASK))
#define MKDEV(ma,mi) (((ma) << MINORBITS) | (mi))
```

**Memory Layout:**

```
                            32-bit dev_t
┌────────────────────────────────────────────────────────────────────────┐
│   MAJOR NUMBER (12 bits)  │         MINOR NUMBER (20 bits)            │
│       bits 31-20          │              bits 19-0                    │
└────────────────────────────────────────────────────────────────────────┘
│                           │                                            │
│   Identifies driver       │   Identifies specific device instance      │
│   (e.g., 1=mem, 4=tty)    │   (e.g., tty0=0, tty1=1, ...)              │
│                           │                                            │

Example: /dev/tty0 → major=4, minor=0 → dev_t = (4 << 20) | 0 = 0x00400000

Historical note: In older kernels, dev_t was 16-bit (8:8).
Modern kernels use 32-bit (12:20) for more devices.
```

**设备号说明：**
- 主设备号(major)标识设备驱动，例如所有tty设备共享major=4
- 次设备号(minor)标识驱动管理的具体设备实例
- 现代内核使用32位设备号：12位主设备号(最多4096个驱动)，20位次设备号(每个驱动最多100万个设备)
- `MKDEV()`宏将major和minor组合成dev_t，`MAJOR()`和`MINOR()`宏执行逆操作

### 3.2 struct cdev (Character Device)

```c
/* include/linux/cdev.h */

struct cdev {
    struct kobject kobj;                    /* Embedded kobject for refcounting */
    struct module *owner;                   /* Module that owns this cdev */
    const struct file_operations *ops;      /* Driver's operation table */
    struct list_head list;                  /* List of inodes using this cdev */
    dev_t dev;                              /* First device number */
    unsigned int count;                     /* Number of consecutive minors */
};
```

**Structure Diagram:**

```
                         struct cdev
    ┌────────────────────────────────────────────────────────────┐
    │                                                             │
    │   ┌─────────────────────────────────────────────────────┐  │
    │   │  kobject kobj                                        │  │
    │   │  ┌───────────────────────────────────────────────┐  │  │
    │   │  │ const char *name                              │  │  │   Reference
    │   │  │ struct kref kref  ←─────────────────────────────────── counting
    │   │  │ struct kobj_type *ktype → release callback    │  │  │
    │   │  └───────────────────────────────────────────────┘  │  │
    │   └─────────────────────────────────────────────────────┘  │
    │                                                             │
    │   struct module *owner ────────────────────────────────────── Prevents
    │                             │                               │ module unload
    │                             ▼                               │ while in use
    │                         ┌─────────┐                         │
    │                         │THIS_    │                         │
    │                         │MODULE   │                         │
    │                         └─────────┘                         │
    │                                                             │
    │   const struct file_operations *ops ───────────────────────── Driver's
    │                             │                               │ callbacks
    │                             ▼                               │
    │   ┌─────────────────────────────────────────────────────┐  │
    │   │  .open    = my_open,                                │  │
    │   │  .release = my_release,                             │  │
    │   │  .read    = my_read,                                │  │
    │   │  .write   = my_write,                               │  │
    │   │  .unlocked_ioctl = my_ioctl,                        │  │
    │   └─────────────────────────────────────────────────────┘  │
    │                                                             │
    │   struct list_head list ───────────────────────────────────── Tracks open
    │                             │                               │ inodes
    │                             ▼                               │
    │                         ┌─────────┐                         │
    │                         │ inode 1 │                         │
    │                         │    ↕    │                         │
    │                         │ inode 2 │                         │
    │                         └─────────┘                         │
    │                                                             │
    │   dev_t dev ─────────────────────────────────────────────── First device
    │        │                                                    │ number
    │        └──▶ MAJOR=4, MINOR=0 (e.g., /dev/tty0)              │
    │                                                             │
    │   unsigned int count ────────────────────────────────────── Minor count
    │        │                                                    │
    │        └──▶ 256 (handles /dev/tty0 through /dev/tty255)     │
    │                                                             │
    └────────────────────────────────────────────────────────────┘
```

**字段说明：**
- `kobj`: 嵌入的kobject，用于引用计数和sysfs集成；当引用计数归零时，调用release回调释放cdev
- `owner`: 指向拥有此cdev的模块，防止设备打开时模块被卸载
- `ops`: 指向驱动实现的file_operations，定义了设备支持的所有操作
- `list`: 双向链表，链接所有打开此设备的inode，用于cdev删除时清理
- `dev`: 此cdev管理的第一个设备号
- `count`: 连续次设备号的数量，即此cdev管理多少个设备实例

### 3.3 struct file_operations

```c
/* include/linux/fs.h (simplified for char devices) */

struct file_operations {
    struct module *owner;
    
    /* File positioning */
    loff_t (*llseek) (struct file *, loff_t, int);
    
    /* Data transfer */
    ssize_t (*read) (struct file *, char __user *, size_t, loff_t *);
    ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *);
    
    /* Async I/O */
    ssize_t (*aio_read) (struct kiocb *, const struct iovec *, unsigned long, loff_t);
    ssize_t (*aio_write) (struct kiocb *, const struct iovec *, unsigned long, loff_t);
    
    /* Polling */
    unsigned int (*poll) (struct file *, struct poll_table_struct *);
    
    /* Device control */
    long (*unlocked_ioctl) (struct file *, unsigned int, unsigned long);
    long (*compat_ioctl) (struct file *, unsigned int, unsigned long);
    
    /* Memory mapping */
    int (*mmap) (struct file *, struct vm_area_struct *);
    
    /* Open/Close */
    int (*open) (struct inode *, struct file *);
    int (*flush) (struct file *, fl_owner_t id);
    int (*release) (struct inode *, struct file *);
    
    /* Synchronization */
    int (*fsync) (struct file *, loff_t, loff_t, int datasync);
    int (*fasync) (int, struct file *, int);
    
    /* File locking */
    int (*lock) (struct file *, int, struct file_lock *);
    int (*flock) (struct file *, int, struct file_lock *);
    
    /* Splice (zero-copy pipe) */
    ssize_t (*splice_write)(struct pipe_inode_info *, struct file *, ...);
    ssize_t (*splice_read)(struct file *, loff_t *, struct pipe_inode_info *, ...);
};
```

**Operation Categories:**

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    file_operations Function Categories                       │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ┌──────────────────────┐  ┌──────────────────────┐  ┌──────────────────┐  │
│  │  LIFECYCLE           │  │  DATA TRANSFER       │  │  CONTROL          │  │
│  │  ┌────────────────┐  │  │  ┌────────────────┐  │  │  ┌────────────┐  │  │
│  │  │ open()         │  │  │  │ read()         │  │  │  │ ioctl()    │  │  │
│  │  │ release()      │  │  │  │ write()        │  │  │  │ llseek()   │  │  │
│  │  │ flush()        │  │  │  │ aio_read()     │  │  │  │            │  │  │
│  │  └────────────────┘  │  │  │ aio_write()    │  │  │  └────────────┘  │  │
│  │                      │  │  │ splice_read()  │  │  │                  │  │
│  │  User space:         │  │  │ splice_write() │  │  │  User space:     │  │
│  │  fd = open(path)     │  │  └────────────────┘  │  │  ioctl(fd,cmd)   │  │
│  │  close(fd)           │  │                      │  │  lseek(fd,off)   │  │
│  └──────────────────────┘  │  User space:         │  └──────────────────┘  │
│                            │  read(fd,buf,n)      │                        │
│  ┌──────────────────────┐  │  write(fd,buf,n)     │  ┌──────────────────┐  │
│  │  ASYNC/POLL          │  └──────────────────────┘  │  MEMORY           │  │
│  │  ┌────────────────┐  │                            │  ┌────────────┐  │  │
│  │  │ poll()         │  │                            │  │ mmap()     │  │  │
│  │  │ fasync()       │  │                            │  │            │  │  │
│  │  └────────────────┘  │                            │  └────────────┘  │  │
│  │                      │                            │                  │  │
│  │  User space:         │                            │  User space:     │  │
│  │  poll(fds,n,timeout) │                            │  mmap(addr,len,  │  │
│  │  select(nfds,...)    │                            │       prot,...)  │  │
│  │  epoll_wait(...)     │                            │                  │  │
│  └──────────────────────┘                            └──────────────────┘  │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

**file_operations说明：**
- `owner`: 模块指针，用于引用计数
- `open/release`: 设备打开和关闭时调用，用于初始化/清理per-open状态
- `read/write`: 数据传输主函数，使用`copy_to_user()`/`copy_from_user()`
- `unlocked_ioctl`: 设备特定命令，无大内核锁(BKL)版本
- `compat_ioctl`: 32位应用在64位内核上的兼容ioctl
- `poll`: 支持select()/poll()/epoll()系统调用
- `mmap`: 将设备内存映射到用户地址空间
- `llseek`: 更新文件位置，字符设备通常返回`-ESPIPE`或使用`noop_llseek`

### 3.4 struct char_device_struct (Registration Record)

```c
/* fs/char_dev.c */

static struct char_device_struct {
    struct char_device_struct *next;   /* Hash chain linkage */
    unsigned int major;                 /* Major device number */
    unsigned int baseminor;             /* First minor number */
    int minorct;                        /* Count of minors */
    char name[64];                      /* Driver name */
    struct cdev *cdev;                  /* Associated cdev (legacy API) */
} *chrdevs[CHRDEV_MAJOR_HASH_SIZE];     /* Hash table, size=255 */
```

**Hash Table Structure:**

```
chrdevs[] Hash Table (indexed by major % 255)
┌───────────────────────────────────────────────────────────────────────────┐
│                                                                            │
│  Index 0:   NULL                                                           │
│                                                                            │
│  Index 1:   ┌───────────────────┐     ┌───────────────────┐               │
│             │ major=1           │────▶│ major=256         │────▶ NULL     │
│             │ name="mem"        │     │ name="xxx"        │               │
│             │ baseminor=0       │     │ ...               │               │
│             │ minorct=16        │     │                   │               │
│             │ cdev=NULL         │     │                   │               │
│             └───────────────────┘     └───────────────────┘               │
│                                                                            │
│  Index 2:   NULL                                                           │
│                                                                            │
│  Index 3:   NULL                                                           │
│                                                                            │
│  Index 4:   ┌───────────────────┐                                         │
│             │ major=4           │────▶ NULL                               │
│             │ name="tty"        │                                          │
│             │ baseminor=0       │                                          │
│             │ minorct=256       │                                          │
│             │ cdev=NULL         │                                          │
│             └───────────────────┘                                          │
│                                                                            │
│  ...                                                                       │
│                                                                            │
│  Index 10:  ┌───────────────────┐                                         │
│             │ major=10          │────▶ NULL                               │
│             │ name="misc"       │                                          │
│             │ baseminor=0       │                                          │
│             │ minorct=256       │                                          │
│             └───────────────────┘                                          │
│                                                                            │
│  ...                                                                       │
│                                                                            │
│  Index 254: NULL                                                           │
│                                                                            │
└───────────────────────────────────────────────────────────────────────────┘
```

**char_device_struct说明：**
- 这是设备号注册的记录结构，存储在`chrdevs[]`哈希表中
- 哈希函数：`index = major % 255`，冲突通过链表解决
- `name`字段出现在`/proc/devices`中，帮助管理员识别驱动
- 此结构仅用于跟踪设备号分配，实际的设备查找通过`cdev_map`进行

### 3.5 struct kobj_map (Device Lookup Map)

```c
/* drivers/base/map.c */

struct kobj_map {
    struct probe {
        struct probe *next;        /* Hash chain */
        dev_t dev;                 /* Device number start */
        unsigned long range;       /* Number of device numbers */
        struct module *owner;      /* Module reference */
        kobj_probe_t *get;         /* Probe function: returns kobject */
        int (*lock)(dev_t, void*); /* Lock function: module_get */
        void *data;                /* Opaque data (points to cdev) */
    } *probes[255];                /* Hash buckets by major % 255 */
    struct mutex *lock;            /* Protects the map */
};
```

**Lookup Flow:**

```
                    kobj_lookup(cdev_map, dev_t)
                                │
                                ▼
    ┌─────────────────────────────────────────────────────────────────────┐
    │                                                                      │
    │   1. Calculate bucket:  bucket = MAJOR(dev) % 255                   │
    │                                                                      │
    │   2. Walk probe chain:  cdev_map->probes[bucket]                    │
    │                                                                      │
    │      ┌─────────────────────────────────────────────────────────┐    │
    │      │                                                          │    │
    │      │  for each probe p in chain:                              │    │
    │      │      if (dev >= p->dev && dev < p->dev + p->range)      │    │
    │      │          // Found matching range                         │    │
    │      │          try_module_get(p->owner);                       │    │
    │      │          if (p->lock(dev, p->data) == 0)                │    │
    │      │              return p->get(dev, &index, p->data);        │    │
    │      │                         │                                │    │
    │      │                         ▼                                │    │
    │      │                  exact_match() returns &cdev->kobj       │    │
    │      │                                                          │    │
    │      └─────────────────────────────────────────────────────────┘    │
    │                                                                      │
    │   3. Use container_of() to get cdev from kobject:                   │
    │      cdev = container_of(kobj, struct cdev, kobj);                  │
    │                                                                      │
    └─────────────────────────────────────────────────────────────────────┘
```

**kobj_map说明：**
- `cdev_map`是字符设备子系统维护的全局kobj_map实例
- 当驱动调用`cdev_add()`时，会在此map中注册设备号范围
- 当打开设备时，`chrdev_open()`使用`kobj_lookup()`查找对应的cdev
- 设计支持设备号范围（一个cdev可管理多个minor），而非逐个注册
- 使用模块引用计数确保查找到的cdev所属模块不会被卸载

### 3.6 struct miscdevice (Simplified Character Device)

```c
/* include/linux/miscdevice.h */

struct miscdevice {
    int minor;                           /* Minor number (or MISC_DYNAMIC_MINOR) */
    const char *name;                    /* Device name (for /dev) */
    const struct file_operations *fops;  /* Driver operations */
    struct list_head list;               /* Internal: linked list of all misc devs */
    struct device *parent;               /* Parent device in device model */
    struct device *this_device;          /* Created device in /sys */
    const char *nodename;                /* Alternate name for /dev node */
    mode_t mode;                         /* Permissions for /dev node */
};
```

**miscdevice vs cdev:**

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                      Comparison: cdev vs miscdevice                          │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   ┌─────────────────────────────┐    ┌─────────────────────────────────┐    │
│   │  STANDARD CHAR DEVICE       │    │  MISC DEVICE                    │    │
│   │  (struct cdev)              │    │  (struct miscdevice)            │    │
│   ├─────────────────────────────┤    ├─────────────────────────────────┤    │
│   │                             │    │                                 │    │
│   │  Registration Steps:        │    │  Registration Steps:            │    │
│   │  1. alloc_chrdev_region()   │    │  1. misc_register()             │    │
│   │  2. cdev_init()             │    │     (that's it!)                │    │
│   │  3. cdev_add()              │    │                                 │    │
│   │  4. class_create()          │    │  Unregistration:                │    │
│   │  5. device_create()         │    │  1. misc_deregister()           │    │
│   │                             │    │                                 │    │
│   │  Unregistration:            │    │                                 │    │
│   │  1. device_destroy()        │    │                                 │    │
│   │  2. class_destroy()         │    │                                 │    │
│   │  3. cdev_del()              │    │                                 │    │
│   │  4. unregister_chrdev_region│    │                                 │    │
│   │                             │    │                                 │    │
│   │  Major Number: Any          │    │  Major Number: 10 (fixed)       │    │
│   │  Minor Count: Multiple OK   │    │  Minor Count: 1 only            │    │
│   │  Device Nodes: Manual       │    │  Device Nodes: Automatic        │    │
│   │                             │    │                                 │    │
│   │  Use When:                  │    │  Use When:                      │    │
│   │  - Multiple device instances│    │  - Single simple device         │    │
│   │  - Need specific major#     │    │  - Quick prototyping            │    │
│   │  - Complex driver           │    │  - Misc functionality           │    │
│   │                             │    │                                 │    │
│   └─────────────────────────────┘    └─────────────────────────────────┘    │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

**miscdevice说明：**
- misc设备是对字符设备的简化封装，所有misc设备共享major 10
- 只需一次`misc_register()`调用即可完成注册，自动创建/dev节点
- 适用于功能简单、只有单个设备实例的驱动
- 内核中很多设备使用misc接口：/dev/watchdog, /dev/fuse, /dev/kvm等

---

## 4. Entry Points & Call Paths

### 4.1 Device Number Registration

Two main registration APIs exist:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    Device Number Registration Options                        │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  OPTION 1: Static Major Number (Legacy)                                     │
│  ════════════════════════════════════════                                   │
│                                                                              │
│  register_chrdev_region(MKDEV(major, first_minor), count, "name")           │
│      │                                                                       │
│      ├──▶ For drivers with pre-assigned major numbers                       │
│      ├──▶ Major numbers defined in Documentation/devices.txt                │
│      └──▶ Example: major=1 for /dev/mem, major=4 for tty                    │
│                                                                              │
│  OPTION 2: Dynamic Major Number (Recommended)                               │
│  ════════════════════════════════════════════                               │
│                                                                              │
│  alloc_chrdev_region(&dev, first_minor, count, "name")                      │
│      │                                                                       │
│      ├──▶ Kernel assigns unused major number                                │
│      ├──▶ dev_t stored in 'dev' parameter                                   │
│      ├──▶ Prevents major number conflicts                                   │
│      └──▶ Preferred for new drivers                                         │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

**Call Chain for `alloc_chrdev_region()`:**

```
alloc_chrdev_region(dev_t *dev, unsigned baseminor, unsigned count, char *name)
│                                        fs/char_dev.c:232
│
└──▶ __register_chrdev_region(0, baseminor, count, name)
     │                                   fs/char_dev.c:94
     │
     ├──▶ kzalloc(sizeof(struct char_device_struct))
     │        Allocate registration record
     │
     ├──▶ mutex_lock(&chrdevs_lock)
     │        Protect hash table
     │
     ├──▶ if (major == 0)
     │    │   // Dynamic allocation: find free major
     │    │   for (i = ARRAY_SIZE(chrdevs)-1; i > 0; i--)
     │    │       if (chrdevs[i] == NULL)
     │    │           break;
     │    │   major = i;  // Allocate from high end down
     │    │
     │    └── This avoids conflicts with well-known low major numbers
     │
     ├──▶ i = major_to_index(major)      // Hash: major % 255
     │
     ├──▶ Check for overlapping minor ranges
     │    │   Walk chrdevs[i] chain
     │    │   Ensure no overlap: [baseminor, baseminor+count)
     │    │
     │    └── Return -EBUSY if overlap detected
     │
     ├──▶ Insert into chrdevs[i] chain (sorted by baseminor)
     │
     └──▶ mutex_unlock(&chrdevs_lock)
          
     Return: cd (char_device_struct) or ERR_PTR(-errno)
```

**设备号注册说明：**
- 推荐使用`alloc_chrdev_region()`动态分配主设备号，避免与其他驱动冲突
- 静态分配(`register_chrdev_region()`)仅用于有预定义主设备号的历史驱动
- 动态分配从高主设备号向低分配，避免与预定义的低主设备号冲突
- 注册信息存储在`chrdevs[]`哈希表中，受`chrdevs_lock`互斥锁保护

### 4.2 cdev Registration

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         cdev Registration Sequence                           │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   Driver Code:                                                               │
│   ════════════                                                               │
│                                                                              │
│   static struct cdev my_cdev;                                               │
│   static dev_t my_dev;                                                       │
│                                                                              │
│   // Step 1: Get device numbers                                             │
│   alloc_chrdev_region(&my_dev, 0, 1, "mydriver");                           │
│                                                                              │
│   // Step 2: Initialize cdev                                                │
│   cdev_init(&my_cdev, &my_fops);                                            │
│   my_cdev.owner = THIS_MODULE;                                              │
│                                                                              │
│   // Step 3: Add to system                                                  │
│   cdev_add(&my_cdev, my_dev, 1);                                            │
│                                                                              │
│   // Step 4: Create device node (for udev)                                  │
│   my_class = class_create(THIS_MODULE, "myclass");                          │
│   device_create(my_class, NULL, my_dev, NULL, "mydev");                     │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

**Call Chain for `cdev_init()` and `cdev_add()`:**

```
cdev_init(struct cdev *cdev, const struct file_operations *fops)
│                                        fs/char_dev.c:542
│
├──▶ memset(cdev, 0, sizeof *cdev)
│        Zero out structure
│
├──▶ INIT_LIST_HEAD(&cdev->list)
│        Initialize inode list
│
├──▶ kobject_init(&cdev->kobj, &ktype_cdev_default)
│        │
│        └── ktype_cdev_default.release = cdev_default_release
│            Will purge inode list when refcount → 0
│
└──▶ cdev->ops = fops
         Store driver's file_operations


cdev_add(struct cdev *p, dev_t dev, unsigned count)
│                                        fs/char_dev.c:472
│
├──▶ p->dev = dev
├──▶ p->count = count
│
└──▶ kobj_map(cdev_map, dev, count, NULL, exact_match, exact_lock, p)
     │                                   drivers/base/map.c:32
     │
     ├──▶ Allocate probe structures (one per major spanned)
     │
     ├──▶ mutex_lock(domain->lock)      // cdev_map lock = chrdevs_lock
     │
     ├──▶ For each major in range:
     │    │   bucket = major % 255
     │    │   Insert probe into probes[bucket] chain
     │    │   Sorted by range (smallest range first for best match)
     │    │
     │    └── Probe contains:
     │        - dev: first device number
     │        - range: count of devices
     │        - get: exact_match → returns &cdev->kobj
     │        - lock: exact_lock → cdev_get() for refcount
     │        - data: pointer to cdev
     │
     └──▶ mutex_unlock(domain->lock)
     
     Return: 0 on success, -ENOMEM on failure
```

**cdev注册说明：**
- `cdev_init()`初始化cdev结构，设置kobject和file_operations
- `cdev_add()`将cdev注册到`cdev_map`，使其对系统可见
- 注册后，对该设备号范围的open()操作会被路由到此cdev
- `exact_match()`和`exact_lock()`是回调函数，用于查找时返回cdev的kobject并获取引用

### 4.3 Device Open Path (Most Important!)

```
                          User Space
                              │
                              │  fd = open("/dev/mydev", O_RDWR);
                              ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                                                                              │
│  sys_open()                                                      fs/open.c  │
│      │                                                                       │
│      └──▶ do_sys_open()                                                     │
│           │                                                                  │
│           └──▶ do_filp_open()                                               │
│                │                                                             │
│                └──▶ path_lookup()                                           │
│                     │   Find dentry and inode for "/dev/mydev"              │
│                     │   inode->i_mode indicates S_IFCHR (char device)       │
│                     │                                                        │
│                     └──▶ init_special_inode() was called at mknod time:     │
│                          │                                                   │
│                          │   if (S_ISCHR(mode)) {                           │
│                          │       inode->i_fop = &def_chr_fops;              │
│                          │       inode->i_rdev = rdev;  // dev_t            │
│                          │   }                                              │
│                          │                                                   │
│                          └── So inode->i_fop points to def_chr_fops         │
│                                                                              │
│                                                                              │
│  finish_open() / vfs_open()                                                 │
│      │                                                                       │
│      └──▶ do_dentry_open()                                                  │
│           │                                                                  │
│           ├──▶ f->f_op = fops_get(inode->i_fop)                             │
│           │        Now file->f_op = &def_chr_fops                           │
│           │                                                                  │
│           └──▶ f->f_op->open(inode, f)                                      │
│                │                                                             │
│                │   This calls: def_chr_fops.open = chrdev_open              │
│                ▼                                                             │
│                                                                              │
│  chrdev_open(struct inode *inode, struct file *filp)          fs/char_dev.c │
│      │                                                              :369    │
│      │                                                                       │
│      ├──▶ spin_lock(&cdev_lock)                                             │
│      │                                                                       │
│      ├──▶ p = inode->i_cdev                                                 │
│      │        Check if cdev already cached in inode                         │
│      │                                                                       │
│      ├──▶ if (!p)    // First open of this inode                            │
│      │    │                                                                  │
│      │    │   spin_unlock(&cdev_lock)                                       │
│      │    │                                                                  │
│      │    │   kobj = kobj_lookup(cdev_map, inode->i_rdev, &idx)             │
│      │    │   │        │                                                     │
│      │    │   │        ▼                                                     │
│      │    │   │   ┌─────────────────────────────────────────────┐           │
│      │    │   │   │  1. bucket = MAJOR(i_rdev) % 255            │           │
│      │    │   │   │  2. Walk probes[bucket] chain               │           │
│      │    │   │   │  3. Find probe where dev_t is in range      │           │
│      │    │   │   │  4. try_module_get(probe->owner)            │           │
│      │    │   │   │  5. Call probe->lock (= exact_lock)         │           │
│      │    │   │   │     └── cdev_get(p) for refcount            │           │
│      │    │   │   │  6. Call probe->get (= exact_match)         │           │
│      │    │   │   │     └── Return &cdev->kobj                  │           │
│      │    │   │   └─────────────────────────────────────────────┘           │
│      │    │   │                                                              │
│      │    │   └──▶ new = container_of(kobj, struct cdev, kobj)              │
│      │    │                                                                  │
│      │    │   spin_lock(&cdev_lock)                                         │
│      │    │                                                                  │
│      │    │   // Double-check (another thread may have done this)           │
│      │    │   if (!inode->i_cdev) {                                         │
│      │    │       inode->i_cdev = p = new;                                  │
│      │    │       list_add(&inode->i_devices, &p->list);                    │
│      │    │   }                                                              │
│      │    │                                                                  │
│      │    └── Caching prevents lookup on every open                         │
│      │                                                                       │
│      ├──▶ spin_unlock(&cdev_lock)                                           │
│      │                                                                       │
│      ├──▶ filp->f_op = fops_get(p->ops)                                     │
│      │        │                                                              │
│      │        └── NOW file->f_op points to YOUR driver's fops!              │
│      │                                                                       │
│      └──▶ if (filp->f_op->open)                                             │
│               ret = filp->f_op->open(inode, filp)                           │
│               │                                                              │
│               └── Call YOUR driver's open() function                        │
│                                                                              │
│      Return 0 on success                                                     │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

**设备打开流程说明：**
- 用户调用`open("/dev/mydev")`，VFS通过路径查找到inode
- 对于字符设备inode，`i_fop`指向`def_chr_fops`（在mknod时设置）
- VFS调用`def_chr_fops.open`即`chrdev_open()`
- `chrdev_open()`使用`kobj_lookup()`在`cdev_map`中查找匹配的cdev
- 找到cdev后，将其缓存到`inode->i_cdev`，避免后续open重复查找
- 关键步骤：`filp->f_op = p->ops`，将file的操作表切换为驱动的file_operations
- 最后调用驱动的`open()`函数（如果存在）

### 4.4 Read/Write/Ioctl Paths

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          Data Transfer Paths                                 │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  READ PATH                                                                   │
│  ═════════                                                                   │
│                                                                              │
│  read(fd, buf, count)                                                       │
│      │                                                                       │
│      └──▶ sys_read()                                                        │
│           │                                                                  │
│           └──▶ vfs_read()                                                   │
│                │                                                             │
│                └──▶ file->f_op->read(file, buf, count, &pos)                │
│                     │                                                        │
│                     └── YOUR my_read() function                             │
│                         │                                                    │
│                         ├── Read from hardware/buffer                        │
│                         ├── copy_to_user(buf, kernel_buf, n)                │
│                         └── Return bytes read or -errno                     │
│                                                                              │
│                                                                              │
│  WRITE PATH                                                                  │
│  ══════════                                                                  │
│                                                                              │
│  write(fd, buf, count)                                                      │
│      │                                                                       │
│      └──▶ sys_write()                                                       │
│           │                                                                  │
│           └──▶ vfs_write()                                                  │
│                │                                                             │
│                └──▶ file->f_op->write(file, buf, count, &pos)               │
│                     │                                                        │
│                     └── YOUR my_write() function                            │
│                         │                                                    │
│                         ├── copy_from_user(kernel_buf, buf, n)              │
│                         ├── Write to hardware/buffer                         │
│                         └── Return bytes written or -errno                  │
│                                                                              │
│                                                                              │
│  IOCTL PATH                                                                  │
│  ══════════                                                                  │
│                                                                              │
│  ioctl(fd, cmd, arg)                                                        │
│      │                                                                       │
│      └──▶ sys_ioctl()                                                       │
│           │                                                                  │
│           └──▶ do_vfs_ioctl()                                               │
│                │                                                             │
│                ├──▶ Check for universal ioctls (FIOCLEX, FIONBIO, etc)      │
│                │                                                             │
│                └──▶ file->f_op->unlocked_ioctl(file, cmd, arg)              │
│                     │                                                        │
│                     └── YOUR my_ioctl() function                            │
│                         │                                                    │
│                         ├── switch(cmd) to handle commands                   │
│                         ├── copy_from_user() for input args                 │
│                         ├── Perform device operation                         │
│                         ├── copy_to_user() for output results               │
│                         └── Return 0 or -errno                              │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

**数据传输路径说明：**
- read/write路径相对简单，VFS直接调用驱动的read/write函数
- 驱动必须使用`copy_to_user()`和`copy_from_user()`进行用户空间数据传输
- 这些函数处理地址验证、缺页异常、安全检查等
- ioctl路径先检查通用ioctl命令，然后调用驱动的`unlocked_ioctl`
- `unlocked_ioctl`不持有大内核锁(BKL)，驱动需自行处理同步

---

## 5. Core Workflows

### 5.1 Driver Initialization Workflow

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    Complete Driver Initialization Sequence                   │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   module_init(my_driver_init)                                               │
│       │                                                                      │
│       ▼                                                                      │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │  my_driver_init()                                                    │   │
│   │                                                                      │   │
│   │  // STEP 1: Allocate device numbers                                 │   │
│   │  ret = alloc_chrdev_region(&my_dev, 0, NUM_DEVICES, "mydriver");    │   │
│   │  if (ret < 0) {                                                      │   │
│   │      pr_err("Failed to alloc chrdev region\n");                     │   │
│   │      return ret;                                                     │   │
│   │  }                                                                   │   │
│   │  // Now my_dev contains: (assigned_major << 20) | 0                 │   │
│   │                                                                      │   │
│   │  // STEP 2: Initialize cdev structure                               │   │
│   │  cdev_init(&my_cdev, &my_fops);                                     │   │
│   │  my_cdev.owner = THIS_MODULE;                                       │   │
│   │                                                                      │   │
│   │  // STEP 3: Add cdev to system                                      │   │
│   │  ret = cdev_add(&my_cdev, my_dev, NUM_DEVICES);                     │   │
│   │  if (ret < 0) {                                                      │   │
│   │      pr_err("Failed to add cdev\n");                                │   │
│   │      goto fail_cdev_add;                                            │   │
│   │  }                                                                   │   │
│   │  // Now device is LIVE! open() can be called                        │   │
│   │                                                                      │   │
│   │  // STEP 4: Create device class (for /sys/class/myclass)            │   │
│   │  my_class = class_create(THIS_MODULE, "myclass");                   │   │
│   │  if (IS_ERR(my_class)) {                                            │   │
│   │      ret = PTR_ERR(my_class);                                       │   │
│   │      goto fail_class;                                               │   │
│   │  }                                                                   │   │
│   │                                                                      │   │
│   │  // STEP 5: Create device (triggers udev to create /dev/mydev)      │   │
│   │  my_device = device_create(my_class, NULL, my_dev, NULL, "mydev");  │   │
│   │  if (IS_ERR(my_device)) {                                           │   │
│   │      ret = PTR_ERR(my_device);                                      │   │
│   │      goto fail_device;                                              │   │
│   │  }                                                                   │   │
│   │                                                                      │   │
│   │  // STEP 6: Hardware initialization (if applicable)                 │   │
│   │  ret = init_hardware();                                             │   │
│   │  if (ret < 0)                                                        │   │
│   │      goto fail_hw;                                                   │   │
│   │                                                                      │   │
│   │  pr_info("mydriver loaded: major=%d\n", MAJOR(my_dev));             │   │
│   │  return 0;                                                           │   │
│   │                                                                      │   │
│   │  // Error handling with reverse cleanup                              │   │
│   │  fail_hw:                                                            │   │
│   │      device_destroy(my_class, my_dev);                              │   │
│   │  fail_device:                                                        │   │
│   │      class_destroy(my_class);                                       │   │
│   │  fail_class:                                                         │   │
│   │      cdev_del(&my_cdev);                                            │   │
│   │  fail_cdev_add:                                                      │   │
│   │      unregister_chrdev_region(my_dev, NUM_DEVICES);                 │   │
│   │      return ret;                                                     │   │
│   │                                                                      │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

**State Diagram:**

```
                      Driver State Machine
              
    ┌──────────────────────────────────────────────────────────────┐
    │                                                               │
    │         [UNLOADED]                                            │
    │              │                                                │
    │              │ insmod / modprobe                              │
    │              ▼                                                │
    │   ┌──────────────────┐                                       │
    │   │  alloc_chrdev_   │──▶ FAIL ──▶ Return -EBUSY             │
    │   │    region()      │                                        │
    │   └────────┬─────────┘                                       │
    │            │ SUCCESS                                          │
    │            ▼                                                  │
    │   ┌──────────────────┐                                       │
    │   │   cdev_init()    │                                       │
    │   │   cdev_add()     │──▶ FAIL ──▶ Cleanup: unregister_chrdev│
    │   └────────┬─────────┘                                       │
    │            │ SUCCESS                                          │
    │            ▼                                                  │
    │   [REGISTERED BUT NO /dev NODE]                              │
    │   (Can be opened if node created manually)                   │
    │            │                                                  │
    │            ▼                                                  │
    │   ┌──────────────────┐                                       │
    │   │  class_create()  │──▶ FAIL ──▶ Cleanup: cdev_del +       │
    │   │ device_create()  │            unregister_chrdev           │
    │   └────────┬─────────┘                                       │
    │            │ SUCCESS                                          │
    │            ▼                                                  │
    │   [FULLY OPERATIONAL]                                        │
    │   - /sys/class/myclass/mydev exists                          │
    │   - udev creates /dev/mydev                                  │
    │   - Ready to handle open/read/write/ioctl                    │
    │            │                                                  │
    │            │ rmmod                                            │
    │            ▼                                                  │
    │   ┌──────────────────┐                                       │
    │   │  Cleanup in      │                                       │
    │   │  reverse order   │                                       │
    │   └────────┬─────────┘                                       │
    │            │                                                  │
    │            ▼                                                  │
    │         [UNLOADED]                                            │
    │                                                               │
    └──────────────────────────────────────────────────────────────┘
```

**驱动初始化说明：**
- 初始化顺序很重要：先注册设备号，再添加cdev，最后创建设备节点
- `cdev_add()`后设备立即可用，必须确保在此之前完成所有初始化
- `class_create()`和`device_create()`用于与设备模型和udev集成
- 错误处理使用goto进行逆序清理，这是内核推荐的模式

### 5.2 Device Open Workflow

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         Device Open Sequence                                 │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   User: fd = open("/dev/mydev", O_RDWR);                                    │
│                                                                              │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │                                                                      │   │
│   │   VFS: Find inode for /dev/mydev                                    │   │
│   │        │                                                             │   │
│   │        │   inode->i_rdev = dev_t from mknod                         │   │
│   │        │   inode->i_fop = &def_chr_fops                             │   │
│   │        │                                                             │   │
│   │        ▼                                                             │   │
│   │   VFS: Allocate struct file                                         │   │
│   │        │                                                             │   │
│   │        │   file->f_op = inode->i_fop (= def_chr_fops)               │   │
│   │        │                                                             │   │
│   │        ▼                                                             │   │
│   │   VFS: Call file->f_op->open() = chrdev_open()                      │   │
│   │        │                                                             │   │
│   │        ▼                                                             │   │
│   │   chrdev_open():                                                     │   │
│   │        │                                                             │   │
│   │        ├── Look up cdev from dev_t using cdev_map                   │   │
│   │        │                                                             │   │
│   │        ├── Cache cdev in inode->i_cdev                              │   │
│   │        │                                                             │   │
│   │        ├── CRITICAL: file->f_op = cdev->ops (YOUR fops!)            │   │
│   │        │                                                             │   │
│   │        └── Call YOUR open(): file->f_op->open(inode, file)          │   │
│   │             │                                                        │   │
│   │             ▼                                                        │   │
│   │   my_open(struct inode *inode, struct file *filp):                  │   │
│   │        │                                                             │   │
│   │        ├── Extract minor number: minor = iminor(inode)              │   │
│   │        │                                                             │   │
│   │        ├── Allocate per-open context if needed:                     │   │
│   │        │   struct my_private *priv = kzalloc(...)                   │   │
│   │        │   priv->minor = minor;                                     │   │
│   │        │   filp->private_data = priv;                               │   │
│   │        │                                                             │   │
│   │        ├── Initialize device state                                   │   │
│   │        │                                                             │   │
│   │        └── return 0; (or -errno on failure)                         │   │
│   │                                                                      │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                                                              │
│   After successful open:                                                     │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │                                                                      │   │
│   │   struct file (fd's kernel representation)                          │   │
│   │   ┌─────────────────────────────────────────────────┐               │   │
│   │   │  f_op ──────────────────▶ YOUR file_operations  │               │   │
│   │   │  f_path.dentry ─────────▶ dentry → inode        │               │   │
│   │   │  f_pos = 0                                      │               │   │
│   │   │  f_flags = O_RDWR                               │               │   │
│   │   │  private_data ──────────▶ YOUR per-open state   │               │   │
│   │   └─────────────────────────────────────────────────┘               │   │
│   │                                                                      │   │
│   │   All subsequent read/write/ioctl go directly to YOUR functions!    │   │
│   │                                                                      │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

**设备打开工作流程说明：**
- VFS分配struct file，初始时f_op指向def_chr_fops
- chrdev_open()是关键桥接函数，将f_op切换为驱动的file_operations
- 驱动的open()函数可以：分配私有数据、检查权限、初始化硬件
- `filp->private_data`用于存储per-open状态，在read/write/ioctl中使用
- `iminor(inode)`获取次设备号，区分同一驱动的不同设备实例

### 5.3 Data Transfer Workflow (Read Example)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           Read Operation Detail                              │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   User: n = read(fd, user_buf, count);                                      │
│                                                                              │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │                                                                      │   │
│   │   Your Driver's Read Function:                                       │   │
│   │                                                                      │   │
│   │   ssize_t my_read(struct file *filp, char __user *buf,              │   │
│   │                   size_t count, loff_t *f_pos)                       │   │
│   │   {                                                                  │   │
│   │       struct my_private *priv = filp->private_data;                 │   │
│   │       unsigned char kernel_buf[BUFFER_SIZE];                         │   │
│   │       size_t bytes_to_read;                                          │   │
│   │                                                                      │   │
│   │       /* 1. Handle EOF or no data condition */                       │   │
│   │       if (device_has_no_data(priv)) {                               │   │
│   │           if (filp->f_flags & O_NONBLOCK)                           │   │
│   │               return -EAGAIN;    // Non-blocking: try again later   │   │
│   │                                                                      │   │
│   │           /* 2. Block until data available */                        │   │
│   │           if (wait_event_interruptible(priv->read_queue,            │   │
│   │                                         device_has_data(priv)))      │   │
│   │               return -ERESTARTSYS;  // Interrupted by signal        │   │
│   │       }                                                              │   │
│   │                                                                      │   │
│   │       /* 3. Read from device into kernel buffer */                   │   │
│   │       bytes_to_read = min(count, available_data(priv));             │   │
│   │       read_from_hardware(priv, kernel_buf, bytes_to_read);          │   │
│   │                                                                      │   │
│   │       /* 4. Copy to user space (CRITICAL!) */                        │   │
│   │       if (copy_to_user(buf, kernel_buf, bytes_to_read))             │   │
│   │           return -EFAULT;        // Bad user address                │   │
│   │                                                                      │   │
│   │       /* 5. Update file position (if meaningful) */                  │   │
│   │       *f_pos += bytes_to_read;                                       │   │
│   │                                                                      │   │
│   │       return bytes_to_read;      // Return actual bytes read        │   │
│   │   }                                                                  │   │
│   │                                                                      │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                                                              │
│                                                                              │
│   Memory Layout During Read:                                                 │
│                                                                              │
│   ┌──────────────────────────────────────────────────────────────────────┐  │
│   │                                                                       │  │
│   │   KERNEL SPACE                        │     USER SPACE               │  │
│   │                                        │                              │  │
│   │   Hardware/Device                      │     User Buffer             │  │
│   │   ┌──────────────┐                    │     ┌──────────────┐         │  │
│   │   │  Device      │  (1) Read          │     │              │         │  │
│   │   │  Registers   │ ────────────▶      │     │              │         │  │
│   │   │  or Buffer   │                    │     │              │         │  │
│   │   └──────────────┘                    │     └──────────────┘         │  │
│   │          │                             │            ▲                 │  │
│   │          │                             │            │                 │  │
│   │          ▼                             │            │                 │  │
│   │   ┌──────────────┐                    │            │                 │  │
│   │   │ Kernel       │  (2) copy_to_user()│            │                 │  │
│   │   │ Buffer       │ ═══════════════════╪════════════╛                 │  │
│   │   │              │     (copies data   │                              │  │
│   │   └──────────────┘      safely)       │                              │  │
│   │                                        │                              │  │
│   └───────────────────────────────────────┴──────────────────────────────┘  │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

**读操作说明：**
- read()的返回值是实际读取的字节数，0表示EOF，负数表示错误
- 必须使用`copy_to_user()`而非直接memcpy，因为用户地址可能无效或需要缺页处理
- 阻塞I/O使用`wait_event_interruptible()`，被信号中断时返回`-ERESTARTSYS`
- 非阻塞模式(O_NONBLOCK)下无数据时应返回`-EAGAIN`
- `f_pos`参数允许支持文件偏移量，对于流设备通常忽略


