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

The **Character Device Driver** subsystem manages devices that transfer data one character (or byte) at a time. Unlike block devices which transfer data in fixed-size blocks, character devices provide unbuffered, direct access to hardware. Examples include `/dev/null`, `/dev/zero`, `/dev/mem`, serial ports, keyboards, and mice.

### What Problem Does It Solve?

1. **Unified Device Access**: Provides a standard file-based interface (`open`, `read`, `write`, `ioctl`, `close`) for accessing diverse hardware
2. **Hardware Abstraction**: Hides device-specific details behind the VFS layer
3. **Device Number Management**: Allocates and manages major/minor numbers for device identification
4. **Driver Dispatch**: Routes file operations to the correct driver based on device numbers
5. **Module Support**: Allows drivers to be loaded/unloaded dynamically

### Where It Sits in the Overall Kernel Architecture

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│                                USER SPACE                                         │
│  ┌────────────────────────────────────────────────────────────────────────────┐  │
│  │  Application                                                                │  │
│  │    fd = open("/dev/mydev", O_RDWR);                                        │  │
│  │    read(fd, buf, size);                                                    │  │
│  │    ioctl(fd, MY_CMD, &arg);                                                │  │
│  │    write(fd, buf, size);                                                   │  │
│  │    close(fd);                                                              │  │
│  └────────────────────────────────────────────────────────────────────────────┘  │
└───────────────────────────────────────┬──────────────────────────────────────────┘
                                        │ System Call Interface
                                        ▼
