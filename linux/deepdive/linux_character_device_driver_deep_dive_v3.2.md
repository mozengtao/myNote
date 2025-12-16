# Linux Kernel Character Device Driver Subsystem Deep Dive (v3.2)

This document provides a comprehensive, code-level walkthrough of the Linux 3.2 Character Device Driver subsystem. It is designed for developers who want to understand how the kernel manages character devices—from registration to file operations.

---

## 1. Subsystem Context (Big Picture)

### What Is the Character Device Driver Subsystem?

The **Character Device Driver** subsystem provides the infrastructure for drivers that handle devices accessed as a stream of bytes—one character at a time. Unlike block devices (which operate on fixed-size blocks), character devices have no inherent structure and support arbitrary-sized reads and writes.

### What Problem Does It Solve?

1. **Unified Device Interface**: Provides a consistent `/dev/` file interface for diverse hardware (serial ports, keyboards, sound cards, GPUs)
2. **Device Namespace Management**: Manages major/minor number allocation to uniquely identify devices
3. **Driver Dispatch**: Routes user-space file operations to the correct driver code
4. **Module Lifecycle**: Coordinates with the module system to prevent driver unload while in use
5. **Abstraction**: Shields user space from hardware complexity via standard POSIX file operations

### Where It Sits in the Kernel Architecture

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         USER SPACE                                       │
│   ┌─────────────────────────────────────────────────────────────────┐   │
│   │  Application: open("/dev/random", O_RDONLY)                     │   │
│   │               read(fd, buf, 256)                                │   │
│   │               ioctl(fd, RNDGETENTCNT, &cnt)                     │   │
│   └───────────────────────────┬─────────────────────────────────────┘   │
└───────────────────────────────│─────────────────────────────────────────┘
                                │ System Call (SVC/INT 0x80/SYSCALL)
================================│=============================================
                                ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                         KERNEL SPACE                                     │
│                                                                          │
│  ┌────────────────────────────────────────────────────────────────────┐ │
│  │                    VFS (Virtual File System)                        │ │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────────────────┐  │ │
│  │  │  sys_open()  │  │  sys_read()  │  │    sys_ioctl()           │  │ │
│  │  │     ▼        │  │      ▼       │  │        ▼                 │  │ │
│  │  │ do_filp_     │  │  vfs_read()  │  │  vfs_ioctl()             │  │ │
│  │  │   open()     │  │              │  │                          │  │ │
│  │  └──────┬───────┘  └──────┬───────┘  └────────┬─────────────────┘  │ │
│  │         │                 │                   │                     │ │
│  │         ▼                 ▼                   ▼                     │ │
│  │  ┌──────────────────────────────────────────────────────────────┐  │ │
│  │  │               struct file_operations                         │  │ │
│  │  │    .open    .read    .write    .unlocked_ioctl    .mmap      │  │ │
│  │  └──────────────────────────────────────────────────────────────┘  │ │
│  └───────────────────────────┬────────────────────────────────────────┘ │
│                              │                                           │
│  ┌───────────────────────────▼────────────────────────────────────────┐ │
│  │              CHARACTER DEVICE SUBSYSTEM                             │ │
│  │  ┌─────────────────────────────────────────────────────────────┐   │ │
│  │  │ chrdev_open() — Lookup cdev by dev_t, replace f_op          │   │ │
│  │  │                                                             │   │ │
│  │  │  ┌────────────────┐    ┌─────────────────────────────────┐  │   │ │
│  │  │  │   cdev_map     │    │   chrdevs[] hash table          │  │   │ │
│  │  │  │  (kobj_map)    │    │   (major → char_device_struct)  │  │   │ │
│  │  │  │                │    │                                 │  │   │ │
│  │  │  │ dev_t→cdev     │    │  Registration tracking          │  │   │ │
│  │  │  └───────┬────────┘    └─────────────────────────────────┘  │   │ │
│  │  │          │                                                   │   │ │
│  │  │          ▼                                                   │   │ │
│  │  │  ┌────────────────────────────────────────────────────────┐ │   │ │
│  │  │  │              struct cdev                               │ │   │ │
│  │  │  │   .kobj (kobject for lifecycle)                        │ │   │ │
│  │  │  │   .owner (struct module *)                             │ │   │ │
│  │  │  │   .ops (struct file_operations *)  ◄────────────────┐  │ │   │ │
│  │  │  │   .dev (dev_t - first device number)                │  │ │   │ │
│  │  │  │   .count (number of minor numbers)                  │  │ │   │ │
│  │  │  └─────────────────────────────────────────────────────│──┘ │   │ │
│  │  └────────────────────────────────────────────────────────│────┘   │ │
│  │                                                           │        │ │
│  └───────────────────────────────────────────────────────────│────────┘ │
│                                                              │          │
│  ┌───────────────────────────────────────────────────────────▼────────┐ │
│  │                    DEVICE DRIVERS                                   │ │
│  │  ┌────────────────┐  ┌────────────────┐  ┌─────────────────────┐   │ │
│  │  │  /dev/mem      │  │  /dev/random   │  │   /dev/ttyS0        │   │ │
│  │  │  mem_fops      │  │  random_fops   │  │   tty_fops          │   │ │
│  │  └────────────────┘  └────────────────┘  └─────────────────────┘   │ │
│  │                              │                                      │ │
│  │                              ▼                                      │ │
│  │                    ┌─────────────────┐                              │ │
│  │                    │    HARDWARE     │                              │ │
│  │                    └─────────────────┘                              │ │
│  └─────────────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────────────┘
```

### How This Subsystem Interacts with Others

| Adjacent Subsystem | Interaction |
|-------------------|-------------|
| **VFS** | Character devices appear as special files; VFS routes operations via `i_fop` |
| **Device Model (kobject)** | `struct cdev` embeds a kobject for reference counting and sysfs |
| **Module System** | `cdev->owner` prevents module unload during device access |
| **udev/devtmpfs** | Automatic `/dev/` node creation based on `class_create()` / `device_create()` |
| **Filesystem (ext4, etc.)** | Filesystems call `init_special_inode()` for device files |
| **Memory Management** | `mmap()` support for memory-mapped device access |

---

## 2. Directory & File Map (Code Navigation)

### Primary Files

```
fs/
├── char_dev.c              → Core character device infrastructure
│                              - cdev registration (cdev_add/cdev_del)
│                              - Major/minor number management
│                              - chrdev_open() dispatch
│                              - chrdevs[] hash table
│
├── inode.c                 → init_special_inode() - sets i_fop for devices
│
└── internal.h              → Internal declarations (chrdev_init, etc.)

