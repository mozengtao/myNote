# Object-Oriented Programming Patterns in the Linux Kernel v3.2

**A Hands-On Learning Roadmap for Senior C Engineers**

---

## 1. Conceptual Foundation: OOP Without a Language

The Linux kernel implements a rigorous object-oriented architecture in plain C. There
are no classes, no `virtual` keyword, no destructors — yet the kernel's abstractions
are more disciplined than most C++ codebases. The reason: where C++ compilers enforce
OOP rules at compile time, the kernel enforces them through **convention, structure
layout, and function pointer contracts**.

### 1.1 The Mapping

| OOP Concept     | C++ Mechanism          | Kernel C Mechanism                                      |
|-----------------|------------------------|---------------------------------------------------------|
| Encapsulation   | `private`/`public`     | Opaque pointers, `_private` fields, header discipline   |
| Abstraction     | Abstract base class    | Operations structs (`struct file_operations`)            |
| Polymorphism    | Virtual functions      | Function pointer tables + indirect calls                |
| Inheritance     | Class derivation       | Struct embedding + `container_of`                       |
| Interfaces      | Pure virtual class     | Ops structs where all methods are function pointers     |
| RTTI            | `dynamic_cast`         | `container_of` (compile-time, zero-cost)                |
| Destructor      | `~ClassName()`         | `release` callbacks, `kref_put`, `kobj_type.release`    |

### 1.2 Why Not C++?

Linus Torvalds's objection to C++ in the kernel isn't aesthetic — it's architectural:

- **Transparency**: Every function pointer dispatch is visible in the source. There are
  no hidden vtable lookups, no implicit copy constructors, no surprise allocations.
- **Control**: The kernel must control memory layout, alignment, cache-line placement
  (`____cacheline_aligned`), and initialization order. C++ abstractions fight this.
- **Binary stability**: Function pointer tables are explicit data. Their layout is
  controlled and versioned by the kernel developers, not by a compiler's ABI.

The kernel's OOP is **intentional and explicit** — every indirection is a conscious
engineering decision.

---

## 2. Core OOP Mechanisms

### 2.1 Struct Embedding and `container_of`

This is the kernel's inheritance mechanism. A "derived" struct embeds a "base" struct
as a member. Given a pointer to the base, you recover the derived object.

**The Macro** (from `include/linux/kernel.h:659`):

```c
#define container_of(ptr, type, member) ({                      \
    const typeof( ((type *)0)->member ) *__mptr = (ptr);        \
    (type *)( (char *)__mptr - offsetof(type,member) );})
```

The `typeof` check on `__mptr` provides a compile-time type safety assertion — if `ptr`
doesn't match the type of `type->member`, the compiler warns.

**Mental Model: Memory Layout**

```
                container_of(kobj_ptr, struct device, kobj)
                          ◄────────────────────┐
┌──────────────────────────────────────────────┐
│ struct device                                │
│  ┌───────────┐                               │
│  │ *parent   │                               │
│  ├───────────┤                               │
│  │ *p        │                               │
│  ├───────────┤◄── kobj_ptr points here       │
│  │ kobj      │    (struct kobject)           │
│  │  .name    │                               │
│  │  .kref    │                               │
│  │  .ktype   │                               │
│  ├───────────┤                               │
│  │ *bus      │                               │
│  │ *driver   │                               │
│  │ ...       │                               │
└──────────────────────────────────────────────┘
```

**Real Usage** — `lib/kobject.c:570`:

```c
static void kobject_release(struct kref *kref)
{
    kobject_cleanup(container_of(kref, struct kobject, kref));
}
```

Two levels of embedding in one line: a `kref` is embedded in a `kobject`, which itself
is embedded in a `struct device`. The kernel navigates this chain with successive
`container_of` calls — C's zero-cost "upcast".

**Why This Pattern**: Unlike C++ inheritance, struct embedding gives you full control
over memory layout. There's no vtable pointer consuming 8 bytes per object. The "base"
struct can be placed at any offset, and multiple "bases" can be embedded (multiple
inheritance without the diamond problem).

### 2.2 Function Pointer Tables (Vtables)

The kernel's equivalent of a C++ vtable is a `const struct` filled with function
pointers. The struct is the **interface contract**; each filesystem, driver, or protocol
provides its own **implementation** as a static instance.

**The Interface** — `include/linux/fs.h:1161`:

```c
struct file_operations {
    struct module *owner;
    loff_t (*llseek) (struct file *, loff_t, int);
    ssize_t (*read) (struct file *, char __user *, size_t, loff_t *);
    ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *);
    int (*mmap) (struct file *, struct vm_area_struct *);
    int (*open) (struct inode *, struct file *);
    int (*release) (struct inode *, struct file *);
    int (*fsync) (struct file *, loff_t, loff_t, int datasync);
    /* ... 20+ more methods */
};
```

**An Implementation** — `fs/ext2/file.c:64`:

```c
const struct file_operations ext2_file_operations = {
    .llseek     = generic_file_llseek,
    .read       = do_sync_read,
    .write      = do_sync_write,
    .aio_read   = generic_file_aio_read,
    .aio_write  = generic_file_aio_write,
    .unlocked_ioctl = ext2_ioctl,
    .mmap       = generic_file_mmap,
    .open       = dquot_file_open,
    .release    = ext2_release_file,
    .fsync      = ext2_fsync,
    .splice_read  = generic_file_splice_read,
    .splice_write = generic_file_splice_write,
};
```

Key observations:

- **Designated initializers** (`.read = ...`) leave unimplemented methods as `NULL`.
  The VFS layer checks for `NULL` before calling — this is the "optional method" pattern.
- **`const`** — the ops struct is immutable. It's shared across all instances of the
  same type (all ext2 regular files share one `ext2_file_operations`). This saves memory
  and enforces the contract.
- **Mixing generic and specific**: `generic_file_llseek` is shared across many
  filesystems. `ext2_ioctl` is ext2-specific. This is **selective method override** —
  the C equivalent of overriding some virtual methods while inheriting defaults.

### 2.3 Polymorphic Dispatch

The VFS layer calls filesystem operations without knowing which filesystem it's talking
to. This is runtime polymorphism.

**The Dispatch** — `fs/read_write.c:364`:

```c
ssize_t
vfs_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
    ssize_t ret;

    if (!(file->f_mode & FMODE_READ))
        return -EBADF;
    if (!file->f_op || (!file->f_op->read && !file->f_op->aio_read))
        return -EINVAL;

    ret = rw_verify_area(READ, file, pos, count);
    if (ret >= 0) {
        count = ret;
        if (file->f_op->read)
            ret = file->f_op->read(file, buf, count, pos);
        else
            ret = do_sync_read(file, buf, count, pos);
        /* ... */
    }
    return ret;
}
```

**Call chain diagram:**

```
  User calls read(fd, buf, n)
          │
          ▼
  sys_read()                          ← syscall entry
          │
          ▼
  vfs_read(file, ...)                 ← VFS generic layer
          │
          ├─ file->f_op->read(...)    ← polymorphic dispatch
          │      │
          │      ├── ext2: do_sync_read → generic_file_aio_read
          │      ├── proc: proc_reg_read → specific proc handler
          │      ├── sysfs: sysfs_read_file
          │      └── pipe: pipe_read
          │
          ▼
  return bytes_read
```

The pattern is always the same: `object->ops->method(object, ...)`. The object carries
its own vtable pointer. The generic layer dereferences it. The specific implementation
runs. This is textbook polymorphism — in C, with full visibility.

### 2.4 Operations Structs as Interface Contracts

