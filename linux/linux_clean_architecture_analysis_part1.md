# Linux Kernel v3.2 — Clean Architecture Analysis (Part 1)

## 1. Process Management

### 1.1 Subsystem Overview

The process management subsystem is the heart of the kernel. It owns task
lifecycle (creation, scheduling, context switching, termination) and CPU time
distribution.

**Key Directories and Files:**

| File                        | Purpose                                         |
|-----------------------------|-------------------------------------------------|
| `kernel/sched.c`            | Core scheduler: `schedule()`, `context_switch()` |
| `kernel/sched_fair.c`       | CFS (Completely Fair Scheduler)                  |
| `kernel/sched_rt.c`         | Real-time scheduler (FIFO, RR)                   |
| `kernel/sched_idletask.c`   | Idle task scheduler                              |
| `kernel/sched_stoptask.c`   | Stop task scheduler (highest priority)           |
| `kernel/fork.c`             | `do_fork()`, `copy_process()`                    |
| `include/linux/sched.h`     | `task_struct`, `sched_class`, `sched_entity`     |

**Build structure note:** `sched.c` directly `#include`s the scheduler class
files to form a single compilation unit:

```c
/* kernel/sched.c lines 2196-2203 */
#include "sched_idletask.c"
#include "sched_fair.c"
#include "sched_rt.c"
#include "sched_autogroup.c"
#include "sched_stoptask.c"
```

### 1.2 Entities (Stable Layer)

#### struct task_struct (`include/linux/sched.h:1220`)

The most fundamental kernel entity — represents a process/thread:

```c
struct task_struct {
    volatile long state;           /* -1 unrunnable, 0 runnable, >0 stopped */
    void *stack;
    unsigned int flags;
    int on_rq;

    int prio, static_prio, normal_prio;
    unsigned int rt_priority;
    const struct sched_class *sched_class;  /* -> interface adapter */
    struct sched_entity se;                 /* CFS scheduling entity */
    struct sched_rt_entity rt;              /* RT scheduling entity */

    unsigned int policy;                    /* SCHED_NORMAL, SCHED_FIFO, etc. */
    cpumask_t cpus_allowed;
    struct mm_struct *mm;                   /* memory descriptor */
    struct files_struct *files;             /* open file table */
    /* ... 200+ more fields ... */
};
```

**Why this is stable:** Every kernel subsystem interacts with `task_struct`.
Changing its layout requires coordinating across scheduling, memory management,
signal handling, credentials, namespaces, and tracing. Fields are added but
almost never removed.

#### struct sched_entity (`include/linux/sched.h:1169`)

The CFS per-task scheduling state:

```c
struct sched_entity {
    struct load_weight  load;
    struct rb_node      run_node;       /* position in CFS red-black tree */
    unsigned int        on_rq;

    u64 exec_start;
    u64 sum_exec_runtime;
    u64 vruntime;                       /* virtual runtime — CFS core metric */
    u64 prev_sum_exec_runtime;
};
```

#### struct rq (`kernel/sched.c:591`)

The per-CPU run queue — one per processor:

```c
struct rq {
    raw_spinlock_t lock;
    unsigned long nr_running;

    struct cfs_rq cfs;                  /* CFS run queue */
    struct rt_rq rt;                    /* RT run queue */

    struct task_struct *curr, *idle, *stop;
    u64 clock;
    u64 clock_task;
};

static DEFINE_PER_CPU_SHARED_ALIGNED(struct rq, runqueues);
```

### 1.3 Use Cases (Policy Logic)

The use-case layer contains the scheduling decisions and process lifecycle
management — the "what" of process management.

#### schedule() — The Central Dispatch

`kernel/sched.c:4486`:

```c
asmlinkage void __sched schedule(void)
{
    struct task_struct *tsk = current;
    sched_submit_work(tsk);
    __schedule();
}
```

`__schedule()` implements the core scheduling algorithm:

1. Disable preemption, acquire the per-CPU runqueue
2. If the current task is no longer runnable, dequeue it
3. Call `put_prev_task()` to update accounting for the outgoing task
4. Call `pick_next_task()` to select the next task
5. If the chosen task differs from current, perform `context_switch()`

#### pick_next_task() — Policy Dispatch via Interface

`kernel/sched.c:4368`:

```c
static inline struct task_struct *
pick_next_task(struct rq *rq)
{
    const struct sched_class *class;
    struct task_struct *p;

    /* Fast path: if all tasks are CFS, skip class iteration */
    if (likely(rq->nr_running == rq->cfs.h_nr_running)) {
        p = fair_sched_class.pick_next_task(rq);
        if (likely(p))
            return p;
    }

    /* Walk the priority chain: stop -> rt -> fair -> idle */
    for_each_class(class) {
        p = class->pick_next_task(rq);
        if (p)
            return p;
    }

    BUG(); /* idle class always has a runnable task */
}
```

#### do_fork() — Process Creation

`kernel/fork.c:1461`:

```c
long do_fork(unsigned long clone_flags, unsigned long stack_start,
             struct pt_regs *regs, unsigned long stack_size,
             int __user *parent_tidptr, int __user *child_tidptr)
{
    struct task_struct *p;
    p = copy_process(clone_flags, stack_start, regs, stack_size,
                     child_tidptr, NULL, trace);
    /* ... */
    wake_up_new_task(p);
    return nr;
}
```

`copy_process()` allocates a new `task_struct`, copies/shares resources,
and calls `sched_fork()` which assigns the scheduling class:

```c
/* kernel/sched.c:3011 */
if (!rt_prio(p->prio))
    p->sched_class = &fair_sched_class;

if (p->sched_class->task_fork)
    p->sched_class->task_fork(p);
```

### 1.4 Interface Adapters — struct sched_class

`include/linux/sched.h:1084`:

```c
struct sched_class {
    const struct sched_class *next;

    void (*enqueue_task)    (struct rq *rq, struct task_struct *p, int flags);
    void (*dequeue_task)    (struct rq *rq, struct task_struct *p, int flags);
    void (*yield_task)      (struct rq *rq);
    void (*check_preempt_curr)(struct rq *rq, struct task_struct *p, int flags);

    struct task_struct *(*pick_next_task)(struct rq *rq);
    void (*put_prev_task)   (struct rq *rq, struct task_struct *p);

    void (*set_curr_task)   (struct rq *rq);
    void (*task_tick)       (struct rq *rq, struct task_struct *p, int queued);
    void (*task_fork)       (struct task_struct *p);

    void (*switched_from)   (struct rq *rq, struct task_struct *task);
    void (*switched_to)     (struct rq *rq, struct task_struct *task);
    void (*prio_changed)    (struct rq *rq, struct task_struct *task, int oldprio);
    unsigned int (*get_rr_interval)(struct rq *rq, struct task_struct *task);
};
```

