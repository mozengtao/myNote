# Linux Kernel Device Model (kobject/sysfs) Deep Dive (v3.2)

This document provides a comprehensive, code-level walkthrough of the Linux 3.2 Device Model subsystem, focusing on **kobject**, **kset**, and **sysfs**. These form the foundation for the kernel's unified device model, enabling hot-plug, power management, and user-space visibility through `/sys`.

---

## 1. Subsystem Context (Big Picture)

### What Is the Device Model Subsystem?

The **Linux Device Model** is the kernel infrastructure that provides:

1. **kobject** — A generic kernel object with reference counting, hierarchy, and user-space representation
2. **kset** — A container for kobjects of a similar type
3. **sysfs** — A virtual filesystem exposing kernel objects to user space (`/sys`)
4. **uevent** — Mechanism to notify user space (udev) of kernel object changes

### What Problem Does It Solve?

| Problem | Solution |
|---------|----------|
| **Object lifetime** | Reference counting via `kref` embedded in `kobject` |
| **Hierarchy** | Parent/child relationships visible in `/sys` tree |
| **User visibility** | sysfs exposes kernel state without custom ioctls |
| **Hot-plug** | uevents notify udev of device add/remove/change |
| **Attributes** | Standardized read/write interfaces via sysfs files |
| **Object grouping** | ksets group related kobjects for collective management |

### Where It Sits in the Kernel Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              USER SPACE                                      │
│  ┌────────────────────────────────────────────────────────────────────────┐ │
│  │   Applications                                                          │ │
│  │     ls /sys/devices/pci0000:00/0000:00:1f.2/                           │ │
│  │     cat /sys/class/net/eth0/address                                    │ │
│  │     echo 1 > /sys/block/sda/queue/scheduler                            │ │
│  └────────────────────────────────┬───────────────────────────────────────┘ │
│                                   │                                          │
│  ┌────────────────────────────────▼───────────────────────────────────────┐ │
│  │   udevd (device manager)                                                │ │
│  │     - Receives uevents via netlink                                      │ │
│  │     - Creates /dev nodes                                                │ │
│  │     - Runs rules to set permissions, symlinks                           │ │
│  └────────────────────────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────────────────────────┘
                                    │
════════════════════════════════════│═══════════════════════════════════════════
              System Calls          │  Netlink socket
                                    │  (KOBJECT_UEVENT)
                                    ▼
┌──────────────────────────────────────────────────────────────────────────────┐
│                              KERNEL SPACE                                    │
│                                                                              │
│  ┌─────────────────────────────────────────────────────────────────────────┐│
│  │                        VFS LAYER                                         ││
│  │    open("/sys/devices/pci0000:00/device")                               ││
│  │         └── sysfs file operations                                       ││
│  └────────────────────────────────┬────────────────────────────────────────┘│
│                                   │                                          │
│  ┌────────────────────────────────▼────────────────────────────────────────┐│
│  │                       SYSFS FILESYSTEM                                   ││
│  │                       (fs/sysfs/)                                        ││
│  │  ┌──────────────────────────────────────────────────────────────────┐   ││
│  │  │  struct sysfs_dirent                                              │   ││
│  │  │    - s_name (directory/file name)                                 │   ││
│  │  │    - s_parent (parent dirent)                                     │   ││
│  │  │    - s_dir.kobj (back-pointer to kobject)                        │   ││
│  │  │    - s_attr.attr (for attribute files)                           │   ││
│  │  │    - Red-black trees for children lookup                         │   ││
│  │  └──────────────────────────────────────────────────────────────────┘   ││
│  └────────────────────────────────┬────────────────────────────────────────┘│
│                                   │                                          │
│  ┌────────────────────────────────▼────────────────────────────────────────┐│
│  │                       KOBJECT/KSET CORE                                  ││
│  │                       (lib/kobject.c)                                    ││
│  │                                                                          ││
│  │   struct kobject {                    struct kset {                      ││
│  │     name                                list (of kobjects)               ││
│  │     entry (in kset list)               kobj (embedded kobject)           ││
│  │     parent (kobject *)                 uevent_ops (callbacks)            ││
│  │     kset   (kset *)                  }                                   ││
│  │     ktype  (sysfs_ops, release)                                          ││
│  │     sd     (sysfs_dirent *)         struct kobj_type {                   ││
│  │     kref   (reference count)           release()                         ││
│  │   }                                    sysfs_ops (show/store)            ││
│  │                                        default_attrs[]                   ││
│  │                                      }                                   ││
│  └────────────────────────────────┬────────────────────────────────────────┘│
│                                   │                                          │
│  ┌────────────────────────────────▼────────────────────────────────────────┐│
│  │                    DRIVER MODEL CONSUMERS                                ││
│  │                                                                          ││
│  │  ┌────────────────┐  ┌────────────────┐  ┌────────────────────────────┐ ││
│  │  │  struct device │  │  struct bus_   │  │  struct device_driver      │ ││
│  │  │    kobj        │  │    type        │  │    kobj                    │ ││
│  │  │    parent      │  │    p->subsys   │  │    bus                     │ ││
│  │  │    bus         │  │      ->kobj    │  │    (methods)               │ ││
│  │  │    driver      │  │    (methods)   │  │                            │ ││
│  │  └────────────────┘  └────────────────┘  └────────────────────────────┘ ││
│  │                                                                          ││
│  │  ┌────────────────┐  ┌────────────────┐                                 ││
│  │  │  struct class  │  │  Block layer   │   (and many more subsystems)   ││
│  │  │    p->subsys   │  │  Network layer │                                 ││
│  │  │      ->kobj    │  │  ...           │                                 ││
│  │  └────────────────┘  └────────────────┘                                 ││
│  └─────────────────────────────────────────────────────────────────────────┘│
└──────────────────────────────────────────────────────────────────────────────┘
```

### Sysfs Directory Structure

```
/sys/
├── block/                     → Block devices (disks, partitions)
├── bus/                       → Bus types (pci, usb, platform, etc.)
│   ├── pci/
│   │   ├── devices/           → Symlinks to devices
│   │   └── drivers/           → Drivers bound to this bus
│   └── usb/
├── class/                     → Device classes (net, tty, block, etc.)
│   ├── net/
│   │   ├── eth0 -> ../../devices/pci.../net/eth0
│   │   └── lo -> ../../devices/virtual/net/lo
│   └── tty/
├── devices/                   → Physical device hierarchy
│   ├── pci0000:00/
│   │   └── 0000:00:1f.2/      → A PCI device
│   │       ├── vendor
│   │       ├── device
│   │       ├── driver -> ../../../bus/pci/drivers/ahci
│   │       └── net/
│   │           └── eth0/
│   ├── platform/
│   └── virtual/
├── firmware/                  → Firmware objects
├── fs/                        → Filesystem parameters
├── kernel/                    → Kernel tunables
│   ├── mm/
│   └── debug/
├── module/                    → Loaded modules
└── power/                     → System power management
```

### How This Subsystem Interacts with Others

| Adjacent Subsystem | Interaction |
|-------------------|-------------|
| **VFS** | sysfs is a filesystem; uses VFS interfaces |
| **Driver Core** | `struct device`, `struct bus_type` embed kobjects |
| **Block Layer** | `struct gendisk` has embedded kobject for `/sys/block/` |
| **Network** | `struct net_device` creates entries in `/sys/class/net/` |
| **Power Management** | Uses sysfs for PM state control |
| **Module Loader** | Module parameters exposed via `/sys/module/` |
| **udev** | Receives uevents to create `/dev` nodes |

---

## 2. Directory & File Map (Code Navigation)

### Primary Directories

```
lib/
├── kobject.c                  → Core kobject implementation
│                                 - kobject_init(), kobject_add()
│                                 - kobject_get(), kobject_put()
│                                 - kobject_del(), kobject_cleanup()
│                                 - kset_init(), kset_register()
│                                 - kobject_create_and_add()
│
├── kobject_uevent.c           → Uevent delivery to user space
│                                 - kobject_uevent(), kobject_uevent_env()
│                                 - Netlink broadcast to udev
│                                 - /sbin/hotplug helper (legacy)
│
└── kref.c                     → Generic reference counting
                                  - kref_init(), kref_get(), kref_put()

