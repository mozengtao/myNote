# VFS Architecture Study: Manual Polymorphism via Ops Tables

## 1. The Polymorphism Problem in C

```
+------------------------------------------------------------------+
|  HOW TO ACHIEVE POLYMORPHISM WITHOUT CLASSES?                   |
+------------------------------------------------------------------+

    In C++/Java:
    ┌─────────────────────────────────────────────────────────────┐
    │  class File {                                               │
    │      virtual ssize_t read(char* buf, size_t n) = 0;        │
    │  };                                                         │
    │  class Ext4File : public File { ... };                     │
    │  class NfsFile : public File { ... };                      │
    │                                                              │
    │  File* f = open(...);                                       │
    │  f->read(buf, n);  // Virtual dispatch                     │
    └─────────────────────────────────────────────────────────────┘

    In C (Linux kernel):
    ┌─────────────────────────────────────────────────────────────┐
    │  struct file_operations {                                   │
    │      ssize_t (*read)(struct file*, char*, size_t, loff_t*);│
    │  };                                                         │
    │                                                              │
    │  struct file {                                              │
    │      const struct file_operations *f_op;  // "vtable"      │
    │  };                                                         │
    │                                                              │
    │  file->f_op->read(file, buf, n, pos);  // Manual dispatch  │
    └─────────────────────────────────────────────────────────────┘

    SAME IDEA, different syntax:
    • f_op IS the vtable
    • Function pointers ARE virtual methods
    • NULL pointer means "operation not supported"
```

**中文解释：**
- C 语言没有类和虚函数，但可以用函数指针结构体实现相同效果
- `file_operations` 就是 vtable（虚函数表）
- 每个文件系统提供自己的 ops 表实现

---

## 2. The Four Core Ops Tables

```
+------------------------------------------------------------------+
|  VFS OPERATIONS TABLES HIERARCHY                                 |
+------------------------------------------------------------------+

    ┌─────────────────────────────────────────────────────────────┐
    │  struct super_operations                                    │
    │  (Filesystem-wide operations)                               │
    │  - alloc_inode, destroy_inode                              │
    │  - dirty_inode, write_inode                                │
    │  - put_super, sync_fs                                      │
    └───────────────────────────────────┬─────────────────────────┘
                                        │
    ┌─────────────────────────────────────────────────────────────┐
    │  struct inode_operations                                    │
    │  (Per-inode operations)                                     │
    │  - lookup, create, mkdir, unlink                           │
    │  - permission, setattr, getattr                            │
    └───────────────────────────────────┬─────────────────────────┘
                                        │
    ┌─────────────────────────────────────────────────────────────┐
    │  struct file_operations                                     │
    │  (Per-file operations)                                      │
    │  - read, write, llseek                                     │
    │  - open, release, mmap                                     │
    └───────────────────────────────────┬─────────────────────────┘
                                        │
    ┌─────────────────────────────────────────────────────────────┐
    │  struct address_space_operations                            │
    │  (Page cache operations)                                    │
    │  - readpage, writepage                                     │
    │  - write_begin, write_end                                  │
    └─────────────────────────────────────────────────────────────┘
```

### 2.1 file_operations (from `fs.h` lines 1583-1611)

```c
struct file_operations {
    struct module *owner;
    
    /* Position management */
    loff_t (*llseek) (struct file *, loff_t, int);
    
    /* Data transfer */
    ssize_t (*read) (struct file *, char __user *, size_t, loff_t *);
    ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *);
    ssize_t (*aio_read) (struct kiocb *, const struct iovec *, 
                         unsigned long, loff_t);
    ssize_t (*aio_write) (struct kiocb *, const struct iovec *, 
                          unsigned long, loff_t);
    
    /* Directory operations */
    int (*readdir) (struct file *, void *, filldir_t);
    
    /* Multiplexing */
    unsigned int (*poll) (struct file *, struct poll_table_struct *);
    
    /* Device control */
    long (*unlocked_ioctl) (struct file *, unsigned int, unsigned long);
    long (*compat_ioctl) (struct file *, unsigned int, unsigned long);
    
    /* Memory mapping */
    int (*mmap) (struct file *, struct vm_area_struct *);
    
    /* Lifecycle */
    int (*open) (struct inode *, struct file *);
    int (*flush) (struct file *, fl_owner_t id);
    int (*release) (struct inode *, struct file *);
    
    /* Durability */
    int (*fsync) (struct file *, loff_t, loff_t, int datasync);
    int (*aio_fsync) (struct kiocb *, int datasync);
    
    /* Advanced */
    int (*fasync) (int, struct file *, int);
    int (*lock) (struct file *, int, struct file_lock *);
    ssize_t (*sendpage) (struct file *, struct page *, int, size_t, 
                         loff_t *, int);
    ssize_t (*splice_write)(struct pipe_inode_info *, struct file *, 
                            loff_t *, size_t, unsigned int);
    ssize_t (*splice_read)(struct file *, loff_t *, 
                           struct pipe_inode_info *, size_t, unsigned int);
};
```