include/linux/
├── cdev.h                  → struct cdev definition
│                              - Core character device structure
│
├── fs.h                    → struct file_operations
│                              - struct inode (i_rdev, i_cdev, i_fop)
│                              - struct file (f_op)
│                              - def_chr_fops declaration
│
├── kdev_t.h                → dev_t manipulation macros
│                              - MAJOR(), MINOR(), MKDEV()
│
├── major.h                 → Well-known major number definitions
│                              - MEM_MAJOR, TTY_MAJOR, MISC_MAJOR, etc.
│
├── kobj_map.h              → kobj_map interface declaration
│
└── miscdevice.h            → Simplified misc device API
                               - struct miscdevice
                               - misc_register/misc_deregister

drivers/base/
└── map.c                   → kobj_map implementation
                               - Device number → kobject lookup
                               - Supports both char and block devices

drivers/char/
├── mem.c                   → /dev/mem, /dev/null, /dev/zero, /dev/random
│                              - Reference implementation
│                              - Memory-mapped device example
│
├── misc.c                  → Miscellaneous device subsystem
│                              - Simplified registration (minor-only)
│
├── random.c                → /dev/random, /dev/urandom implementation
│                              - Complete fops example with ioctl
│
└── [other drivers]         → Various character device drivers
```

### Why Is the Code Split This Way?

1. **`fs/char_dev.c`**: Core infrastructure shared by all character devices. Kept in `fs/` because character devices are fundamentally a VFS abstraction.

2. **`drivers/base/map.c`**: Generic device number lookup (kobj_map) used by both character and block devices. The algorithm is device-type-agnostic.

3. **`drivers/char/`**: Actual driver implementations. Each driver is independent and can be a module.

4. **`include/linux/cdev.h`**: Minimal header for `struct cdev`—keeps compilation fast for drivers.

---

## 3. Core Data Structures

### 3.1 struct cdev — Character Device Object

**Location**: `include/linux/cdev.h`

```c
struct cdev {
    struct kobject kobj;                    /* Embedded kobject for refcounting */
    struct module *owner;                   /* Owning module (THIS_MODULE) */
    const struct file_operations *ops;      /* Driver's file operations */
    struct list_head list;                  /* List of inodes using this cdev */
    dev_t dev;                              /* First device number */
    unsigned int count;                     /* Number of consecutive minors */
};
```

**Field Explanations**:

| Field | Purpose |
|-------|---------|
| `kobj` | Kobject for reference counting and lifecycle management. When refcount hits 0, the cdev can be freed. |
| `owner` | Points to the module that owns this cdev. Used by `try_module_get()` to prevent unloading. |
| `ops` | Pointer to `file_operations`—the driver's implementation of open/read/write/ioctl/etc. |
| `list` | Links all inodes that have opened this device. Used by `cdev_purge()` on removal. |
| `dev` | The base `dev_t` (contains major and first minor number). |
| `count` | How many consecutive minor numbers this cdev handles. |

**Lifetime**:
- **Allocation**: `cdev_alloc()` (dynamic) or embedded in driver structure + `cdev_init()` (static)
- **Registration**: `cdev_add()` makes it visible
- **Unregistration**: `cdev_del()` removes it from the map
- **Freeing**: When `kobj` refcount hits 0 via `kobject_put()`

**Invariants**:
- `cdev_add()` must be called only once per cdev
- `ops` must point to valid memory for the cdev's lifetime
- `owner` should be set before `cdev_add()` (usually via `fops->owner`)

### 3.2 struct char_device_struct — Registration Record

**Location**: `fs/char_dev.c` (file-local)

```c
static struct char_device_struct {
    struct char_device_struct *next;   /* Hash chain link */
    unsigned int major;                 /* Major number */
    unsigned int baseminor;             /* First minor number */
    int minorct;                        /* Count of minor numbers */
    char name[64];                      /* Device name (for /proc/devices) */
    struct cdev *cdev;                  /* Associated cdev (legacy API) */
} *chrdevs[CHRDEV_MAJOR_HASH_SIZE];     /* Hash table, size=255 */
```

**Purpose**: Tracks registered character device number ranges. Used primarily for `/proc/devices` display and preventing overlapping registrations.

**Hash Table Structure**:
```
chrdevs[0]  → [major=0, name="..."] → [major=255, ...] → NULL
chrdevs[1]  → [major=1, name="mem"] → [major=256, ...] → NULL
...
chrdevs[254] → ...
```

### 3.3 struct kobj_map — Device Number Lookup Map

**Location**: `drivers/base/map.c`

```c
struct kobj_map {
    struct probe {
        struct probe *next;             /* Next in hash chain */
        dev_t dev;                      /* Base device number */
        unsigned long range;            /* Number of device numbers */
        struct module *owner;           /* Module for try_module_get() */
        kobj_probe_t *get;              /* Callback to get kobject */
        int (*lock)(dev_t, void *);     /* Lock callback (cdev_get) */
        void *data;                     /* Driver data (struct cdev *) */
    } *probes[255];                     /* Hash buckets by major % 255 */
    struct mutex *lock;                 /* Protects the map */
};
```

**Purpose**: Fast lookup from `dev_t` to `struct cdev *` (via embedded kobject). Used by `chrdev_open()` to find the right driver.

### 3.4 struct file_operations — Driver Interface

**Location**: `include/linux/fs.h`

```c
struct file_operations {
    struct module *owner;
    loff_t (*llseek) (struct file *, loff_t, int);
    ssize_t (*read) (struct file *, char __user *, size_t, loff_t *);
    ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *);
    ssize_t (*aio_read) (struct kiocb *, const struct iovec *, unsigned long, loff_t);
    ssize_t (*aio_write) (struct kiocb *, const struct iovec *, unsigned long, loff_t);
    unsigned int (*poll) (struct file *, struct poll_table_struct *);
    long (*unlocked_ioctl) (struct file *, unsigned int, unsigned long);
    long (*compat_ioctl) (struct file *, unsigned int, unsigned long);
    int (*mmap) (struct file *, struct vm_area_struct *);
    int (*open) (struct inode *, struct file *);
    int (*flush) (struct file *, fl_owner_t id);
    int (*release) (struct inode *, struct file *);
    int (*fsync) (struct file *, loff_t, loff_t, int datasync);
    int (*fasync) (int, struct file *, int);
    /* ... more operations ... */
};
```

**Key Operations for Character Devices**:

| Operation | When Called | Typical Implementation |
|-----------|-------------|------------------------|
| `open` | `open()` syscall | Initialize per-file state, check permissions |
| `release` | Last `close()` | Cleanup per-file state |
| `read` | `read()` syscall | Copy data from device to user buffer |
| `write` | `write()` syscall | Copy data from user buffer to device |
| `unlocked_ioctl` | `ioctl()` syscall | Device-specific commands |
| `poll` | `poll()`/`select()` | Report readiness for I/O |
| `mmap` | `mmap()` syscall | Map device memory to user space |
| `llseek` | `lseek()` syscall | Change file position |

### 3.5 struct inode — Device File Inode

**Relevant fields** (from `include/linux/fs.h`):

```c
struct inode {
    umode_t         i_mode;         /* File type and permissions */
    dev_t           i_rdev;         /* Device number (for device files) */
    
