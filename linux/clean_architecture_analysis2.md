# Clean Architecture in the Linux Kernel v3.2

## A Deep Analysis of Policy vs. Mechanism Separation

---

## 1. Executive Summary

The Linux Kernel v3.2 (codenamed "Saber-toothed Squirrel") is a 15-million-line C
codebase that, despite predating Robert C. Martin's formal articulation of Clean
Architecture by over a decade, embodies its core principles with remarkable
consistency across every major subsystem.

The kernel achieves this through a single, pervasive mechanism: **structs with
function pointers**. These structs serve as the C-language equivalent of
interfaces (or abstract base classes), enabling full **Dependency Inversion** in a
language with no native support for polymorphism.

### Core Architectural Pattern

```
+------------------------------------------------------------------+
|                                                                  |
|  OUTER CIRCLE: Frameworks & Drivers                              |
|  (drivers/, arch/, security/selinux/, fs/ext4/, net/ipv4/)       |
|                                                                  |
|  +------------------------------------------------------------+  |
|  |                                                            |  |
|  |  INTERFACE ADAPTERS: Abstraction Structs                   |  |
|  |  (struct sched_class, struct file_operations,              |  |
|  |   struct proto_ops, struct security_operations,            |  |
|  |   struct clocksource, struct irq_chip)                     |  |
|  |                                                            |  |
|  |  +------------------------------------------------------+  |  |
|  |  |                                                      |  |  |
|  |  |  USE CASES: Core Subsystem Logic                     |  |  |
|  |  |  (kernel/sched.c, mm/memory.c, fs/read_write.c,      |  |  |
|  |  |   net/socket.c, security/security.c,                 |  |  |
|  |  |   kernel/time/timekeeping.c)                         |  |  |
|  |  |                                                      |  |  |
|  |  |  +------------------------------------------------+  |  |  |
|  |  |  |                                                |  |  |  |
|  |  |  |  ENTITIES: Core Data Structures                |  |  |  |
|  |  |  |  (struct task_struct, struct mm_struct,        |  |  |  |
|  |  |  |   struct inode, struct sock, struct irq_desc)  |  |  |  |
|  |  |  |                                                |  |  |  |
|  |  |  +------------------------------------------------+  |  |  |
|  |  +------------------------------------------------------+  |  |
|  +------------------------------------------------------------+  |
+------------------------------------------------------------------+
```

### The Dependency Rule in Practice

In every subsystem, dependencies flow **inward**:

- **Drivers depend on Interfaces**, never the reverse.
- **Core Logic depends on Abstractions** (function pointer structs), not on
  concrete implementations.
- **Execution flows outward** (syscall -> core -> driver), but **dependency flows
  inward** (driver implements interface <- core defines interface).

This is why the kernel can support 20+ filesystems, 4+ scheduling policies,
dozens of network protocols, and thousands of hardware drivers -- all while
keeping the core stable and the subsystems independently evolvable.

---

## 2. Subsystem Deep Dives

---

### 2.1 Process Management: The Scheduler

**Source:** `kernel/sched.c`, `kernel/sched_fair.c`, `kernel/sched_rt.c`,
`kernel/sched_stoptask.c`, `kernel/sched_idletask.c`

#### Layer Mapping

| Clean Architecture Layer | Kernel Component | Key File(s) |
|---|---|---|
| **Entity** | `struct task_struct` | `include/linux/sched.h:1220` |
| **Use Case** | Core scheduler loop (`__schedule()`, `pick_next_task()`) | `kernel/sched.c` |
| **Interface Adapter** | `struct sched_class` | `include/linux/sched.h:1084` |
| **Framework/Driver** | CFS, RT, Stop, Idle scheduling policies | `kernel/sched_fair.c`, `kernel/sched_rt.c`, etc. |

#### The Abstraction: `struct sched_class`

Defined in `include/linux/sched.h` (lines 1084-1126), this struct is the kernel
scheduler's "interface":

```c
struct sched_class {
    const struct sched_class *next;

    void (*enqueue_task) (struct rq *rq, struct task_struct *p, int flags);
    void (*dequeue_task) (struct rq *rq, struct task_struct *p, int flags);
    void (*yield_task)   (struct rq *rq);
    bool (*yield_to_task)(struct rq *rq, struct task_struct *p, bool preempt);

    void (*check_preempt_curr)(struct rq *rq, struct task_struct *p, int flags);

    struct task_struct *(*pick_next_task)(struct rq *rq);
    void (*put_prev_task)(struct rq *rq, struct task_struct *p);

    void (*set_curr_task)(struct rq *rq);
    void (*task_tick)    (struct rq *rq, struct task_struct *p, int queued);
    void (*task_fork)    (struct task_struct *p);

    void (*switched_from)(struct rq *this_rq, struct task_struct *task);
    void (*switched_to)  (struct rq *this_rq, struct task_struct *task);
    void (*prio_changed) (struct rq *this_rq, struct task_struct *task,
                          int oldprio);

    unsigned int (*get_rr_interval)(struct rq *rq, struct task_struct *task);
    /* ... SMP and group scheduling callbacks ... */
};
```

The `next` pointer creates a **priority-ordered linked list** of scheduling
classes -- the kernel walks this chain to find the highest-priority runnable task:

```
stop_sched_class --> rt_sched_class --> fair_sched_class --> idle_sched_class --> NULL
   (highest)            (RT)               (CFS)               (lowest)
```

#### Code Evidence: The Core Scheduler is Policy-Agnostic

In `kernel/sched.c`, the core `pick_next_task()` function (lines 4368-4391) never
references CFS, RT, or any specific policy:

```c
static inline struct task_struct *
pick_next_task(struct rq *rq)
{
    const struct sched_class *class;
    struct task_struct *p;

    /* Optimization: fast path when all tasks are fair-class */
    if (likely(rq->nr_running == rq->cfs.h_nr_running)) {
        p = fair_sched_class.pick_next_task(rq);
        if (likely(p))
            return p;
    }

    for_each_class(class) {
        p = class->pick_next_task(rq);
        if (p)
            return p;
    }

    BUG(); /* the idle class will always have a runnable task */
}
```

The `for_each_class` macro (line 1910) walks the linked list:

```c
#define sched_class_highest (&stop_sched_class)
#define for_each_class(class) \
    for (class = sched_class_highest; class; class = class->next)
```

Similarly, `enqueue_task()` and `dequeue_task()` (lines 1943-1955) dispatch
through the pointer without knowledge of the underlying algorithm:

```c
static void enqueue_task(struct rq *rq, struct task_struct *p, int flags)
{
    update_rq_clock(rq);
    sched_info_queued(p);
    p->sched_class->enqueue_task(rq, p, flags);
}
```

#### Concrete Implementations (Plugins)

**CFS** (`kernel/sched_fair.c:5044`):

```c
static const struct sched_class fair_sched_class = {
    .next            = &idle_sched_class,
    .enqueue_task    = enqueue_task_fair,
    .dequeue_task    = dequeue_task_fair,
    .pick_next_task  = pick_next_task_fair,
    .put_prev_task   = put_prev_task_fair,
    .task_tick       = task_tick_fair,
    .task_fork       = task_fork_fair,
    /* ... */
};
```

**Real-Time** (`kernel/sched_rt.c:1803`):

```c
static const struct sched_class rt_sched_class = {
    .next            = &fair_sched_class,
    .enqueue_task    = enqueue_task_rt,
    .dequeue_task    = dequeue_task_rt,
    .pick_next_task  = pick_next_task_rt,
    .put_prev_task   = put_prev_task_rt,
    .task_tick       = task_tick_rt,
    /* ... */
};
```

#### Dependency Analysis

```
+---------------------+       +--------------------+
|  kernel/sched.c     |       | kernel/sched_fair.c|
|  (Core Scheduler)   |       | (CFS Plugin)       |
|                     |       |                    |
| calls:              |       | implements:        |
| p->sched_class      | <---- | fair_sched_class   |
|   ->pick_next_task()|       |   .pick_next_task  |
|   ->enqueue_task()  |       |   = pick_next_task |
|   ->task_tick()     |       |     _fair          |
+---------------------+       +--------------------+
         |                              |
         v                              v
  Depends on sched_class        Depends on sched_class
  (abstraction)                 (abstraction)
```

**The Scheduler depends on `struct sched_class`, not on `fair_sched_class` or
`rt_sched_class`.** The concrete implementations depend on the same abstraction.
This is textbook Dependency Inversion.

---

### 2.2 Memory Management: Virtual Memory vs. Physical Hardware

**Source:** `mm/memory.c`, `include/linux/mm_types.h`, `arch/x86/include/asm/pgtable.h`

#### Layer Mapping

| Clean Architecture Layer | Kernel Component | Key File(s) |
|---|---|---|
| **Entity** | `struct mm_struct`, `struct vm_area_struct` | `include/linux/mm_types.h` |
| **Use Case** | `handle_mm_fault()`, page allocation, VMA management | `mm/memory.c`, `mm/mmap.c` |
| **Interface Adapter** | `struct vm_operations_struct`, page table macros (`pgd_offset`, `pud_alloc`, `pmd_alloc`, `pte_alloc`) | `include/linux/mm.h`, `include/asm-generic/pgtable.h` |
| **Framework/Driver** | Architecture-specific page table implementations | `arch/x86/include/asm/pgtable.h`, `arch/arm/include/asm/pgtable.h` |

#### The Abstraction: Multi-Level Page Table Macros

The memory management subsystem achieves architecture independence through a
**two-pronged abstraction**:

**1. Page Table Macros** -- Architecture-specific page table operations are
hidden behind macros that have the same API on every architecture:

```c
/* arch/x86/include/asm/pgtable.h */
#define pgd_offset(mm, address) ((mm)->pgd + pgd_index((address)))

static inline pud_t *pud_offset(pgd_t *pgd, unsigned long address)
{
    return (pud_t *)pgd_page_vaddr(*pgd) + pud_index(address);
}

static inline pmd_t *pmd_offset(pud_t *pud, unsigned long address)
{
    return (pmd_t *)pud_page_vaddr(*pud) + pmd_index(address);
}
```

**2. `struct vm_operations_struct`** (`include/linux/mm.h:204`) -- This is the
function-pointer abstraction for memory-mapped regions:

```c
struct vm_operations_struct {
    void (*open)(struct vm_area_struct *area);
    void (*close)(struct vm_area_struct *area);
    int  (*fault)(struct vm_area_struct *vma, struct vm_fault *vmf);
    int  (*page_mkwrite)(struct vm_area_struct *vma, struct vm_fault *vmf);
    int  (*access)(struct vm_area_struct *vma, unsigned long addr,
                   void *buf, int len, int write);
    /* ... NUMA policies ... */
};
```

#### Code Evidence: Generic Fault Handler

`handle_mm_fault()` in `mm/memory.c` (lines 3442-3503) walks the page table
hierarchy using architecture-independent macros:

```c
int handle_mm_fault(struct mm_struct *mm, struct vm_area_struct *vma,
                    unsigned long address, unsigned int flags)
{
    pgd_t *pgd;
    pud_t *pud;
    pmd_t *pmd;
    pte_t *pte;

    __set_current_state(TASK_RUNNING);

    count_vm_event(PGFAULT);

    if (unlikely(is_vm_hugetlb_page(vma)))
        return hugetlb_fault(mm, vma, address, flags);

    pgd = pgd_offset(mm, address);           /* arch-specific macro */
    pud = pud_alloc(mm, pgd, address);       /* arch-specific alloc */
    pmd = pmd_alloc(mm, pud, address);       /* arch-specific alloc */
    /* ... */
    pte = pte_offset_map(pmd, address);      /* arch-specific map */

    return handle_pte_fault(mm, vma, address, pte, pmd, flags);
}
```

The function has **zero knowledge** of whether it is running on x86 with 4-level
page tables, ARM with 3-level page tables, or any other architecture. The macros
(`pgd_offset`, `pud_alloc`, `pmd_alloc`, `pte_offset_map`) resolve to
architecture-specific code at compile time.

#### Dependency Analysis

```
+-----------------------+       +---------------------------+
|  mm/memory.c          |       | arch/x86/include/asm/     |
|  (Generic MM)         |       |   pgtable.h               |
|                       |       | (x86 Page Tables)         |
|  Calls:               |       |                           |
|  pgd_offset(mm, addr) | ----> | #define pgd_offset(mm,    |
|  pud_alloc(mm, ...)   |       |   addr) ((mm)->pgd +      |
|  pmd_alloc(mm, ...)   |       |   pgd_index((addr)))      |
+-----------------------+       +---------------------------+
         |
         | The dependency is at compile time, not runtime.
         | mm/ uses a UNIFORM API; arch/ provides the
         | implementation via macros and inline functions.
```

This is a **compile-time Dependency Inversion**. The generic code in `mm/` is
written against an abstract page-table API. Each architecture provides its own
concrete implementation of that API through its `arch/*/include/asm/pgtable.h`.