This struct is the **dependency inversion point**. The core scheduler
(`schedule()`, `pick_next_task()`) depends only on this interface. It never
calls CFS or RT functions directly (except the fast path optimization).

**Class priority chain:**

```
stop_sched_class -> rt_sched_class -> fair_sched_class -> idle_sched_class
  (highest)                                                  (lowest, .next=NULL)
```

The `for_each_class` macro walks this linked list:

```c
#define sched_class_highest (&stop_sched_class)
#define for_each_class(class) \
    for (class = sched_class_highest; class; class = class->next)
```

**Dispatch examples in core scheduler:**

```c
/* kernel/sched.c:1943 — enqueue uses interface */
static void enqueue_task(struct rq *rq, struct task_struct *p, int flags)
{
    update_rq_clock(rq);
    p->sched_class->enqueue_task(rq, p, flags);
}

/* kernel/sched.c:4357 — put_prev_task uses interface */
static void put_prev_task(struct rq *rq, struct task_struct *prev)
{
    if (prev->on_rq || rq->skip_clock_update < 0)
        update_rq_clock(rq);
    prev->sched_class->put_prev_task(rq, prev);
}
```

### 1.5 Outer Implementation Layer

#### CFS Implementation (`kernel/sched_fair.c:5044`)

```c
static const struct sched_class fair_sched_class = {
    .next               = &idle_sched_class,
    .enqueue_task       = enqueue_task_fair,
    .dequeue_task       = dequeue_task_fair,
    .yield_task         = yield_task_fair,
    .check_preempt_curr = check_preempt_wakeup,
    .pick_next_task     = pick_next_task_fair,
    .put_prev_task      = put_prev_task_fair,
    .set_curr_task      = set_curr_task_fair,
    .task_tick          = task_tick_fair,
    .task_fork          = task_fork_fair,
    .prio_changed       = prio_changed_fair,
    .switched_from      = switched_from_fair,
    .switched_to        = switched_to_fair,
    .get_rr_interval    = get_rr_interval_fair,
};
```

CFS uses a red-black tree keyed by `vruntime`. `pick_next_task_fair()` selects
the leftmost node (smallest vruntime). `enqueue_task_fair()` inserts into the
tree. `task_tick_fair()` updates vruntime and checks for preemption.

#### RT Implementation (`kernel/sched_rt.c:1803`)

```c
static const struct sched_class rt_sched_class = {
    .next               = &fair_sched_class,
    .enqueue_task       = enqueue_task_rt,
    .dequeue_task       = dequeue_task_rt,
    .check_preempt_curr = check_preempt_curr_rt,
    .pick_next_task     = pick_next_task_rt,
    .put_prev_task      = put_prev_task_rt,
    .set_curr_task      = set_curr_task_rt,
    .task_tick          = task_tick_rt,
    .get_rr_interval    = get_rr_interval_rt,
};
```

RT uses a bitmap + linked list structure. `pick_next_task_rt()` finds the
highest-priority runnable RT task in O(1).

### 1.6 Execution Flow

```
User calls fork() / clone()
        |
        v
  sys_fork() / sys_clone()
        |
        v
  do_fork()                               [kernel/fork.c:1461]
        |
        v
  copy_process()                          [kernel/fork.c:1042]
        |--- sched_fork()                 [kernel/sched.c:2972]
        |       |--- p->sched_class = &fair_sched_class
        |       |--- p->sched_class->task_fork(p)
        |
        v
  wake_up_new_task(p)                     [kernel/sched.c:3055]
        |--- activate_task(rq, p, 0)
        |       |--- enqueue_task(rq, p, flags)
        |               |--- p->sched_class->enqueue_task(rq, p, flags)
        |--- check_preempt_curr(rq, p, ...)

... later, on timer tick or explicit yield ...

  schedule()                              [kernel/sched.c:4486]
        |
        v
  __schedule()                            [kernel/sched.c:4395]
        |--- put_prev_task(rq, prev)
        |       |--- prev->sched_class->put_prev_task(rq, prev)
        |--- next = pick_next_task(rq)
        |       |--- for_each_class(class)
        |               |--- class->pick_next_task(rq)
        |--- context_switch(rq, prev, next)
                |--- switch_mm()          [arch-specific]
                |--- switch_to()          [arch-specific]
```

### 1.7 Dependency Flow

```
Dependency Direction (inward):

  sched_fair.c  ----+
  sched_rt.c    ----|---->  struct sched_class  ---->  task_struct
  sched_idle.c  ----+      (include/linux/sched.h)    (include/linux/sched.h)
  sched_stop.c  ----+

  (implementations)        (interface)                 (entity)
```

- `kernel/sched_fair.c` includes `linux/sched.h` (where `sched_class` is
  defined). It fills in the function pointers.
- `kernel/sched.c` includes `linux/sched.h` and dispatches through
  `p->sched_class->*`.
- Neither `sched.c` nor `sched.h` has any knowledge of CFS internals
  (red-black trees, vruntime calculations).

### 1.8 Architecture Diagram

```
                    EXECUTION FLOW
                    ==============

              User Process (fork/yield/sleep)
                        |
                        v
                  System Call Layer
                        |
                        v
              +--------------------+
              |   schedule()       |       USE CASE
              |   do_fork()        |       (Policy)
              |   wake_up_process()|
              +--------+-----------+
                       |
            dispatches via sched_class
                       |
                       v
              +--------------------+
              |  struct sched_class|       INTERFACE ADAPTER
              |  (function ptrs)   |       (Abstraction)
              +--------+-----------+
                       |
          +------------+------------+
          |            |            |
          v            v            v
   +----------+  +----------+  +----------+
   |   CFS    |  |    RT    |  |   Idle   |  OUTER LAYER
   |sched_fair|  |sched_rt  |  |sched_idle|  (Implementation)
   +----------+  +----------+  +----------+


                  DEPENDENCY FLOW
                  ===============

   CFS / RT / Idle -----> sched_class -----> task_struct
   (outer impl)           (interface)        (entity)

   Dependencies point INWARD toward the stable center.
```