    const struct file_operations *i_fop;  /* Default file operations */
    
    union {
        struct pipe_inode_info  *i_pipe;
        struct block_device     *i_bdev;
        struct cdev             *i_cdev;    /* Character device pointer */
    };
    
    struct list_head i_devices;     /* Link in cdev->list */
    /* ... */
};
```

**Key Points**:
- `i_rdev` holds the `dev_t` (major:minor)
- `i_fop` initially points to `def_chr_fops` for character devices
- `i_cdev` is populated on first open to cache the cdev lookup

### 3.6 struct file — Open File Description

**Relevant fields**:

```c
struct file {
    struct path     f_path;         /* Contains dentry and vfsmount */
    const struct file_operations *f_op;  /* Current file operations */
    atomic_long_t   f_count;        /* Reference count */
    unsigned int    f_flags;        /* O_RDONLY, O_NONBLOCK, etc. */
    fmode_t         f_mode;         /* FMODE_READ, FMODE_WRITE */
    loff_t          f_pos;          /* Current position */
    void            *private_data;  /* Driver-specific data */
    /* ... */
};
```

**Important**: `f_op` is initially set from `inode->i_fop`, but `chrdev_open()` replaces it with the driver's actual `file_operations`.

### 3.7 dev_t — Device Number

**Location**: `include/linux/kdev_t.h`

```c
#define MINORBITS   20
#define MINORMASK   ((1U << MINORBITS) - 1)

#define MAJOR(dev)  ((unsigned int) ((dev) >> MINORBITS))
#define MINOR(dev)  ((unsigned int) ((dev) & MINORMASK))
#define MKDEV(ma,mi) (((ma) << MINORBITS) | (mi))
```

**Layout**:
```
       32-bit dev_t
┌──────────────┬────────────────────┐
│  12-bit      │     20-bit         │
│  MAJOR       │     MINOR          │
└──────────────┴────────────────────┘
 bits 31-20        bits 19-0
```

This allows up to 4096 major numbers and 1,048,576 minor numbers.

---

## 4. Entry Points & Call Paths

### 4.1 Device Registration Path

```
Driver initialization (module_init or built-in)
    │
    ▼
┌─────────────────────────────────────────────────────────────────────┐
│ Option A: Modern API (Recommended)                                   │
│                                                                      │
│ alloc_chrdev_region(&dev, 0, count, "mydev")                        │
│     │                                                                │
│     ▼                                                                │
│ __register_chrdev_region(0, baseminor, count, name)                 │
│     ├── kzalloc(char_device_struct)                                 │
│     ├── Find free major number (search chrdevs[] backwards)         │
│     ├── Check for overlapping minor ranges                          │
│     └── Insert into chrdevs[major % 255] hash chain                 │
│                                                                      │
│ cdev_init(&my_cdev, &my_fops)                                       │
│     ├── memset(cdev, 0, sizeof)                                     │
│     ├── INIT_LIST_HEAD(&cdev->list)                                 │
│     ├── kobject_init(&cdev->kobj, &ktype_cdev_default)              │
│     └── cdev->ops = fops                                            │
│                                                                      │
│ cdev_add(&my_cdev, dev, count)                                      │
│     ├── cdev->dev = dev                                             │
│     ├── cdev->count = count                                         │
│     └── kobj_map(cdev_map, dev, count, ..., exact_match, ...)       │
│            └── Allocate probe entries, insert into cdev_map         │
└─────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────┐
│ Option B: Legacy API (Simpler but less flexible)                     │
│                                                                      │
│ register_chrdev(major, "mydev", &my_fops)                           │
│     │                                                                │
│     ▼                                                                │
│ __register_chrdev(major, 0, 256, name, fops)                        │
│     ├── __register_chrdev_region(...)                               │
│     ├── cdev_alloc()                                                │
│     │       └── kzalloc + kobject_init(ktype_cdev_dynamic)          │
│     ├── cdev->owner = fops->owner                                   │
│     ├── cdev->ops = fops                                            │
│     └── cdev_add(...)                                               │
└─────────────────────────────────────────────────────────────────────┘
```

### 4.2 Device Open Path

```
User: open("/dev/random", O_RDONLY)
    │
    ▼
sys_open()
    │
    ▼
do_filp_open()
    │
    ▼
path_openat()
    ├── path_init() + link_path_walk() — resolve "/dev/random"
    │
    ▼
do_last()
    │
    ▼
finish_open()
    │
    ▼
do_dentry_open(file, inode)
    │
    ├── file->f_op = fops_get(inode->i_fop)
    │       │
    │       ▼
    │   For char device inode: i_fop = &def_chr_fops
    │
    └── file->f_op->open(inode, file)
            │
            ▼