---

### 2.3 File Systems: The Virtual File System (VFS)

**Source:** `fs/read_write.c`, `include/linux/fs.h`, `fs/ext4/`, `fs/nfs/`

#### Layer Mapping

| Clean Architecture Layer | Kernel Component | Key File(s) |
|---|---|---|
| **Entity** | `struct inode`, `struct dentry`, `struct super_block` | `include/linux/fs.h` |
| **Use Case** | `vfs_read()`, `vfs_write()`, `vfs_open()`, path lookup | `fs/read_write.c`, `fs/open.c`, `fs/namei.c` |
| **Interface Adapter** | `struct file_operations`, `struct inode_operations`, `struct super_operations` | `include/linux/fs.h` |
| **Framework/Driver** | ext4, NFS, procfs, sysfs, tmpfs, etc. | `fs/ext4/`, `fs/nfs/`, `fs/proc/` |

#### The Abstraction: `struct file_operations`

Defined in `include/linux/fs.h` (lines 1220-1245), this is one of the most
iconic interfaces in the kernel:

```c
struct file_operations {
    struct module *owner;
    loff_t  (*llseek)    (struct file *, loff_t, int);
    ssize_t (*read)      (struct file *, char __user *, size_t, loff_t *);
    ssize_t (*write)     (struct file *, const char __user *, size_t, loff_t *);
    ssize_t (*aio_read)  (struct kiocb *, const struct iovec *, unsigned long, loff_t);
    ssize_t (*aio_write) (struct kiocb *, const struct iovec *, unsigned long, loff_t);
    int     (*readdir)   (struct file *, void *, filldir_t);
    unsigned int (*poll) (struct file *, struct poll_table_struct *);
    long    (*unlocked_ioctl)(struct file *, unsigned int, unsigned long);
    int     (*mmap)      (struct file *, struct vm_area_struct *);
    int     (*open)      (struct inode *, struct file *);
    int     (*flush)     (struct file *, fl_owner_t id);
    int     (*release)   (struct inode *, struct file *);
    int     (*fsync)     (struct file *, loff_t, loff_t, int datasync);
    int     (*lock)      (struct file *, int, struct file_lock *);
    ssize_t (*sendpage)  (struct file *, struct page *, int, size_t, loff_t *, int);
    ssize_t (*splice_write)(struct pipe_inode_info *, struct file *, loff_t *,
                            size_t, unsigned int);
    ssize_t (*splice_read) (struct file *, loff_t *, struct pipe_inode_info *,
                            size_t, unsigned int);
    long    (*fallocate) (struct file *, int mode, loff_t offset, loff_t len);
    /* ... */
};
```

#### Code Evidence: VFS Dispatch

`vfs_read()` in `fs/read_write.c` (lines 358-384) demonstrates the dispatch:

```c
ssize_t vfs_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
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

The critical line is `file->f_op->read(file, buf, count, pos)`. This dispatches
to whichever filesystem implementation owns the file -- ext4, NFS, procfs, or
anything else -- without `vfs_read` having any knowledge of the filesystem type.

#### Concrete Implementations (Plugins)

**ext4** (`fs/ext4/file.c:230`):

```c
const struct file_operations ext4_file_operations = {
    .llseek      = ext4_llseek,
    .read        = do_sync_read,
    .write       = do_sync_write,
    .aio_read    = generic_file_aio_read,
    .aio_write   = ext4_file_write,
    .unlocked_ioctl = ext4_ioctl,
    .mmap        = ext4_file_mmap,
    .open        = ext4_file_open,
    .release     = ext4_release_file,
    .fsync       = ext4_sync_file,
    .splice_read = generic_file_splice_read,
    .splice_write= generic_file_splice_write,
    .fallocate   = ext4_fallocate,
};
```

**NFS** (`fs/nfs/file.c:610`):

```c
const struct file_operations nfs_file_operations = {
    .llseek      = nfs_file_llseek,
    .read        = do_sync_read,
    .write       = do_sync_write,
    .aio_read    = nfs_file_read,
    .aio_write   = nfs_file_write,
    .mmap        = nfs_file_mmap,
    .open        = nfs_file_open,
    .flush       = nfs_file_flush,
    .release     = nfs_file_release,
    .fsync       = nfs_file_fsync,
    .lock        = nfs_lock,
    .flock       = nfs_flock,
    .splice_read = nfs_file_splice_read,
    .splice_write= nfs_file_splice_write,
    .check_flags = nfs_check_flags,
    .setlease    = nfs_setlease,
};
```

The assignment happens during inode creation. For ext4, in `fs/ext4/inode.c`
(lines 3887-3891): `inode->i_fop = &ext4_file_operations;`

#### Dependency Analysis

```
+------------------------+
|  User Space            |
|  read(fd, buf, count)  |
+-----------+------------+
            |  syscall
            v
+------------------------+      +-----------------------+
| fs/read_write.c        |      | struct file_operations|
| vfs_read()             |----->| (Abstraction)         |
|                        |      +----------+------------+
| file->f_op->read(...)  |                 |
+------------------------+     +-----------+-----------+
                               |                       |
                    +----------+---+       +-----------+--+
                    | fs/ext4/     |       | fs/nfs/      |
                    | file.c       |       | file.c       |
                    | ext4_file_   |       | nfs_file_    |
                    |   operations |       |   operations |
                    +--------------+       +--------------+
