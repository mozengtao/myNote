# VFS Architecture Study: Layering and PDD Mapping

## 1. Mapping PDD to VFS

```
+------------------------------------------------------------------+
|  PDD LAYERS IN VFS                                               |
+------------------------------------------------------------------+

    ┌─────────────────────────────────────────────────────────────┐
    │  PRESENTATION LAYER                                         │
    │  (User-space Interface)                                     │
    │                                                              │
    │  fs/read_write.c:  SYSCALL_DEFINE3(read, ...)              │
    │  fs/open.c:        SYSCALL_DEFINE3(open, ...)              │
    │  fs/stat.c:        SYSCALL_DEFINE2(stat, ...)              │
    │                                                              │
    │  RESPONSIBILITY:                                             │
    │  • Validate user pointers (access_ok)                       │
    │  • Copy data to/from user space                             │
    │  • Convert fd → struct file                                 │
    │  • Translate return values to errno                        │
    └─────────────────────────────────────────────────────────────┘
                              │
                              │ Calls VFS APIs
                              ▼
    ┌─────────────────────────────────────────────────────────────┐
    │  DOMAIN LAYER                                               │
    │  (VFS Core)                                                 │
    │                                                              │
    │  fs/read_write.c:  vfs_read(), vfs_write()                 │
    │  fs/open.c:        do_filp_open(), vfs_open()              │
    │  fs/namei.c:       path_lookup(), link_path_walk()         │
    │  fs/inode.c:       inode_permission(), igrab(), iput()     │
    │  fs/dcache.c:      d_lookup(), dget(), dput()              │
    │                                                              │
    │  RESPONSIBILITY:                                             │
    │  • Permission checking                                      │
    │  • Path resolution                                          │
    │  • Reference counting                                       │
    │  • Cache management (dentry, inode, page)                  │
    │  • Dispatch to filesystem via ops tables                   │
    └─────────────────────────────────────────────────────────────┘
                              │
                              │ Calls f_op/i_op/s_op
                              ▼
    ┌─────────────────────────────────────────────────────────────┐
    │  DATA LAYER                                                 │
    │  (Filesystem Implementations)                               │
    │                                                              │
    │  fs/ext4/file.c:   ext4_file_operations                    │
    │  fs/nfs/file.c:    nfs_file_operations                     │
    │  fs/proc/base.c:   proc_file_operations                    │
    │  mm/shmem.c:       shmem_file_operations                   │
    │                                                              │
    │  RESPONSIBILITY:                                             │
    │  • Actual data storage/retrieval                           │
    │  • On-disk format handling                                 │
    │  • Network protocol (NFS)                                  │
    │  • Virtual content generation (proc)                       │
    └─────────────────────────────────────────────────────────────┘
```

**中文解释：**
- **展示层**：系统调用处理，用户空间数据复制，fd 转换
- **领域层**：VFS 核心，权限检查，路径解析，缓存管理
- **数据层**：具体文件系统实现，实际存储/检索

---

## 2. Detailed Layer Analysis

### 2.1 Presentation Layer (System Call Handlers)

```c
/* fs/read_write.c - PRESENTATION */

SYSCALL_DEFINE3(read, unsigned int, fd, char __user *, buf, size_t, count)
{
    struct file *file;
    ssize_t ret = -EBADF;
    int fput_needed;

    /* PRESENTATION: Convert fd to internal object */
    file = fget_light(fd, &fput_needed);
    if (file) {
        loff_t pos = file_pos_read(file);
        
        /* DELEGATE TO DOMAIN */
        ret = vfs_read(file, buf, count, &pos);
        
        /* PRESENTATION: Update user-visible position */
        file_pos_write(file, pos);
        fput_light(file, fput_needed);
    }
    return ret;  /* PRESENTATION: Return to user space */
}
```

**Presentation Layer Characteristics:**
| Aspect | Implementation |
|--------|----------------|
| Input validation | `access_ok()`, range checks |
| Format conversion | fd → struct file, path → nameidata |
| Error mapping | Internal errors → errno values |
| User-space I/O | `copy_from_user()`, `copy_to_user()` |

### 2.2 Domain Layer (VFS Core)

