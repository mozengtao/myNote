# Presentation–Domain–Data Layering: What It REALLY Means

## Executive Summary

```
+------------------------------------------------------------------+
|  PDD IS NOT ABOUT FOLDER NAMES OR FRAMEWORK CONVENTIONS          |
+------------------------------------------------------------------+

    PDD is about:
    ┌─────────────────────────────────────────────────────────────┐
    │  1. DEPENDENCY DIRECTION — who depends on whom              │
    │  2. CHANGE ISOLATION — what changes when X changes          │
    │  3. TESTABILITY — what can be tested in isolation           │
    │  4. REPLACEABILITY — what can be swapped without rewrites   │
    └─────────────────────────────────────────────────────────────┘

    The Three Layers:
    
    ┌─────────────────────────────────────────────────────────────┐
    │                    PRESENTATION                             │
    │  (How the outside world talks to us)                        │
    │  syscalls, CLI, network protocols, device files             │
    └─────────────────────────────────────────────────────────────┘
                              │
                              │ depends on
                              ▼
    ┌─────────────────────────────────────────────────────────────┐
    │                      DOMAIN                                 │
    │  (The rules, policies, invariants — the "WHAT")             │
    │  business logic, state machines, validation                 │
    └─────────────────────────────────────────────────────────────┘
                              │
                              │ depends on
                              ▼
    ┌─────────────────────────────────────────────────────────────┐
    │                       DATA                                  │
    │  (How we persist/retrieve — the "HOW")                      │
    │  storage, networking, hardware access                       │
    └─────────────────────────────────────────────────────────────┘

    CRITICAL RULE:
    Dependencies flow DOWNWARD only.
    Domain NEVER knows about Presentation.
    Data NEVER knows about Domain's business meaning.
```

**中文解释：**
- **PDD 不是关于文件夹命名或框架约定**
- PDD 是关于：依赖方向、变更隔离、可测试性、可替换性
- **展示层**：外部世界如何与我们交互（系统调用、CLI、网络协议）
- **领域层**：规则、策略、不变量——"做什么"
- **数据层**：如何持久化/检索——"怎么做"
- **关键规则**：依赖只能向下流动

---

## 1.1 Presentation Layer: Beyond "UI"

### What Presentation Really Means

```
+------------------------------------------------------------------+
|  PRESENTATION = INPUT ADAPTATION + OUTPUT FORMATTING             |
+------------------------------------------------------------------+

    COMMON MISUNDERSTANDING:
    "Presentation is just the GUI or CLI"
    
    CORRECT UNDERSTANDING:
    Presentation is ANY BOUNDARY between your system and the outside world
    
    ┌─────────────────────────────────────────────────────────────┐
    │  PRESENTATION LAYER EXAMPLES:                               │
    │                                                              │
    │  • System call interface (read, write, ioctl)               │
    │  • Network protocol parsing (HTTP headers, TCP segments)    │
    │  • Device file operations (/dev/xxx)                        │
    │  • Configuration file parsing                                │
    │  • Command-line argument processing                          │
    │  • IPC message serialization/deserialization                │
    │  • Signal handlers (SIGTERM → graceful shutdown)            │
    └─────────────────────────────────────────────────────────────┘
```

### Responsibilities and Non-Responsibilities

```
+------------------------------------------------------------------+
|  PRESENTATION LAYER RESPONSIBILITIES                             |
+------------------------------------------------------------------+

    DO:
    ┌─────────────────────────────────────────────────────────────┐
    │  ✓ Parse external input into internal representation       │
    │  ✓ Validate input FORMAT (not business rules)              │
    │  ✓ Translate domain results into external format           │
    │  ✓ Handle protocol-specific concerns (endianness, encoding)│
    │  ✓ Map errors to external error codes                      │
    │  ✓ Manage session/connection state                         │
    └─────────────────────────────────────────────────────────────┘

    DO NOT:
    ┌─────────────────────────────────────────────────────────────┐
    │  ✗ Make business decisions                                  │
    │  ✗ Validate business invariants                             │
    │  ✗ Access data storage directly                             │
    │  ✗ Contain algorithms or policies                           │
    │  ✗ Know about other presentation channels                   │
    └─────────────────────────────────────────────────────────────┘
```