┌───────────────────────────────────────────────────────────────────────┐
│ chrdev_open(inode, filp)                           [fs/char_dev.c]    │
│     │                                                                  │
│     ├── spin_lock(&cdev_lock)                                         │
│     ├── p = inode->i_cdev                                             │
│     │                                                                  │
│     ├── if (!p)   ◄── First open of this inode                        │
│     │       │                                                          │
│     │       ├── spin_unlock(&cdev_lock)                               │
│     │       │                                                          │
│     │       ├── kobj = kobj_lookup(cdev_map, inode->i_rdev, &idx)     │
│     │       │       │                                                  │
│     │       │       ▼                                                  │
│     │       │   [drivers/base/map.c]                                  │
│     │       │   ├── Lock cdev_map->lock                               │
│     │       │   ├── Search probes[MAJOR(dev) % 255]                   │
│     │       │   ├── Find matching probe (dev in range)                │
│     │       │   ├── try_module_get(probe->owner)                      │
│     │       │   ├── Call probe->lock() → cdev_get()                   │
│     │       │   │       └── try_module_get(cdev->owner)               │
│     │       │   │       └── kobject_get(&cdev->kobj)                  │
│     │       │   └── Call probe->get() → exact_match()                 │
│     │       │           └── return &cdev->kobj                        │
│     │       │                                                          │
│     │       ├── new = container_of(kobj, struct cdev, kobj)           │
│     │       │                                                          │
│     │       ├── spin_lock(&cdev_lock)                                 │
│     │       │                                                          │
│     │       ├── if (!inode->i_cdev)  ◄── Still no cdev? Cache it      │
│     │       │       ├── inode->i_cdev = new                           │
│     │       │       └── list_add(&inode->i_devices, &cdev->list)      │
│     │       │                                                          │
│     │       └── else: cdev_get(inode->i_cdev) ◄── Race: someone else  │
│     │                                              cached it          │
│     │                                                                  │
│     ├── spin_unlock(&cdev_lock)                                       │
│     │                                                                  │
│     ├── cdev_put(new)  ◄── Drop ref if we lost the race               │
│     │                                                                  │
│     ├── filp->f_op = fops_get(cdev->ops)  ◄── REPLACE file_operations │
│     │       │                                                          │
│     │       └── This is where the driver's fops becomes active!       │
│     │                                                                  │
│     └── if (filp->f_op->open)                                         │
│             │                                                          │
│             └── filp->f_op->open(inode, filp)  ◄── DRIVER's open()    │
│                     │                                                  │
│                     ▼                                                  │
│             [Driver-specific initialization]                          │
│             e.g., random_open(), tty_open(), etc.                     │
└───────────────────────────────────────────────────────────────────────┘
```

### 4.3 Read/Write/Ioctl Path

```
User: read(fd, buf, count)
    │
    ▼
sys_read(fd, buf, count)
    │
    ▼
vfs_read(file, buf, count, &pos)
    │
    ├── rw_verify_area(READ, file, pos, count)
    │
    └── file->f_op->read(file, buf, count, pos)  ◄── DRIVER's read()
            │
            ▼
    ┌───────────────────────────────────────────────────────────────┐
    │ Driver's read implementation, e.g., random_read():             │
    │                                                                │
    │ static ssize_t random_read(struct file *file,                 │
    │                            char __user *buf,                   │
    │                            size_t nbytes, loff_t *ppos)       │
    │ {                                                              │
    │     /* Extract entropy from pool */                           │
    │     n = extract_entropy_user(&blocking_pool, buf, n);         │
    │                                                                │
    │     /* May sleep waiting for entropy */                       │
    │     if (n == 0 && !(file->f_flags & O_NONBLOCK)) {           │
    │         wait_event_interruptible(random_read_wait, ...);      │
    │     }                                                          │
    │                                                                │
    │     /* copy_to_user() handled inside extract_entropy_user */  │
    │     return count;                                              │
    │ }                                                              │
    └───────────────────────────────────────────────────────────────┘

User: ioctl(fd, RNDGETENTCNT, &entropy_count)
    │
    ▼
sys_ioctl(fd, cmd, arg)
    │
    ▼
do_vfs_ioctl(file, fd, cmd, arg)
    │
    ├── [Check for standard ioctls: FIOCLEX, FIONBIO, etc.]
    │
    └── file->f_op->unlocked_ioctl(file, cmd, arg)  ◄── DRIVER's ioctl()
            │
            ▼
    ┌───────────────────────────────────────────────────────────────┐
    │ Driver's ioctl implementation, e.g., random_ioctl():          │
    │                                                                │
    │ static long random_ioctl(struct file *f,                      │
    │                          unsigned int cmd, unsigned long arg) │
    │ {                                                              │
    │     int __user *p = (int __user *)arg;                        │
    │                                                                │
    │     switch (cmd) {                                             │
    │     case RNDGETENTCNT:                                         │
    │         if (put_user(input_pool.entropy_count, p))            │
    │             return -EFAULT;                                    │
    │         return 0;                                              │
    │                                                                │
    │     case RNDADDTOENTCNT:                                       │
    │         if (!capable(CAP_SYS_ADMIN))                          │
    │             return -EPERM;                                     │
    │         /* ... */                                              │
    │                                                                │
    │     default:                                                   │
    │         return -EINVAL;                                        │
    │     }                                                          │
    │ }                                                              │
    └───────────────────────────────────────────────────────────────┘
```

---

## 5. Core Workflows (Code-Driven)

### 5.1 Initialization Workflow

**Kernel Boot**:
```c
// fs/char_dev.c
void __init chrdev_init(void)
{
    cdev_map = kobj_map_init(base_probe, &chrdevs_lock);
    bdi_init(&directly_mappable_cdev_bdi);
}
```

Called from `vfs_caches_init()` during kernel startup.

**Module/Driver Init**:
```c
// Typical driver pattern (drivers/char/mem.c)
static int __init chr_dev_init(void)
{
    // 1. Initialize backing device info
    bdi_init(&zero_bdi);
    
    // 2. Register character device with major 1, handling all 256 minors
    if (register_chrdev(MEM_MAJOR, "mem", &memory_fops))
        printk("unable to get major %d for memory devs\n", MEM_MAJOR);
    
    // 3. Create device class for udev
    mem_class = class_create(THIS_MODULE, "mem");
    
    // 4. Create individual device nodes
    for (minor = 1; minor < ARRAY_SIZE(devlist); minor++) {
        device_create(mem_class, NULL, MKDEV(MEM_MAJOR, minor),
                      NULL, devlist[minor].name);
    }
    
    return 0;
}
fs_initcall(chr_dev_init);
```

### 5.2 Fast Path: Read from /dev/null

```c
// drivers/char/mem.c
static ssize_t read_null(struct file *file, char __user *buf,
                         size_t count, loff_t *ppos)
{
    return 0;   // That's it! Returns EOF immediately.
}