include/linux/
├── kobject.h                  → kobject, kset, kobj_type definitions
│                                 - struct kobject
│                                 - struct kset
│                                 - struct kobj_type
│                                 - struct kobj_attribute
│                                 - enum kobject_action (KOBJ_ADD, etc.)
│
├── sysfs.h                    → sysfs API declarations
│                                 - struct attribute
│                                 - struct attribute_group
│                                 - struct bin_attribute
│                                 - struct sysfs_ops
│                                 - sysfs_create_file(), sysfs_remove_file()
│                                 - sysfs_create_group(), sysfs_create_link()
│
├── kref.h                     → struct kref definition
│
└── device.h                   → Driver model structures
                                  - struct device (embeds kobject)
                                  - struct bus_type
                                  - struct device_driver
                                  - struct class

fs/sysfs/
├── mount.c                    → Filesystem registration and mounting
│                                 - sysfs_init() — called at boot
│                                 - sysfs_fill_super() — superblock setup
│                                 - sysfs_root — the root sysfs_dirent
│
├── dir.c                      → Directory operations
│                                 - sysfs_create_dir() — create dir for kobject
│                                 - sysfs_remove_dir() — remove dir
│                                 - sysfs_dirent management
│                                 - Red-black tree for child lookup
│                                 - sysfs_get_active(), sysfs_put_active()
│
├── file.c                     → Regular file operations (attributes)
│                                 - sysfs_create_file() — create attr file
│                                 - sysfs_read_file() — read attribute
│                                 - sysfs_write_file() — write attribute
│                                 - fill_read_buffer() — calls show()
│                                 - flush_write_buffer() — calls store()
│
├── bin.c                      → Binary attribute files
│                                 - For large data (firmware, EEPROM, etc.)
│                                 - mmap support
│
├── symlink.c                  → Symbolic link operations
│                                 - sysfs_create_link()
│                                 - sysfs_remove_link()
│
├── group.c                    → Attribute group management
│                                 - sysfs_create_group()
│                                 - sysfs_remove_group()
│
├── inode.c                    → Inode operations
│                                 - sysfs_get_inode()
│                                 - sysfs_setattr()
│
└── sysfs.h                    → Internal sysfs header
                                  - struct sysfs_dirent (THE key structure)
                                  - struct sysfs_elem_dir
                                  - struct sysfs_elem_attr

drivers/base/
├── core.c                     → struct device implementation
│                                 - device_register(), device_add()
│                                 - device_del(), device_unregister()
│                                 - device_release() — kobject release callback
│
├── bus.c                      → struct bus_type implementation
│                                 - bus_register(), bus_unregister()
│                                 - bus_add_device(), bus_probe_device()
│
├── driver.c                   → struct device_driver implementation
│                                 - driver_register(), driver_unregister()
│
├── class.c                    → struct class implementation
│                                 - class_register(), class_unregister()
│
└── init.c                     → Driver core initialization
                                  - driver_init() — called early in boot
```

### Why Is the Code Split This Way?

1. **`lib/kobject.c`**: Core object model — no dependency on filesystems
2. **`fs/sysfs/`**: Filesystem representation — bridges kobjects to VFS
3. **`drivers/base/`**: Higher-level abstractions (device, bus, driver) built on kobject
4. **Separation of concerns**: kobject can exist without sysfs (CONFIG_SYSFS=n)

---

## 3. Core Data Structures

### 3.1 struct kobject — The Universal Kernel Object

**Location**: `include/linux/kobject.h`

```c
struct kobject {
    const char          *name;              /* Object name (sysfs dir name) */
    struct list_head    entry;              /* Link in kset's list */
    struct kobject      *parent;            /* Parent kobject */
    struct kset         *kset;              /* Containing kset (if any) */
    struct kobj_type    *ktype;             /* Type info (ops, release) */
    struct sysfs_dirent *sd;                /* sysfs directory entry */
    struct kref         kref;               /* Reference count */
    
    /* State flags */
    unsigned int state_initialized:1;       /* kobject_init() called */
    unsigned int state_in_sysfs:1;          /* kobject_add() succeeded */
    unsigned int state_add_uevent_sent:1;   /* KOBJ_ADD sent */
    unsigned int state_remove_uevent_sent:1;/* KOBJ_REMOVE sent */
    unsigned int uevent_suppress:1;         /* Don't send uevents */
};
```

**Memory Layout**:
```
┌─────────────────────────────────────────────────────────────────────────┐
│                         struct kobject                                   │
├─────────────────────────────────────────────────────────────────────────┤
│  name ─────────────────────► "my_device" (dynamically allocated string) │
│                                                                          │
│  entry ◄──────────────────────────────────────────────────────────────► │
│    (list_head)               ▲                                           │
│                              │ (linked in kset->list)                    │
│                              ▼                                           │
│  parent ───────────────────► [parent kobject] ──► /sys/devices/          │
│                                                                          │
│  kset ─────────────────────► [struct kset] (e.g., devices_kset)          │
│                                                                          │
│  ktype ────────────────────► [struct kobj_type]                          │
│                                  ├── release() function                  │
│                                  ├── sysfs_ops (show/store)              │
│                                  └── default_attrs[]                     │
│                                                                          │
│  sd ───────────────────────► [struct sysfs_dirent]                       │
│                                  └── /sys representation                 │
│                                                                          │
│  kref ─────────────────────► atomic_t refcount = 1                       │
│                                                                          │
│  [state bits: initialized=1, in_sysfs=1, ...]                           │
└─────────────────────────────────────────────────────────────────────────┘
```

**Allocation and Lifetime**:

```c
// Option 1: Static kobject (embedded in larger structure)
struct my_device {
    /* ... device fields ... */
    struct kobject kobj;  /* Embedded kobject */
};

// Allocation: The containing structure is allocated
// Lifetime: Controlled by kobj's kref
// Release: kobj_type->release() called when refcount hits 0
//          → release() typically does container_of() and kfree()