### 2.2 inode_operations (from `fs.h` lines 1613-1641)

```c
struct inode_operations {
    /* Name resolution */
    struct dentry * (*lookup) (struct inode *, struct dentry *, 
                               struct nameidata *);
    void * (*follow_link) (struct dentry *, struct nameidata *);
    
    /* Permission checking */
    int (*permission) (struct inode *, int);
    struct posix_acl * (*get_acl)(struct inode *, int);
    
    /* Symlink handling */
    int (*readlink) (struct dentry *, char __user *, int);
    void (*put_link) (struct dentry *, struct nameidata *, void *);
    
    /* File creation */
    int (*create) (struct inode *, struct dentry *, int, struct nameidata *);
    int (*link) (struct dentry *, struct inode *, struct dentry *);
    int (*unlink) (struct inode *, struct dentry *);
    int (*symlink) (struct inode *, struct dentry *, const char *);
    int (*mkdir) (struct inode *, struct dentry *, int);
    int (*rmdir) (struct inode *, struct dentry *);
    int (*mknod) (struct inode *, struct dentry *, int, dev_t);
    int (*rename) (struct inode *, struct dentry *, 
                   struct inode *, struct dentry *);
    
    /* Attribute management */
    void (*truncate) (struct inode *);
    int (*setattr) (struct dentry *, struct iattr *);
    int (*getattr) (struct vfsmount *mnt, struct dentry *, struct kstat *);
    
    /* Extended attributes */
    int (*setxattr) (struct dentry *, const char *, const void *, size_t, int);
    ssize_t (*getxattr) (struct dentry *, const char *, void *, size_t);
    ssize_t (*listxattr) (struct dentry *, char *, size_t);
    int (*removexattr) (struct dentry *, const char *);
};
```

### 2.3 super_operations (from `fs.h` lines 1658-1686)

```c
struct super_operations {
    /* Inode allocation */
    struct inode *(*alloc_inode)(struct super_block *sb);
    void (*destroy_inode)(struct inode *);
    
    /* Inode persistence */
    void (*dirty_inode) (struct inode *, int flags);
    int (*write_inode) (struct inode *, struct writeback_control *wbc);
    int (*drop_inode) (struct inode *);
    void (*evict_inode) (struct inode *);
    
    /* Superblock lifecycle */
    void (*put_super) (struct super_block *);
    void (*write_super) (struct super_block *);
    int (*sync_fs)(struct super_block *sb, int wait);
    int (*freeze_fs) (struct super_block *);
    int (*unfreeze_fs) (struct super_block *);
    
    /* Statistics */
    int (*statfs) (struct dentry *, struct kstatfs *);
    int (*remount_fs) (struct super_block *, int *, char *);
    void (*umount_begin) (struct super_block *);
    
    /* Display */
    int (*show_options)(struct seq_file *, struct vfsmount *);
    int (*show_devname)(struct seq_file *, struct vfsmount *);
    
    /* Quota (if enabled) */
    ssize_t (*quota_read)(struct super_block *, int, char *, size_t, loff_t);
    ssize_t (*quota_write)(struct super_block *, int, const char *, 
                           size_t, loff_t);
    
    /* Cache management */
    int (*nr_cached_objects)(struct super_block *);
    void (*free_cached_objects)(struct super_block *, int);
};
```

---

## 3. Complete Call Path Traces

### 3.1 sys_read → vfs_read → f_op->read