### Linux Kernel Example: `file_operations`

```c
/*
 * From include/linux/fs.h (lines 1583-1611)
 * 
 * file_operations IS the Presentation Layer for file-based I/O.
 * It translates user-space syscalls into domain operations.
 */
struct file_operations {
    struct module *owner;
    
    /* INPUT ADAPTATION: translate syscall args to domain calls */
    loff_t (*llseek) (struct file *, loff_t, int);
    ssize_t (*read) (struct file *, char __user *, size_t, loff_t *);
    ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *);
    
    /* PROTOCOL HANDLING: async I/O specifics */
    ssize_t (*aio_read) (struct kiocb *, const struct iovec *, unsigned long, loff_t);
    ssize_t (*aio_write) (struct kiocb *, const struct iovec *, unsigned long, loff_t);
    
    /* USER INTERACTION: blocking/polling */
    unsigned int (*poll) (struct file *, struct poll_table_struct *);
    
    /* INPUT ADAPTATION: ioctl is presentation's extension point */
    long (*unlocked_ioctl) (struct file *, unsigned int, unsigned long);
    
    /* CONNECTION STATE: open/release manage per-file state */
    int (*open) (struct inode *, struct file *);
    int (*release) (struct inode *, struct file *);
    
    /* ... more presentation concerns ... */
};
```

**What this shows:**
- `read()/write()` translate user buffer + offset into domain operations
- `ioctl()` is the "extension protocol" for device-specific commands
- `open()/release()` manage connection lifecycle
- **None of these decide policy** — they just translate and dispatch

### Input Adaptation vs Decision Making

```
+------------------------------------------------------------------+
|  PRESENTATION ADAPTS INPUT — DOMAIN MAKES DECISIONS              |
+------------------------------------------------------------------+

    WRONG (presentation makes decision):
    ┌─────────────────────────────────────────────────────────────┐
    │  ssize_t my_read(struct file *f, char __user *buf, ...) {  │
    │      if (file_size > MAX_ALLOWED)  // Business rule HERE!   │
    │          return -EFBIG;                                      │
    │      if (user_quota_exceeded())    // Business rule HERE!   │
    │          return -EDQUOT;                                     │
    │      ...                                                     │
    │  }                                                           │
    └─────────────────────────────────────────────────────────────┘

    CORRECT (presentation only adapts):
    ┌─────────────────────────────────────────────────────────────┐
    │  ssize_t my_read(struct file *f, char __user *buf, ...) {  │
    │      /* Validate FORMAT only */                             │
    │      if (!access_ok(VERIFY_WRITE, buf, count))              │
    │          return -EFAULT;                                     │
    │                                                              │
    │      /* Delegate to domain */                                │
    │      return domain_perform_read(f->private_data, ...);      │
    │  }                                                           │
    └─────────────────────────────────────────────────────────────┘
```

---

## 1.2 Domain Layer: The Protected Core

### What Domain Means in Engineering Terms

```
+------------------------------------------------------------------+
|  DOMAIN = POLICY + INVARIANTS + BUSINESS RULES                   |
+------------------------------------------------------------------+

    The domain layer answers: "WHAT should happen?"
    
    NOT how to parse the request (presentation)
    NOT how to store the result (data)
    
    ┌─────────────────────────────────────────────────────────────┐
    │  DOMAIN LAYER CONTAINS:                                     │
    │                                                              │
    │  • State machines (connection states, file modes)           │
    │  • Invariants (refcount > 0 while in use)                   │
    │  • Policies (access control, scheduling decisions)          │
    │  • Algorithms (I/O scheduling, memory allocation)           │
    │  • Business rules (quota enforcement, validation)           │
    │  • Transformations (data processing logic)                  │
    └─────────────────────────────────────────────────────────────┘
```

### Policy vs Mechanism