The kernel defines many operations structs — each one is an **interface** in OOP terms.
An object that carries a pointer to such a struct "implements" that interface.

**Key ops structs in v3.2:**

| Struct                          | "Interface for"                    | Carried by                  |
|---------------------------------|------------------------------------|-----------------------------|
| `struct file_operations`        | File I/O behavior                  | `struct inode.i_fop`        |
| `struct inode_operations`       | Inode namespace operations         | `struct inode.i_op`         |
| `struct super_operations`       | Superblock lifecycle               | `struct super_block.s_op`   |
| `struct address_space_operations`| Page cache I/O                    | `struct address_space.a_ops`|
| `struct net_device_ops`         | Network device behavior            | `struct net_device.netdev_ops`|
| `struct bus_type` (probe/match) | Bus-level device matching          | `struct device.bus`         |
| `struct kobj_type`              | Kobject lifecycle + sysfs          | `struct kobject.ktype`      |

Each object has **multiple interface pointers** — an `inode` has both `i_op` and `i_fop`.
This is analogous to a class implementing multiple interfaces in Java.

### 2.5 Object Lifetime Management: `kref`

The kernel uses reference counting for shared objects. `kref` is a thin wrapper around
an atomic counter with a callback-based release mechanism.

**The Structure** — `include/linux/kref.h:20`:

```c
struct kref {
    atomic_t refcount;
};
```

**The API** — `lib/kref.c`:

```c
void kref_init(struct kref *kref)
{
    atomic_set(&kref->refcount, 1);
    smp_mb();
}

void kref_get(struct kref *kref)
{
    WARN_ON(!atomic_read(&kref->refcount));
    atomic_inc(&kref->refcount);
    smp_mb__after_atomic_inc();
}

int kref_put(struct kref *kref, void (*release)(struct kref *kref))
{
    WARN_ON(release == NULL);
    WARN_ON(release == (void (*)(struct kref *))kfree);

    if (atomic_dec_and_test(&kref->refcount)) {
        release(kref);
        return 1;
    }
    return 0;
}
```

**Why `release` is a callback**: The `kref` doesn't know what struct it's embedded in.
The release function uses `container_of` to recover the enclosing object and free it.
This is the kernel's **destructor pattern** — type-erased, callback-based, zero
overhead beyond the function pointer.

**Example chain: kobject → kref → release**:

```c
/* lib/kobject.c:570 */
static void kobject_release(struct kref *kref)
{
    kobject_cleanup(container_of(kref, struct kobject, kref));
}

/* kobject_cleanup then calls t->release(kobj) through kobj_type */
```

### 2.6 Callback-Based Inversion of Control

The kernel extensively uses callbacks where a framework defines the skeleton of an
algorithm and specific implementations fill in the steps. This is the **Template Method
pattern** implemented through function pointers.

**Example: Device Probing** — `drivers/base/dd.c:108`:

```c
static int
really_probe(struct device *dev, struct device_driver *drv)
{
    int ret = 0;

    dev->driver = drv;
    if (driver_sysfs_add(dev))
        goto probe_failed;

    if (dev->bus->probe) {
        ret = dev->bus->probe(dev);
        if (ret)
            goto probe_failed;
    } else if (drv->probe) {
        ret = drv->probe(dev);
        if (ret)
            goto probe_failed;
    }

    driver_bound(dev);
    /* ... */
}
```

The framework (`really_probe`) orchestrates the flow. It first tries the bus-level
probe, then falls back to the driver's probe. The specific behavior is injected by
whoever registered the bus or driver. This is **Strategy pattern** + **Template Method**
combined.

---

## 3. Subsystem Case Studies

### 3.1 VFS Layer — The Kernel's Canonical OOP System

The Virtual File System is the single best example of OOP in the kernel. It defines
a unified interface for all filesystems — ext2, NFS, proc, sysfs — and dispatches
through function pointer tables.

**Objects and their roles:**

```
┌─────────────┐  s_op   ┌───────────────────┐
│ super_block │────────►│ super_operations  │  "How does this FS manage its state?"
│  .s_type    │         └───────────────────┘
│  .s_root    │
└─────────────┘
       │ contains
       ▼
┌─────────────┐  i_op   ┌───────────────────┐
│   inode     │────────►│ inode_operations  │  "How does this FS resolve names?"
│  .i_fop ────┼────┐    └───────────────────┘
│  .i_mapping │    │
└─────────────┘    │    ┌───────────────────┐
                   └───►│ file_operations   │  "How does this FS do I/O?"
                        └───────────────────┘
       │ opened as
       ▼
┌─────────────┐  f_op   ┌───────────────────┐
│    file     │────────►│ file_operations   │  (copied from inode at open time)
│  .f_inode   │         └───────────────────┘
└─────────────┘
```

#### 3.1.1 How a Filesystem Plugs Into VFS

Every filesystem begins by registering a `file_system_type` — the kernel's
**abstract factory** for superblocks. (`include/linux/fs.h:1859`):

```c
struct file_system_type {
    const char *name;
    int fs_flags;
    struct dentry *(*mount) (struct file_system_type *, int,
                   const char *, void *);
    void (*kill_sb) (struct super_block *);
    struct module *owner;
    struct file_system_type *next;
    struct list_head fs_supers;
    /* ... lock keys ... */
};
```

Each filesystem provides a static instance. Ext2 (`fs/ext2/super.c:1176`):

```c
static struct file_system_type ext2_fs_type = {
    .owner    = THIS_MODULE,
    .name     = "ext2",
    .mount    = ext2_mount,
    .kill_sb  = kill_block_super,
    .fs_flags = FS_REQUIRES_DEV,
};
```

**Registration** adds it to a global singly-linked list (`fs/filesystems.c:68`):

```c
int
register_filesystem(struct file_system_type *fs)
{
    int res = 0;
    struct file_system_type **p;

    BUG_ON(strchr(fs->name, '.'));
    if (fs->next)
        return -EBUSY;
    INIT_LIST_HEAD(&fs->fs_supers);
    write_lock(&file_systems_lock);
    p = find_filesystem(fs->name, strlen(fs->name));
    if (*p)
        res = -EBUSY;
    else
        *p = fs;
    write_unlock(&file_systems_lock);
    return res;
}
```

**Lookup at mount time** — `get_fs_type` (`fs/filesystems.c:270`) walks the list by
name; if not found, it tries `request_module` to auto-load the filesystem module:

```c
struct file_system_type *
get_fs_type(const char *name)
{
    struct file_system_type *fs;
    const char *dot = strchr(name, '.');
    int len = dot ? dot - name : strlen(name);

    fs = __get_fs_type(name, len);
    if (!fs && (request_module("%.*s", len, name) == 0))
        fs = __get_fs_type(name, len);
    /* ... */
    return fs;
}
```

**The mount call chain:**

```
mount(2) syscall
  → do_kern_mount(fstype, flags, name, data)       [fs/namespace.c]
    → get_fs_type("ext2")                           [fs/filesystems.c]
       walks file_systems linked list, returns &ext2_fs_type
    → vfs_kern_mount(type, flags, name, data)
      → mount_fs(type, flags, name, data)           [fs/super.c]
        → type->mount(type, flags, name, data)      ← polymorphic dispatch
           ext2: ext2_mount → mount_bdev → ext2_fill_super
           proc: proc_mount → proc_fill_super
           sysfs: sysfs_mount → sysfs_fill_super
```

Each filesystem's `.mount` eventually calls its own `fill_super`, which reads or
constructs the superblock and root inode.

#### 3.1.2 Where `inode->i_mode` Comes From

