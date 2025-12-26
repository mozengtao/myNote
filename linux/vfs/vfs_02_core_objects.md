# VFS Architecture Study: Core Objects and Boundaries

## Overview: The Four Pillars of VFS

```
+------------------------------------------------------------------+
|  VFS CORE OBJECTS                                                |
+------------------------------------------------------------------+

    ┌─────────────────────────────────────────────────────────────┐
    │                      super_block                            │
    │              (Mounted filesystem instance)                   │
    │                          │                                   │
    │              ┌───────────┴───────────┐                      │
    │              │                       │                       │
    │              ▼                       ▼                       │
    │         ┌─────────┐            ┌─────────┐                  │
    │         │  inode  │ ◄─────────►│  inode  │  ...             │
    │         └────┬────┘            └─────────┘                  │
    │              │ (1:N)                                         │
    │              ▼                                               │
    │         ┌─────────┐                                         │
    │         │ dentry  │ ◄──── Path component cache              │
    │         └────┬────┘                                         │
    │              │ (1:N)                                         │
    │              ▼                                               │
    │         ┌─────────┐                                         │
    │         │  file   │ ◄──── Open file handle                  │
    │         └─────────┘                                         │
    └─────────────────────────────────────────────────────────────┘

    RELATIONSHIPS:
    • super_block ←─1:N─→ inodes (one superblock owns many inodes)
    • inode ←─1:N─→ dentries (one inode may have multiple names)
    • dentry ←─1:N─→ files (one path may be opened multiple times)
```

**中文解释：**
- `super_block`：代表一个已挂载的文件系统实例
- `inode`：代表一个文件或目录的元数据（与路径无关）
- `dentry`：代表路径中的一个组件（目录项缓存）
- `file`：代表一个打开的文件描述符

---

## 1. struct file — The Open File Handle

### What It Represents

```
+------------------------------------------------------------------+
|  struct file: AN OPEN FILE INSTANCE                              |
+------------------------------------------------------------------+

    Real-world concept:
    When a process calls open("/etc/passwd", O_RDONLY), the kernel
    creates a struct file to track THIS SPECIFIC OPEN INSTANCE.
    
    ┌─────────────────────────────────────────────────────────────┐
    │  Process A: fd 3 ───┐                                       │
    │  Process A: fd 5 ───┼──► Same file at different positions  │
    │  Process B: fd 4 ───┘                                       │
    │                                                              │
    │  Three struct file objects, one underlying inode           │
    └─────────────────────────────────────────────────────────────┘
```

### Definition (from `include/linux/fs.h`)

```c
struct file {
    /*
     * fu_list becomes invalid after file_free is called and queued via
     * fu_rcuhead for RCU freeing
     */
    union {
        struct list_head    fu_list;      /* For per-sb file list */
        struct rcu_head     fu_rcuhead;   /* For RCU-delayed free */
    } f_u;
    
    struct path             f_path;        /* dentry + mount point */
#define f_dentry    f_path.dentry
#define f_vfsmnt    f_path.mnt
    
    const struct file_operations *f_op;    /* OPS TABLE (polymorphism!) */
    
    spinlock_t              f_lock;        /* Protects f_ep_links, etc. */
    atomic_long_t           f_count;       /* Reference count */
    unsigned int            f_flags;       /* O_RDONLY, O_NONBLOCK, etc. */
    fmode_t                 f_mode;        /* FMODE_READ | FMODE_WRITE */
    loff_t                  f_pos;         /* Current read/write position */
    
    struct fown_struct      f_owner;       /* For SIGIO */
    const struct cred       *f_cred;       /* Credentials of opener */
    struct file_ra_state    f_ra;          /* Readahead state */
    
    void                    *private_data; /* Filesystem-specific data */
    struct address_space    *f_mapping;    /* Page cache mapping */
};
```

### Key Fields Explained

