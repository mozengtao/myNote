# WHERE｜源代码地图

## 1. fs/ 目录结构

```
FS/ DIRECTORY STRUCTURE
+=============================================================================+
|                                                                              |
|  fs/                                                                         |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  VFS CORE (VFS 核心)                                                      │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  fs/                                                             │    │ |
|  │  │  ├── inode.c           ◄── Inode allocation, lifecycle           │    │ |
|  │  │  ├── dcache.c          ◄── Dentry cache management               │    │ |
|  │  │  ├── namei.c           ◄── Path resolution (name → inode)        │    │ |
|  │  │  ├── file.c            ◄── struct file management                │    │ |
|  │  │  ├── open.c            ◄── open/close syscalls                   │    │ |
|  │  │  ├── read_write.c      ◄── read/write syscalls (vfs_read/write)  │    │ |
|  │  │  ├── super.c           ◄── Superblock management                 │    │ |
|  │  │  ├── namespace.c       ◄── Mount namespace handling              │    │ |
|  │  │  ├── mount.h           ◄── Mount structures (internal)           │    │ |
|  │  │  ├── fs-writeback.c    ◄── Dirty inode writeback                 │    │ |
|  │  │  ├── buffer.c          ◄── Buffer head management                │    │ |
|  │  │  ├── mpage.c           ◄── Multi-page I/O helpers                │    │ |
|  │  │  ├── file_table.c      ◄── Open file table management            │    │ |
|  │  │  ├── filesystems.c     ◄── Filesystem registration               │    │ |
|  │  │  ├── stat.c            ◄── stat() syscall                        │    │ |
|  │  │  ├── readdir.c         ◄── readdir/getdents syscalls             │    │ |
|  │  │  ├── ioctl.c           ◄── ioctl dispatch                        │    │ |
|  │  │  └── select.c          ◄── select/poll                           │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  DISK FILESYSTEMS (磁盘文件系统)                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  fs/                                                             │    │ |
|  │  │  ├── ext2/             ◄── ext2 filesystem                       │    │ |
|  │  │  ├── ext3/             ◄── ext3 filesystem (journal)             │    │ |
|  │  │  ├── ext4/             ◄── ext4 filesystem (extents, journal)    │    │ |
|  │  │  │   ├── super.c       ◄── Superblock operations                 │    │ |
|  │  │  │   ├── inode.c       ◄── Inode operations                      │    │ |
|  │  │  │   ├── file.c        ◄── File operations                       │    │ |
|  │  │  │   ├── dir.c         ◄── Directory operations                  │    │ |
|  │  │  │   ├── namei.c       ◄── Name operations (create/unlink)       │    │ |
|  │  │  │   ├── extents.c     ◄── Extent tree management                │    │ |
|  │  │  │   └── fsync.c       ◄── fsync implementation                  │    │ |
|  │  │  │                                                               │    │ |
|  │  │  ├── xfs/              ◄── XFS filesystem                        │    │ |
|  │  │  ├── btrfs/            ◄── Btrfs filesystem (COW, snapshots)     │    │ |
|  │  │  ├── fat/              ◄── FAT12/16/32                           │    │ |
|  │  │  ├── ntfs/             ◄── NTFS (read, limited write)            │    │ |
|  │  │  └── isofs/            ◄── ISO 9660 (CD-ROM)                     │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  NETWORK FILESYSTEMS (网络文件系统)                                       │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  fs/                                                             │    │ |
|  │  │  ├── nfs/              ◄── NFS client                            │    │ |
|  │  │  │   ├── super.c       ◄── Superblock/mount                      │    │ |
|  │  │  │   ├── inode.c       ◄── Inode management                      │    │ |
|  │  │  │   ├── file.c        ◄── File read/write (RPC)                 │    │ |
|  │  │  │   ├── dir.c         ◄── Directory operations                  │    │ |
|  │  │  │   ├── write.c       ◄── Write path                            │    │ |
|  │  │  │   ├── read.c        ◄── Read path                             │    │ |
|  │  │  │   └── nfs4proc.c    ◄── NFSv4 operations                      │    │ |
|  │  │  │                                                               │    │ |
|  │  │  ├── nfsd/             ◄── NFS server                            │    │ |
|  │  │  ├── cifs/             ◄── SMB/CIFS client                       │    │ |
|  │  │  └── 9p/               ◄── Plan 9 filesystem                     │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  PSEUDO FILESYSTEMS (伪文件系统)                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  fs/                                                             │    │ |
|  │  │  ├── proc/             ◄── procfs (/proc)                        │    │ |
|  │  │  │   ├── base.c        ◄── /proc/[pid]/* files                   │    │ |
|  │  │  │   ├── generic.c     ◄── Generic proc file support             │    │ |
|  │  │  │   └── inode.c       ◄── Proc inode management                 │    │ |
|  │  │  │                                                               │    │ |
|  │  │  ├── sysfs/            ◄── sysfs (/sys)                          │    │ |
|  │  │  ├── debugfs/          ◄── debugfs (/sys/kernel/debug)           │    │ |
|  │  │  ├── devtmpfs.c        ◄── devtmpfs (/dev)                       │    │ |
|  │  │  ├── ramfs/            ◄── RAM-based filesystem                  │    │ |
|  │  │  ├── tmpfs (in mm/)    ◄── tmpfs (shmem.c)                       │    │ |
|  │  │  └── hugetlbfs/        ◄── Huge pages filesystem                 │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  STACKING/SPECIAL (堆叠/特殊)                                             │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  fs/                                                             │    │ |
|  │  │  ├── fuse/             ◄── FUSE (userspace filesystems)          │    │ |
|  │  │  ├── overlayfs/        ◄── Union/overlay filesystem              │    │ |
|  │  │  ├── ecryptfs/         ◄── Encrypted filesystem layer            │    │ |
|  │  │  ├── autofs4/          ◄── Automounter filesystem                │    │ |
|  │  │  └── pipe.c            ◄── Pipe filesystem (pipefs)              │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**fs/ 目录结构**：

**VFS 核心**：
- `inode.c`：inode 分配、生命周期
- `dcache.c`：dentry 缓存管理
- `namei.c`：路径解析
- `read_write.c`：read/write 系统调用
- `super.c`：超级块管理
- `namespace.c`：挂载命名空间

**磁盘文件系统**：`ext4/`、`xfs/`、`btrfs/`、`fat/`

**网络文件系统**：`nfs/`（客户端）、`nfsd/`（服务器）、`cifs/`

**伪文件系统**：`proc/`、`sysfs/`、`debugfs/`

**堆叠/特殊**：`fuse/`、`overlayfs/`、`ecryptfs/`

---

## 2. 架构锚点：struct inode

```
ARCHITECTURAL ANCHOR: STRUCT INODE
+=============================================================================+
|                                                                              |
|  WHERE TO FIND INODE DEFINITION                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  include/linux/fs.h                                                      │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Line ~500-600 (varies by version):                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  struct inode {                                                  │    │ |
|  │  │      umode_t          i_mode;                                    │    │ |
|  │  │      unsigned short   i_opflags;                                 │    │ |
|  │  │      uid_t            i_uid;                                     │    │ |
|  │  │      gid_t            i_gid;                                     │    │ |
|  │  │      unsigned int     i_flags;                                   │    │ |
|  │  │      ...                                                         │    │ |
|  │  │      const struct inode_operations *i_op;                        │    │ |
|  │  │      const struct file_operations  *i_fop;                       │    │ |
|  │  │      struct super_block *i_sb;                                   │    │ |
|  │  │      struct address_space *i_mapping;                            │    │ |
|  │  │      ...                                                         │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  INODE-RELATED CODE LOCATIONS                                                |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  File                        Content                                     │ |
|  │  ────                        ───────                                     │ |
|  │  include/linux/fs.h          struct inode definition                     │ |
|  │  include/linux/fs.h          struct inode_operations definition          │ |
|  │  fs/inode.c                  inode allocation: iget_locked()             │ |
|  │  fs/inode.c                  inode release: iput()                       │ |
|  │  fs/inode.c                  inode eviction: evict()                     │ |
|  │  fs/inode.c                  inode cache (icache) management             │ |
|  │  fs/dcache.c                 dentry-inode relationship                   │ |
|  │  fs/namei.c                  inode lookup via i_op->lookup()             │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  KEY INODE FUNCTIONS                                                         |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  fs/inode.c:                                                             │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  iget_locked(sb, ino)                                            │    │ |
|  │  │  • Get inode by number, possibly from cache                      │    │ |
|  │  │  • Returns locked inode if new, unlocked if cached               │    │ |
|  │  │                                                                  │    │ |
|  │  │  iget5_locked(sb, hashval, test, set, data)                      │    │ |
|  │  │  • More flexible inode lookup with custom test                   │    │ |
|  │  │                                                                  │    │ |
|  │  │  iput(inode)                                                     │    │ |
|  │  │  • Decrement reference count                                     │    │ |
|  │  │  • May trigger eviction if count reaches 0                       │    │ |
|  │  │                                                                  │    │ |
|  │  │  evict(inode)                                                    │    │ |
|  │  │  • Remove inode from caches                                      │    │ |
|  │  │  • Calls s_op->evict_inode() for filesystem cleanup              │    │ |
|  │  │                                                                  │    │ |
|  │  │  new_inode(sb)                                                   │    │ |
|  │  │  • Allocate fresh inode for new file creation                    │    │ |
|  │  │                                                                  │    │ |
|  │  │  mark_inode_dirty(inode)                                         │    │ |
|  │  │  • Mark inode for writeback                                      │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**struct inode 位置**：`include/linux/fs.h`（约 500-600 行）

**inode 相关代码位置**：
- `include/linux/fs.h`：struct inode、struct inode_operations 定义
- `fs/inode.c`：inode 分配（`iget_locked()`）、释放（`iput()`）、驱逐（`evict()`）、icache 管理
- `fs/dcache.c`：dentry-inode 关系
- `fs/namei.c`：通过 `i_op->lookup()` 查找 inode

**关键 inode 函数**（fs/inode.c）：
- `iget_locked(sb, ino)`：按编号获取 inode
- `iput(inode)`：减少引用计数
- `evict(inode)`：从缓存移除 inode
- `new_inode(sb)`：分配新 inode
- `mark_inode_dirty(inode)`：标记需要写回

---

## 3. 控制中心：vfs_read()

```
CONTROL HUB: VFS_READ()
+=============================================================================+
|                                                                              |
|  LOCATION: fs/read_write.c                                                   |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ssize_t vfs_read(struct file *file, char __user *buf,                   │ |
|  │                   size_t count, loff_t *pos)                             │ |
|  │  {                                                                       │ |
|  │      ssize_t ret;                                                        │ |
|  │                                                                          │ |
|  │      /* Security check */                                                │ |
|  │      if (!(file->f_mode & FMODE_READ))                                   │ |
|  │          return -EBADF;                                                  │ |
|  │      if (!file->f_op || (!file->f_op->read && !file->f_op->aio_read))    │ |
|  │          return -EINVAL;                                                 │ |
|  │                                                                          │ |
|  │      /* Permission and area check */                                     │ |
|  │      ret = rw_verify_area(READ, file, pos, count);                       │ |
|  │      if (ret >= 0) {                                                     │ |
|  │          count = ret;                                                    │ |
|  │                                                                          │ |
|  │          /* DISPATCH TO FILESYSTEM */                                    │ |
|  │          if (file->f_op->read)                                           │ |
|  │              ret = file->f_op->read(file, buf, count, pos);              │ |
|  │          else                                                            │ |
|  │              ret = do_sync_read(file, buf, count, pos);                  │ |
|  │                                                                          │ |
|  │          /* Notification */                                              │ |
|  │          if (ret > 0) {                                                  │ |
|  │              fsnotify_access(file);                                      │ |
|  │              add_rchar(current, ret);                                    │ |
|  │          }                                                               │ |
|  │          inc_syscr(current);                                             │ |
|  │      }                                                                   │ |
|  │                                                                          │ |
|  │      return ret;                                                         │ |
|  │  }                                                                       │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  OTHER CONTROL HUBS IN fs/read_write.c                                       |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Function              Purpose                                           │ |
|  │  ────────              ───────                                           │ |
|  │  vfs_read()            Read dispatch hub                                 │ |
|  │  vfs_write()           Write dispatch hub                                │ |
|  │  vfs_readv()           Vectored read                                     │ |
|  │  vfs_writev()          Vectored write                                    │ |
|  │  vfs_llseek()          Seek dispatch                                     │ |
|  │  do_sync_read()        Wrapper for aio_read                              │ |
|  │  do_sync_write()       Wrapper for aio_write                             │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  OTHER VFS CONTROL HUBS                                                      |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  fs/open.c:                                                              │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │  vfs_open()        Open file dispatch                            │    │ |
|  │  │  do_sys_open()     sys_open() implementation                     │    │ |
|  │  │  vfs_truncate()    Truncate dispatch                             │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  fs/namei.c:                                                             │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │  path_lookupat()   Path resolution main function                 │    │ |
|  │  │  link_path_walk()  Walk each path component                      │    │ |
|  │  │  lookup_slow()     Filesystem lookup call (i_op->lookup)         │    │ |
|  │  │  vfs_create()      Create file dispatch                          │    │ |
|  │  │  vfs_unlink()      Unlink dispatch                               │    │ |
|  │  │  vfs_mkdir()       Mkdir dispatch                                │    │ |
|  │  │  vfs_rmdir()       Rmdir dispatch                                │    │ |
|  │  │  vfs_rename()      Rename dispatch                               │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  fs/dcache.c:                                                            │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │  d_lookup()        Dcache lookup                                 │    │ |
|  │  │  d_alloc()         Allocate new dentry                           │    │ |
|  │  │  dput()            Release dentry reference                      │    │ |
|  │  │  d_instantiate()   Connect dentry to inode                       │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**控制中心：vfs_read()** 位于 `fs/read_write.c`

功能：
1. 安全检查（读模式、操作存在）
2. 权限和区域检查（`rw_verify_area()`）
3. **分发到文件系统**：`file->f_op->read()` 或 `do_sync_read()`
4. 通知（fsnotify、统计）

**其他控制中心**：
- `fs/read_write.c`：`vfs_read()`、`vfs_write()`、`vfs_llseek()`
- `fs/open.c`：`vfs_open()`、`do_sys_open()`
- `fs/namei.c`：`path_lookupat()`、`link_path_walk()`、`vfs_create()`、`vfs_unlink()`
- `fs/dcache.c`：`d_lookup()`、`d_alloc()`、`dput()`

---

## 4. 如何追踪文件操作

```
HOW TO TRACE FILE OPERATIONS
+=============================================================================+
|                                                                              |
|  METHOD 1: FTRACE (Kernel Function Tracer)                                   |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  # Enable tracing                                                        │ |
|  │  echo 1 > /sys/kernel/debug/tracing/events/syscalls/sys_enter_read/enable│ |
|  │  echo 1 > /sys/kernel/debug/tracing/events/syscalls/sys_exit_read/enable │ |
|  │                                                                          │ |
|  │  # Or trace VFS functions                                                │ |
|  │  echo vfs_read > /sys/kernel/debug/tracing/set_ftrace_filter             │ |
|  │  echo function > /sys/kernel/debug/tracing/current_tracer                │ |
|  │                                                                          │ |
|  │  # Run workload                                                          │ |
|  │  cat /etc/passwd > /dev/null                                             │ |
|  │                                                                          │ |
|  │  # Read trace                                                            │ |
|  │  cat /sys/kernel/debug/tracing/trace                                     │ |
|  │                                                                          │ |
|  │  Output:                                                                 │ |
|  │  # tracer: function                                                      │ |
|  │  #   TASK-PID    CPU#  TIMESTAMP  FUNCTION                               │ |
|  │  #      |         |       |          |                                   │ |
|  │       cat-1234   [001]  12345.678: vfs_read <-sys_read                   │ |
|  │       cat-1234   [001]  12345.679: vfs_read <-sys_read                   │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  METHOD 2: FUNCTION GRAPH TRACER                                             |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  # Set up function graph tracer                                          │ |
|  │  echo vfs_read > /sys/kernel/debug/tracing/set_graph_function            │ |
|  │  echo function_graph > /sys/kernel/debug/tracing/current_tracer          │ |
|  │                                                                          │ |
|  │  # Trace                                                                 │ |
|  │  cat /tmp/test.txt > /dev/null                                           │ |
|  │  cat /sys/kernel/debug/tracing/trace                                     │ |
|  │                                                                          │ |
|  │  Output:                                                                 │ |
|  │   0)               |  vfs_read() {                                       │ |
|  │   0)               |    rw_verify_area() {                               │ |
|  │   0)   0.123 us    |      security_file_permission();                    │ |
|  │   0)   0.456 us    |    }                                                │ |
|  │   0)               |    do_sync_read() {                                 │ |
|  │   0)               |      generic_file_aio_read() {                      │ |
|  │   0)               |        do_generic_file_read() {                     │ |
|  │   0)   0.234 us    |          find_get_page();                           │ |
|  │   0)   0.567 us    |          copy_page_to_iter();                       │ |
|  │   0)   1.234 us    |        }                                            │ |
|  │   0)   2.345 us    |      }                                              │ |
|  │   0)   3.456 us    |    }                                                │ |
|  │   0)   5.678 us    |  }                                                  │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  METHOD 3: STRACE (System Call Tracing)                                      |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  $ strace -e trace=open,read,write,close cat /etc/passwd                 │ |
|  │                                                                          │ |
|  │  open("/etc/passwd", O_RDONLY)       = 3                                 │ |
|  │  read(3, "root:x:0:0:root:/root:/bin/bash\n"..., 65536) = 1847           │ |
|  │  write(1, "root:x:0:0:root:/root:/bin/bash\n"..., 1847) = 1847           │ |
|  │  read(3, "", 65536)                  = 0                                 │ |
|  │  close(3)                            = 0                                 │ |
|  │                                                                          │ |
|  │  $ strace -e trace=%file cat /etc/passwd   # All file-related calls     │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  METHOD 4: PERF + TRACEPOINTS                                                |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  # List filesystem tracepoints                                           │ |
|  │  perf list 'ext4:*'                                                      │ |
|  │  perf list 'writeback:*'                                                 │ |
|  │                                                                          │ |
|  │  # Record I/O events                                                     │ |
|  │  perf record -e 'ext4:*' -a -- sleep 10                                  │ |
|  │  perf report                                                             │ |
|  │                                                                          │ |
|  │  # Trace writeback                                                       │ |
|  │  perf record -e 'writeback:*' -a -- dd if=/dev/zero of=/tmp/test bs=1M   │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**追踪文件操作的方法**：