```
+------------------------------------------------------------------+
|  POLICY (Domain) vs MECHANISM (Data/Presentation)                |
+------------------------------------------------------------------+

    POLICY (Domain decides):
    ┌─────────────────────────────────────────────────────────────┐
    │  "Which process runs next?"                                 │
    │  "Is this user allowed to read this file?"                  │
    │  "Should we accept this network connection?"                │
    │  "When do we flush dirty pages?"                            │
    │  "How long should the timeout be?"                          │
    └─────────────────────────────────────────────────────────────┘

    MECHANISM (Data/Infrastructure provides):
    ┌─────────────────────────────────────────────────────────────┐
    │  "How to context switch"                                    │
    │  "How to check permission bits"                             │
    │  "How to accept on a socket"                                │
    │  "How to write to disk"                                     │
    │  "How to set a timer"                                       │
    └─────────────────────────────────────────────────────────────┘
```

### Linux Kernel Example: VFS as Domain Layer

```c
/*
 * The VFS is the DOMAIN LAYER for filesystem operations.
 * It defines POLICIES that all filesystems must follow.
 */

/* From fs.h: inode_operations defines domain policies */
struct inode_operations {
    /* Domain policy: how to resolve a name */
    struct dentry * (*lookup) (struct inode *, struct dentry *, struct nameidata *);
    
    /* Domain policy: how to check permissions */
    int (*permission) (struct inode *, int);
    
    /* Domain policy: file creation rules */
    int (*create) (struct inode *, struct dentry *, int, struct nameidata *);
    
    /* Domain policy: what happens when attributes change */
    int (*setattr) (struct dentry *, struct iattr *);
    
    /* ... more domain policies ... */
};

/* 
 * super_operations defines filesystem-level domain policies
 * (Lines 1658-1686 in fs.h)
 */
struct super_operations {
    /* Domain policy: how to allocate new inodes */
    struct inode *(*alloc_inode)(struct super_block *sb);
    
    /* Domain policy: what to do when inode is dirty */
    void (*dirty_inode) (struct inode *, int flags);
    
    /* Domain policy: how to sync filesystem state */
    int (*sync_fs)(struct super_block *sb, int wait);
    
    /* ... policies for the entire filesystem ... */
};
```

### Invariants and Rules

```
+------------------------------------------------------------------+
|  DOMAIN ENFORCES INVARIANTS                                      |
+------------------------------------------------------------------+

    INVARIANT: A condition that must ALWAYS be true
    
    Linux Kernel Inode Invariants (from fs.h comments):
    ┌─────────────────────────────────────────────────────────────┐
    │  • i_count > 0 while inode is in use                       │
    │  • i_nlink reflects actual link count                       │
    │  • I_NEW set until inode fully initialized                  │
    │  • i_mutex held during directory modifications              │
    └─────────────────────────────────────────────────────────────┘

    Example: nlink manipulation (lines 1775-1832)
    
    These functions ENFORCE the invariant that i_nlink 
    is modified correctly:
    
    set_nlink(inode, n)    - set link count
    inc_nlink(inode)       - increment safely  
    drop_nlink(inode)      - decrement (may reach 0)
    clear_nlink(inode)     - set to 0 (unlink)
    
    The domain layer provides THESE functions, not raw access.
```

### Why This Layer Must Be Protected

```
+------------------------------------------------------------------+
|  THE DOMAIN LAYER IS THE MOST VALUABLE ASSET                     |
+------------------------------------------------------------------+

    IF DOMAIN DEPENDS ON PRESENTATION:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Changing CLI args breaks business logic                  │
    │  • Adding REST API requires rewriting domain                │
    │  • Cannot test domain without mock presentation             │
    │  • Protocol bug can corrupt business state                  │
    └─────────────────────────────────────────────────────────────┘

    IF DOMAIN DEPENDS ON DATA:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Changing database breaks business logic                  │
    │  • Cannot test domain without real storage                  │
    │  • Storage bug corrupts business invariants                 │
    │  • Performance optimization requires domain rewrite         │
    └─────────────────────────────────────────────────────────────┘

    PROTECTED DOMAIN (correct):
    ┌─────────────────────────────────────────────────────────────┐
    │  • Domain defines interfaces, others implement              │
    │  • Domain can be tested in complete isolation               │
    │  • Storage can be swapped without domain changes            │
    │  • New presentation channels add code, not change it        │
    └─────────────────────────────────────────────────────────────┘
```

---

## 1.3 Data Layer: Infrastructure and Mechanism