// Option 2: Dynamic kobject
struct kobject *kobj = kobject_create();  // Allocates with kzalloc
// Lifetime: Managed entirely by kobject_put()
// Release: Uses dynamic_kobj_release() which just kfree()s
```

**Key Invariants**:

1. **Initialization before use**: `state_initialized` must be 1 before `kobject_add()`
2. **Reference counting**: Never access kobject after last `kobject_put()`
3. **Parent holds child**: Parent's refcount keeps child accessible
4. **ktype required**: Must have a `kobj_type` with at least a `release()` function
5. **Name required for sysfs**: `kobject_add()` needs a non-empty name

### 3.2 struct kref — Reference Counter

**Location**: `include/linux/kref.h`

```c
struct kref {
    atomic_t refcount;
};
```

**Operations**:

```c
void kref_init(struct kref *kref);           // Set refcount = 1
void kref_get(struct kref *kref);            // refcount++
int kref_put(struct kref *kref,              // refcount--; if 0, call release
             void (*release)(struct kref *));
```

### 3.3 struct kobj_type — Type Information

**Location**: `include/linux/kobject.h`

```c
struct kobj_type {
    void (*release)(struct kobject *kobj);     /* Destructor */
    const struct sysfs_ops *sysfs_ops;         /* show/store callbacks */
    struct attribute **default_attrs;           /* Auto-created attributes */
    const struct kobj_ns_type_operations *(*child_ns_type)(struct kobject *kobj);
    const void *(*namespace)(struct kobject *kobj);
};
```

**Purpose**:

- **release()**: Called when kobject's refcount reaches 0; must free the object
- **sysfs_ops**: How to read/write attribute files
- **default_attrs**: Array of attributes created automatically on `kobject_add()`

### 3.4 struct kset — Kobject Container

**Location**: `include/linux/kobject.h`

```c
struct kset {
    struct list_head list;             /* List of member kobjects */
    spinlock_t list_lock;              /* Protects list */
    struct kobject kobj;               /* Embedded kobject (kset IS a kobject) */
    const struct kset_uevent_ops *uevent_ops;  /* Uevent filter/customize */
};
```

**Relationship Diagram**:
```
                              struct kset
                    ┌─────────────────────────────┐
                    │  list_head ──────────────┐  │
                    │  list_lock               │  │
                    │  kobj (embedded) ────┐   │  │
                    │  uevent_ops          │   │  │
                    └──────────────────────│───│──┘
                                           │   │
                                           │   │
            ┌──────────────────────────────│───│───────────────────────┐
            │                              │   │                       │
            ▼                              ▼   ▼                       ▼
    ┌───────────────┐              ┌───────────────┐           ┌───────────────┐
    │ struct kobject │◄────────────│ struct kobject │──────────►│ struct kobject │
    │   name="dev1"  │   entry     │   name="dev2"  │   entry   │   name="dev3"  │
    │   kset=&kset   │   (list)    │   kset=&kset   │   (list)  │   kset=&kset   │
    │   parent=...   │             │   parent=...   │           │   parent=...   │
    └───────────────┘              └───────────────┘           └───────────────┘
```

### 3.5 struct sysfs_dirent — Sysfs Internal Node

**Location**: `fs/sysfs/sysfs.h`

```c
struct sysfs_dirent {
    atomic_t            s_count;        /* Reference count */
    atomic_t            s_active;       /* Active reference (for file access) */
    struct sysfs_dirent *s_parent;      /* Parent dirent */
    const char          *s_name;        /* Name (filename) */
    
    struct rb_node      inode_node;     /* RB tree by inode number */
    struct rb_node      name_node;      /* RB tree by name */
    
    union {
        struct completion    *completion;   /* For synchronization */
        struct sysfs_dirent  *removed_list; /* Removed dirents chain */
    } u;
    
    const void          *s_ns;          /* Namespace tag */
    
    /* Type-specific data */
    union {
        struct sysfs_elem_dir       s_dir;      /* For directories */
        struct sysfs_elem_symlink   s_symlink;  /* For symlinks */
        struct sysfs_elem_attr      s_attr;     /* For attributes */
        struct sysfs_elem_bin_attr  s_bin_attr; /* For binary attrs */
    };
    
    unsigned int        s_flags;        /* Type and flags */
    unsigned short      s_mode;         /* File mode */
    ino_t               s_ino;          /* Inode number */
    struct sysfs_inode_attrs *s_iattr;  /* Optional: owner/permissions */
};

/* Type-specific element for directories */
struct sysfs_elem_dir {
    struct kobject      *kobj;          /* Back-pointer to kobject */
    unsigned long       subdirs;        /* Number of subdirectories */
    struct rb_root      inode_tree;     /* Children by inode */
    struct rb_root      name_tree;      /* Children by name */
};

/* Type-specific element for attributes */
struct sysfs_elem_attr {
    struct attribute    *attr;          /* The attribute */
    struct sysfs_open_dirent *open;     /* Open file tracking */
};
```

**Two-Level Reference Counting**:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                     sysfs_dirent Reference Counting                          │
│                                                                              │
│   s_count (struct reference)                                                 │
│   ─────────────────────────────                                              │
│   - Keeps the sysfs_dirent structure alive                                   │
│   - Obtained via sysfs_get(sd)                                               │
│   - Released via sysfs_put(sd)                                               │
│   - When reaches 0, structure is freed                                       │
│                                                                              │
│   s_active (access reference)                                                │
│   ─────────────────────────────                                              │
│   - Keeps the backing kobject/attribute valid                                │
│   - Obtained via sysfs_get_active(sd)                                        │
│   - Released via sysfs_put_active(sd)                                        │
│   - Required before accessing sd->s_attr.attr or sd->s_dir.kobj              │
│   - Negative value means deactivated (being removed)                         │
│   - Prevents use-after-free during kobject_del()                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 3.6 struct attribute — Sysfs File Definition

**Location**: `include/linux/sysfs.h`

```c
struct attribute {
    const char          *name;      /* Filename in sysfs */
    mode_t              mode;       /* File permissions (0644, etc.) */
#ifdef CONFIG_DEBUG_LOCK_ALLOC
    struct lock_class_key *key;     /* Lockdep annotation */
    struct lock_class_key skey;
#endif
};

struct sysfs_ops {
    ssize_t (*show)(struct kobject *, struct attribute *, char *);
    ssize_t (*store)(struct kobject *, struct attribute *, const char *, size_t);
    const void *(*namespace)(struct kobject *, const struct attribute *);
};
```

### 3.7 struct device — High-Level Device (Embeds kobject)

**Location**: `include/linux/device.h`

```c
struct device {
    struct device        *parent;           /* Parent device */
    struct device_private *p;               /* Private data */
    
    struct kobject       kobj;              /* EMBEDDED kobject */
    const char           *init_name;        /* Initial name */
    const struct device_type *type;         /* Device type */
    
    struct mutex         mutex;             /* Device lock */
    struct bus_type      *bus;              /* Bus this device is on */
    struct device_driver *driver;           /* Bound driver */
    void                 *platform_data;    /* Platform-specific data */
    
    struct dev_pm_info   power;             /* Power management */
    dev_t                devt;              /* Device number (for /dev) */
    
    struct class         *class;            /* Device class */
    const struct attribute_group **groups;  /* Attribute groups */
    
