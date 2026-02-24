# Module 6: Factory and Singleton

> **Patterns**: Factory Method / Abstract Factory (GoF), Singleton (GoF)
> **Kernel idioms**: alloc+init+register helpers, slab caches, global subsystem structs, per-CPU instances

---

## Part A: Factory-Like Construction

### Mental Model

Creation of "objects" is centralized so allocation, initialization, and
registration follow a single, safe pattern. Callers don't manually allocate
and then forget to init or register.

In C: "allocator + initializer + register" helpers (often named `*_alloc`,
`*_register`, or `*_create`). The factory ensures every created object is
properly set up and tracked.

```
  FACTORY API
  ┌──────────────────────────────────────────────┐
  │  1. Allocate memory (sized for the type)     │
  │  2. Initialize fields to known-good state    │
  │  3. Register with the subsystem              │
  │  4. Return the ready-to-use object           │
  └──────────────────────────────────────────────┘

  TEARDOWN API (symmetric)
  ┌──────────────────────────────────────────────┐
  │  1. Unregister from the subsystem            │
  │  2. Release resources                        │
  │  3. Free memory                              │
  └──────────────────────────────────────────────┘
```

**GoF mapping:**
- **Factory Method** → the `*_alloc()` / `*_create()` function
- **Product** → the object being created (e.g. `struct net_device`)
- **ConcreteProduct** → a specific device/inode/socket instance
- **Creator** → the subsystem that provides the factory API

### In the Kernel (v3.2)

#### Example 1: Network Device — `alloc_netdev` + `register_netdev`

`include/linux/netdevice.h`, lines 2440–2450:

```c
extern struct net_device *alloc_netdev_mqs(int sizeof_priv, const char *name,
                                           void (*setup)(struct net_device *),
                                           unsigned int txqs, unsigned int rxqs);

#define alloc_netdev(sizeof_priv, name, setup) \
    alloc_netdev_mqs(sizeof_priv, name, setup, 1, 1)

extern int      register_netdev(struct net_device *dev);
extern void     unregister_netdev(struct net_device *dev);
```

**The creation sequence:**

```c
/* In a driver's probe function: */
struct net_device *netdev;

/* Step 1: Factory allocates and initializes */
netdev = alloc_etherdev(sizeof(struct my_priv));
/*  alloc_etherdev → alloc_netdev → alloc_netdev_mqs:
 *    - kmalloc for net_device + private data
 *    - initialize queues, name, default ops
 *    - call setup() callback (e.g. ether_setup)
 */

/* Step 2: Driver fills in its specifics */
netdev->netdev_ops = &my_netdev_ops;

/* Step 3: Register with the network stack */
register_netdev(netdev);
/*  - validates the device
 *  - assigns it to a network namespace
 *  - creates sysfs entries
 *  - notifies observers (via netdev_chain)
 */
```

**The teardown sequence (symmetric):**

```c
unregister_netdev(netdev);  /* remove from stack, sysfs */
free_netdev(netdev);        /* release memory */
```

```
  alloc_etherdev()                        free_netdev()
  ┌──────────────────┐                    ┌──────────────────┐
  │ kmalloc          │                    │ kfree            │
  │ init queues      │                    │ release queues   │
  │ ether_setup()    │                    │                  │
  └────────┬─────────┘                    └────────▲─────────┘
           │                                       │
  register_netdev()                       unregister_netdev()
  ┌──────────────────┐                    ┌──────────────────┐
  │ validate         │                    │ remove from ns   │
  │ add to namespace │                    │ remove sysfs     │
  │ create sysfs     │                    │ notify observers │
  │ notify observers │                    │                  │
  └──────────────────┘                    └──────────────────┘
```

#### Example 2: Slab Allocator — `kmem_cache_create` + `kmem_cache_alloc`

`mm/slab.c` (or `mm/slub.c`):

```c
/* Create a typed object pool */
ext4_inode_cachep = kmem_cache_create("ext4_inode_cache",
                                      sizeof(struct ext4_inode_info),
                                      0,
                                      (SLAB_RECLAIM_ACCOUNT|SLAB_MEM_SPREAD),
                                      init_once);

/* Allocate from the pool — like "new Ext4InodeInfo()" */
struct ext4_inode_info *ei = kmem_cache_alloc(ext4_inode_cachep, GFP_NOFS);

/* Free back to the pool — like "delete ei" */
kmem_cache_free(ext4_inode_cachep, ei);
```

The slab allocator is a **factory** that:
1. Pre-allocates memory in efficient slabs (pages subdivided into
   object-sized chunks).
2. Knows the exact size and alignment of each object.
3. Calls `init_once` as a constructor for cache-cold objects.
4. Reuses freed objects without re-initialization.
5. Provides per-type debugging (use-after-free detection, leak tracking).