┌──────────────────────────────────────────────────────────────────────────────────┐
│                                KERNEL SPACE                                       │
│                                                                                   │
│  ┌─────────────────────────────────────────────────────────────────────────────┐ │
│  │                              VFS LAYER                                       │ │
│  │  ┌───────────┐    ┌───────────┐    ┌───────────┐    ┌───────────────────┐   │ │
│  │  │sys_open() │───▶│do_filp_   │───▶│ inode:    │───▶│ def_chr_fops      │   │ │
│  │  │sys_read() │    │open()     │    │ i_rdev    │    │ .open=chrdev_open │   │ │
│  │  │sys_write()│    │           │    │ i_cdev    │    └─────────┬─────────┘   │ │
│  │  │sys_ioctl()│    └───────────┘    └───────────┘              │             │ │
│  │  └───────────┘                                                │             │ │
│  └───────────────────────────────────────────────────────────────┼─────────────┘ │
│                                                                  │               │
│  ┌───────────────────────────────────────────────────────────────▼─────────────┐ │
│  │                        CHARACTER DEVICE CORE                                 │ │
│  │                                                                              │ │
│  │  ┌──────────────────────────────────────────────────────────────────────┐   │ │
│  │  │                         cdev_map (kobj_map)                          │   │ │
│  │  │  ┌────────────────────────────────────────────────────────────────┐  │   │ │
│  │  │  │  Hash Table by MAJOR number (255 buckets)                      │  │   │ │
│  │  │  │  ┌────────┐  ┌────────┐  ┌────────┐                            │  │   │ │
│  │  │  │  │ probe  │  │ probe  │  │ probe  │                            │  │   │ │
│  │  │  │  │major=1 │─▶│major=1 │─▶│major=4 │─▶ ...                      │  │   │ │
│  │  │  │  │minor=3 │  │minor=5 │  │minor=0 │                            │  │   │ │
│  │  │  │  │cdev*───│  │cdev*───│  │cdev*───│                            │  │   │ │
│  │  │  │  └────────┘  └────────┘  └────────┘                            │  │   │ │
│  │  │  └────────────────────────────────────────────────────────────────┘  │   │ │
│  │  └──────────────────────────────────────────────────────────────────────┘   │ │
│  │                                  │                                           │ │
│  │                                  │ kobj_lookup() finds cdev                  │ │
│  │                                  ▼                                           │ │
│  │  ┌──────────────────────────────────────────────────────────────────────┐   │ │
│  │  │                        struct cdev                                   │   │ │
│  │  │  ┌─────────────────────────────────────────────────────────────────┐ │   │ │
│  │  │  │ kobj: kobject for reference counting                            │ │   │ │
│  │  │  │ owner: THIS_MODULE (prevents unload during use)                 │ │   │ │
│  │  │  │ ops: ──────────────▶ struct file_operations                     │ │   │ │
│  │  │  │ dev: device number (major:minor)                                │ │   │ │
│  │  │  │ count: number of minor numbers                                  │ │   │ │
│  │  │  └─────────────────────────────────────────────────────────────────┘ │   │ │
│  │  └──────────────────────────────────────────────────────────────────────┘   │ │
│  │                                  │                                           │ │
│  │                                  │ filp->f_op = cdev->ops                    │ │
│  │                                  ▼                                           │ │
│  └──────────────────────────────────────────────────────────────────────────────┘ │
│                                                                                   │
│  ┌──────────────────────────────────────────────────────────────────────────────┐ │
│  │                    DEVICE DRIVER (file_operations)                            │ │
│  │  ┌──────────────────────────────────────────────────────────────────────────┐ │ │
│  │  │  const struct file_operations my_fops = {                                │ │ │
│  │  │      .owner          = THIS_MODULE,                                      │ │ │
│  │  │      .open           = my_open,      ◀── allocate private data           │ │ │
│  │  │      .release        = my_release,   ◀── free private data               │ │ │
│  │  │      .read           = my_read,      ◀── copy_to_user()                  │ │ │
│  │  │      .write          = my_write,     ◀── copy_from_user()                │ │ │
│  │  │      .unlocked_ioctl = my_ioctl,     ◀── device-specific commands        │ │ │
│  │  │      .mmap           = my_mmap,      ◀── map device memory               │ │ │
│  │  │      .poll           = my_poll,      ◀── select/poll support             │ │ │
│  │  │      .llseek         = my_llseek,    ◀── file position                   │ │ │
│  │  │  };                                                                      │ │ │
│  │  └──────────────────────────────────────────────────────────────────────────┘ │ │
│  └──────────────────────────────────────────────────────────────────────────────┘ │
│                                       │                                           │
│                                       ▼                                           │
│  ┌──────────────────────────────────────────────────────────────────────────────┐ │
│  │                              HARDWARE                                         │ │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐          │ │
│  │  │ Serial Port │  │  Keyboard   │  │    Mouse    │  │   Custom    │          │ │
│  │  │  /dev/ttyS0 │  │ /dev/input  │  │ /dev/input  │  │  Hardware   │          │ │
│  │  └─────────────┘  └─────────────┘  └─────────────┘  └─────────────┘          │ │
│  └──────────────────────────────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────────────────────────────┘
```

**图解说明：**
- 字符设备子系统是VFS与硬件设备之间的桥梁
- 用户空间通过标准文件操作（open/read/write/ioctl/close）访问设备
- VFS层通过设备号（主设备号:次设备号）定位具体的字符设备
- cdev_map是一个哈希表，按主设备号索引，用于快速查找cdev结构
- cdev结构包含指向驱动程序file_operations的指针
- file_operations定义了驱动程序如何响应各种文件操作
- chrdev_open()是字符设备打开的入口点，负责查找cdev并安装正确的file_operations

### How This Subsystem Interacts with Others

| Subsystem | Interaction |
|-----------|-------------|
| VFS | Provides the file abstraction; VFS routes file operations to char device layer |
| Device Model (sysfs) | `struct device` and `struct class` create `/sys` and `/dev` entries |
| Module System | `THIS_MODULE`, `try_module_get()` prevent driver unload during use |
| Memory Management | `copy_to_user()`, `copy_from_user()` for user-kernel data transfer |
| Interrupt Subsystem | Drivers handle hardware interrupts via `request_irq()` |
| Wait Queues | Blocking I/O uses `wait_event()` / `wake_up()` |
| Workqueues | Deferred work from interrupt context to process context |
| udev | Automatically creates `/dev` nodes based on kernel events |

---

## 2. Directory & File Map

### Main Directories and Files

```
linux-3.2/
├── fs/
│   └── char_dev.c              → Core character device management
│                                  - cdev_add(), cdev_del()
│                                  - chrdev_open() - the VFS->chardev entry point
│                                  - register_chrdev_region(), alloc_chrdev_region()
│
├── include/linux/
│   ├── cdev.h                  → struct cdev definition
│   ├── kdev_t.h                → MAJOR(), MINOR(), MKDEV() macros
│   ├── fs.h                    → struct file_operations, struct file, struct inode
│   ├── kobj_map.h              → kobj_map interface for device lookup
│   └── miscdevice.h            → struct miscdevice for simple misc drivers
│
├── drivers/base/
│   └── map.c                   → kobj_map implementation (used by cdev_map)
│
├── drivers/char/
│   ├── mem.c                   → /dev/null, /dev/zero, /dev/mem implementations
│   ├── misc.c                  → Misc device framework (simplified char devices)
│   ├── random.c                → /dev/random, /dev/urandom
│   └── ...                     → Various character drivers
│
└── Documentation/
    └── ioctl/
        └── ioctl-number.txt    → Guidelines for ioctl number allocation
