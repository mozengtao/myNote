# Module 8: Composition, Decorator, and Builder

> **Patterns**: Composition (GoF — "prefer composition over inheritance"),
> Decorator (GoF), Builder (GoF)
> **Kernel idioms**: Struct embedding/referencing, hook chains, multi-step initialization

These three patterns deal with how complex objects are *structured*,
*extended*, and *constructed*. They often appear together.

---

## Part A: Object Composition — "has-a" over "is-a"

### Mental Model

Complex behavior is built by composing smaller, focused objects (structs)
that each own a clear responsibility. In C this is "struct containing (or
pointing to) other structs," and passing the right pointer to the right
layer. No inheritance tree; reuse by composition.

```
  ┌──────────────────────────────────────────────────────┐
  │                  struct task_struct                  │
  │                                                      │
  │  ┌───────────┐  ┌──────────┐  ┌──────────┐           │
  │  │ mm_struct │  │fs_struct │  │files_    │           │
  │  │ (memory)  │  │(fs ctxt) │  │struct    │           │
  │  │           │  │          │  │(open fds)│           │
  │  └───────────┘  └──────────┘  └──────────┘           │
  │                                                      │
  │  ┌──────────────┐  ┌──────────────┐                  │
  │  │signal_struct │  │ sched_entity │                  │
  │  │(signals)     │  │ (scheduler)  │                  │
  │  └──────────────┘  └──────────────┘                  │
  └──────────────────────────────────────────────────────┘
```

**GoF principle:** "Favor object composition over class inheritance."
The kernel follows this almost exclusively — there are no inheritance
hierarchies in the OOP sense, only composition.

### In the Kernel (v3.2)

#### Example 1: `struct task_struct` — Composition of Subsystems

`include/linux/sched.h`, lines 1220+:

```c
struct task_struct {
    volatile long state;
    void *stack;

    /* Scheduler subsystem */
    const struct sched_class *sched_class;
    struct sched_entity se;
    struct sched_rt_entity rt;

    /* Memory management subsystem */
    struct mm_struct *mm;

    /* Filesystem context */
    struct fs_struct *fs;

    /* Open file descriptors */
    struct files_struct *files;

    /* Namespaces */
    struct nsproxy *nsproxy;

    /* Signal handling */
    struct signal_struct *signal;
    struct sighand_struct *sighand;

    /* Credentials */
    const struct cred __rcu *cred;

    /* Thread-local state (arch-specific) */
    struct thread_struct thread;

    /* ... many more ... */
};
```

A process is not a single monolithic object. It is the *composition* of
many subsystem-specific structs, each with its own lifecycle:

| Component | Pointed to / Embedded | Ownership |
|-----------|----------------------|-----------|
| `mm_struct *mm` | Pointer (shared between threads) | Refcounted, shared by thread group |
| `fs_struct *fs` | Pointer (shareable via CLONE_FS) | Refcounted |
| `files_struct *files` | Pointer (shareable via CLONE_FILES) | Refcounted |
| `struct sched_entity se` | Embedded (one per task) | Owned by the task |
| `struct thread_struct thread` | Embedded (arch-specific) | Owned by the task |

The key insight: some components are **embedded** (owned exclusively by
this task), while others are **pointers** (shared between tasks via
refcounting). `fork()` with `CLONE_VM` shares `mm`; `fork()` without it
duplicates `mm`. Composition makes this sharing explicit.

#### Example 2: `struct net_device` — Layered Composition

```
  ┌──────────────────────────────────────────────┐
  │             struct net_device                │
  │                                              │
  │  struct device dev          ◄── device model │
  │  struct net_device_ops *ops ◄── behavior     │
  │  struct netdev_queue *_tx   ◄── TX queues    │
  │  struct netdev_rx_queue *_rx◄── RX queues    │
  │                                              │
  │  [private data area]        ◄── driver state │
  │    struct e1000_adapter {                    │
  │      struct napi_struct napi;                │
  │      struct e1000_ring *tx_ring;             │
  │      /* ... */                               │
  │    }                                         │
  └──────────────────────────────────────────────┘
```