```c
/* fs/read_write.c - DOMAIN */

ssize_t vfs_read(struct file *file, char __user *buf, 
                 size_t count, loff_t *pos)
{
    ssize_t ret;

    /* DOMAIN POLICY: Check file mode */
    if (!(file->f_mode & FMODE_READ))
        return -EBADF;
    
    /* DOMAIN POLICY: Check if operation exists */
    if (!file->f_op || (!file->f_op->read && !file->f_op->aio_read))
        return -EINVAL;
    
    /* DOMAIN POLICY: User pointer validation */
    if (unlikely(!access_ok(VERIFY_WRITE, buf, count)))
        return -EFAULT;

    /* DOMAIN POLICY: Area verification (locks, limits) */
    ret = rw_verify_area(READ, file, pos, count);
    if (ret >= 0) {
        count = ret;
        
        /* DISPATCH TO DATA LAYER */
        if (file->f_op->read)
            ret = file->f_op->read(file, buf, count, pos);
        else
            ret = do_sync_read(file, buf, count, pos);
        
        /* DOMAIN: Notify file access */
        if (ret > 0) {
            fsnotify_access(file);
            add_rchar(current, ret);
        }
        inc_syscr(current);
    }

    return ret;
}
```

**Domain Layer Characteristics:**
| Aspect | Implementation |
|--------|----------------|
| Policy enforcement | Mode checks, permission checks |
| State management | Reference counting, caching |
| Path resolution | namei, dcache lookups |
| Dispatch | Via ops tables (f_op, i_op, s_op) |

### 2.3 Data Layer (Filesystem Implementations)

```c
/* fs/ext4/file.c - DATA */

static ssize_t ext4_file_write(struct kiocb *iocb, const struct iovec *iov,
                               unsigned long nr_segs, loff_t pos)
{
    struct file *file = iocb->ki_filp;
    struct inode *inode = file->f_mapping->host;
    
    /* DATA: ext4-specific write handling */
    /* Handles journaling, block allocation, etc. */
    
    return generic_file_aio_write(iocb, iov, nr_segs, pos);
}

/* mm/shmem.c - DATA (tmpfs) */

static ssize_t shmem_file_aio_read(struct kiocb *iocb,
                                   const struct iovec *iov,
                                   unsigned long nr_segs, loff_t pos)
{
    /* DATA: tmpfs-specific read handling */
    /* Pages from memory or swap */
    
    return generic_file_aio_read(iocb, iov, nr_segs, pos);
}

/* fs/proc/base.c - DATA (procfs) */

static ssize_t proc_pid_cmdline_read(struct file *file, char __user *buf,
                                     size_t count, loff_t *pos)
{
    /* DATA: Generate content dynamically */
    struct task_struct *task = get_proc_task(file->f_path.dentry->d_inode);
    
    /* Read from task's memory space */
    return access_process_vm(task, arg_start, buffer, len, 0);
}
```

---

## 3. Three Filesystem Case Studies

### 3.1 ext4 — Disk-Based Filesystem

```
+------------------------------------------------------------------+
|  ext4 LAYER MAPPING                                              |
+------------------------------------------------------------------+

    VFS LAYER (Domain):
    ┌─────────────────────────────────────────────────────────────┐
    │  • Path resolution                                          │
    │  • Permission checking (generic_permission or ext4 ACLs)   │
    │  • Page cache management                                    │
    │  • Dentry caching                                           │
    └─────────────────────────────────────────────────────────────┘

    ext4 LAYER (Data):
    ┌─────────────────────────────────────────────────────────────┐
    │  • Block allocation (extents)                              │
    │  • Journaling (jbd2)                                       │
    │  • On-disk inode format                                    │
    │  • Directory format (htree)                                │
    │  • Extent mapping                                          │
    └─────────────────────────────────────────────────────────────┘

    MODULE BOUNDARY:
    ┌─────────────────────────────────────────────────────────────┐
    │  fs/ext4/                                                   │
    │  ├── super.c         # ext4_sops (super_operations)        │
    │  ├── inode.c         # ext4_iops (inode_operations)        │
    │  ├── file.c          # ext4_file_operations                │
    │  ├── dir.c           # ext4_dir_operations                 │
    │  ├── namei.c         # Lookup, create, unlink              │
    │  └── ialloc.c        # Inode allocation                    │
    └─────────────────────────────────────────────────────────────┘

    WHAT STAYS IN VFS:
    • sys_read → vfs_read → f_op->read
    • sys_open → path_lookup → i_op->lookup → do_dentry_open
    • Page cache (address_space) — VFS manages caching
    
    WHAT LIVES IN ext4:
    • ext4_file_operations: How to read/write ext4 files
    • ext4_get_block: Map logical block → physical block
    • ext4_writepage: Write page to disk with journaling
```

