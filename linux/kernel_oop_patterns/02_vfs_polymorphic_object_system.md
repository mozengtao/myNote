# Phase 2 — Case Study: VFS as a Polymorphic Object System

## Overview

The Virtual File System (VFS) is the kernel's most mature and most
instructive object-oriented subsystem. It defines abstract interfaces
for files, inodes, superblocks, and dentries. Every filesystem —
ext2, NFS, procfs, sysfs, tmpfs — implements these interfaces by
providing concrete function pointer tables.

The VFS never knows which filesystem it is talking to. It only knows
the interface.

---

## The Core Types

### 1. `struct file` — The Open File Object

**Source:** `include/linux/fs.h` lines 964–1009

```c
struct file {
    union {
        struct list_head    fu_list;
        struct rcu_head     fu_rcuhead;
    } f_u;
    struct path             f_path;
    const struct file_operations    *f_op;      /* <-- vtable */

    spinlock_t              f_lock;
    atomic_long_t           f_count;            /* <-- refcount */
    unsigned int            f_flags;
    fmode_t                 f_mode;
    loff_t                  f_pos;
    struct fown_struct      f_owner;
    const struct cred       *f_cred;
    struct file_ra_state    f_ra;
    u64                     f_version;
    void                    *private_data;
    struct address_space    *f_mapping;
};
```

**Key observations:**

- `f_op` is the vtable. It points to a `struct file_operations` provided
  by whichever filesystem or driver opened this file.
- `f_count` is the reference count. When it hits zero, the file is
  released.
- `private_data` is the filesystem's escape hatch — a `void *` for
  attaching arbitrary per-open-file state.

### 2. `struct inode` — The On-Disk Identity Object

**Source:** `include/linux/fs.h` lines 749–838

```c
struct inode {
    umode_t                         i_mode;
    uid_t                           i_uid;
    gid_t                           i_gid;
    unsigned int                    i_flags;

    const struct inode_operations   *i_op;      /* <-- vtable */
    struct super_block              *i_sb;
    struct address_space            *i_mapping;

    unsigned long                   i_ino;
    atomic_t                        i_count;    /* <-- refcount */
    const struct file_operations    *i_fop;     /* default f_op */
    struct address_space            i_data;

    void                            *i_private;
    /* ... */
};
```

**Key observations:**

- `i_op` and `i_fop` are two separate vtables: one for namespace
  operations (create, link, unlink, mkdir), another for file I/O.
- `i_count` is the reference count.
- `i_fop` is copied into `file->f_op` during `open()`. This is how
  the file object inherits its vtable from the inode.

### 3. `struct file_operations` — The File vtable

**Source:** `include/linux/fs.h` lines 1583–1611

```c
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
    unsigned long (*get_unmapped_area)(struct file *, unsigned long,
            unsigned long, unsigned long, unsigned long);
    int (*check_flags)(int);
    int (*flock) (struct file *, int, struct file_lock *);
    ssize_t (*splice_write)(struct pipe_inode_info *, struct file *,
            loff_t *, size_t, unsigned int);
    ssize_t (*splice_read)(struct file *, loff_t *,
            struct pipe_inode_info *, size_t, unsigned int);
    int (*setlease)(struct file *, long, struct file_lock **);
    long (*fallocate)(struct file *file, int mode, loff_t offset, loff_t len);
};
```

This is a classic vtable. Every function pointer is a "virtual method."
A `NULL` pointer means "not implemented" — the VFS checks for `NULL`
before dispatching and may provide a default or return `-EINVAL`.

### 4. `struct inode_operations` — The Namespace vtable

**Source:** `include/linux/fs.h` lines 1613–1641

```c
struct inode_operations {
    struct dentry * (*lookup) (struct inode *, struct dentry *, struct nameidata *);
    void * (*follow_link) (struct dentry *, struct nameidata *);
    int (*permission) (struct inode *, int);
    int (*create) (struct inode *, struct dentry *, int, struct nameidata *);
    int (*link) (struct dentry *, struct inode *, struct dentry *);
    int (*unlink) (struct inode *, struct dentry *);
    int (*symlink) (struct inode *, struct dentry *, const char *);
    int (*mkdir) (struct inode *, struct dentry *, int);
    int (*rmdir) (struct inode *, struct dentry *);
    int (*rename) (struct inode *, struct dentry *, struct inode *, struct dentry *);
    void (*truncate) (struct inode *);
    int (*setattr) (struct dentry *, struct iattr *);
    int (*getattr) (struct vfsmount *mnt, struct dentry *, struct kstat *);
    /* ... */
};
```