### 1.9 Clean Architecture Insights

**Alignment:**

- `sched_class` is a textbook Dependency Inversion interface. The core
  scheduler depends on the abstract `sched_class`; concrete schedulers
  (CFS, RT) implement it.
- Adding a new scheduling class requires zero changes to `schedule()` or
  `pick_next_task()` — only a new implementation of `sched_class` and
  insertion into the priority chain.
- The scheduler is a **plugin architecture**: scheduling policy is a
  pluggable component.

**Divergence:**

- The `#include "sched_fair.c"` pattern merges all scheduler implementations
  into one translation unit. This is a performance optimization (allows
  inlining across class boundaries) but violates Clean Architecture's
  module-boundary expectations.
- `task_struct` is a god-object containing fields for every subsystem.
  Clean Architecture would prefer smaller, focused entities.
- The CFS fast-path in `pick_next_task()` is a deliberate abstraction leak
  for performance.

---

## 2. Memory Management

### 2.1 Subsystem Overview

The memory management subsystem handles virtual memory, page allocation,
memory mapping, page faults, swap, and the slab allocator.

**Key Files:**

| File                    | Purpose                                          |
|-------------------------|--------------------------------------------------|
| `mm/memory.c`           | Page fault handling, `handle_mm_fault`            |
| `mm/mmap.c`             | `do_mmap_pgoff()`, `do_brk()`, VMA management    |
| `mm/page_alloc.c`       | Buddy allocator, `__alloc_pages_nodemask()`       |
| `mm/filemap.c`          | Page cache, `filemap_fault()`                     |
| `mm/slab.c` / `slub.c`  | Slab/SLUB object allocators                      |
| `mm/vmscan.c`           | Page reclaim                                      |
| `mm/rmap.c`             | Reverse mapping                                   |
| `arch/x86/mm/fault.c`   | x86 page fault entry point                       |
| `include/linux/mm.h`    | `vm_operations_struct`, core MM declarations       |
| `include/linux/mm_types.h` | `vm_area_struct`, `mm_struct`, `page`           |

### 2.2 Entities (Stable Layer)

#### struct page (`include/linux/mm_types.h:40`)

The most primitive entity — represents a single physical page frame:

```c
struct page {
    unsigned long flags;                  /* PG_locked, PG_dirty, etc. */
    struct address_space *mapping;        /* file mapping or anon_vma */
    union {
        pgoff_t index;                    /* offset within mapping */
        void *freelist;                   /* SLUB first free object */
    };
    union {
        struct list_head lru;             /* page reclaim lists */
    };
    union {
        unsigned long private;
        struct kmem_cache *slab;          /* SLUB: owning cache */
        struct page *first_page;          /* compound page head */
    };
};
```

#### struct vm_area_struct (`include/linux/mm_types.h:201`)

Describes a contiguous virtual memory region within a process:

```c
struct vm_area_struct {
    struct mm_struct *vm_mm;
    unsigned long vm_start;
    unsigned long vm_end;
    struct vm_area_struct *vm_next, *vm_prev;
    pgprot_t vm_page_prot;
    unsigned long vm_flags;
    struct rb_node vm_rb;

    const struct vm_operations_struct *vm_ops;   /* -> interface adapter */

    unsigned long vm_pgoff;
    struct file *vm_file;                        /* backing file, or NULL */
    struct anon_vma *anon_vma;
};
```

#### struct mm_struct (`include/linux/mm_types.h:289`)

Per-process memory descriptor:

```c
struct mm_struct {
    struct vm_area_struct *mmap;          /* VMA list head */
    struct rb_root mm_rb;                 /* VMA red-black tree */
    pgd_t *pgd;                          /* page global directory */
    atomic_t mm_users;
    atomic_t mm_count;
    unsigned long total_vm, locked_vm;
    unsigned long start_code, end_code;
    unsigned long start_brk, brk;
    unsigned long start_stack;
};
```

#### struct address_space (`include/linux/fs.h:636`)

Links a file to its cached pages:

```c
struct address_space {
    struct inode *host;
    struct radix_tree_root page_tree;
    unsigned long nrpages;
    const struct address_space_operations *a_ops;   /* -> interface adapter */
    struct backing_dev_info *backing_dev_info;
};
```

### 2.3 Use Cases (Policy Logic)

#### handle_mm_fault() — Page Fault Resolution

`mm/memory.c:3442`:

```c
int handle_mm_fault(struct mm_struct *mm, struct vm_area_struct *vma,
                    unsigned long address, unsigned int flags)
```

This is the central policy function for resolving page faults. It walks the
page table hierarchy (pgd → pud → pmd → pte) and delegates to:

- `do_anonymous_page()` — for anonymous (heap/stack) mappings
- `do_linear_fault()` → `__do_fault()` — for file-backed mappings
- `do_swap_page()` — for swapped-out pages
- `do_nonlinear_fault()` — for nonlinear file mappings

#### __do_fault() — Interface Dispatch Point

`mm/memory.c:3145`:

```c
static int __do_fault(struct mm_struct *mm, struct vm_area_struct *vma,
                      unsigned long address, pmd_t *pmd, pgoff_t pgoff,
                      unsigned int flags, pte_t orig_pte)
{
    struct vm_fault vmf;
    /* ... setup vmf ... */
    ret = vma->vm_ops->fault(vma, &vmf);    /* <-- DISPATCH */
    /* ... install page into page table ... */
}
```

#### __alloc_pages_nodemask() — Physical Page Allocation

`mm/page_alloc.c:2255`: The core buddy allocator entry point, called by
all subsystems needing physical memory.

### 2.4 Interface Adapters

#### struct vm_operations_struct (`include/linux/mm.h:204`)

```c
struct vm_operations_struct {
    void (*open)(struct vm_area_struct *area);
    void (*close)(struct vm_area_struct *area);
    int (*fault)(struct vm_area_struct *vma, struct vm_fault *vmf);
    int (*page_mkwrite)(struct vm_area_struct *vma, struct vm_fault *vmf);
    int (*access)(struct vm_area_struct *vma, unsigned long addr,
                  void *buf, int len, int write);
};
```

This is the key dependency inversion point: the MM core calls `vm_ops->fault()`
without knowing whether the VMA is backed by ext4, tmpfs, or a device driver.