```

**文件职责说明：**

| File | Responsibility |
|------|----------------|
| `fs/char_dev.c` | 字符设备核心管理：设备号分配、cdev注册、打开时的驱动分发 |
| `include/linux/cdev.h` | 定义struct cdev结构和API声明 |
| `include/linux/kdev_t.h` | 设备号操作宏：MAJOR()提取主设备号，MINOR()提取次设备号，MKDEV()组合设备号 |
| `include/linux/fs.h` | 定义file_operations（驱动操作函数表）、struct file（打开文件）、struct inode（文件元数据） |
| `drivers/base/map.c` | kobj_map的哈希表实现，用于按设备号快速查找cdev |
| `drivers/char/mem.c` | 经典的字符设备实现范例：/dev/null、/dev/zero、/dev/mem |
| `drivers/char/misc.c` | 简化的字符设备框架，共享主设备号10，适用于简单设备 |

---

## 3. Core Data Structures

### 3.1 Device Number (`dev_t`)

```c
/* include/linux/kdev_t.h */
#define MINORBITS    20
#define MINORMASK    ((1U << MINORBITS) - 1)

#define MAJOR(dev)   ((unsigned int) ((dev) >> MINORBITS))
#define MINOR(dev)   ((unsigned int) ((dev) & MINORMASK))
#define MKDEV(ma,mi) (((ma) << MINORBITS) | (mi))
```

```
           dev_t (32 bits)
┌────────────────────────────────────────────┐
│  MAJOR (12 bits)  │    MINOR (20 bits)     │
│      0 - 4095     │      0 - 1048575       │
└────────────────────────────────────────────┘
         │                    │
         │                    └── Identifies specific device within driver
         └────────────────────── Identifies driver (historically)
```

**设备号说明：**
- dev_t是一个32位整数，编码了主设备号和次设备号
- 主设备号（12位）：标识设备驱动程序，范围0-4095
- 次设备号（20位）：由驱动程序解释，用于区分同一驱动管理的多个设备
- 例如：/dev/ttyS0的主设备号4标识串口驱动，次设备号0标识第一个串口

### 3.2 Character Device Structure (`struct cdev`)

```c
/* include/linux/cdev.h */
struct cdev {
    struct kobject kobj;                    /* Embedded kobject for refcounting */
    struct module *owner;                   /* Module that owns this cdev */
    const struct file_operations *ops;      /* Driver's file operations */
    struct list_head list;                  /* List of inodes using this cdev */
    dev_t dev;                              /* First device number */
    unsigned int count;                     /* Number of minor numbers */
};
```

```
                    struct cdev