### 5. `struct super_operations` — The Filesystem-Level vtable

**Source:** `include/linux/fs.h` lines 1658–1686

```c
struct super_operations {
    struct inode *(*alloc_inode)(struct super_block *sb);
    void (*destroy_inode)(struct inode *);
    void (*dirty_inode) (struct inode *, int flags);
    int (*write_inode) (struct inode *, struct writeback_control *wbc);
    int (*drop_inode) (struct inode *);
    void (*evict_inode) (struct inode *);
    void (*put_super) (struct super_block *);
    int (*sync_fs)(struct super_block *sb, int wait);
    int (*statfs) (struct dentry *, struct kstatfs *);
    int (*remount_fs) (struct super_block *, int *, char *);
    /* ... */
};
```

`alloc_inode` is particularly important — it is the **factory method**.
The VFS calls it to create inodes, but the filesystem provides the
actual implementation that allocates the filesystem-specific inode
(which embeds `struct inode`).

---

## Encapsulation

The VFS achieves encapsulation through a clean separation:

```
    ┌────────────────────────────────────────────┐
    │           VFS Layer (fs/read_write.c)       │
    │                                              │
    │  Only sees:  struct file                     │
    │              struct inode                     │
    │              struct file_operations *         │
    │              struct inode_operations *        │
    ├──────────────────────────────────────────────┤
    │           Filesystem (fs/ext2/*.c)           │
    │                                              │
    │  Provides:  ext2_file_operations             │
    │             ext2_inode_info  (embeds inode)   │
    │             ext2_alloc_inode()                │
    │             EXT2_I() accessor                 │
    └──────────────────────────────────────────────┘
```

The VFS never includes `fs/ext2/ext2.h`. It has no knowledge of
`struct ext2_inode_info`. The boundary is enforced by translation-unit
visibility — ext2-specific structures are private to the ext2 module.

---

## Polymorphism: Tracing a `read()` System Call

Let's trace the full dispatch path from userspace to filesystem:

```
  userspace: read(fd, buf, count)
       │
       ▼
  SYSCALL_DEFINE3(read, ...)          fs/read_write.c:458
       │
       │  file = fget_light(fd, ...)
       │  pos = file_pos_read(file)
       │
       ▼
  vfs_read(file, buf, count, &pos)   fs/read_write.c:364
       │
       │  /* permission + bounds checks */
       │
       │  if (file->f_op->read)
       │      ret = file->f_op->read(file, buf, count, pos);
       │  else
       │      ret = do_sync_read(file, buf, count, pos);
       │
       ▼
  ext2: do_sync_read()  ──►  generic_file_aio_read()
  proc: proc_read()
  sock: sock_aio_read()
  pipe: pipe_read()
```

**This is polymorphism.** The same VFS code path handles all file types.
The dispatch happens through `file->f_op->read` — a single pointer
indirection. No `switch`, no `if/else` chain, no type tag inspection.

### The `vfs_read()` Implementation

**Source:** `fs/read_write.c` lines 364–389

```c
ssize_t
vfs_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
    ssize_t ret;

    if (!(file->f_mode & FMODE_READ))
        return -EBADF;
    if (!file->f_op || (!file->f_op->read && !file->f_op->aio_read))
        return -EINVAL;
    if (unlikely(!access_ok(VERIFY_WRITE, buf, count)))
        return -EFAULT;

    ret = rw_verify_area(READ, file, pos, count);
    if (ret >= 0) {
        count = ret;
        if (file->f_op->read)
            ret = file->f_op->read(file, buf, count, pos);
        else
            ret = do_sync_read(file, buf, count, pos);
        if (ret > 0) {
            fsnotify_access(file);
            add_rchar(current, ret);
        }
        inc_syscr(current);
    }

    return ret;
}
```

Note the fallback pattern: if `->read` is `NULL`, the VFS falls back to
`do_sync_read()`, which wraps `->aio_read`. This is the "default method
implementation" pattern — the abstract interface provides a reasonable
default when the concrete class omits a method.

---

## Inheritance via Struct Embedding

### The Pattern