#### struct address_space_operations (`include/linux/fs.h:583`)

```c
struct address_space_operations {
    int (*writepage)(struct page *, struct writeback_control *);
    int (*readpage)(struct file *, struct page *);
    int (*writepages)(struct address_space *, struct writeback_control *);
    int (*readpages)(struct file *, struct address_space *,
                     struct list_head *, unsigned);
    int (*write_begin)(struct file *, struct address_space *,
                       loff_t, unsigned, unsigned, struct page **, void **);
    int (*write_end)(struct file *, struct address_space *,
                     loff_t, unsigned, unsigned, struct page *, void *);
    sector_t (*bmap)(struct address_space *, sector_t);
    ssize_t (*direct_IO)(int, struct kiocb *, const struct iovec *,
                         loff_t, unsigned long);
};
```

This separates page cache policy (in `mm/filemap.c`) from filesystem-specific
I/O (in `fs/ext4/inode.c`, etc.).

### 2.5 Outer Implementation Layer

**Generic file mapping implementation** (`mm/filemap.c:1655`):

```c
/* Used by most filesystems via generic_file_vm_ops */
int filemap_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
    /* find page in page cache */
    page = find_get_page(mapping, offset);
    if (!page) {
        /* read from disk */
        page_cache_read(file, offset);
    }
    /* ... */
    vmf->page = page;
    return ret;
}

const struct vm_operations_struct generic_file_vm_ops = {
    .fault  = filemap_fault,
};
```

Most filesystems use `generic_file_vm_ops` for their VMA operations. The
actual disk I/O is further delegated through `address_space_operations`.

**Slab allocator abstraction** (`include/linux/slab.h`):

```c
/* Common API — implementation selected at config time */
#ifdef CONFIG_SLUB
#include <linux/slub_def.h>
#elif defined(CONFIG_SLOB)
#include <linux/slob_def.h>
#else
#include <linux/slab_def.h>
#endif

struct kmem_cache *kmem_cache_create(const char *, size_t, size_t,
                                     unsigned long, void (*)(void *));
void *kmem_cache_alloc(struct kmem_cache *, gfp_t);
void kmem_cache_free(struct kmem_cache *, void *);
```

The slab allocator uses a **compile-time strategy pattern**: the implementation
(SLAB, SLUB, or SLOB) is selected via `CONFIG_*` options, but all three provide
the identical `kmem_cache_*` API.

### 2.6 Execution Flow — Page Fault Path

```
CPU raises page fault exception
        |
        v
  do_page_fault()                         [arch/x86/mm/fault.c:994]
        |--- address = read_cr2()
        |--- find_vma(mm, address)
        |
        v
  handle_mm_fault()                       [mm/memory.c:3442]
        |--- Walk page tables: pgd -> pud -> pmd -> pte
        |
        v
  handle_pte_fault()                      [mm/memory.c:3355]
        |
        +--- pte_none && vm_ops->fault ?
        |       |
        |       v
        |   do_linear_fault()
        |       |
        |       v
        |   __do_fault()                  [mm/memory.c:3145]
        |       |
        |       v
        |   vma->vm_ops->fault(vma, &vmf) [DISPATCH to filesystem]
        |       |
        |       v
        |   filemap_fault()               [mm/filemap.c:1655]
        |       |
        |       v
        |   mapping->a_ops->readpage()    [DISPATCH to fs I/O]
        |
        +--- pte_none && !vm_ops ?
        |       |
        |       v
        |   do_anonymous_page()           [allocate zero page]
        |
        +--- !pte_present ?
                |
                v
            do_swap_page()                [read from swap]
```

### 2.7 Dependency Flow

```
  arch/x86/mm/fault.c  ------>  include/linux/mm.h  ------>  include/linux/mm_types.h
  (arch entry point)            (vm_operations_struct)        (vm_area_struct, page)

  mm/filemap.c  -------------->  include/linux/fs.h
  (generic fault handler)       (address_space_operations)

  fs/ext4/inode.c  ----------->  include/linux/fs.h
  (ext4 a_ops implementation)   (address_space_operations)

  Dependencies point INWARD:

  arch code ----> mm/ core ----> mm entity structs
  fs/ext4   ----> fs.h interfaces ----> mm entity structs
```

### 2.8 Architecture Diagram

```
                   EXECUTION FLOW
                   ==============

            CPU Page Fault Exception
                      |
                      v
            +-------------------+
            | do_page_fault()   |       OUTER (Arch-specific)
            | arch/x86/mm/     |
            +--------+----------+
                     |
                     v
            +-------------------+
            | handle_mm_fault() |       USE CASE (Policy)
            | handle_pte_fault()|
            | mm/memory.c      |
            +--------+----------+
                     |
                     v
            +-------------------+
            | vm_operations_    |       INTERFACE ADAPTER
            |   struct          |
            | (.fault, .open,   |
            |  .page_mkwrite)   |
            +--------+----------+
                     |
          +----------+----------+
          |                     |
          v                     v
  +---------------+    +----------------+
  | filemap_fault |    | shmem_fault    |    OUTER (Implementation)
  | mm/filemap.c  |    | mm/shmem.c     |
  +-------+-------+    +----------------+
          |
          v
  +-------------------+
  | address_space_    |       INTERFACE ADAPTER (I/O layer)
  |   operations      |
  | (.readpage,       |
  |  .writepage)      |
  +--------+----------+
           |
           v
  +-------------------+
  | ext4_readpage()   |       OUTER (Filesystem I/O)
  | fs/ext4/inode.c   |
  +-------------------+


                DEPENDENCY FLOW
                ================

  arch/x86 ---> mm/ core ---> mm_types.h (entities)
  ext4     ---> fs.h (interfaces) ---> mm_types.h (entities)
  filemap  ---> fs.h (interfaces) ---> mm_types.h (entities)
```

### 2.9 Clean Architecture Insights

**Alignment:**

- Two layers of Dependency Inversion: `vm_operations_struct` (MM → filesystem
  fault handler) and `address_space_operations` (page cache → filesystem I/O).
- Arch-specific fault entry code depends on generic MM interfaces, not the
  reverse. x86 fault code includes `linux/mm.h`; `linux/mm.h` knows nothing
  about x86.
- The page cache (`mm/filemap.c`) is a shared use-case module that most
  filesystems reuse via `generic_file_vm_ops`.