### What Data Means Beyond Databases

```
+------------------------------------------------------------------+
|  DATA LAYER = ALL INFRASTRUCTURE / MECHANISM                     |
+------------------------------------------------------------------+

    DATA LAYER IS NOT JUST:
    - SQL databases
    - File storage
    
    DATA LAYER INCLUDES:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Block device drivers                                     │
    │  • Network socket implementation                            │
    │  • Hardware register access                                 │
    │  • Memory allocation (mechanism, not policy)                │
    │  • Timer hardware                                           │
    │  • DMA engines                                              │
    │  • Cache implementations                                    │
    │  • Serialization formats                                    │
    │  • External service clients                                 │
    └─────────────────────────────────────────────────────────────┘

    KEY INSIGHT:
    Data layer provides MECHANISM.
    It answers "HOW to do X" — never "SHOULD we do X".
```

### Linux Kernel Example: block_device as Data Layer

```c
/*
 * From fs.h (lines 660-694)
 * block_device is pure DATA LAYER — mechanism only
 */
struct block_device {
    dev_t           bd_dev;         /* Device identification */
    int             bd_openers;     /* Reference counting mechanism */
    struct inode *  bd_inode;       /* Representation in VFS */
    struct super_block * bd_super;  /* If mounted */
    struct mutex    bd_mutex;       /* Synchronization mechanism */
    
    unsigned        bd_block_size;  /* Hardware characteristic */
    struct hd_struct *bd_part;      /* Partition info */
    struct gendisk  *bd_disk;       /* Underlying disk */
    
    /* ... more hardware/mechanism details ... */
};

/*
 * block_device_operations (from blkdev.h) — pure mechanism
 */
struct block_device_operations {
    int (*open) (struct block_device *, fmode_t);
    int (*release) (struct gendisk *, fmode_t);
    int (*ioctl) (struct block_device *, fmode_t, unsigned, unsigned long);
    int (*media_changed) (struct gendisk *);
    int (*revalidate_disk) (struct gendisk *);
    /* ... how to talk to hardware ... */
};
```

### Replaceability and Volatility

```
+------------------------------------------------------------------+
|  DATA LAYER SHOULD BE REPLACEABLE                                |
+------------------------------------------------------------------+

    REPLACEABILITY EXAMPLES IN LINUX:
    
    ┌─────────────────────────────────────────────────────────────┐
    │  DOMAIN CONCEPT          DATA LAYER IMPLEMENTATIONS         │
    ├─────────────────────────────────────────────────────────────┤
    │  "file storage"          ext4, xfs, btrfs, nfs, tmpfs      │
    │  "network transport"     TCP/IPv4, TCP/IPv6, SCTP, DCCP    │
    │  "block I/O"            SATA, NVMe, virtio-blk, NBD        │
    │  "memory pages"          RAM, swap, zram, compressed       │
    └─────────────────────────────────────────────────────────────┘

    This works because:
    1. Domain defines the INTERFACE (e.g., address_space_operations)
    2. Data layer IMPLEMENTS the interface
    3. Domain never knows which implementation is running
```

### Data Layer Contract Example

```c
/*
 * address_space_operations (fs.h lines 583-619)
 * 
 * This is the CONTRACT that the data layer must fulfill.
 * The domain (VFS page cache) calls these; storage implements them.
 */
struct address_space_operations {
    /* Data layer must: write a page to storage */
    int (*writepage)(struct page *page, struct writeback_control *wbc);
    
    /* Data layer must: read a page from storage */
    int (*readpage)(struct file *, struct page *);
    
    /* Data layer must: mark page as dirty in storage-specific way */
    int (*set_page_dirty)(struct page *page);
    
    /* Data layer must: handle direct I/O */
    ssize_t (*direct_IO)(int, struct kiocb *, const struct iovec *iov,
                         loff_t offset, unsigned long nr_segs);
    
    /* ... storage must implement these mechanisms ... */
};
```

---

## 1.4 How PDD Works as a Whole

### Dependency Direction

