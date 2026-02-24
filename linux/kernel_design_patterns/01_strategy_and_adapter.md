# Module 1: Strategy and Adapter

> **Patterns**: Strategy (GoF), Adapter (GoF)
> **Kernel idioms**: ops structs, vtable dispatch, interface translation layers

These two patterns are the most visible in the kernel and the foundation for
everything else. Master them first.

---

## Part A: Strategy

### Mental Model

Behavior is chosen at runtime by selecting one of several implementations
of the same "interface." In C there are no virtual methods, so the interface
is a struct of function pointers; each "strategy" fills in that struct
differently.

```
                            ┌──────────────────────┐
                            │   Strategy Interface │
                            │  (struct of fn ptrs) │
                            └──────────┬───────────┘
                                       │
              ┌────────────────────────┼────────────────────────┐
              ▼                        ▼                        ▼
   ┌───────────────────┐   ┌───────────────────┐   ┌───────────────────┐
   │ ConcreteStrategy A│   │ ConcreteStrategy B│   │ ConcreteStrategy C│
   │  (fills in ops)   │   │  (fills in ops)   │   │  (fills in ops)   │
   └───────────────────┘   └───────────────────┘   └───────────────────┘
```

**GoF mapping:**
- **Strategy interface** → struct of function pointers (e.g. `struct file_operations`)
- **ConcreteStrategy** → a specific `static const` instance of that struct
- **Context** → the object that holds a pointer to the ops struct

### In the Kernel (v3.2)

#### Example 1: `struct file_operations`

`include/linux/fs.h`, lines 1516–1556:

```c
struct file_operations {
    struct module *owner;
    loff_t (*llseek) (struct file *, loff_t, int);
    ssize_t (*read) (struct file *, char __user *, size_t, loff_t *);
    ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *);
    ssize_t (*aio_read) (struct kiocb *, const struct iovec *,
                         unsigned long, loff_t);
    ssize_t (*aio_write) (struct kiocb *, const struct iovec *,
                          unsigned long, loff_t);
    /* ... more ... */
    int (*open) (struct inode *, struct file *);
    int (*release) (struct inode *, struct file *);
    /* ... */
};
```

Each open file has `file->f_op` pointing to one of these. When you open
`/dev/null`, `f_op` points to `null_fops` (from `drivers/char/mem.c`).
When you open a socket, `f_op` points to `socket_file_ops`. Same `.read`
call, completely different behavior:

```
  userspace: read(fd, buf, n)
       │
       ▼
  VFS: vfs_read(file, ...)
       │
       ▼
  file->f_op->read(file, buf, n, &pos)
       │
       ├── /dev/null:   read_null()        → returns 0
       ├── /dev/zero:   read_zero()        → fills buf with zeros
       ├── ext4 file:   do_sync_read()     → reads from disk
       ├── pipe:        pipe_read()        → reads from pipe buffer
       └── socket:      sock_aio_read()    → reads from network
```

#### Example 2: `struct sched_class`

`include/linux/sched.h`, lines 1084–1119:

```c
struct sched_class {
    const struct sched_class *next;

    void (*enqueue_task) (struct rq *rq, struct task_struct *p, int flags);
    void (*dequeue_task) (struct rq *rq, struct task_struct *p, int flags);
    void (*yield_task)   (struct rq *rq);

    void (*check_preempt_curr)(struct rq *rq, struct task_struct *p,
                               int flags);
    struct task_struct *(*pick_next_task)(struct rq *rq);
    void (*put_prev_task)(struct rq *rq, struct task_struct *p);
    void (*task_tick)(struct rq *rq, struct task_struct *p, int queued);
    /* ... more ... */
};
```

Each `task_struct` holds `const struct sched_class *sched_class`. The core
scheduler dispatches through it:

```c
/* kernel/sched/core.c — conceptual */
for_each_class(class) {
    p = class->pick_next_task(rq);
    if (p)
        return p;
}
```

The concrete strategies form a priority chain:

```
stop_sched_class → rt_sched_class → fair_sched_class → idle_sched_class
  (highest)                                               (lowest)
```

Changing a task's policy via `sched_setscheduler()` swaps which strategy
governs that task — the core scheduler code doesn't change at all.

#### Example 3: `struct tcp_congestion_ops`

`include/net/tcp.h`, lines 689–714:

```c
struct tcp_congestion_ops {
    struct list_head    list;
    unsigned long flags;

    void (*init)(struct sock *sk);
    void (*release)(struct sock *sk);

    u32 (*ssthresh)(struct sock *sk);
    u32 (*min_cwnd)(const struct sock *sk);
    void (*cong_avoid)(struct sock *sk, u32 ack, u32 in_flight);
    void (*set_state)(struct sock *sk, u8 new_state);
    void (*cwnd_event)(struct sock *sk, enum tcp_ca_event ev);
    /* ... */
};
```

TCP Reno, CUBIC, Vegas — each registers a `tcp_congestion_ops`. The active
one is selected per-socket and swapped via setsockopt. Identical pattern:
interface struct, concrete instances, runtime selection.

### Why Strategy Here