```

**`vfs_read` depends on `struct file_operations`, not on `ext4_file_operations`
or `nfs_file_operations`.** Each filesystem "plugs in" by assigning its own
operations struct to `inode->i_fop`.

---

### 2.4 Network Stack: Socket Layer vs. Protocol Implementations

**Source:** `net/socket.c`, `include/linux/net.h`, `include/net/protocol.h`,
`net/ipv4/af_inet.c`

#### Layer Mapping

| Clean Architecture Layer | Kernel Component | Key File(s) |
|---|---|---|
| **Entity** | `struct socket`, `struct sock`, `struct sk_buff` | `include/linux/net.h`, `include/net/sock.h`, `include/linux/skbuff.h` |
| **Use Case** | Socket syscall handlers (`sock_sendmsg`, `sock_recvmsg`) | `net/socket.c` |
| **Interface Adapter** | `struct proto_ops`, `struct net_protocol`, `struct net_proto_family` | `include/linux/net.h`, `include/net/protocol.h` |
| **Framework/Driver** | TCP/IPv4, UDP/IPv4, IPv6, Unix sockets, etc. | `net/ipv4/af_inet.c`, `net/ipv4/tcp.c`, `net/ipv6/` |

#### The Abstraction: `struct proto_ops`

Defined in `include/linux/net.h` (lines 164-207):

```c
struct proto_ops {
    int  family;
    struct module *owner;
    int  (*release)  (struct socket *sock);
    int  (*bind)     (struct socket *sock, struct sockaddr *myaddr, int sockaddr_len);
    int  (*connect)  (struct socket *sock, struct sockaddr *vaddr,
                      int sockaddr_len, int flags);
    int  (*accept)   (struct socket *sock, struct socket *newsock, int flags);
    int  (*getname)  (struct socket *sock, struct sockaddr *addr,
                      int *sockaddr_len, int peer);
    unsigned int (*poll)(struct file *file, struct socket *sock,
                         struct poll_table_struct *wait);
    int  (*listen)   (struct socket *sock, int len);
    int  (*shutdown) (struct socket *sock, int flags);
    int  (*setsockopt)(struct socket *sock, int level, int optname,
                       char __user *optval, unsigned int optlen);
    int  (*getsockopt)(struct socket *sock, int level, int optname,
                       char __user *optval, int __user *optlen);
    int  (*sendmsg)  (struct kiocb *iocb, struct socket *sock,
                      struct msghdr *m, size_t total_len);
    int  (*recvmsg)  (struct kiocb *iocb, struct socket *sock,
                      struct msghdr *m, size_t total_len, int flags);
    int  (*mmap)     (struct file *file, struct socket *sock,
                      struct vm_area_struct *vma);
    ssize_t (*sendpage)(struct socket *sock, struct page *page,
                        int offset, size_t size, int flags);
    /* ... */
};
```

#### The Abstraction: `struct net_protocol`

Defined in `include/net/protocol.h` (lines 36-47), this handles the IP-layer
dispatch to the correct transport protocol:

```c
struct net_protocol {
    int  (*handler)(struct sk_buff *skb);
    void (*err_handler)(struct sk_buff *skb, u32 info);
    int  (*gso_send_check)(struct sk_buff *skb);
    struct sk_buff *(*gso_segment)(struct sk_buff *skb, u32 features);
    struct sk_buff **(*gro_receive)(struct sk_buff **head, struct sk_buff *skb);
    int  (*gro_complete)(struct sk_buff *skb);
    unsigned int no_policy:1,
                 netns_ok:1;
};
```

#### Code Evidence: Socket Layer Dispatch

`__sock_sendmsg_nosec()` in `net/socket.c` (lines 414-420):

```c
static inline int __sock_sendmsg_nosec(struct kiocb *iocb, struct socket *sock,
                                       struct msghdr *msg, size_t size)
{
    struct sock_iocb *si = kiocb_to_siocb(iocb);

    sock_update_classid(sock->sk);

    si->sock = sock;
    si->scm  = NULL;
    si->msg  = msg;
    si->size = size;

    return sock->ops->sendmsg(iocb, sock, msg, size);
}
```

The dispatch `sock->ops->sendmsg(...)` routes to whichever protocol owns this
socket, without `net/socket.c` knowing whether it is TCP, UDP, or Unix sockets.

#### Concrete Implementation: TCP/IPv4

In `net/ipv4/af_inet.c` (lines 908-934):

```c
const struct proto_ops inet_stream_ops = {
    .family      = PF_INET,
    .owner       = THIS_MODULE,
    .release     = inet_release,
    .bind        = inet_bind,
    .connect     = inet_stream_connect,
    .accept      = inet_accept,
    .getname     = inet_getname,
    .poll        = tcp_poll,
    .listen      = inet_listen,
    .shutdown    = inet_shutdown,
    .setsockopt  = sock_common_setsockopt,
    .getsockopt  = sock_common_getsockopt,
    .sendmsg     = inet_sendmsg,
    .recvmsg     = inet_recvmsg,
    .sendpage    = inet_sendpage,
    .splice_read = tcp_splice_read,
    /* ... */
};
```

Socket type registration via `inetsw_array[]` (lines 1002-1040):

```c
static struct inet_protosw inetsw_array[] =
{
    {
        .type     = SOCK_STREAM,
        .protocol = IPPROTO_TCP,
        .prot     = &tcp_prot,
        .ops      = &inet_stream_ops,
        .flags    = INET_PROTOSW_PERMANENT | INET_PROTOSW_ICSK,
    },
    {
        .type     = SOCK_DGRAM,
        .protocol = IPPROTO_UDP,
        .prot     = &udp_prot,
        .ops      = &inet_dgram_ops,
        /* ... */
    },
    /* ... */
};
```

#### The Multi-Layer Abstraction

The network stack has an unusually deep abstraction hierarchy:

```
  struct socket              (VFS-facing, user-visible)
      |
      +-- const struct proto_ops *ops;    (protocol-family operations)
      |
      +-- struct sock *sk;               (protocol state)
              |
              +-- struct proto *sk_prot;  (transport-level protocol ops)
```

`struct socket` (VFS layer) holds a pointer to `proto_ops` (socket operations)
and `sock` (protocol state). `struct sock` in turn holds `sk_prot`, which points
to transport-level operations like `tcp_prot` or `udp_prot`.

#### Dependency Analysis

```
+-----------------+      +----------------+      +------------------+
| net/socket.c    |      | struct         |      | net/ipv4/        |
| (Socket Layer)  |----->| proto_ops      |<-----| af_inet.c        |
|                 |      | (Abstraction)  |      | inet_stream_ops  |
| sock->ops->     |      +----------------+      | (TCP plugin)     |
|   sendmsg(...)  |                               +------------------+
+-----------------+                               | net/ipv4/        |
                                                  | udp.c            |
                                                  | inet_dgram_ops   |
                                                  | (UDP plugin)     |
                                                  +------------------+
```

---

### 2.5 Security: Linux Security Modules (LSM)

**Source:** `security/security.c`, `include/linux/security.h`,
`security/selinux/hooks.c`, `security/apparmor/lsm.c`

#### Layer Mapping

| Clean Architecture Layer | Kernel Component | Key File(s) |
|---|---|---|
| **Entity** | Security labels, credentials (`struct cred`) | `include/linux/cred.h` |
| **Use Case** | Security hook callsites throughout the kernel | `security/security.c` |
| **Interface Adapter** | `struct security_operations` | `include/linux/security.h:1380` |
| **Framework/Driver** | SELinux, AppArmor, SMACK, TOMOYO | `security/selinux/`, `security/apparmor/` |

#### The Abstraction: `struct security_operations`

This is the **purest example of Clean Architecture** in the kernel. Defined in
`include/linux/security.h` (lines 1380-1524+), it contains over **150 function
pointers** covering every security-sensitive operation in the kernel:

```c
struct security_operations {
    char name[SECURITY_NAME_MAX + 1];