A filesystem-specific inode "derives from" `struct inode` by embedding it:

```c
/* fs/ext2/ext2.h */
struct ext2_inode_info {
    __le32  i_data[15];
    __u32   i_flags;
    __u32   i_faddr;
    __u8    i_frag_no;
    __u8    i_frag_size;
    __u16   i_state;
    __u32   i_file_acl;
    __u32   i_dir_acl;
    __u32   i_dtime;
    __u32   i_block_group;
    struct ext2_block_alloc_info *i_block_alloc_info;
    __u32   i_dir_start_lookup;
    rwlock_t i_meta_lock;
    struct mutex truncate_mutex;
    struct inode    vfs_inode;          /* <-- base class embedded */
    struct list_head i_orphan;
};
```

**Memory layout:**

```
  struct ext2_inode_info
  ┌───────────────────────────────────────┐
  │  i_data[15]                           │  offset 0
  │  i_flags                              │
  │  i_faddr                              │
  │  i_frag_no, i_frag_size              │
  │  i_state                              │
  │  i_file_acl, i_dir_acl              │
  │  i_dtime                              │
  │  i_block_group                        │
  │  *i_block_alloc_info                  │
  │  i_dir_start_lookup                   │
  │  i_meta_lock                          │
  │  truncate_mutex                       │
  ├───────────────────────────────────────┤
  │  vfs_inode  (struct inode)            │  offset N
  │    ├── i_mode                         │
  │    ├── i_op   ──► inode_operations    │
  │    ├── i_fop  ──► file_operations     │
  │    ├── i_count                        │
  │    └── ...                            │
  ├───────────────────────────────────────┤
  │  i_orphan                             │
  └───────────────────────────────────────┘
```

The VFS only ever sees the `struct inode *` pointer. It never knows that
the inode is actually embedded inside a larger `ext2_inode_info`. This
is information hiding via structural containment.

### Upcasting (Derived → Base)

Trivial — just take the address of the embedded member:

```c
struct ext2_inode_info *ei = /* ... */;
struct inode *inode = &ei->vfs_inode;   /* safe, always valid */
```

### Downcasting (Base → Derived) — `container_of`

This is where `container_of` enters. Given a `struct inode *`, recover
the enclosing `ext2_inode_info *`:

```c
/* fs/ext2/ext2.h line 78 */
static inline struct ext2_inode_info *EXT2_I(struct inode *inode)
{
    return container_of(inode, struct ext2_inode_info, vfs_inode);
}
```

---

## `container_of` — Runtime Type Recovery

**Source:** `include/linux/kernel.h` lines 652–661

```c
/**
 * container_of - cast a member of a structure out to the containing structure
 * @ptr:     the pointer to the member.
 * @type:    the type of the container struct this is embedded in.
 * @member:  the name of the member within the struct.
 */
#define container_of(ptr, type, member) ({                  \
    const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
    (type *)( (char *)__mptr - offsetof(type,member) );})
```

### How It Works

1. `((type *)0)->member` — Compute the type of `member` without
   dereferencing anything. This is a compile-time expression.

2. `const typeof(...) *__mptr = (ptr)` — Type-check the input pointer.
   If `ptr` doesn't match the type of `member`, the compiler emits a
   warning. This is the safety net.

3. `(char *)__mptr - offsetof(type, member)` — Subtract the byte offset
   of `member` within `type` from the pointer. This yields the address
   of the containing structure.

### Visual

```
  Memory:
                          ┌── ptr points here
                          ▼
  ┌──────────┬───────────────────┬──────────┐
  │  field_a │  member (inode)   │  field_b │
  └──────────┴───────────────────┴──────────┘
  ▲
  └── container_of returns here
      (ptr - offsetof(type, member))
```

### Why This Is Safe

- `offsetof` is computed at compile time. No runtime cost.
- The type check via `__mptr` catches mismatched pointer types.
- The result is exact — no rounding, no guessing.

### Why This Is Effectively Downcasting

In C++ terms:

```cpp
// C++ equivalent (conceptual)
ext2_inode_info *ei = static_cast<ext2_inode_info *>(inode);
```

But unlike `static_cast`, `container_of` works at any embedding offset,
not just offset zero. And unlike `dynamic_cast`, there is no runtime
type information — the programmer must know the correct containing type.
Getting it wrong is undefined behavior.

---

## The Factory Method: `alloc_inode`