┌──────────────────────────────────────────────────────┐
│ ┌────────────────────────────────────────────────┐   │
│ │ kobj (struct kobject)                          │   │
│ │   - Reference counting (kobject_get/put)       │   │
│ │   - Tied to cdev lifetime                      │   │
│ │   - ktype determines release function          │   │
│ └────────────────────────────────────────────────┘   │
│                                                      │
│ owner ─────────────────▶ struct module               │
│                          (prevents module unload)    │
│                                                      │
│ ops ───────────────────▶ struct file_operations      │
│                          { .read, .write, .ioctl }   │
│                                                      │
│ list ◀─────────────────▶ inode->i_devices            │
│                          (inodes using this cdev)    │
│                                                      │
│ dev = MKDEV(major, baseminor)                        │
│                                                      │
│ count = number of minor numbers                      │
└──────────────────────────────────────────────────────┘
```

**cdev结构说明：**
- `kobj`：内嵌的kobject用于引用计数，当refcount为0时释放cdev
- `owner`：拥有此cdev的模块，防止在设备被使用时卸载驱动模块
- `ops`：指向驱动程序的file_operations，这是驱动的核心
- `list`：链接所有使用此cdev的inode，用于模块卸载时清理
- `dev`：起始设备号（主设备号:基准次设备号）
- `count`：管理的次设备号数量

### 3.3 File Operations (`struct file_operations`)

```c
/* include/linux/fs.h */
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
    long (*compat_ioctl) (struct file *, unsigned int, unsigned long);
    int (*mmap) (struct file *, struct vm_area_struct *);
    int (*open) (struct inode *, struct file *);
    int (*flush) (struct file *, fl_owner_t id);
    int (*release) (struct inode *, struct file *);
    int (*fsync) (struct file *, loff_t, loff_t, int datasync);
    int (*aio_fsync) (struct kiocb *, int datasync);
    int (*fasync) (int, struct file *, int);
    int (*lock) (struct file *, int, struct file_lock *);
    ssize_t (*sendpage) (struct file *, struct page *, int, size_t, loff_t *, int);
    unsigned long (*get_unmapped_area)(struct file *, unsigned long, unsigned long,
                                       unsigned long, unsigned long);
    int (*check_flags)(int);
    int (*flock) (struct file *, int, struct file_lock *);
    ssize_t (*splice_write)(struct pipe_inode_info *, struct file *, loff_t *,
                            size_t, unsigned int);
    ssize_t (*splice_read)(struct file *, loff_t *, struct pipe_inode_info *,
                           size_t, unsigned int);
    int (*setlease)(struct file *, long, struct file_lock **);
    long (*fallocate)(struct file *file, int mode, loff_t offset, loff_t len);
};
```

**file_operations关键函数说明：**

| Method | Purpose | User-space Trigger |
|--------|---------|-------------------|
| `open` | 初始化设备，分配私有数据 | `open()` syscall |
| `release` | 清理资源，释放私有数据 | `close()` when refcount=0 |
| `read` | 从设备读取数据到用户空间 | `read()` syscall |
| `write` | 从用户空间写入数据到设备 | `write()` syscall |
| `unlocked_ioctl` | 设备特定控制命令 | `ioctl()` syscall |
| `poll` | 等待设备就绪 | `select()`/`poll()`/`epoll()` |
| `mmap` | 将设备内存映射到用户空间 | `mmap()` syscall |
| `llseek` | 修改文件位置 | `lseek()` syscall |
| `fasync` | 异步通知设置 | `fcntl(F_SETFL, O_ASYNC)` |

### 3.4 Open File (`struct file`)

```c
/* include/linux/fs.h */
struct file {
    union {
        struct list_head    fu_list;
        struct rcu_head     fu_rcuhead;
    } f_u;
    struct path             f_path;
    const struct file_operations *f_op;    /* Driver's operations */
    spinlock_t              f_lock;
    atomic_long_t           f_count;       /* Reference count */
    unsigned int            f_flags;       /* O_RDONLY, O_NONBLOCK, etc. */
    fmode_t                 f_mode;        /* FMODE_READ, FMODE_WRITE */
    loff_t                  f_pos;         /* Current file position */
    struct fown_struct      f_owner;       /* For SIGIO */
    const struct cred       *f_cred;       /* Credentials */
    /* ... */
    void                    *private_data; /* Driver's private data */
};
```

```
                  struct file (Open File Description)