**Divergence:**

- The slab allocator uses compile-time selection rather than runtime dispatch.
  This is an architectural choice driven by performance — an extra function
  pointer dereference on every allocation would be costly.
- `struct page` is a heavily overloaded union structure. Different subsystems
  interpret the same bytes differently (file mapping vs. slab object vs.
  compound page). This violates the single-responsibility ideal.

---

## 3. Virtual File System (VFS)

### 3.1 Subsystem Overview

The VFS is the kernel's most celebrated example of interface-driven
architecture. It defines a uniform API for file operations and delegates
to concrete filesystem implementations.

**Key Files:**

| File                    | Purpose                                        |
|-------------------------|------------------------------------------------|
| `fs/read_write.c`       | `vfs_read()`, `vfs_write()`, syscall wrappers  |
| `fs/open.c`             | `do_sys_open()`, file open path                |
| `fs/namei.c`            | Path resolution, `path_lookup()`               |
| `fs/super.c`            | Superblock management                          |
| `fs/inode.c`            | Inode allocation and lifecycle                 |
| `fs/dcache.c`           | Dentry cache                                   |
| `fs/filesystems.c`      | `register_filesystem()`                        |
| `include/linux/fs.h`    | All core VFS structures and interfaces         |
| `include/linux/dcache.h`| `struct dentry`                                |

### 3.2 Entities (Stable Layer)

#### struct inode (`include/linux/fs.h:749`)

The in-memory representation of a file's metadata:

```c
struct inode {
    umode_t         i_mode;
    uid_t           i_uid;
    gid_t           i_gid;
    unsigned long   i_ino;
    dev_t           i_rdev;
    loff_t          i_size;
    struct timespec i_atime, i_mtime, i_ctime;

    const struct inode_operations   *i_op;   /* -> interface adapter */
    const struct file_operations    *i_fop;  /* default for new opens */
    struct super_block              *i_sb;
    struct address_space            *i_mapping;
    struct address_space            i_data;
};
```

#### struct dentry (`include/linux/dcache.h:116`)

A directory entry in the path-lookup cache:

```c
struct dentry {
    unsigned int d_flags;
    struct dentry *d_parent;
    struct qstr d_name;
    struct inode *d_inode;
    const struct dentry_operations *d_op;
    struct super_block *d_sb;
    struct list_head d_subdirs;
    struct list_head d_alias;
};
```

#### struct file (`include/linux/fs.h:964`)

An open file instance:

```c
struct file {
    struct path                     f_path;
    const struct file_operations    *f_op;    /* -> interface adapter */
    atomic_long_t                   f_count;
    unsigned int                    f_flags;
    fmode_t                         f_mode;
    loff_t                          f_pos;
    struct address_space            *f_mapping;
    void                            *private_data;
};
```

#### struct super_block (`include/linux/fs.h:1400`)

Per-mounted-filesystem state:

```c
struct super_block {
    dev_t                           s_dev;
    unsigned long                   s_blocksize;
    loff_t                          s_maxbytes;
    struct file_system_type         *s_type;
    const struct super_operations   *s_op;     /* -> interface adapter */
    struct dentry                   *s_root;
    struct block_device             *s_bdev;
    void                            *s_fs_info;
};
```

### 3.3 Use Cases (Policy Logic)

#### vfs_read() — The Read Policy

`fs/read_write.c:364`:

```c
ssize_t vfs_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
    ssize_t ret;

    if (!(file->f_mode & FMODE_READ))
        return -EBADF;
    if (!file->f_op || (!file->f_op->read && !file->f_op->aio_read))
        return -EINVAL;

    ret = rw_verify_area(READ, file, pos, count);
    if (ret >= 0) {
        count = ret;
        if (file->f_op->read)
            ret = file->f_op->read(file, buf, count, pos);  /* DISPATCH */
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

`vfs_read()` enforces the VFS policy (permission checks, accounting, notification)
then dispatches to the filesystem through `file->f_op->read()`.

#### Syscall entry — sys_read()

`fs/read_write.c:460`:

```c
SYSCALL_DEFINE3(read, unsigned int, fd, char __user *, buf, size_t, count)
{
    struct file *file;
    file = fget_light(fd, &fput_needed);
    if (file) {
        loff_t pos = file_pos_read(file);
        ret = vfs_read(file, buf, count, &pos);
        file_pos_write(file, pos);
        fput_light(file, fput_needed);
    }
    return ret;
}
```

#### register_filesystem() — Plugin Registration

`fs/filesystems.c:69`:

```c
int register_filesystem(struct file_system_type *fs)
{
    struct file_system_type **p;
    write_lock(&file_systems_lock);
    p = find_filesystem(fs->name, strlen(fs->name));
    if (*p)
        res = -EBUSY;
    else
        *p = fs;
    write_unlock(&file_systems_lock);
    return res;
}
```

### 3.4 Interface Adapters

The VFS has **five** major interface structs — an exceptionally rich adapter layer:

| Interface                    | Purpose                              | Methods (key)                           |
|------------------------------|--------------------------------------|-----------------------------------------|
| `struct file_operations`     | Per-file I/O dispatch                | read, write, mmap, open, release, fsync |
| `struct inode_operations`    | Inode namespace operations           | lookup, create, link, unlink, mkdir     |
| `struct super_operations`    | Filesystem-level operations          | alloc_inode, write_inode, statfs        |
| `struct dentry_operations`   | Dentry cache behavior                | d_revalidate, d_hash, d_compare        |
| `struct address_space_operations` | Page cache I/O                  | readpage, writepage, write_begin        |

#### struct file_operations (`include/linux/fs.h:1583`)

```c
struct file_operations {
    struct module *owner;
    loff_t (*llseek)(struct file *, loff_t, int);
    ssize_t (*read)(struct file *, char __user *, size_t, loff_t *);
    ssize_t (*write)(struct file *, const char __user *, size_t, loff_t *);
    ssize_t (*aio_read)(struct kiocb *, const struct iovec *, unsigned long, loff_t);
    ssize_t (*aio_write)(struct kiocb *, const struct iovec *, unsigned long, loff_t);
    int (*readdir)(struct file *, void *, filldir_t);
    unsigned int (*poll)(struct file *, struct poll_table_struct *);
    long (*unlocked_ioctl)(struct file *, unsigned int, unsigned long);
    int (*mmap)(struct file *, struct vm_area_struct *);
    int (*open)(struct inode *, struct file *);
    int (*flush)(struct file *, fl_owner_t);
    int (*release)(struct inode *, struct file *);
    int (*fsync)(struct file *, loff_t, loff_t, int);
};
```

#### struct inode_operations (`include/linux/fs.h:1613`)

```c
struct inode_operations {
    struct dentry *(*lookup)(struct inode *, struct dentry *, struct nameidata *);
    int (*create)(struct inode *, struct dentry *, int, struct nameidata *);
    int (*link)(struct dentry *, struct inode *, struct dentry *);
    int (*unlink)(struct inode *, struct dentry *);
    int (*symlink)(struct inode *, struct dentry *, const char *);
    int (*mkdir)(struct inode *, struct dentry *, int);
    int (*rmdir)(struct inode *, struct dentry *);
    int (*rename)(struct inode *, struct dentry *, struct inode *, struct dentry *);
    int (*setattr)(struct dentry *, struct iattr *);
    int (*getattr)(struct vfsmount *, struct dentry *, struct kstat *);
};
```

### 3.5 Outer Implementation Layer — ext4 Example

**ext4 file_operations** (`fs/ext4/file.c:231`):

```c
const struct file_operations ext4_file_operations = {
    .llseek         = ext4_llseek,
    .read           = do_sync_read,
    .write          = do_sync_write,
    .aio_read       = generic_file_aio_read,
    .aio_write      = ext4_file_write,
    .unlocked_ioctl = ext4_ioctl,
    .mmap           = ext4_file_mmap,
    .open           = ext4_file_open,
    .release        = ext4_release_file,
    .fsync          = ext4_sync_file,
};
```

**ext4 registration** (`fs/ext4/super.c:5011`):

```c
static struct file_system_type ext4_fs_type = {
    .owner      = THIS_MODULE,
    .name       = "ext4",
    .mount      = ext4_mount,
    .kill_sb    = kill_block_super,
    .fs_flags   = FS_REQUIRES_DEV,
};