`net_device` composes:
- A generic `struct device` for the device model (sysfs, power management)
- An ops pointer for behavior (Strategy / Adapter)
- Queue structures for TX/RX
- Private driver data appended at the end

Each piece has a clear responsibility. The network stack only touches
the generic parts; the driver only touches its private data.

#### Example 3: `struct sk_buff` — Protocol Layer Composition

Network packets are modeled as `sk_buff` with protocol-specific data
composed at each layer:

```
  ┌──────────────────────────────────────────────┐
  │              struct sk_buff                  │
  │                                              │
  │  data pointer ─────────────────────────┐     │
  │  transport_header ──────────┐          │     │
  │  network_header ──────┐     │          │     │
  │  mac_header ─────┐    │     │          │     │
  │                  │    │     │          │     │
  │                  ▼    ▼     ▼          ▼     │
  │              ┌────┬────┬─────┬──────────┐    │
  │   packet:    │MAC │ IP │ TCP │  payload │    │
  │              └────┴────┴─────┴──────────┘    │
  │                                              │
  │  Each layer "owns" its header region         │
  │  without knowing about the others.           │
  └──────────────────────────────────────────────┘
```

Each protocol layer adds or interprets its part of the buffer. This is
composition at the data level — each layer "has" its header within the
same buffer, and the `sk_buff` metadata tracks where each layer starts.

### Real Code Path Walkthrough: `fork()` → `copy_mm()` — Share vs. Duplicate

Trace how the Composition pattern plays out at runtime during `fork()`. The
`copy_mm()` function decides whether the new child *shares* the parent's
memory subsystem (threads via `CLONE_VM`) or *duplicates* it (new process).
This is composition in action — the mm component has its own lifecycle.

```
  USERSPACE
  ─────────
  fork() — or — pthread_create() (which uses clone(CLONE_VM|...))
       │
       │  syscall → do_fork() → copy_process()
       │    [see Module 3 for the full template method]
       │    → among many copy_* calls, reaches:
       ▼
  kernel/fork.c:593 — copy_mm(clone_flags, tsk)
  ┌──────────────────────────────────────────────────────────────────┐
  │                                                                  │
  │  /* Initialize fault counters for the new task */                │
  │  tsk->min_flt = tsk->maj_flt = 0;                                │
  │  tsk->mm = NULL;                                                 │
  │  tsk->active_mm = NULL;                                          │
  │                                                                  │
  │  oldmm = current->mm;                                            │
  │  if (!oldmm) return 0;    /* kernel thread — no mm at all */     │
  │                                                                  │
  │  ═══════════════════════════════════════════════                 │
  │   DECISION POINT: share or duplicate?                            │
  │  ═══════════════════════════════════════════════                 │
  │                                                                  │
  │  if (clone_flags & CLONE_VM) {                                   │
  │      /*                                                          │
  │       * SHARING path (threads / clone with CLONE_VM):            │
  │       * Both tasks point to the SAME mm_struct.                  │
  │       * Refcount bumped so mm outlives either task.              │
  │       */                                                         │
  │      atomic_inc(&oldmm->mm_users);   ◄── bump refcount           │
  │      mm = oldmm;                     ◄── SHARE the same mm       │
  │      goto good_mm;                                               │
  │                                                                  │
  │  } else {                                                        │
  │      /*                                                          │
  │       * DUPLICATION path (fork):                                 │
  │       * Create a NEW mm_struct with COW copies of all VMAs.      │
  │       */                                                         │
  │      mm = dup_mm(tsk);               ◄── DUPLICATE               │
  │           │                                                      │
  │           │  kernel/fork.c — dup_mm():                           │
  │           │    mm = allocate_mm();    /* kmem_cache_alloc */     │
  │           │    memcpy(mm, oldmm, sizeof(*mm));                   │
  │           │    mm_init(mm, tsk);      /* init locks, counters */ │
  │           │    dup_mmap(mm, oldmm);   /* copy all VMAs,          │
  │           │         set up copy-on-write page table entries */   │
  │           │    return mm;                                        │
  │           ▼                                                      │
  │  }                                                               │
  │                                                                  │
  │  good_mm:                                                        │
  │  tsk->mm = mm;           /* attach the (shared or new) mm */     │
  │  tsk->active_mm = mm;                                            │
  │  return 0;                                                       │
  │                                                                  │
  └──────────────────────────────────────────────────────────────────┘
```