```
+------------------------------------------------------------------+
|  struct file KEY FIELDS                                          |
+------------------------------------------------------------------+

    f_path.dentry
    ┌─────────────────────────────────────────────────────────────┐
    │  Points to dentry (which points to inode)                   │
    │  Via this, we can reach: filename, inode, permissions       │
    └─────────────────────────────────────────────────────────────┘

    f_op (file_operations)
    ┌─────────────────────────────────────────────────────────────┐
    │  THE POLYMORPHISM MECHANISM                                 │
    │  Different files have different f_op:                       │
    │  • Regular file: ext4_file_operations                       │
    │  • Directory: ext4_dir_operations                           │
    │  • /proc file: proc_file_operations                         │
    │  • Device: driver-specific operations                       │
    └─────────────────────────────────────────────────────────────┘

    f_pos
    ┌─────────────────────────────────────────────────────────────┐
    │  Current position for read/write                            │
    │  Each struct file has its OWN position                      │
    │  Why: Multiple opens of same file need independent offsets  │
    └─────────────────────────────────────────────────────────────┘

    f_count (reference count)
    ┌─────────────────────────────────────────────────────────────┐
    │  Tracks how many references exist to this struct file      │
    │  • fd table entry: +1                                       │
    │  • mmap region: +1                                          │
    │  • Kernel holding reference: +1                             │
    │  When f_count → 0: file is released                        │
    └─────────────────────────────────────────────────────────────┘

    private_data
    ┌─────────────────────────────────────────────────────────────┐
    │  Opaque pointer for filesystem/driver use                  │
    │  • ext4: Extended file state                               │
    │  • Device driver: Per-open driver context                  │
    │  • Socket: socket structure                                │
    └─────────────────────────────────────────────────────────────┘
```

### Ownership and Lifetime

```
+------------------------------------------------------------------+
|  struct file LIFETIME                                            |
+------------------------------------------------------------------+

    CREATED BY:
    ┌─────────────────────────────────────────────────────────────┐
    │  do_filp_open() → get_empty_filp()                         │
    │  Called during: open(), openat(), accept()                  │
    └─────────────────────────────────────────────────────────────┘

    OWNED BY:
    ┌─────────────────────────────────────────────────────────────┐
    │  Shared ownership via reference counting (f_count)          │
    │  • Process fd table holds reference                        │
    │  • Kernel code uses fget()/fput() for temporary refs       │
    └─────────────────────────────────────────────────────────────┘

    DESTROYED BY:
    ┌─────────────────────────────────────────────────────────────┐
    │  fput() when f_count reaches 0                             │
    │  Calls: f_op->release() then frees struct file             │
    │  RCU delay: actual free may be deferred                    │
    └─────────────────────────────────────────────────────────────┘

    REFERENCE COUNTING APIs:
    ┌─────────────────────────────────────────────────────────────┐
    │  get_file(f)     — Increment f_count                       │
    │  fget(fd)        — Get file from fd, increment count       │
    │  fput(f)         — Decrement count, free if zero           │
    │  fget_light()    — Fast path when caller already holds ref │
    └─────────────────────────────────────────────────────────────┘
```

---

## 2. struct inode — The On-Disk Object Identity

### What It Represents

```
+------------------------------------------------------------------+
|  struct inode: METADATA OF A FILE/DIRECTORY                      |
+------------------------------------------------------------------+

    Real-world concept:
    An inode represents "the file itself" — independent of its name.
    
    ┌─────────────────────────────────────────────────────────────┐
    │  /home/user/readme.txt  ───┐                                │
    │  /tmp/link_to_readme    ───┼──► Same inode (hard links)    │
    │                             │                                │
    │  Both paths, one inode (one i_nlink = 2)                    │
    └─────────────────────────────────────────────────────────────┘

    The inode contains:
    • Owner (uid, gid)
    • Permissions (mode)
    • Timestamps (atime, mtime, ctime)
    • Size
    • Block pointers (for disk-based filesystems)
    
    The inode does NOT contain:
    • Filename (that's in the dentry/directory)
    • Open position (that's in struct file)
```

### Key Fields (from `include/linux/fs.h`)