/* In ext4_init_fs(): */
err = register_filesystem(&ext4_fs_type);
```

**ext4 inode_operations** (`fs/ext4/file.c:249`):

```c
const struct inode_operations ext4_file_inode_operations = {
    .setattr    = ext4_setattr,
    .getattr    = ext4_getattr,
    .setxattr   = generic_setxattr,
    .getxattr   = generic_getxattr,
    .listxattr  = ext4_listxattr,
    .removexattr = generic_removexattr,
    .fiemap     = ext4_fiemap,
};
```

### 3.6 Execution Flow — read() Syscall

```
  User program: read(fd, buf, count)
        |
        v
  SYSCALL_DEFINE3(read, ...)              [fs/read_write.c:460]
        |--- fget_light(fd)               [get struct file from fd table]
        |
        v
  vfs_read(file, buf, count, &pos)        [fs/read_write.c:364]
        |--- permission check (f_mode)
        |--- rw_verify_area()
        |--- security_file_permission()   [LSM hook]
        |
        v
  file->f_op->read(file, buf, count, pos) [DISPATCH]
        |
        v
  do_sync_read() / generic_file_aio_read() [for ext4]
        |
        v
  do_generic_file_read()
        |--- find_get_page()              [page cache lookup]
        |--- if miss: mapping->a_ops->readpage()  [DISPATCH to ext4]
        |
        v
  ext4_readpage() -> mpage_readpage()
        |
        v
  submit_bio()                            [block layer]
        |
        v
  Device driver                           [e.g., SCSI, NVMe]
        |
        v
  Hardware
```

### 3.7 Dependency Flow

```
  fs/ext4/file.c   --------> include/linux/fs.h
  fs/ext4/super.c  --------> include/linux/fs.h
  fs/ext4/inode.c  --------> include/linux/fs.h

  include/linux/fs.h:
    - defines file_operations, inode_operations, super_operations
    - defines inode, file, super_block, dentry
    - has NO include of anything in fs/ext4/

  fs/read_write.c  --------> include/linux/fs.h

  Direction: ext4 -> VFS interfaces -> VFS entities
             (outer)  (adapter)        (stable core)
```

### 3.8 Architecture Diagram

```
                  EXECUTION FLOW
                  ==============

        User Program: read(fd, buf, count)
                      |
                      v
              +-----------------+
              |  sys_read()     |       SYSTEM CALL BOUNDARY
              +-----------------+
                      |
                      v
              +-----------------+
              |  vfs_read()     |       USE CASE (VFS Policy)
              |  - permissions  |
              |  - accounting   |
              |  - fsnotify     |
              +--------+--------+
                       |
           dispatches via f_op->read()
                       |
                       v
              +--------------------+
              | struct file_       |    INTERFACE ADAPTER
              |   operations       |
              | (.read, .write,    |
              |  .mmap, .fsync)    |
              +--------+-----------+
                       |
          +------------+------------+
          |            |            |
          v            v            v
   +----------+  +----------+  +----------+
   |   ext4   |  |   xfs    |  |  tmpfs   |  OUTER LAYER
   |file_ops  |  |file_ops  |  |file_ops  |  (FS Implementation)
   +----+-----+  +----------+  +----------+
        |
        v
   +--------------------+
   | address_space_     |              INTERFACE ADAPTER (I/O)
   |   operations       |
   | (.readpage,        |
   |  .writepage)       |
   +--------+-----------+
            |
            v
   +--------------------+
   | Block Layer        |              USE CASE (Block I/O)
   | submit_bio()       |
   +--------+-----------+
            |
            v
   +--------------------+
   | Device Driver      |              OUTER LAYER
   +--------+-----------+
            |
            v
         Hardware


                  DEPENDENCY FLOW
                  ===============

   ext4 -------> file_operations -------> inode / file / dentry
   (outer)       (interface, fs.h)        (entities, fs.h / dcache.h)

   block layer -> block structs
   device drv --> block layer interfaces -> kernel core