    void (*release)(struct device *dev);    /* Destructor */
};
```

**Container Relationship**:
```
┌─────────────────────────────────────────────────────────────────────┐
│                      struct device                                  │
│  ┌─────────────────────────────────────────────────────────────┐    │
│  │                                                             │    │
│  │   parent ────────────────────────► [parent device]          │    │
│  │                                                             │    │
│  │   kobj ◄─────────────────────────────────────────────────┐  │    │
│  │     │                                                    │  │    │
│  │     │ (struct kobject is EMBEDDED, not a pointer)        │  │    │
│  │     │                                                    │  │    │
│  │     └── kobj.ktype = &device_ktype                       │  │    │
│  │              └── release = device_release()              │  │    │
│  │                       └── calls dev->release()           │  │    │
│  │                                                             │    │
│  │   bus ───────────────────────────► [struct bus_type]        │    │
│  │   driver ────────────────────────► [struct device_driver]   │    │
│  │   class ─────────────────────────► [struct class]           │    │
│  │                                                             │    │
│  └─────────────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────────────┘

/* Accessing device from kobject */
#define to_dev(obj) container_of(obj, struct device, kobj)
```

---

## 4. Entry Points & Call Paths

### 4.1 Kobject Lifecycle

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        KOBJECT LIFECYCLE                                     │
│                                                                              │
│   ┌─────────────────┐                                                        │
│   │   Uninitialized │                                                        │
│   └────────┬────────┘                                                        │
│            │                                                                 │
│            │ kobject_init(kobj, ktype)                                       │
│            │   - Sets kobj->ktype = ktype                                    │
│            │   - kref_init(&kobj->kref) → refcount = 1                       │
│            │   - state_initialized = 1                                       │
│            ▼                                                                 │
│   ┌─────────────────┐                                                        │
│   │   Initialized   │  (refcount = 1, not in sysfs)                          │
│   └────────┬────────┘                                                        │
│            │                                                                 │
│            │ kobject_add(kobj, parent, "name")                               │
│            │   - Sets kobj->name                                             │
│            │   - kobj->parent = parent                                       │
│            │   - If kset: add to kset->list                                  │
│            │   - sysfs_create_dir(kobj) → creates /sys entry                 │
│            │   - populate_dir() → creates default_attrs                      │
│            │   - state_in_sysfs = 1                                          │
│            ▼                                                                 │
│   ┌─────────────────┐                                                        │
│   │  In Hierarchy   │  (visible in /sys)                                     │
│   └────────┬────────┘                                                        │
│            │                                                                 │
│            │ kobject_uevent(kobj, KOBJ_ADD)                                  │
│            │   - Sends netlink to udev                                       │
│            │   - state_add_uevent_sent = 1                                   │
│            ▼                                                                 │
│   ┌─────────────────┐                                                        │
│   │     Active      │  (fully registered, udev notified)                     │
│   └────────┬────────┘                                                        │
│            │                                                                 │
│            │ kobject_get(kobj) / kobject_put(kobj)                           │
│            │   - Increments/decrements refcount                              │
│            │   - Normal usage by subsystems                                  │
│            │                                                                 │
│            │ kobject_del(kobj)                                               │
│            │   - sysfs_remove_dir(kobj)                                      │
│            │   - state_in_sysfs = 0                                          │
│            │   - Removes from kset list                                      │
│            │   - kobject_put(parent)                                         │
│            ▼                                                                 │
│   ┌─────────────────┐                                                        │
│   │    Deleted      │  (not in sysfs, refcount > 0)                          │
│   └────────┬────────┘                                                        │
│            │                                                                 │
│            │ kobject_put(kobj) → refcount reaches 0                          │
│            │   - kobject_cleanup()                                           │
│            │     - If still in sysfs: auto-remove                            │
│            │     - If uevent not sent: auto-send KOBJ_REMOVE                 │
│            │     - ktype->release(kobj) → frees memory                       │
│            ▼                                                                 │
│   ┌─────────────────┐                                                        │
│   │     Freed       │  (memory returned to allocator)                        │
│   └─────────────────┘                                                        │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 4.2 Device Registration Path

```
driver_register(&my_driver)
       │
       ▼
device_register(dev)                              [drivers/base/core.c]
       │
       ├── device_initialize(dev)
       │       │
       │       ├── dev->kobj.kset = devices_kset
       │       ├── kobject_init(&dev->kobj, &device_ktype)
       │       └── Initialize various fields
       │
       └── device_add(dev)
               │
               ├── dev_set_name(dev, "name")
               │       └── kobject_set_name_vargs(&dev->kobj, ...)
               │
               ├── setup_parent(dev, parent)
               │       └── Sets dev->kobj.parent
               │
               ├── kobject_add(&dev->kobj, parent_kobj, NULL)
               │       │
               │       └── kobject_add_internal(kobj)
               │               │
               │               ├── kobj_kset_join(kobj)
               │               │       └── Add to kset list
               │               │
               │               └── create_dir(kobj)
               │                       │
               │                       ├── sysfs_create_dir(kobj)
               │                       │       └── Creates /sys/devices/.../name/
               │                       │
               │                       └── populate_dir(kobj)
               │                               └── Creates default attribute files
               │
               ├── device_create_file(dev, &uevent_attr)
               │       └── Creates "uevent" file
               │
               ├── device_create_file(dev, &devt_attr)  [if MAJOR(dev->devt)]
               │       └── Creates "dev" file (major:minor)
               │
               ├── device_add_class_symlinks(dev)
               │       └── Creates symlinks in /sys/class/
               │
               ├── bus_add_device(dev)
               │       └── Adds to bus device list
               │
               ├── bus_probe_device(dev)
               │       └── Tries to match with drivers
               │
               └── kobject_uevent(&dev->kobj, KOBJ_ADD)
                       │
                       └── kobject_uevent_env(kobj, KOBJ_ADD, NULL)
                               │
                               ├── Build environment (DEVPATH, SUBSYSTEM, etc.)
                               ├── kset->uevent_ops->uevent() if exists
                               ├── netlink_broadcast() to udev
                               └── call_usermodehelper(/sbin/hotplug) [legacy]
```

### 4.3 Sysfs Attribute Read Path

```
User: cat /sys/devices/pci0000:00/0000:00:1f.2/vendor
       │
       ▼
sys_open("/sys/devices/pci0000:00/0000:00:1f.2/vendor", O_RDONLY)
       │
       ▼
VFS lookup → sysfs_lookup()                       [fs/sysfs/dir.c]
       │
       └── Find sysfs_dirent for "vendor"
       │
       ▼
sys_read(fd, buf, count)
       │
       ▼
sysfs_read_file(file, buf, count, ppos)           [fs/sysfs/file.c]
       │
       ├── buffer = file->private_data
       │
       ├── mutex_lock(&buffer->mutex)
       │
       ├── fill_read_buffer(dentry, buffer)
       │       │
       │       ├── attr_sd = dentry->d_fsdata        (sysfs_dirent)
       │       ├── kobj = attr_sd->s_parent->s_dir.kobj
       │       ├── ops = buffer->ops                  (sysfs_ops)
       │       │
       │       ├── sysfs_get_active(attr_sd)         ◄── Active reference!
       │       │       └── Prevents kobject removal during read
       │       │
       │       ├── count = ops->show(kobj, attr, buffer->page)
       │       │       │
       │       │       └── For device attributes:
       │       │           dev_attr_show(kobj, attr, buf)
       │       │               │
       │       │               ├── dev = container_of(kobj, struct device, kobj)
       │       │               ├── dev_attr = container_of(attr, ...)
       │       │               │
       │       │               └── dev_attr->show(dev, dev_attr, buf)
       │       │                       │
       │       │                       └── Driver's show function
       │       │                           e.g., returns "0x8086\n"
       │       │
       │       └── sysfs_put_active(attr_sd)
       │
       ├── simple_read_from_buffer(user_buf, count, ppos, buffer->page, ...)
       │       └── Copy to user space
       │
       └── mutex_unlock(&buffer->mutex)
       │
       ▼