    int (*ptrace_access_check)(struct task_struct *child, unsigned int mode);
    int (*ptrace_traceme)(struct task_struct *parent);
    int (*capable)(struct task_struct *tsk, const struct cred *cred,
                   struct user_namespace *ns, int cap, int audit);

    /* Binary execution */
    int (*bprm_set_creds)(struct linux_binprm *bprm);
    int (*bprm_check_security)(struct linux_binprm *bprm);

    /* Superblock / mount security */
    int (*sb_alloc_security)(struct super_block *sb);
    int (*sb_mount)(char *dev_name, struct path *path, char *type,
                    unsigned long flags, void *data);

    /* Inode security */
    int (*inode_alloc_security)(struct inode *inode);
    int (*inode_create)(struct inode *dir, struct dentry *dentry, int mode);
    int (*inode_permission)(struct inode *inode, int mask);

    /* File security */
    int (*file_permission)(struct file *file, int mask);
    int (*file_mmap)(struct file *file, unsigned long reqprot,
                     unsigned long prot, unsigned long flags,
                     unsigned long addr, unsigned long addr_only);

    /* ... 130+ more hooks ... */
};
```

#### Code Evidence: Security Hook Dispatch

In `security/security.c`, a global `security_ops` pointer dispatches every
security check. Example -- `security_file_permission()` (lines 519-528):

```c
int security_file_permission(struct file *file, int mask)
{
    int ret;
    ret = security_ops->file_permission(file, mask);
    if (ret)
        return ret;
    return fsnotify_perm(file, mask);
}
```

And `security_inode_create()` (lines 364-369):

```c
int security_inode_create(struct inode *dir, struct dentry *dentry, int mode)
{
    if (unlikely(IS_PRIVATE(dir)))
        return 0;
    return security_ops->inode_create(dir, dentry, mode);
}
```

These functions are called from throughout the kernel -- from VFS, from the
process manager, from the network stack -- without any of those subsystems
knowing which security module is active.

#### Registration Mechanism

`register_security()` in `security/security.c` (lines 112-126):

```c
int __init register_security(struct security_operations *ops)
{
    if (verify(ops)) {
        printk(KERN_DEBUG "%s could not verify "
               "security_operations structure.\n", __func__);
        return -EINVAL;
    }

    if (security_ops != &default_security_ops)
        return -EAGAIN;

    security_ops = ops;
    return 0;
}
```

`security_module_enable()` (lines 95-98) checks the boot parameter:

```c
int __init security_module_enable(struct security_operations *ops)
{
    return !strcmp(ops->name, chosen_lsm);
}
```

#### Concrete Implementations (Plugins)

**SELinux** (`security/selinux/hooks.c:5452`):

```c
static struct security_operations selinux_ops = {
    .name                = "selinux",
    .ptrace_access_check = selinux_ptrace_access_check,
    .capget              = selinux_capget,
    .capable             = selinux_capable,
    .file_permission     = selinux_file_permission,
    .inode_create        = selinux_inode_create,
    .inode_permission    = selinux_inode_permission,
    /* ... 150+ more ... */
};
```

Registration in `selinux_init()` (lines 5648-5674):

```c
static __init int selinux_init(void)
{
    if (!security_module_enable(&selinux_ops))
        return 0;
    if (register_security(&selinux_ops))
        panic("SELinux: Unable to register with kernel.\n");
    /* ... */
}
```

**AppArmor** (`security/apparmor/lsm.c:624`):

```c
static struct security_operations apparmor_ops = {
    .name                = "apparmor",
    .ptrace_access_check = apparmor_ptrace_access_check,
    /* ... */
};
```

#### Dependency Analysis

```
+---------------------------+
|  Kernel Core              |
|  (fs/, kernel/, net/)     |
|                           |
|  security_inode_create()  |
|  security_file_permission |
|  security_bprm_check()   |
+-----------+---------------+
            |  calls
            v
+-----------+---------------+       +-----------------------+
| security/security.c       |       | struct                |
| security_ops->inode_create| ----> | security_operations   |
| security_ops->file_perm   |       | (Abstraction)         |
+---------------------------+       +----------+------------+
                                               |
                              +----------------+----------------+
                              |                                 |
                   +----------+--------+             +----------+--------+
                   | security/selinux/ |             | security/apparmor/|
                   | hooks.c           |             | lsm.c             |
                   | selinux_ops       |             | apparmor_ops      |
                   +-------------------+             +-------------------+