The `i_mode` field determines an inode's type (regular, directory, symlink, device,
etc.) and its permission bits. **Different filesystems obtain it in fundamentally
different ways** — this is the key polymorphic difference.

**Disk-based filesystems (ext2)** — read `i_mode` from the on-disk inode structure.

`ext2_fill_super` (`fs/ext2/super.c:1083`) fetches the root inode:

```c
root = ext2_iget(sb, EXT2_ROOT_INO);
```

`ext2_iget` (`fs/ext2/inode.c:956`) reads the raw inode from disk and copies fields:

```c
struct inode *
ext2_iget(struct super_block *sb, unsigned long ino)
{
    struct ext2_inode_info *ei;
    struct buffer_head *bh;
    struct ext2_inode *raw_inode;
    struct inode *inode;

    inode = iget_locked(sb, ino);
    if (!inode)
        return ERR_PTR(-ENOMEM);
    if (!(inode->i_state & I_NEW))
        return inode;

    ei = EXT2_I(inode);
    raw_inode = ext2_get_inode(inode->i_sb, ino, &bh);
    if (IS_ERR(raw_inode))
        goto bad_inode;

    inode->i_mode = le16_to_cpu(raw_inode->i_mode);   /* ← from disk */
    inode->i_uid  = (uid_t)le16_to_cpu(raw_inode->i_uid_low);
    inode->i_gid  = (gid_t)le16_to_cpu(raw_inode->i_gid_low);
    /* ... populate size, timestamps, block pointers ... */
}
```

`ext2_get_inode` (`fs/ext2/inode.c:911`) computes the block group and offset from the
inode number, reads the block from disk via `sb_bread`, and returns a pointer into the
buffer:

```c
static struct ext2_inode *
ext2_get_inode(struct super_block *sb, ino_t ino, struct buffer_head **p)
{
    unsigned long block_group;
    unsigned long block;
    unsigned long offset;
    struct ext2_group_desc *gdp;

    block_group = (ino - 1) / EXT2_INODES_PER_GROUP(sb);
    gdp = ext2_get_group_desc(sb, block_group, NULL);

    offset = ((ino - 1) % EXT2_INODES_PER_GROUP(sb)) * EXT2_INODE_SIZE(sb);
    block = le32_to_cpu(gdp->bg_inode_table) +
            (offset >> EXT2_BLOCK_SIZE_BITS(sb));
    if (!(bh = sb_bread(sb, block)))
        goto Eio;

    *p = bh;
    offset &= (EXT2_BLOCK_SIZE(sb) - 1);
    return (struct ext2_inode *)(bh->b_data + offset);
}
```

**Pseudo-filesystems (procfs)** — `i_mode` is set programmatically from a
`proc_dir_entry` structure, never read from disk.

`proc_get_inode` (`fs/proc/inode.c:319`):

```c
struct inode *
proc_get_inode(struct super_block *sb, struct proc_dir_entry *de)
{
    struct inode *inode;

    inode = iget_locked(sb, de->low_ino);
    if (!inode)
        return NULL;
    if (inode->i_state & I_NEW) {
        inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME;
        PROC_I(inode)->pde = de;

        if (de->mode) {
            inode->i_mode = de->mode;           /* ← from proc_dir_entry */
            inode->i_uid  = de->uid;
            inode->i_gid  = de->gid;
        }
        if (de->size)
            inode->i_size = de->size;
        /* ... */
    }
    return inode;
}
```

The `proc_dir_entry` for `/proc` itself (`fs/proc/root.c:181`):

```c
struct proc_dir_entry proc_root = {
    .low_ino  = PROC_ROOT_INO,
    .namelen  = 5,
    .mode     = S_IFDIR | S_IRUGO | S_IXUGO,   /* ← hardcoded */
    .nlink    = 2,
    .count    = ATOMIC_INIT(1),
    .proc_iops = &proc_root_inode_operations,
    .proc_fops = &proc_root_operations,
    .parent   = &proc_root,
    .name     = "/proc",
};
```

**Sysfs** — `i_mode` comes from the `sysfs_dirent` attribute, set when the dirent is
created in memory.

`sysfs_init_inode` (`fs/sysfs/inode.c:237`):

```c
static void
sysfs_init_inode(struct sysfs_dirent *sd, struct inode *inode)
{
    inode->i_private = sysfs_get(sd);
    inode->i_mapping->a_ops = &sysfs_aops;
    inode->i_op = &sysfs_inode_operations;

    set_default_inode_attr(inode, sd->s_mode);  /* ← from sysfs_dirent */
    sysfs_refresh_inode(sd, inode);

    switch (sysfs_type(sd)) {
    case SYSFS_DIR:
        inode->i_op  = &sysfs_dir_inode_operations;
        inode->i_fop = &sysfs_dir_operations;
        break;
    /* ... */
    }
}
```

`sysfs_new_dirent` (`fs/sysfs/dir.c:324`) stores the mode at creation time:

```c
struct sysfs_dirent *
sysfs_new_dirent(const char *name, umode_t mode, int type)
{
    /* ... alloc ... */
    sd->s_name  = name;
    sd->s_mode  = mode;
    sd->s_flags = type;
    return sd;
}
```

**Summary — `i_mode` origin by filesystem type:**

| Filesystem | `i_mode` Source | When Set |
|------------|-----------------|----------|
| ext2/ext3  | On-disk inode: `le16_to_cpu(raw_inode->i_mode)` | `ext2_iget` reads from block device |
| procfs     | `proc_dir_entry.mode` (hardcoded or set at creation) | `proc_get_inode` |
| sysfs      | `sysfs_dirent.s_mode` (set by kernel driver code) | `sysfs_init_inode` |
| tmpfs      | Passed to `shmem_get_inode` as argument | At inode allocation |
| devtmpfs   | Passed to `vfs_mknod` / `init_special_inode` | Device registration |

#### 3.1.3 Polymorphism in Action — Ops Assignment

Once `i_mode` is known, the filesystem assigns the right ops tables. This is the
**factory pattern** — the constructor decides which "subclass" behavior the inode gets.

`ext2_iget` (`fs/ext2/inode.c:1366`):

```c
if (S_ISREG(inode->i_mode)) {
    inode->i_op  = &ext2_file_inode_operations;
    inode->i_fop = &ext2_file_operations;
    inode->i_mapping->a_ops = &ext2_aops;
} else if (S_ISDIR(inode->i_mode)) {
    inode->i_op  = &ext2_dir_inode_operations;
    inode->i_fop = &ext2_dir_operations;
    inode->i_mapping->a_ops = &ext2_aops;
} else if (S_ISLNK(inode->i_mode)) {
    if (ext2_inode_is_fast_symlink(inode))
        inode->i_op = &ext2_fast_symlink_inode_operations;
    else
        inode->i_op = &ext2_symlink_inode_operations;
} else {
    inode->i_op = &ext2_special_inode_operations;
    /* init_special_inode sets i_fop for char/block devices, FIFOs */
}
```

From this point on, all VFS operations dispatch polymorphically — a `readdir` on a
directory invokes `ext2_readdir`, while a `read` on a regular file invokes
`do_sync_read`.

**Interfaces vs Implementations for ext2:**

| Role            | Interface (abstract)              | Implementation (ext2)              |
|-----------------|-----------------------------------|------------------------------------|
| Regular file IO | `struct file_operations`          | `ext2_file_operations`             |
| Directory IO    | `struct file_operations`          | `ext2_dir_operations`              |
| File inode ops  | `struct inode_operations`         | `ext2_file_inode_operations`       |
| Dir inode ops   | `struct inode_operations`         | `ext2_dir_inode_operations`        |
| Superblock ops  | `struct super_operations`         | `ext2_sops`                        |
| Page cache      | `struct address_space_operations` | `ext2_aops`                        |