Returns bytes read (e.g., "0x8086\n" = 7 bytes)
```

### 4.4 Uevent Delivery Path

```
kobject_uevent(&dev->kobj, KOBJ_ADD)
       │
       ▼
kobject_uevent_env(kobj, KOBJ_ADD, NULL)          [lib/kobject_uevent.c]
       │
       ├── Find top-level kset (for subsystem name)
       │       top_kobj = kobj;
       │       while (!top_kobj->kset && top_kobj->parent)
       │           top_kobj = top_kobj->parent;
       │       kset = top_kobj->kset;
       │
       ├── kset->uevent_ops->filter() — Can suppress uevent
       │
       ├── Build environment variables:
       │       ACTION=add
       │       DEVPATH=/devices/pci0000:00/0000:00:1f.2
       │       SUBSYSTEM=pci
       │       SEQNUM=1234
       │       [device-specific variables...]
       │
       ├── kset->uevent_ops->uevent() — Add custom env vars
       │
       ├── #ifdef CONFIG_NET
       │   │
       │   └── For each uevent_sock in uevent_sock_list:
       │           netlink_broadcast_filtered(sk, skb, ...)
       │               │
       │               └── Delivers to listening sockets (udevd)
       │
       └── #if defined(CONFIG_HOTPLUG)
           │
           └── If uevent_helper[0] != '\0':
                   call_usermodehelper(uevent_helper, ...)
                       │
                       └── Executes /sbin/hotplug (legacy)
```

---

## 5. Core Workflows (Code-Driven)

### 5.1 Sysfs Initialization (Boot)

```c
// Called from init/main.c:start_kernel()
void __init driver_init(void)
{
    // ... other init ...
    
    // Initialize sysfs - creates /sys
    sysfs_init();  // fs/sysfs/mount.c
}

// fs/sysfs/mount.c
int __init sysfs_init(void)
{
    // 1. Create slab cache for sysfs_dirent allocation
    sysfs_dir_cachep = kmem_cache_create("sysfs_dir_cache",
                                          sizeof(struct sysfs_dirent),
                                          0, 0, NULL);
    
    // 2. Initialize inode infrastructure
    sysfs_inode_init();
    
    // 3. Register sysfs filesystem type
    register_filesystem(&sysfs_fs_type);
    
    // 4. Mount sysfs internally (kernel mount)
    sysfs_mnt = kern_mount(&sysfs_fs_type);
    
    return 0;
}
```

### 5.2 Creating a Simple Kobject

```c
/* Example: Create /sys/kernel/my_kobj/ with attributes */

// Define an attribute
static ssize_t my_show(struct kobject *kobj, struct kobj_attribute *attr,
                       char *buf)
{
    return sprintf(buf, "42\n");
}

static ssize_t my_store(struct kobject *kobj, struct kobj_attribute *attr,
                        const char *buf, size_t count)
{
    /* Parse and store value */
    return count;
}

static struct kobj_attribute my_attr = __ATTR(my_value, 0644, my_show, my_store);

static struct attribute *my_attrs[] = {
    &my_attr.attr,
    NULL,
};

static struct attribute_group my_group = {
    .attrs = my_attrs,
};

// Module init
static struct kobject *my_kobj;

static int __init my_init(void)
{
    int ret;
    
    // 1. Create kobject under /sys/kernel/
    my_kobj = kobject_create_and_add("my_kobj", kernel_kobj);
    if (!my_kobj)
        return -ENOMEM;
    
    // 2. Create attribute files
    ret = sysfs_create_group(my_kobj, &my_group);
    if (ret) {
        kobject_put(my_kobj);
        return ret;
    }
    
    // Result: /sys/kernel/my_kobj/my_value exists
    return 0;
}

static void __exit my_exit(void)
{
    kobject_put(my_kobj);  // This handles all cleanup
}
```

### 5.3 Kobject Cleanup Path

```c
// When refcount reaches 0:
void kobject_put(struct kobject *kobj)
{
    if (kobj) {
        WARN_ON(!kobj->state_initialized);
        kref_put(&kobj->kref, kobject_release);
    }
}

// kref callback:
static void kobject_release(struct kref *kref)
{
    kobject_cleanup(container_of(kref, struct kobject, kref));
}

// Actual cleanup:
static void kobject_cleanup(struct kobject *kobj)
{
    struct kobj_type *t = get_ktype(kobj);
    
    // 1. Auto-send KOBJ_REMOVE if ADD was sent but not REMOVE
    if (kobj->state_add_uevent_sent && !kobj->state_remove_uevent_sent) {
        kobject_uevent(kobj, KOBJ_REMOVE);
    }
    
    // 2. Auto-remove from sysfs if still there
    if (kobj->state_in_sysfs) {
        kobject_del(kobj);
    }
    
    // 3. Call type's release function
    if (t && t->release) {
        t->release(kobj);  // ← Frees containing structure
    }
    
    // 4. Free dynamically allocated name
    kfree(kobj->name);
}
```

### 5.4 Error Handling: kobject_add Failure

```c
int my_register(struct my_device *dev)
{
    int ret;
    
    // Initialize
    kobject_init(&dev->kobj, &my_ktype);
    
    // Try to add
    ret = kobject_add(&dev->kobj, parent, "dev%d", dev->id);
    if (ret) {
        // MUST call kobject_put even on add failure!
        // kobject_init incremented refcount, so we must decrement
        kobject_put(&dev->kobj);
        return ret;
    }
    
    // Success - send uevent
    kobject_uevent(&dev->kobj, KOBJ_ADD);
    return 0;
}
```

---

## 6. Important Algorithms & Mechanisms

### 6.1 Reference Counting with kref

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         kref Reference Counting                             │
│                                                                             │
│   ┌──────────────────────────────────────────────────────────────────────┐  │
│   │  kref_init(&kref)                                                    │  │
│   │      └── atomic_set(&kref->refcount, 1)                              │  │
│   │                                                                      │  │
│   │  kref_get(&kref)                                                     │  │
│   │      └── atomic_inc(&kref->refcount)                                 │  │
│   │          └── WARN_ON(refcount == 0)  ← Can't get dead object         │  │
│   │                                                                      │  │
│   │  kref_put(&kref, release)                                            │  │
│   │      └── if (atomic_dec_and_test(&kref->refcount))                   │  │
│   │              release(&kref)  ← Called when hits 0                    │  │
│   │              return 1                                                │  │
│   │          return 0                                                    │  │
│   └──────────────────────────────────────────────────────────────────────┘  │
│                                                                             │
│   Usage Pattern:                                                            │
│   ─────────────────                                                         │
│                                                                             │
│   kobject_init()        refcount = 1                                        │
│        │                                                                    │
│        ▼                                                                    │
│   kobject_add()         (doesn't change refcount)                           │
│        │                                                                    │
│        ├── User A: kobject_get()     refcount = 2                           │
│        │                                                                    │
│        ├── User B: kobject_get()     refcount = 3                           │
│        │                                                                    │
│        ├── User A: kobject_put()     refcount = 2                           │
│        │                                                                    │
│        ├── Creator: kobject_put()    refcount = 1                           │
│        │                                                                    │
│        └── User B: kobject_put()     refcount = 0 → release() called        │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 6.2 Sysfs Active Reference (s_active)

The `s_active` counter prevents use-after-free during concurrent access:

```c
// File access needs active reference
static int fill_read_buffer(struct dentry *dentry, struct sysfs_buffer *buffer)
{
    struct sysfs_dirent *attr_sd = dentry->d_fsdata;
    
    // Get active reference - may fail if kobject being removed
    if (!sysfs_get_active(attr_sd))
        return -ENODEV;
    
    // Now safe to access kobject
    count = ops->show(kobj, attr, buffer->page);
    
    sysfs_put_active(attr_sd);
    return count;
}