```c
struct inode {
    /* Identity and permissions */
    umode_t         i_mode;        /* File type + permissions */
    uid_t           i_uid;         /* Owner UID */
    gid_t           i_gid;         /* Owner GID */
    unsigned int    i_flags;       /* Inode flags (S_SYNC, etc.) */
    
    /* Operations (polymorphism) */
    const struct inode_operations   *i_op;   /* Inode ops (lookup, etc.) */
    const struct file_operations    *i_fop;  /* Default file ops */
    
    /* Filesystem linkage */
    struct super_block  *i_sb;     /* Owning superblock */
    struct address_space *i_mapping; /* Page cache for this inode */
    
    /* Statistics */
    unsigned long   i_ino;         /* Inode number */
    unsigned int    i_nlink;       /* Hard link count */
    dev_t           i_rdev;        /* Device ID (if device file) */
    loff_t          i_size;        /* File size in bytes */
    
    /* Timestamps */
    struct timespec i_atime;       /* Access time */
    struct timespec i_mtime;       /* Modification time */
    struct timespec i_ctime;       /* Change time (metadata) */
    
    /* Reference counting */
    atomic_t        i_count;       /* Reference count */
    
    /* Locking */
    spinlock_t      i_lock;        /* Protects i_state, etc. */
    struct mutex    i_mutex;       /* Directory operations */
    
    /* Caching and lists */
    struct hlist_node i_hash;      /* Hash table linkage */
    struct list_head  i_lru;       /* LRU list */
    struct list_head  i_sb_list;   /* Per-superblock list */
    
    /* Directory entries pointing here */
    union {
        struct list_head i_dentry;  /* List of dentries */
        struct rcu_head  i_rcu;     /* RCU delayed free */
    };
    
    /* Special file types */
    union {
        struct pipe_inode_info  *i_pipe;  /* If pipe */
        struct block_device     *i_bdev;  /* If block device */
        struct cdev             *i_cdev;  /* If char device */
    };
    
    /* Filesystem-private data */
    void            *i_private;
};
```

### Why inode Is Separate from dentry and file

```
+------------------------------------------------------------------+
|  SEPARATION OF CONCERNS                                          |
+------------------------------------------------------------------+

    WHY NOT PUT EVERYTHING IN ONE STRUCT?
    
    ┌─────────────────────────────────────────────────────────────┐
    │  INODE represents: The file/directory IDENTITY             │
    │  - Exists on disk (for disk-based filesystems)              │
    │  - Independent of pathnames                                  │
    │  - One per file, regardless of link count                   │
    │  - Cached in memory (inode cache)                           │
    └─────────────────────────────────────────────────────────────┘

    ┌─────────────────────────────────────────────────────────────┐
    │  DENTRY represents: A PATH COMPONENT                        │
    │  - Maps name → inode                                        │
    │  - Multiple dentries can point to one inode (hard links)   │
    │  - Purely in-memory (not stored on disk*)                   │
    │  - Cached for fast path lookups                             │
    └─────────────────────────────────────────────────────────────┘

    ┌─────────────────────────────────────────────────────────────┐
    │  FILE represents: An OPEN INSTANCE                          │
    │  - Current position, flags for this open                   │
    │  - Process-specific state                                   │
    │  - Multiple struct files can share one inode               │
    │  - No on-disk representation                                │
    └─────────────────────────────────────────────────────────────┘

    * Directory entries are stored on disk, but struct dentry is
      an in-memory cache of those entries.
```

### Inode Lifetime

```
+------------------------------------------------------------------+
|  struct inode LIFETIME                                           |
+------------------------------------------------------------------+

    CREATED BY:
    ┌─────────────────────────────────────────────────────────────┐
    │  new_inode(sb) — Allocate new inode                        │
    │  iget_locked(sb, ino) — Get or create from inode number    │
    │  s_op->alloc_inode(sb) — Filesystem-specific allocation    │
    └─────────────────────────────────────────────────────────────┘

    REFERENCE COUNTING:
    ┌─────────────────────────────────────────────────────────────┐
    │  ihold(inode)   — Increment i_count                        │
    │  iput(inode)    — Decrement, may trigger eviction          │
    │  igrab(inode)   — Get reference if inode is active         │
    └─────────────────────────────────────────────────────────────┘

    CACHED:
    ┌─────────────────────────────────────────────────────────────┐
    │  Inodes stay in cache even when i_count = 0                │
    │  LRU eviction reclaims unused inodes under memory pressure │
    │  i_state flags track: I_NEW, I_DIRTY*, I_FREEING           │
    └─────────────────────────────────────────────────────────────┘

    EVICTION:
    ┌─────────────────────────────────────────────────────────────┐
    │  When i_count = 0 and memory pressure:                     │
    │  1. Write back dirty data                                   │
    │  2. Call s_op->evict_inode()                               │
    │  3. Free inode structure                                    │
    └─────────────────────────────────────────────────────────────┘
```