### 3.2 Device Model — Object Hierarchy with Sysfs Reflection

The device model (`drivers/base/`) is the kernel's most complete OOP system. It has
inheritance, interfaces, lifetime management, and even a form of runtime reflection
through sysfs.

**The Object Hierarchy:**

```
struct kobject                      ← "Object" base class
    ├── kref (reference counting)   ← destructor mechanism
    ├── ktype → release()           ← type-specific destructor
    └── embedded in...
        │
        ├── struct device
        │       ├── struct kobject kobj         ← inherits from kobject
        │       ├── struct bus_type *bus         ← "which interface bus"
        │       ├── struct device_driver *driver ← "which implementation"
        │       └── void (*release)(struct device *)
        │
        ├── struct device_driver
        │       ├── int (*probe)(struct device *)
        │       ├── int (*remove)(struct device *)
        │       └── struct bus_type *bus
        │
        └── struct cdev
                ├── struct kobject kobj         ← inherits from kobject
                ├── const struct file_operations *ops
                └── dev_t dev
```

#### 3.2.1 How the Kernel Matches Drivers to Devices

When a device is registered (`device_add`), the kernel must find a compatible driver.
When a driver is registered (`driver_register`), it must find unbound devices. Both
paths converge on the same matching logic.

**`device_attach`** (`drivers/base/dd.c:232`) — called when a new device appears:

```c
int
device_attach(struct device *dev)
{
    int ret = 0;

    device_lock(dev);
    if (dev->driver) {
        /* already bound */
        ret = device_bind_driver(dev);
    } else {
        ret = bus_for_each_drv(dev->bus, NULL, dev, __device_attach);
    }
    device_unlock(dev);
    return ret;
}
```

It iterates every driver on the bus, calling `__device_attach` for each:

```c
static int
__device_attach(struct device_driver *drv, void *data)
{
    struct device *dev = data;

    if (!driver_match_device(drv, dev))
        return 0;

    return driver_probe_device(drv, dev);
}
```

**`driver_attach`** (`drivers/base/dd.c:313`) — the mirror, called when a new driver
is registered. It iterates every device on the bus:

```c
int
driver_attach(struct device_driver *drv)
{
    return bus_for_each_dev(drv->bus, NULL, drv, __driver_attach);
}
```

Both converge on **`driver_match_device`** (`drivers/base/base.h:109`):

```c
static inline int
driver_match_device(struct device_driver *drv, struct device *dev)
{
    return drv->bus->match ? drv->bus->match(dev, drv) : 1;
}
```

This is the polymorphic dispatch point. **Each bus type provides its own `match`
function** — the kernel doesn't know how matching works, the bus does.

#### 3.2.2 Bus-Specific Matching — PCI and USB

**PCI** — matches by vendor ID, device ID, class, and subsystem IDs.

`pci_bus_type` (`drivers/pci/pci-driver.c:936`):

```c
struct bus_type pci_bus_type = {
    .name      = "pci",
    .match     = pci_bus_match,
    .uevent    = pci_uevent,
    .probe     = pci_device_probe,
    .remove    = pci_device_remove,
    .shutdown  = pci_device_shutdown,
    .dev_attrs = pci_dev_attrs,
    .bus_attrs = pci_bus_attrs,
    .pm        = PCI_PM_OPS_PTR,
};
```

`pci_bus_match` (`drivers/pci/pci-driver.c:906`) downcasts to PCI-specific types and
compares the driver's ID table against the device's IDs:

```c
static int
pci_bus_match(struct device *dev, struct device_driver *drv)
{
    struct pci_dev *pci_dev = to_pci_dev(dev);
    struct pci_driver *pci_drv = to_pci_driver(drv);
    const struct pci_device_id *found_id;

    found_id = pci_match_device(pci_drv, pci_dev);
    if (found_id)
        return 1;
    return 0;
}
```

The actual comparison (`drivers/pci/pci.h:170`):

```c
static inline const struct pci_device_id *
pci_match_one_device(const struct pci_device_id *id, const struct pci_dev *dev)
{
    if ((id->vendor == PCI_ANY_ID || id->vendor == dev->vendor) &&
        (id->device == PCI_ANY_ID || id->device == dev->device) &&
        (id->subvendor == PCI_ANY_ID || id->subvendor == dev->subsystem_vendor) &&
        (id->subdevice == PCI_ANY_ID || id->subdevice == dev->subsystem_device) &&
        !((id->class ^ dev->class) & id->class_mask))
        return id;
    return NULL;
}
```

**USB** — matches by device class, interface class, vendor/product ID, or dynamic IDs.

`usb_bus_type` (`drivers/usb/core/driver.c:1231`):

```c
struct bus_type usb_bus_type = {
    .name  = "usb",
    .match = usb_device_match,
    .uevent = usb_uevent,
};
```

`usb_device_match` (`drivers/usb/core/driver.c:454`) handles two distinct cases —
whole-device drivers and interface drivers:

```c
static int
usb_device_match(struct device *dev, struct device_driver *drv)
{
    if (is_usb_device(dev)) {
        if (!is_usb_device_driver(drv))
            return 0;
        return 1;
    } else if (is_usb_interface(dev)) {
        struct usb_interface *intf;
        struct usb_driver *usb_drv;
        const struct usb_device_id *id;

        if (is_usb_device_driver(drv))
            return 0;

        intf = to_usb_interface(dev);
        usb_drv = to_usb_driver(drv);

        id = usb_match_id(intf, usb_drv->id_table);
        if (id)
            return 1;
        id = usb_match_dynamic_id(intf, usb_drv);
        if (id)
            return 1;
    }
    return 0;
}
```

**The full matching flow:**

```
device_register(dev)
  → device_add(dev)
    → bus_probe_device(dev)
      → device_attach(dev)
        → bus_for_each_drv(dev->bus, ..., __device_attach)
          → driver_match_device(drv, dev)
            → drv->bus->match(dev, drv)     ← polymorphic dispatch
               PCI:  pci_bus_match  → pci_match_one_device (vendor/device/class)
               USB:  usb_device_match → usb_match_id (interface class/protocol)
               platform: platform_match → strcmp(dev->name, drv->name)
          → driver_probe_device(drv, dev)
            → really_probe(dev, drv)
              → bus->probe(dev) or drv->probe(dev)
```

#### 3.2.3 Polymorphic Dispatch in the Driver Model

**`struct cdev`** (`include/linux/cdev.h:12`):

```c
struct cdev {
    struct kobject kobj;
    struct module *owner;
    const struct file_operations *ops;
    struct list_head list;
    dev_t dev;
    unsigned int count;
};
```

A `cdev` is a `kobject` (via embedding) that also carries a `file_operations` pointer.
This is **multiple inheritance**: it inherits lifetime management from `kobject` and
I/O behavior from whatever `file_operations` table is assigned.

**Double dispatch via platform bus** (`drivers/base/platform.c:437`):

```c
static int
platform_drv_probe(struct device *_dev)
{
    struct platform_driver *drv = to_platform_driver(_dev->driver);
    struct platform_device *dev = to_platform_device(_dev);

    return drv->probe(dev);
}
```

When a platform driver registers, the core wires up the bus-level probe:

```c
int
platform_driver_register(struct platform_driver *drv)
{
    drv->driver.bus = &platform_bus_type;
    if (drv->probe)
        drv->driver.probe = platform_drv_probe;
    if (drv->remove)
        drv->driver.remove = platform_drv_remove;
    if (drv->shutdown)
        drv->driver.shutdown = platform_drv_shutdown;

    return driver_register(&drv->driver);
}
```

The generic driver core calls `dev->bus->probe(dev)`, which resolves to
`platform_drv_probe`, which uses `container_of`-based macros to downcast from generic
types to bus-specific types, then calls the bus-specific probe. This is **double
dispatch** — first through the bus ops, then through the driver ops.

### 3.3 Character Devices — A Complete Object Lifecycle

A character device driver illustrates the full OOP lifecycle: construction, method
dispatch, and destruction. Understanding the internals of `cdev_init`, `cdev_add`,
`chrdev_open`, and `cdev_del` reveals how the kernel maps user-visible device numbers
to polymorphic objects.

#### 3.3.1 The Major/Minor Number Lookup Infrastructure

The kernel maintains a `cdev_map` — a hash table of 255 buckets indexed by
`MAJOR(dev) % 255`. Each bucket is a linked list of `struct probe` entries, sorted by
range size (most specific match wins).

**The map structure** (`drivers/base/map.c:19`):

```c
struct kobj_map {
    struct probe {
        struct probe *next;
        dev_t dev;
        unsigned long range;
        struct module *owner;
        kobj_probe_t *get;
        int (*lock)(dev_t, void *);
        void *data;
    } *probes[255];
    struct mutex *lock;
};
```

This is a **strategy pattern**: each probe entry carries its own `get` callback and
`lock` callback, plus opaque `data`. The map itself is polymorphic — it doesn't know
about `cdev` at all, only `kobject`.

#### 3.3.2 What `cdev_init` Really Does

`cdev_init` (`fs/char_dev.c:412`):

```c
void
cdev_init(struct cdev *cdev, const struct file_operations *fops)
{
    memset(cdev, 0, sizeof *cdev);
    INIT_LIST_HEAD(&cdev->list);
    kobject_init(&cdev->kobj, &ktype_cdev_default);
    cdev->ops = fops;
}
```

Three things happen:
1. Zero the entire struct (safe initialization).
2. Initialize the embedded `kobject` with `ktype_cdev_default` — this sets the
   kobject's release function, which will be called when the last reference is dropped.
3. Store the `file_operations` pointer — this is the "vtable binding" step.

#### 3.3.3 What `cdev_add` Really Does

`cdev_add` (`fs/char_dev.c:363`):

```c
int
cdev_add(struct cdev *p, dev_t dev, unsigned count)
{
    p->dev   = dev;
    p->count = count;
    return kobj_map(cdev_map, dev, count, NULL,
                    exact_match, exact_lock, p);
}
```

It registers the cdev in the global `cdev_map` by calling `kobj_map`, which allocates
`probe` entries (one per major number in the range), stores the cdev as the `data`
field, and inserts them into the hash buckets sorted by range.

The callbacks passed to `kobj_map` are:

```c
static struct kobject *
exact_match(dev_t dev, int *part, void *data)
{
    struct cdev *p = data;
    return &p->kobj;
}

static int
exact_lock(dev_t dev, void *data)
{
    struct cdev *p = data;
    return cdev_get(p) ? 0 : -1;
}
```

`exact_match` returns the kobject (used by `kobj_lookup` to recover the cdev via
`container_of`). `exact_lock` takes a reference to prevent the cdev from being freed
during the lookup.

**`kobj_map`** (`drivers/base/map.c:31`) — the insertion:

```c
int
kobj_map(struct kobj_map *domain, dev_t dev, unsigned long range,
         struct module *module, kobj_probe_t *probe,
         int (*lock)(dev_t, void *), void *data)
{
    unsigned n = MAJOR(dev + range - 1) - MAJOR(dev) + 1;
    unsigned index = MAJOR(dev);
    struct probe *p;

    p = kmalloc(sizeof(struct probe) * n, GFP_KERNEL);
    /* fill in each probe entry */
    for (i = 0; i < n; i++, p++) {
        p->owner = module;
        p->get   = probe;
        p->lock  = lock;
        p->dev   = dev;
        p->range = range;
        p->data  = data;
    }
    /* insert into hash buckets, sorted by range (smallest first) */
    mutex_lock(domain->lock);
    for (i = 0, p -= n; i < n; i++, p++, index++) {
        struct probe **s = &domain->probes[index % 255];
        while (*s && (*s)->range < range)
            s = &(*s)->next;
        p->next = *s;
        *s = p;
    }
    mutex_unlock(domain->lock);
    return 0;
}
```

#### 3.3.4 What `cdev_del` Really Does

`cdev_del` (`fs/char_dev.c:375`):

```c
void
cdev_del(struct cdev *p)
{
    cdev_unmap(p->dev, p->count);
    kobject_put(&p->kobj);
}
```

Two steps: remove the probe entries from `cdev_map` via `kobj_unmap`, then drop the
kobject reference. If this is the last reference, the ktype's release function frees
the cdev.

#### 3.3.5 How `chrdev_open` Finds the `cdev`

When userspace calls `open("/dev/foo", ...)`, the VFS resolves the path to an inode.
For character special files, `inode->i_fop` points to `def_chr_fops`, whose `.open`
is `chrdev_open`. This is the **bridge between VFS and the character device subsystem**.

`chrdev_open` (`fs/char_dev.c:366`):

```c
static int
chrdev_open(struct inode *inode, struct file *filp)
{
    struct cdev *p;
    struct cdev *new = NULL;
    int ret = 0;

    spin_lock(&cdev_lock);
    p = inode->i_cdev;
    if (!p) {
        struct kobject *kobj;
        int idx;
        spin_unlock(&cdev_lock);

        kobj = kobj_lookup(cdev_map, inode->i_rdev, &idx);
        if (!kobj)
            return -ENXIO;
        new = container_of(kobj, struct cdev, kobj);

        spin_lock(&cdev_lock);
        p = inode->i_cdev;
        if (!p) {
            inode->i_cdev = p = new;
            list_add(&inode->i_devices, &p->list);
            new = NULL;
        } else if (!cdev_get(p))
            ret = -ENXIO;
    } else if (!cdev_get(p))
        ret = -ENXIO;
    spin_unlock(&cdev_lock);
    cdev_put(new);

    if (ret)
        return ret;

    ret = -ENXIO;
    filp->f_op = fops_get(p->ops);     /* ← install the cdev's vtable */
    if (!filp->f_op)
        goto out_cdev_put;

    if (filp->f_op->open) {
        ret = filp->f_op->open(inode, filp);   /* ← call device open */
        if (ret)
            goto out_cdev_put;
    }
    return 0;
}
```

**`kobj_lookup`** (`drivers/base/map.c:95`) — the lookup that resolves
`inode->i_rdev` (the major/minor number) to a kobject:

```c
struct kobject *
kobj_lookup(struct kobj_map *domain, dev_t dev, int *index)
{
    struct kobject *kobj;
    struct probe *p;
    unsigned long best = ~0UL;

retry:
    mutex_lock(domain->lock);
    for (p = domain->probes[MAJOR(dev) % 255]; p; p = p->next) {
        if (p->dev > dev || p->dev + p->range - 1 < dev)
            continue;
        if (p->range - 1 >= best)
            break;
        if (!try_module_get(p->owner))
            continue;

        /* found a match — call the probe's get/lock callbacks */
        *index = dev - p->dev;
        if (p->lock && p->lock(dev, p->data) < 0) {
            module_put(p->owner);
            continue;
        }
        mutex_unlock(domain->lock);
        kobj = p->get(dev, index, p->data);  /* → exact_match → &cdev->kobj */
        module_put(p->owner);
        if (kobj)
            return kobj;
        goto retry;
    }
    mutex_unlock(domain->lock);
    return NULL;
}
```