┌────────────────────────────────────────────────────────────────────┐
│                                                                    │
│  f_path ──────────────────▶ { dentry, vfsmount }                   │
│                              (path to the device node)             │
│                                                                    │
│  f_op ────────────────────▶ struct file_operations                 │
│                              (from cdev->ops, driver-specific)     │
│                                                                    │
│  f_flags ──────────────────  O_RDONLY | O_NONBLOCK | ...           │
│                              (open flags from user)                │
│                                                                    │
│  f_mode ───────────────────  FMODE_READ | FMODE_WRITE              │
│                              (derived from f_flags)                │
│                                                                    │
│  f_pos ────────────────────  Current position in "file"            │
│                              (driver interprets this)              │
│                                                                    │
│  private_data ─────────────▶ Driver's per-open-instance data       │
│                              (allocated in open, freed in release) │
│                                                                    │
│  f_count ──────────────────  Reference count                       │
│                              (dup/fork increase, close decreases)  │
└────────────────────────────────────────────────────────────────────┘
```

**struct file说明：**
- 每次`open()`系统调用创建一个新的struct file
- `f_op`：指向驱动的file_operations，在chrdev_open()中设置
- `private_data`：驱动程序的私有数据，通常在open中分配，release中释放
- `f_pos`：文件位置，对字符设备的意义由驱动定义
- `f_flags`：用户传入的打开标志，如O_NONBLOCK
- `f_count`：引用计数，当降为0时调用release

### 3.5 Inode (`struct inode`) - Character Device Relevant Fields

```c
/* include/linux/fs.h */
struct inode {
    /* ... many fields ... */
    dev_t                   i_rdev;     /* Device number (for device files) */
    /* ... */
    union {
        struct pipe_inode_info *i_pipe;
        struct block_device    *i_bdev;
        struct cdev            *i_cdev;  /* Pointer to cdev (cached) */
    };
    /* ... */
    struct list_head        i_devices;  /* Links to cdev->list */
};
```

**inode与字符设备的关系：**
- `i_rdev`：存储设备号，由mknod或udev设置
- `i_cdev`：缓存指向cdev的指针，首次open时通过cdev_map查找并缓存
- `i_devices`：链接到cdev->list，用于跟踪使用此cdev的所有inode

### 3.6 Device Registration Structure (`struct char_device_struct`)

```c
/* fs/char_dev.c */
static struct char_device_struct {
    struct char_device_struct *next;    /* Hash chain */
    unsigned int major;                  /* Major number */
    unsigned int baseminor;              /* First minor number */
    int minorct;                         /* Minor count */
    char name[64];                       /* Device name (/proc/devices) */
    struct cdev *cdev;                   /* Associated cdev (for register_chrdev) */
} *chrdevs[CHRDEV_MAJOR_HASH_SIZE];      /* Hash table indexed by major */
```

```
              chrdevs[] Hash Table
┌─────────────────────────────────────────────────────────────────┐
│  Index = major % CHRDEV_MAJOR_HASH_SIZE                         │
│                                                                 │
│  [0]  ──▶ char_device_struct ──▶ char_device_struct ──▶ NULL    │
│           major=1, name="mem"    major=256, name="xxx"          │
│                                                                 │
│  [1]  ──▶ char_device_struct ──▶ NULL                           │
│           major=1, name="mem"                                   │
│                                                                 │
│  [2]  ──▶ NULL                                                  │
│                                                                 │
│  ...                                                            │
│                                                                 │
│  [254] ──▶ ...                                                  │
└─────────────────────────────────────────────────────────────────┘
```

**char_device_struct说明：**
- 这是register_chrdev_region()使用的内部结构，用于管理设备号分配
- 哈希表chrdevs[]按主设备号索引，支持快速查找
- 主要用于：显示/proc/devices，检查设备号冲突
- 注意：这与cdev_map是独立的机制，二者都是必需的

### 3.7 kobj_map (Device Dispatch Map)

```c
/* drivers/base/map.c */
struct kobj_map {
    struct probe {
        struct probe *next;
        dev_t dev;              /* First device in range */
        unsigned long range;    /* Number of devices */
        struct module *owner;
        kobj_probe_t *get;      /* Callback to get kobject */
        int (*lock)(dev_t, void *);
        void *data;             /* Points to cdev */
    } *probes[255];             /* Hash by MAJOR % 255 */
    struct mutex *lock;
};
```

```
                     cdev_map (struct kobj_map)