static const struct file_operations null_fops = {
    .llseek     = null_lseek,
    .read       = read_null,
    .write      = write_null,
    .splice_write = splice_write_null,
};
```

This is the simplest possible character device: read returns 0 (EOF), write discards all data.

### 5.3 Read Path with Blocking: /dev/random

```c
// drivers/char/random.c
static ssize_t random_read(struct file *file, char __user *buf,
                           size_t nbytes, loff_t *ppos)
{
    ssize_t n, retval = 0, count = 0;

    while (nbytes > 0) {
        n = nbytes;
        if (n > SEC_XFER_SIZE)
            n = SEC_XFER_SIZE;

        // Extract entropy from blocking pool
        n = extract_entropy_user(&blocking_pool, buf, n);

        if (n == 0) {
            // No entropy available
            if (file->f_flags & O_NONBLOCK) {
                retval = -EAGAIN;    // Non-blocking: return immediately
                break;
            }

            // Blocking: sleep until entropy available
            wait_event_interruptible(random_read_wait,
                input_pool.entropy_count >= random_read_wakeup_thresh);

            if (signal_pending(current)) {
                retval = -ERESTARTSYS;  // Interrupted by signal
                break;
            }
            continue;  // Try again
        }

        if (n < 0) {
            retval = n;
            break;
        }
        
        count += n;
        buf += n;
        nbytes -= n;
        break;  // Like a pipe: return after first successful read
    }

    return (count ? count : retval);
}
```

### 5.4 Memory-Mapped Device: /dev/mem

```c
// drivers/char/mem.c
static ssize_t read_mem(struct file *file, char __user *buf,
                        size_t count, loff_t *ppos)
{
    unsigned long p = *ppos;    // File position = physical address
    ssize_t read, sz;
    char *ptr;

    // Validate physical address range
    if (!valid_phys_addr_range(p, count))
        return -EFAULT;

    while (count > 0) {
        sz = size_inside_page(p, count);  // Don't cross page boundary

        // Security check: is this memory region allowed?
        if (!range_is_allowed(p >> PAGE_SHIFT, count))
            return -EPERM;

        // Map physical address to kernel virtual address
        ptr = xlate_dev_mem_ptr(p);
        if (!ptr)
            return -EFAULT;

        // Copy to user space
        if (copy_to_user(buf, ptr, sz)) {
            unxlate_dev_mem_ptr(p, ptr);
            return -EFAULT;
        }
        
        unxlate_dev_mem_ptr(p, ptr);
        
        buf += sz;
        p += sz;
        count -= sz;
        read += sz;
    }

    *ppos += read;
    return read;
}
```

### 5.5 Error Handling: cdev_add Failure

```c
// Proper error handling pattern
static int __init mydev_init(void)
{
    int err;

    // Step 1: Allocate device numbers
    err = alloc_chrdev_region(&my_dev, 0, 1, "mydev");
    if (err < 0) {
        pr_err("Failed to allocate chrdev region\n");
        return err;
    }

    // Step 2: Initialize cdev
    cdev_init(&my_cdev, &my_fops);
    my_cdev.owner = THIS_MODULE;

    // Step 3: Add cdev (make it live)
    err = cdev_add(&my_cdev, my_dev, 1);
    if (err < 0) {
        pr_err("Failed to add cdev\n");
        goto fail_cdev_add;
    }

    return 0;

fail_cdev_add:
    unregister_chrdev_region(my_dev, 1);
    return err;
}

static void __exit mydev_exit(void)
{
    cdev_del(&my_cdev);                    // Remove from map
    unregister_chrdev_region(my_dev, 1);   // Release numbers
}
```

---

## 6. Important Algorithms & Mechanisms

### 6.1 Device Number Lookup (kobj_map)

The `kobj_map` uses a hash table with linear probing to look up devices:

```c
// drivers/base/map.c
struct kobject *kobj_lookup(struct kobj_map *domain, dev_t dev, int *index)
{
    struct kobject *kobj;
    struct probe *p;
    unsigned long best = ~0UL;

retry:
    mutex_lock(domain->lock);
    
    // Hash by major number
    for (p = domain->probes[MAJOR(dev) % 255]; p; p = p->next) {
        // Check if dev is in this probe's range
        if (p->dev > dev || p->dev + p->range - 1 < dev)
            continue;
            
        // Prefer smallest range (most specific match)
        if (p->range - 1 >= best)
            break;
            
        // Try to acquire module reference
        if (!try_module_get(p->owner))
            continue;
            
        // Call the lock function (cdev_get)
        if (p->lock && p->lock(dev, p->data) < 0) {
            module_put(p->owner);
            continue;
        }
        
        mutex_unlock(domain->lock);
        
        // Call the probe function (exact_match)
        kobj = p->get(dev, index, p->data);
        
        module_put(p->owner);
        
        if (kobj)
            return kobj;
        goto retry;  // Probe failed, try next
    }
    
    mutex_unlock(domain->lock);
    return NULL;
}
```

**Algorithm Properties**:
- **O(n)** worst case per hash bucket, but typically short chains
- Prefers **most specific match** (smallest range)
- **Module safety**: Holds module reference during lookup
- **Retry mechanism**: If probe fails, search continues

### 6.2 Module Reference Counting

Prevents module unload while device is in use:

```c
// include/linux/fs.h
#define fops_get(fops) \
    (((fops) && try_module_get((fops)->owner) ? (fops) : NULL))
    
#define fops_put(fops) \
    do { if (fops) module_put((fops)->owner); } while(0)
```

**Sequence**:
1. `chrdev_open()` → `fops_get(cdev->ops)` → increments module refcount
2. `chrdev_release()` called implicitly via VFS
3. `fops_put()` → decrements module refcount
4. Module can only unload when refcount reaches 0

### 6.3 Dynamic Major Number Allocation

```c
// fs/char_dev.c
static struct char_device_struct *
__register_chrdev_region(unsigned int major, unsigned int baseminor,
                         int minorct, const char *name)
{
    if (major == 0) {
        // Dynamic allocation: search backwards for free slot
        for (i = ARRAY_SIZE(chrdevs)-1; i > 0; i--) {
            if (chrdevs[i] == NULL)
                break;
        }
        
        if (i == 0) {
            ret = -EBUSY;  // No free major numbers!
            goto out;
        }
        major = i;
        ret = major;
    }
    
    // Check for overlapping ranges...
}
```

**Why search backwards?** Traditional/well-known majors (1-255) are at the beginning; dynamic allocation uses higher numbers to avoid conflicts.

### 6.4 inode Caching Optimization

```c
// fs/char_dev.c - chrdev_open()
spin_lock(&cdev_lock);
p = inode->i_cdev;
if (!p) {
    // First open: lookup and cache
    spin_unlock(&cdev_lock);
    kobj = kobj_lookup(cdev_map, inode->i_rdev, &idx);
    // ...
    spin_lock(&cdev_lock);
    if (!inode->i_cdev) {
        inode->i_cdev = p = new;  // Cache for future opens
        list_add(&inode->i_devices, &p->list);
    }
}
```

After the first open, subsequent opens skip the `kobj_lookup()` and reuse `inode->i_cdev` directly.

---

## 7. Concurrency & Synchronization

### 7.1 Locks in the Character Device Subsystem

| Lock | Type | Protects | Contention |
|------|------|----------|------------|
| `chrdevs_lock` | Mutex | `chrdevs[]` hash table | Low (registration only) |
| `cdev_lock` | Spinlock | `inode->i_cdev`, `cdev->list` | Medium (every open) |
| `cdev_map->lock` | Mutex | `cdev_map` probe chains | Low (first open only) |

### 7.2 Critical Sections

**Registration (rare, non-time-critical)**:
```c
int cdev_add(struct cdev *p, dev_t dev, unsigned count)
{
    // kobj_map() internally locks cdev_map->lock
    return kobj_map(cdev_map, dev, count, NULL, exact_match, exact_lock, p);
}
```

**Open (frequent, must be fast)**:
```c
static int chrdev_open(struct inode *inode, struct file *filp)
{
    spin_lock(&cdev_lock);
    p = inode->i_cdev;
    if (!p) {
        spin_unlock(&cdev_lock);       // Drop lock for slow path
        kobj = kobj_lookup(...);       // May sleep (mutex inside)
        spin_lock(&cdev_lock);         // Reacquire
        // Double-check i_cdev (someone else may have set it)
    }
    spin_unlock(&cdev_lock);
}
```

### 7.3 Race Condition Prevention

**Race: Two threads open same device file simultaneously**

```
Thread A                          Thread B
────────                          ────────
chrdev_open()                     chrdev_open()
  spin_lock(&cdev_lock)             (blocked)
  i_cdev == NULL
  spin_unlock()
                                    spin_lock(&cdev_lock)
  kobj_lookup()                     i_cdev == NULL
                                    spin_unlock()
  spin_lock()                       kobj_lookup()
  i_cdev still NULL?
    inode->i_cdev = new_cdev
    list_add(...)
  spin_unlock()
                                    spin_lock()
                                    i_cdev != NULL (set by A)
                                      cdev_get(i_cdev)
                                    spin_unlock()
                                    cdev_put(new_from_B) // discard duplicate