```

**The entire kernel depends on `struct security_operations`, not on SELinux or
AppArmor.** The security module is a true **plugin** that can be swapped at
boot time via the `security=` parameter.

---

### 2.6 Device Drivers & Interrupts: The Outer Circle

**Source:** `drivers/base/platform.c`, `include/linux/platform_device.h`,
`include/linux/device.h`, `kernel/irq/`

#### Layer Mapping

| Clean Architecture Layer | Kernel Component | Key File(s) |
|---|---|---|
| **Entity** | `struct irq_desc`, `struct device` | `include/linux/irqdesc.h`, `include/linux/device.h` |
| **Use Case** | Generic IRQ handling, device model matching | `kernel/irq/handle.c`, `drivers/base/dd.c` |
| **Interface Adapter** | `struct irq_chip`, `struct bus_type`, `struct device_driver`, `irq_handler_t` | `include/linux/irq.h`, `include/linux/device.h` |
| **Framework/Driver** | Concrete drivers, interrupt controllers | `drivers/`, `arch/x86/kernel/apic/` |

#### The Driver Model Abstraction

The kernel's driver model uses a three-tier abstraction:

**`struct bus_type`** (`include/linux/device.h:86`) -- abstracts a bus:

```c
struct bus_type {
    const char *name;
    int (*match)(struct device *dev, struct device_driver *drv);
    int (*probe)(struct device *dev);
    int (*remove)(struct device *dev);
    void (*shutdown)(struct device *dev);
    int (*suspend)(struct device *dev, pm_message_t state);
    int (*resume)(struct device *dev);
    const struct dev_pm_ops *pm;
    /* ... */
};
```

**`struct device_driver`** (`include/linux/device.h:193`) -- base driver:

```c
struct device_driver {
    const char      *name;
    struct bus_type *bus;
    struct module   *owner;
    int  (*probe)   (struct device *dev);
    int  (*remove)  (struct device *dev);
    void (*shutdown)(struct device *dev);
    int  (*suspend) (struct device *dev, pm_message_t state);
    int  (*resume)  (struct device *dev);
    /* ... */
};
```

**`struct platform_driver`** (`include/linux/platform_device.h:164`) -- embeds
`device_driver`:

```c
struct platform_driver {
    int  (*probe)   (struct platform_device *);
    int  (*remove)  (struct platform_device *);
    void (*shutdown)(struct platform_device *);
    int  (*suspend) (struct platform_device *, pm_message_t state);
    int  (*resume)  (struct platform_device *);
    struct device_driver driver;
    const struct platform_device_id *id_table;
};
```

#### Registration: How Drivers Plug In

`platform_driver_register()` in `drivers/base/platform.c` (lines 467-482):

```c
int platform_driver_register(struct platform_driver *drv)
{
    drv->driver.bus = &platform_bus_type;
    if (drv->probe)
        drv->driver.probe = platform_drv_probe;
    if (drv->remove)
        drv->driver.remove = platform_drv_remove;
    if (drv->shutdown)
        drv->driver.shutdown = platform_drv_shutdown;

    return driver_register(&drv->driver);
}
```

The pattern is the same for PCI (`pci_register_driver`), USB
(`usb_register`), and every other bus type: the driver fills in a struct
with function pointers and registers it with the core.

#### The Interrupt Abstraction

**`irq_handler_t`** (`include/linux/interrupt.h:91`):

```c
typedef irqreturn_t (*irq_handler_t)(int, void *);
```

The kernel defines the **slot** (the function prototype); the driver fills it:

```c
/* Driver code */
request_irq(irq_num, my_interrupt_handler, IRQF_SHARED, "my_device", dev);
```

**`struct irq_chip`** (`include/linux/irq.h:267`) abstracts the interrupt
controller hardware:

```c
struct irq_chip {
    const char *name;
    unsigned int (*irq_startup)(struct irq_data *data);
    void (*irq_shutdown)(struct irq_data *data);
    void (*irq_enable)(struct irq_data *data);
    void (*irq_disable)(struct irq_data *data);
    void (*irq_ack)(struct irq_data *data);
    void (*irq_mask)(struct irq_data *data);
    void (*irq_unmask)(struct irq_data *data);
    void (*irq_eoi)(struct irq_data *data);
    int  (*irq_set_affinity)(struct irq_data *data,
                             const struct cpumask *dest, bool force);
    int  (*irq_set_type)(struct irq_data *data, unsigned int flow_type);
    /* ... */
};
```

#### The Dispatch Chain

```c
/* kernel/irq/irqdesc.c */
int generic_handle_irq(unsigned int irq)
{
    struct irq_desc *desc = irq_to_desc(irq);
    if (!desc)
        return -EINVAL;
    generic_handle_irq_desc(irq, desc);
    return 0;
}

/* include/linux/irqdesc.h */
static inline void generic_handle_irq_desc(unsigned int irq, struct irq_desc *desc)
{
    desc->handle_irq(irq, desc);
}
```

The flow: `generic_handle_irq()` -> `desc->handle_irq()` (a flow handler like
`handle_level_irq` or `handle_edge_irq`) -> `handle_irq_event()` ->
`action->handler(irq, dev_id)` (the driver's registered handler).

#### Dependency Analysis

```
+----------------------------+       +-------------------------+
| kernel/irq/handle.c        |       | drivers/net/e1000/      |
| (Generic IRQ Core)         |       | (NIC Driver)            |
|                            |       |                         |
| handle_irq_event():        |       | request_irq(irq,        |
|   action->handler(irq, ..) | <---- |   e1000_intr, ...)      |
|                            |       |                         |
| Uses:                      |       | Implements:             |
|   irq_handler_t prototype  |       |   e1000_intr() matching |
|   irq_chip for HW control  |       |   irq_handler_t         |
+----------------------------+       +-------------------------+
```

The kernel core **defines the slot**. The driver **fills the slot**. The core
never depends on any specific driver.

---

### 2.7 Time Management: Clock Events and Clock Sources

**Source:** `kernel/time/timekeeping.c`, `kernel/time/clockevents.c`,
`include/linux/clocksource.h`, `include/linux/clockchips.h`

#### Layer Mapping

| Clean Architecture Layer | Kernel Component | Key File(s) |
|---|---|---|
| **Entity** | `struct timekeeper` (internal state) | `kernel/time/timekeeping.c:24` |
| **Use Case** | `timekeeping_get_ns()`, `getnstimeofday()`, tick handling | `kernel/time/timekeeping.c` |
| **Interface Adapter** | `struct clocksource`, `struct clock_event_device` | `include/linux/clocksource.h`, `include/linux/clockchips.h` |
| **Framework/Driver** | TSC, HPET, ACPI PM Timer, PIT | `arch/x86/kernel/tsc.c`, `arch/x86/kernel/hpet.c` |

#### The Abstraction: `struct clocksource`

Defined in `include/linux/clocksource.h` (lines 166-201):

```c
struct clocksource {
    cycle_t (*read)(struct clocksource *cs);
    cycle_t cycle_last;
    cycle_t mask;
    u32 mult;
    u32 shift;
    u64 max_idle_ns;
    u32 maxadj;