┌────────────────────────────────────────────────────────────────────────────────┐
│  probes[255] - Array of linked lists                                           │
│                                                                                │
│  [MAJOR(dev) % 255]                                                            │
│         │                                                                      │
│         ▼                                                                      │
│  ┌──────────────┐     ┌──────────────┐     ┌──────────────┐                    │
│  │   probe      │────▶│   probe      │────▶│   probe      │────▶ NULL          │
│  │ dev=1:0      │     │ dev=1:3      │     │ dev=256:0    │                    │
│  │ range=3      │     │ range=5      │     │ range=16     │                    │
│  │ data=cdev1*  │     │ data=cdev2*  │     │ data=cdev3*  │                    │
│  └──────────────┘     └──────────────┘     └──────────────┘                    │
│                                                                                │
│  kobj_lookup(cdev_map, dev, &idx):                                             │
│    1. Find bucket: probes[MAJOR(dev) % 255]                                    │
│    2. Search for probe where: probe->dev <= dev < probe->dev + range           │
│    3. Return probe->data (the cdev)                                            │
└────────────────────────────────────────────────────────────────────────────────┘
```

**cdev_map工作原理：**
- cdev_map是一个全局的kobj_map实例，用于字符设备分发
- 当驱动调用cdev_add()时，会在cdev_map中注册一个probe条目
- 当用户open设备文件时，chrdev_open()调用kobj_lookup()在cdev_map中查找对应的cdev
- 查找算法：按主设备号哈希定位bucket，然后线性搜索匹配的设备号范围

### 3.8 Miscellaneous Device (`struct miscdevice`)

```c
/* include/linux/miscdevice.h */
struct miscdevice {
    int minor;                          /* Minor number (or MISC_DYNAMIC_MINOR) */
    const char *name;                   /* Device name */
    const struct file_operations *fops; /* Driver's operations */
    struct list_head list;              /* Links into misc_list */
    struct device *parent;              /* Parent device */
    struct device *this_device;         /* Device created for this miscdev */
    const char *nodename;               /* /dev node name (if different) */
    mode_t mode;                        /* Permission mode */
};
```

**miscdevice说明：**
- misc设备是一种简化的字符设备框架
- 所有misc设备共享主设备号10（MISC_MAJOR）
- 驱动只需指定次设备号和file_operations
- 适用于简单的、无需复杂设备号管理的驱动

---

## 4. Entry Points & Call Paths

### 4.1 Driver Registration Call Path

```
Driver Module                   Kernel Core
    │
    │  module_init(my_init)
    ▼
┌─────────────────┐
│   my_init()     │
└────────┬────────┘
         │
         │  (1) Allocate device numbers
         ▼
┌────────────────────────────────────────────────────┐
│  alloc_chrdev_region(&dev, 0, count, "mydev")      │
│    └──▶ __register_chrdev_region(0, baseminor,     │
│              count, name)                           │
│           - Allocate char_device_struct             │
│           - Find free major if major==0             │
│           - Add to chrdevs[] hash                   │
│           - Return major in *dev                    │
└────────────────────────────────────────────────────┘
         │
         │  (2) Initialize cdev
         ▼
┌────────────────────────────────────────────────────┐
│  cdev_init(&my_cdev, &my_fops)                     │
│    - kobject_init(&cdev->kobj, &ktype_cdev_default)│
│    - INIT_LIST_HEAD(&cdev->list)                   │
│    - cdev->ops = fops                              │
└────────────────────────────────────────────────────┘
         │
         │  (3) Add cdev to system
         ▼
┌────────────────────────────────────────────────────┐
│  cdev_add(&my_cdev, dev, count)                    │
│    - cdev->dev = dev                               │
│    - cdev->count = count                           │
│    - kobj_map(cdev_map, dev, count,                │
│               NULL, exact_match, exact_lock, cdev) │
│      └──▶ Allocate probe structures                │
│      └──▶ Insert into probes[MAJOR(dev) % 255]     │
│    *** Device is now LIVE - can receive open() *** │
└────────────────────────────────────────────────────┘
         │
         │  (4) Create device node (optional)
         ▼
┌────────────────────────────────────────────────────┐
│  class_create(THIS_MODULE, "myclass")              │
│  device_create(class, NULL, dev, NULL, "mydev")    │
│    └──▶ Triggers uevent → udev creates /dev/mydev  │
└────────────────────────────────────────────────────┘
```

**驱动注册流程说明：**
1. `alloc_chrdev_region()` 动态分配主设备号（推荐）或使用`register_chrdev_region()`指定固定设备号
2. `cdev_init()` 初始化cdev结构，关联file_operations
3. `cdev_add()` 将cdev添加到cdev_map，此时设备变为活跃状态
4. 可选地使用device_create()创建sysfs条目，触发udev自动创建/dev节点

### 4.2 Device Open Call Path (The Critical Path)

```
User Space                          Kernel Space
    │
    │  fd = open("/dev/mydev", O_RDWR)
    │
    ▼
┌─────────────────┐
│  sys_open()     │
└────────┬────────┘
         │
         ▼
