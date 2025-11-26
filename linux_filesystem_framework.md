# Linux 文件系统框架深入讲解

基于 Linux 3.2 内核源码分析

---

## 目录

- [文件系统架构概述](#文件系统架构概述)
- [VFS 核心数据结构](#vfs-核心数据结构)
- [文件系统注册与挂载](#文件系统注册与挂载)
- [文件操作流程](#文件操作流程)
- [页缓存与写回机制](#页缓存与写回机制)
- [关键源码文件](#关键源码文件)

---

## 文件系统架构概述

### 整体架构图

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                            用户空间 (User Space)                             │
│                                                                              │
│        open()    read()    write()    close()    stat()    mkdir()          │
└───────────────────────────────────┬──────────────────────────────────────────┘
                                    │ 系统调用
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                        VFS (Virtual File System)                             │
│                           虚拟文件系统层                                      │
│                                                                              │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐         │
│  │ super_block │  │   inode     │  │   dentry    │  │    file     │         │
│  │  (超级块)    │  │ (索引节点)  │  │  (目录项)   │   │  (文件对象)  │         │
│  └─────────────┘  └─────────────┘  └─────────────┘  └─────────────┘         │
│                                                                              │
│  ┌──────────────────────────────────────────────────────────────────────┐   │
│  │                    统一的操作接口                                     │   │
│  │  file_operations  inode_operations  super_operations  dentry_operations │
│  └──────────────────────────────────────────────────────────────────────┘   │
└───────────────────────────────────┬──────────────────────────────────────────┘
                                    │
        ┌───────────────────────────┼───────────────────────────┐
        │                           │                           │
        ▼                           ▼                           ▼
┌───────────────────┐     ┌───────────────────┐     ┌───────────────────┐
│     ext4          │     │       xfs         │     │       nfs         │
│   本地文件系统     │     │   本地文件系统     │     │   网络文件系统     │
└─────────┬─────────┘     └─────────┬─────────┘     └─────────┬─────────┘
          │                         │                         │
          ▼                         ▼                         ▼
┌───────────────────┐     ┌───────────────────┐     ┌───────────────────┐
│    Page Cache     │     │    Page Cache     │     │    Page Cache     │
│      页缓存       │     │      页缓存       │     │      页缓存       │
└─────────┬─────────┘     └─────────┬─────────┘     └─────────┬─────────┘
          │                         │                         │
          ▼                         ▼                         ▼
┌───────────────────┐     ┌───────────────────┐     ┌───────────────────┐
│   Block Layer     │     │   Block Layer     │     │   Network Stack   │
│     块设备层      │     │     块设备层      │     │     网络协议栈     │
└─────────┬─────────┘     └─────────┬─────────┘     └─────────┬─────────┘
          │                         │                         │
          ▼                         ▼                         ▼
┌───────────────────────────────────────────────────────────────────────────┐
│                              硬件层                                        │
│                     磁盘        SSD        网络设备                        │
└───────────────────────────────────────────────────────────────────────────┘
```

### VFS 的设计理念

1. **抽象层**: 为所有文件系统提供统一接口
2. **可扩展**: 新文件系统只需实现指定接口
3. **缓存**: 通过 dentry cache 和 inode cache 提高性能
4. **统一命名空间**: 所有文件系统挂载到统一的目录树

---

## VFS 核心数据结构

### 1. struct super_block - 超级块

超级块代表一个已挂载的文件系统实例。

```c
// include/linux/fs.h
struct super_block {
    struct list_head    s_list;         // 超级块链表
    dev_t               s_dev;          // 设备标识符
    unsigned char       s_blocksize_bits;
    unsigned long       s_blocksize;    // 块大小
    loff_t              s_maxbytes;     // 最大文件大小
    struct file_system_type *s_type;    // 文件系统类型
    const struct super_operations *s_op; // 超级块操作
    
    unsigned long       s_flags;        // 挂载标志
    unsigned long       s_magic;        // 魔数
    struct dentry       *s_root;        // 根目录项
    struct rw_semaphore s_umount;       // 卸载信号量
    
    struct list_head    s_inodes;       // 所有 inode 列表
    struct list_head    s_dirty;        // 脏 inode 列表
    struct list_head    s_files;        // 已打开文件列表
    
    struct block_device *s_bdev;        // 块设备
    void                *s_fs_info;     // 文件系统私有数据
    // ...
};
```

### 2. struct inode - 索引节点

inode 代表文件系统中的一个文件对象 (文件、目录、设备等)。

```c
// include/linux/fs.h
struct inode {
    umode_t             i_mode;         // 文件类型和权限
    uid_t               i_uid;          // 所有者 UID
    gid_t               i_gid;          // 所有者 GID
    unsigned int        i_flags;        // 文件系统标志
    
    const struct inode_operations *i_op; // inode 操作
    struct super_block  *i_sb;          // 所属超级块
    struct address_space *i_mapping;    // 地址空间 (页缓存)
    
    unsigned long       i_ino;          // inode 号
    unsigned int        i_nlink;        // 硬链接数
    dev_t               i_rdev;         // 设备号 (字符/块设备)
    loff_t              i_size;         // 文件大小
    
    struct timespec     i_atime;        // 访问时间
    struct timespec     i_mtime;        // 修改时间
    struct timespec     i_ctime;        // 状态改变时间
    
    blkcnt_t            i_blocks;       // 块数
    
    const struct file_operations *i_fop; // 默认文件操作
    
    struct list_head    i_dentry;       // 关联的目录项
    union {
        struct hlist_head i_dentry;
        struct rcu_head   i_rcu;
    };
    
    union {
        struct pipe_inode_info *i_pipe;  // 管道
        struct block_device    *i_bdev;  // 块设备
        struct cdev            *i_cdev;  // 字符设备
    };
    // ...
};
```

### 3. struct dentry - 目录项

dentry 是路径名到 inode 的映射，构成目录树结构。

```c
// include/linux/dcache.h
struct dentry {
    unsigned int d_flags;               // 标志
    seqcount_t d_seq;                   // 序列锁
    struct hlist_bl_node d_hash;        // 哈希表节点
    struct dentry *d_parent;            // 父目录项
    struct qstr d_name;                 // 名称
    struct inode *d_inode;              // 关联的 inode
    
    unsigned char d_iname[DNAME_INLINE_LEN]; // 短名称内联存储
    
    struct lockref d_lockref;           // 引用计数和锁
    const struct dentry_operations *d_op; // 目录项操作
    struct super_block *d_sb;           // 超级块
    unsigned long d_time;               // 有效时间
    void *d_fsdata;                     // 文件系统私有数据
    
    struct list_head d_lru;             // LRU 链表
    struct list_head d_child;           // 子目录项链表节点
    struct list_head d_subdirs;         // 子目录列表
    
    union {
        struct hlist_node d_alias;      // inode 的别名链表
        struct rcu_head d_rcu;
    } d_u;
};
```

### 4. struct file - 文件对象

file 代表进程打开的一个文件实例。

```c
// include/linux/fs.h
struct file {
    struct path         f_path;         // 文件路径 (vfsmount + dentry)
    const struct file_operations *f_op; // 文件操作
    
    spinlock_t          f_lock;
    atomic_long_t       f_count;        // 引用计数
    unsigned int        f_flags;        // 打开标志 (O_RDONLY, O_NONBLOCK 等)
    fmode_t             f_mode;         // 访问模式
    loff_t              f_pos;          // 当前读写位置
    
    struct fown_struct  f_owner;        // 异步通知信息
    const struct cred   *f_cred;        // 凭证
    struct file_ra_state f_ra;          // 预读状态
    
    struct address_space *f_mapping;    // 页缓存映射
    void                *private_data;  // 驱动私有数据
    // ...
};
```

### 5. 操作函数表

```c
// 超级块操作
struct super_operations {
    struct inode *(*alloc_inode)(struct super_block *sb);
    void (*destroy_inode)(struct inode *);
    void (*dirty_inode)(struct inode *, int flags);
    int (*write_inode)(struct inode *, struct writeback_control *wbc);
    int (*drop_inode)(struct inode *);
    void (*evict_inode)(struct inode *);
    void (*put_super)(struct super_block *);
    int (*sync_fs)(struct super_block *sb, int wait);
    int (*statfs)(struct dentry *, struct kstatfs *);
    int (*remount_fs)(struct super_block *, int *, char *);
    // ...
};

// inode 操作
struct inode_operations {
    struct dentry * (*lookup)(struct inode *, struct dentry *, struct nameidata *);
    int (*create)(struct inode *, struct dentry *, int, struct nameidata *);
    int (*link)(struct dentry *, struct inode *, struct dentry *);
    int (*unlink)(struct inode *, struct dentry *);
    int (*symlink)(struct inode *, struct dentry *, const char *);
    int (*mkdir)(struct inode *, struct dentry *, int);
    int (*rmdir)(struct inode *, struct dentry *);
    int (*rename)(struct inode *, struct dentry *, struct inode *, struct dentry *);
    int (*permission)(struct inode *, int);
    // ...
};

// 文件操作
struct file_operations {
    struct module *owner;
    loff_t (*llseek)(struct file *, loff_t, int);
    ssize_t (*read)(struct file *, char __user *, size_t, loff_t *);
    ssize_t (*write)(struct file *, const char __user *, size_t, loff_t *);
    int (*open)(struct inode *, struct file *);
    int (*release)(struct inode *, struct file *);
    int (*mmap)(struct file *, struct vm_area_struct *);
    int (*fsync)(struct file *, loff_t, loff_t, int datasync);
    unsigned int (*poll)(struct file *, struct poll_table_struct *);
    long (*unlocked_ioctl)(struct file *, unsigned int, unsigned long);
    // ...
};
```

### 数据结构关系图

```
                    进程打开文件表
                         │
                         ▼
┌─────────────────────────────────────────────────────────────────────┐
│                        struct file                                   │
│                                                                      │
│  f_path ─────────────────────┐                                      │
│  f_op ────────────┐          │                                      │
│  f_pos            │          │                                      │
│  private_data     │          │                                      │
└───────────────────┼──────────┼──────────────────────────────────────┘
                    │          │
                    ▼          ▼
           ┌───────────────────────────────────────┐
           │            struct path                │
           │                                       │
           │  mnt ──► struct vfsmount             │
           │  dentry ─┐                           │
           └──────────┼───────────────────────────┘
                      │
                      ▼
┌─────────────────────────────────────────────────────────────────────┐
│                        struct dentry                                 │
│                                                                      │
│  d_name = "myfile.txt"                                              │
│  d_parent ────────────────────┐                                     │
│  d_inode ─────┐               │                                     │
│  d_sb ────────┼───────────┐   │                                     │
│  d_subdirs    │           │   │                                     │
└───────────────┼───────────┼───┼─────────────────────────────────────┘
                │           │   │
                │           │   └──► 父 dentry ──► ... ──► 根 dentry
                │           │
                ▼           ▼
┌──────────────────────┐   ┌──────────────────────────────────────────┐
│    struct inode      │   │           struct super_block             │
│                      │   │                                          │
│  i_mode = -rw-r--r-- │   │  s_root ──► 根 dentry                   │
│  i_size = 1234       │   │  s_type ──► file_system_type            │
│  i_ino = 12345       │   │  s_op ───► super_operations             │
│  i_op ──► inode_ops  │   │  s_bdev ──► block_device                │
│  i_fop ─► file_ops   │   │  s_fs_info                              │
│  i_sb ───────────────┼───┤                                          │
│  i_mapping           │   │                                          │
└──────────────────────┘   └──────────────────────────────────────────┘
```

---

## 文件系统注册与挂载

### 文件系统类型注册

```c
// include/linux/fs.h
struct file_system_type {
    const char *name;                    // 文件系统名称 ("ext4", "xfs" 等)
    int fs_flags;                        // 标志
    struct dentry *(*mount)(struct file_system_type *, int,
                            const char *, void *);  // 挂载函数
    void (*kill_sb)(struct super_block *);          // 卸载函数
    struct module *owner;                // 所属模块
    struct file_system_type *next;       // 链表指针
    struct hlist_head fs_supers;         // 该类型的所有超级块
    // ...
};

// 注册文件系统
int register_filesystem(struct file_system_type *fs);

// 注销文件系统
int unregister_filesystem(struct file_system_type *fs);
```

### 挂载流程

```
mount("/dev/sda1", "/mnt", "ext4", 0, NULL)
                │
                ▼
        sys_mount()
                │
                ▼
        do_mount()
                │
                ├── 路径解析: path_lookup("/mnt")
                │
                ├── 查找文件系统类型: get_fs_type("ext4")
                │
                └── vfs_kern_mount()
                        │
                        ▼
                ┌───────────────────────────────────────┐
                │  file_system_type->mount()            │
                │  (ext4_mount)                         │
                │                                       │
                │  1. 分配 super_block                  │
                │  2. 读取磁盘超级块                     │
                │  3. 初始化 s_op                       │
                │  4. 创建根 inode                      │
                │  5. 创建根 dentry                     │
                │  6. 设置 s_root                       │
                │                                       │
                │  返回根 dentry                        │
                └───────────────────────────────────────┘
                        │
                        ▼
                do_add_mount()
                        │
                        ▼
                关联到挂载点
```

---

## 文件操作流程

### open() 系统调用

```
open("/mnt/myfile.txt", O_RDWR)
            │
            ▼
      sys_open()
            │
            ▼
      do_sys_open()
            │
            ├── 分配文件描述符 fd
            │
            └── do_filp_open()
                    │
                    ▼
              path_openat()
                    │
                    ├─────────────────────────────────────────┐
                    │         路径解析 (path_walk)            │
                    │                                         │
                    │  "/" ─► root_dentry                     │
                    │   ▼                                     │
                    │  "mnt" ─► dentry_lookup ─► dentry      │
                    │   ▼        (查 dcache 或调用 lookup)    │
                    │  "myfile.txt" ─► dentry_lookup ─► dentry
                    │                                         │
                    └─────────────────────────────────────────┘
                    │
                    ▼
              do_last()
                    │
                    ├── 获取 inode
                    │
                    └── __dentry_open()
                            │
                            ├── 分配 struct file
                            │
                            ├── f->f_op = inode->i_fop
                            │
                            └── f->f_op->open(inode, file)
                                    │
                                    ▼
                              具体文件系统的 open
                    │
                    ▼
            返回文件描述符 fd
```

### read() 系统调用

```
read(fd, buf, count)
        │
        ▼
  sys_read()
        │
        ▼
  vfs_read()
        │
        ├── 获取 struct file (从 fd)
        │
        └── file->f_op->read(file, buf, count, &pos)
                │
                ├─────────────────────────────────────────┐
                │    通用文件读取 (do_sync_read)          │
                │            或                           │
                │    具体文件系统实现                      │
                └─────────────────────────────────────────┘
                        │
                        ▼
                generic_file_aio_read()
                        │
                        ▼
                do_generic_file_read()
                        │
                        ├── 检查页缓存
                        │
                        ├── 如果命中: 直接从缓存读取
                        │
                        └── 如果未命中:
                                │
                                ├── 分配页面
                                │
                                ├── a_ops->readpage()
                                │   (触发实际 I/O)
                                │
                                └── 等待 I/O 完成
                        │
                        ▼
                copy_to_user(buf, page_data, count)
```

### write() 系统调用

```
write(fd, buf, count)
        │
        ▼
  sys_write()
        │
        ▼
  vfs_write()
        │
        └── file->f_op->write(file, buf, count, &pos)
                │
                ▼
        generic_file_aio_write()
                │
                ▼
        __generic_file_aio_write()
                │
                ├── 查找或分配页面
                │
                ├── copy_from_user(page_data, buf, count)
                │
                ├── 标记页面为脏 (set_page_dirty)
                │
                └── 返回 (延迟写回)
                        │
                        ▼
              后台: pdflush/writeback 线程
                        │
                        ▼
              a_ops->writepage() 或 writepages()
                        │
                        ▼
                  实际 I/O 写入磁盘
```

---

## 页缓存与写回机制

### 页缓存结构

```c
// include/linux/fs.h
struct address_space {
    struct inode        *host;          // 所属 inode
    struct radix_tree_root page_tree;   // 基数树存储页面
    spinlock_t          tree_lock;
    unsigned int        i_mmap_writable; // 可写映射计数
    struct prio_tree_root i_mmap;       // 映射到此文件的 VMA
    struct list_head    i_mmap_nonlinear;
    struct mutex        i_mmap_mutex;
    unsigned long       nrpages;        // 页面数量
    pgoff_t             writeback_index; // 写回起始位置
    const struct address_space_operations *a_ops; // 地址空间操作
    unsigned long       flags;
    struct backing_dev_info *backing_dev_info;
    // ...
};

struct address_space_operations {
    int (*writepage)(struct page *page, struct writeback_control *wbc);
    int (*readpage)(struct file *, struct page *);
    int (*writepages)(struct address_space *, struct writeback_control *);
    int (*set_page_dirty)(struct page *page);
    int (*readpages)(struct file *, struct address_space *,
                     struct list_head *, unsigned);
    int (*write_begin)(struct file *, struct address_space *mapping,
                       loff_t pos, unsigned len, unsigned flags,
                       struct page **pagep, void **fsdata);
    int (*write_end)(struct file *, struct address_space *mapping,
                     loff_t pos, unsigned len, unsigned copied,
                     struct page *page, void *fsdata);
    // ...
};
```

### 页缓存工作原理

```
┌─────────────────────────────────────────────────────────────────────┐
│                         Page Cache                                   │
│                                                                      │
│   struct address_space                                               │
│          │                                                           │
│          ▼                                                           │
│   ┌─────────────────────────────────────────────────────────────┐   │
│   │              Radix Tree (page_tree)                          │   │
│   │                                                              │   │
│   │         index=0    index=1    index=2    index=3            │   │
│   │            │          │          │          │                │   │
│   │            ▼          ▼          ▼          ▼                │   │
│   │        ┌──────┐  ┌──────┐  ┌──────┐  ┌──────┐               │   │
│   │        │ Page │  │ Page │  │ Page │  │ Page │               │   │
│   │        │ 4KB  │  │ 4KB  │  │ 4KB  │  │ 4KB  │               │   │
│   │        └──────┘  └──────┘  └──────┘  └──────┘               │   │
│   │           ↕          ↕          ↕          ↕                 │   │
│   │         文件偏移 0-4095  4096-8191  8192-12287  12288-16383   │   │
│   └─────────────────────────────────────────────────────────────┘   │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
                              │
                              ▼
                    ┌─────────────────┐
                    │  Page 状态标志   │
                    │                 │
                    │  PG_uptodate    │ ← 数据有效
                    │  PG_dirty       │ ← 需要写回
                    │  PG_locked      │ ← 正在 I/O
                    │  PG_writeback   │ ← 正在写回
                    └─────────────────┘
```

### 写回机制

```
┌─────────────────────────────────────────────────────────────────────┐
│                        写回触发条件                                   │
│                                                                      │
│  1. 脏页数量超过阈值 (dirty_ratio)                                   │
│  2. 脏页存在时间过长 (dirty_expire_interval)                         │
│  3. 显式调用 sync/fsync                                              │
│  4. 内存压力 (页面回收)                                               │
│  5. 文件系统卸载                                                      │
└───────────────────────────────────┬─────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────┐
│                      Writeback 线程                                  │
│                                                                      │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │  bdi_writeback                                               │   │
│  │                                                              │   │
│  │  for_each_dirty_inode:                                       │   │
│  │      writeback_single_inode(inode)                           │   │
│  │          │                                                   │   │
│  │          ├── do_writepages(mapping)                          │   │
│  │          │       │                                           │   │
│  │          │       └── a_ops->writepages()                     │   │
│  │          │               或 a_ops->writepage()               │   │
│  │          │                                                   │   │
│  │          └── 清除 inode 脏标志                                │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
                              Block Layer
                                    │
                                    ▼
                              磁盘设备
```

---

## 关键源码文件

### VFS 核心

| 文件 | 功能 |
|------|------|
| `fs/inode.c` | inode 管理，alloc_inode, iget, iput |
| `fs/dcache.c` | dentry 缓存，路径查找 |
| `fs/namei.c` | 路径名解析，lookup, create |
| `fs/open.c` | open, close 系统调用 |
| `fs/read_write.c` | read, write 系统调用 |
| `fs/file.c` | file 对象管理 |
| `fs/file_table.c` | 文件表管理 |
| `fs/super.c` | 超级块管理，mount, umount |
| `fs/namespace.c` | 挂载命名空间 |
| `fs/filesystems.c` | 文件系统注册 |
| `fs/buffer.c` | 缓冲区头管理 |
| `fs/bio.c` | Block I/O |
| `fs/direct-io.c` | 直接 I/O |
| `fs/mpage.c` | 多页 I/O |

### 页缓存与写回

| 文件 | 功能 |
|------|------|
| `mm/filemap.c` | 页缓存核心 |
| `mm/page-writeback.c` | 脏页写回 |
| `mm/readahead.c` | 预读 |
| `mm/truncate.c` | 页面截断 |
| `fs/fs-writeback.c` | 文件系统写回 |

### 具体文件系统示例 (ext4)

| 文件 | 功能 |
|------|------|
| `fs/ext4/super.c` | 超级块操作 |
| `fs/ext4/inode.c` | inode 操作 |
| `fs/ext4/namei.c` | 目录操作 |
| `fs/ext4/file.c` | 文件操作 |
| `fs/ext4/dir.c` | 目录读取 |
| `fs/ext4/extents.c` | extent 管理 |

---

## 总结

### VFS 核心机制

1. **四大对象**: super_block, inode, dentry, file
2. **操作函数表**: 通过函数指针实现多态
3. **缓存机制**: dentry cache, inode cache, page cache
4. **统一接口**: 所有文件系统实现相同的 VFS 接口

### 设计优点

1. **抽象统一**: 用户程序无需关心底层文件系统类型
2. **高效缓存**: 多级缓存减少磁盘 I/O
3. **可扩展**: 轻松添加新文件系统
4. **延迟写回**: 提高写入性能

---

*本文档基于 Linux 3.2 内核源码分析*