The kernel must support many concrete types (filesystems, scheduling policies,
congestion algorithms) without a single giant switch or if-ladder. Adding a
new implementation means filling in a new ops struct, not editing core
VFS/scheduler/TCP code.

**What would break without it:** Every `read()` call would need
`switch (file_type) { case SOCKET: ... case PIPE: ... case EXT4: ... }`.
Adding a new filesystem would require editing the central VFS. Modules
could not provide new implementations.

### Real Code Path Walkthrough: `read()` on `/dev/null`

Trace what happens when userspace calls `read(fd, buf, 128)` on an fd
that refers to `/dev/null`. Every line below references real v3.2 code.

```
  USERSPACE
  ─────────
  read(fd, buf, 128)
       │
       │  syscall entry (arch-specific, lands in):
       ▼
  fs/read_write.c:460 — SYSCALL_DEFINE3(read, ...)
  ┌──────────────────────────────────────────────────────┐
  │  file = fget_light(fd, &fput_needed);                │  ← look up struct file from fd
  │  pos = file_pos_read(file);                          │
  │  ret = vfs_read(file, buf, count, &pos);             │  ← enter VFS template method
  │  file_pos_write(file, pos);                          │
  │  fput_light(file, fput_needed);                      │
  │  return ret;                                         │
  └──────────────────────────────┬───────────────────────┘
                                 │
                                 ▼
  fs/read_write.c:364 — vfs_read(file, buf, 128, &pos)
  ┌──────────────────────────────────────────────────────┐
  │  check file->f_mode & FMODE_READ                     │  ← invariant: permission
  │  check file->f_op != NULL                            │  ← invariant: ops exist
  │  check access_ok(VERIFY_WRITE, buf, 128)             │  ← invariant: user buffer
  │  rw_verify_area(READ, file, &pos, 128)               │  ← invariant: LSM/security
  │                                                      │
  │  file->f_op->read(file, buf, 128, &pos)  ◄────────  │  ← STRATEGY DISPATCH
  │       │                                              │
  │  fsnotify_access(file)                               │  ← invariant: notification
  │  add_rchar(current, ret)                             │  ← invariant: accounting
  └──────┼───────────────────────────────────────────────┘
         │
         │  file->f_op == &null_fops (installed when /dev/null was opened)
         │  file->f_op->read == read_null
         │
         ▼
  drivers/char/mem.c:447 — read_null(file, buf, 128, &ppos)
  ┌──────────────────────────────────────────────────────┐
  │  return 0;   /* /dev/null: nothing to read, ever */  │
  └──────────────────────────────────────────────────────┘
```

The `null_fops` struct that was installed when `/dev/null` was opened:

```c
/* drivers/char/mem.c, line 567 */
static const struct file_operations null_fops = {
    .llseek      = null_lseek,
    .read        = read_null,       /* ← the strategy selected */
    .write       = write_null,
    .splice_write = splice_write_null,
};
```

**Key observation:** The exact same VFS machinery (`sys_read` → `vfs_read` →
`f_op->read`) handles `/dev/null`, `/dev/zero`, ext4 files, pipes, and
sockets. The *only* thing that differs is which `file_operations` struct was
installed at open time. That's the Strategy pattern in action.

### Check Your Understanding

1. Pick one `file_operations` instance (e.g. `null_fops` in
   `drivers/char/mem.c`). Trace who fills in `.read` and `.write`,
   and who calls them through `f_op->read`.

2. In one sentence: what "strategy" is being selected at runtime when
   you `open()` a file?

3. Map to GoF: "The **Strategy** is ___; the **Context** is ___; the
   **ConcreteStrategy** is ___."

---

## Part B: Adapter

### Mental Model

Code expects interface A; you have implementation B. An adapter presents
A's API and translates calls into B (or the reverse for results). In C:
a thin layer of functions that take A's arguments, convert them if needed,
and call B.

```
  Upper layer expects:          Lower layer provides:
  ┌──────────────────┐          ┌──────────────────┐
  │ Interface A      │          │ Implementation B │
  │ (VFS ops)        │          │ (on-disk format, │
  │                  │          │  HW registers)   │
  └────────┬─────────┘          └────────┬─────────┘
           │                             │
           │     ┌──────────────┐        │
           └────►│   ADAPTER    │◄───────┘
                 │ (translates  │
                 │  A calls     │
                 │  into B)     │
                 └──────────────┘
```

**GoF mapping:**
- **Target interface** → what the upper layer expects (e.g. `struct file_operations`)
- **Adaptee** → the concrete implementation's native interface
- **Adapter** → the ops struct instance + its functions that translate between them

### In the Kernel (v3.2)

#### Example 1: VFS on Top of ramfs

The VFS expects `inode_operations`, `file_operations`, `super_operations`.
Ramfs adapts its in-memory tree to those interfaces.

`fs/ramfs/inode.c`, lines 141–157:

```c
static const struct inode_operations ramfs_dir_inode_operations = {
    .create     = ramfs_create,
    .lookup     = simple_lookup,
    .link       = simple_link,
    .unlink     = simple_unlink,
    .symlink    = ramfs_symlink,
    .mkdir      = ramfs_mkdir,
    .rmdir      = simple_rmdir,
    .mknod      = ramfs_mknod,
    .rename     = simple_rename,
};

static const struct super_operations ramfs_ops = {
    .statfs     = simple_statfs,
    .drop_inode = generic_delete_inode,
    .show_options = generic_show_options,
};
```

Each function in `ramfs_dir_inode_operations` is an adapter. The VFS calls
`.create(dir, dentry, mode, nd)` — the generic interface. `ramfs_create`
translates that into ramfs-specific work (allocate an in-memory inode,
instantiate the dentry).

The **target** is the VFS interface (`inode_operations`). The **adaptee** is
ramfs's own in-memory data. The **adapter** is each `ramfs_*` function that
bridges the two.

#### Example 2: Socket Layer — `struct proto_ops`

`include/linux/net.h`, lines 161–191:

```c
struct proto_ops {
    int     family;
    struct module   *owner;
    int     (*release)   (struct socket *sock);
    int     (*bind)      (struct socket *sock, struct sockaddr *myaddr,
                          int sockaddr_len);
    int     (*connect)   (struct socket *sock, struct sockaddr *vaddr,
                          int sockaddr_len, int flags);
    int     (*accept)    (struct socket *sock, struct socket *newsock,
                          int flags);
    int     (*listen)    (struct socket *sock, int len);
    int     (*shutdown)  (struct socket *sock, int flags);
    /* ... many more ... */
};
```

The BSD socket API (`bind`, `connect`, `accept`, `listen`) is the **target**.
Each protocol family (AF_INET, AF_NETLINK, AF_UNIX) provides its own
`proto_ops` as the **adapter** that translates generic socket calls into
protocol-specific work.

```
  Userspace:  connect(sockfd, addr, len)
       │
       ▼
  Socket layer:  sock->ops->connect(sock, addr, len, flags)
       │
       ├── AF_INET:   inet_stream_connect()   → TCP handshake
       ├── AF_UNIX:   unix_stream_connect()    → local pipe-like setup
       └── AF_NETLINK: netlink_connect()       → set destination PID
```

#### Example 3: Block Layer — `struct block_device_operations`

`include/linux/blkdev.h`, lines 1298–1315:

```c
struct block_device_operations {
    int (*open) (struct block_device *, fmode_t);
    int (*release) (struct gendisk *, fmode_t);
    int (*ioctl) (struct block_device *, fmode_t, unsigned, unsigned long);
    int (*compat_ioctl) (struct block_device *, fmode_t, unsigned,
                         unsigned long);
    unsigned int (*check_events) (struct gendisk *disk, unsigned int clearing);
    int (*revalidate_disk) (struct gendisk *);
    int (*getgeo)(struct block_device *, struct hd_geometry *);
    struct module *owner;
};
```

The block layer expects this interface. SCSI, ATA, NVMe (in later kernels),
virtio-blk — each provides its own instance. The adapter translates
generic block operations into hardware-specific commands.

### Why Adapter Here

Upper layers (VFS, block layer, socket layer) stay generic; diversity is
handled at the edges by adapters that speak "kernel API" on one side and
"hardware/spec/format" on the other.

**What would break without it:** The VFS would need to know about every
on-disk format. Adding a new filesystem would require changing the VFS core.
The block layer would need a switch on every hardware type.

### The Strategy–Adapter Overlap

You may notice these look similar. They are related:

| Aspect | Strategy | Adapter |
|--------|----------|---------|
| **Intent** | Choose among algorithms at runtime | Make incompatible interfaces work together |
| **Focus** | The *selection* of behavior | The *translation* between layers |
| **Typical kernel use** | `sched_class`, `tcp_congestion_ops` | `file_operations` per filesystem, `proto_ops` per protocol |
| **How to tell them apart** | "Which algorithm?" | "Which translation?" |

In practice many kernel ops structs serve *both* roles. `file_operations`
is both a strategy (which behavior for this file type?) and an adapter
(how does this specific filesystem map VFS calls to its own logic?).

### Check Your Understanding

1. Pick one filesystem (e.g. ramfs or ext4). Point to the struct that
   "implements" the VFS interface and one function that clearly
   "translates" a VFS call into fs-specific work.

2. In one sentence: what is the "target" interface and what is the
   "adaptee" in the socket `proto_ops` example?

3. Find one `block_device_operations` instance in `drivers/block/`. What
   hardware-specific work does its `.open` do that the block layer doesn't
   know about?

4. When you see an ops struct, ask: "Is this more Strategy or more Adapter?"
   Try it on `struct sched_class` vs. `struct super_operations`.

---

## Pattern Summary

| GoF Pattern | C Mechanism | Kernel Example | Key Insight |
|-------------|-------------|---------------|-------------|
| Strategy | struct of function pointers; one per "algorithm" | `sched_class`, `tcp_congestion_ops` | Behavior is selected at runtime by swapping the ops pointer |
| Adapter | struct of function pointers; translates upper→lower | `file_operations`, `proto_ops`, `block_device_operations` | Upper layers stay generic; diversity at the edges |

---

Proceed to [Module 2: Observer](02_observer.md).