┌────────────────────────────────────────────────────────────────────────────────┐
│  do_filp_open()                                                                │
│    │                                                                           │
│    ├──▶ path_openat()                                                          │
│    │      └──▶ do_last()                                                       │
│    │             └──▶ nameidata_to_filp()                                      │
│    │                    └──▶ __dentry_open()                                   │
│    │                                                                           │
│    ▼                                                                           │
│  __dentry_open(dentry, mnt, filp, ...)                                         │
│    │                                                                           │
│    ├── inode = dentry->d_inode                                                 │
│    │                                                                           │
│    ├── if (S_ISCHR(inode->i_mode)):    ◀── Is this a character device?         │
│    │       filp->f_op = &def_chr_fops   ◀── Use default char fops initially    │
│    │                                                                           │
│    └── filp->f_op->open(inode, filp)   ◀── This calls chrdev_open()            │
└────────────────────────────────────────────────────────────────────────────────┘
         │
         ▼
┌────────────────────────────────────────────────────────────────────────────────┐
│  chrdev_open(inode, filp)               [fs/char_dev.c:369]                    │
│    │                                                                           │
│    ├── spin_lock(&cdev_lock)                                                   │
│    │                                                                           │
│    ├── p = inode->i_cdev               ◀── Check cached cdev pointer           │
│    │                                                                           │
│    ├── if (!p):                         ◀── First open of this inode           │
│    │       spin_unlock(&cdev_lock)                                             │
│    │       kobj = kobj_lookup(cdev_map, inode->i_rdev, &idx)                   │
│    │                     │                                                     │
│    │                     ▼                                                     │
│    │       ┌─────────────────────────────────────────────────────────────┐     │
│    │       │  kobj_lookup():                                             │     │
│    │       │    - bucket = probes[MAJOR(dev) % 255]                      │     │
│    │       │    - for each probe in bucket:                              │     │
│    │       │        if (probe->dev <= dev < probe->dev + range):         │     │
│    │       │            try_module_get(probe->owner)                     │     │
│    │       │            return probe->get(dev, &idx, data)               │     │
│    │       │                   └──▶ exact_match() returns &cdev->kobj    │     │
│    │       └─────────────────────────────────────────────────────────────┘     │
│    │       if (!kobj) return -ENXIO                                            │
│    │       new = container_of(kobj, struct cdev, kobj)                         │
│    │       spin_lock(&cdev_lock)                                               │
│    │       inode->i_cdev = p = new      ◀── Cache for future opens             │
│    │       list_add(&inode->i_devices, &p->list)                               │
│    │                                                                           │
│    ├── cdev_get(p)                      ◀── Increment refcount                 │
│    │       try_module_get(p->owner)                                            │
│    │       kobject_get(&p->kobj)                                               │
│    │                                                                           │
│    ├── spin_unlock(&cdev_lock)                                                 │
│    │                                                                           │
│    ├── filp->f_op = fops_get(p->ops)    ◀── *** KEY: Install driver's fops *** │
│    │                                                                           │
│    └── if (filp->f_op->open):                                                  │
│            ret = filp->f_op->open(inode, filp)  ◀── Call DRIVER's open()       │
│                         │                                                      │
│                         ▼                                                      │
│            ┌─────────────────────────────────────┐                             │
│            │  my_open(inode, filp)               │  ← Your driver code         │
│            │    - Allocate private_data          │                             │
│            │    - Initialize hardware            │                             │
│            │    - filp->private_data = priv      │                             │
│            └─────────────────────────────────────┘                             │
└────────────────────────────────────────────────────────────────────────────────┘
         │
         ▼
    fd returned to user space
```

**设备打开流程详解：**
1. 用户调用`open("/dev/mydev", ...)`
2. VFS通过路径查找定位到设备inode
3. VFS识别这是字符设备（S_ISCHR），设置`filp->f_op = &def_chr_fops`
4. VFS调用`def_chr_fops.open`，即`chrdev_open()`
5. `chrdev_open()`从inode->i_rdev获取设备号
6. 通过kobj_lookup()在cdev_map中查找对应的cdev
7. **关键**：用驱动的fops替换filp->f_op：`filp->f_op = cdev->ops`
8. 调用驱动的open函数初始化设备
9. 后续的read/write/ioctl直接通过filp->f_op调用驱动函数

### 4.3 Read/Write/Ioctl Call Paths

```
                    After open(), filp->f_op points to driver's fops
                    
User Space                              Kernel Space
    │
    │  read(fd, buf, count)
    │
    ▼
