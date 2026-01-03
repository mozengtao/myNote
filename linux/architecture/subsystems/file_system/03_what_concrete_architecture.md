# WHAT｜具体架构

## 1. 模式：操作表多态

```
OPS-TABLE POLYMORPHISM PATTERN
+=============================================================================+
|                                                                              |
|  THE FUNDAMENTAL VFS DESIGN PATTERN                                          |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │ VFS defines INTERFACES (structs of function pointers):          │    │ |
|  │  │                                                                  │    │ |
|  │  │  struct file_operations {                                        │    │ |
|  │  │      loff_t (*llseek)(struct file *, loff_t, int);               │    │ |
|  │  │      ssize_t (*read)(struct file *, char __user *, size_t, loff_t *);│  │ |
|  │  │      ssize_t (*write)(struct file *, const char __user *, size_t, loff_t *);│ |
|  │  │      int (*open)(struct inode *, struct file *);                 │    │ |
|  │  │      int (*release)(struct inode *, struct file *);              │    │ |
|  │  │      int (*fsync)(struct file *, loff_t, loff_t, int);           │    │ |
|  │  │      int (*mmap)(struct file *, struct vm_area_struct *);        │    │ |
|  │  │      ...                                                         │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  struct inode_operations {                                       │    │ |
|  │  │      struct dentry *(*lookup)(struct inode *, struct dentry *, unsigned int);│ |
|  │  │      int (*create)(struct inode *, struct dentry *, umode_t, bool);│  │ |
|  │  │      int (*link)(struct dentry *, struct inode *, struct dentry *);│  │ |
|  │  │      int (*unlink)(struct inode *, struct dentry *);             │    │ |
|  │  │      int (*mkdir)(struct inode *, struct dentry *, umode_t);     │    │ |
|  │  │      int (*rmdir)(struct inode *, struct dentry *);              │    │ |
|  │  │      int (*rename)(struct inode *, struct dentry *, struct inode *, struct dentry *);│ |
|  │  │      ...                                                         │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  struct super_operations {                                       │    │ |
|  │  │      struct inode *(*alloc_inode)(struct super_block *);         │    │ |
|  │  │      void (*destroy_inode)(struct inode *);                      │    │ |
|  │  │      void (*dirty_inode)(struct inode *, int);                   │    │ |
|  │  │      int (*write_inode)(struct inode *, struct writeback_control *);│ │ |
|  │  │      int (*sync_fs)(struct super_block *, int);                  │    │ |
|  │  │      ...                                                         │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  FILESYSTEM IMPLEMENTATIONS                                                  |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  EXT4:                                                                   │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │ const struct file_operations ext4_file_operations = {           │    │ |
|  │  │     .llseek     = ext4_llseek,                                   │    │ |
|  │  │     .read       = do_sync_read,          // generic + page cache│    │ |
|  │  │     .write      = do_sync_write,                                 │    │ |
|  │  │     .aio_read   = generic_file_aio_read,                         │    │ |
|  │  │     .aio_write  = ext4_file_write,       // ext4-specific       │    │ |
|  │  │     .open       = ext4_file_open,                                │    │ |
|  │  │     .release    = ext4_release_file,                             │    │ |
|  │  │     .fsync      = ext4_sync_file,        // journal-aware       │    │ |
|  │  │     .mmap       = ext4_file_mmap,                                │    │ |
|  │  │ };                                                               │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  NFS:                                                                    │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │ const struct file_operations nfs_file_operations = {            │    │ |
|  │  │     .llseek     = nfs_file_llseek,                               │    │ |
|  │  │     .read       = do_sync_read,                                  │    │ |
|  │  │     .write      = do_sync_write,                                 │    │ |
|  │  │     .aio_read   = nfs_file_read,         // RPC-based           │    │ |
|  │  │     .aio_write  = nfs_file_write,        // RPC-based           │    │ |
|  │  │     .open       = nfs_file_open,         // stateful open       │    │ |
|  │  │     .release    = nfs_file_release,                              │    │ |
|  │  │     .fsync      = nfs_file_fsync,        // commit to server    │    │ |
|  │  │     .mmap       = nfs_file_mmap,                                 │    │ |
|  │  │ };                                                               │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  PROCFS:                                                                 │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │ const struct file_operations proc_pid_stat_operations = {       │    │ |
|  │  │     .open       = pid_stat_open,         // generate on open    │    │ |
|  │  │     .read       = seq_read,              // sequential read     │    │ |
|  │  │     .llseek     = seq_lseek,                                     │    │ |
|  │  │     .release    = single_release,                                │    │ |
|  │  │     /* no write - read only */                                   │    │ |
|  │  │ };                                                               │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**操作表多态模式**：

VFS 定义接口（函数指针结构体）：
- **`file_operations`**：文件操作（read/write/open/close/mmap）
- **`inode_operations`**：inode 操作（lookup/create/unlink/mkdir/rename）
- **`super_operations`**：超级块操作（alloc_inode/write_inode/sync_fs）

每个文件系统提供自己的实现：
- **ext4**：使用 page cache 和日志
- **NFS**：基于 RPC 调用
- **procfs**：读取时生成，只读

---

## 2. 核心数据结构

```
CORE DATA STRUCTURES
+=============================================================================+
|                                                                              |
|  STRUCT INODE (include/linux/fs.h)                                           |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  struct inode {                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  /* Identity */                                                  │    │ |
|  │  │  umode_t          i_mode;      // File type + permissions        │    │ |
|  │  │  uid_t            i_uid;       // Owner user ID                  │    │ |
|  │  │  gid_t            i_gid;       // Owner group ID                 │    │ |
|  │  │  unsigned long    i_ino;       // Inode number                   │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* Size and allocation */                                       │    │ |
|  │  │  loff_t           i_size;      // File size in bytes             │    │ |
|  │  │  blkcnt_t         i_blocks;    // Blocks allocated               │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* Timestamps */                                                │    │ |
|  │  │  struct timespec  i_atime;     // Access time                    │    │ |
|  │  │  struct timespec  i_mtime;     // Modification time              │    │ |
|  │  │  struct timespec  i_ctime;     // Change time (metadata)         │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* Reference counting */                                        │    │ |
|  │  │  atomic_t         i_count;     // In-memory references           │    │ |
|  │  │  unsigned int     i_nlink;     // Hard link count                │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* VFS linkage */                                               │    │ |
|  │  │  struct super_block *i_sb;     // Owning superblock              │    │ |
|  │  │  struct address_space *i_mapping; // Page cache mapping          │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* Operations tables (POLYMORPHISM) */                          │    │ |
|  │  │  const struct inode_operations *i_op;   // inode ops             │    │ |
|  │  │  const struct file_operations  *i_fop;  // default file ops      │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* Locking */                                                   │    │ |
|  │  │  struct mutex     i_mutex;     // Protects directory ops         │    │ |
|  │  │  struct rw_semaphore i_alloc_sem; // Allocation lock             │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* Filesystem-specific data */                                  │    │ |
|  │  │  void            *i_private;   // FS-specific pointer            │    │ |
|  │  │  union { ... } i_data;         // Embedded address_space         │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │  };                                                                      │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**struct inode**：

- **身份**：`i_mode`（类型+权限）、`i_uid/i_gid`（所有者）、`i_ino`（inode 号）
- **大小**：`i_size`（字节数）、`i_blocks`（分配的块）
- **时间戳**：`i_atime/i_mtime/i_ctime`
- **引用计数**：`i_count`（内存引用）、`i_nlink`（硬链接数）
- **VFS 链接**：`i_sb`（超级块）、`i_mapping`（page cache 映射）
- **操作表**：`i_op`（inode ops）、`i_fop`（file ops）—— 多态关键
- **锁**：`i_mutex`（保护目录操作）

---

```
STRUCT FILE (include/linux/fs.h)
+=============================================================================+
|                                                                              |
|  struct file {                                                               |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  /* Linkage */                                                           │ |
|  │  struct path         f_path;      // dentry + vfsmount                   │ |
|  │  struct inode       *f_inode;     // Cached inode pointer                │ |
|  │                                                                          │ |
|  │  /* Operations (POLYMORPHISM) */                                         │ |
|  │  const struct file_operations *f_op;  // File operations                 │ |
|  │                                                                          │ |
|  │  /* State */                                                             │ |
|  │  unsigned int        f_flags;     // O_RDONLY, O_WRONLY, etc.            │ |
|  │  fmode_t             f_mode;      // FMODE_READ, FMODE_WRITE             │ |
|  │  loff_t              f_pos;       // Current file position               │ |
|  │                                                                          │ |
|  │  /* Reference counting */                                                │ |
|  │  atomic_long_t       f_count;     // Reference count                     │ |
|  │                                                                          │ |
|  │  /* Ownership */                                                         │ |
|  │  struct fown_struct  f_owner;     // For async I/O                       │ |
|  │  const struct cred  *f_cred;      // Credentials at open time            │ |
|  │                                                                          │ |
|  │  /* Filesystem-specific */                                               │ |
|  │  void               *private_data; // FS driver data                     │ |
|  │                                                                          │ |
|  │  /* Mapping */                                                           │ |
|  │  struct address_space *f_mapping; // Page cache                          │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|  };                                                                          |
|                                                                              |
|  FILE VS INODE:                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌────────────────┐     ┌────────────────┐     ┌────────────────┐       │ |
|  │  │ struct file A  │────►│                │◄────│ struct file B  │       │ |
|  │  │ (fd=3, proc 1) │     │  struct inode  │     │ (fd=5, proc 2) │       │ |
|  │  │ f_pos = 100    │     │                │     │ f_pos = 0      │       │ |
|  │  │ f_flags=O_RDWR │     │  i_size=4096   │     │ f_flags=O_RDONLY│      │ |
|  │  └────────────────┘     │  i_mode=0644   │     └────────────────┘       │ |
|  │                          │  i_nlink=1     │                              │ |
|  │  Multiple open files    └────────────────┘    Different positions,      │ |
|  │  can share one inode                          different flags            │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**struct file**：

- **链接**：`f_path`（dentry + vfsmount）、`f_inode`（inode 指针）
- **操作表**：`f_op`（file_operations）—— 多态关键
- **状态**：`f_flags`（O_RDONLY 等）、`f_mode`、`f_pos`（当前位置）
- **引用计数**：`f_count`
- **凭据**：`f_cred`（打开时的凭据）

**file vs inode**：
- 多个打开的文件可共享一个 inode
- 不同的 f_pos、不同的 f_flags

---

```
STRUCT SUPER_BLOCK (include/linux/fs.h)
+=============================================================================+
|                                                                              |
|  struct super_block {                                                        |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  /* Identity */                                                          │ |
|  │  struct list_head    s_list;      // Link in global sb list              │ |
|  │  dev_t               s_dev;       // Device identifier                   │ |
|  │  unsigned char       s_blocksize_bits;                                   │ |
|  │  unsigned long       s_blocksize; // Block size                          │ |
|  │  loff_t              s_maxbytes;  // Max file size                       │ |
|  │  struct file_system_type *s_type; // Filesystem type                     │ |
|  │                                                                          │ |
|  │  /* Operations (POLYMORPHISM) */                                         │ |
|  │  const struct super_operations *s_op;                                    │ |
|  │                                                                          │ |
|  │  /* Quota and export */                                                  │ |
|  │  const struct dquot_operations *dq_op;                                   │ |
|  │  const struct export_operations *s_export_op;                            │ |
|  │                                                                          │ |
|  │  /* Mount flags */                                                       │ |
|  │  unsigned long       s_flags;     // MS_RDONLY, MS_NOSUID, etc.          │ |
|  │                                                                          │ |
|  │  /* Root dentry */                                                       │ |
|  │  struct dentry      *s_root;      // Root of this filesystem             │ |
|  │                                                                          │ |
|  │  /* All inodes on this fs */                                             │ |
|  │  struct list_head    s_inodes;    // All inodes list                     │ |
|  │  struct list_head    s_dirty;     // Dirty inodes                        │ |
|  │                                                                          │ |
|  │  /* Locking */                                                           │ |
|  │  struct rw_semaphore s_umount;    // Unmount protection                  │ |
|  │  struct mutex        s_lock;      // Superblock lock                     │ |
|  │                                                                          │ |
|  │  /* Filesystem-specific */                                               │ |
|  │  void               *s_fs_info;   // FS-specific data                    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|  };                                                                          |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**struct super_block**：

- **身份**：`s_dev`（设备标识）、`s_blocksize`（块大小）、`s_type`（文件系统类型）
- **操作表**：`s_op`（super_operations）—— 多态关键
- **挂载标志**：`s_flags`（MS_RDONLY 等）
- **根 dentry**：`s_root`
- **inode 列表**：`s_inodes`（所有 inode）、`s_dirty`（脏 inode）
- **锁**：`s_umount`（卸载保护）、`s_lock`

---

## 3. 控制流：open/read/write 路径

```
CONTROL FLOW: OPEN PATH
+=============================================================================+
|                                                                              |
|  open("/home/user/file.txt", O_RDWR)                                         |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────┐                                                     │ |
|  │  │ sys_open()       │ ◄── System call entry                              │ |
|  │  └────────┬────────┘                                                     │ |
|  │           │                                                              │ |
|  │           ▼                                                              │ |
|  │  ┌─────────────────┐                                                     │ |
|  │  │ do_sys_open()    │                                                     │ |
|  │  │ • Get unused fd  │                                                     │ |
|  │  │ • Call do_filp_open()                                                 │ |
|  │  └────────┬────────┘                                                     │ |
|  │           │                                                              │ |
|  │           ▼                                                              │ |
|  │  ┌─────────────────┐                                                     │ |
|  │  │ do_filp_open()   │                                                     │ |
|  │  │ • Setup nameidata│                                                     │ |
|  │  │ • Call path_openat()                                                  │ |
|  │  └────────┬────────┘                                                     │ |
|  │           │                                                              │ |
|  │           ▼                                                              │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │ path_openat() - PATH RESOLUTION                                  │    │ |
|  │  │                                                                  │    │ |
|  │  │  "/home/user/file.txt"                                           │    │ |
|  │  │      │                                                           │    │ |
|  │  │      ▼                                                           │    │ |
|  │  │  ┌──────────────────────────────────────────────────────────┐   │    │ |
|  │  │  │ link_path_walk() - for each component:                    │   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  │  "home" → lookup_fast() → dcache hit? → dentry            │   │    │ |
|  │  │  │                 │                                         │   │    │ |
|  │  │  │                 └─ miss → lookup_slow()                   │   │    │ |
|  │  │  │                           └─ inode->i_op->lookup()        │   │    │ |
|  │  │  │                              (filesystem specific)        │   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  │  "user" → same process...                                 │   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  │  "file.txt" → lookup → get final dentry/inode             │   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  └──────────────────────────────────────────────────────────┘   │    │ |
|  │  │                                                                  │    │ |
|  │  └────────┬────────────────────────────────────────────────────────┘    │ |
|  │           │                                                              │ |
|  │           ▼                                                              │ |
|  │  ┌─────────────────┐                                                     │ |
|  │  │ do_last()        │                                                     │ |
|  │  │ • Permission check                                                    │ |
|  │  │ • Allocate struct file                                                │ |
|  │  │ • Call vfs_open()                                                     │ |
|  │  └────────┬────────┘                                                     │ |
|  │           │                                                              │ |
|  │           ▼                                                              │ |
|  │  ┌─────────────────┐                                                     │ |
|  │  │ vfs_open()       │                                                     │ |
|  │  │ • file->f_op = inode->i_fop                                           │ |
|  │  │ • Call f_op->open()  ◄── FILESYSTEM CALLBACK                          │ |
|  │  └────────┬────────┘                                                     │ |
|  │           │                                                              │ |
|  │           ▼                                                              │ |
|  │  ┌─────────────────┐                                                     │ |
|  │  │ fd_install()     │                                                     │ |
|  │  │ • Install file to fd table                                            │ |
|  │  │ • Return fd to userspace                                              │ |
|  │  └─────────────────┘                                                     │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**open 路径**：

1. `sys_open()` → `do_sys_open()` → `do_filp_open()` → `path_openat()`
2. **路径解析**：`link_path_walk()` 对每个组件执行：
   - `lookup_fast()`：查询 dcache
   - 未命中 → `lookup_slow()` → `inode->i_op->lookup()`（文件系统回调）
3. `do_last()`：权限检查，分配 struct file，调用 `vfs_open()`
4. `vfs_open()`：设置 `file->f_op`，调用 `f_op->open()`（文件系统回调）
5. `fd_install()`：安装到 fd 表，返回 fd

---

```
CONTROL FLOW: READ PATH
+=============================================================================+
|                                                                              |
|  read(fd, buf, count)                                                        |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────┐                                                     │ |
|  │  │ sys_read()       │ ◄── System call entry                              │ |
|  │  └────────┬────────┘                                                     │ |
|  │           │                                                              │ |
|  │           ▼                                                              │ |
|  │  ┌─────────────────┐                                                     │ |
|  │  │ fget_light(fd)   │ → struct file *                                     │ |
|  │  └────────┬────────┘                                                     │ |
|  │           │                                                              │ |
|  │           ▼                                                              │ |
|  │  ┌─────────────────┐                                                     │ |
|  │  │ vfs_read()       │                                                     │ |
|  │  │ • rw_verify_area() - permission check                                 │ |
|  │  │ • Dispatch to filesystem                                              │ |
|  │  └────────┬────────┘                                                     │ |
|  │           │                                                              │ |
|  │    ┌──────┴──────────────────────────────┐                               │ |
|  │    │                                      │                               │ |
|  │    ▼ if f_op->read                        ▼ if f_op->aio_read             │ |
|  │  ┌─────────────────┐                ┌─────────────────┐                  │ |
|  │  │ f_op->read()     │                │ do_sync_read()   │                  │ |
|  │  │ (direct FS call) │                │ • Setup kiocb    │                  │ |
|  │  │                  │                │ • f_op->aio_read()                 │ |
|  │  └─────────────────┘                └────────┬────────┘                  │ |
|  │                                              │                            │ |
|  │                                              ▼                            │ |
|  │                                   ┌─────────────────────────────────┐    │ |
|  │                                   │ generic_file_aio_read()          │    │ |
|  │                                   │ (common for page-cache-based FS) │    │ |
|  │                                   │                                  │    │ |
|  │                                   │  ┌───────────────────────────┐  │    │ |
|  │                                   │  │ do_generic_file_read()     │  │    │ |
|  │                                   │  │                            │  │    │ |
|  │                                   │  │  for each page:            │  │    │ |
|  │                                   │  │   find_get_page()          │  │    │ |
|  │                                   │  │   • In page cache? copy    │  │    │ |
|  │                                   │  │   • Not in cache?          │  │    │ |
|  │                                   │  │     page_cache_read()      │  │    │ |
|  │                                   │  │     └─ readpage()          │  │    │ |
|  │                                   │  │        (FS callback)       │  │    │ |
|  │                                   │  │                            │  │    │ |
|  │                                   │  └───────────────────────────┘  │    │ |
|  │                                   │                                  │    │ |
|  │                                   │  copy_page_to_iter()             │    │ |
|  │                                   │  (copy to userspace buffer)      │    │ |
|  │                                   │                                  │    │ |
|  │                                   └─────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**read 路径**：

1. `sys_read()` → `fget_light(fd)` 获取 struct file
2. `vfs_read()` 权限检查，分发到文件系统
3. 如果 `f_op->read` 存在：直接调用
4. 如果 `f_op->aio_read` 存在：`do_sync_read()` → `generic_file_aio_read()`
5. **Page cache 逻辑**：
   - `find_get_page()`：在 page cache 中？直接复制
   - 不在？`page_cache_read()` → `readpage()`（文件系统回调）
6. `copy_page_to_iter()`：复制到用户空间缓冲区

---

```
CONTROL FLOW: WRITE PATH
+=============================================================================+
|                                                                              |
|  write(fd, buf, count)                                                       |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────┐                                                     │ |
|  │  │ sys_write()      │ ◄── System call entry                              │ |
|  │  └────────┬────────┘                                                     │ |
|  │           │                                                              │ |
|  │           ▼                                                              │ |
|  │  ┌─────────────────┐                                                     │ |
|  │  │ vfs_write()      │                                                     │ |
|  │  │ • Permission check                                                    │ |
|  │  │ • Dispatch to filesystem                                              │ |
|  │  └────────┬────────┘                                                     │ |
|  │           │                                                              │ |
|  │           ▼                                                              │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │ generic_file_aio_write() (common path)                           │    │ |
|  │  │                                                                  │    │ |
|  │  │  1. Acquire i_mutex                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  2. __generic_file_aio_write()                                   │    │ |
|  │  │     │                                                            │    │ |
|  │  │     ▼                                                            │    │ |
|  │  │  ┌───────────────────────────────────────────────────────────┐  │    │ |
|  │  │  │ generic_file_buffered_write()                              │  │    │ |
|  │  │  │                                                            │  │    │ |
|  │  │  │  for each page:                                            │  │    │ |
|  │  │  │   grab_cache_page_write_begin()                            │  │    │ |
|  │  │  │   • Get or create page in cache                            │  │    │ |
|  │  │  │                                                            │  │    │ |
|  │  │  │   a_ops->write_begin()  ◄── FS prepare (allocate blocks)   │  │    │ |
|  │  │  │                                                            │  │    │ |
|  │  │  │   iov_iter_copy_from_user()                                │  │    │ |
|  │  │  │   • Copy from userspace to page                            │  │    │ |
|  │  │  │                                                            │  │    │ |
|  │  │  │   a_ops->write_end()    ◄── FS finish (mark dirty)         │  │    │ |
|  │  │  │   • Mark page dirty                                        │  │    │ |
|  │  │  │   • Update inode size                                      │  │    │ |
|  │  │  │                                                            │  │    │ |
|  │  │  └───────────────────────────────────────────────────────────┘  │    │ |
|  │  │                                                                  │    │ |
|  │  │  3. Release i_mutex                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  4. Return bytes written (data may not be on disk yet!)          │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  WRITEBACK (later, asynchronous):                                        │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  pdflush / bdi-flusher threads                                   │    │ |
|  │  │  │                                                               │    │ |
|  │  │  ▼                                                               │    │ |
|  │  │  writeback_inodes()                                              │    │ |
|  │  │  │                                                               │    │ |
|  │  │  ▼                                                               │    │ |
|  │  │  do_writepages()                                                 │    │ |
|  │  │  │                                                               │    │ |
|  │  │  ▼                                                               │    │ |
|  │  │  a_ops->writepages() or generic_writepages()                     │    │ |
|  │  │  │                                                               │    │ |
|  │  │  ▼                                                               │    │ |
|  │  │  submit_bio() → block layer → disk                               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**write 路径**：

1. `sys_write()` → `vfs_write()` 权限检查
2. `generic_file_aio_write()`：
   - 获取 `i_mutex`
   - `generic_file_buffered_write()`：
     - `grab_cache_page_write_begin()`：获取或创建 page cache 页
     - `a_ops->write_begin()`：文件系统准备（分配块）
     - `iov_iter_copy_from_user()`：从用户空间复制到页
     - `a_ops->write_end()`：文件系统完成（标记脏）
   - 释放 `i_mutex`
   - 返回（数据可能还没在磁盘上！）

3. **写回（异步）**：
   - pdflush / bdi-flusher 线程
   - `writeback_inodes()` → `do_writepages()` → `a_ops->writepages()`
   - `submit_bio()` → 块层 → 磁盘

---

## 4. 扩展点：新文件系统

```
EXTENSION POINTS: ADDING NEW FILESYSTEM
+=============================================================================+
|                                                                              |
|  REGISTERING A NEW FILESYSTEM                                                |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Step 1: Define file_system_type                                         │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  static struct file_system_type myfs_type = {                    │    │ |
|  │  │      .owner     = THIS_MODULE,                                   │    │ |
|  │  │      .name      = "myfs",                                        │    │ |
|  │  │      .mount     = myfs_mount,       // Called on mount           │    │ |
|  │  │      .kill_sb   = kill_block_super, // Called on umount          │    │ |
|  │  │      .fs_flags  = FS_REQUIRES_DEV,  // Needs block device        │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  Step 2: Implement mount callback                                        │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  static struct dentry *myfs_mount(struct file_system_type *type, │    │ |
|  │  │                                   int flags, const char *dev,    │    │ |
|  │  │                                   void *data)                    │    │ |
|  │  │  {                                                               │    │ |
|  │  │      return mount_bdev(type, flags, dev, data, myfs_fill_super); │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  static int myfs_fill_super(struct super_block *sb, void *data,  │    │ |
|  │  │                             int silent)                          │    │ |
|  │  │  {                                                               │    │ |
|  │  │      sb->s_op = &myfs_super_operations;  // Set super ops        │    │ |
|  │  │      sb->s_root = d_make_root(myfs_get_root_inode(sb));          │    │ |
|  │  │      return 0;                                                   │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  Step 3: Define operations tables                                        │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  static const struct super_operations myfs_super_operations = {  │    │ |
|  │  │      .alloc_inode   = myfs_alloc_inode,                          │    │ |
|  │  │      .destroy_inode = myfs_destroy_inode,                        │    │ |
|  │  │      .write_inode   = myfs_write_inode,                          │    │ |
|  │  │      .sync_fs       = myfs_sync_fs,                              │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  static const struct inode_operations myfs_dir_inode_operations = {│  │ |
|  │  │      .lookup = myfs_lookup,                                      │    │ |
|  │  │      .create = myfs_create,                                      │    │ |
|  │  │      .unlink = myfs_unlink,                                      │    │ |
|  │  │      .mkdir  = myfs_mkdir,                                       │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  static const struct file_operations myfs_file_operations = {    │    │ |
|  │  │      .read     = do_sync_read,                                   │    │ |
|  │  │      .write    = do_sync_write,                                  │    │ |
|  │  │      .aio_read = generic_file_aio_read,                          │    │ |
|  │  │      .aio_write= myfs_file_write,                                │    │ |
|  │  │      .mmap     = generic_file_mmap,                              │    │ |
|  │  │      .fsync    = myfs_file_fsync,                                │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  Step 4: Register on module load                                         │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  static int __init myfs_init(void)                               │    │ |
|  │  │  {                                                               │    │ |
|  │  │      return register_filesystem(&myfs_type);                     │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  static void __exit myfs_exit(void)                              │    │ |
|  │  │  {                                                               │    │ |
|  │  │      unregister_filesystem(&myfs_type);                          │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  module_init(myfs_init);                                         │    │ |
|  │  │  module_exit(myfs_exit);                                         │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**添加新文件系统的步骤**：

1. **定义 `file_system_type`**：指定名称、mount/kill_sb 回调
2. **实现 mount 回调**：调用 `mount_bdev()` 并提供 `fill_super()` 函数
3. **定义操作表**：
   - `super_operations`：alloc_inode/write_inode/sync_fs
   - `inode_operations`：lookup/create/unlink/mkdir
   - `file_operations`：read/write/mmap/fsync
4. **注册**：模块加载时调用 `register_filesystem()`

---

## 5. 代价：缓存一致性和间接调用

```
COSTS: CACHE COHERENCY
+=============================================================================+
|                                                                              |
|  CACHE INVALIDATION CHALLENGES                                               |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Problem 1: Page cache vs disk                                           │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Process A: write(fd, data, 100);  // Data in page cache        │    │ |
|  │  │  Process B: read(fd, buf, 100);    // Reads from page cache     │    │ |
|  │  │                                                                  │    │ |
|  │  │  ✓ Coherent within single system                                 │    │ |
|  │  │                                                                  │    │ |
|  │  │  BUT:                                                            │    │ |
|  │  │  Process A: write(fd, data, 100);                                │    │ |
|  │  │  [CRASH]                                                         │    │ |
|  │  │  → Data lost! Was only in page cache, not on disk                │    │ |
|  │  │                                                                  │    │ |
|  │  │  Solution: fsync() for durability guarantee                      │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  Problem 2: NFS cache coherency                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Client A                     Server                  Client B   │    │ |
|  │  │  ┌──────────┐                ┌──────────┐            ┌──────────┐│    │ |
|  │  │  │ Cache:   │                │ File:    │            │ Cache:   ││    │ |
|  │  │  │ "old"    │                │ "old"    │            │ "old"    ││    │ |
|  │  │  └──────────┘                └──────────┘            └──────────┘│    │ |
|  │  │       │                            ▲                       │     │    │ |
|  │  │       │ write("new")               │                       │     │    │ |
|  │  │       └────────────────────────────┘                       │     │    │ |
|  │  │                                                            │     │    │ |
|  │  │                              ┌──────────┐                  │     │    │ |
|  │  │                              │ File:    │                  │     │    │ |
|  │  │                              │ "new"    │◄─────────────────┘     │    │ |
|  │  │                              └──────────┘  Client B reads        │    │ |
|  │  │                                            stale cache!          │    │ |
|  │  │                                                                  │    │ |
|  │  │  Solutions:                                                      │    │ |
|  │  │  • Close-to-open consistency (NFS default)                       │    │ |
|  │  │  • Attribute caching with timeout                                │    │ |
|  │  │  • Delegations (NFSv4)                                           │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  Problem 3: Dentry cache invalidation                                    │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Local filesystem: No problem, dcache updated atomically         │    │ |
|  │  │                                                                  │    │ |
|  │  │  Network filesystem:                                             │    │ |
|  │  │  • Client caches dentry for "/mnt/file"                          │    │ |
|  │  │  • Another client deletes file on server                         │    │ |
|  │  │  • First client's dentry is stale                                │    │ |
|  │  │                                                                  │    │ |
|  │  │  Solution: d_revalidate() callback                               │    │ |
|  │  │  • Called before using cached dentry                             │    │ |
|  │  │  • NFS checks with server if dentry still valid                  │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**缓存一致性代价**：

**问题 1：Page cache vs 磁盘**
- 同一系统内一致（进程 A 写，进程 B 读 page cache）
- 崩溃时数据丢失！解决方案：`fsync()` 保证持久性

**问题 2：NFS 缓存一致性**
- 客户端 A 写入服务器，客户端 B 可能读取过时缓存
- 解决方案：close-to-open 一致性、属性缓存超时、delegations（NFSv4）

**问题 3：Dentry 缓存失效**
- 本地文件系统：无问题，dcache 原子更新
- 网络文件系统：其他客户端删除文件，本地 dentry 过时
- 解决方案：`d_revalidate()` 回调，使用前检查有效性

---

```
COSTS: INDIRECTION OVERHEAD
+=============================================================================+
|                                                                              |
|  INDIRECTION LAYERS IN VFS                                                   |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  read(fd, buf, 100)                                                      │ |
|  │        │                                                                 │ |
|  │        ▼                                                                 │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │ LAYER 1: System call dispatch                                    │    │ |
|  │  │ sys_read() → table lookup → syscall handler                      │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │        │                                                                 │ |
|  │        ▼                                                                 │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │ LAYER 2: FD → struct file lookup                                 │    │ |
|  │  │ fget_light(fd) → file descriptor table → struct file             │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │        │                                                                 │ |
|  │        ▼                                                                 │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │ LAYER 3: VFS ops dispatch                                        │    │ |
|  │  │ vfs_read() → file->f_op->read (function pointer call)            │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │        │                                                                 │ |
|  │        ▼                                                                 │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │ LAYER 4: Page cache lookup                                       │    │ |
|  │  │ find_get_page() → radix tree lookup → struct page                │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │        │                                                                 │ |
|  │        ▼                                                                 │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │ LAYER 5: Address space ops (if cache miss)                       │    │ |
|  │  │ a_ops->readpage() (another function pointer call)                │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │        │                                                                 │ |
|  │        ▼                                                                 │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │ LAYER 6: Block layer dispatch                                    │    │ |
|  │  │ submit_bio() → request queue → block driver                      │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  PERFORMANCE IMPACT:                                                     │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Function pointer calls:                                         │    │ |
|  │  │  • Cannot be inlined by compiler                                 │    │ |
|  │  │  • Branch prediction less effective                              │    │ |
|  │  │  • Retpoline overhead (for Spectre mitigation)                   │    │ |
|  │  │                                                                  │    │ |
|  │  │  Mitigations in kernel:                                          │    │ |
|  │  │  • Hot path optimizations (likely/unlikely hints)                │    │ |
|  │  │  • Inline common cases (generic_file_read for page cache)        │    │ |
|  │  │  • Cache-aligned data structures                                 │    │ |
|  │  │                                                                  │    │ |
|  │  │  Tradeoff:                                                       │    │ |
|  │  │  • Indirection cost: ~10-50 cycles per call                      │    │ |
|  │  │  • Disk I/O: ~10,000,000 cycles                                  │    │ |
|  │  │  • Network I/O: ~1,000,000 cycles                                │    │ |
|  │  │  → Indirection overhead is negligible for I/O-bound workloads    │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**间接调用开销**：

6 层间接：
1. 系统调用分发
2. FD → struct file 查找
3. VFS ops 分发（函数指针调用）
4. Page cache 查找
5. Address space ops（函数指针调用）
6. 块层分发

**性能影响**：
- 函数指针调用：不能内联，分支预测效果差，Retpoline 开销
- 缓解措施：热路径优化、内联常见情况、缓存对齐数据结构

**权衡**：
- 间接开销：~10-50 周期/调用
- 磁盘 I/O：~10,000,000 周期
- 网络 I/O：~1,000,000 周期
- → I/O 密集型工作负载中间接开销可忽略
