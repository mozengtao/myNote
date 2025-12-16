# Linux Kernel VFS Deep Dive (v3.2)
## A Code-Level Walkthrough of the Virtual File System

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

The **Virtual File System (VFS)** is the abstraction layer that provides a uniform interface for all file operations in Linux. It allows applications to use the same system calls (`open()`, `read()`, `write()`, `close()`) regardless of the underlying filesystem (ext4, XFS, NFS, procfs, etc.).

### What Problem Does It Solve?

1. **Filesystem Abstraction**: Uniform API across 50+ different filesystems
2. **Path Resolution**: Translates paths like `/home/user/file.txt` to inodes
3. **Caching**: dcache (directory entry cache) and inode cache for performance
4. **Mount Management**: Unifies multiple filesystems into a single namespace
5. **File Operations**: Common infrastructure for read/write/seek/mmap
6. **Permission Checking**: Centralized security and access control

### Where It Sits in the Overall Kernel Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              USER SPACE                                      │
│                                                                              │
│  open()  read()  write()  close()  stat()  mkdir()  mount()  ...            │
│     │      │       │        │        │       │        │                      │
└─────┼──────┼───────┼────────┼────────┼───────┼────────┼──────────────────────┘
      │      │       │        │        │       │        │
      └──────┴───────┴────────┴────────┴───────┴────────┘
                              │ System Call Interface
                              ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                              VFS LAYER                                       │
│                                                                              │
│  ┌─────────────────────────────────────────────────────────────────────────┐│
│  │                         CORE VFS OBJECTS                                ││
│  │                                                                         ││
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  ┌─────────────┐ ││
│  │  │ struct file  │  │struct inode  │  │struct dentry │  │struct super │ ││
│  │  │   (open     │  │  (on-disk   │  │  (path name  │  │   _block    │ ││
│  │  │   file)     │  │   object)   │  │   cache)     │  │ (filesystem)│ ││
│  │  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘  └──────┬──────┘ ││
│  │         │                 │                 │                 │         ││
│  │         └─────────────────┴─────────────────┴─────────────────┘         ││
│  │                                    │                                     ││
│  │                                    ▼                                     ││
│  │  ┌───────────────────────────────────────────────────────────────────┐  ││
│  │  │                      OPERATIONS TABLES                             │  ││
│  │  │  file_operations  inode_operations  dentry_operations  super_ops  │  ││
│  │  └───────────────────────────────────────────────────────────────────┘  ││
│  └─────────────────────────────────────────────────────────────────────────┘│
│                                    │                                         │
│  ┌─────────────────────────────────┼─────────────────────────────────┐      │
│  │         CACHES                  │         PATH RESOLUTION         │      │
│  │  ┌──────────────────────┐      │      ┌──────────────────────┐   │      │
│  │  │  dcache (dentry)     │      │      │  namei.c             │   │      │
│  │  │  inode cache         │◄─────┼─────►│  path_lookup()       │   │      │
│  │  │  buffer cache        │      │      │  link_path_walk()    │   │      │
│  │  │  page cache          │      │      └──────────────────────┘   │      │
│  │  └──────────────────────┘      │                                  │      │
│  └─────────────────────────────────┴─────────────────────────────────┘      │
│                                    │                                         │
└────────────────────────────────────┼─────────────────────────────────────────┘
                                     │
         ┌───────────────────────────┼───────────────────────────┐
         │                           │                           │
         ▼                           ▼                           ▼
┌─────────────────┐       ┌─────────────────┐       ┌─────────────────┐
│     ext4        │       │      XFS        │       │      NFS        │
│  (disk-based)   │       │  (disk-based)   │       │   (network)     │
├─────────────────┤       ├─────────────────┤       ├─────────────────┤
│ ext4_file_ops   │       │ xfs_file_ops    │       │ nfs_file_ops    │
│ ext4_inode_ops  │       │ xfs_inode_ops   │       │ nfs_inode_ops   │
│ ext4_super_ops  │       │ xfs_super_ops   │       │ nfs_super_ops   │
└────────┬────────┘       └────────┬────────┘       └────────┬────────┘
         │                         │                         │
         ▼                         ▼                         ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                        BLOCK I/O / NETWORK                                   │
│              (bio, block device layer, network stack)                        │
└─────────────────────────────────────────────────────────────────────────────┘
```

**图解说明：**
- VFS是用户空间系统调用和底层文件系统之间的抽象层
- 四个核心对象：file（打开的文件）、inode（磁盘对象）、dentry（路径缓存）、super_block（文件系统实例）
- 每个文件系统提供自己的操作函数表（file_operations、inode_operations等）
- dcache和inode cache是关键性能组件，避免重复的磁盘访问
- 路径解析（namei.c）将路径名转换为inode

### How This Subsystem Interacts with Others

| Subsystem | Interaction |
|-----------|-------------|
| **Memory (mm)** | Page cache for file data, mmap() implementation |
| **Block Layer** | Read/write requests to block devices |
| **Network Stack** | NFS, CIFS network filesystems |
| **Process Management** | files_struct, fs_struct in task_struct |
| **Security** | LSM hooks, permission checks |
| **Namespace** | Mount namespaces for container isolation |

---

## 2. Directory & File Map

```
fs/
│
├── namei.c              → Path name resolution (the heart of VFS)
│                          path_lookup(), link_path_walk()
│                          ~3400 lines - translates paths to inodes
│
├── open.c               → File opening: do_sys_open(), filp_open()
│                          ~1150 lines
│
├── read_write.c         → Read/write operations: vfs_read(), vfs_write()
│                          Generic file operations
│                          ~1000 lines
│
├── file.c               → File descriptor operations
│                          fget(), fput(), file allocation
│
├── file_table.c         → File table management
│                          SLAB cache for struct file
│
├── dcache.c             → Dentry cache implementation
│                          Lookup, hash table, LRU
│                          ~3000 lines - critical for performance
│
├── inode.c              → Inode management
│                          Allocation, hash table, lifecycle
│                          ~1700 lines
│
├── super.c              → Superblock operations
│                          Mount/umount, filesystem registration
│                          ~1200 lines
│
├── namespace.c          → Mount namespace management
│                          do_mount(), pivot_root()
│
├── stat.c               → File status: vfs_stat(), vfs_lstat()
│
├── attr.c               → Attribute changes: chmod, chown
│
├── xattr.c              → Extended attributes
│
├── buffer.c             → Buffer cache (legacy)
│
├── libfs.c              → Common filesystem helpers
│                          simple_lookup(), generic_read_dir()
│
├── filesystems.c        → Filesystem registration
│                          register_filesystem()
│
└── exec.c               → Program execution (binary loading)
    search_binary_handler(), do_execve()

include/linux/
│
├── fs.h                 → Core VFS structures:
│                          struct file, struct inode
│                          struct super_block
│                          file_operations, inode_operations
│                          super_operations
│
├── dcache.h             → struct dentry, dentry_operations
│
├── mount.h              → struct vfsmount
│
├── namei.h              → Path lookup flags, struct nameidata
│
└── fs_struct.h          → struct fs_struct (process cwd/root)