```
+------------------------------------------------------------------+
|  CALL PATH: read() SYSTEM CALL                                   |
+------------------------------------------------------------------+

    User space:
    ┌─────────────────────────────────────────────────────────────┐
    │  ssize_t n = read(fd, buf, 1024);                          │
    └─────────────────────────────────────────────────────────────┘
                              │
                              │ System call
                              ▼
    ┌─────────────────────────────────────────────────────────────┐
    │  SYSCALL_DEFINE3(read, unsigned int, fd,                   │
    │                  char __user *, buf, size_t, count)         │
    │  {                                                          │
    │      struct file *file;                                     │
    │      file = fget_light(fd, &fput_needed);  // fd → file    │
    │      if (file) {                                            │
    │          ret = vfs_read(file, buf, count, &pos);           │
    │          fput_light(file, fput_needed);                     │
    │      }                                                       │
    │      return ret;                                            │
    │  }                                                          │
    └─────────────────────────────────────────────────────────────┘
                              │
                              ▼
    ┌─────────────────────────────────────────────────────────────┐
    │  ssize_t vfs_read(struct file *file, char __user *buf,     │
    │                   size_t count, loff_t *pos)                │
    │  {                                                          │
    │      /* VFS policy checks */                               │
    │      if (!(file->f_mode & FMODE_READ))                     │
    │          return -EBADF;                                     │
    │      if (!file->f_op || (!file->f_op->read && ...))        │
    │          return -EINVAL;                                    │
    │                                                              │
    │      ret = rw_verify_area(READ, file, pos, count);         │
    │      if (ret >= 0) {                                        │
    │          /* DISPATCH TO FILESYSTEM */                      │
    │          if (file->f_op->read)                             │
    │              ret = file->f_op->read(file, buf, count, pos);│
    │          else                                               │
    │              ret = do_sync_read(file, buf, count, pos);    │
    │      }                                                       │
    │      return ret;                                            │
    │  }                                                          │
    └─────────────────────────────────────────────────────────────┘
                              │
                              │ file->f_op->read(...)
                              ▼
    ┌─────────────────────────────────────────────────────────────┐
    │  Filesystem-specific implementation:                        │
    │                                                              │
    │  • ext4: ext4_file_read() or generic_file_aio_read()       │
    │  • proc: proc_file_read()                                  │
    │  • device: driver-specific read                            │
    └─────────────────────────────────────────────────────────────┘
```

**Key Points:**
- VFS (`vfs_read`) handles common policy: mode checking, area verification
- VFS dispatches via `file->f_op->read` — no switch on filesystem type
- Each filesystem sets `f_op` when file is opened

### 3.2 sys_open → do_filp_open → lookup

```
+------------------------------------------------------------------+
|  CALL PATH: open() SYSTEM CALL                                   |
+------------------------------------------------------------------+

    User: open("/home/user/file.txt", O_RDONLY)
                              │
                              ▼
    ┌─────────────────────────────────────────────────────────────┐
    │  do_sys_open(dfd, filename, flags, mode)                   │
    │  └── do_filp_open(dfd, pathname, op, lookup_flags)         │
    │      └── path_openat(dfd, pathname, nd, op, flags)         │
    │          └── link_path_walk(name, nd)                      │
    │              └── walk_component(nd, ...)                   │
    │                  └── do_lookup(nd, name, path, ...)        │
    │                      └── inode->i_op->lookup(...)          │
    │                          ↓                                  │
    │                  [FILESYSTEM LOOKUP]                       │
    └─────────────────────────────────────────────────────────────┘
                              │
                              ▼
    Path resolved, now open:
    ┌─────────────────────────────────────────────────────────────┐
    │  do_dentry_open(file, dentry->d_inode, NULL)               │
    │  {                                                          │
    │      f->f_inode = inode;                                    │
    │      f->f_mapping = inode->i_mapping;                       │
    │      f->f_op = fops_get(inode->i_fop);  // Get ops table   │
    │                                                              │
    │      if (f->f_op->open)                                     │
    │          error = f->f_op->open(inode, f);  // FS-specific  │
    │  }                                                          │
    └─────────────────────────────────────────────────────────────┘
```

**Key Points:**
- Path resolution uses `inode->i_op->lookup` at each component
- When file found, `f_op` is copied from `inode->i_fop`
- Filesystem's `open()` can customize the file handle

### 3.3 Permission Check Flow