---

## 3. struct dentry — The Path Cache

### What It Represents

```
+------------------------------------------------------------------+
|  struct dentry: DIRECTORY ENTRY CACHE                            |
+------------------------------------------------------------------+

    Real-world concept:
    When resolving /home/user/readme.txt, we need to:
    1. Look up "home" in root directory
    2. Look up "user" in /home directory
    3. Look up "readme.txt" in /home/user directory
    
    Each lookup could require disk I/O!
    
    DENTRY CACHE speeds this up:
    ┌─────────────────────────────────────────────────────────────┐
    │  PATH: /home/user/readme.txt                                │
    │                                                              │
    │  ┌───────┐    ┌───────┐    ┌───────┐    ┌─────────────┐    │
    │  │   /   │───►│ home  │───►│ user  │───►│ readme.txt  │    │
    │  └───┬───┘    └───┬───┘    └───┬───┘    └──────┬──────┘    │
    │      │            │            │               │            │
    │      ▼            ▼            ▼               ▼            │
    │   inode1       inode2       inode3         inode4          │
    │                                                              │
    │  Each box is a struct dentry                               │
    │  Cached in memory for fast re-lookup                       │
    └─────────────────────────────────────────────────────────────┘
```

### Key Fields

```c
struct dentry {
    /* Reference counting */
    unsigned int d_count;          /* Usage count */
    unsigned int d_flags;          /* Dentry flags */
    
    /* Identity */
    spinlock_t d_lock;             /* Per-dentry lock */
    struct inode *d_inode;         /* Associated inode (or NULL) */
    
    /* Name */
    struct qstr d_name;            /* Quick string (hash + name) */
    unsigned char d_iname[DNAME_INLINE_LEN]; /* Short name storage */
    
    /* Hierarchy */
    struct dentry *d_parent;       /* Parent dentry */
    struct list_head d_subdirs;    /* Child dentries */
    struct list_head d_child;      /* Sibling linkage */
    
    /* Hash table */
    struct hlist_bl_node d_hash;   /* Lookup hash table */
    
    /* Operations */
    const struct dentry_operations *d_op;
    
    /* Superblock */
    struct super_block *d_sb;      /* Owning superblock */
    
    /* Aliases (multiple dentries → same inode for hard links) */
    struct list_head d_alias;      /* List of aliases (in inode) */
    
    /* Filesystem-private */
    void *d_fsdata;
};
```

### Dentry States

```
+------------------------------------------------------------------+
|  DENTRY STATES                                                   |
+------------------------------------------------------------------+

    POSITIVE DENTRY (d_inode != NULL)
    ┌─────────────────────────────────────────────────────────────┐
    │  Name exists and points to valid inode                     │
    │  Example: dentry for "/etc/passwd" with d_inode set        │
    └─────────────────────────────────────────────────────────────┘

    NEGATIVE DENTRY (d_inode == NULL)
    ┌─────────────────────────────────────────────────────────────┐
    │  Name was looked up but DOES NOT EXIST                     │
    │  Cached to avoid repeated failed lookups                   │
    │  Example: open("/nonexistent") → cache "nonexistent" = NULL│
    └─────────────────────────────────────────────────────────────┘

    DISCONNECTED DENTRY
    ┌─────────────────────────────────────────────────────────────┐
    │  Has inode but no parent (for NFS file handles)            │
    │  Can be reconnected when needed                            │
    └─────────────────────────────────────────────────────────────┘
```

---

## 4. struct super_block — The Mounted Filesystem