### 3.2 tmpfs — Memory-Based Filesystem

```
+------------------------------------------------------------------+
|  tmpfs LAYER MAPPING                                             |
+------------------------------------------------------------------+

    Location: mm/shmem.c (not in fs/)

    VFS LAYER (Domain):
    ┌─────────────────────────────────────────────────────────────┐
    │  • Path resolution (same as always)                        │
    │  • Permission checking (same as always)                    │
    │  • File handle management                                  │
    └─────────────────────────────────────────────────────────────┘

    tmpfs LAYER (Data):
    ┌─────────────────────────────────────────────────────────────┐
    │  • Pages in memory (no disk)                               │
    │  • Swap support (can swap pages to disk)                   │
    │  • No journaling needed                                    │
    │  • Simple inode structure                                  │
    └─────────────────────────────────────────────────────────────┘

    KEY DIFFERENCE FROM ext4:
    ┌─────────────────────────────────────────────────────────────┐
    │  ext4:   file → page cache → block layer → disk            │
    │  tmpfs:  file → page cache → (memory or swap)              │
    │                                                              │
    │  tmpfs uses VFS page cache but stores pages in RAM/swap   │
    │  No block device, no journaling, faster                    │
    └─────────────────────────────────────────────────────────────┘
```

```c
/* mm/shmem.c — tmpfs operations */

static const struct super_operations shmem_ops = {
    .alloc_inode    = shmem_alloc_inode,
    .destroy_inode  = shmem_destroy_inode,
    .statfs         = shmem_statfs,
    .remount_fs     = shmem_remount_fs,
    .show_options   = shmem_show_options,
    .evict_inode    = shmem_evict_inode,
    .drop_inode     = generic_delete_inode,
    /* No write_inode — nothing to persist */
};

static const struct file_operations shmem_file_operations = {
    .mmap           = shmem_mmap,
    .llseek         = generic_file_llseek,
    .read           = do_sync_read,
    .write          = do_sync_write,
    .aio_read       = shmem_file_aio_read,
    .aio_write      = generic_file_aio_write,
    .fsync          = noop_fsync,  /* Nothing to sync — in memory! */
    .splice_read    = shmem_file_splice_read,
    .splice_write   = generic_file_splice_write,
};
```

### 3.3 procfs — Virtual Filesystem

```
+------------------------------------------------------------------+
|  procfs LAYER MAPPING                                            |
+------------------------------------------------------------------+

    Location: fs/proc/

    VFS LAYER (Domain):
    ┌─────────────────────────────────────────────────────────────┐
    │  • Path resolution                                          │
    │  • File handle management                                  │
    │  • Standard permission model                               │
    └─────────────────────────────────────────────────────────────┘

    procfs LAYER (Data):
    ┌─────────────────────────────────────────────────────────────┐
    │  • Content generated on read                               │
    │  • No actual storage                                       │
    │  • Dynamic directory entries (/proc/[pid]/)               │
    │  • Callback-based content generation                       │
    └─────────────────────────────────────────────────────────────┘

    KEY DIFFERENCE:
    ┌─────────────────────────────────────────────────────────────┐
    │  ext4:   file → page cache → disk storage                  │
    │  procfs: file → callback function → kernel data structure  │
    │                                                              │
    │  procfs has NO storage — content is computed on every read │
    └─────────────────────────────────────────────────────────────┘
```

```c
/* fs/proc/base.c — procfs entry */

static const struct file_operations proc_pid_cmdline_operations = {
    .read     = proc_pid_cmdline_read,
    .llseek   = generic_file_llseek,
};

static ssize_t proc_pid_cmdline_read(struct file *file, char __user *buf,
                                     size_t count, loff_t *pos)
{
    struct task_struct *task;
    char *page;
    unsigned long len;
    
    /* Get task from inode */
    task = get_proc_task(file->f_path.dentry->d_inode);
    if (!task)
        return -ESRCH;
    
    /* Generate content dynamically from task's memory */
    len = get_cmdline(task, page, PAGE_SIZE);
    
    /* Copy to user */
    if (copy_to_user(buf, page, count))
        return -EFAULT;
    
    return count;
}
```