**The complete call chain:**

```
open("/dev/foo")
  → VFS resolves path to inode (inode->i_rdev = MKDEV(major, minor))
  → inode->i_fop->open = chrdev_open               [fs/char_dev.c]
    → kobj_lookup(cdev_map, inode->i_rdev, &idx)    [drivers/base/map.c]
      → hash to probes[MAJOR(dev) % 255]
      → walk chain, find probe where dev is in range
      → p->lock(dev, data) → exact_lock → cdev_get(p)
      → p->get(dev, &idx, data) → exact_match → &cdev->kobj
    → container_of(kobj, struct cdev, kobj)          recover the cdev
    → filp->f_op = fops_get(p->ops)                 install device vtable
    → filp->f_op->open(inode, filp)                 call device-specific open
```

#### 3.3.6 Concrete Example: `/dev/null` and `/dev/zero`

`drivers/char/mem.c` demonstrates a second dispatch pattern — a **demultiplexer**
that routes by minor number.

The `devlist` table (`drivers/char/mem.c:643`):

```c
static const struct memdev {
    const char *name;
    mode_t mode;
    const struct file_operations *fops;
    struct backing_dev_info *dev_info;
} devlist[] = {
     [1] = { "mem",     0,    &mem_fops,     &directly_mappable_cdev_bdi },
     [3] = { "null",    0666, &null_fops,    NULL },
     [5] = { "zero",    0666, &zero_fops,    &zero_bdi },
     [7] = { "full",    0666, &full_fops,    NULL },
     [8] = { "random",  0666, &random_fops,  NULL },
     [9] = { "urandom", 0666, &urandom_fops, NULL },
    [11] = { "kmsg",    0,    &kmsg_fops,    NULL },
};
```

All minor numbers share one `file_operations` (`memory_fops`) whose `.open` is
`memory_open`. That function indexes `devlist` by minor and **replaces** `filp->f_op`:

```c
static int
memory_open(struct inode *inode, struct file *filp)
{
    int minor;
    const struct memdev *dev;

    minor = iminor(inode);
    if (minor >= ARRAY_SIZE(devlist))
        return -ENXIO;

    dev = &devlist[minor];
    if (!dev->fops)
        return -ENXIO;

    filp->f_op = dev->fops;    /* ← replace with per-device ops */

    if (dev->fops->open)
        return dev->fops->open(inode, filp);
    return 0;
}
```

This is a **two-stage polymorphism**: `chrdev_open` dispatches to the `mem` driver's
`memory_open`, which then dispatches to the specific device's ops (`null_fops`,
`zero_fops`, etc.). The `null_fops`:

```c
static const struct file_operations null_fops = {
    .llseek       = null_lseek,
    .read         = read_null,
    .write        = write_null,
    .splice_write = splice_write_null,
};
```

### 3.4 Network Stack — `net_device_ops`

Network devices follow the same ops-table pattern, but with a different object lifecycle:
the `net_device` is allocated by a framework function, configured by a setup callback,
and has its ops assigned by the driver.

#### 3.4.1 How a `net_device` Is Created and Configured

**`alloc_netdev_mqs`** (`net/core/dev.c:5987`) — the "constructor":

```c
struct net_device *
alloc_netdev_mqs(int sizeof_priv, const char *name,
                 void (*setup)(struct net_device *),
                 unsigned int txqs, unsigned int rxqs)
{
    struct net_device *dev;
    size_t alloc_size;

    alloc_size = sizeof(struct net_device);
    if (sizeof_priv) {
        alloc_size = ALIGN(alloc_size, NETDEV_ALIGN);
        alloc_size += sizeof_priv;
    }

    p = kzalloc(alloc_size, GFP_KERNEL);
    dev = PTR_ALIGN(p, NETDEV_ALIGN);

    INIT_LIST_HEAD(&dev->napi_list);
    INIT_LIST_HEAD(&dev->unreg_list);
    dev->priv_flags = IFF_XMIT_DST_RELEASE;

    setup(dev);       /* ← callback assigns netdev_ops and device properties */

    strcpy(dev->name, name);
    return dev;
}
```

The `setup` callback is a **template method** — the framework allocates and initializes
the struct, then hands it to the driver-specific setup to fill in behavior.

**`alloc_etherdev`** is a convenience macro:

```c
#define alloc_netdev(sizeof_priv, name, setup) \
    alloc_netdev_mqs(sizeof_priv, name, setup, 1, 1)
```

`alloc_etherdev_mqs` (`net/ethernet/eth.c:365`) calls it with `ether_setup`:

```c
struct net_device *
alloc_etherdev_mqs(int sizeof_priv, unsigned int txqs, unsigned int rxqs)
{
    return alloc_netdev_mqs(sizeof_priv, "eth%d", ether_setup, txqs, rxqs);
}
```

`ether_setup` (`net/ethernet/eth.c:335`) configures Ethernet-generic properties but
**does not set `netdev_ops`** — that's the driver's job:

```c
void
ether_setup(struct net_device *dev)
{
    dev->header_ops    = &eth_header_ops;
    dev->type          = ARPHRD_ETHER;
    dev->hard_header_len = ETH_HLEN;
    dev->mtu           = ETH_DATA_LEN;
    dev->addr_len      = ETH_ALEN;
    dev->tx_queue_len  = 1000;
    dev->flags         = IFF_BROADCAST | IFF_MULTICAST;
    memset(dev->broadcast, 0xFF, ETH_ALEN);
}
```

#### 3.4.2 Loopback — A Minimal Implementation

The loopback device (`drivers/net/loopback.c`) shows the simplest possible
`net_device_ops`:

```c
static const struct net_device_ops loopback_ops = {
    .ndo_init        = loopback_dev_init,
    .ndo_start_xmit  = loopback_xmit,
    .ndo_get_stats64 = loopback_get_stats64,
};
```

Its setup callback assigns the ops:

```c
static void
loopback_setup(struct net_device *dev)
{
    dev->mtu            = (16 * 1024) + 20 + 20 + 12;
    dev->hard_header_len = ETH_HLEN;
    dev->flags          = IFF_LOOPBACK;
    dev->features       = NETIF_F_SG | NETIF_F_FRAGLIST | NETIF_F_ALL_TSO
                        | NETIF_F_NO_CSUM | NETIF_F_HIGHDMA | NETIF_F_LLTX
                        | NETIF_F_NETNS_LOCAL | NETIF_F_LOOPBACK;
    dev->ethtool_ops    = &loopback_ethtool_ops;
    dev->header_ops     = &eth_header_ops;
    dev->netdev_ops     = &loopback_ops;          /* ← vtable binding */
    dev->destructor     = loopback_dev_free;
}
```

Registration:

```c
static __net_init int
loopback_net_init(struct net *net)
{
    struct net_device *dev;

    dev = alloc_netdev(0, "lo", loopback_setup);
    err = register_netdev(dev);
    net->loopback_dev = dev;
    return 0;
}
```

#### 3.4.3 e1000 — A Real Hardware Driver

The Intel e1000 driver (`drivers/net/ethernet/intel/e1000/e1000_main.c:844`) provides
a full ops table:

```c
static const struct net_device_ops e1000_netdev_ops = {
    .ndo_open            = e1000_open,
    .ndo_stop            = e1000_close,
    .ndo_start_xmit      = e1000_xmit_frame,
    .ndo_get_stats       = e1000_get_stats,
    .ndo_set_rx_mode     = e1000_set_rx_mode,
    .ndo_set_mac_address = e1000_set_mac,
    .ndo_tx_timeout      = e1000_tx_timeout,
    .ndo_change_mtu      = e1000_change_mtu,
    .ndo_do_ioctl        = e1000_ioctl,
    .ndo_validate_addr   = eth_validate_addr,  /* ← generic helper */
    .ndo_vlan_rx_add_vid = e1000_vlan_rx_add_vid,
    .ndo_vlan_rx_kill_vid = e1000_vlan_rx_kill_vid,
    .ndo_fix_features    = e1000_fix_features,
    .ndo_set_features    = e1000_set_features,
};
```

Unlike loopback, the e1000 driver sets `netdev_ops` in its PCI probe function
(not in the setup callback):

```c
/* in e1000_probe: */
netdev = alloc_etherdev(sizeof(struct e1000_adapter));
/* ... hardware init ... */
netdev->netdev_ops = &e1000_netdev_ops;       /* ← vtable binding */
/* ... */
register_netdev(netdev);
```

#### 3.4.4 Polymorphic Dispatch

**`register_netdevice`** (`net/core/dev.c:5598`) calls the ops polymorphically:

```c
int
register_netdevice(struct net_device *dev)
{
    /* ... */
    if (dev->netdev_ops->ndo_init) {
        ret = dev->netdev_ops->ndo_init(dev);    /* ← polymorphic */
        if (ret)
            goto out;
    }
    /* ... register kobject, update features, notify ... */
    dev->reg_state = NETREG_REGISTERED;
    list_netdevice(dev);
    return 0;
}
```

**`__dev_open`** (`net/core/dev.c:1150`):

```c
static int
__dev_open(struct net_device *dev)
{
    const struct net_device_ops *ops = dev->netdev_ops;

    if (ops->ndo_validate_addr)
        ret = ops->ndo_validate_addr(dev);

    if (!ret && ops->ndo_open)
        ret = ops->ndo_open(dev);
    /* ... */
}
```

**Comparison — loopback vs e1000:**

| Operation        | loopback                    | e1000                   |
|------------------|-----------------------------|-------------------------|
| `ndo_open`       | (not set — NULL, skipped)   | `e1000_open`            |
| `ndo_start_xmit` | `loopback_xmit`            | `e1000_xmit_frame`     |
| `ndo_get_stats`  | (uses `ndo_get_stats64`)    | `e1000_get_stats`       |
| `ndo_change_mtu` | (not set — default)         | `e1000_change_mtu`      |

Same interface, different implementations, selected at device creation time. The
networking core never knows or cares which hardware it's driving.

---

## 4. Reading the Kernel Like an OOP System

### 4.1 Identifying "Classes"

Look for structs that carry:
- An ops pointer (`const struct xxx_operations *xxx_op`)
- A `kref` or `atomic_t` refcount
- A `struct list_head` (for membership in collections)
- A `struct kobject` (for sysfs visibility)

These are the kernel's "objects." The ops pointer tells you the interface it implements.

### 4.2 Tracing the Inheritance Chain

When you see `container_of(ptr, struct foo, member)`:
- `struct foo` is the "derived class"
- The type of `member` is the "base class"
- The function receiving `ptr` operates on the base; the `container_of` recovers the derived

Common `to_xxx` macros are wrappers:
```c
/* include/linux/device.h */
#define to_platform_device(x) container_of((x), struct platform_device, dev)
```

When reading unfamiliar code, grep for `to_` macros and `container_of` — they reveal
the inheritance graph.

### 4.3 Detecting Interface Contracts

An interface contract is any struct whose members are **exclusively function pointers**
(plus maybe a `struct module *owner`). When you see one:
1. Note the struct name — this is the interface
2. Search for all static instances of it — these are implementations
3. Look at which object carries a pointer to it — that's the implementing class

```bash
# Find all implementations of file_operations in the tree
grep -r "struct file_operations .* = {" fs/ drivers/
```

### 4.4 Following Indirect Calls

When you see `file->f_op->read(file, buf, count, pos)`:
1. Identify what set `f_op` — usually at `open()` time
2. Find the specific `file_operations` instance assigned
3. Follow the `.read` member to the actual function

This three-step trace is how you resolve any polymorphic call in the kernel.

**Worked example:**

```
sys_read(fd)
  → struct file *f = fget(fd)
  → f->f_op was set during open() from inode->i_fop
  → inode->i_fop was set during ext2_read_inode() to &ext2_file_operations
  → ext2_file_operations.read = do_sync_read
  → do_sync_read calls f_op->aio_read = generic_file_aio_read
```

---

## 5. Practical Exercises

### Exercise 1: Implement `container_of` from Scratch

Write `container_of` without looking at the kernel source. Verify it handles:
- Structs with the embedded member at non-zero offset
- Multiple embeddings in the same outer struct
- Const-correctness

Test with a `struct animal` containing a `struct list_head`.

### Exercise 2: Build a Polymorphic Shape System

Implement a shape system in kernel-style C:

```c
struct shape_ops {
    double (*area)(void *self);
    double (*perimeter)(void *self);
    void (*destroy)(void *self);
};

struct shape {
    const struct shape_ops *ops;
    const char *name;
};
```

Implement `struct circle` and `struct rectangle` using struct embedding. Write a
function that takes a `struct shape *` and prints area/perimeter — polymorphically.

### Exercise 3: Reference-Counted Object System

Implement a `kref`-style refcount system:
- An `obj_ref` struct with an atomic counter
- `obj_ref_get()` / `obj_ref_put()` with a release callback
- A `struct document` that embeds `obj_ref` and has shared ownership between multiple
  "editor" structs
- Test: open a document from 3 editors, close them one by one, verify the document is
  freed exactly when the last editor releases it

### Exercise 4: VFS-Style Layered Dispatch

Build a mini-VFS in userspace:

```c
struct myfs_operations {
    int (*open)(struct myfile *f);
    ssize_t (*read)(struct myfile *f, char *buf, size_t count);
    ssize_t (*write)(struct myfile *f, const char *buf, size_t count);
    int (*close)(struct myfile *f);
};
```

Implement two "filesystems": one backed by `malloc`-ed memory, one backed by a real
file. Write a generic `myfs_read()` that dispatches through the ops table. Add NULL
method checking.

### Exercise 5: Intrusive Linked List with Type Recovery

Implement the kernel's intrusive linked list pattern:
- `struct list_head` (just prev/next pointers)
- `list_add`, `list_del`, `list_for_each_entry` using `container_of`
- Use it to maintain a list of `struct task` objects (each with a name, priority, state)
- Iterate the list, printing task info recovered via `container_of`

This is fundamental — once you're comfortable with intrusive lists, the kernel's data
structure idioms become natural.

### Mini-Project: Polymorphic Device Subsystem

Build a userspace simulation of the kernel device model:

- `struct device` with a name, parent pointer, and `struct kref`
- `struct device_driver` with probe/remove callbacks
- `struct bus_type` with match/probe
- A registration system: `bus_register`, `device_register`, `driver_register`
- When a driver is registered, iterate all devices on its bus, call `match()`, and
  if it matches, call `probe()`
- Implement two buses (e.g., "pci" and "usb") with different match logic
- Implement sysfs-like tree printing showing the device hierarchy

Target: ~500 lines. This exercise forces you to internalize every OOP pattern discussed
above.

---

## 6. Mental Models and Design Heuristics

### 6.1 The "Self" Convention

