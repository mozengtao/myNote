# Module 3: Polymorphism — Function Pointer Tables (vtables)

> **Core question**: When you call `read()` on a file descriptor, the kernel
> calls a different function depending on whether the fd points to a regular
> file, a socket, a pipe, or a device. There's no `virtual` keyword.
> How does dispatch work?

---

## 3.1 The Operations Struct as a vtable

The fundamental polymorphism mechanism in the kernel is the **operations
struct**: a struct of function pointers that serves as a virtual method table.

### The Real Code

`include/linux/fs.h`, lines 1583–1610 — `struct file_operations`:

```c
struct file_operations {
    struct module *owner;
    loff_t (*llseek) (struct file *, loff_t, int);
    ssize_t (*read) (struct file *, char __user *, size_t, loff_t *);
    ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *);
    ssize_t (*aio_read) (struct kiocb *, const struct iovec *,
                         unsigned long, loff_t);
    ssize_t (*aio_write) (struct kiocb *, const struct iovec *,
                          unsigned long, loff_t);
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
    ssize_t (*sendpage) (struct file *, struct page *, int, size_t,
                         loff_t *, int);
    unsigned long (*get_unmapped_area)(struct file *, unsigned long,
                                      unsigned long, unsigned long,
                                      unsigned long);
    int (*check_flags)(int);
    int (*flock) (struct file *, int, struct file_lock *);
    ssize_t (*splice_write)(struct pipe_inode_info *, struct file *,
                            loff_t *, size_t, unsigned int);
    ssize_t (*splice_read)(struct file *, loff_t *,
                           struct pipe_inode_info *, size_t,
                           unsigned int);
    int (*setlease)(struct file *, long, struct file_lock **);
    long (*fallocate)(struct file *file, int mode, loff_t offset,
                      loff_t len);
};
```

Each function pointer is a **virtual method**. The struct itself is the
**vtable**. A concrete "class" (a driver or filesystem) fills in the
function pointers it supports and leaves the rest NULL.

### The C++ Equivalent

```cpp
class FileOperations {
public:
    virtual loff_t  llseek(File *, loff_t, int)           = 0;
    virtual ssize_t read(File *, char *, size_t, loff_t *)  = 0;
    virtual ssize_t write(File *, const char *, size_t, loff_t *) = 0;
    virtual int     open(Inode *, File *)                   = 0;
    virtual int     release(Inode *, File *)                = 0;
    // ...
};
```

The difference: in C++, the compiler generates the vtable and handles
dispatch. In the kernel, the programmer explicitly declares the vtable
struct, fills it in, and calls through it.

---

## 3.2 Static Initialization — The "Class Definition"

Each driver or filesystem defines its "class" by initializing an operations
struct with designated initializers.

### The Real Code: `/dev/null`

`drivers/char/mem.c`, lines 763–768:

```c
static const struct file_operations null_fops = {
    .llseek      = null_lseek,
    .read        = read_null,
    .write       = write_null,
    .splice_write = splice_write_null,
};
```

### The Real Code: `/dev/zero`

`drivers/char/mem.c`, lines 779–784:

```c
static const struct file_operations zero_fops = {
    .llseek = zero_lseek,
    .read   = read_zero,
    .write  = write_zero,
    .mmap   = mmap_zero,
};
```

Key observations:

1. **Designated initializers** (`.field = value`) fill in only the methods
   this "class" implements. Fields not listed are implicitly zero (NULL).

2. **`const` qualifier** prevents runtime modification — like `final` in Java.
   Once the vtable is defined, no one can patch it. This is important for
   security (prevents vtable hijacking).

3. **`static` keyword** — the vtable itself is file-scoped. It is only
   installed on the appropriate `struct file` during `open`.

### What the Implementations Look Like

```c
/* drivers/char/mem.c, lines 616–626 */
static ssize_t
read_null(struct file *file, char __user *buf,
          size_t count, loff_t *ppos)
{
    return 0;          /* /dev/null: reads yield nothing */
}

static ssize_t
write_null(struct file *file, const char __user *buf,
           size_t count, loff_t *ppos)
{
    return count;      /* /dev/null: writes "succeed" silently */
}
```