fs/<filesystem>/         → Individual filesystem implementations
│
├── ext4/                → ext4 filesystem
│   ├── super.c          → ext4_fill_super(), mount
│   ├── inode.c          → ext4_iget(), inode operations
│   ├── file.c           → ext4_file_operations
│   └── namei.c          → ext4_lookup()
│
├── proc/                → /proc pseudo-filesystem
├── sysfs/               → /sys pseudo-filesystem
├── ramfs/               → RAM-based filesystem (tmpfs base)
└── nfs/                 → Network filesystem
```

### Why Is the Code Split This Way?

1. **namei.c** - Path resolution is complex enough to warrant its own file. It handles symlinks, mount points, permissions, and RCU-based lockless lookup.

2. **dcache.c** - Dentry cache is critical for performance. Keeps it separate allows focused optimization.

3. **Separation of Concerns**:
   - `open.c` - File opening (creating struct file)
   - `read_write.c` - Data transfer
   - `stat.c` - Metadata retrieval
   
4. **Per-Filesystem Directories** - Each filesystem implements its own operations, keeping core VFS generic.

---

## 3. Core Data Structures

### 3.1 The Four VFS Objects

The VFS revolves around four interconnected objects:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         VFS OBJECT RELATIONSHIPS                            │
└─────────────────────────────────────────────────────────────────────────────┘

     struct file                struct dentry              struct inode
  (open file instance)       (directory entry)          (file metadata)
 ┌──────────────────┐      ┌──────────────────┐      ┌──────────────────┐
 │ f_path.dentry ───┼──────►│ d_inode ─────────┼──────►│ i_ino = 12345   │
 │ f_path.mnt       │      │ d_parent         │      │ i_mode          │
 │ f_op ────────────┼─┐    │ d_name = "foo"   │      │ i_size          │
 │ f_pos            │ │    │ d_sb ────────────┼──┐   │ i_op            │
 │ f_count          │ │    │ d_op             │  │   │ i_fop           │
 │ f_mode           │ │    │ d_subdirs        │  │   │ i_sb ───────────┼──┐
 └──────────────────┘ │    │ d_child          │  │   │ i_mapping       │  │
                      │    │ d_hash           │  │   │ i_dentry ◄──────┼──┤
                      │    └──────────────────┘  │   └──────────────────┘  │
                      │                          │                         │
                      │                          │                         │
                      │                          ▼                         │
                      │    ┌──────────────────────────────────────────┐    │
                      │    │           struct super_block             │    │
                      │    │        (mounted filesystem instance)     │◄───┘
                      │    │                                          │
                      │    │  s_root (root dentry)                    │
                      │    │  s_op (super_operations)                 │
                      │    │  s_type (file_system_type)               │
                      │    │  s_inodes (list of all inodes)           │
                      │    │  s_bdev (block device)                   │
                      │    └──────────────────────────────────────────┘
                      │
                      ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                        struct file_operations                               │
│  (function pointers - the "vtable" for file types)                          │
│                                                                             │
│  .llseek   = ext4_llseek                                                    │
│  .read     = do_sync_read                                                   │
│  .write    = do_sync_write                                                  │
│  .open     = ext4_file_open                                                 │
│  .release  = ext4_release_file                                              │
│  .mmap     = ext4_file_mmap                                                 │
│  .fsync    = ext4_sync_file                                                 │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 3.2 struct file (Open File Description)

```c
// include/linux/fs.h:964-1010

struct file {
    union {
        struct list_head    fu_list;     // 链接到super_block->s_files
        struct rcu_head     fu_rcuhead;  // RCU延迟释放
    } f_u;
    
    struct path     f_path;              // 包含dentry和vfsmount
#define f_dentry    f_path.dentry        // 兼容性别名
#define f_vfsmnt    f_path.mnt
    
    const struct file_operations *f_op;  // 操作函数表（核心！）
    
    spinlock_t      f_lock;              // 保护f_flags, f_pos
    atomic_long_t   f_count;             // 引用计数
    unsigned int    f_flags;             // O_RDONLY, O_NONBLOCK等
    fmode_t         f_mode;              // FMODE_READ, FMODE_WRITE
    loff_t          f_pos;               // 当前文件偏移
    
    struct fown_struct  f_owner;         // 异步I/O所有者
    const struct cred   *f_cred;         // 打开时的凭证
    struct file_ra_state f_ra;           // 预读状态
    
    u64             f_version;           // 用于目录遍历
    void            *private_data;       // 文件系统私有数据
    // ...
};
```

**关键点：**
- 每次`open()`创建一个新的`struct file`
- `fork()`后父子进程共享同一个`struct file`（引用计数增加）
- `dup()`也共享同一个`struct file`
- 多个file可以指向同一个dentry/inode

### 3.3 struct inode (Index Node)

```c
// include/linux/fs.h:749-838

struct inode {
    /*=== 常访问字段（RCU路径查找） ===*/
    umode_t         i_mode;              // 文件类型和权限
    unsigned short  i_opflags;
    uid_t           i_uid;               // 所有者UID
    gid_t           i_gid;               // 所有组GID
    unsigned int    i_flags;             // 文件系统标志
    
    const struct inode_operations *i_op; // inode操作表
    struct super_block *i_sb;            // 所属超级块
    struct address_space *i_mapping;     // 页缓存映射
    
    /*=== 元数据 ===*/
    unsigned long   i_ino;               // inode号
    unsigned int    i_nlink;             // 硬链接数
    dev_t           i_rdev;              // 设备号（如果是设备文件）
    
    struct timespec i_atime;             // 最后访问时间
    struct timespec i_mtime;             // 最后修改时间
    struct timespec i_ctime;             // 状态改变时间
    
    spinlock_t      i_lock;              // 保护i_blocks, i_bytes
    unsigned short  i_bytes;
    blkcnt_t        i_blocks;            // 分配的块数
    loff_t          i_size;              // 文件大小（字节）
    
    /*=== 同步与状态 ===*/
    unsigned long   i_state;             // I_DIRTY, I_LOCK等
    struct mutex    i_mutex;             // 主inode锁
    
    /*=== 缓存链接 ===*/
    struct hlist_node i_hash;            // inode哈希表
    struct list_head i_lru;              // 未使用inode LRU链表
    struct list_head i_sb_list;          // super_block->s_inodes链表
    union {
        struct list_head i_dentry;       // 所有引用此inode的dentry
        struct rcu_head  i_rcu;
    };
    
    atomic_t        i_count;             // 引用计数
    const struct file_operations *i_fop; // 默认file_operations
    
    struct address_space i_data;         // 内嵌的address_space
    
    union {
        struct pipe_inode_info *i_pipe;  // 如果是管道
        struct block_device    *i_bdev;  // 如果是块设备
        struct cdev            *i_cdev;  // 如果是字符设备
    };
    
    void            *i_private;          // 文件系统私有数据
};
```

**内存布局图：**

```
                        struct inode (~400 bytes)
┌─────────────────────────────────────────────────────────────────────────────┐
│  i_mode = 0100644 (regular file, rw-r--r--)                                 │
│  i_uid = 1000, i_gid = 1000                                                 │
│  i_ino = 12345                                                              │
├─────────────────────────────────────────────────────────────────────────────┤
│  i_size = 4096                                                              │
│  i_blocks = 8 (4KB blocks)                                                  │
│  i_atime = 2024-01-01 12:00:00                                             │
│  i_mtime = 2024-01-01 11:00:00                                             │
├─────────────────────────────────────────────────────────────────────────────┤
│  i_sb ────────────────────────────► super_block (ext4)                      │
│  i_op ────────────────────────────► ext4_file_inode_operations              │
│  i_fop ───────────────────────────► ext4_file_operations                    │
├─────────────────────────────────────────────────────────────────────────────┤
│  i_mapping ───────────────────────► &i_data (usually points to self)        │
│                                                                             │
│  i_data (embedded address_space):                                           │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │  host = this inode                                                  │   │
│  │  page_tree (radix tree of cached pages)                             │   │
│  │  a_ops → ext4_aops (address_space_operations)                       │   │
│  │  nrpages = 1 (number of cached pages)                               │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
├─────────────────────────────────────────────────────────────────────────────┤
│  i_hash ──────────────────────────► inode hash table bucket                 │
│  i_dentry ────────────────────────► list of dentries (hardlinks)            │
│  i_count = 2                        (2 processes have it open)              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 3.4 struct dentry (Directory Entry)