```
  After fork() (no CLONE_VM):          After clone(CLONE_VM):
  ┌──────────────┐ ┌──────────────┐    ┌──────────────┐ ┌──────────────┐
  │ parent task  │ │  child task  │    │   thread 1   │ │   thread 2   │
  │  tsk->mm ────┼→│mm_users = 1  │    │  tsk->mm ──┐ │ │  tsk->mm ──┐ │
  │              │ │ (COW copy)   │    │            │ │ │            │ │
  └──────────────┘ └──────────────┘    └────────────┼─┘ └────────────┼─┘
  │  tsk->mm ────┼→ ┌──────────────┐                │                │
  │              │  │mm_users = 1  │                ▼                ▼
  └──────────────┘  │ (original)   │    ┌──────────────────────────────┐
                    └──────────────┘    │     SHARED mm_struct         │
                                        │     mm_users = 2             │
                    Two separate mm's   │     (same page tables,       │
                    with COW pages      │      same VMAs)              │
                                        └──────────────────────────────┘
```

**Key observation:** Because `mm_struct` is a *composed component* (pointer,
not embedded), this share-vs-duplicate decision is trivial — either bump a
refcount or copy the struct. If mm were embedded flat inside `task_struct`,
sharing would be impossible and threads would need a completely different
implementation. This is why composition (has-a via pointer) is fundamental
to the kernel's design.

### Why Composition Here

The kernel models many different "kinds" of things (processes, devices,
packets) without a single inheritance tree. Composition keeps each
subsystem's data and logic local and makes dependencies explicit.

**What would break without it:**
- If `task_struct` were one flat struct instead of composed parts, sharing
  `mm` between threads would require copying the entire task struct.
- If `net_device` didn't compose `struct device`, it couldn't participate
  in the generic device model (sysfs, power management).
- If packet headers weren't composed within `sk_buff`, each protocol layer
  would need to copy the packet into its own buffer.

---

## Part B: Decorator — Adding Behavior Without Changing the Core

### Mental Model

Add optional behavior by wrapping the "real" path: calls go to the
wrapper, which does extra work (filtering, logging, stats, security)
and then forwards to the inner implementation. In C: a chain of
handlers that each process and then forward.

```
     incoming call
          │
          ▼
  ┌────────────────┐
  │  Decorator 1   │  extra behavior (e.g. firewall rule check)
  │  (wrapper)     │
  └───────┬────────┘
          │ forward
          ▼
  ┌────────────────┐
  │  Decorator 2   │  extra behavior (e.g. NAT translation)
  │  (wrapper)     │
  └───────┬────────┘
          │ forward
          ▼
  ┌────────────────┐
  │  Real impl     │  the actual operation
  └────────────────┘
```

### In the Kernel (v3.2)

#### Example 1: Netfilter — Packet Hook Chain

`include/linux/netfilter.h`, lines 110–120:

```c
struct nf_hook_ops {
    struct list_head list;

    nf_hookfn *hook;          /* the filter function */
    struct module *owner;
    u_int8_t pf;              /* protocol family */
    unsigned int hooknum;     /* which hook point */
    int priority;             /* ordering */
};
```