```
+------------------------------------------------------------------+
|  CALL PATH: PERMISSION CHECKING                                  |
+------------------------------------------------------------------+

    Any file access triggers permission check:
                              │
                              ▼
    ┌─────────────────────────────────────────────────────────────┐
    │  inode_permission(inode, mask)                              │
    │  {                                                          │
    │      /* Check for special flags */                         │
    │      if (unlikely(mask & MAY_WRITE)) {                     │
    │          if (IS_IMMUTABLE(inode)) return -EACCES;          │
    │      }                                                       │
    │                                                              │
    │      /* Security module check */                           │
    │      retval = security_inode_permission(inode, mask);      │
    │      if (retval) return retval;                            │
    │                                                              │
    │      /* Filesystem-specific permission check */            │
    │      if (inode->i_op->permission)                          │
    │          return inode->i_op->permission(inode, mask);      │
    │                                                              │
    │      /* Default: generic_permission */                     │
    │      return generic_permission(inode, mask);               │
    │  }                                                          │
    └─────────────────────────────────────────────────────────────┘
```

---

## 4. Inversion of Control

```
+------------------------------------------------------------------+
|  WHO CONTROLS BEHAVIOR?                                          |
+------------------------------------------------------------------+

    WITHOUT INVERSION (Procedural):
    ┌─────────────────────────────────────────────────────────────┐
    │  int do_read(file *f, char *buf, size_t n) {               │
    │      if (f->type == EXT4)                                  │
    │          return ext4_read(f, buf, n);                      │
    │      else if (f->type == NFS)                              │
    │          return nfs_read(f, buf, n);                       │
    │      else if (f->type == PROC)                             │
    │          return proc_read(f, buf, n);                      │
    │      // ... 60+ more cases ...                             │
    │  }                                                          │
    │                                                              │
    │  VFS KNOWS about all filesystems = TIGHT COUPLING          │
    └─────────────────────────────────────────────────────────────┘

    WITH INVERSION (VFS Pattern):
    ┌─────────────────────────────────────────────────────────────┐
    │  ssize_t vfs_read(struct file *file, ...) {                │
    │      // VFS does policy, then delegates                    │
    │      return file->f_op->read(file, ...);                   │
    │  }                                                          │
    │                                                              │
    │  // ext4 module:                                            │
    │  const struct file_operations ext4_file_ops = {            │
    │      .read = ext4_file_read,                               │
    │  };                                                         │
    │                                                              │
    │  VFS KNOWS NOTHING about ext4, NFS, etc.                   │
    │  Filesystems plug themselves in via registration.          │
    └─────────────────────────────────────────────────────────────┘

    KEY INSIGHT:
    ┌─────────────────────────────────────────────────────────────┐
    │  VFS defines WHAT to call (interface)                      │
    │  Filesystem defines HOW to do it (implementation)          │
    │  Control is INVERTED: VFS doesn't decide behavior          │
    └─────────────────────────────────────────────────────────────┘
```

---

## 5. Why VFS Never Switches on Filesystem Type

```
+------------------------------------------------------------------+
|  NO TYPE SWITCHES IN VFS                                         |
+------------------------------------------------------------------+

    YOU WILL NEVER SEE THIS IN VFS:
    
    ✗ if (sb->s_magic == EXT4_MAGIC) { ... }
    ✗ switch (inode->i_fs_type) { case FS_NFS: ... }
    ✗ if (strcmp(fs->name, "proc") == 0) { ... }

    INSTEAD, VFS ALWAYS DOES:
    
    ✓ inode->i_op->lookup(...)
    ✓ file->f_op->read(...)
    ✓ sb->s_op->alloc_inode(...)

    WHY THIS MATTERS:
    ┌─────────────────────────────────────────────────────────────┐
    │  1. EXTENSIBILITY                                           │
    │     New filesystem = new module, zero VFS changes          │
    │                                                              │
    │  2. MAINTAINABILITY                                         │
    │     VFS code stays constant, FS code evolves independently │
    │                                                              │
    │  3. TESTABILITY                                             │
    │     Mock filesystem implements ops for testing             │
    │                                                              │
    │  4. COMPILE-TIME INDEPENDENCE                               │
    │     VFS compiles without any FS headers                    │
    └─────────────────────────────────────────────────────────────┘
```

---

## 6. Example: How Different Filesystems Implement read()

