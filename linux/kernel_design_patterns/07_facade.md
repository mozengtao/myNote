# Module 7: Facade

> **Pattern**: Facade (GoF)
> **Kernel idioms**: Simple public API over complex internals, subsystem entry points

---

## Mental Model

A complex subsystem exposes a small, simple front — a few functions or
one logical API — so callers don't deal with internal modules, ordering,
or implementation details.

In C: a handful of public functions declared in a widely-included header
that orchestrate internal calls to multiple subsystem components.

```
  CALLERS (most of the kernel)
  ┌────────────────────────────────┐
  │  kmalloc(size, GFP_KERNEL)     │  ← simple, stable API
  └─────────────┬──────────────────┘
                │
                ▼
  ┌──────────────────────────────────────────────────┐
  │              FACADE LAYER                        │
  │  (translates one call into internal subsystem    │
  │   operations — size class selection, slab vs.    │
  │   page allocator, per-CPU caches, NUMA, etc.)    │
  └──────────────────────────────────────────────────┘
                │         │         │
                ▼         ▼         ▼
           ┌────────┐ ┌────────┐ ┌────────┐
           │ slab   │ │ buddy  │ │per-CPU │
           │alloctor│ │alloctor│ │ cache  │
           └────────┘ └────────┘ └────────┘
```

**GoF mapping:**
- **Facade** → the public API functions (`kmalloc`, `schedule`, `dev_queue_xmit`)
- **Subsystem classes** → the internal components the facade orchestrates
- **Client** → all code that calls the facade

---

## In the Kernel (v3.2)

### Example 1: Memory Allocation — `kmalloc` / `kfree`

The memory subsystem is enormously complex internally:

- **Buddy allocator** (`mm/page_alloc.c`): manages physical pages in
  power-of-two order groups.
- **Slab/SLUB/SLOB** (`mm/slab.c`, `mm/slub.c`, `mm/slob.c`): carves
  pages into object-sized chunks with caching.
- **Per-CPU caches**: hot objects kept per-CPU to avoid lock contention.
- **NUMA awareness**: allocate from the node closest to the requesting CPU.
- **GFP flags**: control where and how memory is obtained (sleep? DMA?).

The facade:

```c
/* include/linux/slab.h — what callers see */
void *kmalloc(size_t size, gfp_t flags);
void kfree(const void *);
void *kzalloc(size_t size, gfp_t flags);  /* zero-filled variant */
```

**What `kmalloc` hides from the caller:**

```
  kmalloc(512, GFP_KERNEL)
       │
       ├── determine size class (which slab cache fits 512 bytes?)
       │
       ├── check per-CPU cache for a free object
       │     └── if hit: return immediately (fast path)
       │
       ├── check slab partial list
       │     └── if available: allocate from partial slab
       │
       ├── no free slab? ask buddy allocator for new pages
       │     └── buddy may need to reclaim memory, compact, or OOM
       │
       ├── carve a new slab from the pages
       │
       ├── NUMA: prefer the local node
       │
       └── return pointer to the object
```

The caller just says `kmalloc(512, GFP_KERNEL)` and gets a pointer.
They never see slab caches, buddy allocator internals, per-CPU lists,
or NUMA node selection.

### Example 2: Scheduling — `schedule()`

The scheduler facade:

```c
/* kernel/sched/core.c — what callers see */
void schedule(void);
void yield(void);
void wake_up_process(struct task_struct *p);
```

Behind these calls:

```
  schedule()
       │
       ├── disable preemption
       ├── choose the highest-priority sched_class with a runnable task
       │     ├── stop_sched_class  → any stop-machine tasks?
       │     ├── rt_sched_class    → any real-time tasks?
       │     ├── fair_sched_class  → CFS: walk the red-black tree
       │     └── idle_sched_class  → nothing to do, go idle
       │
       ├── context switch: save current registers, load next task's
       │
       ├── update accounting (vruntime, CPU time, load balancing)
       │
       └── re-enable preemption
```

Most code never touches run queues, CFS trees, or sched_class pointers.
They call `schedule()` or `wake_up_process()` and the facade handles
everything.

