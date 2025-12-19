# Object-Oriented Design Patterns in the Linux Kernel

A deep architectural analysis of how Linux kernel v3.2 implements OO design patterns in plain C.

---

## Table of Contents

1. [Define "Object" in Kernel Terms](#step-1--define-object-in-kernel-terms)
2. [Core Kernel OO Building Blocks](#step-2--core-kernel-oo-building-blocks)
3. [Top-Down Architectural View](#step-3--top-down-architectural-view)
4. [Pattern Catalog (Conceptual)](#step-4--pattern-catalog-conceptual)
5. [10+ Real Kernel Examples](#step-5--10-real-kernel-examples-top--down)
6. [Inheritance via Embedding](#step-6--inheritance-via-embedding)
7. [Polymorphism and Substitution](#step-7--polymorphism-and-substitution)
8. [Object Lifetime Management](#step-8--object-lifetime-management)
9. [Why This Is NOT "Just OOP in C"](#step-9--why-this-is-not-just-oop-in-c)
10. [Architecture Lessons to Internalize](#step-10--architecture-lessons-to-internalize)

---

## Step 1 — Define "Object" in Kernel Terms

### What Qualifies as an "Object" in Linux Kernel

```
┌──────────────────────────────────────────────────────────────────────────────┐
│                    KERNEL "OBJECT" ANATOMY                                    │
├──────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│    C++ Object                           Kernel "Object"                      │
│    ──────────                           ───────────────                      │
│                                                                              │
│    ┌─────────────────┐                  ┌─────────────────┐                  │
│    │ class File {    │                  │ struct file {   │                  │
│    │   // data       │                  │   // data       │                  │
│    │   int fd;       │                  │   loff_t f_pos; │                  │
│    │   char *buf;    │                  │   void *private;│                  │
│    │                 │                  │                 │                  │
│    │   // vtable ptr │  ───────────►    │   // ops ptr    │                  │
│    │   virtual read()│                  │   f_op ─────────┼──► file_operations│
│    │   virtual write()                  │                 │    {.read=...}   │
│    │ };              │                  │ };              │                  │
│    └─────────────────┘                  └─────────────────┘                  │
│                                                                              │
│    KEY DIFFERENCES:                                                          │
│    ├── C++: vtable hidden, compiler-managed                                  │
│    ├── Kernel: ops table explicit, manually managed                          │
│    ├── C++: constructor/destructor automatic                                 │
│    ├── Kernel: alloc/init/release explicit                                   │
│    ├── C++: inheritance via class hierarchy                                  │
│    └── Kernel: inheritance via struct embedding                              │
│                                                                              │
└──────────────────────────────────────────────────────────────────────────────┘
```

**说明:**
- 内核"对象"由 struct（数据）+ ops table（方法）组成
- 与 C++ 不同，vtable 是显式的（`f_op` 指针），不是编译器隐藏的
- 构造/析构需要显式调用，不是语言自动处理
- 继承通过结构体嵌入实现，而非类层次

### The Five Pillars of Kernel Objects

| Pillar | C++ Equivalent | Kernel Implementation | Example |
|--------|---------------|----------------------|---------|
| **Encapsulation** | private/public | Opaque structs, static functions | `struct sock` internals hidden |
| **Data** | Member variables | struct fields | `struct file.f_pos` |
| **Methods** | Virtual functions | Function pointer tables | `file_operations.read` |
| **Inheritance** | class extends | struct embedding | `tcp_sock` embeds `inet_sock` |
| **Polymorphism** | virtual dispatch | `obj->ops->method(obj)` | `file->f_op->read(file,...)` |

### struct as Object

```c
/* include/linux/fs.h - A kernel "object" */
struct file {
    /* === OBJECT STATE (data members) === */
    union {
        struct list_head    fu_list;
        struct rcu_head     fu_rcuhead;
    } f_u;
    struct path             f_path;
    const struct file_operations *f_op;  /* [KEY] "vtable" pointer */
    spinlock_t              f_lock;
    atomic_long_t           f_count;     /* [KEY] Reference count */
    unsigned int            f_flags;
    fmode_t                 f_mode;
    loff_t                  f_pos;       /* Current file position */
    struct fown_struct      f_owner;
    const struct cred       *f_cred;
    struct file_ra_state    f_ra;
    void                    *private_data; /* [KEY] Implementation-specific data */
    struct address_space    *f_mapping;
    /* ... */
};
```

### ops Tables as vtables

```c
/* include/linux/fs.h - The "vtable" */
struct file_operations {
    struct module *owner;
    
    /* [KEY] "Virtual methods" - function pointers */
    loff_t (*llseek) (struct file *, loff_t, int);
    ssize_t (*read) (struct file *, char __user *, size_t, loff_t *);
    ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *);
    unsigned int (*poll) (struct file *, struct poll_table_struct *);
    long (*unlocked_ioctl) (struct file *, unsigned int, unsigned long);
    int (*mmap) (struct file *, struct vm_area_struct *);
    int (*open) (struct inode *, struct file *);
    int (*flush) (struct file *, fl_owner_t id);
    int (*release) (struct inode *, struct file *);
    int (*fsync) (struct file *, loff_t, loff_t, int datasync);
    /* ... ~30 operations total */
};
```

### Embedded Base Objects

```
INHERITANCE VIA EMBEDDING:

    ┌───────────────────────────────────────┐
    │ struct tcp_sock                       │
    │   ┌─────────────────────────────────┐ │
    │   │ struct inet_connection_sock     │ │ ◄── "inherits" from
    │   │   ┌───────────────────────────┐ │ │
    │   │   │ struct inet_sock          │ │ │ ◄── "inherits" from
    │   │   │   ┌─────────────────────┐ │ │ │
    │   │   │   │ struct sock         │ │ │ │ ◄── "base class"
    │   │   │   └─────────────────────┘ │ │ │
    │   │   └───────────────────────────┘ │ │
    │   └─────────────────────────────────┘ │
    │   u16 tcp_header_len;                 │ ◄── TCP-specific fields
    │   u32 rcv_nxt;                        │
    │   u32 snd_nxt;                        │
    │   ...                                 │
    └───────────────────────────────────────┘
    
    UPCASTING:   (struct sock *)tcp_sk  →  Always safe (first member)
    DOWNCASTING: tcp_sk(sk)             →  container_of or simple cast
```

**说明:**
- TCP socket "继承" inet_connection_sock，后者"继承" inet_sock，再"继承" sock
- 基类 struct 作为派生类的第一个成员嵌入
- 向上转型安全（指针值不变）
- 向下转型使用 `container_of` 或直接强制转换

### Private Data Ownership

```c
/* Two patterns for private data */

/* Pattern 1: Explicit private pointer */
struct file {
    void *private_data;  /* Implementation stores its data here */
};

/* Pattern 2: Embedding (container_of) */
struct ext4_inode_info {
    __u32 i_dtime;
    __u32 i_flags;
    /* ... ext4-specific fields ... */
    struct inode vfs_inode;  /* Base class embedded */
};

/* Recovery */
#define EXT4_I(inode) container_of(inode, struct ext4_inode_info, vfs_inode)
```

### Lifetime Management

```
OBJECT LIFECYCLE IN KERNEL:

    ┌────────────────┐
    │   ALLOCATION   │  kmalloc / kmem_cache_alloc / alloc_*
    └───────┬────────┘
            ▼
    ┌────────────────┐
    │ INITIALIZATION │  *_init() / *_setup() / callback
    └───────┬────────┘
            ▼
    ┌────────────────┐
    │   ACTIVE USE   │  reference count > 0
    │   get() / put()│  atomic_inc / atomic_dec_and_test
    └───────┬────────┘
            ▼
    ┌────────────────┐
    │    RELEASE     │  release callback (ops->release)
    └───────┬────────┘
            ▼
    ┌────────────────┐
    │  DESTRUCTION   │  kfree / kmem_cache_free / RCU callback
    └────────────────┘
```

**说明:**
- 内核对象生命周期完全显式管理
- 引用计数决定何时可以释放
- 释放回调（如 `file_operations.release`）允许实现清理资源
- 延迟销毁（RCU）用于无锁读取场景

---

## Step 2 — Core Kernel OO Building Blocks

### Building Block 1: `struct + ops`

```c
/* THE FUNDAMENTAL PATTERN */

/* "Class definition" */
struct device {                              /* Object data */
    struct device *parent;
    const char *init_name;
    struct bus_type *bus;
    struct device_driver *driver;
    void *platform_data;
    /* ... */
};

struct device_driver {                       /* "vtable" */
    const char *name;
    struct bus_type *bus;
    struct module *owner;
    
    int (*probe)(struct device *dev);        /* "Virtual method" */
    int (*remove)(struct device *dev);       /* "Virtual method" */
    void (*shutdown)(struct device *dev);    /* "Virtual method" */
    int (*suspend)(struct device *dev, pm_message_t state);
    int (*resume)(struct device *dev);
    /* ... */
};

/* Polymorphic call */
if (dev->driver && dev->driver->probe)
    ret = dev->driver->probe(dev);  /* obj->vtable->method(obj) */
```

### Building Block 2: `container_of`

```c
/* include/linux/kernel.h */
#define container_of(ptr, type, member) ({                      \
    const typeof(((type *)0)->member) *__mptr = (ptr);          \
    (type *)((char *)__mptr - offsetof(type, member));          \
})

/*
 * WHAT IT DOES:
 * Given a pointer to a member, return pointer to containing struct
 *
 * MEMORY VISUALIZATION:
 *
 * Address 0x1000: struct ext4_inode_info
 *                   ├── i_dtime         @ offset 0
 *                   ├── i_flags         @ offset 4
 *                   ├── ...
 *                   └── vfs_inode       @ offset 128  ◄── ptr points here
 *
 * container_of(ptr, struct ext4_inode_info, vfs_inode)
 *   = ptr - offsetof(struct ext4_inode_info, vfs_inode)
 *   = 0x1000 + 128 - 128
 *   = 0x1000  (start of ext4_inode_info)
 */
```

### Building Block 3: Embedding for Inheritance

```c
/* INHERITANCE HIERARCHY: sock → inet_sock → inet_connection_sock → tcp_sock */

struct sock {
    /* Common socket state */
    unsigned int        sk_padding : 2;
    unsigned int        sk_no_check : 2;
    unsigned int        sk_userlocks : 4;
    unsigned int        sk_protocol : 8;
    unsigned int        sk_type : 16;
    /* ... ~80 fields ... */
};

struct inet_sock {
    struct sock         sk;           /* [KEY] Base class embedded */
    __be32              inet_saddr;   /* Source address */
    __be32              inet_daddr;   /* Destination address */
    __be16              inet_sport;   /* Source port */
    __be16              inet_dport;   /* Destination port */
    /* ... */
};

struct inet_connection_sock {
    struct inet_sock    icsk_inet;    /* [KEY] Base class embedded */
    __u8                icsk_ca_state;
    struct request_sock_queue icsk_accept_queue;
    const struct tcp_congestion_ops *icsk_ca_ops;  /* Strategy pattern! */
    /* ... */
};

struct tcp_sock {
    struct inet_connection_sock inet_conn;  /* [KEY] Base class embedded */
    u16     tcp_header_len;
    u32     rcv_nxt;     /* Next sequence number expected */
    u32     snd_nxt;     /* Next sequence number to send */
    u32     snd_una;     /* First unacknowledged byte */
    /* ... ~100 TCP-specific fields ... */
};
```

### Building Block 4: Function Pointers for Polymorphism

```c
/* POLYMORPHIC DISPATCH */

/* The "interface" */
struct proto_ops {
    int  (*bind)(struct socket *sock, struct sockaddr *myaddr, int sockaddr_len);
    int  (*connect)(struct socket *sock, struct sockaddr *vaddr, int sockaddr_len, int flags);
    int  (*sendmsg)(struct kiocb *iocb, struct socket *sock, struct msghdr *m, size_t len);
    int  (*recvmsg)(struct kiocb *iocb, struct socket *sock, struct msghdr *m, size_t len, int flags);
    /* ... */
};

/* "Implementation 1" - TCP */
const struct proto_ops inet_stream_ops = {
    .bind      = inet_bind,
    .connect   = inet_stream_connect,
    .sendmsg   = tcp_sendmsg,
    .recvmsg   = tcp_recvmsg,
    /* ... */
};

/* "Implementation 2" - UDP */
const struct proto_ops inet_dgram_ops = {
    .bind      = inet_bind,
    .connect   = inet_dgram_connect,
    .sendmsg   = udp_sendmsg,
    .recvmsg   = udp_recvmsg,
    /* ... */
};

/* Caller doesn't know which implementation */
static inline int sock_sendmsg(struct socket *sock, struct msghdr *msg, size_t len)
{
    /* Polymorphic call - behavior depends on socket type */
    return sock->ops->sendmsg(NULL, sock, msg, len);
}
```

### Building Block 5: Reference Counting

```c
/* include/linux/fs.h */
struct file {
    atomic_long_t f_count;  /* Reference count */
    /* ... */
};

/* Get a reference */
static inline struct file *get_file(struct file *f)
{
    atomic_long_inc(&f->f_count);  /* Increment count */
    return f;
}

/* Release a reference */
void fput(struct file *file)
{
    if (atomic_long_dec_and_test(&file->f_count)) {
        /* Last reference - schedule destruction */
        /* Calls file->f_op->release() then frees memory */
        __fput(file);
    }
}

/*
 * INVARIANTS:
 * 1. Object valid while count > 0
 * 2. get() must be called while you have a valid reference
 * 3. put() must be called exactly once for each get()
 * 4. After put(), don't access the object
 */
```

### Summary: OO Invariants in Kernel

| Invariant | Description | Violation Consequence |
|-----------|-------------|----------------------|
| **First-member embedding** | Base struct at offset 0 for safe casting | Pointer corruption |
| **ops validity** | ops pointer set before use | NULL dereference crash |
| **Reference counting balance** | get/put calls balanced | Use-after-free or leak |
| **Allocation matching** | kmalloc with kfree, cache with cache_free | Memory corruption |
| **Locking discipline** | Lock order respected | Deadlock |

---

## Step 3 — Top-Down Architectural View

### The Three-Layer OO Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           KERNEL OO ARCHITECTURE                             │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  LAYER 1: CORE INFRASTRUCTURE (Defines patterns, rarely changes)            │
│  ═══════════════════════════════════════════════════════════════            │
│                                                                             │
│    ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐      │
│    │ kobject     │  │ kref        │  │ list_head   │  │ work_struct │      │
│    │ (object     │  │ (reference  │  │ (container) │  │ (deferred   │      │
│    │  identity)  │  │  counting)  │  │             │  │  execution) │      │
│    └─────────────┘  └─────────────┘  └─────────────┘  └─────────────┘      │
│                                                                             │
│  LAYER 2: SUBSYSTEM FRAMEWORKS (Defines interfaces, stable)                 │
│  ═══════════════════════════════════════════════════════════                │
│                                                                             │
│    VFS                  Network              Block           Device Model   │
│    ┌─────────────┐      ┌─────────────┐      ┌──────────┐   ┌────────────┐ │
│    │ file_ops    │      │ proto_ops   │      │ blk_ops  │   │ driver_ops │ │
│    │ inode_ops   │      │ net_dev_ops │      │ req_ops  │   │ bus_ops    │ │
│    │ super_ops   │      │ sk_proto    │      │          │   │            │ │
│    └─────────────┘      └─────────────┘      └──────────┘   └────────────┘ │
│                                                                             │
│  LAYER 3: IMPLEMENTATIONS (Many implementations, volatile)                  │
│  ═══════════════════════════════════════════════════════                    │
│                                                                             │
│    ext4_ops             tcp_prot             sd_fops        e1000_driver   │
│    nfs_ops              udp_prot             nvme_fops      usb_driver     │
│    proc_ops             icmp_prot            loop_fops      pci_driver     │
│    tmpfs_ops            sctp_prot            ...            ...            │
│    ...                  ...                                                 │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘

DEPENDENCY DIRECTION:
    Layer 3 ──depends on──► Layer 2 ──depends on──► Layer 1
    (Implementations)       (Frameworks)            (Infrastructure)

CHANGE FREQUENCY:
    Layer 1: Rarely changes (core abstractions)
    Layer 2: Occasionally changes (new interfaces)
    Layer 3: Frequently changes (new drivers, filesystems)
```

**说明:**
- 第一层定义核心基础设施（`kobject`, `kref` 等），极少变化
- 第二层定义子系统框架（VFS, 网络栈等），定义稳定接口
- 第三层是具体实现（ext4, TCP, 设备驱动等），频繁变化
- 依赖方向从下到上：实现依赖框架，框架依赖基础设施

### Pattern Repetition Across Layers

```
SAME PATTERN, DIFFERENT DOMAINS:

VFS LAYER:                          NETWORK LAYER:
┌───────────────────────┐           ┌───────────────────────┐
│ struct inode          │           │ struct sock           │
│   i_op ───────────────┼──┐        │   sk_prot ────────────┼──┐
│   i_fop ──────────────┼──┤        │                       │  │
│   i_sb ───────────────┼──┤        │                       │  │
└───────────────────────┘  │        └───────────────────────┘  │
                           │                                   │
           ┌───────────────┘                   ┌───────────────┘
           ▼                                   ▼
┌───────────────────────┐           ┌───────────────────────┐
│ struct inode_operations│          │ struct proto          │
│   .lookup              │          │   .connect            │
│   .create              │          │   .sendmsg            │
│   .unlink              │          │   .recvmsg            │
│   .mkdir               │          │   .close              │
└───────────────────────┘           └───────────────────────┘

BLOCK LAYER:                        DEVICE MODEL:
┌───────────────────────┐           ┌───────────────────────┐
│ struct gendisk        │           │ struct device         │
│   fops ───────────────┼──┐        │   driver ─────────────┼──┐
│   queue ──────────────┼──┤        │   bus ────────────────┼──┤
│                       │  │        │                       │  │
└───────────────────────┘  │        └───────────────────────┘  │
                           │                                   │
           ┌───────────────┘                   ┌───────────────┘
           ▼                                   ▼
┌───────────────────────┐           ┌───────────────────────┐
│ struct block_device_  │           │ struct device_driver  │
│        operations     │           │   .probe              │
│   .open               │           │   .remove             │
│   .release            │           │   .shutdown           │
│   .ioctl              │           │                       │
└───────────────────────┘           └───────────────────────┘
```

**说明:**
- 同样的模式（struct + ops）在不同子系统重复出现
- VFS: inode + inode_operations
- Network: sock + proto
- Block: gendisk + block_device_operations
- Device Model: device + device_driver
- 这不是偶然——是统一的架构设计原则

---

## Step 4 — Pattern Catalog (Conceptual)

### Classical OO Patterns → Kernel Equivalents

```
┌───────────────────────────────────────────────────────────────────────────────┐
│                      OO PATTERN MAPPING: C++ → KERNEL                          │
├───────────────────────────────────────────────────────────────────────────────┤
│                                                                               │
│  PATTERN              C++ APPROACH              KERNEL APPROACH               │
│  ───────────────────────────────────────────────────────────────────────────  │
│                                                                               │
│  INTERFACE            class IReader {           struct file_operations {      │
│                         virtual read() = 0;       ssize_t (*read)(...);       │
│                       };                        };                            │
│                                                                               │
│  ABSTRACT CLASS       class AbstractSocket {    struct proto {                │
│                         virtual connect()=0;      int (*connect)(...);        │
│                         void helper() {...}       // helpers in .c file       │
│                       };                        };                            │
│                                                                               │
│  STRATEGY             class CongestionAlgo;     struct tcp_congestion_ops {   │
│                       socket.setAlgo(algo);       void (*cong_avoid)(...);    │
│                                                 };                            │
│                                                 icsk->icsk_ca_ops = &cubic;   │
│                                                                               │
│  TEMPLATE METHOD      class Protocol {          struct proto {                │
│                         final void process() {    // proto->connect calls     │
│                           beforeHook();           // common code, then        │
│                           doWork();               // protocol-specific        │
│                           afterHook();          };                            │
│                         }                       // inet_stream_connect calls  │
│                       };                        // tcp_v4_connect internally  │
│                                                                               │
│  FACTORY              static File* create();    struct file_system_type {     │
│                                                   struct dentry *(*mount)();  │
│                                                 };                            │
│                                                                               │
│  ADAPTER              class USBSerial :         struct usb_serial_driver {    │
│                         public Serial,            struct tty_operations *ops; │
│                         private USB                // adapts USB to TTY       │
│                       {};                       };                            │
│                                                                               │
│  COMPOSITE            class Directory {         struct dentry {               │
│                         vector<Entry> children;   struct list_head d_subdirs; │
│                       };                        };                            │
│                                                                               │
│  STATE                enum State;               sk->sk_state = TCP_ESTABLISHED│
│                       state.handle(event);      // tcp_rcv_state_process()    │
│                                                 // switches on sk_state       │
│                                                                               │
│  OBSERVER             subject.notify();         struct notifier_block {       │
│                                                   int (*notifier_call)(...);  │
│                                                 };                            │
│                                                 register_netdevice_notifier() │
│                                                                               │
│  RAII                 ~File() { close(); }      file->f_op->release() called  │
│                                                 when f_count reaches 0        │
│                                                                               │
└───────────────────────────────────────────────────────────────────────────────┘
```

### Pattern Details

#### Pattern 1: Interface

```c
/* Pure interface - all function pointers, no data processing in struct */
struct file_operations {
    loff_t (*llseek) (struct file *, loff_t, int);
    ssize_t (*read) (struct file *, char __user *, size_t, loff_t *);
    ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *);
    /* ... */
};

/* Multiple implementations */
const struct file_operations ext4_file_operations = { .read = ext4_file_read, ... };
const struct file_operations nfs_file_operations = { .read = nfs_file_read, ... };
const struct file_operations proc_file_operations = { .read = proc_file_read, ... };
```

#### Pattern 2: Strategy

```c
/* Strategy interface */
struct tcp_congestion_ops {
    u32 (*ssthresh)(struct sock *sk);
    void (*cong_avoid)(struct sock *sk, u32 ack, u32 in_flight);
    void (*set_state)(struct sock *sk, u8 new_state);
    void (*pkts_acked)(struct sock *sk, u32 num_acked, s32 rtt_us);
    char name[TCP_CA_NAME_MAX];
};

/* Concrete strategies */
struct tcp_congestion_ops tcp_reno = { .name = "reno", .cong_avoid = tcp_reno_cong_avoid };
struct tcp_congestion_ops cubictcp = { .name = "cubic", .cong_avoid = bictcp_cong_avoid };
struct tcp_congestion_ops tcp_vegas = { .name = "vegas", .cong_avoid = vegas_cong_avoid };

/* Runtime strategy selection */
icsk->icsk_ca_ops = &cubictcp;  /* Use CUBIC algorithm */

/* Strategy invocation */
icsk->icsk_ca_ops->cong_avoid(sk, ack, in_flight);  /* Polymorphic */
```

#### Pattern 3: Factory

```c
/* Factory interface */
struct file_system_type {
    const char *name;
    struct dentry *(*mount)(struct file_system_type *, int,
                            const char *, void *);  /* Factory method */
    void (*kill_sb)(struct super_block *);
    struct module *owner;
};

/* Concrete factories */
static struct file_system_type ext4_fs_type = {
    .name    = "ext4",
    .mount   = ext4_mount,  /* Creates ext4 superblock */
    .kill_sb = kill_block_super,
};

static struct file_system_type nfs_fs_type = {
    .name    = "nfs",
    .mount   = nfs_mount,   /* Creates NFS superblock */
    .kill_sb = nfs_kill_super,
};

/* Factory invocation */
struct dentry *root = type->mount(type, flags, name, data);
```

#### Pattern 4: Observer (Notifier Chain)

```c
/* Observer pattern via notifier chains */
struct notifier_block {
    int (*notifier_call)(struct notifier_block *, unsigned long, void *);
    struct notifier_block *next;
    int priority;
};

/* Register observer */
int register_netdevice_notifier(struct notifier_block *nb);

/* Notify all observers */
int call_netdevice_notifiers(unsigned long val, struct net_device *dev)
{
    return raw_notifier_call_chain(&netdev_chain, val, dev);
}

/* Example observer */
static struct notifier_block my_notifier = {
    .notifier_call = my_netdev_event,
};
register_netdevice_notifier(&my_notifier);
```

### Patterns That DON'T Appear (and Why)

| Pattern | Why Not in Kernel |
|---------|-------------------|
| **Singleton** | Global variables serve this role; no need for runtime enforcement |
| **Prototype** | Objects created via explicit allocation, not cloning |
| **Builder** | Initialization is explicit, step-by-step, not via builder chain |
| **Decorator** | Composition via embedding; wrapping would add overhead |
| **Flyweight** | Memory management too critical; sharing must be explicit |
| **Proxy** | Direct function calls preferred; RPC-like patterns rare |

**说明:**
- 内核避免使用会增加运行时开销或复杂性的模式
- 单例由全局变量实现，无需复杂的运行时保证
- 装饰器模式会增加间接调用开销
- 享元模式要求共享透明，但内核需要显式控制内存

---

## Step 5 — 10+ Real Kernel Examples (Top → Down)

### Example 1: `struct file` + `file_operations`

**Domain:** Virtual File System (VFS)  
**Role:** File I/O abstraction

```c
/* === THE OBJECT === */
/* include/linux/fs.h */
struct file {
    struct path             f_path;
    const struct file_operations *f_op;  /* [KEY] "vtable" */
    atomic_long_t           f_count;     /* Reference count */
    unsigned int            f_flags;
    fmode_t                 f_mode;
    loff_t                  f_pos;       /* Current position */
    void                    *private_data;
};

/* === THE "VTABLE" === */
struct file_operations {
    struct module *owner;
    loff_t (*llseek) (struct file *, loff_t, int);
    ssize_t (*read) (struct file *, char __user *, size_t, loff_t *);
    ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *);
    int (*open) (struct inode *, struct file *);
    int (*release) (struct inode *, struct file *);
    /* ... ~30 operations ... */
};

/* === POLYMORPHIC BEHAVIOR === */
/* fs/read_write.c */
ssize_t vfs_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
    /* Polymorphic dispatch - behavior depends on file type */
    if (file->f_op->read)
        return file->f_op->read(file, buf, count, pos);
    else
        return do_sync_read(file, buf, count, pos);
}

/* === IMPLEMENTATIONS === */
/* fs/ext4/file.c */
const struct file_operations ext4_file_operations = {
    .llseek     = ext4_llseek,
    .read       = do_sync_read,
    .write      = do_sync_write,
    .aio_read   = generic_file_aio_read,
    .aio_write  = ext4_file_write,
    .open       = ext4_file_open,
    .release    = ext4_release_file,
    .mmap       = ext4_file_mmap,
    .fsync      = ext4_sync_file,
};

/* fs/nfs/file.c */
const struct file_operations nfs_file_operations = {
    .llseek     = nfs_file_llseek,
    .read       = do_sync_read,
    .write      = do_sync_write,
    .aio_read   = nfs_file_read,
    .aio_write  = nfs_file_write,
    .open       = nfs_file_open,
    .release    = nfs_file_release,
    .mmap       = nfs_file_mmap,
    .fsync      = nfs_file_fsync,
};
```

**OO Analysis:**
| Aspect | Description |
|--------|-------------|
| **Object** | `struct file` instance |
| **Base Class** | N/A (file is a base) |
| **Virtual Methods** | `read`, `write`, `open`, `release`, `mmap`, etc. |
| **Polymorphism** | Same `vfs_read()` call invokes different implementations |
| **Encapsulation** | VFS core doesn't know about ext4 or NFS internals |

---

### Example 2: `struct inode` + `inode_operations`

**Domain:** VFS metadata  
**Role:** File/directory metadata abstraction

```c
/* === THE OBJECT === */
struct inode {
    umode_t                 i_mode;
    uid_t                   i_uid;
    gid_t                   i_gid;
    const struct inode_operations *i_op;  /* [KEY] Metadata ops */
    const struct file_operations *i_fop;  /* [KEY] File I/O ops */
    struct super_block      *i_sb;
    loff_t                  i_size;
    struct timespec         i_atime;
    struct timespec         i_mtime;
    struct timespec         i_ctime;
    unsigned long           i_ino;
    atomic_t                i_count;
};

/* === THE "VTABLE" === */
struct inode_operations {
    struct dentry *(*lookup)(struct inode *, struct dentry *, struct nameidata *);
    int (*create)(struct inode *, struct dentry *, int, struct nameidata *);
    int (*link)(struct dentry *, struct inode *, struct dentry *);
    int (*unlink)(struct inode *, struct dentry *);
    int (*symlink)(struct inode *, struct dentry *, const char *);
    int (*mkdir)(struct inode *, struct dentry *, int);
    int (*rmdir)(struct inode *, struct dentry *);
    int (*rename)(struct inode *, struct dentry *, struct inode *, struct dentry *);
    int (*permission)(struct inode *, int);
    /* ... */
};

/* === POLYMORPHIC CALL === */
/* fs/namei.c */
static struct dentry *real_lookup(struct dentry *parent, struct qstr *name,
                                   struct nameidata *nd)
{
    struct inode *dir = parent->d_inode;
    /* Polymorphic: ext4_lookup, nfs_lookup, proc_lookup, etc. */
    result = dir->i_op->lookup(dir, dentry, nd);
    return result;
}
```

**OO Analysis:**
| Aspect | Description |
|--------|-------------|
| **Object** | `struct inode` instance |
| **Base Class** | N/A (inode is a base, but ext4_inode_info embeds it) |
| **Virtual Methods** | `lookup`, `create`, `unlink`, `mkdir`, `permission` |
| **Polymorphism** | Path resolution calls filesystem-specific lookup |
| **Encapsulation** | VFS path walking doesn't know directory format |

---

### Example 3: `struct super_block` + `super_operations`

**Domain:** Filesystem instance  
**Role:** Mounted filesystem abstraction

```c
/* === THE OBJECT === */
struct super_block {
    struct list_head        s_list;
    dev_t                   s_dev;
    unsigned long           s_blocksize;
    const struct super_operations *s_op;  /* [KEY] */
    struct dentry           *s_root;
    struct file_system_type *s_type;
    void                    *s_fs_info;  /* Filesystem private data */
};

/* === THE "VTABLE" === */
struct super_operations {
    struct inode *(*alloc_inode)(struct super_block *sb);
    void (*destroy_inode)(struct inode *);
    void (*dirty_inode)(struct inode *, int flags);
    int (*write_inode)(struct inode *, struct writeback_control *);
    void (*put_super)(struct super_block *);
    int (*sync_fs)(struct super_block *sb, int wait);
    int (*statfs)(struct dentry *, struct kstatfs *);
    int (*remount_fs)(struct super_block *, int *, char *);
};

/* === FACTORY PATTERN === */
/* Inode allocation is a factory method */
struct inode *new_inode(struct super_block *sb)
{
    struct inode *inode;
    /* Factory call - returns ext4_inode_info, nfs_inode_info, etc. */
    if (sb->s_op->alloc_inode)
        inode = sb->s_op->alloc_inode(sb);
    else
        inode = alloc_inode(sb);
    return inode;
}
```

---

### Example 4: `struct net_device` + `net_device_ops`

**Domain:** Network interfaces  
**Role:** Network device abstraction

```c
/* === THE OBJECT === */
struct net_device {
    char                    name[IFNAMSIZ];
    const struct net_device_ops *netdev_ops;  /* [KEY] */
    const struct ethtool_ops *ethtool_ops;
    unsigned int            flags;
    unsigned int            mtu;
    unsigned char           dev_addr[MAX_ADDR_LEN];
    void                    *priv;  /* Driver private data */
};

/* === THE "VTABLE" === */
struct net_device_ops {
    int  (*ndo_open)(struct net_device *dev);
    int  (*ndo_stop)(struct net_device *dev);
    netdev_tx_t (*ndo_start_xmit)(struct sk_buff *skb, struct net_device *dev);
    void (*ndo_set_rx_mode)(struct net_device *dev);
    int  (*ndo_set_mac_address)(struct net_device *dev, void *addr);
    int  (*ndo_change_mtu)(struct net_device *dev, int new_mtu);
    struct net_device_stats *(*ndo_get_stats)(struct net_device *dev);
};

/* === POLYMORPHIC CALL === */
/* net/core/dev.c */
int dev_hard_start_xmit(struct sk_buff *skb, struct net_device *dev, ...)
{
    /* Polymorphic: e1000_xmit, virtio_xmit, loopback_xmit, etc. */
    rc = ops->ndo_start_xmit(skb, dev);
    return rc;
}

/* === IMPLEMENTATION: Intel e1000 === */
static const struct net_device_ops e1000_netdev_ops = {
    .ndo_open           = e1000_open,
    .ndo_stop           = e1000_close,
    .ndo_start_xmit     = e1000_xmit_frame,
    .ndo_set_rx_mode    = e1000_set_rx_mode,
    .ndo_set_mac_address = e1000_set_mac,
    .ndo_change_mtu     = e1000_change_mtu,
    .ndo_get_stats      = e1000_get_stats,
};
```

---

### Example 5: `struct socket` + `proto_ops`

**Domain:** BSD socket layer  
**Role:** Socket API abstraction

```c
/* === THE OBJECT === */
struct socket {
    socket_state            state;
    short                   type;
    unsigned long           flags;
    struct socket_wq __rcu  *wq;
    struct file             *file;
    struct sock             *sk;            /* Internal socket */
    const struct proto_ops  *ops;           /* [KEY] BSD socket ops */
};

/* === THE "VTABLE" === */
struct proto_ops {
    int  (*release)(struct socket *sock);
    int  (*bind)(struct socket *sock, struct sockaddr *myaddr, int sockaddr_len);
    int  (*connect)(struct socket *sock, struct sockaddr *vaddr, int sockaddr_len, int flags);
    int  (*accept)(struct socket *sock, struct socket *newsock, int flags);
    int  (*listen)(struct socket *sock, int len);
    int  (*sendmsg)(struct kiocb *iocb, struct socket *sock, struct msghdr *m, size_t len);
    int  (*recvmsg)(struct kiocb *iocb, struct socket *sock, struct msghdr *m, size_t len, int flags);
};

/* === IMPLEMENTATIONS === */
/* TCP sockets */
const struct proto_ops inet_stream_ops = {
    .family     = PF_INET,
    .bind       = inet_bind,
    .connect    = inet_stream_connect,
    .listen     = inet_listen,
    .accept     = inet_accept,
    .sendmsg    = tcp_sendmsg,
    .recvmsg    = tcp_recvmsg,
};

/* UDP sockets */
const struct proto_ops inet_dgram_ops = {
    .family     = PF_INET,
    .bind       = inet_bind,
    .connect    = inet_dgram_connect,
    .sendmsg    = udp_sendmsg,
    .recvmsg    = udp_recvmsg,
};
```

---

### Example 6: `struct sock` + `struct proto`

**Domain:** Transport protocol  
**Role:** Transport layer abstraction

```c
/* === THE OBJECT === */
struct sock {
    struct sock_common      __sk_common;
    #define sk_prot         __sk_common.skc_prot  /* [KEY] Protocol ops */
    socket_lock_t           sk_lock;
    struct sk_buff_head     sk_receive_queue;
    struct sk_buff_head     sk_write_queue;
    int                     sk_wmem_queued;
    atomic_t                sk_refcnt;  /* Reference count */
    /* ... ~100 fields ... */
};

/* === THE "VTABLE" === */
struct proto {
    void     (*close)(struct sock *sk, long timeout);
    int      (*connect)(struct sock *sk, struct sockaddr *uaddr, int addr_len);
    int      (*disconnect)(struct sock *sk, int flags);
    struct sock *(*accept)(struct sock *sk, int flags, int *err);
    int      (*sendmsg)(struct kiocb *iocb, struct sock *sk, struct msghdr *msg, size_t len);
    int      (*recvmsg)(struct kiocb *iocb, struct sock *sk, struct msghdr *msg, size_t len, int noblock, int flags, int *addr_len);
    void     (*hash)(struct sock *sk);
    void     (*unhash)(struct sock *sk);
    
    unsigned int        obj_size;    /* Size for allocation */
    char                name[32];
};

/* === IMPLEMENTATIONS === */
struct proto tcp_prot = {
    .name           = "TCP",
    .owner          = THIS_MODULE,
    .close          = tcp_close,
    .connect        = tcp_v4_connect,
    .disconnect     = tcp_disconnect,
    .accept         = inet_csk_accept,
    .sendmsg        = tcp_sendmsg,
    .recvmsg        = tcp_recvmsg,
    .hash           = inet_hash,
    .unhash         = inet_unhash,
    .obj_size       = sizeof(struct tcp_sock),  /* [KEY] Factory info */
};

struct proto udp_prot = {
    .name           = "UDP",
    .close          = udp_lib_close,
    .connect        = ip4_datagram_connect,
    .sendmsg        = udp_sendmsg,
    .recvmsg        = udp_recvmsg,
    .hash           = udp_lib_hash,
    .unhash         = udp_lib_unhash,
    .obj_size       = sizeof(struct udp_sock),
};
```

---

### Example 7: `struct gendisk` + `block_device_operations`

**Domain:** Block devices  
**Role:** Block device abstraction

```c
/* === THE OBJECT === */
struct gendisk {
    int major;
    int first_minor;
    int minors;
    char disk_name[DISK_NAME_LEN];
    const struct block_device_operations *fops;  /* [KEY] */
    struct request_queue *queue;
    void *private_data;
};

/* === THE "VTABLE" === */
struct block_device_operations {
    int (*open)(struct block_device *, fmode_t);
    int (*release)(struct gendisk *, fmode_t);
    int (*ioctl)(struct block_device *, fmode_t, unsigned, unsigned long);
    int (*media_changed)(struct gendisk *);
    int (*revalidate_disk)(struct gendisk *);
    int (*getgeo)(struct block_device *, struct hd_geometry *);
};

/* === IMPLEMENTATION: SCSI Disk === */
static const struct block_device_operations sd_fops = {
    .owner          = THIS_MODULE,
    .open           = sd_open,
    .release        = sd_release,
    .ioctl          = sd_ioctl,
    .media_changed  = sd_media_changed,
    .revalidate_disk = sd_revalidate_disk,
    .getgeo         = sd_getgeo,
};
```

---

### Example 8: `struct device` + `device_driver`

**Domain:** Driver model  
**Role:** Device/driver binding

```c
/* === THE OBJECT === */
struct device {
    struct device           *parent;
    struct device_private   *p;
    struct kobject          kobj;
    const char              *init_name;
    struct device_type      *type;
    struct bus_type         *bus;
    struct device_driver    *driver;       /* [KEY] Bound driver */
    void                    *platform_data;
    void                    *driver_data;
};

/* === THE "VTABLE" === */
struct device_driver {
    const char              *name;
    struct bus_type         *bus;
    struct module           *owner;
    
    int (*probe)(struct device *dev);      /* [KEY] "Constructor" */
    int (*remove)(struct device *dev);     /* [KEY] "Destructor" */
    void (*shutdown)(struct device *dev);
    int (*suspend)(struct device *dev, pm_message_t state);
    int (*resume)(struct device *dev);
};

/* === BUS OPERATIONS === */
struct bus_type {
    const char              *name;
    int (*match)(struct device *dev, struct device_driver *drv);
    int (*probe)(struct device *dev);
    int (*remove)(struct device *dev);
};

/* === POLYMORPHIC BINDING === */
static int driver_probe_device(struct device_driver *drv, struct device *dev)
{
    /* Polymorphic: calls driver-specific probe */
    if (drv->probe)
        ret = drv->probe(dev);
    return ret;
}
```

---

### Example 9: `struct tty_struct` + `tty_operations`

**Domain:** Terminal devices  
**Role:** TTY abstraction

```c
/* === THE OBJECT === */
struct tty_struct {
    int magic;
    struct kref kref;
    struct device *dev;
    struct tty_driver *driver;
    const struct tty_operations *ops;  /* [KEY] */
    int index;
    struct tty_ldisc *ldisc;  /* Line discipline - Strategy pattern */
    void *driver_data;
};

/* === THE "VTABLE" === */
struct tty_operations {
    int  (*open)(struct tty_struct *tty, struct file *filp);
    void (*close)(struct tty_struct *tty, struct file *filp);
    int  (*write)(struct tty_struct *tty, const unsigned char *buf, int count);
    int  (*write_room)(struct tty_struct *tty);
    void (*flush_buffer)(struct tty_struct *tty);
    int  (*chars_in_buffer)(struct tty_struct *tty);
    int  (*ioctl)(struct tty_struct *tty, unsigned int cmd, unsigned long arg);
    void (*set_termios)(struct tty_struct *tty, struct ktermios *old);
    void (*throttle)(struct tty_struct *tty);
    void (*unthrottle)(struct tty_struct *tty);
};

/* === IMPLEMENTATION: Serial Port === */
static const struct tty_operations serial8250_ops = {
    .open       = serial8250_open,
    .close      = serial8250_close,
    .write      = serial8250_write,
    .write_room = serial8250_write_room,
    .chars_in_buffer = serial8250_chars_in_buffer,
    .flush_buffer = serial8250_flush_buffer,
    .ioctl      = serial8250_ioctl,
    .set_termios = serial8250_set_termios,
};
```

---

### Example 10: `struct timer_list` + Callback Model

**Domain:** Timer subsystem  
**Role:** Deferred execution

```c
/* === THE OBJECT === */
struct timer_list {
    struct list_head entry;
    unsigned long expires;
    void (*function)(unsigned long);  /* [KEY] Callback */
    unsigned long data;               /* Callback argument */
    struct tvec_base *base;
};

/* === "VIRTUAL METHOD" IS THE CALLBACK === */
static void timer_callback(unsigned long data)
{
    struct my_device *dev = (struct my_device *)data;
    /* Handle timer expiry */
}

/* === USAGE === */
struct timer_list my_timer;

void setup_timer_example(void)
{
    init_timer(&my_timer);
    my_timer.function = timer_callback;  /* Set "virtual method" */
    my_timer.data = (unsigned long)my_device;
    my_timer.expires = jiffies + HZ;
    add_timer(&my_timer);
}

/* Timer subsystem calls: my_timer.function(my_timer.data) */
```

---

### Example 11: `struct work_struct` + Workqueue Model

**Domain:** Deferred work  
**Role:** Asynchronous execution

```c
/* === THE OBJECT === */
struct work_struct {
    atomic_long_t data;
    struct list_head entry;
    work_func_t func;  /* [KEY] The "virtual method" */
};

typedef void (*work_func_t)(struct work_struct *work);

/* === POLYMORPHIC EXECUTION === */
static void my_work_handler(struct work_struct *work)
{
    /* Recover container */
    struct my_device *dev = container_of(work, struct my_device, work);
    /* Do deferred work */
}

/* === USAGE === */
struct my_device {
    int state;
    struct work_struct work;  /* Embedded work struct */
};

void init_my_device(struct my_device *dev)
{
    INIT_WORK(&dev->work, my_work_handler);  /* Set callback */
}

void trigger_work(struct my_device *dev)
{
    schedule_work(&dev->work);  /* Queue for execution */
    /* Workqueue calls: dev->work.func(&dev->work) */
}
```

### Examples Summary Table

| # | Object | vtable | Key Methods | Pattern | Domain |
|---|--------|--------|-------------|---------|--------|
| 1 | `file` | `file_operations` | read, write, mmap | Interface | VFS I/O |
| 2 | `inode` | `inode_operations` | lookup, create, unlink | Interface | VFS metadata |
| 3 | `super_block` | `super_operations` | alloc_inode, statfs | Factory | FS instance |
| 4 | `net_device` | `net_device_ops` | ndo_start_xmit, ndo_open | Interface | Network |
| 5 | `socket` | `proto_ops` | sendmsg, recvmsg | Interface | BSD socket |
| 6 | `sock` | `proto` | connect, sendmsg | Interface | Transport |
| 7 | `gendisk` | `block_device_operations` | open, ioctl | Interface | Block |
| 8 | `device` | `device_driver` | probe, remove | Template | Driver model |
| 9 | `tty_struct` | `tty_operations` | write, ioctl | Interface | Terminal |
| 10 | `timer_list` | (callback) | function | Callback | Timer |
| 11 | `work_struct` | (callback) | func | Callback | Deferred |

---

## Step 6 — Inheritance via Embedding

### The Embedding Pattern

```
INHERITANCE VIA STRUCT EMBEDDING:

C++ Inheritance:                    Kernel Embedding:

class Base {                        struct base {
    int x;                              int x;
};                                  };

class Derived : Base {              struct derived {
    int y;                              struct base base;  // EMBEDDED
};                                      int y;
                                    };

MEMORY LAYOUT (identical):

    ┌────────────────┐              ┌────────────────┐
    │ x (from Base)  │              │ x (from base)  │
    ├────────────────┤              ├────────────────┤
    │ y (Derived)    │              │ y (derived)    │
    └────────────────┘              └────────────────┘

UPCASTING:
    Derived* d;                     struct derived *d;
    Base* b = d;     // implicit    struct base *b = &d->base;

DOWNCASTING:
    Base* b;                        struct base *b;
    Derived* d =                    struct derived *d =
        dynamic_cast<Derived*>(b);      container_of(b, struct derived, base);
```

**说明:**
- C++ 继承和内核嵌入产生相同的内存布局
- 向上转型：C++ 隐式，内核取成员地址
- 向下转型：C++ 用 dynamic_cast，内核用 container_of

### Real Kernel Example 1: Socket Hierarchy

```c
/* === BASE CLASS === */
struct sock {
    struct sock_common      __sk_common;
    unsigned int            sk_shutdown : 2;
    unsigned int            sk_no_check : 2;
    int                     sk_sndbuf;
    struct sk_buff_head     sk_receive_queue;
    /* ... ~80 common fields ... */
};

/* === DERIVED CLASS 1: inet_sock === */
struct inet_sock {
    struct sock             sk;           /* [BASE] embedded at offset 0 */
    __be32                  inet_saddr;   /* [DERIVED] source address */
    __be32                  inet_daddr;   /* [DERIVED] dest address */
    __be16                  inet_sport;   /* [DERIVED] source port */
    __be16                  inet_dport;   /* [DERIVED] dest port */
    __u8                    tos;
};

/* === DERIVED CLASS 2: inet_connection_sock (from inet_sock) === */
struct inet_connection_sock {
    struct inet_sock          icsk_inet;  /* [BASE] inet_sock embedded */
    __u8                      icsk_ca_state;
    struct request_sock_queue icsk_accept_queue;
    const struct tcp_congestion_ops *icsk_ca_ops;  /* Strategy! */
    /* ... */
};

/* === DERIVED CLASS 3: tcp_sock (from inet_connection_sock) === */
struct tcp_sock {
    struct inet_connection_sock inet_conn;  /* [BASE] embedded */
    
    /* TCP-specific fields */
    u16     tcp_header_len;
    u16     xmit_size_goal_segs;
    u32     rcv_nxt;        /* Next expected sequence */
    u32     snd_nxt;        /* Next sequence to send */
    u32     snd_una;        /* First unacknowledged */
    u32     snd_wnd;        /* Send window */
    u32     rcv_wnd;        /* Receive window */
    /* ... ~100 TCP fields ... */
};

/* === CASTING MACROS === */
static inline struct inet_sock *inet_sk(const struct sock *sk)
{
    return (struct inet_sock *)sk;  /* Safe: inet_sock.sk at offset 0 */
}

static inline struct tcp_sock *tcp_sk(const struct sock *sk)
{
    return (struct tcp_sock *)sk;   /* Safe: tcp_sock embeds sock at offset 0 */
}

static inline struct inet_connection_sock *inet_csk(const struct sock *sk)
{
    return (struct inet_connection_sock *)sk;
}

/* === USAGE IN CODE === */
void tcp_rcv_established(struct sock *sk, struct sk_buff *skb, ...)
{
    /* Downcast to TCP-specific structure */
    struct tcp_sock *tp = tcp_sk(sk);
    
    /* Access TCP-specific fields */
    if (after(TCP_SKB_CB(skb)->seq, tp->rcv_nxt)) {
        /* Out of order packet */
    }
    
    /* Update TCP state */
    tp->rcv_nxt = TCP_SKB_CB(skb)->end_seq;
}
```

### Real Kernel Example 2: Inode Hierarchy

```c
/* === BASE CLASS === */
struct inode {
    umode_t                 i_mode;
    uid_t                   i_uid;
    gid_t                   i_gid;
    const struct inode_operations *i_op;
    struct super_block      *i_sb;
    unsigned long           i_ino;
    atomic_t                i_count;
    /* ... many common fields ... */
};

/* === DERIVED CLASS: ext4_inode_info === */
struct ext4_inode_info {
    __le32  i_data[15];           /* Block pointers - ext4 specific */
    __u32   i_dtime;              /* Deletion time */
    __u32   i_flags;              /* ext4 flags */
    ext4_fsblk_t i_file_acl;      /* File ACL */
    ext4_group_t i_block_group;   /* Block group */
    
    /* ... many ext4 fields ... */
    
    struct inode vfs_inode;       /* [BASE] embedded (NOT at offset 0!) */
};

/* === CASTING MACRO === */
static inline struct ext4_inode_info *EXT4_I(struct inode *inode)
{
    /* Must use container_of because vfs_inode is NOT at offset 0 */
    return container_of(inode, struct ext4_inode_info, vfs_inode);
}

/* === USAGE === */
int ext4_file_open(struct inode *inode, struct file *file)
{
    /* Recover ext4-specific structure */
    struct ext4_inode_info *ei = EXT4_I(inode);
    
    /* Access ext4-specific fields */
    if (ei->i_flags & EXT4_ENCRYPT_FL) {
        /* Handle encrypted file */
    }
    
    return 0;
}

/* === ALLOCATION (Factory Pattern) === */
static struct inode *ext4_alloc_inode(struct super_block *sb)
{
    struct ext4_inode_info *ei;
    
    /* Allocate the DERIVED class */
    ei = kmem_cache_alloc(ext4_inode_cachep, GFP_NOFS);
    if (!ei)
        return NULL;
    
    /* Initialize ext4 fields */
    ei->i_reserved_data_blocks = 0;
    
    /* Return pointer to EMBEDDED base class */
    return &ei->vfs_inode;
}
```

### Inheritance Comparison

| Aspect | Socket Hierarchy | Inode Hierarchy |
|--------|-----------------|-----------------|
| Base position | First member (offset 0) | Not first member |
| Upcast | Implicit pointer conversion | Take address of member |
| Downcast | Simple cast | `container_of` required |
| Why different? | All sockets share sock* API | VFS allocates inode, FS extends |

---

## Step 7 — Polymorphism and Substitution

### Liskov Substitution Principle in Kernel

```
LISKOV SUBSTITUTION PRINCIPLE (LSP):

"Objects of a superclass should be replaceable with objects of
 a subclass without affecting program correctness."

KERNEL TRANSLATION:

"Any filesystem's file_operations can be used wherever
 file_operations is expected. The caller (VFS) makes assumptions
 that ALL implementations MUST honor."
```

### What Callers Assume

```c
/* VFS assumes about ANY file_operations implementation: */

ssize_t vfs_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
    /* ASSUMPTION 1: If read is non-NULL, it's callable */
    /* ASSUMPTION 2: Return value semantics are consistent */
    /*   - Positive: bytes read */
    /*   - Zero: EOF */
    /*   - Negative: -errno */
    /* ASSUMPTION 3: pos is updated correctly */
    /* ASSUMPTION 4: User buffer is accessed via copy_to_user */
    /* ASSUMPTION 5: Locking requirements documented in filesystems/Locking */
    
    if (file->f_op->read)
        ret = file->f_op->read(file, buf, count, pos);
    else
        ret = do_sync_read(file, buf, count, pos);
    
    /* ASSUMPTION 6: After return, no lingering references to buf */
    
    return ret;
}
```

### What Implementations Must Guarantee

```c
/* CORRECT implementation of file_operations.read */
static ssize_t myfs_file_read(struct file *file, char __user *buf,
                               size_t count, loff_t *pos)
{
    struct inode *inode = file->f_inode;
    struct myfs_inode_info *mi = MYFS_I(inode);
    ssize_t ret = 0;
    
    /* GUARANTEE 1: Handle user pointer correctly */
    if (!access_ok(VERIFY_WRITE, buf, count))
        return -EFAULT;
    
    /* GUARANTEE 2: Respect file position */
    if (*pos >= inode->i_size)
        return 0;  /* EOF - must return 0, not error */
    
    /* GUARANTEE 3: Don't read past EOF */
    if (*pos + count > inode->i_size)
        count = inode->i_size - *pos;
    
    /* GUARANTEE 4: Use copy_to_user for user buffer */
    if (copy_to_user(buf, mi->data + *pos, count))
        return -EFAULT;
    
    /* GUARANTEE 5: Update position */
    *pos += count;
    
    /* GUARANTEE 6: Return bytes read */
    return count;
}
```

### What Breaks When Assumptions Are Violated

| Violation | Consequence | Example |
|-----------|-------------|---------|
| Return wrong sign | Caller misinterprets result | Return `count` instead of `-EFAULT` |
| Don't update `*pos` | Infinite loop in caller | Forget `*pos += count` |
| Access user buf directly | Kernel oops | `memcpy(buf, ...)` instead of `copy_to_user` |
| Hold locks across return | Deadlock | Return with inode lock held |
| Return with incomplete state | Data corruption | Partial write not recorded |

### Implicit Contract Enforcement

```c
/* Contracts enforced by documentation and review, not compiler */

/* Documentation/filesystems/Locking documents: */
/*
 * ->read() MAY be called with inode->i_sem held
 * ->write() is called with inode->i_sem held
 * ->readdir() is called with inode->i_sem held
 * ->fsync() is called with inode->i_sem held (if filemap_fdatawrite used)
 */

/* Violation example - WRONG: */
static ssize_t bad_write(struct file *file, const char __user *buf,
                          size_t count, loff_t *pos)
{
    mutex_lock(&inode->i_mutex);  /* DEADLOCK! Already held by VFS */
    /* ... */
    mutex_unlock(&inode->i_mutex);
    return count;
}
```

---

## Step 8 — Object Lifetime Management

### The Lifetime Challenge

```
PROBLEM: When is it safe to free a kernel object?

    Thread A                Thread B               Thread C
        │                       │                      │
        ▼                       ▼                      ▼
    get_file(f)            get_file(f)            ───────────
        │                       │                      │
        ▼                       ▼                      ▼
    use file               use file               close(fd)
        │                       │                  fput(f)
        ▼                       ▼                      │
    fput(f)                    │                       ▼
        │                       ▼                  Last put?
        │                   fput(f)                    │
        ▼                       │                      ▼
    File still                  ▼                  Free file?
    needed?                 Last put?                  │
                                                       ▼
                                               When is it SAFE?

SOLUTION: Reference counting ensures object lives while anyone uses it.
```

### Reference Counting Pattern

```c
/* include/linux/kref.h */
struct kref {
    atomic_t refcount;
};

void kref_init(struct kref *kref)
{
    atomic_set(&kref->refcount, 1);  /* Start at 1 */
}

void kref_get(struct kref *kref)
{
    atomic_inc(&kref->refcount);     /* Increment */
}

int kref_put(struct kref *kref, void (*release)(struct kref *kref))
{
    if (atomic_dec_and_test(&kref->refcount)) {
        release(kref);               /* Call destructor */
        return 1;
    }
    return 0;
}

/* === USAGE EXAMPLE: struct file === */
struct file {
    atomic_long_t f_count;  /* Reference count */
    /* ... */
};

struct file *get_file(struct file *f)
{
    atomic_long_inc(&f->f_count);
    return f;
}

void fput(struct file *file)
{
    if (atomic_long_dec_and_test(&file->f_count))
        __fput(file);  /* Last reference - destroy */
}
```

### Explicit Release Callbacks

```c
/* Release callback allows implementation cleanup */

struct file_operations {
    /* ... */
    int (*release)(struct inode *, struct file *);
};

/* Called when last reference is dropped */
static void __fput(struct file *file)
{
    struct inode *inode = file->f_inode;
    
    /* Call implementation's cleanup */
    if (file->f_op && file->f_op->release)
        file->f_op->release(inode, file);  /* "Destructor" */
    
    /* Framework cleanup */
    path_put(&file->f_path);
    put_cred(file->f_cred);
    
    /* Free memory */
    kmem_cache_free(filp_cachep, file);
}

/* Implementation's release */
static int ext4_release_file(struct inode *inode, struct file *file)
{
    /* ext4-specific cleanup */
    if (ext4_test_inode_flag(inode, EXT4_INODE_EXTENTS))
        ext4_ext_release(inode);
    
    /* Truncate preallocated blocks if needed */
    ext4_discard_preallocations(inode);
    
    return 0;
}
```

### Delayed Destruction (RCU)

```c
/* RCU: Read-Copy-Update for lock-free reads */

/*
 * PROBLEM: Reader might be using object while writer frees it
 *
 * Thread A (reader)        Thread B (writer)
 *     │                         │
 *     ▼                         ▼
 * rcu_read_lock()           Delete from list
 *     │                         │
 *     ▼                         ▼
 * Access object             Want to free
 *     │                         │
 *     ▼                         ▼
 * rcu_read_unlock()         call_rcu(&obj->rcu, free_callback)
 *                               │
 *                               ▼
 *                           Wait for all readers
 *                               │
 *                               ▼
 *                           free_callback() called
 */

/* Example: Delayed file free */
struct file {
    union {
        struct list_head fu_list;
        struct rcu_head fu_rcuhead;  /* For RCU delayed free */
    } f_u;
    /* ... */
};

static void file_free_rcu(struct rcu_head *head)
{
    struct file *f = container_of(head, struct file, f_u.fu_rcuhead);
    kmem_cache_free(filp_cachep, f);
}

static void delayed_fput(struct file *file)
{
    /* Delay free until all RCU readers done */
    call_rcu(&file->f_u.fu_rcuhead, file_free_rcu);
}
```

### Lifetime Management Summary

```
KERNEL OBJECT LIFECYCLE:

    ┌─────────────────────────────────────────────────────────────┐
    │                        ALLOCATION                           │
    │  kmalloc() / kmem_cache_alloc() / alloc_*()                 │
    │  refcount = 1                                                │
    └───────────────────────────┬─────────────────────────────────┘
                                │
                                ▼
    ┌─────────────────────────────────────────────────────────────┐
    │                      INITIALIZATION                          │
    │  *_init() / callback / registration                          │
    │  Set ops pointer, initialize state                           │
    └───────────────────────────┬─────────────────────────────────┘
                                │
                                ▼
    ┌─────────────────────────────────────────────────────────────┐
    │                       ACTIVE USE                             │
    │  get() → refcount++                                          │
    │  put() → refcount--                                          │
    │  (refcount > 0: object guaranteed valid)                     │
    └───────────────────────────┬─────────────────────────────────┘
                                │
                                ▼
    ┌─────────────────────────────────────────────────────────────┐
    │                    RELEASE TRIGGER                           │
    │  Last put() makes refcount = 0                               │
    │  OR explicit close/remove                                    │
    └───────────────────────────┬─────────────────────────────────┘
                                │
                                ▼
    ┌─────────────────────────────────────────────────────────────┐
    │                   RELEASE CALLBACK                           │
    │  ops->release() called                                       │
    │  Implementation cleans up private data                       │
    └───────────────────────────┬─────────────────────────────────┘
                                │
                                ▼
    ┌─────────────────────────────────────────────────────────────┐
    │                      DESTRUCTION                             │
    │  Immediate: kfree() / kmem_cache_free()                      │
    │  Delayed: call_rcu() → RCU grace period → free               │
    └─────────────────────────────────────────────────────────────┘
```

---

## Step 9 — Why This Is NOT "Just OOP in C"

### Key Differences from C++ OOP

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    KERNEL OO vs C++ OOP COMPARISON                           │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ASPECT              C++ OOP                    KERNEL OO                   │
│  ─────────────────────────────────────────────────────────────────────────  │
│                                                                             │
│  VTABLE              Hidden, compiler-managed   Explicit, hand-managed      │
│                      Automatic dispatch         Manual NULL checks          │
│                                                                             │
│  INHERITANCE         Deep hierarchies common    Shallow (1-3 levels)        │
│                      class A : B : C : D        struct contains struct      │
│                                                                             │
│  CONSTRUCTION        Automatic constructors     Explicit init functions     │
│                      Guaranteed initialization  May forget initialization   │
│                                                                             │
│  DESTRUCTION         Automatic destructors      Explicit release callbacks  │
│                      Stack unwinding            Reference counting          │
│                                                                             │
│  EXCEPTIONS          throw/catch                Error return codes          │
│                      Stack unwinding            goto cleanup                │
│                                                                             │
│  RTTI                dynamic_cast, typeid       container_of, type fields   │
│                      Runtime type info          Compile-time known          │
│                                                                             │
│  MEMORY              new/delete                 kmalloc/kfree               │
│                      Smart pointers             Manual refcounting          │
│                                                                             │
│  TEMPLATES           std::vector<T>             Macro-based (rare)          │
│                      Type-safe generics         void* and casts             │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Why Inheritance Depth is Shallow

```
C++ DEEP HIERARCHY (Common):

    Animal
       │
       ├── Mammal
       │      │
       │      ├── Carnivore
       │      │      │
       │      │      └── Cat
       │      │           │
       │      │           └── HouseCat
       │      │                 │
       │      │                 └── PersianCat  (6 levels!)
       │      │
       │      └── Herbivore
       │
       └── Bird

KERNEL SHALLOW HIERARCHY (Typical):

    sock                     (base)
       │
       └── inet_sock         (level 1)
              │
              └── inet_connection_sock  (level 2)
                     │
                     └── tcp_sock       (level 3) ← Maximum!

WHY SHALLOW?
├── Deep hierarchies increase memory overhead
├── Deep hierarchies complicate cache behavior
├── Deep hierarchies make code harder to understand
├── Each level adds vtable indirection cost
└── Composition preferred over inheritance
```

### Why Composition Dominates

```c
/* COMPOSITION OVER INHERITANCE */

/* BAD: Deep inheritance for timer capability */
struct timed_socket : socket {           /* Level 1 */
    struct timer_list timer;
};
struct retrying_socket : timed_socket {  /* Level 2 */
    int retry_count;
};
struct tcp_socket : retrying_socket {    /* Level 3 */
    /* TCP-specific */
};

/* GOOD: Composition - embed timer where needed */
struct tcp_sock {
    struct inet_connection_sock inet_conn;  /* Inheritance (shallow) */
    
    /* COMPOSED capabilities: */
    struct timer_list retransmit_timer;     /* Has-a timer */
    struct timer_list delack_timer;         /* Has-a timer */
    struct timer_list keepalive_timer;      /* Has-a timer */
    
    /* TCP state */
    u32 rcv_nxt;
    /* ... */
};

/* Timers are COMPOSED, not inherited */
/* Each timer is independent, can be added/removed */
/* No "is-a" relationship needed */
```

### Why Runtime Polymorphism is Controlled

```c
/* CONTROLLED POLYMORPHISM */

/* 1. NULL checks before every call */
if (file->f_op && file->f_op->read)  /* Check before call */
    ret = file->f_op->read(file, buf, count, pos);
else
    ret = -EINVAL;  /* Or use default */

/* 2. Operations set once, rarely changed */
file->f_op = inode->i_fop;  /* Set at open time */
/* Never changes during file lifetime */

/* 3. Limited number of implementations known at compile time */
/* VFS knows: ext4_file_operations, nfs_file_operations, etc. */
/* All statically defined, no runtime-generated vtables */

/* 4. No virtual inheritance, no diamond problems */
/* No ambiguity about which implementation to call */

/* C++ UNCONTROLLED (by comparison): */
/* - Can override any virtual method */
/* - Can add new methods in subclasses */
/* - Virtual inheritance creates complexity */
/* - Multiple inheritance path ambiguity */
```

### Strictness: Why Kernel OO is Harder

| Aspect | C++ | Kernel | Why Kernel is Stricter |
|--------|-----|--------|------------------------|
| **Memory errors** | Possible crash | Kernel panic, security hole | No memory protection |
| **Null pointers** | Exception | Oops, possible exploit | No exception handling |
| **Type errors** | Compile error | Silent corruption | No RTTI |
| **Resource leaks** | Eventually GC'd | Permanent leak | No garbage collection |
| **Race conditions** | Data corruption | Kernel deadlock/crash | Preemptible kernel |
| **Stack overflow** | Process crash | Kernel stack overflow | 8KB kernel stack |

---

## Step 10 — Architecture Lessons to Internalize

### Lesson 1: OO Patterns Are Unavoidable at Scale

```
WHY OO PATTERNS EMERGE:

Problem: Linux supports:
├── 50+ filesystem types
├── 100s of network protocols
├── 1000s of device drivers
├── Multiple architectures

Without OO patterns:
├── VFS would have switch(fs_type) for every operation
├── Network stack would have if(protocol == TCP) everywhere
├── Device model would be unmanageable
├── Adding new driver = modify all callers

With OO patterns:
├── VFS calls file->f_op->read() - works for any filesystem
├── Network calls sk->sk_prot->sendmsg() - works for any protocol
├── Device model calls drv->probe() - works for any driver
├── New implementation = new ops table, zero caller changes
```

### Lesson 2: Discipline Through Convention

```
HOW LINUX KEEPS OO DISCIPLINED:

1. NAMING CONVENTIONS
   ├── struct xxx_operations  → vtable for xxx
   ├── xxx_init(), xxx_exit() → lifecycle functions
   ├── xxx_get(), xxx_put()   → reference counting
   └── xxx_register(), xxx_unregister() → subsystem registration

2. FILE LAYOUT
   ├── include/linux/xxx.h    → public interface
   ├── xxx/xxx_core.c         → core implementation
   ├── xxx/xxx_impl.c         → specific implementations
   └── Documentation/xxx/     → contract documentation

3. CODE REVIEW
   ├── Maintainers enforce patterns
   ├── Subsystem-specific requirements
   ├── Locking documentation required
   └── API stability expectations

4. BUILD-TIME CHECKS
   ├── Sparse: type checking
   ├── lockdep: lock order
   └── kasan/ubsan: memory safety
```

### Lesson 3: Rules That Prevent Architecture Collapse

```
ARCHITECTURAL RULES:

RULE 1: DEPENDENCY DIRECTION
─────────────────────────────
   Driver → Subsystem → Core
   ✓ Driver includes subsystem headers
   ✗ Subsystem never includes driver headers
   
RULE 2: INTERFACE STABILITY
─────────────────────────────
   ops structures rarely change
   New operations added at end
   Deprecated operations → NULL check
   
RULE 3: SINGLE RESPONSIBILITY
─────────────────────────────
   file_operations: file I/O only
   inode_operations: metadata only
   super_operations: filesystem instance only
   
RULE 4: NO UPWARD CALLS
─────────────────────────────
   ext4 never calls VFS internals
   TCP never calls socket internals
   Driver never calls subsystem internals
   
RULE 5: OWNERSHIP CLARITY
─────────────────────────────
   Framework owns object lifecycle
   Implementation owns private data
   Reference counting for shared ownership
   
RULE 6: EXPLICIT IS BETTER
─────────────────────────────
   No hidden vtables
   No implicit construction
   No automatic cleanup
   No exception magic
```

### Summary: The Kernel OO Philosophy

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         KERNEL OO PHILOSOPHY                                 │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  "We use OO CONCEPTS, not OO LANGUAGE FEATURES"                             │
│                                                                             │
│  WHAT WE USE:                     WHAT WE AVOID:                            │
│  ─────────────                    ──────────────                            │
│  Encapsulation                    Deep inheritance                          │
│  Polymorphism                     Runtime type info                         │
│  Composition                      Exceptions                                │
│  Interfaces (ops tables)          Virtual inheritance                       │
│  Factory methods                  Implicit construction                     │
│  Strategy pattern                 Automatic destruction                     │
│  Observer pattern                 Hidden vtables                            │
│  Reference counting               Smart pointers                            │
│                                                                             │
│  RESULT:                                                                    │
│  ────────                                                                   │
│  ├── 25+ million lines of maintainable code                                 │
│  ├── 1000s of contributors can add code safely                              │
│  ├── Subsystems remain decoupled                                            │
│  ├── Performance remains predictable                                        │
│  └── Architecture survives 30 years of evolution                            │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

**说明:**
- 内核使用 OO 概念，而非 OO 语言特性
- 显式优于隐式：没有隐藏的 vtable，没有自动构造/析构
- 浅继承 + 广泛组合：避免深层次类层次结构
- 严格的依赖方向：驱动依赖子系统，子系统依赖核心
- 命名约定和代码审查强制执行模式一致性
- 结果：25+ 百万行可维护代码，架构经受 30 年演进

---

## Appendix: Quick Reference

### Common OO Constructs in Kernel

| Construct | C++ | Kernel C |
|-----------|-----|----------|
| Class | `class Foo { ... };` | `struct foo { ... };` |
| Virtual method | `virtual void bar();` | `void (*bar)(struct foo *);` |
| vtable | Implicit | `struct foo_operations` |
| Constructor | `Foo() { ... }` | `foo_init(struct foo *)` |
| Destructor | `~Foo() { ... }` | `foo_ops.release()` or `foo_exit()` |
| Inheritance | `class Bar : Foo` | `struct bar { struct foo base; }` |
| Upcast | Implicit | `&derived->base` |
| Downcast | `dynamic_cast` | `container_of()` |
| Reference count | `shared_ptr` | `atomic_t` + `get()`/`put()` |
| Interface | `class IFoo = 0;` | `struct foo_operations` |
| Abstract | Pure virtual | NULL function pointer |
| Factory | Static factory method | `ops->alloc_*()` |

### ops Table Checklist

When defining a new ops table:

1. **Name**: `struct xxx_operations` or `struct xxx_ops`
2. **Owner field**: `struct module *owner` for module refcounting
3. **Function pointers**: All take object as first parameter
4. **Optional ops**: Callers check for NULL before calling
5. **Documentation**: Document locking requirements
6. **const instance**: Define as `const struct xxx_operations my_ops = {...};`

### Object Lifecycle Checklist

1. **Allocation**: `kmalloc`/`kmem_cache_alloc`
2. **Initialization**: Set all fields, set ops pointer
3. **Reference counting**: `atomic_t` field, `get()`/`put()` functions
4. **Registration**: Add to subsystem lists
5. **Active use**: Reference count > 0
6. **Unregistration**: Remove from lists
7. **Release callback**: Clean up private data
8. **Destruction**: `kfree`/`kmem_cache_free` or RCU delay

---

*This document analyzes OO patterns from Linux kernel v3.2. The architectural principles apply to modern kernels and large C systems in general.*