```c
/* REGULAR FILES (ext4, most disk filesystems) */
/* Use generic infrastructure: page cache + address_space_operations */

const struct file_operations ext4_file_operations = {
    .llseek     = generic_file_llseek,
    .read       = do_sync_read,        /* Uses aio_read */
    .write      = do_sync_write,       /* Uses aio_write */
    .aio_read   = generic_file_aio_read,  /* Page cache */
    .aio_write  = ext4_file_write,
    .mmap       = generic_file_mmap,
    .open       = ext4_file_open,
    .release    = ext4_release_file,
    .fsync      = ext4_sync_file,
    /* ... */
};


/* PROCFS (virtual filesystem) */
/* Each proc entry may have custom read function */

const struct file_operations proc_file_operations = {
    .llseek     = proc_file_lseek,
    .read       = proc_file_read,      /* Custom: reads from callback */
    .write      = proc_file_write,
    /* No mmap - virtual content */
};

static ssize_t proc_file_read(struct file *file, char __user *buf,
                              size_t count, loff_t *ppos)
{
    /* Get proc_dir_entry from file */
    struct proc_dir_entry *pde = PDE(file->f_path.dentry->d_inode);
    
    /* Call registered read function */
    if (pde->read_proc)
        return pde->read_proc(page, start, *ppos, count, eof, pde->data);
    
    return -EIO;
}


/* DEVICE FILES */
/* Forward to device driver's file_operations */

/* In drivers/char/mem.c for /dev/null: */
static ssize_t null_read(struct file *file, char __user *buf,
                         size_t count, loff_t *ppos)
{
    return 0;  /* Always EOF */
}

static ssize_t null_write(struct file *file, const char __user *buf,
                          size_t count, loff_t *ppos)
{
    return count;  /* Discard everything */
}

const struct file_operations null_fops = {
    .llseek     = null_lseek,
    .read       = null_read,
    .write      = null_write,
};
```

---

## 7. Pattern: Optional Operations

```
+------------------------------------------------------------------+
|  HANDLING OPTIONAL OPERATIONS                                    |
+------------------------------------------------------------------+

    Some operations are optional (NULL pointer = not supported):
    
    ┌─────────────────────────────────────────────────────────────┐
    │  ssize_t vfs_read(struct file *file, ...)                  │
    │  {                                                          │
    │      if (!file->f_op || (!file->f_op->read &&              │
    │                          !file->f_op->aio_read))           │
    │          return -EINVAL;  /* Operation not supported */    │
    │                                                              │
    │      if (file->f_op->read)                                 │
    │          ret = file->f_op->read(file, buf, count, pos);    │
    │      else                                                   │
    │          ret = do_sync_read(file, buf, count, pos);        │
    │                                                              │
    │      return ret;                                            │
    │  }                                                          │
    └─────────────────────────────────────────────────────────────┘

    CONVENTION:
    • NULL = operation not supported → return -EINVAL or -ENOSYS
    • Function present = operation supported
    • Some have defaults (e.g., generic_file_llseek)
```

---

## Summary

```
+------------------------------------------------------------------+
|  VFS POLYMORPHISM SUMMARY                                        |
+------------------------------------------------------------------+

    MECHANISM:
    • Ops tables = vtables in C
    • Function pointers = virtual methods
    • NULL = operation not supported

    KEY OPS TABLES:
    • file_operations: Per-open-file operations (read, write, ...)
    • inode_operations: Per-inode operations (lookup, create, ...)
    • super_operations: Per-filesystem operations (sync, alloc_inode)
    • address_space_operations: Page cache operations

    INVERSION OF CONTROL:
    • VFS defines interfaces
    • Filesystems implement interfaces
    • VFS never knows about specific filesystems
    • No type switches in VFS code

    CALL PATH PATTERN:
    1. User syscall (read)
    2. VFS policy (vfs_read: check mode, verify area)
    3. Dispatch (file->f_op->read)
    4. Filesystem implementation (ext4_file_read)
```

**中文总结：**
- **ops 表就是 C 语言的 vtable**，函数指针就是虚方法
- **四个核心 ops 表**：file_operations、inode_operations、super_operations、address_space_operations
- **控制反转**：VFS 定义接口，文件系统实现接口；VFS 对具体文件系统一无所知
- **调用路径**：系统调用 → VFS 策略检查 → 通过 ops 表分发 → 文件系统实现