```
+------------------------------------------------------------------+
|  THE FUNDAMENTAL RULE: DEPENDENCIES FLOW DOWNWARD                |
+------------------------------------------------------------------+

                    OUTSIDE WORLD
                          │
                          ▼
    ┌─────────────────────────────────────────────────────────────┐
    │                    PRESENTATION                             │
    │                                                              │
    │   - Knows about: external protocols, Domain interfaces     │
    │   - Does NOT know: Data layer details                       │
    └─────────────────────────────────────────────────────────────┘
                          │
                          │ calls Domain interfaces
                          ▼
    ┌─────────────────────────────────────────────────────────────┐
    │                      DOMAIN                                 │
    │                                                              │
    │   - Knows about: Domain interfaces, Data interfaces        │
    │   - Does NOT know: Presentation details, Data impl         │
    └─────────────────────────────────────────────────────────────┘
                          │
                          │ calls Data interfaces
                          ▼
    ┌─────────────────────────────────────────────────────────────┐
    │                       DATA                                  │
    │                                                              │
    │   - Knows about: Data interfaces, Hardware                 │
    │   - Does NOT know: Domain meaning, Presentation            │
    └─────────────────────────────────────────────────────────────┘

    KEY: Each layer defines INTERFACES that lower layers implement.
         Higher layers depend on interfaces, not implementations.
```

### Information Hiding

```
+------------------------------------------------------------------+
|  EACH LAYER HIDES ITS INTERNALS                                  |
+------------------------------------------------------------------+

    PRESENTATION HIDES:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Protocol details (HTTP headers, packet framing)          │
    │  • Session management (connection pooling, retry)           │
    │  • Input parsing complexity                                 │
    │  • Output formatting                                        │
    └─────────────────────────────────────────────────────────────┘

    DOMAIN HIDES:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Business rule complexity                                 │
    │  • State machine internals                                  │
    │  • Algorithm implementations                                │
    │  • Validation logic                                         │
    └─────────────────────────────────────────────────────────────┘

    DATA HIDES:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Storage format                                           │
    │  • Hardware quirks                                          │
    │  • Caching strategies                                       │
    │  • Connection management                                    │
    └─────────────────────────────────────────────────────────────┘
```

### Linux VFS Example: Complete PDD Stack

```
+------------------------------------------------------------------+
|  VFS AS A COMPLETE PDD EXAMPLE                                   |
+------------------------------------------------------------------+

    PRESENTATION LAYER:
    ┌─────────────────────────────────────────────────────────────┐
    │  fs/read_write.c          sys_read(), sys_write()           │
    │  fs/open.c                sys_open(), sys_close()           │
    │  fs/ioctl.c               sys_ioctl()                       │
    │  fs/readdir.c             sys_getdents()                    │
    │                                                              │
    │  file_operations          per-file syscall handlers         │
    └─────────────────────────────────────────────────────────────┘
                          │
                          ▼
    ┌─────────────────────────────────────────────────────────────┐
    │  DOMAIN LAYER:                                              │
    │                                                              │
    │  fs/namei.c               Path resolution policy            │
    │  fs/inode.c               Inode lifecycle policy            │
    │  fs/dcache.c              Dentry caching policy             │
    │  fs/super.c               Superblock management policy      │
    │                                                              │
    │  inode_operations         Per-inode policies                │
    │  super_operations         Per-filesystem policies           │
    │  dentry_operations        Name resolution policies          │
    └─────────────────────────────────────────────────────────────┘
                          │
                          ▼
    ┌─────────────────────────────────────────────────────────────┐
    │  DATA LAYER:                                                │
    │                                                              │
    │  fs/ext4/                 ext4 storage mechanism            │
    │  fs/xfs/                  XFS storage mechanism             │
    │  fs/nfs/                  NFS network storage               │
    │  drivers/block/           Block device drivers              │
    │                                                              │
    │  address_space_operations Page cache interface              │
    │  block_device_operations  Block device interface            │
    └─────────────────────────────────────────────────────────────┘
```

### Local Reasoning

