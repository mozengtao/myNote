# Linux Kernel VFS Architecture (v3.2)

## Table of Contents

1. [Phase 1 — Why VFS Exists](#phase-1--why-vfs-exists)
2. [Phase 2 — Core VFS Objects](#phase-2--core-vfs-objects)
3. [Phase 3 — Ops Tables as Interfaces](#phase-3--ops-tables-as-interfaces)
4. [Phase 4 — Call Path Walkthrough](#phase-4--call-path-walkthrough)
5. [Phase 5 — Object Lifetime & Refcounting](#phase-5--object-lifetime--refcounting)
6. [Phase 6 — Common Violations & Lessons](#phase-6--common-violations--lessons)
7. [Appendix A — Reusable Patterns for User-Space](#appendix-a--reusable-patterns-for-user-space)

---

## Phase 1 — Why VFS Exists

### 1.1 The Problem VFS Solves

```
+------------------------------------------------------------------------+
|                    THE PROBLEM: FILESYSTEM DIVERSITY                    |
+------------------------------------------------------------------------+

Without VFS:
    User Process
         |
         | read("/mnt/ext4/file", ...)
         v
    +--------+
    | ext4   |  <-- User must know which FS!
    +--------+
    
         | read("/mnt/nfs/file", ...)
         v
    +--------+
    | nfs    |  <-- Different API!
    +--------+
    
         | read("/proc/meminfo", ...)
         v
    +--------+
    | proc   |  <-- Yet another API!
    +--------+

With VFS:
    User Process
         |
         | read(fd, buf, count)   <-- ONE API for everything
         v
    +--------+
    |  VFS   |  <-- Uniform interface
    +--------+
         |
    +----+----+----+----+
    |    |    |    |    |
   ext4 nfs  proc fat  ...

VFS provides:
1. UNIFORM INTERFACE to all filesystems
2. COMMON IMPLEMENTATION of generic operations
3. POLYMORPHISM via ops tables (function pointers)
4. CACHING of dentries, inodes for performance
```

**中文解释：**
- **问题**：Linux支持几十种文件系统（ext4、NFS、procfs等），每种有不同实现
- **解决方案**：VFS提供统一接口，用户程序只需调用`read()`/`write()`等标准系统调用
- **核心机制**：通过函数指针表(ops)实现多态，VFS调用具体文件系统的实现

### 1.2 Why Linux Chose Ops Tables Over Inheritance

| Approach | Problems | VFS Solution |
|----------|----------|--------------|
| **C++ Inheritance** | No C++ in kernel; vtable overhead; rigid hierarchy | Ops tables are plain C |
| **Giant switch()** | Unmaintainable; every new FS modifies core | Each FS registers its own ops |
| **Callbacks per-function** | No grouping; hard to ensure consistency | Ops table groups related operations |

**The ops table pattern:**

```c
/* Each filesystem provides an ops table */
static const struct file_operations ext4_file_operations = {
    .read       = do_sync_read,
    .write      = do_sync_write,
    .open       = ext4_file_open,
    .release    = ext4_release_file,
    .mmap       = ext4_file_mmap,
    /* ... 20+ operations */
};

/* VFS calls through the table */
ssize_t vfs_read(struct file *file, ...) {
    if (file->f_op->read)
        return file->f_op->read(file, buf, count, pos);
    else
        return do_sync_read(file, buf, count, pos);
}
```

**Why this is better than inheritance:**

```
+------------------------------------------------------------------------+
|                    OPS TABLES vs INHERITANCE                            |
+------------------------------------------------------------------------+

Inheritance (C++):                   Ops Tables (C):
+-----------------+                  +------------------+
| FileSystemBase  |                  | struct file_ops  |  <-- Just data
|   virtual read()|                  |   .read = ...    |
|   virtual write()|                 |   .write = ...   |
+-----------------+                  +------------------+
        ^                                    |
        |                            +-------+-------+
   +----+----+                       |       |       |
   |         |                      ext4    nfs    proc
  Ext4      NFS                      ops     ops    ops
  
Problems:                            Advantages:
- Requires C++ runtime               - Plain C, no runtime
- Inflexible hierarchy               - Composable (pick and choose)
- All-or-nothing override            - NULL = use default
- vtable per object                  - One table per FS type
```

### 1.3 Why Filesystems Never Call Each Other Directly

```
+------------------------------------------------------------------------+
|                    LAYERING DISCIPLINE                                  |
+------------------------------------------------------------------------+

FORBIDDEN:
    ext4 ───────────────> nfs
         direct call!     
         
    WHY: Creates tight coupling; changes to NFS break ext4

REQUIRED:
    ext4 ───> VFS ───> nfs
         via ops     via ops
         
    WHY: VFS mediates all interaction; FS implementations are isolated

Example - mounting NFS over ext4 directory:
    /mnt/ext4/nfs_mount/file
    
    1. VFS resolves path
    2. At mount point, VFS switches to NFS ops
    3. ext4 and NFS never communicate directly
```

**Contract:** Filesystems ONLY interact with:
1. The VFS layer (via defined callbacks)
2. The block layer (for disk-based FS)
3. Network layer (for network FS)

They NEVER call other filesystem code directly.

---

## Phase 2 — Core VFS Objects

### 2.1 Object Relationship Overview

```
+------------------------------------------------------------------------+
|                    VFS OBJECT RELATIONSHIPS                             |
+------------------------------------------------------------------------+

                         +------------------+
                         |  super_block     |
                         |  (per-mount)     |
                         +--------+---------+
                                  |
                    +-------------+-------------+
                    |                           |
            +-------v-------+           +-------v-------+
            |    inode      |           |    inode      |
            | (per-file)    |           | (per-file)    |
            +-------+-------+           +-------+-------+
                    |                           |
           +--------+--------+                  |
           |                 |                  |
    +------v------+   +------v------+    +------v------+
    |   dentry    |   |   dentry    |    |   dentry    |
    | (path comp) |   | (path comp) |    | (path comp) |
    +------+------+   +------+------+    +------+------+
           |                 |                  |
    +------v------+   +------v------+    +------v------+
    |    file     |   |    file     |    |    file     |
    | (per-open)  |   | (per-open)  |    | (per-open)  |
    +-------------+   +-------------+    +-------------+
           ^                 ^                  ^
           |                 |                  |
        Process A         Process A          Process B
        fd=3              fd=4               fd=3

RELATIONSHIPS:
- One super_block per mounted filesystem
- One inode per unique file (shared across opens)
- One dentry per path component (cached for lookup speed)
- One file struct per open() call (tracks position, flags)
```

**中文解释：**
- **super_block**：每个挂载的文件系统一个，包含文件系统级元数据
- **inode**：每个唯一文件一个，多次打开同一文件共享同一inode
- **dentry**：路径组件的缓存，加速路径查找
- **file**：每次`open()`调用一个，跟踪当前位置和打开标志

### 2.2 struct super_block

```c
/* include/linux/fs.h */
struct super_block {
    struct list_head    s_list;         /* [LIST] All superblocks */
    dev_t               s_dev;          /* [ID] Device identifier */
    unsigned long       s_blocksize;    /* [SIZE] Block size in bytes */
    loff_t              s_maxbytes;     /* [LIMIT] Max file size */
    struct file_system_type *s_type;    /* [TYPE] FS type (ext4, nfs...) */
    const struct super_operations *s_op;/* [OPS] Superblock operations */
    struct dentry       *s_root;        /* [ROOT] Root dentry */
    struct rw_semaphore s_umount;       /* [LOCK] Unmount semaphore */
    int                 s_count;        /* [REF] Reference count */
    atomic_t            s_active;       /* [REF] Active reference count */
    struct list_head    s_inodes;       /* [LIST] All inodes */
    void                *s_fs_info;     /* [PRIVATE] FS-specific data */
    /* ... more fields ... */
};
```

| Aspect | Details |
|--------|---------|
| **Who creates** | VFS core via `mount()` → filesystem's `fill_super()` |
| **Who owns** | VFS core; filesystem populates fields |
| **Who destroys** | VFS core via `umount()` → filesystem's `put_super()` |
| **Invariants** | `s_root` set before mount completes; `s_op` always valid |

### 2.3 struct inode

```c
/* include/linux/fs.h */
struct inode {
    umode_t             i_mode;         /* [TYPE] File type and permissions */
    uid_t               i_uid;          /* [OWNER] User ID */
    gid_t               i_gid;          /* [OWNER] Group ID */
    const struct inode_operations *i_op;/* [OPS] Inode operations */
    struct super_block  *i_sb;          /* [PARENT] Owning superblock */
    unsigned long       i_ino;          /* [ID] Inode number */
    union {
        const unsigned int i_nlink;     /* [LINK] Hard link count */
        unsigned int __i_nlink;
    };
    loff_t              i_size;         /* [SIZE] File size */
    struct timespec     i_atime;        /* [TIME] Access time */
    struct timespec     i_mtime;        /* [TIME] Modification time */
    struct timespec     i_ctime;        /* [TIME] Change time */
    atomic_t            i_count;        /* [REF] Reference count */
    const struct file_operations *i_fop;/* [OPS] Default file ops */
    struct address_space i_data;        /* [CACHE] Page cache */
    void                *i_private;     /* [PRIVATE] FS-specific data */
    /* ... more fields ... */
};
```

| Aspect | Details |
|--------|---------|
| **Who creates** | `alloc_inode()` → FS's `alloc_inode()` or generic slab |
| **Who owns** | VFS; FS populates via `read_inode()` or `get_inode()` |
| **Who destroys** | VFS via `iput()` → FS's `destroy_inode()` when refcount=0 |
| **Invariants** | `i_sb` always valid; `i_count >= 1` while in use |

### 2.4 struct dentry

```c
/* include/linux/dcache.h */
struct dentry {
    unsigned int        d_flags;        /* [FLAGS] Dentry flags */
    seqcount_t          d_seq;          /* [LOCK] Per-dentry seqlock */
    struct hlist_bl_node d_hash;        /* [HASH] Lookup hash list */
    struct dentry       *d_parent;      /* [TREE] Parent directory */
    struct qstr         d_name;         /* [NAME] Name of this component */
    struct inode        *d_inode;       /* [LINK] Associated inode (or NULL) */
    unsigned int        d_count;        /* [REF] Reference count */
    spinlock_t          d_lock;         /* [LOCK] Per-dentry lock */
    const struct dentry_operations *d_op;/* [OPS] Dentry operations */
    struct super_block  *d_sb;          /* [PARENT] Owning superblock */
    void                *d_fsdata;      /* [PRIVATE] FS-specific data */
    struct list_head    d_subdirs;      /* [TREE] Child dentries */
    struct list_head    d_alias;        /* [LINK] Inode alias list */
    /* ... more fields ... */
};
```

**Dentry States:**

```
+------------------------------------------------------------------------+
|                    DENTRY STATE MACHINE                                 |
+------------------------------------------------------------------------+

                    +---------------+
                    |   UNUSED      |  d_count = 0, on LRU
                    +-------+-------+
                            |
                            | dget() / path lookup
                            v
                    +-------+-------+
                    |   IN USE      |  d_count > 0
                    +-------+-------+
                            |
                            | dput() when d_count → 0
                            v
                    +-------+-------+
                    |   ON LRU      |  Cached for reuse
                    +-------+-------+
                            |
                            | Memory pressure / invalidation
                            v
                    +-------+-------+
                    |   FREED       |  Memory returned
                    +---------------+

Special: NEGATIVE DENTRY
    - d_inode = NULL
    - Caches "file does not exist" result
    - Prevents repeated failed lookups
```

| Aspect | Details |
|--------|---------|
| **Who creates** | `d_alloc()` during path lookup |
| **Who owns** | dcache (VFS); FS can attach `d_op` and `d_fsdata` |
| **Who destroys** | `dput()` → LRU → `d_delete()` when pruned |
| **Invariants** | `d_parent` always valid (root is its own parent); `d_sb` always valid |

### 2.5 struct file

```c
/* include/linux/fs.h */
struct file {
    union {
        struct list_head fu_list;       /* [LIST] File list */
        struct rcu_head  fu_rcuhead;    /* [RCU] RCU callback */
    } f_u;
    struct path         f_path;         /* [PATH] Contains dentry and mnt */
#define f_dentry        f_path.dentry
#define f_vfsmnt        f_path.mnt
    const struct file_operations *f_op; /* [OPS] File operations */
    spinlock_t          f_lock;         /* [LOCK] Protects f_pos, f_flags */
    atomic_long_t       f_count;        /* [REF] Reference count */
    unsigned int        f_flags;        /* [FLAGS] O_RDONLY, O_NONBLOCK... */
    fmode_t             f_mode;         /* [MODE] FMODE_READ, FMODE_WRITE */
    loff_t              f_pos;          /* [POS] Current file position */
    struct fown_struct  f_owner;        /* [ASYNC] For SIGIO */
    const struct cred   *f_cred;        /* [SEC] Opener's credentials */
    struct file_ra_state f_ra;          /* [CACHE] Read-ahead state */
    void                *private_data;  /* [PRIVATE] Driver-specific data */
    struct address_space *f_mapping;    /* [CACHE] Page cache mapping */
    /* ... more fields ... */
};
```

| Aspect | Details |
|--------|---------|
| **Who creates** | `get_empty_filp()` during `open()` syscall |
| **Who owns** | Process (via fd table); kernel (via reference) |
| **Who destroys** | `fput()` → `__fput()` → FS's `release()` when refcount=0 |
| **Invariants** | `f_count >= 1` while open; `f_op` always valid after open |

### 2.6 Object Hierarchy Diagram

```
+------------------------------------------------------------------------+
|                    COMPLETE OBJECT HIERARCHY                            |
+------------------------------------------------------------------------+

Process A                          Process B
+------------+                     +------------+
| fd_table   |                     | fd_table   |
| [0] stdin  |                     | [0] stdin  |
| [1] stdout |                     | [1] stdout |
| [2] stderr |                     | [2] stderr |
| [3]--------|--+    +-------------|----[3]     |
| [4]--------|--|----+             +------------+
+------------+  |    |
                |    |
         +------+    +------+
         |                  |
    +----v----+        +----v----+
    |  file A |        |  file B |  (different opens of same file)
    | f_pos=0 |        | f_pos=100|
    | f_flags |        | f_flags |
    +----+----+        +----+----+
         |                  |
         +--------+---------+
                  |
            +-----v-----+
            |  dentry   |  (shared, cached)
            | "/tmp/x"  |
            +-----+-----+
                  |
            +-----v-----+
            |  inode    |  (shared, unique per file)
            | i_ino=123 |
            | i_nlink=2 |
            +-----+-----+
                  |
            +-----v-----+
            |super_block|  (shared, one per mount)
            | ext4 on   |
            | /dev/sda1 |
            +-----------+
```

---

## Phase 3 — Ops Tables as Interfaces

### 3.1 struct file_operations

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
    int (*fasync) (int, struct file *, int);
    int (*lock) (struct file *, int, struct file_lock *);
    ssize_t (*sendpage) (struct file *, struct page *, int, size_t, loff_t *, int);
    ssize_t (*splice_write)(struct pipe_inode_info *, struct file *, loff_t *, size_t, unsigned int);
    ssize_t (*splice_read)(struct file *, loff_t *, struct pipe_inode_info *, size_t, unsigned int);
    int (*setlease)(struct file *, long, struct file_lock **);
    long (*fallocate)(struct file *file, int mode, loff_t offset, loff_t len);
};
```

**Contract Table:**

| Callback | When Called | VFS Guarantees | FS Must |
|----------|-------------|----------------|---------|
| `open` | File opened | inode locked | Return 0 or error; may allocate `private_data` |
| `release` | Last close | Called once per open | Free `private_data`; return 0 |
| `read` | `read()` syscall | File opened for read | Copy to user buffer; update `*pos`; return bytes |
| `write` | `write()` syscall | File opened for write | Copy from user buffer; return bytes written |
| `llseek` | `lseek()` syscall | Valid `whence` | Return new position or error |
| `mmap` | `mmap()` syscall | Valid VMA | Set up page tables; return 0 |
| `fsync` | `fsync()` syscall | File opened for write | Flush data to disk; return 0 |

**Who Fills It:**

```c
/* Example: ext4 regular file operations */
const struct file_operations ext4_file_operations = {
    .llseek     = ext4_llseek,
    .read       = do_sync_read,         /* Generic implementation */
    .write      = do_sync_write,        /* Generic implementation */
    .aio_read   = generic_file_aio_read,/* Generic implementation */
    .aio_write  = ext4_file_write,      /* FS-specific */
    .unlocked_ioctl = ext4_ioctl,
    .mmap       = ext4_file_mmap,
    .open       = ext4_file_open,
    .release    = ext4_release_file,
    .fsync      = ext4_sync_file,
    .splice_read    = generic_file_splice_read,
    .splice_write   = generic_file_splice_write,
    .fallocate  = ext4_fallocate,
};
```

### 3.2 struct inode_operations

```c
/* include/linux/fs.h */
struct inode_operations {
    struct dentry * (*lookup) (struct inode *,struct dentry *, struct nameidata *);
    void * (*follow_link) (struct dentry *, struct nameidata *);
    int (*permission) (struct inode *, int);
    int (*readlink) (struct dentry *, char __user *,int);
    void (*put_link) (struct dentry *, struct nameidata *, void *);
    int (*create) (struct inode *,struct dentry *,int, struct nameidata *);
    int (*link) (struct dentry *,struct inode *,struct dentry *);
    int (*unlink) (struct inode *,struct dentry *);
    int (*symlink) (struct inode *,struct dentry *,const char *);
    int (*mkdir) (struct inode *,struct dentry *,int);
    int (*rmdir) (struct inode *,struct dentry *);
    int (*mknod) (struct inode *,struct dentry *,int,dev_t);
    int (*rename) (struct inode *, struct dentry *, struct inode *, struct dentry *);
    void (*truncate) (struct inode *);
    int (*setattr) (struct dentry *, struct iattr *);
    int (*getattr) (struct vfsmount *mnt, struct dentry *, struct kstat *);
    int (*setxattr) (struct dentry *, const char *,const void *,size_t,int);
    ssize_t (*getxattr) (struct dentry *, const char *, void *, size_t);
    ssize_t (*listxattr) (struct dentry *, char *, size_t);
    int (*removexattr) (struct dentry *, const char *);
    int (*fiemap)(struct inode *, struct fiemap_extent_info *, u64 start, u64 len);
};
```

**Contract Table:**

| Callback | When Called | VFS Guarantees | FS Must |
|----------|-------------|----------------|---------|
| `lookup` | Path resolution | Parent inode locked | Return dentry (positive or negative) |
| `create` | Creating regular file | Parent locked; dentry negative | Create inode; `d_instantiate()` |
| `mkdir` | Creating directory | Parent locked; dentry negative | Create dir inode; `d_instantiate()` |
| `unlink` | Deleting file | Parent locked; victim exists | Decrement `i_nlink`; remove dentry |
| `rmdir` | Deleting directory | Parent locked; dir empty | Decrement `i_nlink`; remove dentry |
| `rename` | Renaming file | Both parents locked | Move dentry; update links |
| `permission` | Permission check | inode valid | Return 0 if allowed, -EACCES if not |

### 3.3 struct super_operations

```c
/* include/linux/fs.h */
struct super_operations {
    struct inode *(*alloc_inode)(struct super_block *sb);
    void (*destroy_inode)(struct inode *);
    void (*dirty_inode) (struct inode *, int flags);
    int (*write_inode) (struct inode *, struct writeback_control *wbc);
    int (*drop_inode) (struct inode *);
    void (*evict_inode) (struct inode *);
    void (*put_super) (struct super_block *);
    void (*write_super) (struct super_block *);
    int (*sync_fs)(struct super_block *sb, int wait);
    int (*freeze_fs) (struct super_block *);
    int (*unfreeze_fs) (struct super_block *);
    int (*statfs) (struct dentry *, struct kstatfs *);
    int (*remount_fs) (struct super_block *, int *, char *);
    void (*umount_begin) (struct super_block *);
    int (*show_options)(struct seq_file *, struct vfsmount *);
};
```

**Contract Table:**

| Callback | When Called | VFS Guarantees | FS Must |
|----------|-------------|----------------|---------|
| `alloc_inode` | New inode needed | Called once per inode | Allocate (embedded in FS struct) |
| `destroy_inode` | Inode being freed | Inode evicted | Free FS-specific data |
| `write_inode` | Inode dirty, writeback | Inode locked | Write to disk; return 0 |
| `evict_inode` | Inode dropped (nlink=0) | Inode unhashed | Truncate; free blocks |
| `put_super` | Unmounting | All inodes gone | Free FS-private data in `s_fs_info` |
| `statfs` | `statfs()` syscall | Valid superblock | Fill `kstatfs` with FS stats |
| `sync_fs` | `sync()` syscall | Called periodically | Flush all pending data |

### 3.4 How VFS Invokes Ops

```
+------------------------------------------------------------------------+
|                    VFS DISPATCH PATTERN                                 |
+------------------------------------------------------------------------+

User: read(fd, buf, 100)
         |
         v
sys_read(fd, buf, 100)                    [syscall entry]
         |
         v
fget(fd) → file                           [get file from fd table]
         |
         v
vfs_read(file, buf, 100, &pos)            [VFS layer]
         |
    +----+----+
    |         |
    v         v
file->f_op   file->f_op->read             [ops table lookup]
    |              |
    |              v
    |         ext4_file_read(file, ...)    [FS implementation]
    |              |
    |              v
    |         generic_file_aio_read(...)   [common implementation]
    |              |
    +------<-------+
         |
         v
Return bytes read to user

KEY PATTERN:
    object->ops->method(object, args...)
    
VFS NEVER knows which FS is handling the call!
```

---

## Phase 4 — Call Path Walkthrough

### 4.1 Tracing read() End-to-End

```
+------------------------------------------------------------------------+
|                    read() SYSCALL COMPLETE PATH                         |
+------------------------------------------------------------------------+

USER SPACE
    |
    | read(fd=3, buf, count=100)
    |
+===|====================================================================+
    |
    v
SYSCALL ENTRY (arch/x86/kernel/entry_64.S)
    |
    | sys_read(fd, buf, count)
    v
+-----------------------------------------------------------------------+
|                         fs/read_write.c                                |
+-----------------------------------------------------------------------+
    |
    | SYSCALL_DEFINE3(read, ...)
    v
    |
    | struct file *file = fget_light(fd, &fput_needed);
    |     [Get file from fd table, increment refcount]
    |
    | if (!file) return -EBADF;
    |
    v
    |
    | ret = vfs_read(file, buf, count, &file->f_pos);
    |
    +-----------------------------+
                                  |
                                  v
+-----------------------------------------------------------------------+
| vfs_read() - VFS LAYER                                                 |
+-----------------------------------------------------------------------+
    |
    | /* [CHECK] File opened for reading? */
    | if (!(file->f_mode & FMODE_READ))
    |     return -EBADF;
    |
    | /* [CHECK] Ops exist? */
    | if (!file->f_op || (!file->f_op->read && !file->f_op->aio_read))
    |     return -EINVAL;
    |
    | /* [CHECK] User buffer accessible? */
    | if (!access_ok(VERIFY_WRITE, buf, count))
    |     return -EFAULT;
    |
    | /* [CHECK] Security: may read? */
    | ret = rw_verify_area(READ, file, pos, count);
    |
    | /* [DISPATCH] Call FS-specific read */
    | if (file->f_op->read)
    |     ret = file->f_op->read(file, buf, count, pos);
    | else
    |     ret = do_sync_read(file, buf, count, pos);
    |
    +-----------------------------+
                                  |
                                  v
+-----------------------------------------------------------------------+
| FS-SPECIFIC (e.g., ext4 via generic implementation)                   |
+-----------------------------------------------------------------------+
    |
    | do_sync_read(file, buf, count, pos)
    |     |
    |     | /* Build kiocb for AIO infrastructure */
    |     | init_sync_kiocb(&kiocb, filp);
    |     |
    |     | /* Call AIO read */
    |     | ret = filp->f_op->aio_read(&kiocb, &iov, 1, pos);
    |     |
    |     +-----------------------------+
    |                                   |
    v                                   v
    |                    generic_file_aio_read()
    |                         |
    |                         | /* Access page cache */
    |                         | do_generic_file_read(file, ppos, ...)
    |                         |     |
    |                         |     | /* Find or create page */
    |                         |     | page = find_get_page(mapping, index);
    |                         |     | if (!page) {
    |                         |     |     /* Read from disk via address_space_operations */
    |                         |     |     mapping->a_ops->readpage(file, page);
    |                         |     | }
    |                         |     |
    |                         |     | /* Copy to user space */
    |                         |     | copy_page_to_iter(page, ...);
    |                         |     |
    |                         +-----+
    |
    +-----------------------------+
                                  |
                                  v
+-----------------------------------------------------------------------+
| RETURN PATH                                                            |
+-----------------------------------------------------------------------+
    |
    | /* Update accounting */
    | fsnotify_access(file);
    | add_rchar(current, ret);
    |
    | /* Release file reference */
    | fput_light(file, fput_needed);
    |
    | return ret;  /* Bytes read or error */
    |
+===|====================================================================+
    |
    v
USER SPACE receives return value
```

**中文解释：**
- **系统调用入口**：用户调用`read()`进入内核
- **VFS层**：验证权限、检查操作合法性
- **文件系统层**：通过`f_op->read`或`f_op->aio_read`调用具体实现
- **页缓存**：大多数读操作通过页缓存，使用`address_space_operations`
- **返回路径**：更新统计信息，释放引用，返回用户空间

### 4.2 Layering Rules

```
+------------------------------------------------------------------------+
|                    STRICT LAYERING                                      |
+------------------------------------------------------------------------+

Layer 0: User Space
    - Only uses system calls
    - NEVER accesses kernel structures directly
    
Layer 1: Syscall Entry
    - Translates fd to file*
    - Validates user pointers
    
Layer 2: VFS
    - Uniform interface for all filesystems
    - Calls through ops tables
    - Manages caches (dcache, icache)
    
Layer 3: Filesystem Implementation
    - Implements ops callbacks
    - May use generic helpers
    - Accesses disk or network
    
Layer 4: Block/Network Layer
    - Actual I/O to devices
    - Only accessed by Layer 3

FORBIDDEN:
    Layer 2 → Layer 4 (VFS directly doing I/O)
    Layer 3 → Layer 3 (FS calling another FS)
    Layer 1 → Layer 3 (syscall bypassing VFS)
```

---

## Phase 5 — Object Lifetime & Refcounting

### 5.1 Reference Counting Rules

**struct file:**

```c
/* Reference counting for files */
#define get_file(x)     atomic_long_inc(&(x)->f_count)
#define file_count(x)   atomic_long_read(&(x)->f_count)

/* Acquire reference */
struct file *fget(int fd);           /* From fd table, increments count */
void get_file(struct file *f);       /* Increment count */

/* Release reference */
void fput(struct file *f);           /* Decrement, free if 0 */
void fput_light(struct file *f, int); /* Optimized fput */
```

**Lifecycle:**

```
+------------------------------------------------------------------------+
|                    FILE REFERENCE COUNTING                              |
+------------------------------------------------------------------------+

open() syscall:
    get_empty_filp()    → f_count = 1
    fd_install(fd, file)  → fd table holds reference
    return fd

read(fd)/write(fd):
    fget(fd)            → f_count++
    ... do I/O ...
    fput(file)          → f_count--

dup(fd):
    fget(fd)            → f_count++
    fd_install(newfd, file) → fd table holds reference
    
close(fd):
    filp_close()        → fput()
    fput()              → f_count--
    if (f_count == 0)
        __fput()        → f_op->release()
                        → dput(dentry)
                        → mntput(mnt)
                        → file_free()

INVARIANT: f_count >= 1 while any fd references the file
```

**struct inode:**

```c
/* Reference counting for inodes */
atomic_t i_count;                    /* Reference count */

void ihold(struct inode *inode);     /* Increment count (must already have ref) */
void iget(struct inode *inode);      /* Get reference (used internally) */
void iput(struct inode *inode);      /* Release reference */
```

**Lifecycle:**

```
+------------------------------------------------------------------------+
|                    INODE REFERENCE COUNTING                             |
+------------------------------------------------------------------------+

Creation (e.g., creat()):
    new_inode()         → i_count = 1
    d_instantiate()     → dentry holds reference
    
Path lookup:
    iget_locked()       → i_count++ (or create new)
    
File open:
    __dentry_open()     → ihold(inode) (indirectly via dentry)
    
File close:
    __fput()            → dput() → iput()
    
iput() logic:
    i_count--
    if (i_count == 0) {
        if (i_nlink == 0)
            evict()     → s_op->evict_inode()
                        → actually delete from disk
        else
            /* Keep in cache (LRU) */
    }

KEY: i_nlink tracks disk references (hard links)
     i_count tracks in-memory references
```

**struct dentry:**

```c
/* Reference counting for dentries */
unsigned int d_count;                /* Reference count */

struct dentry *dget(struct dentry *); /* Increment count */
void dput(struct dentry *dentry);     /* Decrement count */
```

**Lifecycle:**

```
+------------------------------------------------------------------------+
|                    DENTRY REFERENCE COUNTING                            |
+------------------------------------------------------------------------+

Path lookup:
    __d_lookup()        → d_count++ if found
    d_alloc()           → d_count = 1 (new dentry)
    
File open:
    path_openat()       → holds dentry reference
    
dput() logic:
    d_count--
    if (d_count == 1) {
        if (d_op->d_delete && d_op->d_delete(dentry))
            dentry_kill()   /* Delete immediately */
        else if (d_unhashed())
            dentry_kill()   /* Already removed from hash */
        else
            dentry_lru_add() /* Cache on LRU */
    }

NEGATIVE DENTRY:
    - d_inode == NULL
    - Caches "file does not exist"
    - Still has d_count, cached normally
```

### 5.2 RCU in VFS

```
+------------------------------------------------------------------------+
|                    RCU USAGE IN VFS                                     |
+------------------------------------------------------------------------+

RCU protects:
1. Dentry hash table lookups (read-side)
2. Dentry tree traversal (path lookup)
3. File table access (fget_light)

RCU Path Lookup (rcu-walk):
    do_lookup() in RCU mode:
        rcu_read_lock()
        dentry = __d_lookup_rcu(parent, name, ...)
        /* No d_count increment! */
        /* If dentry valid, continue */
        /* If need to block → drop to ref-walk */
        rcu_read_unlock()

Benefits:
    - No cache line bouncing on d_count
    - Fast path for cache hits
    - Falls back to ref-walk when needed

RCU Freeing:
    struct file freed via:
        call_rcu(&f->f_u.fu_rcuhead, file_free_rcu)
    
    struct dentry freed via:
        call_rcu(&dentry->d_u.d_rcu, __d_free)
```

### 5.3 Common Lifetime Bugs

| Bug | Symptom | Cause | Prevention |
|-----|---------|-------|------------|
| **Use-after-free (inode)** | Panic, corruption | `iput()` then access | Always check `i_count` before access |
| **Use-after-free (file)** | Panic, corruption | `fput()` then access | Never save file* beyond scope |
| **Refcount leak** | Memory exhaustion | Missing `fput()`/`iput()` | Match every get with put |
| **Double free** | Panic | Two `fput()` for one `fget()` | Track reference origin |
| **Dentry cache pollution** | Stale data | Not invalidating after rename | Use `d_invalidate()` |
| **Negative dentry leak** | Memory exhaustion | Infinite failed lookups | LRU pruning, `d_drop()` |

**Example Bug: Missing fput()**

```c
/* WRONG */
int broken_read(int fd) {
    struct file *file = fget(fd);
    if (!file) return -EBADF;
    
    if (some_condition) {
        return -EINVAL;  /* [BUG] fput() not called! */
    }
    
    /* ... read ... */
    fput(file);
    return 0;
}

/* CORRECT */
int correct_read(int fd) {
    struct file *file = fget(fd);
    int ret;
    
    if (!file) return -EBADF;
    
    if (some_condition) {
        ret = -EINVAL;
        goto out;
    }
    
    /* ... read ... */
    ret = 0;
out:
    fput(file);
    return ret;
}
```

---

## Phase 6 — Common Violations & Lessons

### 6.1 Layering Violations

**Violation 1: Filesystem Accessing Another Filesystem**

```c
/* WRONG: ext4 directly calling NFS functions */
int ext4_read_remote(struct inode *inode) {
    return nfs_readpage(inode, ...);  /* [VIOLATION] Direct FS call */
}

/* CORRECT: Go through VFS */
int ext4_read_remote(struct inode *inode) {
    /* Use standard VFS interface */
    return vfs_read(file, buf, count, pos);
}
```

**Violation 2: Bypassing Ops Table**

```c
/* WRONG: Hardcoding knowledge of FS type */
ssize_t broken_vfs_read(struct file *file, ...) {
    if (file->f_dentry->d_sb->s_magic == EXT4_MAGIC) {
        return ext4_specific_read(...);  /* [VIOLATION] */
    }
    return file->f_op->read(...);
}

/* CORRECT: Always use ops table */
ssize_t correct_vfs_read(struct file *file, ...) {
    return file->f_op->read(file, buf, count, pos);
}
```

### 6.2 Refcount Leaks

**Pattern: Error Path Leak**

```c
/* WRONG: Leak on error */
int broken_open(struct inode *inode, struct file *file) {
    struct my_data *data = kmalloc(...);
    if (!data) return -ENOMEM;
    
    data->buffer = kmalloc(...);
    if (!data->buffer) {
        return -ENOMEM;  /* [LEAK] data not freed */
    }
    
    file->private_data = data;
    return 0;
}

/* CORRECT: Clean up on error */
int correct_open(struct inode *inode, struct file *file) {
    struct my_data *data = kmalloc(...);
    if (!data) return -ENOMEM;
    
    data->buffer = kmalloc(...);
    if (!data->buffer) {
        kfree(data);  /* [FIX] Free on error */
        return -ENOMEM;
    }
    
    file->private_data = data;
    return 0;
}
```

### 6.3 Private Data Misuse

**Pattern: Wrong Container Recovery**

```c
/* WRONG: Incorrect private data cast */
struct my_inode_info {
    int some_field;
    struct inode vfs_inode;  /* NOT first field! */
};

struct my_inode_info *get_my_info(struct inode *inode) {
    return (struct my_inode_info *)inode;  /* [BUG] Wrong offset! */
}

/* CORRECT: Use container_of */
struct my_inode_info *get_my_info(struct inode *inode) {
    return container_of(inode, struct my_inode_info, vfs_inode);
}

/* OR: Put VFS struct first */
struct my_inode_info {
    struct inode vfs_inode;  /* First field */
    int some_field;
};
/* Now simple cast works (but container_of is still cleaner) */
```

### 6.4 Summary: Reusable Patterns

```
+------------------------------------------------------------------------+
|                    VFS PATTERNS FOR USER-SPACE                          |
+------------------------------------------------------------------------+

PATTERN 1: Ops Table Polymorphism
    struct file_ops {
        int (*read)(struct file *, void *, size_t);
        int (*write)(struct file *, const void *, size_t);
        void (*close)(struct file *);
    };
    
    struct file {
        const struct file_ops *ops;
        void *private_data;
    };
    
    /* Dispatch */
    int file_read(struct file *f, void *buf, size_t n) {
        return f->ops->read(f, buf, n);
    }

PATTERN 2: Reference Counting
    struct object {
        atomic_t refcount;
    };
    
    void object_get(struct object *o) {
        atomic_inc(&o->refcount);
    }
    
    void object_put(struct object *o) {
        if (atomic_dec_and_test(&o->refcount)) {
            object_free(o);
        }
    }

PATTERN 3: Embedded Private Data
    struct my_file {
        struct file base;     /* Generic part first */
        int my_private_field; /* Extension */
    };
    
    /* Recovery */
    struct my_file *my = container_of(base, struct my_file, base);

PATTERN 4: Two-Level Caching
    - Fast path: Check cache (dentry → inode → data)
    - Slow path: Load from storage
    - LRU eviction when memory pressure

PATTERN 5: Uniform Interface
    - Generic layer validates and dispatches
    - Implementation layer only handles specific logic
    - Generic layer NEVER knows about specific types
```

---

## Appendix A — Reusable Patterns for User-Space

### A.1 Complete User-Space VFS-Like Framework

```c
/*
 * user_vfs.c - VFS-inspired polymorphic file abstraction
 *
 * Demonstrates:
 * 1. Ops table polymorphism
 * 2. Reference counting
 * 3. Private data embedding
 * 4. Layered dispatching
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

/*
 * ============================================================
 * GENERIC LAYER (like VFS)
 * ============================================================
 */

/* Forward declarations */
struct file;
struct file_ops;

/* [OPS TABLE] File operations interface */
struct file_ops {
    const char *name;
    int (*open)(struct file *f, const char *path, int flags);
    ssize_t (*read)(struct file *f, void *buf, size_t count);
    ssize_t (*write)(struct file *f, const void *buf, size_t count);
    int (*close)(struct file *f);
};

/* [GENERIC OBJECT] Base file structure */
struct file {
    atomic_int refcount;              /* [REF] Reference count */
    const struct file_ops *ops;       /* [OPS] Operations table */
    char path[256];                   /* [DATA] File path */
    int flags;                        /* [DATA] Open flags */
    size_t pos;                       /* [DATA] Current position */
};

/* [REFCOUNT] Get reference */
static inline struct file *file_get(struct file *f) {
    if (f) {
        atomic_fetch_add(&f->refcount, 1);
    }
    return f;
}

/* [REFCOUNT] Release reference */
static inline void file_put(struct file *f) {
    if (f && atomic_fetch_sub(&f->refcount, 1) == 1) {
        /* Last reference - call close and free */
        if (f->ops && f->ops->close) {
            f->ops->close(f);
        }
        free(f);
    }
}

/* [DISPATCH] Generic read - dispatches to ops */
ssize_t vfs_read(struct file *f, void *buf, size_t count) {
    if (!f || !f->ops) return -1;
    if (!f->ops->read) return -1;  /* No read support */
    return f->ops->read(f, buf, count);
}

/* [DISPATCH] Generic write - dispatches to ops */
ssize_t vfs_write(struct file *f, const void *buf, size_t count) {
    if (!f || !f->ops) return -1;
    if (!f->ops->write) return -1;
    return f->ops->write(f, buf, count);
}

/*
 * ============================================================
 * IMPLEMENTATION 1: Memory File (like ramfs)
 * ============================================================
 */

/* [PRIVATE] Memory file extension */
struct mem_file {
    struct file base;       /* [EMBED] Must be first for safe casting */
    char *buffer;           /* [PRIVATE] Data buffer */
    size_t size;            /* [PRIVATE] Current size */
    size_t capacity;        /* [PRIVATE] Buffer capacity */
};

/* [HELPER] Recover private data */
static struct mem_file *to_mem_file(struct file *f) {
    return (struct mem_file *)f;  /* Safe because base is first */
}

static int mem_open(struct file *f, const char *path, int flags) {
    struct mem_file *mf = to_mem_file(f);
    mf->capacity = 4096;
    mf->buffer = malloc(mf->capacity);
    mf->size = 0;
    if (!mf->buffer) return -1;
    printf("[mem] Opened: %s\n", path);
    return 0;
}

static ssize_t mem_read(struct file *f, void *buf, size_t count) {
    struct mem_file *mf = to_mem_file(f);
    size_t available = mf->size - f->pos;
    size_t to_read = count < available ? count : available;
    
    if (to_read > 0) {
        memcpy(buf, mf->buffer + f->pos, to_read);
        f->pos += to_read;
    }
    return to_read;
}

static ssize_t mem_write(struct file *f, const void *buf, size_t count) {
    struct mem_file *mf = to_mem_file(f);
    
    /* Expand buffer if needed */
    if (f->pos + count > mf->capacity) {
        size_t new_cap = (f->pos + count) * 2;
        char *new_buf = realloc(mf->buffer, new_cap);
        if (!new_buf) return -1;
        mf->buffer = new_buf;
        mf->capacity = new_cap;
    }
    
    memcpy(mf->buffer + f->pos, buf, count);
    f->pos += count;
    if (f->pos > mf->size) mf->size = f->pos;
    return count;
}

static int mem_close(struct file *f) {
    struct mem_file *mf = to_mem_file(f);
    printf("[mem] Closed: %s (size=%zu)\n", f->path, mf->size);
    free(mf->buffer);
    return 0;
}

/* [OPS TABLE] Memory file operations */
static const struct file_ops mem_file_ops = {
    .name = "memfs",
    .open = mem_open,
    .read = mem_read,
    .write = mem_write,
    .close = mem_close,
};

/*
 * ============================================================
 * IMPLEMENTATION 2: Log File (append-only)
 * ============================================================
 */

struct log_file {
    struct file base;
    FILE *fp;
    int entry_count;
};

static struct log_file *to_log_file(struct file *f) {
    return (struct log_file *)f;
}

static int log_open(struct file *f, const char *path, int flags) {
    struct log_file *lf = to_log_file(f);
    lf->fp = fopen(path, "a+");
    lf->entry_count = 0;
    if (!lf->fp) return -1;
    printf("[log] Opened: %s\n", path);
    return 0;
}

static ssize_t log_read(struct file *f, void *buf, size_t count) {
    struct log_file *lf = to_log_file(f);
    return fread(buf, 1, count, lf->fp);
}

static ssize_t log_write(struct file *f, const void *buf, size_t count) {
    struct log_file *lf = to_log_file(f);
    
    /* Add timestamp prefix */
    fprintf(lf->fp, "[%d] ", ++lf->entry_count);
    size_t written = fwrite(buf, 1, count, lf->fp);
    fprintf(lf->fp, "\n");
    fflush(lf->fp);
    return written;
}

static int log_close(struct file *f) {
    struct log_file *lf = to_log_file(f);
    printf("[log] Closed: %s (entries=%d)\n", f->path, lf->entry_count);
    if (lf->fp) fclose(lf->fp);
    return 0;
}

static const struct file_ops log_file_ops = {
    .name = "logfs",
    .open = log_open,
    .read = log_read,
    .write = log_write,
    .close = log_close,
};

/*
 * ============================================================
 * FACTORY (like mount + open)
 * ============================================================
 */

struct file *file_open(const char *path, int flags, const struct file_ops *ops) {
    /* Allocate based on ops type */
    size_t size;
    if (ops == &mem_file_ops) {
        size = sizeof(struct mem_file);
    } else if (ops == &log_file_ops) {
        size = sizeof(struct log_file);
    } else {
        size = sizeof(struct file);
    }
    
    struct file *f = calloc(1, size);
    if (!f) return NULL;
    
    atomic_store(&f->refcount, 1);
    f->ops = ops;
    strncpy(f->path, path, sizeof(f->path) - 1);
    f->flags = flags;
    f->pos = 0;
    
    if (ops->open && ops->open(f, path, flags) != 0) {
        free(f);
        return NULL;
    }
    
    return f;
}

/*
 * ============================================================
 * DEMONSTRATION
 * ============================================================
 */

int main() {
    printf("=== VFS-Like Polymorphism Demo ===\n\n");
    
    /* Create memory file */
    struct file *mem = file_open("mem://data", 0, &mem_file_ops);
    if (mem) {
        vfs_write(mem, "Hello, Memory!", 14);
        mem->pos = 0;  /* Seek to start */
        
        char buf[32] = {0};
        vfs_read(mem, buf, sizeof(buf));
        printf("Read from memfs: '%s'\n", buf);
        
        file_put(mem);
    }
    
    printf("\n");
    
    /* Create log file */
    struct file *log = file_open("/tmp/demo.log", 0, &log_file_ops);
    if (log) {
        vfs_write(log, "First entry", 11);
        vfs_write(log, "Second entry", 12);
        vfs_write(log, "Third entry", 11);
        
        file_put(log);
    }
    
    printf("\n=== Demo Complete ===\n");
    return 0;
}
```

### A.2 Architecture Diagram for User-Space VFS

```
+------------------------------------------------------------------------+
|                    USER-SPACE VFS ARCHITECTURE                          |
+------------------------------------------------------------------------+

Application Code
         |
         | file_open(path, flags, ops)
         v
+-------------------+
|    Factory        |  <-- Selects implementation based on path/type
+-------------------+
         |
         | Allocates file + private extension
         v
+-------------------+
|  struct file      |  <-- Base object
|  - refcount       |
|  - ops pointer    |---+
|  - path, pos      |   |
+-------------------+   |
         ^              |
         |              v
+-------------------+  +-------------------+
| struct mem_file   |  | struct file_ops   |  <-- Operations table
| [file base]       |  | .open = mem_open  |
| [buffer]          |  | .read = mem_read  |
| [size, capacity]  |  | .write = mem_write|
+-------------------+  | .close = mem_close|
                       +-------------------+

DISPATCH:
    vfs_read(file, buf, n)
         |
         | file->ops->read(file, buf, n)
         v
    mem_read(file, buf, n)
         |
         | to_mem_file(file)->buffer
         v
    Implementation-specific logic
```

### A.3 Key Lessons from VFS

| VFS Pattern | User-Space Application |
|-------------|----------------------|
| **Ops table polymorphism** | Plugin systems, driver frameworks |
| **Embedded private data** | Extending base objects without void* |
| **Reference counting** | Shared resource management |
| **Layered dispatch** | Framework ↔ Implementation separation |
| **Negative caching** | Cache "not found" results |
| **RCU for reads** | Read-mostly concurrent data structures |
| **Uniform interface** | Hide implementation details from callers |

---

## Summary

### VFS Design Principles

1. **Polymorphism via Ops Tables**
   - No C++ required
   - Composable (NULL = use default)
   - One table per type, not per instance

2. **Strict Layering**
   - VFS never knows which FS is active
   - Filesystems never call each other
   - All communication through ops tables

3. **Reference Counting**
   - `file`: Per-open instance
   - `inode`: Per-unique-file, shared across opens
   - `dentry`: Path component cache

4. **Two-Level Caching**
   - Dentry cache: Path → inode mapping
   - Page cache: Inode → data mapping

5. **Ownership and Lifetime**
   - Clear creation/destruction responsibility
   - Invariants enforced by VFS core
   - FS only populates fields

### Reusable in User-Space

```
IF you have multiple implementations of the same interface:
    → Use ops tables (function pointer structs)

IF you have shared resources with unknown lifetime:
    → Use reference counting with atomic operations

IF you have base objects that need extension:
    → Embed base struct as first field; use container_of

IF you have expensive operations with temporal locality:
    → Cache results with LRU eviction

IF you have read-mostly data with rare updates:
    → Consider RCU-like patterns (read without locks)
```

These patterns have proven their value over 30+ years of Linux kernel development and scale from embedded systems to supercomputers.