    const char *name;
    struct list_head list;
    int rating;
    int  (*enable)(struct clocksource *cs);
    void (*disable)(struct clocksource *cs);
    unsigned long flags;
    void (*suspend)(struct clocksource *cs);
    void (*resume)(struct clocksource *cs);
};
```

The critical function pointer is `read()` -- the generic timekeeping code calls
this to get the current cycle count without knowing what hardware provides it.

#### The Abstraction: `struct clock_event_device`

Defined in `include/linux/clockchips.h` (lines 82-108):

```c
struct clock_event_device {
    void (*event_handler)(struct clock_event_device *);
    int  (*set_next_event)(unsigned long evt, struct clock_event_device *);
    int  (*set_next_ktime)(ktime_t expires, struct clock_event_device *);
    ktime_t next_event;
    u64 max_delta_ns;
    u64 min_delta_ns;
    u32 mult;
    u32 shift;
    enum clock_event_mode mode;
    void (*broadcast)(const struct cpumask *mask);
    void (*set_mode)(enum clock_event_mode mode,
                     struct clock_event_device *);
    const char *name;
    int rating;
    int irq;
    const struct cpumask *cpumask;
    struct list_head list;
};
```

#### Code Evidence: Generic Timekeeping

`timekeeping_get_ns()` in `kernel/time/timekeeping.c` (lines 105-119):

```c
static inline s64 timekeeping_get_ns(void)
{
    cycle_t cycle_now, cycle_delta;
    struct clocksource *clock;

    clock = timekeeper.clock;
    cycle_now = clock->read(clock);

    cycle_delta = (cycle_now - clock->cycle_last) & clock->mask;

    return clocksource_cyc2ns(cycle_delta, timekeeper.mult,
                              timekeeper.shift);
}
```

`clock->read(clock)` dispatches to whichever clocksource is currently active.
The generic code converts cycles to nanoseconds using `mult` and `shift` --
never touching hardware registers directly.

#### Concrete Implementations (Plugins)

**TSC** (`arch/x86/kernel/tsc.c:757`):

```c
static struct clocksource clocksource_tsc = {
    .name   = "tsc",
    .rating = 300,
    .read   = read_tsc,
    .resume = resume_tsc,
    .mask   = CLOCKSOURCE_MASK(64),
    .flags  = CLOCK_SOURCE_IS_CONTINUOUS | CLOCK_SOURCE_MUST_VERIFY,
};
```

**HPET** (`arch/x86/kernel/hpet.c:738`):

```c
static struct clocksource clocksource_hpet = {
    .name   = "hpet",
    .rating = 250,
    .read   = read_hpet,
    .mask   = HPET_MASK,
    .flags  = CLOCK_SOURCE_IS_CONTINUOUS,
    .resume = hpet_resume_counter,
};
```

The `rating` field enables automatic selection -- the kernel picks the
highest-rated available clocksource.

#### Dependency Analysis

```
+----------------------------+       +----------------------------+
| kernel/time/timekeeping.c  |       | arch/x86/kernel/tsc.c      |
| (Generic Timekeeping)      |       | (TSC Plugin)               |
|                            |       |                            |
| timekeeper.clock->read()   | <---- | clocksource_tsc            |
|                            |       |   .read = read_tsc         |
| Depends on:                |       |                            |
|   struct clocksource       |       | Depends on:                |
|   (abstraction)            |       |   struct clocksource       |
+----------------------------+       +----------------------------+
                                     +----------------------------+
                                     | arch/x86/kernel/hpet.c     |
                                     | (HPET Plugin)              |
                                     |                            |
                                     | clocksource_hpet           |
                                     |   .read = read_hpet        |
                                     +----------------------------+
```

---

## 3. Cross-Subsystem Comparison

### 3.1 Dependency Inversion Comparison Table

| Subsystem | Abstraction Struct | Key Function Pointer(s) | Core Consumer | Concrete Plugins | Registration Mechanism |
|---|---|---|---|---|---|
| **Scheduler** | `struct sched_class` | `pick_next_task`, `enqueue_task`, `task_tick` | `kernel/sched.c` | `fair_sched_class`, `rt_sched_class` | Compile-time linked list via `.next` pointer |
| **Memory Mgmt** | Page table macros + `struct vm_operations_struct` | `pgd_offset`, `pud_alloc`, `fault` | `mm/memory.c` | `arch/x86/`, `arch/arm/` | Compile-time macro substitution |
| **VFS** | `struct file_operations` | `read`, `write`, `open`, `mmap` | `fs/read_write.c` | `ext4_file_operations`, `nfs_file_operations` | Runtime: `inode->i_fop = &ops` |
| **Network** | `struct proto_ops` | `sendmsg`, `recvmsg`, `bind`, `connect` | `net/socket.c` | `inet_stream_ops`, `inet_dgram_ops` | Runtime: `sock->ops = &ops` in `inet_create()` |
| **Security** | `struct security_operations` | `file_permission`, `inode_create`, `bprm_check` | `security/security.c` | `selinux_ops`, `apparmor_ops` | Boot-time: `register_security()` |
| **Drivers/IRQ** | `struct irq_chip` + `irq_handler_t` | `irq_ack`, `irq_mask`, handler callback | `kernel/irq/handle.c` | Per-driver handlers, per-chip impls | Runtime: `request_irq()`, `irq_set_chip()` |
| **Timekeeping** | `struct clocksource` | `read` | `kernel/time/timekeeping.c` | `clocksource_tsc`, `clocksource_hpet` | Runtime: `clocksource_register_khz()` |

### 3.2 The Universal Pattern

Every subsystem follows the same structural pattern:

```
+-----------------------+       +-------------------------+       +----------------------+
| Core Logic            |       | Abstraction Struct      |       | Concrete Plugin      |
| (Use Case Layer)      | ----> | (Interface Adapter)     | <---- | (Framework/Driver)   |
|                       |       |                         |       |                      |
| Calls function ptrs   |       | Defines the contract    |       | Fills in the struct  |
| Does NOT #include     |       | Lives in include/linux/ |       | Lives in drivers/,   |
|   any plugin code     |       |   or include/net/       |       |   fs/, security/,    |
|                       |       |                         |       |   arch/              |
+-----------------------+       +-------------------------+       +----------------------+
```

### 3.3 Registration Timing Spectrum

```
Compile-time                                                    Runtime
     |                                                               |
     v                                                               v
  sched_class     Page table       security_ops    file_operations   irq_handler_t
  (linked list    macros           (boot param)    (mount time)      (request_irq)
   at compile)    (arch headers)                   clocksource       proto_ops
                                                   (probe time)      (socket create)
```

---

## 4. Execution vs. Dependency Trace

### Network Packet Arrival: From NIC Interrupt to User Space

This trace follows a TCP packet arriving at a network interface card, being
processed through the kernel, and delivered to a user-space `recv()` call.

#### Execution Flow (Bottom-Up)

```
  Hardware NIC generates interrupt
       |
  [1]  v
  +----------------------------+
  | arch/x86/kernel/entry_64.S |   Architecture-specific IRQ entry
  | do_IRQ()                   |
  +-------------+--------------+
                |
  [2]           v
  +----------------------------+
  | kernel/irq/handle.c        |   Generic IRQ dispatch
  | generic_handle_irq_desc()  |
  | desc->handle_irq()         |   Calls flow handler (e.g., handle_edge_irq)
  | handle_irq_event()         |
  | action->handler()          |   Calls driver's registered handler
  +-------------+--------------+
                |
  [3]           v
  +----------------------------+
  | drivers/net/e1000/         |   NIC driver interrupt handler
  | e1000_intr()               |   Reads packet from hardware
  | netif_rx() / napi_schedule |   Hands sk_buff to network core
  +-------------+--------------+
                |
  [4]           v
  +----------------------------+
  | net/core/dev.c             |   Network core processing
  | netif_receive_skb()        |   Protocol demultiplexing
  | deliver_skb()              |
  +-------------+--------------+
                |
  [5]           v
  +----------------------------+
  | net/ipv4/ip_input.c        |   IP layer processing
  | ip_rcv() -> ip_rcv_finish  |
  | ip_local_deliver()         |   Looks up net_protocol by protocol number
  | ipprot->handler(skb)       |   Dispatches to TCP via struct net_protocol
  +-------------+--------------+
                |
  [6]           v
  +----------------------------+
  | net/ipv4/tcp_ipv4.c        |   TCP layer processing
  | tcp_v4_rcv()               |   Processes TCP state machine
  | tcp_rcv_established()      |   Queues data to socket receive buffer
  +-------------+--------------+
                |
  [7]           v
  +----------------------------+
  | net/socket.c               |   Socket layer
  | sock_recvmsg()             |   Called from sys_recvfrom()
  | sock->ops->recvmsg()       |   Dispatches through proto_ops
  +-------------+--------------+
                |
  [8]           v
  +----------------------------+
  | User Space                 |
  | recv(fd, buf, len, flags)  |
  +----------------------------+