```

### 3.9 Clean Architecture Insights

**Alignment:**

- The VFS is the **purest example of Clean Architecture** in the kernel. It
  defines five orthogonal interface structs that completely decouple policy
  from implementation.
- Filesystems are literal **plugins**: they register via `register_filesystem()`,
  fill in function pointer structs, and can be loaded/unloaded as kernel modules.
- The VFS enforces the Dependency Rule: `fs/ext4/` depends on `include/linux/fs.h`,
  never the reverse.
- ext4 reuses generic implementations (`generic_file_aio_read`, `generic_setxattr`)
  where possible — the inner layer provides default use cases that outer layers
  can override selectively.

**Divergence:**

- Some VFS code has filesystem-specific checks (e.g., special handling for
  particular filesystem types), which leak implementation knowledge into the
  policy layer.
- `struct inode` contains a union of filesystem-specific data, meaning the
  entity struct has awareness of the range of possible implementations.

---

## 4. Network Stack

### 4.1 Subsystem Overview

The Linux network stack processes packets through a multi-layered architecture
with abstraction at each level: socket → transport → network → device.

**Key Files:**

| File                      | Purpose                                     |
|---------------------------|---------------------------------------------|
| `net/socket.c`            | BSD socket API, syscall entry               |
| `net/core/dev.c`          | Core device layer, packet receive path      |
| `net/ipv4/af_inet.c`      | IPv4 address family, protocol registration  |
| `net/ipv4/ip_input.c`     | IP receive path                             |
| `net/ipv4/udp.c`          | UDP transport                               |
| `net/ipv4/tcp.c`          | TCP transport                               |
| `include/linux/net.h`     | `socket`, `proto_ops`                       |
| `include/net/sock.h`      | `sock`, `proto`                             |
| `include/linux/netdevice.h` | `net_device`, `net_device_ops`            |
| `include/linux/skbuff.h`  | `sk_buff`                                   |

### 4.2 Entities (Stable Layer)

#### struct sk_buff (`include/linux/skbuff.h:372`)

The fundamental network data entity — a packet buffer:

```c
struct sk_buff {
    struct sk_buff *next, *prev;
    ktime_t tstamp;
    struct sock *sk;
    struct net_device *dev;
    char cb[48] __aligned(8);       /* control buffer — protocol-private */
    unsigned int len, data_len;
    __u16 mac_len, hdr_len;
    __u32 priority;
    __be16 protocol;                /* ETH_P_IP, ETH_P_ARP, etc. */
    int skb_iif;                    /* incoming interface index */
};
```

#### struct sock (`include/net/sock.h`)

The network-layer socket entity:

```c
struct sock {
    struct sock_common __sk_common;
    socket_lock_t sk_lock;
    struct sk_buff_head sk_receive_queue;
    struct sk_buff_head sk_write_queue;
    struct proto *sk_prot;              /* -> transport protocol interface */
    /* ... state, timers, routing, etc. */
};
```

#### struct socket (`include/linux/net.h:137`)

The user-facing socket entity:

```c
struct socket {
    socket_state        state;
    short               type;
    unsigned long       flags;
    struct file         *file;
    struct sock         *sk;
    const struct proto_ops *ops;        /* -> socket-layer interface */
};
```

#### struct net_device (`include/linux/netdevice.h:964`)

The network device entity:

```c
struct net_device {
    char name[IFNAMSIZ];
    unsigned int irq;
    unsigned long state;
    u32 features;
    int ifindex;
    const struct net_device_ops *netdev_ops;  /* -> device-layer interface */
    const struct ethtool_ops *ethtool_ops;
    unsigned int mtu;
    unsigned short hard_header_len;
};
```

### 4.3 Interface Adapters

The network stack has **three distinct abstraction layers**, each with its own
interface struct:

#### Layer 1: Socket Operations — struct proto_ops (`include/linux/net.h:161`)

Abstracts the address family (AF_INET, AF_INET6, AF_UNIX, ...):

```c
struct proto_ops {
    int family;
    int (*release)(struct socket *);
    int (*bind)(struct socket *, struct sockaddr *, int);
    int (*connect)(struct socket *, struct sockaddr *, int, int);
    int (*accept)(struct socket *, struct socket *, int);
    int (*listen)(struct socket *, int);
    int (*sendmsg)(struct kiocb *, struct socket *, struct msghdr *, size_t);
    int (*recvmsg)(struct kiocb *, struct socket *, struct msghdr *, size_t, int);
    int (*shutdown)(struct socket *, int);
    int (*setsockopt)(struct socket *, int, int, char __user *, unsigned int);
    int (*getsockopt)(struct socket *, int, int, char __user *, int __user *);
};
```

#### Layer 2: Transport Protocol — struct proto (`include/net/sock.h:739`)

Abstracts the transport protocol (TCP, UDP, RAW, ...):

```c
struct proto {
    void (*close)(struct sock *, long);
    int (*connect)(struct sock *, struct sockaddr *, int);
    int (*sendmsg)(struct kiocb *, struct sock *, struct msghdr *, size_t);
    int (*recvmsg)(struct kiocb *, struct sock *, struct msghdr *, size_t, int, int, int *);
    int (*sendpage)(struct sock *, struct page *, int, size_t, int);
    int (*bind)(struct sock *, struct sockaddr *, int);
    int (*backlog_rcv)(struct sock *, struct sk_buff *);
    void (*hash)(struct sock *);
    void (*unhash)(struct sock *);
    int (*get_port)(struct sock *, unsigned short);
};
```

#### Layer 3: Device Operations — struct net_device_ops (`include/linux/netdevice.h:859`)

Abstracts the NIC hardware:

```c
struct net_device_ops {
    int (*ndo_open)(struct net_device *dev);
    int (*ndo_stop)(struct net_device *dev);
    netdev_tx_t (*ndo_start_xmit)(struct sk_buff *skb, struct net_device *dev);
    void (*ndo_set_rx_mode)(struct net_device *dev);
    int (*ndo_set_mac_address)(struct net_device *dev, void *addr);
    int (*ndo_do_ioctl)(struct net_device *dev, struct ifreq *ifr, int cmd);
    int (*ndo_change_mtu)(struct net_device *dev, int new_mtu);
    void (*ndo_tx_timeout)(struct net_device *dev);
};
```

### 4.4 Outer Implementation Layer

**inet_dgram_ops** (IPv4 datagram sockets) — `net/ipv4/af_inet.c:936`:

```c
const struct proto_ops inet_dgram_ops = {
    .family     = PF_INET,
    .release    = inet_release,
    .bind       = inet_bind,
    .connect    = inet_dgram_connect,
    .sendmsg    = inet_sendmsg,
    .recvmsg    = inet_recvmsg,
    .poll       = udp_poll,
    .listen     = sock_no_listen,
};
```

**UDP transport** — `udp_prot` implements `struct proto`:

```c
/* net/ipv4/udp.c */
struct proto udp_prot = {
    .name       = "UDP",
    .close      = udp_lib_close,
    .connect    = ip4_datagram_connect,
    .sendmsg    = udp_sendmsg,
    .recvmsg    = udp_recvmsg,
    .hash       = udp_lib_hash,
    .unhash     = udp_lib_unhash,
    .get_port   = udp_v4_get_port,
};
```

### 4.5 Execution Flow — Sending a UDP Packet

```
  User: sendto(fd, buf, len, flags, addr, addrlen)
        |
        v
  SYSCALL_DEFINE6(sendto, ...)           [net/socket.c:1673]
        |--- sockfd_lookup_light(fd)
        |
        v
  sock_sendmsg(sock, &msg, len)          [net/socket.c:568]
        |
        v
  sock->ops->sendmsg(iocb, sock, msg, size)   [DISPATCH: proto_ops]
        |
        v
  inet_sendmsg()                         [net/ipv4/af_inet.c:732]
        |
        v
  sk->sk_prot->sendmsg(iocb, sk, msg, size)   [DISPATCH: proto]
        |
        v
  udp_sendmsg()                          [net/ipv4/udp.c]
        |--- build UDP header
        |--- ip_make_skb() / ip_append_data()
        |
        v
  ip_send_skb()
        |--- ip_local_out() -> NF hooks -> dst_output()
        |
        v
  dev_queue_xmit(skb)                    [net/core/dev.c]
        |
        v
  dev->netdev_ops->ndo_start_xmit(skb, dev)   [DISPATCH: net_device_ops]
        |
        v
  e1000_xmit_frame() (or any NIC driver)
        |
        v
  Hardware NIC