```
  kmem_cache_create("ext4_inode_cache", ...)
  ┌─────────────────────────────────────────────┐
  │         SLAB CACHE (the factory)            │
  │                                             │
  │  ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐    │
  │  │ obj │ │ obj │ │ obj │ │FREE │ │FREE │    │  ← pre-sliced pages
  │  └─────┘ └─────┘ └─────┘ └─────┘ └─────┘    │
  │                                             │
  │  kmem_cache_alloc() → return next free obj  │
  │  kmem_cache_free()  → mark obj as free      │
  └─────────────────────────────────────────────┘
```

#### Example 3: `seq_open` / `single_open` — File Iterator Factory

```c
/* fs/seq_file.c */
int seq_open(struct file *file, const struct seq_operations *op)
{
    struct seq_file *p = file->private_data;

    if (!p) {
        p = kmalloc(sizeof(*p), GFP_KERNEL);
        if (!p)
            return -ENOMEM;
        file->private_data = p;
    }
    memset(p, 0, sizeof(*p));
    mutex_init(&p->lock);
    p->op = op;
    /* ... */
    return 0;
}
```

`seq_open` is a factory: it allocates the `seq_file` context, initializes
it, and attaches it to the `file`. Callers don't manually allocate or init
the seq_file struct.

### Real Code Path Walkthrough: ext4 Inode Allocation Through the Slab Factory

Trace what happens when ext4 needs a new inode (e.g. creating a file).
The VFS calls the filesystem's `alloc_inode` method, which uses the slab
cache factory to produce a properly initialized `ext4_inode_info`.

```
  VFS: create a new file on ext4
       │
       │  VFS calls sb->s_op->alloc_inode(sb)
       │  because ext4 registered: .alloc_inode = ext4_alloc_inode
       ▼
  fs/ext4/super.c:889 — ext4_alloc_inode(sb)
  ┌──────────────────────────────────────────────────────────────────┐
  │                                                                  │
  │  /* STEP 1: Allocate from the typed slab cache (the factory) */  │
  │  ei = kmem_cache_alloc(ext4_inode_cachep, GFP_NOFS);             │
  │       │                                                          │
  │       │  mm/slub.c (or mm/slab.c) — kmem_cache_alloc():          │
  │       │    1. Check per-CPU freelist for a hot object            │
  │       │       → if available, return it immediately (fast path)  │
  │       │    2. Check the slab partial list                        │
  │       │       → if a partial slab exists, carve from it          │
  │       │    3. If no free objects, get new pages from buddy       │
  │       │       allocator and create a new slab                    │
  │       │    4. On the very first allocation of a COLD slab,       │
  │       │       call init_once(ptr) — the slab constructor:        │
  │       │         fs/ext4/super.c — init_once():                   │
  │       │           inode_init_once(&ei->vfs_inode);               │
  │       │    5. Return pointer to ext4_inode_info-sized chunk      │
  │       ▼                                                          │
  │  if (!ei) return NULL;                                           │
  │                                                                  │
  │  /* STEP 2: Initialize ext4-specific fields */                   │
  │  ei->vfs_inode.i_version = 1;                                    │
  │  memset(&ei->i_cached_extent, 0, ...);                           │
  │  INIT_LIST_HEAD(&ei->i_prealloc_list);                           │
  │  spin_lock_init(&ei->i_prealloc_lock);                           │
  │  ei->i_reserved_data_blocks = 0;                                 │
  │  ei->i_reserved_meta_blocks = 0;                                 │
  │  /* ... 15+ more field initializations ... */                    │
  │                                                                  │
  │  /* STEP 3: Return the embedded VFS inode */                     │
  │  return &ei->vfs_inode;                                          │
  │  /* VFS gets a struct inode*; ext4 later uses container_of       │
  │     to recover the full ext4_inode_info from that pointer. */    │
  │                                                                  │
  └──────────────────────────────────────────────────────────────────┘

  THE SLAB CACHE WAS CREATED ONCE AT MOUNT TIME:

  fs/ext4/super.c:965 — init_inodecache()
  ┌──────────────────────────────────────────────────────────────────┐
  │  ext4_inode_cachep = kmem_cache_create("ext4_inode_cache",       │
  │                          sizeof(struct ext4_inode_info),         │
  │                          0,                                      │
  │                          (SLAB_RECLAIM_ACCOUNT|SLAB_MEM_SPREAD), │
  │                          init_once);                             │
  │  /* This creates the typed pool — the "factory" */               │
  │  /* init_once is the constructor for cold objects */             │
  └──────────────────────────────────────────────────────────────────┘
```

**Key observation:** ext4 never calls `kmalloc(sizeof(struct ext4_inode_info))`.
It always goes through the slab cache factory (`kmem_cache_alloc`), which
guarantees: correct size, proper alignment, the `init_once` constructor runs
for cold objects, and freed inodes are recycled efficiently. The factory
makes 15+ field initializations impossible to skip.

### Why Factory Here

**What bad thing could happen without the factory:**
- A driver allocates `net_device` with bare `kmalloc` but forgets to
  initialize the TX/RX queues → NULL pointer dereference on first packet.