// During kobject removal
void sysfs_deactivate(struct sysfs_dirent *sd)
{
    DECLARE_COMPLETION_ONSTACK(wait);
    int v;
    
    // Mark as deactivating (negative value)
    v = atomic_add_return(SD_DEACTIVATED_BIAS, &sd->s_active);
    
    if (v != SD_DEACTIVATED_BIAS) {
        // There are active users - must wait
        sd->u.completion = &wait;
        wait_for_completion(&wait);  // Block until all users done
    }
}
```

### 6.3 Sysfs Directory Red-Black Trees

Sysfs uses two RB-trees for efficient child lookup:

```
                    Parent sysfs_dirent
                    ┌────────────────────────────────────────────┐
                    │  s_dir.inode_tree  (RB tree by inode #)   │
                    │  s_dir.name_tree   (RB tree by name)      │
                    └────────────────┬───────────────────────────┘
                                     │
            ┌────────────────────────┼────────────────────────┐
            │                        │                        │
            ▼                        ▼                        ▼
    ┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
    │ sysfs_dirent    │     │ sysfs_dirent    │     │ sysfs_dirent    │
    │ s_name="addr"   │     │ s_name="dev"    │     │ s_name="vendor" │
    │ s_ino=1001      │     │ s_ino=1002      │     │ s_ino=1003      │
    │ inode_node ──┐  │     │ inode_node ──┐  │     │ inode_node ──┐  │
    │ name_node ───│──┤     │ name_node ───│──┤     │ name_node ───│──┤
    └──────────────│──┘     └──────────────│──┘     └──────────────│──┘
                   │                       │                       │
                   └───────────────────────┴───────────────────────┘
                              (RB tree linkage)

Benefits:
- O(log n) lookup by name (for path resolution)
- O(log n) lookup by inode (for readdir)
- Handles large directories efficiently
```

### 6.4 Uevent Netlink Delivery

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                       Uevent Delivery Mechanism                              │
│                                                                              │
│   Kernel                                  User Space                         │
│   ──────                                  ──────────                         │
│                                                                              │
│   kobject_uevent()                                                           │
│        │                                                                     │
│        ▼                                                                     │
│   Build skb with:                         udevd                              │
│     ACTION=add                               │                               │
│     DEVPATH=/devices/...                     │                               │
│     SUBSYSTEM=pci                            │                               │
│     SEQNUM=1234                              │                               │
│        │                                     │                               │
│        ▼                                     │                               │
│   netlink_broadcast()  ─────────────────────►│ recv() on NETLINK_KOBJECT_UEVENT
│        │                                     │                               │
│        │                                     ▼                               │
│        │                              Parse uevent                           │
│        │                                     │                               │
│        │                                     ▼                               │
│        │                              Match rules                            │
│        │                                     │                               │
│        │                                     ▼                               │
│        │                              Actions:                               │
│        │                                - Create /dev/xxx                    │
│        │                                - Set permissions                    │
│        │                                - Run scripts                        │
│        │                                                                     │
│   Legacy fallback (if enabled):                                              │
│   call_usermodehelper("/sbin/hotplug")                                       │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 7. Concurrency & Synchronization

### 7.1 Lock Summary

| Lock | Type | Protects | Scope |
|------|------|----------|-------|
| `sysfs_mutex` | Mutex | sysfs_dirent tree structure | Global sysfs |
| `sysfs_assoc_lock` | Spinlock | kobject↔sysfs_dirent association | Global |
| `kset->list_lock` | Spinlock | kset member list | Per-kset |
| `sysfs_open_dirent_lock` | Spinlock | Open file tracking | Global |
| `buffer->mutex` | Mutex | Per-file read/write buffer | Per-open-file |

### 7.2 Sysfs Mutex Usage

```c
// fs/sysfs/dir.c
DEFINE_MUTEX(sysfs_mutex);

// Any sysfs tree modification requires sysfs_mutex
int sysfs_create_dir(struct kobject *kobj)
{
    // ... setup ...
    
    mutex_lock(&sysfs_mutex);
    
    // Add to parent's children
    sysfs_link_sibling(sd);  // Links into RB trees
    
    mutex_unlock(&sysfs_mutex);
    
    return 0;
}

void sysfs_remove_dir(struct kobject *kobj)
{
    mutex_lock(&sysfs_mutex);
    
    // Deactivate - waits for all active users
    sysfs_deactivate(sd);
    
    // Unlink from tree
    sysfs_unlink_sibling(sd);
    
    mutex_unlock(&sysfs_mutex);
}
```

### 7.3 Active Reference vs Structural Reference

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                   Two-Phase Locking in Sysfs                                 │
│                                                                              │
│   Phase 1: Structural (s_count)                                              │
│   ─────────────────────────────                                              │
│   - Keeps sysfs_dirent structure valid                                       │
│   - Obtained: sysfs_get(sd)                                                  │
│   - Released: sysfs_put(sd)                                                  │
│   - Can be held long-term                                                    │
│                                                                              │
│   Phase 2: Active (s_active)                                                 │
│   ─────────────────────────────                                              │
│   - Keeps backing kobject/attribute valid                                    │
│   - Obtained: sysfs_get_active(sd) — may return NULL!                        │
│   - Released: sysfs_put_active(sd)                                           │
│   - Must be held during show()/store() calls                                 │
│   - Prevents removal while in use                                            │
│                                                                              │
│   Removal sequence:                                                          │
│   ─────────────────────                                                      │
│   1. kobject_del() called                                                    │
│   2. sysfs_deactivate(sd) marks s_active negative                            │
│   3. New sysfs_get_active() calls fail (return NULL)                         │
│   4. Wait for existing active references to drop                             │
│   5. Now safe to free kobject                                                │
│                                                                              │
│   Why this matters:                                                          │
│   ─────────────────────                                                      │
│   Without active refs, this race could crash:                                │
│                                                                              │
│     CPU A                            CPU B                                   │
│     ─────                            ─────                                   │
│     read("/sys/device/attr")                                                 │
│       fill_read_buffer()                                                     │
│         // About to call show()      kobject_del()                           │
│                                        // frees device struct                │
│         ops->show(kobj, ...)         // kobj is now invalid!                 │
│           ← CRASH: use-after-free                                            │
│                                                                              │
│   With active refs:                                                          │
│                                                                              │
│     CPU A                            CPU B                                   │
│     ─────                            ─────                                   │
│     sysfs_get_active(sd)                                                     │
│       // s_active = 1                                                        │
│                                      kobject_del()                           │
│                                        sysfs_deactivate(sd)                  │
│                                          // s_active = -INT_MIN + 1          │
│                                          // Waits for s_active == -INT_MIN   │
│     ops->show(kobj, ...)             // Still waiting...                     │
│     sysfs_put_active(sd)                                                     │
│       // s_active = -INT_MIN                                                 │
│                                        // Now wakes up and proceeds          │
│                                        // Safe to free kobject               │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 7.4 What Breaks Without Proper Synchronization

1. **Missing sysfs_mutex**: Corrupted RB-trees, dangling pointers
2. **Missing s_active check**: Use-after-free when reading attributes during removal
3. **Missing kref**: Memory freed while still referenced
4. **Missing kset->list_lock**: Corrupt kset member list during iteration

---

## 8. Performance Considerations

### 8.1 Hot Paths vs Cold Paths

| Operation | Frequency | Path Type |
|-----------|-----------|-----------|
| `kobject_get()`/`put()` | Very frequent | Hot |
| Attribute read/write | Frequent | Hot |
| `kobject_add()`/`del()` | Infrequent | Cold |
| Uevent generation | Infrequent | Cold |
| Sysfs tree modification | Rare | Cold |

### 8.2 Caching in Sysfs File Access

```c
struct sysfs_buffer {
    char    *page;              /* Cached page for show() output */
    int     needs_read_fill;    /* Re-read on next access? */
};

// First read: calls show(), caches in page
// Subsequent reads: returns cached data
// Seek to 0: clears cache, next read re-calls show()
```

### 8.3 Sysfs RB-Tree Lookup

```
Directory with N children:
- Linear list: O(N) lookup
- RB-tree: O(log N) lookup

Benefits for large directories like /sys/devices/pci*/ with many devices
```

### 8.4 Atomic Operations in kref

```c
// kref uses atomic operations - lock-free for common case
void kref_get(struct kref *kref)
{
    WARN_ON(!atomic_read(&kref->refcount));
    atomic_inc(&kref->refcount);  // Single atomic instruction
}

int kref_put(struct kref *kref, void (*release)(struct kref *kref))
{
    if (atomic_dec_and_test(&kref->refcount)) {  // Atomic decrement + test
        release(kref);
        return 1;
    }
    return 0;
}
```

### 8.5 Scalability Limits in v3.2

1. **Global sysfs_mutex**: Serializes all sysfs modifications
2. **Single uevent sequence number**: Global lock for each uevent
3. **Sysfs page size limit**: Attributes limited to ~PAGE_SIZE output

---

## 9. Common Pitfalls & Bugs

### 9.1 Forgetting kobject_put After Failed kobject_add

```c
// BUG: Memory leak
ret = kobject_add(&dev->kobj, parent, "name");
if (ret) {
    // WRONG: Just returning leaks the kobject!
    return ret;
}

// CORRECT:
ret = kobject_add(&dev->kobj, parent, "name");
if (ret) {
    kobject_put(&dev->kobj);  // Must put even on add failure
    return ret;
}
```

### 9.2 Missing Release Function

```c
// BUG: Kernel warning, memory leak
static struct kobj_type my_ktype = {
    .sysfs_ops = &kobj_sysfs_ops,
    // MISSING: .release = my_release,  ← Will trigger WARN
};

// CORRECT:
static void my_release(struct kobject *kobj)
{
    struct my_device *dev = container_of(kobj, struct my_device, kobj);
    kfree(dev);
}

static struct kobj_type my_ktype = {
    .release = my_release,
    .sysfs_ops = &kobj_sysfs_ops,
};
```

### 9.3 Direct kfree Instead of kobject_put

```c
// BUG: Use-after-free, skips cleanup
void my_destroy(struct my_device *dev)
{
    kfree(dev);  // WRONG: Bypasses refcount, sysfs cleanup, uevent
}

// CORRECT:
void my_destroy(struct my_device *dev)
{
    kobject_del(&dev->kobj);                    // Remove from sysfs
    kobject_put(&dev->kobj);                    // Decrement refcount
    // kobj_type->release() will kfree() when refcount hits 0
}
```

### 9.4 Accessing Freed Kobject in Show/Store

```c
// BUG: Use-after-free if device removed during read
static ssize_t my_show(struct kobject *kobj, struct attribute *attr, char *buf)
{
    struct my_device *dev = container_of(kobj, struct my_device, kobj);
    // If kobject_del() races here, 'dev' may be freed
    return sprintf(buf, "%d\n", dev->value);  // CRASH!
}

// Sysfs protects this with s_active reference, but show/store must be careful
// not to access data that the device remove path frees before kobject_put
```

### 9.5 Attribute Buffer Overflow

```c
// BUG: Potential buffer overflow
static ssize_t my_show(struct kobject *kobj, struct kobj_attribute *attr,
                       char *buf)
{
    // buf is PAGE_SIZE bytes, but...
    return sprintf(buf, "%s", very_long_string);  // Could overflow!
}

// CORRECT: Use scnprintf or check length
static ssize_t my_show(struct kobject *kobj, struct kobj_attribute *attr,
                       char *buf)
{
    return scnprintf(buf, PAGE_SIZE, "%s", string);
}
```

### 9.6 Historical Issues in v3.2

1. **Namespace support incomplete**: Network namespace sysfs support still evolving
2. **No kernfs**: v3.2 uses traditional sysfs; kernfs abstraction came later (v3.14)
3. **Uevent helper dependency**: Still falls back to /sbin/hotplug if netlink fails

---

## 10. How to Read This Code Yourself

### 10.1 Recommended Reading Order

1. **`include/linux/kobject.h`**: Start with structure definitions
   - `struct kobject`
   - `struct kset`
   - `struct kobj_type`

2. **`include/linux/kref.h`**: Understand reference counting basics

3. **`lib/kobject.c`**: Core kobject operations
   - `kobject_init()`
   - `kobject_add()`
   - `kobject_put()` and `kobject_cleanup()`
   - `kobject_create_and_add()`

4. **`include/linux/sysfs.h`**: Sysfs API
   - `struct attribute`
   - `struct sysfs_ops`

5. **`fs/sysfs/sysfs.h`**: Internal sysfs structures
   - `struct sysfs_dirent`

6. **`fs/sysfs/file.c`**: How attributes work
   - `sysfs_read_file()`
   - `fill_read_buffer()`

7. **`lib/kobject_uevent.c`**: Uevent mechanism
   - `kobject_uevent_env()`

8. **`drivers/base/core.c`**: How devices use kobjects
   - `device_add()`
   - `device_release()`

### 10.2 What to Ignore Initially

- **Namespace support** (`kobject_ns.h`, `kobj_ns_type_operations`)
- **Binary attributes** (`fs/sysfs/bin.c`) — specialized use
- **Attribute groups complexity** — start with single attributes
- **Lockdep annotations** — debugging infrastructure

### 10.3 Useful Search Commands

```bash
# Find all kobject users
grep -r "kobject_init\|kobject_add" drivers/ | head -20

# Find kobj_type definitions  
grep -r "struct kobj_type.*=" drivers/

# Find attribute show/store implementations
grep -r "\.show.*=.*_show" drivers/base/

# Find all places that create sysfs files
grep -r "sysfs_create_file\|device_create_file" drivers/

# Find uevent additions
grep -r "kobject_uevent\|add_uevent_var" drivers/

# Trace kobject lifecycle with cscope
cscope -d
# Search: kobject_cleanup
```

### 10.4 Debugging Tips

```bash
# Enable kobject debugging
echo 'module kobject +p' > /sys/kernel/debug/dynamic_debug/control

# Watch uevents
udevadm monitor --kernel --property

# Dump kobject refcount (if you have access to memory)
crash> struct kobject.kref <address>

# List sysfs tree
find /sys -type d -name "*pci*" 2>/dev/null

# Check kobject hierarchy
ls -la /sys/devices/*/
```

---

## 11. Summary & Mental Model

### One-Paragraph Summary

The Linux device model provides a unified infrastructure for managing kernel objects through three core components: **kobject** (a reference-counted object with name, parent, and sysfs representation), **kset** (a container grouping related kobjects), and **sysfs** (a virtual filesystem exposing the kobject hierarchy to user space). When a kobject is added via `kobject_add()`, it creates a directory in sysfs; attributes defined in `kobj_type->default_attrs` or added via `sysfs_create_file()` become readable/writable files. The `kobj_type->release()` callback is invoked when the last reference (tracked by embedded `kref`) is dropped, ensuring deterministic cleanup. Uevents notify user space (udev) of object changes via netlink, enabling hot-plug functionality. All higher-level abstractions (`struct device`, `struct bus_type`, `struct class`) embed kobjects and leverage this infrastructure.

### Key Invariants

1. **Always call kobject_put() after kobject_init()** — even if `kobject_add()` fails
2. **kobj_type must have release()** — or get a WARN and potential memory leak
3. **Never kfree() a kobject directly** — always `kobject_put()` and let release handle it
4. **kobject_uevent() after kobject_add()** — not before (sysfs entry must exist)
5. **show()/store() must not exceed PAGE_SIZE** — use `scnprintf()` to be safe

### Mental Model

Think of kobjects as **building blocks with three aspects**:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                     THE KOBJECT TRINITY                                     │
│                                                                             │
│   ┌───────────────────┐                                                     │
│   │     IDENTITY      │ ← name, parent, kset membership                     │
│   │   (Hierarchical)  │   "Who am I and where do I belong?"                 │
│   └─────────┬─────────┘                                                     │
│             │                                                               │
│   ┌─────────┴─────────┐                                                     │
│   │                   │                                                     │
│   ▼                   ▼                                                     │
│ ┌───────────────────┐ ┌───────────────────┐                                 │
│ │    VISIBILITY     │ │    LIFETIME       │                                 │
│ │     (sysfs)       │ │  (kref + release) │                                 │
│ │                   │ │                   │                                 │
│ │ "How can users    │ │ "When do I die?"  │                                 │
│ │  see and control  │ │                   │                                 │
│ │  me?"             │ │ refcount tracks   │                                 │
│ │                   │ │ all users;        │                                 │
│ │ sysfs_dirent      │ │ release() called  │                                 │
│ │ creates /sys dir  │ │ when count hits 0 │                                 │
│ │ attributes are    │ │                   │                                 │
│ │ files             │ │                   │                                 │
│ └───────────────────┘ └───────────────────┘                                 │
│                                                                             │
│   Plus: NOTIFICATION (uevent) - "How do I tell userspace about changes?"    │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 12. What to Study Next

### Recommended Learning Order

| Order | Subsystem | Why It Matters |
|-------|-----------|----------------|
| 1 | **Driver Core** (`drivers/base/`) | Devices, buses, drivers built on kobjects |
| 2 | **Platform Devices** (`drivers/base/platform.c`) | Simple device model example |
| 3 | **PCI Subsystem** (`drivers/pci/`) | Real-world bus implementation |
| 4 | **USB Subsystem** (`drivers/usb/core/`) | Complex hot-plug device model |
| 5 | **Block Layer** (`block/genhd.c`) | Disks use kobjects for /sys/block/ |
| 6 | **Network Devices** (`net/core/net-sysfs.c`) | /sys/class/net/ implementation |
| 7 | **debugfs** (`fs/debugfs/`) | Alternative to sysfs for debugging |

### Related Files for Further Study

**Driver Core Deep Dive**:
- `drivers/base/core.c` — `device_add()`, `device_del()`
- `drivers/base/bus.c` — `bus_register()`, device↔driver matching
- `drivers/base/driver.c` — `driver_register()`
- `drivers/base/class.c` — `class_register()`, `/sys/class/`

**Attribute Groups**:
- `fs/sysfs/group.c` — Managing attribute collections
- `include/linux/sysfs.h` — `struct attribute_group`

**Real Device Examples**:
- `drivers/pci/pci-sysfs.c` — PCI device attributes
- `drivers/usb/core/sysfs.c` — USB device attributes
- `net/core/net-sysfs.c` — Network interface attributes

---

## Appendix A: Key Functions Quick Reference

### Kobject Operations
```c
kobject_init(kobj, ktype)           // Initialize (refcount = 1)
kobject_add(kobj, parent, fmt, ...) // Add to hierarchy + sysfs
kobject_init_and_add(...)           // Combined init + add
kobject_del(kobj)                   // Remove from sysfs + hierarchy
kobject_get(kobj)                   // Increment refcount
kobject_put(kobj)                   // Decrement refcount (may free)
kobject_create()                    // Dynamically allocate kobject
kobject_create_and_add(name, parent)// Create + init + add
kobject_set_name(kobj, fmt, ...)    // Set/change name
kobject_rename(kobj, new_name)      // Rename in sysfs
kobject_move(kobj, new_parent)      // Move to different parent
kobject_uevent(kobj, action)        // Send uevent to userspace
```

### Kset Operations
```c
kset_init(kset)                     // Initialize kset
kset_register(kset)                 // Register (adds embedded kobj)
kset_unregister(kset)               // Unregister
kset_create_and_add(name, ops, parent) // Create + init + register
kset_get(kset)                      // Get reference (via embedded kobj)
kset_put(kset)                      // Put reference
kset_find_obj(kset, name)           // Find member by name
```

### Sysfs Operations
```c
sysfs_create_file(kobj, attr)       // Create attribute file
sysfs_remove_file(kobj, attr)       // Remove attribute file
sysfs_create_group(kobj, grp)       // Create attribute group
sysfs_remove_group(kobj, grp)       // Remove attribute group
sysfs_create_link(kobj, target, name) // Create symlink
sysfs_remove_link(kobj, name)       // Remove symlink
sysfs_create_bin_file(kobj, attr)   // Create binary attribute
sysfs_notify(kobj, dir, attr)       // Wake poll() waiters
```

### Reference Counting
```c
kref_init(kref)                     // Set refcount = 1
kref_get(kref)                      // Increment
kref_put(kref, release)             // Decrement, call release if 0
```

---

*Document generated for Linux kernel v3.2. Some details may differ in other versions.*