```c
// include/linux/dcache.h:116-145

struct dentry {
    /*=== RCU查找触及的字段 ===*/
    unsigned int d_flags;              // DCACHE_* flags
    seqcount_t d_seq;                  // 顺序锁（用于无锁查找）
    struct hlist_bl_node d_hash;       // 查找哈希表链表
    struct dentry *d_parent;           // 父目录dentry
    struct qstr d_name;                // 文件名（含哈希）
    
    struct inode *d_inode;             // 对应的inode（NULL=negative dentry）
    unsigned char d_iname[DNAME_INLINE_LEN]; // 短名内联存储
    
    /*=== 引用计数 ===*/
    unsigned int d_count;              // 引用计数
    spinlock_t d_lock;                 // per-dentry锁
    
    const struct dentry_operations *d_op;  // 操作函数表
    struct super_block *d_sb;          // 文件系统super_block
    unsigned long d_time;              // revalidate时间戳
    void *d_fsdata;                    // 文件系统私有数据
    
    struct list_head d_lru;            // LRU链表（未使用dentry）
    union {
        struct list_head d_child;      // 父目录的子列表
        struct rcu_head d_rcu;
    } d_u;
    struct list_head d_subdirs;        // 子目录/文件列表
    struct list_head d_alias;          // inode->i_dentry别名列表
};

// struct qstr - "quick string"
struct qstr {
    unsigned int hash;                 // 名称哈希值
    unsigned int len;                  // 名称长度
    const unsigned char *name;         // 指向d_iname或外部分配
};
```

**dentry树结构：**

```
                      dentry cache (dcache)

                         "/" (root)
                      ┌────────────┐
                      │ d_inode ───┼──► inode #2
                      │ d_parent ──┼──► self (IS_ROOT)
                      │ d_subdirs ─┼──┐
                      │ d_name="/" │  │
                      └────────────┘  │
                                      │ d_child list
              ┌───────────────────────┴───────────────────────┐
              │                                               │
              ▼                                               ▼
        "/home"                                          "/etc"
     ┌────────────┐                                   ┌────────────┐
     │ d_inode ───┼──► inode #100                     │ d_inode ───┼──► inode #50
     │ d_parent ──┼──► "/" dentry                     │ d_parent ──┼──► "/" dentry
     │ d_subdirs ─┼──┐                                │ d_subdirs ─┼──► ...
     │d_name="home"│  │                                │d_name="etc"│
     └────────────┘  │                                └────────────┘
                     │
                     ▼
             "/home/user"
          ┌────────────────┐
          │ d_inode ───────┼──► inode #1000
          │ d_parent ──────┼──► "/home" dentry
          │ d_subdirs ─────┼──► ...
          │ d_name="user"  │
          │ d_alias ───────┼──► (hardlink list)
          └────────────────┘

Negative dentry (文件不存在):
          ┌────────────────┐
          │ d_inode = NULL │ ← 表示文件不存在
          │ d_parent = ... │   (缓存"不存在"的查找结果)
          │ d_name="xyz"   │
          └────────────────┘
```

### 3.5 struct super_block (Filesystem Instance)

```c
// include/linux/fs.h:1400-1470

struct super_block {
    struct list_head    s_list;          // 全局super_blocks链表
    dev_t               s_dev;           // 设备号
    unsigned char       s_blocksize_bits;
    unsigned long       s_blocksize;     // 块大小
    loff_t              s_maxbytes;      // 最大文件大小
    
    struct file_system_type *s_type;     // 文件系统类型
    const struct super_operations *s_op; // 超级块操作
    
    unsigned long       s_flags;         // MS_RDONLY等
    unsigned long       s_magic;         // 魔数（如EXT4_SUPER_MAGIC）
    struct dentry       *s_root;         // 根dentry
    
    struct rw_semaphore s_umount;        // 卸载信号量
    struct mutex        s_lock;
    int                 s_count;         // 引用计数
    atomic_t            s_active;        // 活动引用
    
    struct list_head    s_inodes;        // 所有inode列表
    struct list_head    s_dentry_lru;    // 未使用dentry LRU
    int                 s_nr_dentry_unused;
    
    struct list_head    s_inode_lru;     // 未使用inode LRU
    int                 s_nr_inodes_unused;
    
    struct block_device *s_bdev;         // 底层块设备
    
    char s_id[32];                       // 标识名称
    u8 s_uuid[16];                       // UUID
    void *s_fs_info;                     // 文件系统私有数据
};
```

### 3.6 Operations Structures

**file_operations - 文件操作：**
```c
// include/linux/fs.h:1583-1611

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
    int (*mmap) (struct file *, struct vm_area_struct *);
    int (*open) (struct inode *, struct file *);
    int (*flush) (struct file *, fl_owner_t id);
    int (*release) (struct inode *, struct file *);
    int (*fsync) (struct file *, loff_t, loff_t, int datasync);
    int (*fasync) (int, struct file *, int);
    int (*lock) (struct file *, int, struct file_lock *);
    ssize_t (*splice_write) (...);
    ssize_t (*splice_read) (...);
};
```

**inode_operations - inode操作：**
```c
// include/linux/fs.h:1613-1641

struct inode_operations {
    struct dentry * (*lookup) (struct inode *, struct dentry *, struct nameidata *);
    void * (*follow_link) (struct dentry *, struct nameidata *);
    int (*permission) (struct inode *, int);
    
    int (*readlink) (struct dentry *, char __user *, int);
    void (*put_link) (struct dentry *, struct nameidata *, void *);
    
    int (*create) (struct inode *, struct dentry *, int, struct nameidata *);
    int (*link) (struct dentry *, struct inode *, struct dentry *);
    int (*unlink) (struct inode *, struct dentry *);
    int (*symlink) (struct inode *, struct dentry *, const char *);
    int (*mkdir) (struct inode *, struct dentry *, int);
    int (*rmdir) (struct inode *, struct dentry *);
    int (*mknod) (struct inode *, struct dentry *, int, dev_t);
    int (*rename) (struct inode *, struct dentry *, struct inode *, struct dentry *);
    
    void (*truncate) (struct inode *);
    int (*setattr) (struct dentry *, struct iattr *);
    int (*getattr) (struct vfsmount *, struct dentry *, struct kstat *);
};
```

---

## 4. Entry Points & Call Paths

### 4.1 Key Entry Points

| Entry Point | Trigger | Purpose |
|-------------|---------|---------|
| `do_sys_open()` | open() syscall | Open file, create struct file |
| `vfs_read()` | read() syscall | Read data from file |
| `vfs_write()` | write() syscall | Write data to file |
| `filp_close()` | close() syscall | Close file descriptor |
| `vfs_stat()` | stat() syscall | Get file metadata |
| `path_lookup()` | Various | Resolve path to dentry/inode |
| `do_mount()` | mount() syscall | Mount filesystem |

### 4.2 The Open Path