```

**What if synchronization is wrong?**

1. **Missing cdev_lock**: Two threads could both set `i_cdev`, causing a memory leak and inconsistent `cdev->list`.

2. **Missing module reference**: Module could be unloaded while `f_op` still points to its code → kernel oops.

3. **Missing kobject_get**: `cdev` could be freed while still referenced → use-after-free.

### 7.4 Open Count Management

The character device subsystem itself doesn't track open count—that's the driver's responsibility:

```c
// Driver pattern for exclusive access
static int mydev_open(struct inode *inode, struct file *file)
{
    spin_lock(&mydev_lock);
    if (mydev_open_count && (file->f_flags & O_EXCL)) {
        spin_unlock(&mydev_lock);
        return -EBUSY;
    }
    mydev_open_count++;
    spin_unlock(&mydev_lock);
    return 0;
}

static int mydev_release(struct inode *inode, struct file *file)
{
    spin_lock(&mydev_lock);
    mydev_open_count--;
    spin_unlock(&mydev_lock);
    return 0;
}
```

---

## 8. Performance Considerations

### 8.1 Hot Paths vs. Cold Paths

| Path | Frequency | Optimization |
|------|-----------|--------------|
| `read()/write()` | Very hot | Driver-specific; minimize copies |
| `chrdev_open()` (cached) | Hot | Single spinlock, no allocation |
| `chrdev_open()` (first) | Warm | Hash lookup, may allocate |
| `cdev_add()/cdev_del()` | Cold | Full mutex, memory allocation |
| `register_chrdev_region()` | Very cold | Only at init/exit |

### 8.2 Cacheline Considerations

```c
// drivers/char/mem.c - devlist is read-only after init
static const struct memdev {
    const char *name;
    mode_t mode;
    const struct file_operations *fops;
    struct backing_dev_info *dev_info;
} devlist[] = { ... };
```

- `const` data can be in read-only memory, shared across CPUs
- No cacheline bouncing during reads

### 8.3 Lock Contention Analysis

**`cdev_lock`**: Global spinlock, but held very briefly:
```c
spin_lock(&cdev_lock);
p = inode->i_cdev;       // Just a pointer read
// or
inode->i_cdev = p;       // Just a pointer write
list_add(&inode->i_devices, &p->list);  // Two pointer updates
spin_unlock(&cdev_lock);
```

Total: ~10-20 CPU cycles. Contention unlikely unless thousands of opens/second.

**Scalability Limit**: In v3.2, `cdev_lock` is global. With thousands of devices and very high open rates, this could become a bottleneck. (Later kernels improved this with per-cdev locking in some paths.)

### 8.4 Copy Overhead

User-kernel data transfer is often the bottleneck:

```c
// Naive (slow for large transfers)
copy_to_user(ubuf, kbuf, count);

// Better for large transfers: zero-copy via mmap
static int mydev_mmap(struct file *file, struct vm_area_struct *vma)
{
    // Map device memory directly into user address space
    return remap_pfn_range(vma, vma->vm_start, 
                           phys_addr >> PAGE_SHIFT,
                           vma->vm_end - vma->vm_start,
                           vma->vm_page_prot);
}
```

### 8.5 Memory Allocation in Fast Path

Avoid allocations in read/write paths:

```c
// BAD: allocates on every read
static ssize_t mydev_read(struct file *file, char __user *buf, 
                          size_t count, loff_t *ppos)
{
    char *kbuf = kmalloc(count, GFP_KERNEL);  // Slow!
    // ...
    kfree(kbuf);
}

// GOOD: use stack buffer for small transfers
static ssize_t mydev_read(struct file *file, char __user *buf,
                          size_t count, loff_t *ppos)
{
    char kbuf[256];  // Stack allocation is fast
    if (count > sizeof(kbuf))
        count = sizeof(kbuf);
    // ...
}

// GOOD: pre-allocate in open()
static int mydev_open(struct inode *inode, struct file *file)
{
    struct mydev_data *data = kzalloc(sizeof(*data), GFP_KERNEL);
    file->private_data = data;
    return 0;
}
```

---

## 9. Common Pitfalls & Bugs

### 9.1 Forgetting Module Owner

```c
// BUG: module can be unloaded while file is open
static const struct file_operations bad_fops = {
    // .owner = THIS_MODULE,  // MISSING!
    .read = mydev_read,
};

// CORRECT:
static const struct file_operations good_fops = {
    .owner = THIS_MODULE,
    .read = mydev_read,
};
```

**Consequence**: Kernel oops when accessing unloaded module's code.

### 9.2 User Pointer Validation

```c
// BUG: direct dereference of user pointer
static long mydev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int *p = (int *)arg;
    int val = *p;  // KERNEL OOPS if arg is invalid!
}