┌─────────────────┐      ┌────────────────────────────────────────────────────┐
│  sys_read()     │─────▶│  vfs_read(file, buf, count, &pos)                  │
└─────────────────┘      │    └──▶ file->f_op->read(file, buf, count, &pos)   │
                         │              │                                      │
                         │              ▼                                      │
                         │    ┌─────────────────────────────────────────┐      │
                         │    │  my_read(filp, buf, count, ppos)        │      │
                         │    │    - priv = filp->private_data          │      │
                         │    │    - Read from hardware/buffer          │      │
                         │    │    - copy_to_user(buf, kbuf, n)         │      │
                         │    │    - Update *ppos                       │      │
                         │    │    - Return bytes read                  │      │
                         │    └─────────────────────────────────────────┘      │
                         └────────────────────────────────────────────────────┘

    │
    │  write(fd, buf, count)
    │
    ▼
┌─────────────────┐      ┌────────────────────────────────────────────────────┐
│  sys_write()    │─────▶│  vfs_write(file, buf, count, &pos)                 │
└─────────────────┘      │    └──▶ file->f_op->write(file, buf, count, &pos)  │
                         │              │                                      │
                         │              ▼                                      │
                         │    ┌─────────────────────────────────────────┐      │
                         │    │  my_write(filp, buf, count, ppos)       │      │
                         │    │    - priv = filp->private_data          │      │
                         │    │    - copy_from_user(kbuf, buf, n)       │      │
                         │    │    - Write to hardware/buffer           │      │
                         │    │    - Update *ppos                       │      │
                         │    │    - Return bytes written               │      │
                         │    └─────────────────────────────────────────┘      │
                         └────────────────────────────────────────────────────┘

    │
    │  ioctl(fd, MY_CMD, arg)
    │
    ▼
┌─────────────────┐      ┌────────────────────────────────────────────────────┐
│  sys_ioctl()    │─────▶│  do_vfs_ioctl(filp, fd, cmd, arg)                  │
└─────────────────┘      │    └──▶ vfs_ioctl(filp, cmd, arg)                  │
                         │           └──▶ filp->f_op->unlocked_ioctl(...)     │
                         │                     │                               │
                         │                     ▼                               │
                         │    ┌─────────────────────────────────────────┐      │
                         │    │  my_ioctl(filp, cmd, arg)               │      │
                         │    │    - switch (cmd) {                     │      │
                         │    │        case MY_GET_INFO:                │      │
                         │    │          copy_to_user(...);             │      │
                         │    │        case MY_SET_CONFIG:              │      │
                         │    │          copy_from_user(...);           │      │
                         │    │      }                                  │      │
                         │    └─────────────────────────────────────────┘      │
                         └────────────────────────────────────────────────────┘
```

### 4.4 Release (Close) Call Path

```
User Space                          Kernel Space
    │
    │  close(fd)
    │
    ▼
┌─────────────────┐
│  sys_close()    │
└────────┬────────┘
         │
         ▼
┌────────────────────────────────────────────────────────────────────────────────┐
│  filp_close(filp, ...)                                                         │
│    │                                                                           │
│    ├── if (filp->f_op && filp->f_op->flush):                                   │
│    │       filp->f_op->flush(filp, ...)                                        │
│    │                                                                           │
│    └── fput(filp)                                                              │
│           │                                                                    │
│           ├── atomic_long_dec_and_test(&filp->f_count)                         │
│           │                                                                    │
│           └── if (count == 0):     ◀── Last reference closed                   │
│                   __fput(filp)                                                 │
│                     │                                                          │
│                     ├── if (filp->f_op && filp->f_op->release):                │
│                     │       filp->f_op->release(inode, filp)                   │
│                     │                │                                         │
│                     │                ▼                                         │
│                     │      ┌────────────────────────────────────┐              │
│                     │      │  my_release(inode, filp)           │              │
│                     │      │    - Free filp->private_data       │              │
│                     │      │    - Release hardware resources    │              │
│                     │      │    - Cleanup                       │              │
│                     │      └────────────────────────────────────┘              │
│                     │                                                          │
│                     ├── fops_put(filp->f_op)                                   │
│                     │       module_put(fops->owner)                            │
│                     │                                                          │
│                     └── cdev_put() (via dentry_open cleanup)                   │
│                             kobject_put(&cdev->kobj)                           │
│                             module_put(cdev->owner)                            │
└────────────────────────────────────────────────────────────────────────────────┘
```

**release调用说明：**
- `release`只在最后一个引用关闭时调用（f_count降为0）
- 多个fd指向同一个struct file时（dup/fork），只有全部关闭后才调用release
- 驱动应在release中释放所有在open中分配的资源