```
User calls open("/home/user/file.txt", O_RDWR)
            │
            ▼
┌───────────────────────────────────────────────────────────────────────────┐
│  SYSCALL_DEFINE3(open, ...)                       [fs/open.c:997]         │
│       │                                                                   │
│       └── do_sys_open(AT_FDCWD, filename, flags, mode)  [fs/open.c:973]  │
│            │                                                              │
│            ├── getname(filename)           ← 从用户空间复制路径名         │
│            │                                                              │
│            ├── get_unused_fd_flags(flags)  ← 分配文件描述符号            │
│            │   └── __alloc_fd(files, 0, rlimit(RLIMIT_NOFILE), flags)    │
│            │                                                              │
│            ├── do_filp_open(dfd, pathname, &op, lookup_flags)            │
│            │   │                               [fs/namei.c]               │
│            │   │                                                          │
│            │   │  ┌───────────────────────────────────────────────────┐   │
│            │   │  │ PHASE 1: PATH RESOLUTION                          │   │
│            │   │  └───────────────────────────────────────────────────┘   │
│            │   ├── path_init(dfd, name, flags, &nd)                      │
│            │   │   // 设置起始点（cwd或根目录）                           │
│            │   │                                                          │
│            │   ├── link_path_walk(name, &nd)    ← 核心路径解析           │
│            │   │   │                                                      │
│            │   │   │ for each path component ("/home", "user", "file"):  │
│            │   │   │   ├── hash_name = hash(component)                   │
│            │   │   │   │                                                 │
│            │   │   │   ├── __d_lookup(parent, name)  ← dcache查找        │
│            │   │   │   │   // 在哈希表中查找dentry                       │
│            │   │   │   │   // 命中 → 直接使用cached dentry               │
│            │   │   │   │   // 未命中 → 调用i_op->lookup()                │
│            │   │   │   │                                                 │
│            │   │   │   ├── (if mount point) lookup_mnt()                 │
│            │   │   │   │   // 跨越挂载点                                  │
│            │   │   │   │                                                 │
│            │   │   │   ├── (if symlink) follow_link()                    │
│            │   │   │   │   // 跟随符号链接                                │
│            │   │   │   │                                                 │
│            │   │   │   └── inode_permission(inode, MAY_EXEC)             │
│            │   │   │       // 检查目录执行权限                            │
│            │   │   │                                                      │
│            │   │   └── Returns: dentry of "file.txt"                     │
│            │   │                                                          │
│            │   │  ┌───────────────────────────────────────────────────┐   │
│            │   │  │ PHASE 2: OPEN THE FILE                            │   │
│            │   │  └───────────────────────────────────────────────────┘   │
│            │   └── do_last(&nd, &path, &op, ...)                         │
│            │       │                                                      │
│            │       ├── (if O_CREAT && !exists) vfs_create()              │
│            │       │   └── i_op->create(dir, dentry, mode)               │
│            │       │                                                      │
│            │       ├── may_open(&path, acc_mode, flag)                   │
│            │       │   // 检查打开权限                                    │
│            │       │                                                      │
│            │       └── finish_open(file, dentry, open)                   │
│            │           │                                                  │
│            │           ├── get_empty_filp()     ← 分配struct file        │
│            │           │   // 从filp SLAB cache分配                      │
│            │           │                                                  │
│            │           ├── file->f_path.dentry = dentry                  │
│            │           │   file->f_path.mnt = mnt                        │
│            │           │   file->f_op = inode->i_fop                     │
│            │           │   file->f_mode = FMODE_READ | FMODE_WRITE       │
│            │           │   file->f_pos = 0                               │
│            │           │                                                  │
│            │           └── f_op->open(inode, file)                       │
│            │               // 调用具体文件系统的open                      │
│            │                                                              │
│            ├── fd_install(fd, file)            ← 安装到fd表              │
│            │   // current->files->fdt->fd[fd] = file                     │
│            │                                                              │
│            └── putname(filename)                                          │
│                                                                           │
│  Returns: fd (or negative error code)                                     │
└───────────────────────────────────────────────────────────────────────────┘
```

### 4.3 The Read Path

```
User calls read(fd, buf, count)
            │
            ▼
┌───────────────────────────────────────────────────────────────────────────┐
│  SYSCALL_DEFINE3(read, ...)                       [fs/read_write.c]       │
│       │                                                                   │
│       ├── fget_light(fd, &fput_needed)         ← 获取struct file         │
│       │   // 查找current->files->fdt->fd[fd]                             │
│       │   // 轻量级：如果只有当前线程访问files，不加锁                    │
│       │                                                                   │
│       └── vfs_read(file, buf, count, &pos)      [fs/read_write.c:364]    │
│            │                                                              │
│            ├── if (!(file->f_mode & FMODE_READ))                         │
│            │       return -EBADF;               ← 检查读权限             │
│            │                                                              │
│            ├── if (!file->f_op || (!f_op->read && !f_op->aio_read))      │
│            │       return -EINVAL;              ← 检查操作存在           │
│            │                                                              │
│            ├── rw_verify_area(READ, file, pos, count)                    │
│            │   // 检查访问区域合法性，调用security_file_permission        │
│            │                                                              │
│            └── file->f_op->read(file, buf, count, pos)                   │
│                │                                                          │
│                │  或者如果没有同步read方法：                              │
│                └── do_sync_read(file, buf, count, pos)                   │
│                    └── file->f_op->aio_read(...)                         │
│                                                                           │
│            ┌─────────────────────────────────────────────────────────┐    │
│            │ 典型的f_op->read (以generic_file_aio_read为例)           │    │
│            │                                                         │    │
│            │  generic_file_aio_read():                               │    │
│            │  ├── 检查是否可以使用页缓存                              │    │
│            │  │                                                       │    │
│            │  ├── do_generic_file_read():                            │    │
│            │  │   for each page needed:                              │    │
│            │  │   │                                                  │    │
│            │  │   ├── find_get_page(mapping, index)                  │    │
│            │  │   │   // 在页缓存(page cache)中查找                  │    │
│            │  │   │                                                  │    │
│            │  │   ├── 如果页面不在缓存中:                            │    │
│            │  │   │   └── page_cache_sync_readahead()                │    │
│            │  │   │       └── address_space_operations->readpage()   │    │
│            │  │   │           └── 从磁盘读取数据到页面               │    │
│            │  │   │                                                  │    │
│            │  │   └── copy_page_to_user(page, buf)                   │    │
│            │  │       // 从内核页面复制到用户缓冲区                  │    │
│            │  │                                                       │    │
│            │  └── 更新file->f_pos                                    │    │
│            └─────────────────────────────────────────────────────────┘    │
│                                                                           │
│  Returns: bytes read (or negative error code)                             │
└───────────────────────────────────────────────────────────────────────────┘
```

### 4.4 The Write Path

```
User calls write(fd, buf, count)
            │
            ▼
┌───────────────────────────────────────────────────────────────────────────┐
│  vfs_write(file, buf, count, pos)                 [fs/read_write.c]       │
│       │                                                                   │
│       ├── 检查FMODE_WRITE, f_op->write存在                               │
│       │                                                                   │
│       └── file->f_op->write(file, buf, count, pos)                       │
│           │                                                               │
│           │  典型的generic_file_aio_write():                             │
│           │  ├── mutex_lock(&inode->i_mutex)    ← 获取inode锁           │
│           │  │                                                            │
│           │  ├── generic_write_checks(file, pos, count)                  │
│           │  │   // 检查限制：O_APPEND, file size limits                 │
│           │  │                                                            │
│           │  ├── generic_perform_write():                                │
│           │  │   for each page:                                          │
│           │  │   │                                                       │
│           │  │   ├── a_ops->write_begin()       ← 准备页面              │
│           │  │   │   // 分配页面，可能需要预留空间                       │
│           │  │   │                                                       │
│           │  │   ├── copy_from_user_to_page()   ← 复制数据              │
│           │  │   │                                                       │
│           │  │   └── a_ops->write_end()         ← 完成写入              │
│           │  │       // 标记页面dirty，更新inode                         │
│           │  │                                                            │
│           │  ├── mutex_unlock(&inode->i_mutex)                           │
│           │  │                                                            │
│           │  └── 标记inode dirty → 加入writeback队列                    │
│           │      // 实际磁盘写入由后台writeback线程完成                  │
│           │                                                               │
└───────────────────────────────────────────────────────────────────────────┘
```

---

## 5. Core Workflows

### 5.1 Path Resolution (link_path_walk)