// CORRECT: use get_user/put_user/copy_from_user
static long mydev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int val;
    if (get_user(val, (int __user *)arg))
        return -EFAULT;
}
```

### 9.3 Missing Error Cleanup

```c
// BUG: resource leak on failure
static int __init mydev_init(void)
{
    int err;
    
    err = alloc_chrdev_region(&my_dev, 0, 1, "mydev");
    if (err < 0)
        return err;
    
    err = cdev_add(&my_cdev, my_dev, 1);
    if (err < 0)
        return err;  // BUG: chrdev region not freed!
    
    return 0;
}

// CORRECT: use goto-based cleanup
static int __init mydev_init(void)
{
    int err;
    
    err = alloc_chrdev_region(&my_dev, 0, 1, "mydev");
    if (err < 0)
        return err;
    
    err = cdev_add(&my_cdev, my_dev, 1);
    if (err < 0)
        goto fail_cdev;
    
    return 0;

fail_cdev:
    unregister_chrdev_region(my_dev, 1);
    return err;
}
```

### 9.4 cdev_add Before Full Initialization

```c
// BUG: device visible before ready
static int __init mydev_init(void)
{
    cdev_add(&my_cdev, my_dev, 1);  // Device is live NOW
    
    // BUG: initialization continues after device is visible
    my_private_data = kmalloc(...);  // Race condition!
}

// CORRECT: complete all setup before cdev_add
static int __init mydev_init(void)
{
    my_private_data = kmalloc(...);
    if (!my_private_data)
        return -ENOMEM;
    
    // Now safe to make visible
    cdev_add(&my_cdev, my_dev, 1);
}
```

### 9.5 Blocking in Non-Blockable Context

```c
// BUG: GFP_KERNEL in atomic context
static ssize_t mydev_read(struct file *file, char __user *buf,
                          size_t count, loff_t *ppos)
{
    spin_lock(&mydev_lock);
    kbuf = kmalloc(count, GFP_KERNEL);  // BUG: may sleep!
    spin_unlock(&mydev_lock);
}

// CORRECT: use GFP_ATOMIC or allocate before lock
static ssize_t mydev_read(struct file *file, char __user *buf,
                          size_t count, loff_t *ppos)
{
    kbuf = kmalloc(count, GFP_KERNEL);  // OK: before lock
    if (!kbuf)
        return -ENOMEM;
    
    spin_lock(&mydev_lock);
    // Use kbuf...
    spin_unlock(&mydev_lock);
}
```

### 9.6 Not Checking copy_to_user Return Value

```c
// BUG: ignoring partial copy
static ssize_t mydev_read(struct file *file, char __user *buf,
                          size_t count, loff_t *ppos)
{
    copy_to_user(buf, kbuf, count);  // BUG: ignoring return!
    return count;
}

// CORRECT: handle partial copies
static ssize_t mydev_read(struct file *file, char __user *buf,
                          size_t count, loff_t *ppos)
{
    unsigned long remaining = copy_to_user(buf, kbuf, count);
    if (remaining)
        return -EFAULT;  // Or return count - remaining for partial
    return count;
}
```

### 9.7 Historical Issues in v3.2

1. **BKL (Big Kernel Lock) removal**: By 3.2, the BKL was mostly gone, but some TTY code still had remnants. Use `unlocked_ioctl` instead of `ioctl`.

2. **No per-device locking in cdev core**: The global `cdev_lock` can be a bottleneck under extreme load.

3. **Limited error reporting**: Many character device errors just return `-EFAULT` without detailed diagnostics.

---

## 10. How to Read This Code Yourself

### 10.1 Recommended Reading Order

1. **Start with `include/linux/cdev.h`**: Just 35 lines, defines the core structure.

2. **Read `include/linux/kdev_t.h`**: Understand `dev_t`, MAJOR/MINOR macros.

3. **Read simple driver: `drivers/char/mem.c`**:
   - `null_fops` — simplest possible fops
   - `memory_fops` — dispatcher pattern
   - `chr_dev_init()` — registration example

4. **Read `fs/char_dev.c`** in this order:
   - `struct char_device_struct` and `chrdevs[]`
   - `__register_chrdev_region()` — number allocation
   - `cdev_init()` and `cdev_alloc()` — object setup
   - `cdev_add()` and `cdev_del()` — registration
   - `chrdev_open()` — the key dispatch function

5. **Read `drivers/base/map.c`**:
   - `struct kobj_map` and `struct probe`
   - `kobj_map()` — insertion
   - `kobj_lookup()` — lookup algorithm

6. **Study `drivers/char/random.c`** for a complete driver:
   - `random_fops` / `urandom_fops`
   - `random_read()` — blocking reads
   - `random_ioctl()` — ioctl implementation
   - `random_poll()` — poll support

### 10.2 What to Ignore Initially

- `compat_ioctl` — 32-bit compatibility, complex
- `aio_read/aio_write` — async I/O, advanced
- `splice_read/splice_write` — zero-copy I/O, advanced
- TTY subsystem — very complex, deserves separate study
- GPU/DRM drivers — highly specialized

### 10.3 Useful Search Commands

```bash
# Find all character device registrations
grep -r "register_chrdev\|cdev_add\|alloc_chrdev_region" drivers/

# Find file_operations definitions
grep -r "struct file_operations.*=" drivers/char/

# Find ioctl command definitions
grep -r "^#define.*_IO" include/

# Find all uses of a specific ioctl
grep -r "RNDGETENTCNT" --include="*.c"