Netfilter inserts hooks at defined points in the packet path. Each hook
can inspect, modify, accept, or drop the packet before it reaches the
next hook (or the real processing function):

```
  Packet arrives
       │
       ▼
  NF_HOOK(PF_INET, NF_INET_PRE_ROUTING, skb, ...)
       │
       ├── Hook 1: conntrack (priority -200)
       │     └── track connection state, modify if needed
       │     └── return NF_ACCEPT → continue
       │
       ├── Hook 2: iptables filter (priority 0)
       │     └── check firewall rules
       │     └── return NF_ACCEPT or NF_DROP
       │
       ├── Hook 3: custom module (priority 100)
       │     └── log the packet
       │     └── return NF_ACCEPT
       │
       └── All hooks passed → call okfn(skb)
              └── normal packet processing continues
```

**The dispatch mechanism:**

```c
/* include/linux/netfilter.h, line 238 — simplified */
static inline int
NF_HOOK(uint8_t pf, unsigned int hook, struct sk_buff *skb,
        struct net_device *in, struct net_device *out,
        int (*okfn)(struct sk_buff *))
{
    int ret = nf_hook_thresh(pf, hook, skb, in, out, okfn, INT_MIN);
    if (ret == 1)
        ret = okfn(skb);  /* all hooks accepted → proceed */
    return ret;
}
```

`nf_hook_slow()` walks the list of `nf_hook_ops` registered for this
hook point, calling each hook function. Each can:
- `NF_ACCEPT` → pass to next hook
- `NF_DROP` → discard packet, stop chain
- `NF_QUEUE` → send to userspace for decision
- `NF_STOLEN` → hook consumed the packet

This is the Decorator pattern: each hook wraps the next layer, adding
behavior (filtering, NAT, logging) without modifying the core packet
processing path.

#### Example 2: `compat_ioctl` — Compatibility Wrapper

```c
struct file_operations {
    /* ... */
    long (*unlocked_ioctl)(struct file *, unsigned int, unsigned long);
    long (*compat_ioctl)(struct file *, unsigned int, unsigned long);
    /* ... */
};
```

On a 64-bit kernel, 32-bit userspace programs issue `ioctl` with 32-bit
struct layouts. `compat_ioctl` is a decorator: it translates 32-bit
arguments into 64-bit format, then delegates to the real ioctl:

```
  32-bit ioctl call
       │
       ▼
  compat_ioctl()           ← DECORATOR
  ├── translate arg layout
  ├── call unlocked_ioctl() ← REAL IMPL
  └── translate result back
```

#### Example 3: Security Module Hooks (LSM)

The Linux Security Module framework inserts hooks at security-sensitive
points. Each hook can deny an operation without changing the core path:

```
  sys_open(filename, flags)
       │
       ├── inode_permission()     ← normal DAC check
       │
       ├── security_inode_permission()  ← LSM HOOK (decorator)
       │     └── SELinux: check policy
       │     └── AppArmor: check profile
       │     └── if denied → return -EACCES
       │
       └── proceed with open
```

The LSM hooks decorate the VFS operations. Removing the security module
(or building without CONFIG_SECURITY) removes the decorator; the core
path is unchanged.

### Why Decorator Here

Cross-cutting concerns (security, tracing, packet filtering, compatibility)
can be added in one place by wrapping the core path. No need to edit every
call site in the kernel.

**The kernel typically represents "the inner object" as:**
- A pointer to the next ops struct (netfilter chain)
- A direct call to the real function (compat_ioctl → unlocked_ioctl)
- A chain of registered hooks (LSM)

---

## Part C: Builder — Multi-Step Initialization With Clear Ordering

### Mental Model

Constructing a complex object requires multiple steps in a fixed order.
A "builder" (or a set of init helpers) enforces that order and keeps
partial states valid. In C: a set of `*_init` or `*_setup` functions
documented or coded to be called in sequence.