```c
// fs/namei.c - 简化的路径解析流程

static int link_path_walk(const char *name, struct nameidata *nd)
{
    struct path next;
    int err;
    
    // 跳过开头的'/'
    while (*name == '/')
        name++;
    if (!*name)
        return 0;  // 空路径
    
    for (;;) {
        struct qstr this;
        
        // 1. 提取路径组件 (e.g., "home" from "/home/user/file")
        this.name = name;
        do {
            name++;
        } while (*name && (*name != '/'));
        this.len = name - this.name;
        this.hash = full_name_hash(this.name, this.len);
        
        // 2. 特殊处理 "." 和 ".."
        if (this.name[0] == '.') {
            if (this.len == 1)
                continue;  // "." - 当前目录
            if (this.len == 2 && this.name[1] == '.') {
                // ".." - 父目录
                err = follow_dotdot(nd);
                if (err)
                    return err;
                continue;
            }
        }
        
        // 3. 在当前目录查找该组件
        err = do_lookup(nd, &this, &next);
        if (err)
            return err;
        
        // 4. 处理符号链接
        if (next.dentry->d_inode->i_op->follow_link) {
            err = follow_link(&next, nd, ...);
            if (err)
                return err;
        }
        
        // 5. 处理挂载点
        err = follow_mount(&next);
        if (err)
            return err;
        
        // 6. 检查目录执行权限
        err = inode_permission(next.dentry->d_inode, MAY_EXEC);
        if (err)
            return err;
        
        // 7. 移动到下一个组件
        path_to_nameidata(&next, nd);
        
        // 8. 跳过分隔符
        while (*name == '/')
            name++;
        if (!*name)
            return 0;  // 到达路径末尾
    }
}
```

**路径解析可视化：**

```
Path: "/home/user/file.txt"

Step 1: Start at root dentry (nd->path.dentry = "/")
        │
        ▼
Step 2: Extract "home", compute hash
        do_lookup(/, "home") → dcache lookup
        │
        ├── dcache hit? → Use cached dentry
        └── dcache miss? → Call i_op->lookup()
                           → ext4_lookup() reads from disk
                           → Create new dentry, add to dcache
        │
        ▼
Step 3: Check if "/home" is a mount point
        follow_mount() → Check mount hash table
        │
        ├── Not a mount point → Continue
        └── Mount point → Switch to mounted filesystem's root
        │
        ▼
Step 4: Check execute permission on "home" directory
        inode_permission(home_inode, MAY_EXEC)
        │
        ▼
Step 5: Move to "home" (nd->path.dentry = "/home")
        │
        ▼
Step 6-8: Repeat for "user"
        │
        ▼
Step 9-11: Process "file.txt" (final component)
        Handle differently based on operation:
        - open(): Lookup and open
        - create(): Create new file if O_CREAT
        - stat(): Just lookup
        │
        ▼
Result: nd->path.dentry = dentry of "file.txt"
        nd->path.mnt = mount of containing filesystem
```

### 5.2 Mounting a Filesystem

```c
// fs/namespace.c - 挂载流程

do_mount(dev_name, dir_name, type, flags, data)
    │
    ├── kern_path(dir_name, LOOKUP_FOLLOW, &path)
    │   // 解析挂载点路径
    │
    ├── do_new_mount(path, type, flags, dev_name, data)
    │   │
    │   ├── get_fs_type(type)
    │   │   // 查找注册的文件系统类型
    │   │   // e.g., ext4_fs_type for "ext4"
    │   │
    │   ├── fs_type->mount(fs_type, flags, dev_name, data)
    │   │   // 调用具体文件系统的mount函数
    │   │   // e.g., ext4_mount()
    │   │   │
    │   │   └── mount_bdev(fs_type, flags, dev_name, data, ext4_fill_super)
    │   │       │
    │   │       ├── blkdev_get_by_path(dev_name)
    │   │       │   // 打开块设备
    │   │       │
    │   │       ├── sget(fs_type, ...) 
    │   │       │   // 分配或复用super_block
    │   │       │
    │   │       └── fill_super(sb, data, silent)
    │   │           // ext4_fill_super():
    │   │           // - 读取磁盘上的superblock
    │   │           // - 初始化sb->s_op, sb->s_root
    │   │           // - 创建根inode和dentry
    │   │
    │   └── do_add_mount(newmnt, path, mnt_flags)
    │       // 将新挂载添加到挂载树
    │       // 更新mount哈希表
    │
    └── path_put(&path)
```

**挂载树结构：**

```
                     Mount Tree

            rootfs (/)
            ┌────────────────┐
            │ mnt_root = "/" │
            │ mnt_sb = sysfs │
            │ mnt_parent =   │
            │   self         │
            │ mnt_mountpoint │
            │   = "/"        │
            │ mnt_child ─────┼───┐
            └────────────────┘   │
                                 │
      ┌──────────────────────────┴────────────────────────┐
      │                                                   │
      ▼                                                   ▼
   /dev (devtmpfs)                                    /home (ext4)
┌────────────────┐                                 ┌────────────────┐
│ mnt_root="/"   │                                 │ mnt_root="/"   │
│ mnt_sb=devtmpfs│                                 │ mnt_sb=ext4    │
│ mnt_parent = / │                                 │ mnt_parent = / │
│ mnt_mountpoint │                                 │ mnt_mountpoint │
│   = "/dev"     │                                 │   = "/home"    │
└────────────────┘                                 └────────────────┘

Crossing mount point:
  When resolving "/home/user":
  1. Walk to "/" → rootfs
  2. Lookup "home" → Get dentry for mountpoint
  3. Check mount hash table: Is anything mounted on "/home"?
  4. YES → follow_mount() → Switch to ext4's root dentry
  5. Continue with "user" in ext4 filesystem
```

### 5.3 Dentry Cache Operations

```c
// fs/dcache.c - dentry缓存操作

// 查找dentry
struct dentry *__d_lookup(struct dentry *parent, struct qstr *name)
{
    struct hlist_bl_head *b = d_hash(parent, name->hash);
    struct hlist_bl_node *node;
    struct dentry *found = NULL;
    
    rcu_read_lock();
    hlist_bl_for_each_entry_rcu(dentry, node, b, d_hash) {
        if (dentry->d_parent != parent)
            continue;
        if (d_unhashed(dentry))
            continue;
        if (!d_same_name(dentry, parent, name))
            continue;
        
        // 找到匹配的dentry
        spin_lock(&dentry->d_lock);
        if (!d_unhashed(dentry)) {
            dentry->d_count++;
            found = dentry;
        }
        spin_unlock(&dentry->d_lock);
        break;
    }
    rcu_read_unlock();
    return found;
}

// 添加dentry到缓存
void d_add(struct dentry *entry, struct inode *inode)
{
    d_instantiate(entry, inode);  // 关联inode
    d_rehash(entry);              // 加入哈希表
}

// 删除dentry
void d_delete(struct dentry *dentry)
{
    if (dentry->d_count == 1) {
        dput(dentry);  // 最后一个引用，可能释放
    } else {
        d_drop(dentry);  // 从哈希表移除
    }
}
```

**dcache哈希表：**

```
                    Dentry Hash Table
                    
hash = f(parent, name) % table_size

Bucket 0    Bucket 1    Bucket 2    ...    Bucket N
   │           │           │                  │
   ▼           ▼           ▼                  ▼
┌─────┐     ┌─────┐     ┌─────┐            ┌─────┐
│ d1  │     │ d5  │     │NULL │            │ d10 │
│"foo"│     │"bar"│     │     │            │"qux"│
└──┬──┘     └──┬──┘     └─────┘            └──┬──┘
   │           │                              │
   ▼           ▼                              ▼
┌─────┐     ┌─────┐                        ┌─────┐
│ d2  │     │NULL │                        │NULL │
│"baz"│     │     │                        │     │
└──┬──┘     └─────┘                        └─────┘
   │
   ▼
┌─────┐
│NULL │
└─────┘

Lookup "/home/user/foo":
1. hash = full_name_hash("foo", 3)
2. bucket = hash % table_size = 0
3. Walk bucket 0's chain, compare (parent, name)
4. Found d1 with matching parent and name "foo"
```

---

## 6. Important Algorithms & Mechanisms

### 6.1 RCU Path Lookup (rcu-walk)