# Cross-reference function calls
cscope -d  # then search for "chrdev_open"
```

### 10.4 Debugging Tips

**Enable kernel tracing**:
```bash
# Trace chrdev_open calls
echo 'p:chrdev_open chrdev_open inode=%di filp=%si' > /sys/kernel/debug/tracing/kprobe_events
echo 1 > /sys/kernel/debug/tracing/events/kprobes/chrdev_open/enable
cat /sys/kernel/debug/tracing/trace_pipe
```

**Check registered devices**:
```bash
cat /proc/devices       # List all major numbers
ls -la /dev/            # Device nodes with major:minor
cat /sys/class/*/dev    # Sysfs device information
```

**Module debugging**:
```bash
# Check module refcount
lsmod | grep mymodule

# Force show module sections
cat /sys/module/mymodule/sections/.text
```

---

## 11. Summary & Mental Model

### One-Paragraph Summary

The Linux character device driver subsystem provides a simple abstraction: a driver registers a `struct cdev` with associated `file_operations`, the kernel maps device numbers to cdvs via `kobj_map`, and when user space opens a device file, `chrdev_open()` looks up the cdev, replaces the file's operations with the driver's ops, and calls the driver's `open()`. From then on, all file operations (read/write/ioctl/etc.) go directly to the driver. The kobject embedded in cdev provides reference counting, and the module owner field prevents premature module unloading.

### Key Invariants

1. **dev_t uniqueness**: No two cdvs can claim overlapping (major, minor) ranges.

2. **Module safety**: While a file is open, `cdev->owner` module cannot be unloaded.

3. **cdev lifecycle**: `cdev_add()` before any opens can succeed; `cdev_del()` prevents new opens but existing files continue working.

4. **f_op replacement**: After `chrdev_open()`, `file->f_op` points to driver's ops, not `def_chr_fops`.

5. **User pointer safety**: All user-space pointers must be validated via `copy_from_user()` / `copy_to_user()` / `get_user()` / `put_user()`.

### Mental Model

Think of the character device subsystem as a **phone switchboard**:

```
        User dials               Switchboard                     Extension
        (dev_t)                  (cdev_map)                      (driver)
    ┌──────────────┐         ┌───────────────┐              ┌──────────────┐
    │  1:8         │────────►│ Lookup        │─────────────►│ random_fops  │
    │  (random)    │         │ Connect       │              │              │
    └──────────────┘         │ Replace       │              │ .read        │
                             │ f_op          │              │ .write       │
    ┌──────────────┐         │               │              │ .ioctl       │
    │  1:3         │────────►│               │─────────────►│ null_fops    │
    │  (null)      │         │               │              │              │
    └──────────────┘         └───────────────┘              └──────────────┘
                                    │
                             First call sets up
                             the connection;
                             subsequent calls
                             go directly to
                             the extension
```

Once connected (after open), all calls bypass the switchboard and go directly to the driver.

---

## 12. What to Study Next

### Recommended Learning Order

| Order | Subsystem | Why It Matters |
|-------|-----------|----------------|
| 1 | **Block Device Drivers** | Contrast with char devices; understand I/O schedulers |
| 2 | **Device Model (kobject/sysfs)** | Foundation for device lifecycle and user-space visibility |
| 3 | **TTY Subsystem** | Complex char device; line disciplines, pseudo-terminals |
| 4 | **Input Subsystem** | Event-driven char devices; evdev |
| 5 | **DRM/GPU Drivers** | Modern char device patterns; ioctls, mmap |
| 6 | **USB Drivers** | Hot-pluggable devices; usbfs |
| 7 | **V4L2 (Video4Linux2)** | Complex ioctl interface; streaming I/O |

### Relevant Files for Further Study

**Block Devices**:
- `fs/block_dev.c` — analogous to `char_dev.c`
- `block/genhd.c` — disk registration
- `include/linux/blkdev.h` — core structures

**Device Model**:
- `include/linux/kobject.h` — base object
- `drivers/base/core.c` — device core
- `fs/sysfs/` — sysfs filesystem

**TTY**:
- `drivers/tty/tty_io.c` — TTY core
- `include/linux/tty.h` — data structures
- `drivers/tty/n_tty.c` — line discipline

### Why This Order?

1. **Block devices** share the `kobj_map` mechanism—you'll recognize patterns.
2. **Device model** explains where `kobject` comes from and how sysfs works.
3. **TTY** is the most complex character device—represents the "advanced" end.
4. **Input/DRM/USB** show how modern drivers organize around the char device core.

---

## Appendix A: Quick Reference — Driver Registration

### Modern API (Recommended)

```c
#include <linux/cdev.h>
#include <linux/fs.h>

static dev_t my_dev;
static struct cdev my_cdev;

static struct file_operations my_fops = {
    .owner = THIS_MODULE,
    .open = my_open,
    .release = my_release,
    .read = my_read,
    .write = my_write,
    .unlocked_ioctl = my_ioctl,
};

static int __init my_init(void)
{
    int ret;
    
    // 1. Allocate device numbers
    ret = alloc_chrdev_region(&my_dev, 0, 1, "mydev");
    if (ret < 0)
        return ret;
    
    // 2. Initialize cdev
    cdev_init(&my_cdev, &my_fops);
    my_cdev.owner = THIS_MODULE;
    
    // 3. Add to system (makes it live!)
    ret = cdev_add(&my_cdev, my_dev, 1);
    if (ret < 0) {
        unregister_chrdev_region(my_dev, 1);
        return ret;
    }
    
    pr_info("mydev: major=%d, minor=%d\n", MAJOR(my_dev), MINOR(my_dev));
    return 0;
}

static void __exit my_exit(void)
{
    cdev_del(&my_cdev);
    unregister_chrdev_region(my_dev, 1);
}

module_init(my_init);
module_exit(my_exit);
```

### Legacy API (Simple but Less Flexible)

```c
#include <linux/fs.h>

static int major;

static struct file_operations my_fops = {
    .owner = THIS_MODULE,
    .read = my_read,
};

static int __init my_init(void)
{
    major = register_chrdev(0, "mydev", &my_fops);
    if (major < 0)
        return major;
    
    pr_info("mydev: major=%d\n", major);
    return 0;
}

static void __exit my_exit(void)
{
    unregister_chrdev(major, "mydev");
}
```

### Miscdevice API (Simplest)

```c
#include <linux/miscdevice.h>

static struct file_operations my_fops = {
    .owner = THIS_MODULE,
    .read = my_read,
};

static struct miscdevice my_misc = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "mydev",
    .fops = &my_fops,
};

static int __init my_init(void)
{
    return misc_register(&my_misc);
}

static void __exit my_exit(void)
{
    misc_deregister(&my_misc);
}
```

---

## Appendix B: Key Macros and Inline Functions

```c
// Device number manipulation
#define MAJOR(dev)      ((unsigned int) ((dev) >> 20))
#define MINOR(dev)      ((unsigned int) ((dev) & 0xfffff))
#define MKDEV(ma, mi)   (((ma) << 20) | (mi))

// Get minor from inode
static inline unsigned iminor(const struct inode *inode)
{
    return MINOR(inode->i_rdev);
}

// Get major from inode  
static inline unsigned imajor(const struct inode *inode)
{
    return MAJOR(inode->i_rdev);
}

// Module reference helpers
#define fops_get(fops) \
    (((fops) && try_module_get((fops)->owner) ? (fops) : NULL))
    
#define fops_put(fops) \
    do { if (fops) module_put((fops)->owner); } while(0)

// Container of for getting cdev from kobject
container_of(kobj, struct cdev, kobj)
```

---

*Document generated for Linux kernel v3.2. Some details may differ in other versions.*