```

### 4.6 Execution Flow — Receiving a Packet

```
  NIC receives packet, raises IRQ
        |
        v
  Driver interrupt handler
        |--- allocate sk_buff, fill from DMA
        |--- napi_schedule() or netif_rx()
        |
        v
  NET_RX_SOFTIRQ: net_rx_action()       [net/core/dev.c:3894]
        |--- napi->poll()               [driver NAPI poll]
        |
        v
  netif_receive_skb(skb)                 [net/core/dev.c:3364]
        |--- __netif_receive_skb()
        |--- match skb->protocol in ptype_base
        |
        v
  deliver_skb() -> pt_prev->func()      [DISPATCH: protocol handler]
        |
        v
  ip_rcv()                              [net/ipv4/ip_input.c:375]
        |--- NF_HOOK -> ip_rcv_finish()
        |--- routing: ip_local_deliver()
        |
        v
  ip_local_deliver_finish()
        |--- lookup protocol handler via ipprot->handler
        |
        v
  udp_rcv()                             [net/ipv4/udp.c]
        |--- find socket, enqueue to sk->sk_receive_queue
        |--- wake up waiting process
```

### 4.7 Dependency Flow

```
  NIC driver (e1000)  ---->  include/linux/netdevice.h  ---->  include/linux/skbuff.h
  (outer impl)               (net_device_ops interface)        (sk_buff entity)

  net/ipv4/udp.c      ---->  include/net/sock.h         ---->  include/linux/skbuff.h
  (transport impl)           (proto interface)                 (sk_buff entity)

  net/socket.c         ---->  include/linux/net.h        ---->  include/net/sock.h
  (socket use case)          (proto_ops interface)             (sock entity)

  Direction:
  NIC driver -> netdevice.h -> sk_buff
  UDP        -> sock.h      -> sk_buff
  socket.c   -> net.h       -> sock.h -> sk_buff
```

### 4.8 Architecture Diagram

```
                  EXECUTION FLOW (Send)
                  =====================

        User: sendto(fd, buf, len, ...)
                      |
                      v
              +-----------------+
              |  sock_sendmsg() |       USE CASE (Socket policy)
              +-----------------+
                      |
          via sock->ops->sendmsg()
                      |
                      v
              +--------------------+
              | struct proto_ops   |    INTERFACE (Address family)
              | (inet_dgram_ops)   |
              +--------+-----------+
                       |
           via sk->sk_prot->sendmsg()
                       |
                       v
              +--------------------+
              | struct proto       |    INTERFACE (Transport)
              | (udp_prot)         |
              +--------+-----------+
                       |
                       v
              +--------------------+
              |  IP layer          |    USE CASE (Routing)
              |  ip_send_skb()     |
              +--------+-----------+
                       |
           via dev->netdev_ops->ndo_start_xmit()
                       |
                       v
              +--------------------+
              | struct net_device_ |    INTERFACE (Device)
              |   ops              |
              +--------+-----------+
                       |
                       v
              +--------------------+
              | NIC Driver         |    OUTER LAYER
              | (e1000, ixgbe)     |
              +--------+-----------+
                       |
                       v
                    Hardware


                  DEPENDENCY FLOW
                  ===============

   NIC driver -> netdevice.h -> sk_buff (entity)
   UDP impl   -> sock.h      -> sk_buff (entity)
   socket.c   -> net.h       -> sock / sk_buff (entities)

   All dependencies point inward toward stable entities.
```

### 4.9 Clean Architecture Insights

**Alignment:**

- The network stack has **three concentric interface layers** (proto_ops →
  proto → net_device_ops), each providing dependency inversion at a different
  abstraction level.
- New protocols can be added by implementing `struct proto` and registering.
  New device drivers implement `struct net_device_ops`. Neither requires changes
  to the core stack.
- `sk_buff` is the universal entity — every layer works with the same buffer
  structure, adding/stripping headers without copying data.

**Divergence:**

- The receive path uses a different dispatch mechanism (`ptype_base` hash table
  and `packet_type.func` callback) rather than a formal interface struct. This is
  a performance-driven design choice.
- Protocol handlers are registered dynamically via `dev_add_pack()` / `inet_add_protocol()`,
  which is more like an event-driven architecture than the strict layering of
  Clean Architecture.
- The dual `proto_ops` / `proto` split creates a two-stage dispatch that adds
  complexity: `inet_sendmsg()` essentially just calls `sk->sk_prot->sendmsg()`,
  acting as a thin delegation layer.

---

*Continued in [Part 2](clean_architecture_analysis_part2.md): Device Driver Model,
Security Framework, Interrupt Handling, Time Management.*