```
  BUILDER SEQUENCE
  ┌──────────────────────────────────────────────┐
  │  Step 1: alloc_thing()       → allocate      │
  │  Step 2: thing_set_ops()     → configure     │
  │  Step 3: thing_add_queues()  → add parts     │
  │  Step 4: register_thing()    → publish       │
  └──────────────────────────────────────────────┘
       ORDER MATTERS. Swapping steps → crash.
```

### In the Kernel (v3.2)

#### Example 1: Network Device — Multi-Step Build

A typical network driver's probe function follows a strict build sequence:

```c
static int __devinit
my_driver_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
    struct net_device *netdev;
    struct my_priv *priv;

    /* Step 1: Allocate (factory) — sets up queues and basic fields */
    netdev = alloc_etherdev(sizeof(struct my_priv));

    /* Step 2: Set up PCI resources — must happen before hardware access */
    pci_enable_device(pdev);
    pci_set_master(pdev);
    bars = pci_select_bars(pdev, IORESOURCE_MEM);
    pci_request_selected_regions(pdev, bars, "my_driver");

    /* Step 3: Map hardware registers */
    priv = netdev_priv(netdev);
    priv->hw_addr = pci_iomap(pdev, 0, 0);

    /* Step 4: Configure behavior (install ops) */
    netdev->netdev_ops = &my_netdev_ops;
    netdev->ethtool_ops = &my_ethtool_ops;

    /* Step 5: Read hardware identity */
    my_read_mac_address(priv, netdev->dev_addr);

    /* Step 6: Register with the network stack (publish) */
    register_netdev(netdev);

    return 0;
}
```

**What goes wrong if you swap steps:**

```
  Wrong order                         What happens
  ────────────────────────────────    ────────────────────────────
  register_netdev before set ops  →  stack tries to use NULL ops → OOPS
  read MAC before iomap           →  dereference NULL hw_addr → OOPS
  pci_set_master before enable    →  bus mastering on disabled device
  register_netdev before MAC read →  device visible with 00:00:00:00:00:00
```

#### Example 2: Request Queue — Block Device Build

```c
/* Creating a block device request queue (simplified): */

/* Step 1: Allocate the queue */
struct request_queue *q = blk_init_queue(my_request_fn, &my_lock);

/* Step 2: Set queue properties */
blk_queue_logical_block_size(q, 512);
blk_queue_max_hw_sectors(q, 255);

/* Step 3: Allocate the gendisk */
struct gendisk *disk = alloc_disk(1);

/* Step 4: Configure the disk */
disk->major = my_major;
disk->first_minor = 0;
disk->fops = &my_block_ops;
disk->queue = q;        /* link queue to disk */
set_capacity(disk, my_size);

/* Step 5: Register (publish) */
add_disk(disk);
```

Each step depends on the previous. The queue must exist before the disk
links to it. The disk must be configured before `add_disk` makes it
visible to the system.

#### Example 3: Boot Sequence — Building the Running Kernel

The entire boot process is a builder:

```
  Boot build sequence:
  ├── Level 0: pure_initcall       ← memory fundamentals
  ├── Level 1: core_initcall       ← core subsystems (slab, scheduler)
  ├── Level 2: postcore_initcall   ← post-core (device model)
  ├── Level 3: arch_initcall       ← architecture-specific
  ├── Level 4: subsys_initcall     ← subsystems (networking, block)
  ├── Level 5: fs_initcall         ← filesystems
  ├── Level 6: device_initcall     ← device drivers
  ├── Level 7: late_initcall       ← final setup
  └── exec /sbin/init              ← system ready
```

Each level depends on the previous. Device drivers (level 6) cannot run
before the subsystems they register with (level 4). Filesystems (level 5)
need the block layer (level 4). The ordering is the "build plan" for the
running kernel.

### Why Builder Here

Complex objects (queues, devices, the running kernel) have ordering and
dependency constraints. Centralizing construction reduces use-after-free
and half-initialized bugs.