### Example 3: Networking — Transmit Path

```c
/* include/linux/netdevice.h — what callers see */
int dev_queue_xmit(struct sk_buff *skb);
int netif_rx(struct sk_buff *skb);
```

**What `dev_queue_xmit` hides:**

```
  dev_queue_xmit(skb)
       │
       ├── select TX queue (multi-queue devices)
       ├── traffic control / queueing discipline (tc/qdisc)
       │     └── classify, enqueue, possibly delay or drop
       ├── if queue not stopped:
       │     └── dev->netdev_ops->ndo_start_xmit(skb, dev)
       │           └── hardware-specific transmit
       ├── handle TX completion / error
       └── update statistics
```

The caller (protocol stack, socket layer) just hands off an `sk_buff`
and says "transmit this." The facade handles queue selection, traffic
shaping, driver dispatch, and error handling.

### Example 4: VFS — `sys_read()` / `sys_write()`

The system call layer is a facade over the entire VFS:

```
  sys_read(fd, buf, count)
       │
       ├── look up struct file from fd table
       ├── vfs_read() [template method — see Module 3]
       │     ├── permission checks
       │     ├── security hooks
       │     ├── dispatch to f_op->read()
       │     └── accounting
       └── return bytes read
```

The userspace caller knows nothing about inodes, dentries, page cache,
block I/O, or filesystem-specific logic. `read(fd, buf, count)` is the
facade.

### Real Code Path Walkthrough: `schedule()` — Facade Over Scheduler Internals

Trace what happens when kernel code calls `schedule()` — a one-function
facade that hides run queues, scheduling classes, priority chains, load
balancing, and context switching.

```
  CALLER (e.g. mutex_lock blocked, or voluntary yield)
  ─────────
  schedule()
       │
       ▼
  kernel/sched.c:4486 — schedule()
  ┌──────────────────────────────────────────────────────────────────┐
  │  tsk = current;                                                  │
  │  sched_submit_work(tsk);   /* flush blk plug if worker */        │
  │  __schedule();             ◄── the real work                     │
  └──────────────────────────────┬───────────────────────────────────┘
                                 │
                                 ▼
  kernel/sched.c:4395 — __schedule()
  ┌──────────────────────────────────────────────────────────────────┐
  │  ── INTERNAL 1: identify current CPU and run queue ──            │
  │  preempt_disable();                                              │
  │  cpu = smp_processor_id();                                       │
  │  rq = cpu_rq(cpu);         /* per-CPU singleton run queue */     │
  │  prev = rq->curr;          /* the task being preempted */        │
  │                                                                  │
  │  raw_spin_lock_irq(&rq->lock);                                   │
  │                                                                  │
  │  ── INTERNAL 2: deactivate sleeping task ──                      │
  │  if (prev->state && !(preempt_count() & PREEMPT_ACTIVE)) {       │
  │      deactivate_task(rq, prev, DEQUEUE_SLEEP);                   │
  │      prev->on_rq = 0;                                            │
  │  }                                                               │
  │                                                                  │
  │  ── INTERNAL 3: load balancing ──                                │
  │  if (unlikely(!rq->nr_running))                                  │
  │      idle_balance(cpu, rq);    /* pull tasks from other CPUs */  │
  │                                                                  │
  │  ── INTERNAL 4: pick next task via sched_class chain ──          │
  │  put_prev_task(rq, prev);                                        │
  │  next = pick_next_task(rq);    ◄── Strategy pattern inside       │
  │       │                                                          │
  │       │  kernel/sched.c:4368 — pick_next_task(rq):               │
  │       │    /* fast path: if only CFS tasks, ask CFS directly */  │
  │       │    if (rq->nr_running == rq->cfs.h_nr_running)           │
  │       │        p = fair_sched_class.pick_next_task(rq);          │
  │       │    else                                                  │
  │       │        /* walk priority chain: */                        │
  │       │        for_each_class(class) {                           │
  │       │            p = class->pick_next_task(rq);                │
  │       │            if (p) return p;                              │
  │       │        }                                                 │
  │       │    /* stop → rt → fair → idle (always returns) */        │
  │       ▼                                                          │
  │                                                                  │
  │  ── INTERNAL 5: context switch ──                                │
  │  if (likely(prev != next)) {                                     │
  │      rq->nr_switches++;                                          │
  │      rq->curr = next;                                            │
  │      context_switch(rq, prev, next);                             │
  │      /* saves prev's registers, loads next's registers,          │
  │         switches address spaces (mm), switches kernel stack */   │
  │  }                                                               │
  │                                                                  │
  │  ── INTERNAL 6: post-schedule cleanup ──                         │
  │  raw_spin_unlock_irq(&rq->lock);                                 │
  │  preempt_enable_no_resched();                                    │
  │  if (need_resched()) goto need_resched;   /* loop if needed */   │
  │                                                                  │
  └──────────────────────────────────────────────────────────────────┘
```