Linux 3.2引入了"RCU-walk"模式，允许无锁路径查找：

```c
// fs/namei.c - RCU路径查找

/*
 * RCU-walk vs REF-walk:
 * 
 * RCU-walk: 无锁，使用RCU和seqcount
 * - 适用于大多数只读操作
 * - 如果遇到需要阻塞的情况，退化到REF-walk
 * 
 * REF-walk: 传统模式，获取dentry引用和锁
 * - 用于创建、删除等修改操作
 */

// RCU-walk中的dentry验证
static inline bool d_lookup_is_negative(struct dentry *dentry)
{
    seqcount_t *seq = &dentry->d_seq;
    unsigned seq_nr = read_seqcount_begin(seq);
    
    // 快速检查
    if (d_unhashed(dentry))
        return false;
        
    // 验证seqcount没有变化
    if (read_seqcount_retry(seq, seq_nr))
        return false;  // 需要重试
        
    return ACCESS_ONCE(dentry->d_inode) == NULL;
}
```

**RCU-walk vs REF-walk：**

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          RCU-WALK MODE                                      │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  优点:                                                                      │
│  • 无锁操作，极高并发性                                                     │
│  • 不需要获取dentry引用                                                     │
│  • 不需要原子操作增减计数器                                                 │
│                                                                             │
│  约束:                                                                      │
│  • 不能阻塞                                                                 │
│  • 不能调用文件系统可能睡眠的函数                                           │
│  • 必须使用seqcount验证                                                     │
│                                                                             │
│  退化条件（切换到REF-walk）:                                                │
│  • 遇到需要revalidate的dentry                                               │
│  • 符号链接跟随                                                             │
│  • 挂载点跨越失败                                                           │
│  • seqcount验证失败                                                         │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘

                     RCU-walk Path Lookup
                     
Start: rcu_read_lock()
  │
  ▼
┌─────────────────────────────────────────────────────────────────┐
│ for each path component:                                        │
│   1. Read dentry->d_seq (seqcount)                              │
│   2. Check dentry in hash table (RCU protected)                 │
│   3. Verify d_seq didn't change                                 │
│   4. If changed → retry or fall back to REF-walk                │
│   5. Read inode pointer (RCU protected)                         │
│   6. Check permissions (no blocking!)                           │
└─────────────────────────────────────────────────────────────────┘
  │
  ▼
End: rcu_read_unlock()
```

### 6.2 Page Cache

```
                        PAGE CACHE ARCHITECTURE
                        
┌─────────────────────────────────────────────────────────────────────────────┐
│                           address_space                                      │
│  (one per inode, manages cached pages for a file)                           │
│                                                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                    page_tree (radix tree)                            │   │
│  │                                                                      │   │
│  │                        [root]                                        │   │
│  │                           │                                          │   │
│  │          ┌────────────────┼────────────────┐                         │   │
│  │          │                │                │                         │   │
│  │        [0-63]          [64-127]        [128-191]                     │   │
│  │          │                │                │                         │   │
│  │    ┌─────┴─────┐          │          ┌─────┴─────┐                   │   │
│  │    │           │          │          │           │                   │   │
│  │  Page 0     Page 5     Page 100    Page 128   Page 150              │   │
│  │  (index 0)  (index 5)  (index 100) (index 128)(index 150)           │   │
│  │                                                                      │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                                                             │
│  a_ops (address_space_operations):                                          │
│  ├── readpage(file, page)        ← 从磁盘读取单页                          │
│  ├── readpages(file, pages, nr)  ← 批量预读                                │
│  ├── writepage(page, wbc)        ← 写回单页                                │
│  ├── writepages(mapping, wbc)    ← 批量写回                                │
│  └── write_begin/write_end       ← 准备/完成写入                           │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘

Read flow (cache miss):
1. find_get_page(mapping, index) → NULL (not cached)
2. Allocate new page
3. Add to radix tree: add_to_page_cache()
4. Call a_ops->readpage() → block layer → disk
5. Wait for I/O completion
6. Copy to user buffer

Write flow:
1. find_or_create_page(mapping, index)
2. a_ops->write_begin() → prepare page, allocate blocks
3. Copy from user buffer to page
4. a_ops->write_end() → update inode size
5. Mark page dirty: set_page_dirty()
6. Later: writeback thread calls a_ops->writepage()
```

### 6.3 Inode Cache

```c
// fs/inode.c - inode缓存管理

// inode哈希表
static struct hlist_head *inode_hashtable;

// 查找或创建inode
struct inode *iget_locked(struct super_block *sb, unsigned long ino)
{
    struct hlist_head *head = inode_hashtable + hash(sb, ino);
    struct inode *inode;
    
    // 1. 在哈希表中查找
    spin_lock(&inode_hash_lock);
    inode = find_inode_fast(sb, head, ino);
    spin_unlock(&inode_hash_lock);
    
    if (inode) {
        wait_on_inode(inode);
        return inode;  // 缓存命中
    }
    
    // 2. 缓存未命中，分配新inode
    inode = alloc_inode(sb);  // 使用SLAB或sb->s_op->alloc_inode
    
    // 3. 加入哈希表
    spin_lock(&inode_hash_lock);
    old = find_inode_fast(sb, head, ino);
    if (!old) {
        hlist_add_head(&inode->i_hash, head);
        inode->i_state = I_NEW;
    } else {
        // 竞争失败，使用已存在的
        destroy_inode(inode);
        inode = old;
    }
    spin_unlock(&inode_hash_lock);
    
    return inode;  // I_NEW表示需要从磁盘读取
}
```

---

## 7. Concurrency & Synchronization

### 7.1 VFS Locking Hierarchy

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         VFS LOCKING HIERARCHY                               │
│                      (acquire in this order)                                │
└─────────────────────────────────────────────────────────────────────────────┘

1. rename_lock (seqlock)
   │  保护dentry树结构在rename期间
   │
   ▼
2. i_mutex (per-inode mutex)
   │  保护inode数据
   │  规则：parent before child
   │        smaller pointer first (for same-level)
   │
   ▼
3. i_lock (per-inode spinlock)
   │  保护i_state, i_count
   │
   ▼
4. dentry->d_lock (per-dentry spinlock)
   │  保护d_count, d_flags, d_name
   │  规则：parent before child
   │
   ▼
5. dcache_lru_lock (global)
   │  保护dentry LRU链表
   │
   ▼
6. inode_hash_lock (global)
      保护inode哈希表
```

### 7.2 Common Lock Patterns

```c
// 模式1：目录操作（需要parent的i_mutex）
int vfs_mkdir(struct inode *dir, struct dentry *dentry, int mode)
{
    mutex_lock(&dir->i_mutex);       // 锁定父目录
    error = dir->i_op->mkdir(dir, dentry, mode);
    mutex_unlock(&dir->i_mutex);
    return error;
}

// 模式2：rename（需要锁定两个目录）
int vfs_rename(struct inode *old_dir, struct dentry *old_dentry,
               struct inode *new_dir, struct dentry *new_dentry)
{
    // 按地址顺序锁定，避免死锁
    if (old_dir < new_dir) {
        mutex_lock(&old_dir->i_mutex);
        mutex_lock_nested(&new_dir->i_mutex, I_MUTEX_PARENT);
    } else {
        mutex_lock(&new_dir->i_mutex);
        mutex_lock_nested(&old_dir->i_mutex, I_MUTEX_PARENT);
    }
    
    error = old_dir->i_op->rename(old_dir, old_dentry, new_dir, new_dentry);
    
    mutex_unlock(&new_dir->i_mutex);
    mutex_unlock(&old_dir->i_mutex);
    return error;
}

// 模式3：读文件（通常不需要i_mutex，但可能需要页锁）
ssize_t generic_file_read(file, buf, count, pos)
{
    // 不持有i_mutex
    // 页级别锁通过page->flags中的PG_locked实现
    for each page:
        lock_page(page);
        // 读取
        unlock_page(page);
}

// 模式4：写文件（需要i_mutex防止并发写入）
ssize_t generic_file_write(file, buf, count, pos)
{
    mutex_lock(&inode->i_mutex);
    // 写入操作
    mutex_unlock(&inode->i_mutex);
}
```