```

#### Dependency Flow (Inward)

Now trace the **compile-time and link-time dependencies** -- which module
`#includes` or references which:

```
  User Space (no kernel deps)
       ^
       | syscall interface (ABI, not code dependency)
       |
  [7]  net/socket.c
       |  depends on: struct proto_ops (include/linux/net.h)
       |  does NOT depend on: net/ipv4/af_inet.c
       ^
  [6]  net/ipv4/af_inet.c
       |  depends on: struct proto_ops (implements inet_stream_ops)
       |  depends on: struct net_protocol (implements tcp_protocol)
       ^
  [5]  net/ipv4/ip_input.c
       |  depends on: struct net_protocol (include/net/protocol.h)
       |  does NOT depend on: tcp_ipv4.c directly
       ^
  [4]  net/core/dev.c
       |  depends on: struct packet_type, netdev abstractions
       |  does NOT depend on: any specific driver
       ^
  [3]  drivers/net/e1000/
       |  depends on: kernel IRQ API (request_irq, irq_handler_t)
       |  depends on: network API (netif_rx, alloc_skb)
       |  depends on: PCI API (pci_register_driver)
       ^
  [2]  kernel/irq/handle.c
       |  depends on: struct irq_chip, irq_handler_t
       |  does NOT depend on: any specific driver or HW
       ^
  [1]  arch/x86/kernel/
       |  depends on: generic IRQ API (generic_handle_irq)
       |  provides: architecture-specific entry code
```

#### Execution vs. Dependency: The Key Insight

```
EXECUTION FLOW:                    DEPENDENCY FLOW:
(bottom-up)                        (outer to inner)

Hardware                           drivers/net/e1000/
    |                                   |
    v                                   |  depends on
arch/ entry code                        v
    |                              kernel/irq/ APIs
    v                              net/core/ APIs
kernel/irq/                        include/linux/net.h
    |                                   ^
    v                                   |  depends on
NIC driver                         net/ipv4/af_inet.c
    |                                   ^
    v                                   |  depends on
net/core/                          net/socket.c
    |                                   ^
    v                                   |  syscall ABI
IP layer                           User Space
    |
    v
TCP layer
    |
    v
Socket layer
    |
    v
User Space
```

**Execution flows upward** from hardware to user space. **Dependencies flow
inward** from drivers to core abstractions. This is exactly the separation
Clean Architecture demands.

---

## 5. Conclusion

### Why This Architecture Works

The Linux Kernel v3.2 demonstrates that Clean Architecture principles are not
academic ideals confined to enterprise Java -- they are **engineering
necessities** that emerge naturally in any system that must:

1. **Support diverse hardware.** The kernel runs on x86, ARM, MIPS, PowerPC,
   and dozens of other architectures. The `arch/` abstraction layer makes this
   possible without rewriting `mm/`, `kernel/`, or `net/`.

2. **Allow independent evolution.** The CFS scheduler was introduced in Linux
   2.6.23, replacing the O(1) scheduler, without modifying the core scheduling
   loop in `kernel/sched.c`. The `struct sched_class` interface made this a
   clean swap.

3. **Enable third-party extension.** Thousands of device drivers, filesystems,
   and network protocols are developed independently. The function-pointer
   interfaces (`file_operations`, `proto_ops`, `irq_handler_t`) provide stable
   contracts that decouple the core from its extensions.

4. **Maintain security flexibility.** The LSM framework allows different security
   policies (SELinux, AppArmor) to be selected at boot time without recompiling
   any kernel code. This is the Plugin Architecture in its purest form.

### The C-Language Clean Architecture Toolkit

| Clean Architecture Concept | C Implementation in Linux |
|---|---|
| Interface / Abstract Class | `struct` with function pointers |
| Concrete Implementation | `static const struct ... = { .method = func, ... }` |
| Dependency Injection | Assignment: `inode->i_fop = &ext4_file_operations` |
| Plugin Registration | `register_security()`, `platform_driver_register()`, `request_irq()` |
| Interface Segregation | Separate structs: `file_operations` vs. `inode_operations` vs. `super_operations` |
| Dependency Rule Enforcement | Header file discipline: `include/linux/` defines interfaces; `drivers/`, `fs/`, `arch/` implement them |

### The Dependency Rule in the Linux Build System

The build system reinforces the architectural boundaries:

- `include/linux/*.h` -- Shared interfaces (the abstraction layer)
- `kernel/`, `mm/`, `net/core/`, `fs/` -- Core logic (Use Cases)
- `arch/*/include/asm/` -- Architecture-specific implementations
- `drivers/`, `fs/ext4/`, `security/selinux/` -- Concrete plugins

A driver in `drivers/` includes headers from `include/linux/`, but the core
code in `kernel/` never includes headers from `drivers/`. This directory
structure **physically enforces** the Dependency Rule.

### Final Observation

The Linux Kernel proves that Clean Architecture is not about languages,
frameworks, or design patterns -- it is about **the direction of dependencies**.
By consistently ensuring that dependencies point from mechanism to policy, from
concrete to abstract, from outer to inner, the kernel has maintained stability
and extensibility across 20+ years, thousands of contributors, and millions of
lines of code.

The `struct` with function pointers is perhaps the most elegant implementation
of Dependency Inversion ever conceived -- simpler than Java interfaces, more
explicit than C++ virtual methods, and battle-tested in the most critical
software system on the planet.