**What the `schedule()` facade hides from every caller:**

| Hidden Concept | Internal Location |
|---------------|------------------|
| Per-CPU run queue (`struct rq`) | `cpu_rq()` macro, `DEFINE_PER_CPU` |
| Scheduling class priority chain | `for_each_class` in `pick_next_task` |
| CFS red-black tree walk | `fair_sched_class.pick_next_task` |
| Real-time priority bitmap | `rt_sched_class.pick_next_task` |
| Cross-CPU load balancing | `idle_balance()` |
| Register save/restore | `context_switch()` → `switch_to()` |
| Address space switching | `switch_mm()` inside context_switch |

The caller just says `schedule()`. One word. The facade handles everything.

---

## Facade vs. Other Patterns

| Pattern | Intent | Kernel Example |
|---------|--------|---------------|
| **Facade** | Hide subsystem complexity behind a simple API | `kmalloc`, `schedule()`, `dev_queue_xmit` |
| **Adapter** | Make one interface look like another | `file_operations` per filesystem |
| **Template Method** | Fixed skeleton with pluggable hooks | `vfs_read()` |

A facade often *contains* template methods and adapters internally. The
facade is about what the caller sees; the other patterns are about how
the internals are organized.

---

## The Facade Principle in Kernel Headers

The kernel enforces facades through header discipline:

```
  include/linux/slab.h       ← PUBLIC facade (callers include this)
       │
       │  internally uses:
       ├── mm/slab.h          ← INTERNAL (not for general use)
       ├── mm/slab.c          ← IMPLEMENTATION
       ├── mm/slub.c          ← IMPLEMENTATION (alternative)
       └── mm/page_alloc.c    ← IMPLEMENTATION (underlying)
```

Callers include `<linux/slab.h>` and get `kmalloc`/`kfree`. They never
include internal headers. This header split is the C equivalent of
public/private access control — it enforces the facade boundary.

---

## Why Facade Here

Most code should not care about slab internals or scheduler classes.
A facade reduces coupling and keeps the "contract" of the subsystem
stable even when internals change.

**What would break without it:**

- If every caller used slab internals directly, changing from SLAB to
  SLUB (which happened in the real kernel) would require rewriting
  thousands of call sites.
- If callers manipulated run queues directly, the scheduler could not
  be refactored without breaking drivers and filesystems.
- If protocol code called hardware transmit functions directly, traffic
  control and multi-queue support would be impossible to add.

**The benefit:** internals can change freely (slab → slub, single queue →
multi-queue, O(1) scheduler → CFS) without any caller code changing.
The facade absorbs the impact.

---

## Check Your Understanding

1. Trace `kmalloc()` (or one variant) through to one internal allocator.
   List two or three internal concepts that the facade hides from the
   caller.

2. Name one benefit of having a facade instead of letting every caller
   use internal APIs directly.

3. `schedule()` is called from hundreds of places in the kernel. How
   many of those callers need to know about CFS trees or real-time
   priority queues? What does this tell you about the value of the
   facade?

4. Find one place where the kernel changed its internal implementation
   (e.g. SLAB → SLUB) but the facade API (`kmalloc`/`kfree`) stayed
   the same.

---

Proceed to [Module 8: Composition, Decorator, and Builder](08_composition_decorator_builder.md).