The VFS calls `super_operations->alloc_inode()` to create inodes. The
filesystem provides the implementation:

**Source:** `fs/ext2/super.c` lines 162–171

```c
static struct inode *
ext2_alloc_inode(struct super_block *sb)
{
    struct ext2_inode_info *ei;
    ei = (struct ext2_inode_info *)kmem_cache_alloc(ext2_inode_cachep,
                                                     GFP_KERNEL);
    if (!ei)
        return NULL;
    ei->i_block_alloc_info = NULL;
    ei->vfs_inode.i_version = 1;
    return &ei->vfs_inode;
}
```

This is a textbook factory method:
1. Allocate the derived type (`ext2_inode_info`) from a slab cache.
2. Initialize filesystem-specific fields.
3. Return a pointer to the **base** type (`struct inode`).

The VFS caller receives a `struct inode *` and never knows about the
ext2 wrapper. The symmetrical destructor is:

```c
static void
ext2_destroy_inode(struct inode *inode)
{
    call_rcu(&inode->i_rcu, ext2_i_callback);
}

static void
ext2_i_callback(struct rcu_head *head)
{
    struct inode *inode = container_of(head, struct inode, i_rcu);
    INIT_LIST_HEAD(&inode->i_dentry);
    kmem_cache_free(ext2_inode_cachep, EXT2_I(inode));
}
```

Note the double `container_of`: first from `rcu_head` to `inode`, then
(via `EXT2_I`) from `inode` to `ext2_inode_info`. The destructor must
free the **containing** object, not the embedded base.

---

## vtable Installation: How an Inode Gets Its Methods

When ext2 reads an inode from disk, it installs the appropriate vtables
based on the inode type:

**Source:** `fs/ext2/inode.c` lines 1366–1405

```c
if (S_ISREG(inode->i_mode)) {
    inode->i_op = &ext2_file_inode_operations;
    inode->i_fop = &ext2_file_operations;
} else if (S_ISDIR(inode->i_mode)) {
    inode->i_op = &ext2_dir_inode_operations;
    inode->i_fop = &ext2_dir_operations;
} else if (S_ISLNK(inode->i_mode)) {
    if (ext2_inode_is_fast_symlink(inode))
        inode->i_op = &ext2_fast_symlink_inode_operations;
    else
        inode->i_op = &ext2_symlink_inode_operations;
}
```

This is the **constructor** — the point where the object's behavior
is bound. After this, every VFS operation dispatches through these
pointers without type checks.

### The ext2 `file_operations` Instance

**Source:** `fs/ext2/file.c` lines 63–79

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
    .splice_read    = generic_file_splice_read,
    .splice_write   = generic_file_splice_write,
};
```

Notice: most methods delegate to generic implementations. ext2 only
overrides what it needs. Unset function pointers remain `NULL` — which
the C standard guarantees for partially-initialized static structs.
This is the C equivalent of inheriting default method implementations
from an abstract base class.

---

## The `super_block` — Filesystem Instance Object

**Source:** `include/linux/fs.h` lines 1400–1459

```c
struct super_block {
    struct list_head        s_list;
    dev_t                   s_dev;
    unsigned long           s_blocksize;
    loff_t                  s_maxbytes;
    struct file_system_type *s_type;
    const struct super_operations   *s_op;   /* <-- vtable */
    unsigned long           s_flags;
    unsigned long           s_magic;
    struct dentry           *s_root;
    void                    *s_fs_info;      /* <-- private data */
    /* ... */
};
```

`s_op` is the superblock vtable. `s_fs_info` is the filesystem's private
data pointer — ext2 stores `struct ext2_sb_info *` here.

---

## Reflection Questions

Before proceeding to Phase 3, consider:

1. **Why does `struct inode` carry both `i_op` and `i_fop`?** What
   would break if they were merged into a single operations struct?

2. **Why are the `file_operations` structs declared `const`?** What
   vulnerability would arise if they were mutable?

3. **In `vfs_read()`, why does the VFS check `file->f_op->read` for
   NULL before calling?** What does this tell you about how the kernel
   handles "abstract methods"?

4. **Why does `ext2_alloc_inode` use `kmem_cache_alloc` instead of
   `kmalloc`?** What performance property does this exploit?

5. **In `ext2_i_callback`, why is `container_of` applied twice?**
   Trace the pointer arithmetic.