**方法 1：ftrace（内核函数追踪器）**
- 启用系统调用事件或 VFS 函数追踪
- 读取 `/sys/kernel/debug/tracing/trace`

**方法 2：function graph tracer**
- 显示函数调用层次和时间
- 直观展示 `vfs_read()` → `do_sync_read()` → `generic_file_aio_read()` 调用链

**方法 3：strace（系统调用追踪）**
- 用户空间追踪系统调用
- `strace -e trace=open,read,write,close`

**方法 4：perf + tracepoints**
- 列出 ext4、writeback 等追踪点
- `perf record -e 'ext4:*'` 记录 I/O 事件

---

## 5. 阅读顺序

```
RECOMMENDED READING ORDER
+=============================================================================+
|                                                                              |
|  LEVEL 1: VFS BASICS (VFS 基础)                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  1. include/linux/fs.h                                                   │ |
|  │     • struct inode (understand the core object)                          │ |
|  │     • struct file                                                        │ |
|  │     • struct super_block                                                 │ |
|  │     • struct file_operations                                             │ |
|  │     • struct inode_operations                                            │ |
|  │                                                                          │ |
|  │  2. fs/read_write.c                                                      │ |
|  │     • vfs_read() - understand dispatch mechanism                         │ |
|  │     • vfs_write()                                                        │ |
|  │     • Follow the code: sys_read → vfs_read → f_op->read                  │ |
|  │                                                                          │ |
|  │  3. fs/open.c                                                            │ |
|  │     • do_sys_open() - file opening flow                                  │ |
|  │     • vfs_open()                                                         │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  LEVEL 2: PATH RESOLUTION (路径解析)                                         |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  4. fs/namei.c                                                           │ |
|  │     • path_lookupat() - main path resolution                             │ |
|  │     • link_path_walk() - walk each component                             │ |
|  │     • lookup_slow() - call into filesystem                               │ |
|  │     • This is complex; focus on main flow first                          │ |
|  │                                                                          │ |
|  │  5. fs/dcache.c                                                          │ |
|  │     • d_lookup() - dentry cache lookup                                   │ |
|  │     • d_alloc() - allocate dentry                                        │ |
|  │     • __d_lookup() - internal lookup                                     │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  LEVEL 3: INODE AND CACHING (Inode 和缓存)                                   |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  6. fs/inode.c                                                           │ |
|  │     • iget_locked() - get or create inode                                │ |
|  │     • iput() - release inode                                             │ |
|  │     • evict() - evict inode from cache                                   │ |
|  │                                                                          │ |
|  │  7. mm/filemap.c                                                         │ |
|  │     • generic_file_aio_read() - page cache read                          │ |
|  │     • generic_file_buffered_write() - page cache write                   │ |
|  │     • find_get_page() - page cache lookup                                │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  LEVEL 4: SPECIFIC FILESYSTEM (具体文件系统)                                  |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  8. fs/ext4/file.c                                                       │ |
|  │     • ext4_file_operations - study a real implementation                 │ |
|  │     • ext4_file_open()                                                   │ |
|  │     • ext4_file_write()                                                  │ |
|  │                                                                          │ |
|  │  9. fs/ext4/inode.c                                                      │ |
|  │     • ext4_iget() - read inode from disk                                 │ |
|  │     • ext4_readpage() - read file page                                   │ |
|  │                                                                          │ |
|  │  10. fs/ext4/namei.c                                                     │ |
|  │      • ext4_lookup() - directory lookup                                  │ |
|  │      • ext4_create() - create file                                       │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  LEVEL 5: ADVANCED TOPICS (高级主题)                                         |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  11. fs/fs-writeback.c                                                   │ |
|  │      • Dirty inode writeback mechanism                                   │ |
|  │      • writeback_inodes_sb()                                             │ |
|  │                                                                          │ |
|  │  12. fs/super.c                                                          │ |
|  │      • mount/umount handling                                             │ |
|  │      • deactivate_super()                                                │ |
|  │                                                                          │ |
|  │  13. fs/namespace.c                                                      │ |
|  │      • Mount namespace management                                        │ |
|  │      • do_mount()                                                        │ |
|  │                                                                          │ |
|  │  14. fs/proc/base.c (for pseudo-fs understanding)                        │ |
|  │      • /proc/[pid]/* implementation                                      │ |
|  │      • Simpler than disk filesystems                                     │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  READING STRATEGY                                                            |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐   │ |
|  │  │                                                                   │   │ |
|  │  │  1. START WITH DATA STRUCTURES                                    │   │ |
|  │  │     Read fs.h structs before diving into .c files                 │   │ |
|  │  │                                                                   │   │ |
|  │  │  2. TRACE A SIMPLE PATH                                           │   │ |
|  │  │     Follow open("/tmp/test") → read() → close()                   │   │ |
|  │  │                                                                   │   │ |
|  │  │  3. USE FTRACE TO VERIFY                                          │   │ |
|  │  │     Confirm your understanding with actual traces                 │   │ |
|  │  │                                                                   │   │ |
|  │  │  4. COMPARE FILESYSTEMS                                           │   │ |
|  │  │     ext4 (complex) vs ramfs (simple) vs proc (virtual)            │   │ |
|  │  │                                                                   │   │ |
|  │  │  5. FOCUS ON OPS-TABLES                                           │   │ |
|  │  │     Understand which callbacks are called when                    │   │ |
|  │  │                                                                   │   │ |
|  │  └──────────────────────────────────────────────────────────────────┘   │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**推荐阅读顺序**：

**第 1 层：VFS 基础**
1. `include/linux/fs.h`：核心数据结构（inode、file、super_block、操作表）
2. `fs/read_write.c`：理解分发机制（vfs_read、vfs_write）
3. `fs/open.c`：文件打开流程

**第 2 层：路径解析**
4. `fs/namei.c`：path_lookupat()、link_path_walk()
5. `fs/dcache.c`：d_lookup()、d_alloc()

**第 3 层：Inode 和缓存**
6. `fs/inode.c`：iget_locked()、iput()、evict()
7. `mm/filemap.c`：page cache 读写

**第 4 层：具体文件系统**
8-10. `fs/ext4/`：file.c、inode.c、namei.c

**第 5 层：高级主题**
11-14. fs-writeback.c、super.c、namespace.c、proc/

**阅读策略**：
1. 先读数据结构（fs.h）
2. 追踪简单路径：open → read → close
3. 用 ftrace 验证理解
4. 比较不同文件系统：ext4（复杂）vs ramfs（简单）vs proc（虚拟）
5. 关注操作表：理解何时调用哪个回调