These are the **concrete method implementations** — the bodies of the
virtual functions.

---

## 3.3 Polymorphic Dispatch — `vfs_read()`

How does the VFS call the right `read` function? Through the vtable pointer.

### The Real Code

`fs/read_write.c`, lines 364–389 — `vfs_read()`:

```c
ssize_t
vfs_read(struct file *file, char __user *buf,
         size_t count, loff_t *pos)
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

The dispatch happens at line 378–381:

```c
if (file->f_op->read)
    ret = file->f_op->read(file, buf, count, pos);
```

This is **polymorphic dispatch**: `file->f_op` points to whichever vtable
was installed during `open`. If the file is `/dev/null`, `f_op` points to
`null_fops`, so `f_op->read` calls `read_null`. If the file is an ext4
regular file, `f_op->read` calls ext4's read implementation.

### Tracing the Full Path

```
User calls:     read(fd, buf, count)
                     │
syscall entry:  sys_read()
                     │
                vfs_read(file, buf, count, &pos)
                     │
                file->f_op->read(file, buf, count, &pos)
                     │
        ┌────────────┼────────────────┐
        │            │                │
   read_null    ext4_file_read    sock_read
  (/dev/null)   (regular file)   (socket)
```

### NULL Function Pointers — "Pure Virtual" in C

Notice `vfs_read` checks `file->f_op->read` for NULL before calling it.
This is important: a NULL function pointer is the kernel's equivalent of
a **pure virtual function** or an **unimplemented interface method**.

If `/dev/zero` had left its `write` pointer as NULL, any write call would
get `-EINVAL` from the check in the VFS layer.

---

## 3.4 Multiple vtables on a Single Object

A single kernel object often has **multiple** vtable pointers, each
representing a different interface.

### `struct inode` — Two Interfaces

`include/linux/fs.h`, lines 749–838:

```c
struct inode {
    /* ... */
    const struct inode_operations   *i_op;   /* line 761 */
    /* ... */
    const struct file_operations    *i_fop;  /* line 814 */
    /* ... */
};
```

- `i_op` → `struct inode_operations` — operations on the inode itself
  (create, link, unlink, lookup, mkdir, etc.)
- `i_fop` → `struct file_operations` — operations on files opened from
  this inode (read, write, mmap, ioctl, etc.)

This is like implementing **multiple interfaces** in Java:

```java
class Ext4Inode implements InodeOperations, FileOperations {
    // ...
}
```

### `struct inode_operations`

`include/linux/fs.h`, lines 1613–1638:

```c
struct inode_operations {
    struct dentry * (*lookup) (struct inode *, struct dentry *,
                               struct nameidata *);
    void * (*follow_link) (struct dentry *, struct nameidata *);
    int (*permission) (struct inode *, int);
    int (*create) (struct inode *, struct dentry *, int,
                   struct nameidata *);
    int (*link) (struct dentry *, struct inode *, struct dentry *);
    int (*unlink) (struct inode *, struct dentry *);
    int (*symlink) (struct inode *, struct dentry *, const char *);
    int (*mkdir) (struct inode *, struct dentry *, int);
    int (*rmdir) (struct inode *, struct dentry *);
    int (*mknod) (struct inode *, struct dentry *, int, dev_t);
    int (*rename) (struct inode *, struct dentry *,
                   struct inode *, struct dentry *);
    void (*truncate) (struct inode *);
    int (*setattr) (struct dentry *, struct iattr *);
    int (*getattr) (struct vfsmount *mnt, struct dentry *,
                    struct kstat *);
    /* ... more ... */
};
```

---

## 3.5 Survey of Key vtable Structs in v3.2

The kernel has dozens of operations structs. Here are the most important:

| Operations Struct           | Attached To             | Purpose                     |
|-----------------------------|-------------------------|-----------------------------|
| `file_operations`           | `struct file`           | read/write/ioctl/mmap       |
| `inode_operations`          | `struct inode`          | create/link/unlink/lookup   |
| `super_operations`          | `struct super_block`    | filesystem-level ops        |
| `address_space_operations`  | `struct address_space`  | page cache I/O              |
| `net_device_ops`            | `struct net_device`     | network interface ops       |
| `block_device_operations`   | `struct gendisk`        | block device ops            |
| `vm_operations_struct`      | `struct vm_area_struct` | memory mapping ops          |
| `sysfs_ops`                 | `struct kobj_type`      | sysfs show/store            |
| `proto_ops`                 | `struct socket`         | socket protocol ops         |
| `seq_operations`            | `struct seq_file`       | iterator for /proc files    |

Each of these follows the same pattern:
1. A struct of function pointers is defined in a header
2. Concrete implementations fill in the struct with designated initializers
3. A generic layer dispatches through the function pointers
4. NULL means "not implemented"

---

## 3.6 The Polymorphism Pattern — Summary

```
┌─────────────────────────────────────────────────────────────┐
│              POLYMORPHISM IN KERNEL C                       │
│                                                             │
│  ┌──────────────┐     ┌──────────────────┐                  │
│  │ struct file  │     │ struct file_     │                  │
│  │   .f_op ─────┼────→│   operations     │  ← vtable        │
│  │   ...        │     │   .read          │                  │
│  └──────────────┘     │   .write         │                  │
│                       │   .open          │                  │
│                       │   .release       │                  │
│                       └──────────────────┘                  │
│                                                             │
│  /dev/null:  f_op → null_fops  { .read = read_null, ... }   │
│  /dev/zero:  f_op → zero_fops  { .read = read_zero, ... }   │
│  ext4 file:  f_op → ext4_fops  { .read = ext4_read, ... }   │
│  socket:     f_op → sock_fops  { .read = sock_read, ... }   │
│                                                             │
│  vfs_read(file, ...) calls file->f_op->read(file, ...)      │
│  → dispatches to the correct implementation automatically   │
└─────────────────────────────────────────────────────────────┘
```

### Advantages Over a Switch Statement

The alternative to vtables would be a giant switch:

```c
/* HYPOTHETICAL — the kernel does NOT do this */
ssize_t
vfs_read(struct file *file, char __user *buf,
         size_t count, loff_t *pos)
{
    switch (file->type) {
    case FILE_TYPE_NULL:   return read_null(file, buf, count, pos);
    case FILE_TYPE_ZERO:   return read_zero(file, buf, count, pos);
    case FILE_TYPE_EXT4:   return ext4_read(file, buf, count, pos);
    /* ... hundreds more cases ... */
    }
}
```

Problems with the switch approach:

1. **Open/Closed Violation**: Adding a new driver requires modifying
   `vfs_read` — a core kernel file that thousands of developers touch.

2. **Scalability**: With 6000+ drivers in the tree, the switch would be
   enormous and unmaintainable.

3. **Modularity**: Loadable kernel modules could not add new cases to
   a compiled-in switch statement.

4. **Performance**: A function pointer call is O(1). A long switch chain
   may degrade (though compilers can optimize to jump tables).

The vtable pattern satisfies the **Open/Closed Principle**: the VFS is
open for extension (new drivers) but closed for modification (the VFS
dispatch code never changes).

---

## Exercise

Find the `struct file_operations` for the `/dev/zero` device in
`drivers/char/mem.c`.

1. Which methods does it implement?
2. Which does it leave as NULL?
3. What happens when userspace calls `lseek()` on `/dev/zero`?

Trace the dispatch path:

```
sys_lseek(fd, offset, whence)
  → vfs_llseek(file, offset, whence)      [fs/read_write.c]
    → file->f_op->llseek(file, offset, whence)
      → ??? (which function gets called?)
```

**Hints:**

- Look at `zero_fops` at line 779 of `drivers/char/mem.c`
- The `.llseek` field points to `zero_lseek` — find that function and
  read what it does
- What does `vfs_llseek()` do if `f_op->llseek` were NULL instead?
  (Check `fs/read_write.c`)

---

## Socratic Check

Before moving to Module 4, answer:

> The kernel has both `struct inode_operations` and `struct file_operations`
> attached to the same inode. Why two separate vtables instead of one big
> one? What does this tell you about interface segregation?
>
> (Hint: think about which operations make sense for a directory vs. a
> regular file vs. a device node.)

Proceed to [Module 4: The kobject Hierarchy](04_kobject_hierarchy.md).