**How the kernel documents the "right" order:**
- **Naming convention:** `alloc` → `init`/`setup` → `register`/`add`
- **Placement in one function:** the probe/init function is the builder
- **Linker sections:** initcall levels enforce boot ordering at link time
- **Comments:** headers document required call sequences

---

## How the Three Patterns Interact

In a single driver probe function, all three patterns appear:

```
  e1000_probe() {
      /* FACTORY: create the composed object */
      netdev = alloc_etherdev(sizeof(struct e1000_adapter));

      /* BUILDER: multi-step init in correct order */
      pci_enable_device(pdev);
      priv->hw_addr = pci_iomap(pdev, ...);

      /* COMPOSITION: link composed parts */
      netdev->netdev_ops = &e1000e_netdev_ops;    /* behavior */
      adapter->napi = ...;                          /* polling */
      adapter->tx_ring = ...;                       /* TX queue */

      /* BUILDER: final publish step */
      register_netdev(netdev);
  }
```

And the object's behavior is extended at runtime:

```
  /* DECORATOR: netfilter hooks wrap packet processing */
  NF_HOOK(PF_INET, NF_INET_LOCAL_IN, skb, ...)
       → firewall check → NAT → real processing
```

---

## Check Your Understanding

1. Pick one field inside `task_struct` that represents a subsystem (e.g.
   `mm`, `fs`). Trace one use of that field and say what "part" it
   represents in the overall process model.

2. Give one example where the kernel prefers "struct A has a struct B"
   over "A and B are the same struct."

3. Find one place where a "middle" layer calls into a "lower" layer
   after doing its own work (Decorator). What extra behavior does the
   middle layer add?

4. How does the kernel typically represent "the inner object" in a
   Decorator (pointer to ops? pointer to struct? chain list)?

5. Pick one multi-step init sequence in the kernel. What goes wrong if
   you swap two steps or skip one?

6. How does the kernel usually document or enforce the "right" init
   order (naming, placement in one file, linker sections)?

---

## Grand Summary: All Twelve Patterns

| # | Pattern | C Mechanism | Primary Kernel Example |
|---|---------|-------------|----------------------|
| 1 | **Strategy** | Ops struct swapped at runtime | `sched_class`, `tcp_congestion_ops` |
| 2 | **Template Method** | Framework function calls ops hooks | `vfs_read()`, `copy_process()` |
| 3 | **Observer** | Notifier chain + register/notify | `notifier_block`, netdev events |
| 4 | **Composition** | Struct embedding / pointer to substruct | `task_struct` components |
| 5 | **Factory** | alloc + init + register helpers | `alloc_netdev`, `kmem_cache_create` |
| 6 | **Singleton** | Global/per-CPU struct, init-once | Run queues, buddy allocator |
| 7 | **Iterator** | start/next/stop or `list_for_each_entry` | `seq_file`, list macros |
| 8 | **Adapter** | Ops struct translating upper→lower | `file_operations` per FS |
| 9 | **Facade** | Simple public API over complex internals | `kmalloc`, `schedule()` |
| 10 | **State** | Enum + switch on events | TCP states, device reg states |
| 11 | **Decorator** | Hook chain wrapping the real path | Netfilter, LSM, `compat_ioctl` |
| 12 | **Builder** | Multi-step init in required order | Driver probe, boot initcalls |

---

## What to Do Next

1. **Pick any subsystem** you haven't read before. Apply the
   [eight-question framework](../kernel_oop_masterclass/06_synthesis.md#61-the-analysis-framework)
   and identify as many of these twelve patterns as you can.

2. **Write a one-page summary** for that subsystem listing: the main
   structs (objects), their ops structs (interfaces), the lifecycle
   (constructor → use → destructor), and the patterns at play.

3. **Design your own:** When you next write a C subsystem, consciously
   choose from this pattern catalog. Document which pattern you chose
   and why.

---

[Back to Overview](00_overview.md)