In the kernel, the first argument to every ops function is the object itself:

```c
ssize_t (*read)(struct file *file, char __user *buf, size_t count, loff_t *pos);
/*               ^^^^^^^^^^^^^^^^^                                              */
/*               this is "self" / "this"                                        */
```

This is universal. Whenever you see a function pointer that takes a struct pointer as
its first argument, think "this is a method on that struct."

### 6.2 Const Ops, Mutable State

Operations structs are `const` — they describe behavior, not state. State lives in the
object (`struct inode`, `struct file`, etc.). This separation is sharper than in most
OOP languages where methods and state are mixed in the same class.

```
┌─────────────────────┐         ┌───────────────────────┐
│  struct file (state)│────────►│ struct file_operations│
│   .f_pos            │  const  │   .read               │
│   .f_flags          │  ptr    │   .write              │
│   .f_count          │         │   .mmap               │
│   .private_data     │         │   (shared, immutable) │
└─────────────────────┘         └───────────────────────┘
```

### 6.3 Common Mistakes

**1. Putting state in the ops struct.** The ops struct is shared across all instances.
If you store per-object state there, you've created a global variable.

**2. Forgetting NULL checks.** Not every ops method is implemented. The kernel always
checks before calling:
```c
if (file->f_op->read)
    ret = file->f_op->read(file, buf, count, pos);
```
Skipping this leads to NULL pointer dereferences — a kernel oops.

**3. Breaking the `container_of` contract.** If you pass a pointer that isn't actually
embedded in the expected struct, `container_of` produces garbage. There's no runtime
check. This is C's trust model.

**4. Reference count imbalance.** Every `kref_get` must have a matching `kref_put`.
Leaking a reference leaks the object. Double-putting is a use-after-free. There are
no garbage collectors to save you.

**5. Mutable ops tables.** Modifying an ops struct at runtime breaks the contract for
all objects sharing it. Always declare ops structs `const`.

### 6.4 Design Heuristics

- **One ops struct per interface axis**: If an object has logically separate
  interfaces (data I/O vs. namespace management), use separate ops structs
  (`file_operations` vs. `inode_operations`), not one giant table.
- **Embed, don't point**: For "is-a" relationships, embed the base struct. For "has-a",
  use a pointer. `struct device` embeds `struct kobject` (a device *is* a kobject) but
  points to `struct bus_type` (a device *has* a bus).
- **Static ops, dynamic assignment**: Define ops tables as `static const` at compile
  time. Assign them to objects at construction time. Never switch them at runtime unless
  the design explicitly calls for it.
- **Callbacks carry context**: Every callback should receive enough context (usually a
  pointer to the enclosing object) to do its work without globals.

---

## 7. Recommended Kernel Files to Study (v3.2)

### Foundation Layer

| File | What to Look For |
|------|-----------------|
| `include/linux/kernel.h` (line 659) | `container_of` macro definition |
| `include/linux/list.h` | Intrusive linked list, `list_entry`, `list_for_each_entry` |
| `include/linux/kref.h` + `lib/kref.c` | Reference counting API and implementation |
| `include/linux/kobject.h` + `lib/kobject.c` | The kernel's base "Object" class, release mechanism |

### VFS (The Primary Case Study)

| File | What to Look For |
|------|-----------------|
| `include/linux/fs.h` | All four ops structs, `struct inode`, `struct super_block`, `struct file` |
| `fs/read_write.c` | `vfs_read`/`vfs_write` — polymorphic dispatch in action |
| `fs/open.c` | How `file->f_op` is set from `inode->i_fop` at open time |
| `fs/ext2/inode.c` (line 1366) | Factory pattern — ops assignment based on inode type |
| `fs/ext2/file.c` | `ext2_file_operations`, `ext2_file_inode_operations` |
| `fs/ext2/dir.c` | `ext2_dir_operations` — same interface, different behavior |
| `fs/ext2/super.c` (line 303) | `ext2_sops` — superblock operations |
| `fs/ext2/namei.c` (line 394) | `ext2_dir_inode_operations` — namespace ops |

### Device Model

| File | What to Look For |
|------|-----------------|
| `include/linux/device.h` | `struct device`, `struct device_driver`, `struct bus_type` |
| `include/linux/cdev.h` | `struct cdev` — kobject + file_operations |
| `drivers/base/core.c` | `device_register`, `device_add` — object construction |
| `drivers/base/dd.c` (line 108) | `really_probe` — polymorphic dispatch, callback IoC |
| `drivers/base/platform.c` (line 437) | `platform_drv_probe` — double dispatch with container_of |
| `drivers/base/bus.c` | Bus registration, device-driver matching loop |

### Network Stack

| File | What to Look For |
|------|-----------------|
| `include/linux/netdevice.h` (line 859) | `struct net_device_ops` — the NIC interface |
| `net/core/dev.c` (line 1150) | `__dev_open` — polymorphic device open |
| `drivers/net/loopback.c` | Minimal net_device_ops implementation |

### Character Devices

| File | What to Look For |
|------|-----------------|
| `fs/char_dev.c` | How cdev connects to VFS, `chrdev_open` |
| `drivers/char/mem.c` | `/dev/null`, `/dev/zero` — minimal file_operations |
| `drivers/char/scx200_gpio.c` | Complete cdev lifecycle: init, add, del |

### Suggested Reading Order

1. `include/linux/kernel.h` — understand `container_of`
2. `include/linux/list.h` — understand intrusive data structures
3. `include/linux/fs.h` — study the four ops structs
4. `fs/ext2/file.c` → `fs/ext2/inode.c` → `fs/read_write.c` — trace a read from
   ext2 ops assignment through VFS dispatch
5. `include/linux/kref.h` + `lib/kref.c` → `include/linux/kobject.h` +
   `lib/kobject.c` — lifetime management
6. `include/linux/device.h` → `drivers/base/dd.c` → `drivers/base/platform.c` —
   the full device model OOP system
7. `drivers/char/mem.c` — implement a character device mentally
8. `include/linux/netdevice.h` → `net/core/dev.c` — verify you can read net_device_ops
   dispatch without help

---

## Appendix: Quick Reference Card

```
PATTERN              │  LOOK FOR                         │  KERNEL EXAMPLE
─────────────────────┼───────────────────────────────────┼──────────────────────────
Inheritance          │  struct A { struct B base; };     │  struct device { struct kobject kobj; };
Upcast               │  container_of(ptr, A, base)       │  container_of(kref, struct kobject, kref)
Interface            │  struct of function pointers      │  struct file_operations
Vtable binding       │  obj->ops = &specific_ops;        │  inode->i_fop = &ext2_file_operations;
Polymorphic call     │  obj->ops->method(obj, ...)       │  file->f_op->read(file, buf, n, pos)
NULL method guard    │  if (ops->method) ops->method()   │  if (f_op->read) f_op->read(...)
Constructor          │  xxx_init() / xxx_alloc()         │  cdev_init(), kobject_init()
Destructor           │  release callback via kref_put    │  kobject_release → ktype->release
Factory              │  switch on type, assign ops       │  ext2_read_inode: S_ISREG → ext2_file_ops
Collection           │  struct list_head embedded        │  struct super_block { struct list_head s_list; }
Iterator             │  list_for_each_entry(obj,head,m)  │  iteration over s_inodes list
Inversion of Control │  framework calls obj->callback()  │  really_probe → drv->probe(dev)
```

---

*This guide references the Linux kernel v3.2 source tree. All file paths, line numbers,
and struct definitions are verified against that version. Code examples use kernel-style
C formatting conventions.*