### What It Represents

```
+------------------------------------------------------------------+
|  struct super_block: A MOUNTED FILESYSTEM                        |
+------------------------------------------------------------------+

    Real-world concept:
    When you mount a filesystem:
    $ mount /dev/sda1 /mnt
    
    The kernel creates a super_block to represent THIS MOUNT:
    
    ┌─────────────────────────────────────────────────────────────┐
    │  super_block                                                │
    │  ├── s_type: ext4                                          │
    │  ├── s_dev: /dev/sda1                                      │
    │  ├── s_root: dentry for /mnt                               │
    │  ├── s_op: ext4_sops (superblock operations)               │
    │  └── s_inodes: list of all inodes on this mount           │
    └─────────────────────────────────────────────────────────────┘

    Multiple mounts = Multiple super_blocks:
    ┌─────────────────────────────────────────────────────────────┐
    │  mount /dev/sda1 /mnt       → super_block A                │
    │  mount /dev/sda2 /data      → super_block B                │
    │  mount -t tmpfs none /tmp   → super_block C                │
    │  mount -t proc none /proc   → super_block D                │
    └─────────────────────────────────────────────────────────────┘
```

### Key Fields

```c
struct super_block {
    /* Identity */
    struct list_head    s_list;        /* List of all superblocks */
    dev_t               s_dev;         /* Device identifier */
    unsigned long       s_magic;       /* Filesystem magic number */
    
    /* Filesystem type */
    struct file_system_type *s_type;   /* Which filesystem (ext4, etc.) */
    
    /* Operations (polymorphism) */
    const struct super_operations   *s_op;
    const struct dquot_operations   *dq_op;    /* Quota */
    const struct export_operations  *s_export_op; /* NFS export */
    
    /* Configuration */
    unsigned long       s_flags;       /* Mount flags (MS_RDONLY, etc.) */
    unsigned long       s_blocksize;   /* Block size in bytes */
    loff_t              s_maxbytes;    /* Max file size */
    
    /* Root dentry */
    struct dentry       *s_root;       /* Root of this filesystem */
    
    /* Reference counting */
    int                 s_count;       /* Reference count */
    atomic_t            s_active;      /* Active references */
    struct rw_semaphore s_umount;      /* Unmount lock */
    
    /* Inode and dentry management */
    struct list_head    s_inodes;      /* All inodes */
    struct list_head    s_dentry_lru;  /* Unused dentry LRU */
    struct list_head    s_inode_lru;   /* Unused inode LRU */
    
    /* Block device (if applicable) */
    struct block_device *s_bdev;
    struct backing_dev_info *s_bdi;
    
    /* Filesystem-private */
    void                *s_fs_info;    /* Filesystem-specific info */
};
```

---

## 5. How Objects Work Together

```
+------------------------------------------------------------------+
|  OBJECT RELATIONSHIPS DURING open("/home/user/file.txt")         |
+------------------------------------------------------------------+

    1. PATH RESOLUTION (using dentry cache)
    
       "/" ──► "home" ──► "user" ──► "file.txt"
        │        │          │            │
        ▼        ▼          ▼            ▼
    [dentry] [dentry]  [dentry]     [dentry]
        │        │          │            │
        └────────┴──────────┴────────────┤
                                         ▼
                                     [inode]
                                    (the file)

    2. CREATE struct file
    
    ┌──────────────────────────────────────────────────────────────┐
    │  struct file                                                │
    │  ├── f_path.dentry ──────────► dentry for "file.txt"       │
    │  │                                  │                       │
    │  │                                  ▼                       │
    │  │                              [inode]                     │
    │  │                                  │                       │
    │  ├── f_op ◄─────────────────────────┘ (from inode->i_fop)  │
    │  │                                                          │
    │  ├── f_pos = 0                                              │
    │  └── f_count = 1                                            │
    └──────────────────────────────────────────────────────────────┘

    3. SUPERBLOCK tracks everything
    
    ┌──────────────────────────────────────────────────────────────┐
    │  super_block (for the filesystem containing file.txt)       │
    │  ├── s_inodes: list including the inode                    │
    │  ├── s_root: dentry for mount point                        │
    │  └── s_op: filesystem operations                           │
    └──────────────────────────────────────────────────────────────┘
```