```
+------------------------------------------------------------------+
|  LOCAL REASONING: UNDERSTAND ONE LAYER WITHOUT OTHERS            |
+------------------------------------------------------------------+

    WITH PROPER PDD:
    
    ┌─────────────────────────────────────────────────────────────┐
    │  "I can understand inode_operations WITHOUT knowing:        │
    │   - Which syscall invoked it                                │
    │   - Which filesystem implements it                          │
    │   - How the data is stored on disk"                         │
    └─────────────────────────────────────────────────────────────┘

    ┌─────────────────────────────────────────────────────────────┐
    │  "I can understand ext4_file_write() WITHOUT knowing:       │
    │   - Which process called write()                            │
    │   - What the file contains semantically                     │
    │   - How VFS chose this filesystem"                          │
    └─────────────────────────────────────────────────────────────┘

    THIS ENABLES:
    - Debugging one layer in isolation
    - Testing without full system
    - Modifying without ripple effects
    - Parallel development
```

### Change Isolation

```
+------------------------------------------------------------------+
|  CHANGE ISOLATION: CHANGES STAY IN ONE LAYER                     |
+------------------------------------------------------------------+

    PRESENTATION CHANGE:
    "Add a new syscall for batch reads"
    
    ┌─────────────────────────────────────────────────────────────┐
    │  AFFECTED:    Presentation layer (new syscall handler)      │
    │  UNAFFECTED:  Domain (same inode_operations)                │
    │  UNAFFECTED:  Data (same address_space_operations)          │
    └─────────────────────────────────────────────────────────────┘

    DOMAIN CHANGE:
    "Add mandatory access control"
    
    ┌─────────────────────────────────────────────────────────────┐
    │  AFFECTED:    Domain (permission checks)                    │
    │  UNAFFECTED:  Presentation (still calls same interface)     │
    │  UNAFFECTED:  Data (doesn't know about access control)      │
    └─────────────────────────────────────────────────────────────┘

    DATA CHANGE:
    "Add new filesystem type (btrfs)"
    
    ┌─────────────────────────────────────────────────────────────┐
    │  AFFECTED:    Data layer (new implementation)               │
    │  UNAFFECTED:  Domain (same inode_operations interface)      │
    │  UNAFFECTED:  Presentation (doesn't know filesystem type)   │
    └─────────────────────────────────────────────────────────────┘
```

---

## Summary: The Engineering Value of PDD

```
+------------------------------------------------------------------+
|  WHY PDD EXISTS — THE ENGINEERING PROBLEMS IT SOLVES             |
+------------------------------------------------------------------+

    PROBLEM 1: Cascading Changes
    ┌─────────────────────────────────────────────────────────────┐
    │  Without PDD: "I changed the database schema, now I have   │
    │               to change 50 files across the codebase"       │
    │                                                              │
    │  With PDD:    "I changed the database schema, only the     │
    │               data layer needed updates"                     │
    └─────────────────────────────────────────────────────────────┘

    PROBLEM 2: Testing Difficulty
    ┌─────────────────────────────────────────────────────────────┐
    │  Without PDD: "I can't test business logic without a       │
    │               running database and network server"          │
    │                                                              │
    │  With PDD:    "Domain layer tests run in milliseconds       │
    │               with mock interfaces"                          │
    └─────────────────────────────────────────────────────────────┘

    PROBLEM 3: Parallel Development
    ┌─────────────────────────────────────────────────────────────┐
    │  Without PDD: "We can't start on the API until the         │
    │               database team finishes their work"            │
    │                                                              │
    │  With PDD:    "Teams work on interfaces, integrate later"  │
    └─────────────────────────────────────────────────────────────┘

    PROBLEM 4: Technology Lock-in
    ┌─────────────────────────────────────────────────────────────┐
    │  Without PDD: "We're stuck with MySQL forever because      │
    │               business logic is full of SQL"                │
    │                                                              │
    │  With PDD:    "Swap PostgreSQL implementation, domain      │
    │               logic unchanged"                               │
    └─────────────────────────────────────────────────────────────┘
```

**中文总结：**
- **展示层**：将外部输入适配为内部表示，将内部结果转换为外部格式
- **领域层**：包含所有业务规则、策略、不变量——系统的"大脑"
- **数据层**：提供存储、网络、硬件访问的机制——不知道业务含义
- **依赖方向**：只能向下流动，绝不能反向
- **核心价值**：变更隔离、可测试性、可替换性、并行开发