### 7.3 Dentry Reference Counting

```c
// 获取dentry引用
static inline struct dentry *dget(struct dentry *dentry)
{
    if (dentry)
        dentry->d_count++;  // 在d_lock保护下
    return dentry;
}

// 释放dentry引用
void dput(struct dentry *dentry)
{
    if (!dentry)
        return;
        
repeat:
    spin_lock(&dentry->d_lock);
    if (dentry->d_count > 1) {
        dentry->d_count--;
        spin_unlock(&dentry->d_lock);
        return;
    }
    
    // d_count == 1，可能需要释放
    if (d_unhashed(dentry)) {
        // 已从哈希表移除，可以释放
        dentry_kill(dentry);  // 释放dentry
    } else {
        // 加入LRU链表，稍后可能回收
        dentry->d_count--;
        dentry_lru_add(dentry);
        spin_unlock(&dentry->d_lock);
    }
}
```

---

## 8. Performance Considerations

### 8.1 Hot Paths

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          HOT PATHS                                          │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  1. Path Lookup (namei.c)                                                   │
│     • link_path_walk() - 每次open/stat都会调用                             │
│     • __d_lookup() - dcache查找，极其频繁                                  │
│     • RCU-walk模式减少锁争用                                               │
│                                                                             │
│  2. Read/Write (read_write.c)                                               │
│     • vfs_read()/vfs_write() - 每次I/O调用                                 │
│     • Page cache lookup - find_get_page()                                  │
│                                                                             │
│  3. File Descriptor Operations                                              │
│     • fget_light() - 每次系统调用开始时获取file                            │
│     • 优化：如果files_struct只有一个用户，不需要原子操作                   │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────┐
│                          COLD PATHS                                         │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  1. File System Mount/Unmount                                               │
│  2. File Creation/Deletion                                                  │
│  3. Disk I/O (cache miss)                                                   │
│  4. fsync() - 同步写入                                                      │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 8.2 Cache Efficiency

```
                    VFS CACHING STRATEGY
                    
┌─────────────────────────────────────────────────────────────────────────────┐
│                                                                             │
│  DCACHE (Directory Entry Cache)                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │ • 缓存路径名→inode的映射                                            │   │
│  │ • 哈希表 + LRU链表                                                  │   │
│  │ • Negative entries: 缓存"文件不存在"的结果                          │   │
│  │ • 通常最重要的缓存                                                  │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                                                             │
│  INODE CACHE                                                                │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │ • 缓存inode元数据                                                   │   │
│  │ • 哈希表(sb, ino) → inode                                          │   │
│  │ • inode被dentry引用时不能被回收                                     │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                                                             │
│  PAGE CACHE                                                                 │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │ • 缓存文件数据页                                                    │   │
│  │ • Radix tree per inode                                              │   │
│  │ • Write-back caching (dirty pages)                                  │   │
│  │ • Read-ahead (预读)                                                 │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                                                             │
│  Cache Pressure:                                                            │
│  /proc/sys/vm/vfs_cache_pressure = 100 (default)                           │
│  • < 100: 倾向保留dcache/icache                                            │
│  • > 100: 更积极回收                                                       │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 8.3 Data Structure Layout

```c
// dentry结构优化：
struct dentry {
    // RCU查找触及的字段放在前面，同一cache line
    unsigned int d_flags;
    seqcount_t d_seq;
    struct hlist_bl_node d_hash;
    struct dentry *d_parent;
    struct qstr d_name;
    struct inode *d_inode;
    unsigned char d_iname[DNAME_INLINE_LEN];  // 短名内联，避免额外分配
    
    // ...
};

// DNAME_INLINE_LEN在64位系统为32字节
// 大多数文件名都小于32字符，避免了额外的内存分配

// inode结构优化：
struct inode {
    // 只读、常访问字段在前
    umode_t i_mode;
    unsigned short i_opflags;
    uid_t i_uid;
    gid_t i_gid;
    // ...
    
    // 不常访问的字段在后
    // ...
};
```

---

## 9. Common Pitfalls & Bugs

### 9.1 Typical Mistakes

| 错误 | 后果 | 正确做法 |
|------|------|----------|
| 忘记dput() | 内存泄漏，inode无法释放 | 每个dget()对应一个dput() |
| 在持有i_mutex时调用可能死锁的函数 | 死锁 | 遵循锁序，使用mutex_lock_nested() |
| 直接操作dentry而不检查d_unhashed | 操作已删除的文件 | 检查d_unhashed()或持有d_lock |
| f_op->read()中阻塞而不设置可中断状态 | 无法被信号中断 | 使用可中断的等待 |
| 不检查path_lookup()返回值 | 空指针访问 | 总是检查错误码 |

### 9.2 Filesystem Developer Mistakes

```c
// 错误1：忘记在lookup()中处理负dentry
struct dentry *my_lookup(struct inode *dir, struct dentry *dentry, ...)
{
    struct inode *inode = my_get_inode(dir, dentry->d_name.name);
    // 错误：如果inode为NULL，应该返回NULL而不是ERR_PTR
    if (!inode)
        return ERR_PTR(-ENOENT);  // 错！
    
    // 正确做法：
    if (!inode)
        return NULL;  // 这会创建negative dentry
    
    d_add(dentry, inode);
    return NULL;
}

// 错误2：在i_op->create()中不更新父目录mtime
int my_create(struct inode *dir, struct dentry *dentry, ...)
{
    // 创建文件...
    d_instantiate(dentry, inode);
    // 忘记了：
    dir->i_mtime = dir->i_ctime = CURRENT_TIME;
    mark_inode_dirty(dir);
}

// 错误3：write()中不正确处理O_APPEND
ssize_t my_write(struct file *file, ...)
{
    // 错误：没有检查O_APPEND
    pos = file->f_pos;
    
    // 正确：
    if (file->f_flags & O_APPEND)
        pos = i_size_read(inode);
}
```

### 9.3 Race Conditions to Watch

```
Race 1: unlink() vs. read()
┌────────────────────┬────────────────────┐
│     Thread 1       │     Thread 2       │
├────────────────────┼────────────────────┤
│ open("/file")      │                    │
│ fd = 3             │                    │
│                    │ unlink("/file")    │
│                    │ // 成功，nlink=0   │
│ read(fd, ...)      │                    │
│ // 仍然成功！      │                    │
│ // 因为struct file │                    │
│ // 持有inode引用   │                    │
│ close(fd)          │                    │
│ // 现在inode被释放 │                    │
└────────────────────┴────────────────────┘
这不是bug，是预期行为。

Race 2: rename() vs. lookup()
┌────────────────────┬────────────────────┐
│     Thread 1       │     Thread 2       │
├────────────────────┼────────────────────┤
│ stat("/a/b/c")     │                    │
│ lookup "a" OK      │                    │
│ lookup "b" OK      │                    │
│                    │ rename("/a/b","/x")│
│ lookup "c"         │                    │
│ // 可能失败或成功  │                    │
│ // 取决于时机      │                    │
└────────────────────┴────────────────────┘
VFS使用d_seq和rename_lock处理这种情况。
```

---

## 10. How to Read This Code Yourself

### 10.1 Recommended Reading Order

```
第一阶段：理解数据结构
1. include/linux/fs.h: struct file, struct inode
2. include/linux/dcache.h: struct dentry
3. include/linux/fs.h: struct super_block
4. include/linux/fs.h: 各种operations结构

第二阶段：理解路径解析
5. fs/namei.c: path_lookup(), link_path_walk()
6. fs/dcache.c: __d_lookup(), d_alloc()