---

## 6. Reference Counting and Caching

```
+------------------------------------------------------------------+
|  REFERENCE COUNTING RULES                                        |
+------------------------------------------------------------------+

    struct file:
    ┌─────────────────────────────────────────────────────────────┐
    │  f_count via atomic_long_t                                  │
    │  get_file() / fput() — standard refcount                   │
    │  fget(fd) — get from fd table                              │
    │  When 0: release() called, file freed                      │
    └─────────────────────────────────────────────────────────────┘

    struct inode:
    ┌─────────────────────────────────────────────────────────────┐
    │  i_count via atomic_t                                       │
    │  ihold() / iput() — standard refcount                      │
    │  When 0: inode may stay in cache (LRU)                     │
    │  Evicted under memory pressure                             │
    └─────────────────────────────────────────────────────────────┘

    struct dentry:
    ┌─────────────────────────────────────────────────────────────┐
    │  d_count via unsigned int (d_lock protected)               │
    │  dget() / dput() — standard refcount                       │
    │  When 0: dentry may stay in cache (LRU)                    │
    │  Evicted under memory pressure                             │
    └─────────────────────────────────────────────────────────────┘

    struct super_block:
    ┌─────────────────────────────────────────────────────────────┐
    │  s_active for active usage                                  │
    │  s_count for structural references                         │
    │  grab_super() / drop_super()                               │
    │  Unmount only when s_active = 0                            │
    └─────────────────────────────────────────────────────────────┘
```

### Cache Hierarchy

```
+------------------------------------------------------------------+
|  VFS CACHE HIERARCHY                                             |
+------------------------------------------------------------------+

    ┌─────────────────────────────────────────────────────────────┐
    │  DENTRY CACHE (dcache)                                      │
    │  • Hash table for path → dentry lookup                     │
    │  • LRU list for unused dentries                            │
    │  • sysctl: vfs_cache_pressure controls eviction            │
    └─────────────────────────────────────────────────────────────┘
                              │
                              │ d_inode pointer
                              ▼
    ┌─────────────────────────────────────────────────────────────┐
    │  INODE CACHE (icache)                                       │
    │  • Hash table for (dev, ino) → inode lookup                │
    │  • LRU list for unused inodes                              │
    │  • Per-superblock inode lists                              │
    └─────────────────────────────────────────────────────────────┘
                              │
                              │ i_mapping (address_space)
                              ▼
    ┌─────────────────────────────────────────────────────────────┐
    │  PAGE CACHE                                                 │
    │  • Radix tree of pages for each inode                      │
    │  • address_space_operations for I/O                        │
    │  • Writeback via background threads                        │
    └─────────────────────────────────────────────────────────────┘
```

---

## Summary

```
+------------------------------------------------------------------+
|  CORE OBJECTS SUMMARY                                            |
+------------------------------------------------------------------+

    OBJECT          REPRESENTS              OWNED BY
    ──────────────────────────────────────────────────────────────
    super_block     Mounted filesystem      Global list, refcounted
    inode           File identity           Superblock, cached
    dentry          Path component          Parent dentry, cached
    file            Open file handle        Process fd table

    KEY INSIGHTS:
    
    1. SEPARATION allows multiple dentries → one inode (hard links)
    2. SEPARATION allows multiple files → one inode (multiple opens)
    3. CACHING reduces disk I/O (dcache, icache, page cache)
    4. REFCOUNTING enables safe sharing without explicit ownership
    5. OPS TABLES enable polymorphism (different FS, same interface)
```

**中文总结：**
- **super_block**：挂载的文件系统，拥有所有 inodes
- **inode**：文件身份（元数据），与路径无关
- **dentry**：路径组件缓存，映射名称到 inode
- **file**：打开的文件句柄，进程特定状态

**分离的好处**：
- 硬链接：多个 dentry → 一个 inode
- 多次打开：多个 file → 一个 inode
- 缓存减少磁盘 I/O
- 引用计数实现安全共享