- A driver calls `register_netdev` before setting up queues → the
  networking stack tries to use an incomplete device.
- A filesystem allocates inodes directly but skips the slab cache →
  loses the `init_once` constructor, gets uninitialized fields.

The factory makes the right thing easy and the wrong thing hard.

**Symmetry:** Every `*_alloc`/`*_create`/`*_register` has a matching
`*_free`/`*_destroy`/`*_unregister`. This symmetry prevents leaks and
ensures orderly shutdown.

---

## Part B: Singleton

### Mental Model

Exactly one logical instance of a component exists for the whole system
(or per CPU, or per container). Other code gets a reference to it via
a well-known global, an accessor function, or a macro.

In C: a static or global struct (or pointer) plus an accessor or
init-once pattern.

```
  ┌──────────────────────────────────────────┐
  │          THE SINGLETON                   │
  │  (one instance, many users)              │
  │                                          │
  │  static struct subsystem the_subsystem;  │
  │                                          │
  │  Accessor: get_subsystem() or            │
  │            direct global reference       │
  └──────────────────────────────────────────┘
       ▲         ▲          ▲
       │         │          │
   user A    user B     user C
```

### In the Kernel (v3.2)

#### Example 1: The Scheduler — One Per CPU

Each CPU has exactly one `struct rq` (run queue). There is no API to
create a second one:

```c
/* kernel/sched/core.c */
static DEFINE_PER_CPU_SHARED_ALIGNED(struct rq, runqueues);

#define cpu_rq(cpu)     (&per_cpu(runqueues, (cpu)))
#define this_rq()       (&__get_cpu_var(runqueues))
```

The scheduler core is global; `schedule()` is the single entry point
that all code uses to yield the CPU. You can't instantiate a second
scheduler.

#### Example 2: The Memory Management Subsystem

Physical memory management is a singleton: one buddy allocator per zone,
one set of page tables per process, one global `mem_map` array. The API
is `alloc_pages()` / `free_pages()` — everyone goes through the same
allocator.

#### Example 3: Reboot Notifier List

```c
/* include/linux/notifier.h, line 209 */
extern struct blocking_notifier_head reboot_notifier_list;
```

One global list. Every subsystem that cares about reboot registers on
this single instance. There is no way to create a second reboot notifier
chain — nor would it make sense to.

#### Example 4: Init Process (PID 1)

```c
/* include/linux/sched.h */
extern struct task_struct init_task;
```

There is exactly one `init_task`. It is the ancestor of all other
processes. It is statically allocated (not kmalloc'd) and lives for
the entire lifetime of the kernel.

### "One Per CPU" vs. True Singleton

The kernel often avoids a *literally* one global variable and instead uses
"one per CPU" (`DEFINE_PER_CPU`) or "one per network namespace"
(`struct net`). This is still a singleton at the logical level — each CPU
or namespace has exactly one instance. The "per-X" scoping avoids
contention while preserving the "single point of access" intent.

```
  True singleton:           Per-CPU singleton:
  ┌───────────────┐         ┌────────┐ ┌────────┐ ┌────────┐
  │ one instance  │         │ CPU 0  │ │ CPU 1  │ │ CPU 2  │
  │ global var    │         │ rq[0]  │ │ rq[1]  │ │ rq[2]  │
  └───────────────┘         └────────┘ └────────┘ └────────┘

  Both: exactly one instance per scope.
  Per-CPU avoids lock contention.
```

### Why Singleton Here

Some subsystems must be unique by nature. Two schedulers competing for
the same CPU would be chaos. Two buddy allocators for the same physical
page would corrupt memory. A single point of access avoids duplicate
state and inconsistent behavior.

**What would break without it:**
- If any code could create its own `struct rq`, tasks might be scheduled
  on a rogue run queue that the real scheduler doesn't know about.
- If there were two reboot notifier lists, some subsystems wouldn't get
  notified on reboot.

---

## Factory + Singleton: How They Interact

The singleton often *owns* the factory. Example:

```
  Singleton: slab allocator subsystem (global, init'd once at boot)
       │
       └── provides kmem_cache_create()  (factory for typed pools)
              │
              └── provides kmem_cache_alloc()  (factory for objects)
```

The allocator is a singleton (one per system). It provides factory methods
for creating typed object pools, which in turn are factories for individual
objects.

---

## Check Your Understanding

1. Find one "create/register" API. What bad thing could happen if a driver
   allocated the struct and started using it without going through the
   register path?

2. How does the "factory" in the kernel usually pair with a "teardown"
   (unregister, free) so that the mental model is symmetric?

3. Name one kernel subsystem that is effectively a singleton. How does
   the rest of the kernel get to it (global name, pointer from process,
   macro)?

4. Why might the kernel avoid *literally* one global variable and instead
   use "one per CPU" or "one per container"? How does that still fit the
   "single logical instance" idea?

---

Proceed to [Module 7: Facade](07_facade.md).