---

## 4. Module Boundaries Enforcement

```
+------------------------------------------------------------------+
|  HOW BOUNDARIES ARE ENFORCED                                     |
+------------------------------------------------------------------+

    MECHANISM 1: Ops Tables (Compile-time + Runtime)
    ┌─────────────────────────────────────────────────────────────┐
    │  VFS code only sees:                                        │
    │    struct file_operations *f_op;                           │
    │    struct inode_operations *i_op;                          │
    │                                                              │
    │  VFS code NEVER includes filesystem headers:               │
    │    #include "ext4.h"  ← NEVER in VFS                       │
    │                                                              │
    │  Only filesystem includes VFS headers:                      │
    │    #include <linux/fs.h>  ← ext4.c includes this           │
    └─────────────────────────────────────────────────────────────┘

    MECHANISM 2: Header Organization
    ┌─────────────────────────────────────────────────────────────┐
    │  include/linux/fs.h     # VFS interface definitions        │
    │  fs/internal.h          # VFS internal (not for FS use)    │
    │  fs/ext4/ext4.h         # ext4 internal                    │
    │                                                              │
    │  Filesystem can only use public VFS APIs in fs.h          │
    │  VFS doesn't know ext4.h exists                            │
    └─────────────────────────────────────────────────────────────┘

    MECHANISM 3: Export Symbols
    ┌─────────────────────────────────────────────────────────────┐
    │  EXPORT_SYMBOL(generic_file_llseek);  # For FS use         │
    │  EXPORT_SYMBOL(d_instantiate);        # For FS use         │
    │  /* Internal VFS functions: NOT exported */                │
    └─────────────────────────────────────────────────────────────┘
```

---

## 5. Dependency Direction

```
+------------------------------------------------------------------+
|  DEPENDENCY DIRECTION IN VFS                                     |
+------------------------------------------------------------------+

    CORRECT DEPENDENCIES:
    
         ┌──────────────┐
         │ Application  │  ───► Uses POSIX API
         └──────────────┘
                │
                ▼
         ┌──────────────┐
         │   Syscall    │  ───► Uses VFS API
         │   Handlers   │
         └──────────────┘
                │
                ▼
         ┌──────────────┐
         │   VFS Core   │  ───► Defines ops interfaces
         │              │
         └──────────────┘
                │
                ▼ (via registration)
         ┌──────────────┐
         │ Filesystems  │  ───► Implements ops interfaces
         │ ext4, nfs... │       Uses VFS APIs (generic_*)
         └──────────────┘

    DEPENDENCIES ONLY FLOW DOWNWARD.
    VFS never imports filesystem headers.
    Filesystems import VFS headers.

    REGISTRATION REVERSES CONTROL:
    ┌─────────────────────────────────────────────────────────────┐
    │  1. ext4 compiles with knowledge of VFS interface          │
    │  2. ext4 calls register_filesystem(&ext4_fs_type)          │
    │  3. At runtime, VFS calls ext4 via function pointers       │
    │  4. VFS code never mentions "ext4" by name                 │
    └─────────────────────────────────────────────────────────────┘
```

---

## Summary

```
+------------------------------------------------------------------+
|  PDD MAPPING SUMMARY                                             |
+------------------------------------------------------------------+

    PRESENTATION (System Call Layer):
    • fs/read_write.c syscall handlers
    • User pointer validation
    • fd → file conversion
    • errno mapping

    DOMAIN (VFS Core):
    • vfs_read, vfs_write, vfs_open
    • Permission checking
    • Path resolution (namei.c)
    • Caching (dcache.c, inode.c)
    • Dispatch via ops tables

    DATA (Filesystem Implementations):
    • ext4: Block-based with journaling
    • tmpfs: Memory-based with swap
    • procfs: Callback-based virtual

    BOUNDARY ENFORCEMENT:
    • Ops tables as interfaces
    • Header organization
    • Export symbol control
    • Downward-only dependencies
```

**中文总结：**
- **展示层**：系统调用处理，用户指针验证，fd 转换
- **领域层**：VFS 核心，权限检查，路径解析，缓存管理
- **数据层**：ext4（磁盘+日志）、tmpfs（内存+swap）、procfs（回调生成）
- **边界执行**：ops 表作为接口，头文件组织，符号导出控制