第三阶段：理解文件操作
7. fs/open.c: do_sys_open(), filp_open()
8. fs/read_write.c: vfs_read(), vfs_write()
9. fs/file_table.c: alloc_file(), fput()

第四阶段：理解挂载
10. fs/super.c: sget(), deactivate_super()
11. fs/namespace.c: do_mount()

第五阶段：看一个具体文件系统
12. fs/ramfs/: 最简单的文件系统
13. fs/ext4/: 功能完整的磁盘文件系统
```

### 10.2 Useful Search Commands

```bash
# 查找VFS操作调用
grep -rn "i_op->lookup" fs/

# 查找file_operations定义
grep -rn "struct file_operations.*=" fs/ext4/

# 查找特定函数
grep -n "^ssize_t vfs_read" fs/read_write.c

# 查找dentry缓存操作
grep -rn "d_lookup\|d_add\|d_delete" fs/

# 使用cscope
# cscope -d
# 查找符号定义：Ctrl+\ s
# 查找调用者：Ctrl+\ c
```

### 10.3 Debug Interfaces

```bash
# 查看挂载信息
cat /proc/mounts
cat /proc/self/mountinfo

# 查看文件系统统计
cat /proc/filesystems

# 查看dcache统计
cat /proc/sys/fs/dentry-state

# 查看inode统计
cat /proc/sys/fs/inode-nr
cat /proc/sys/fs/inode-state

# 调试打开的文件
ls -l /proc/<pid>/fd/
cat /proc/<pid>/fdinfo/<fd>

# VFS缓存压力
echo 50 > /proc/sys/vm/vfs_cache_pressure

# 手动清除缓存（测试用）
echo 3 > /proc/sys/vm/drop_caches
# 1 = pagecache, 2 = dentries+inodes, 3 = all
```

---

## 11. Summary & Mental Model

### One-Paragraph Summary

VFS是Linux内核的文件系统抽象层，通过四个核心对象（file、inode、dentry、super_block）和对应的操作函数表（file_operations、inode_operations、super_operations）实现统一的文件访问接口。路径解析（namei.c）将用户提供的路径字符串转换为dentry/inode，dcache（目录项缓存）和inode cache大幅减少磁盘访问。每个打开的文件用struct file表示，包含当前位置、访问模式和指向底层dentry/inode的指针。挂载机制通过vfsmount将不同文件系统整合到统一的命名空间中。Linux 3.2引入的RCU-walk使得只读路径查找几乎无锁，显著提升了并发性能。

### Key Invariants

1. **dentry→inode一致性**: dentry->d_inode非NULL时，inode有效且被正确引用
2. **打开文件持有引用**: struct file持有dentry引用，dentry持有inode引用
3. **dcache是icache的主人**: inode通过dentry引用保持活跃，dentry释放时inode可能被回收
4. **负dentry**: d_inode==NULL表示文件不存在，这也被缓存以避免重复查找
5. **mount点**: dentry既是父文件系统的点，也关联子文件系统的根

### Mental Model

```
┌─────────────────────────────────────────────────────────────────────────────┐
│  把VFS想象成一个"通用文件访问API"：                                          │
│                                                                             │
│  • 用户只需要知道：open(), read(), write(), close()                        │
│  • VFS翻译这些调用为具体文件系统的操作                                      │
│                                                                             │
│  类比图书馆：                                                               │
│  ┌─────────────────────────────────────────────────────────────┐            │
│  │ 你（用户）: "我要借《Linux内核》这本书"                      │            │
│  │            ↓                                                 │            │
│  │ 图书管理员（VFS）: 查目录卡片（dcache）→ 找到书架位置       │            │
│  │            ↓                                                 │            │
│  │ 书架（inode）: 存放书的元信息和位置                          │            │
│  │            ↓                                                 │            │
│  │ 具体的书（page cache中的数据）                               │            │
│  │            ↓                                                 │            │
│  │ 借书证（struct file）: 你当前阅读到哪一页                    │            │
│  └─────────────────────────────────────────────────────────────┘            │
│                                                                             │
│  缓存的重要性：                                                             │
│  • dcache: 记住"这本书在哪个书架"（路径→inode映射）                        │
│  • icache: 缓存"书架信息"（inode元数据）                                    │
│  • page cache: 缓存"书的内容"（文件数据）                                   │
│  • 大多数访问都能从缓存得到，不需要真正去"仓库"（磁盘）                     │
│                                                                             │
│  挂载的概念：                                                               │
│  • 就像图书馆的不同分馆                                                     │
│  • 你可以无缝地从"主馆"走到"分馆"                                           │
│  • VFS自动处理"跨馆"的情况                                                  │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 12. What to Study Next

### Recommended Learning Order

| 顺序 | 子系统 | 与VFS的关系 |
|------|--------|-------------|
| 1 | **进程管理 (files_struct)** | 每个进程的打开文件表 |
| 2 | **内存管理 (page cache)** | 文件数据缓存 |
| 3 | **块设备层** | VFS如何与磁盘交互 |
| 4 | **ext4文件系统** | 最常用的磁盘文件系统实现 |
| 5 | **procfs/sysfs** | 伪文件系统，理解VFS的灵活性 |
| 6 | **NFS** | 网络文件系统 |
| 7 | **I/O调度器** | 优化磁盘访问顺序 |
| 8 | **命名空间 (mount ns)** | 容器文件系统隔离 |

### Related Files to Study

```
fs/namei.c            - 路径解析（最重要）
fs/dcache.c           - dentry缓存
fs/inode.c            - inode管理
fs/open.c             - 文件打开
fs/read_write.c       - 读写操作
fs/super.c            - 超级块管理
fs/namespace.c        - 挂载命名空间
fs/file_table.c       - 文件表
mm/filemap.c          - 页缓存操作
fs/ramfs/             - 最简单的文件系统（学习用）
fs/ext4/              - 功能完整的文件系统
```

---

## Appendix: Quick Reference

### A. Key System Calls

| Syscall | VFS Entry Point | Purpose |
|---------|-----------------|---------|
| open() | do_sys_open() | 打开文件 |
| read() | vfs_read() | 读取数据 |
| write() | vfs_write() | 写入数据 |
| close() | filp_close() | 关闭文件 |
| stat() | vfs_stat() | 获取元数据 |
| lseek() | vfs_llseek() | 移动文件指针 |
| mkdir() | vfs_mkdir() | 创建目录 |
| unlink() | vfs_unlink() | 删除文件 |
| rename() | vfs_rename() | 重命名 |
| mount() | do_mount() | 挂载文件系统 |

### B. Important /proc Files

```bash
/proc/filesystems      # 已注册的文件系统类型
/proc/mounts           # 当前挂载点
/proc/self/mountinfo   # 详细挂载信息
/proc/sys/fs/file-nr   # 已分配/使用/最大文件数
/proc/sys/fs/inode-nr  # inode统计
/proc/sys/fs/dentry-state  # dentry统计
```

### C. Common File Flags

```c
// Open flags (fs.h)
O_RDONLY    0x0000    // 只读
O_WRONLY    0x0001    // 只写
O_RDWR      0x0002    // 读写
O_CREAT     0x0040    // 不存在则创建
O_EXCL      0x0080    // 与O_CREAT一起使用，文件必须不存在
O_TRUNC     0x0200    // 截断为0
O_APPEND    0x0400    // 追加写
O_NONBLOCK  0x0800    // 非阻塞
O_SYNC      0x1000    // 同步写
O_DIRECTORY 0x10000   // 必须是目录
O_NOFOLLOW  0x20000   // 不跟随符号链接
```

---

**Author**: Linux Kernel Study Guide  
**Kernel Version**: 3.2.0 ("Saber-toothed Squirrel")  
**Last Updated**: Based on kernel source analysis

