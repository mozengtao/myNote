# Module 5: Classic Design Patterns in Kernel C

> **Core question**: Gang of Four design patterns were written for Smalltalk
> and C++. Which of those patterns appear in the Linux kernel, and how does
> C change the implementation?

---

## 5.1 Observer / Publish-Subscribe — Notifier Chains

The kernel needs a way to broadcast events ("network interface went down",
"system is rebooting") to an unknown number of interested subsystems. This
is the classic **Observer** pattern.

### The Real Code

`include/linux/notifier.h`, lines 50–54:

```c
struct notifier_block {
    int (*notifier_call)(struct notifier_block *,
                         unsigned long, void *);
    struct notifier_block __rcu *next;
    int priority;
};
```

Each `notifier_block` is a **subscriber** — it contains a callback function
and a link to the next subscriber in the chain. The chain itself is a
singly-linked list, ordered by priority.

### How It Works

```
  Publisher:  call_netdevice_notifiers(NETDEV_UP, dev)
                │
                ▼
  ┌──────────────────┐    ┌──────────────────┐    ┌──────────────────┐
  │ notifier_block A │──→ │ notifier_block B │──→ │ notifier_block C │
  │ .notifier_call() │    │ .notifier_call() │    │ .notifier_call() │
  │ .priority = 10   │    │ .priority = 5    │    │ .priority = 0    │
  └──────────────────┘    └──────────────────┘    └──────────────────┘
          │                       │                       │
          ▼                       ▼                       ▼
      bonding_event()        bridge_event()           my_event()
```

### Subscribe / Unsubscribe

`net/core/dev.c`, line 1363:

```c
int register_netdevice_notifier(struct notifier_block *nb)
```

A subsystem that cares about network device events calls
`register_netdevice_notifier()` to subscribe. The notifier block is
inserted into the chain. When any network event occurs, the chain is
walked and each callback is invoked.

### The Four Notifier Chain Types

The kernel provides four chain variants for different concurrency contexts:

| Type                        | Locking               | Use case                        |
|-----------------------------|-----------------------|---------------------------------|
| `atomic_notifier_head`      | `spinlock_t`          | Callable from interrupt context |
| `blocking_notifier_head`    | `rw_semaphore`        | May sleep in callbacks          |
| `raw_notifier_head`         | None (caller manages) | Maximum flexibility             |
| `srcu_notifier_head`        | SRCU                  | High-frequency read paths       |

`include/linux/notifier.h`, lines 56–68:

```c
struct atomic_notifier_head {
    spinlock_t lock;
    struct notifier_block __rcu *head;
};

struct blocking_notifier_head {
    struct rw_semaphore rwsem;
    struct notifier_block __rcu *head;
};

struct raw_notifier_head {
    struct notifier_block __rcu *head;
};
```

### The C++ / Java Equivalent

```java
// Java Observer pattern
interface NetDeviceListener {
    void onEvent(long eventType, Object data);
}

class NetDeviceNotifier {
    List<NetDeviceListener> listeners = new ArrayList<>();

    void register(NetDeviceListener l) { listeners.add(l); }
    void unregister(NetDeviceListener l) { listeners.remove(l); }

    void notifyAll(long event, Object data) {
        for (NetDeviceListener l : listeners)
            l.onEvent(event, data);
    }
}
```

The kernel version is more efficient (singly-linked list, no heap
allocation for the list itself) and more nuanced (priority ordering,
four concurrency variants).

---

## 5.2 Iterator — `seq_file` and `list_for_each_entry`

The kernel has two complementary iterator patterns: macro-based iteration
over intrusive linked lists, and a formal iterator protocol for `/proc`
files.

### Pattern A: `list_for_each_entry`

`include/linux/list.h`, lines 418–421:

```c
#define list_for_each_entry(pos, head, member)              \
    for (pos = list_entry((head)->next, typeof(*pos), member);  \
         &pos->member != (head);                            \
         pos = list_entry(pos->member.next, typeof(*pos), member))
```

This macro iterates over a `struct list_head`-based linked list,
automatically using `container_of` (via `list_entry`) to yield typed
pointers to the enclosing structs.

Usage:

```c
struct my_device {
    struct list_head list;
    int id;
    char name[32];
};

struct list_head device_list;  /* the list head */

struct my_device *dev;
list_for_each_entry(dev, &device_list, list) {
    printk("device: %s (id=%d)\n", dev->name, dev->id);
}
```

### The C++ Equivalent

```cpp
// list_for_each_entry ≈ range-based for
for (auto &dev : device_list) {
    std::cout << dev.name << " (id=" << dev.id << ")\n";
}
```

The kernel version is more explicit but achieves the same decoupling:
iteration logic is separated from the data structure internals.

### Pattern B: `seq_file` — Iterator Protocol for `/proc`

`include/linux/seq_file.h`, lines 30–35:

```c
struct seq_operations {
    void * (*start) (struct seq_file *m, loff_t *pos);
    void   (*stop)  (struct seq_file *m, void *v);
    void * (*next)  (struct seq_file *m, void *v, loff_t *pos);
    int    (*show)  (struct seq_file *m, void *v);
};
```

This is a formal **iterator interface**:

| Method  | Role                                          | C++ Equivalent       |
|---------|-----------------------------------------------|----------------------|
| `start` | Position the iterator at the beginning        | `begin()`            |
| `next`  | Advance to the next element                   | `++it`               |
| `stop`  | Release resources (end of iteration)          | (destructor)         |
| `show`  | Render the current element to the output      | `operator<<`         |

The `seq_file` framework calls these methods in sequence to produce
the content of a `/proc` file. The implementation provides the
domain-specific iteration; the framework handles buffering, userspace
copying, and partial reads.

---

## 5.3 Strategy — Scheduler Classes

The **Strategy** pattern encapsulates a family of algorithms and makes
them interchangeable. The scheduler classes are a textbook example.

### The Real Code

`include/linux/sched.h`, lines 1084–1119:

```c
struct sched_class {
    const struct sched_class *next;

    void (*enqueue_task) (struct rq *rq, struct task_struct *p,
                          int flags);
    void (*dequeue_task) (struct rq *rq, struct task_struct *p,
                          int flags);
    void (*yield_task)   (struct rq *rq);
    bool (*yield_to_task)(struct rq *rq, struct task_struct *p,
                          bool preempt);

    void (*check_preempt_curr)(struct rq *rq,
                               struct task_struct *p, int flags);

    struct task_struct *(*pick_next_task)(struct rq *rq);
    void (*put_prev_task)(struct rq *rq, struct task_struct *p);

    void (*set_curr_task)(struct rq *rq);
    void (*task_tick)(struct rq *rq, struct task_struct *p,
                      int queued);
    void (*task_fork)(struct task_struct *p);
    /* ... more ... */
};
```

### The Concrete Strategies

The kernel defines several scheduler classes, each a different scheduling
algorithm:

`kernel/sched_fair.c`, lines 5044–5054:

```c
static const struct sched_class fair_sched_class = {
    .next               = &idle_sched_class,
    .enqueue_task       = enqueue_task_fair,
    .dequeue_task       = dequeue_task_fair,
    .yield_task         = yield_task_fair,
    .yield_to_task      = yield_to_task_fair,
    .check_preempt_curr = check_preempt_wakeup,
    .pick_next_task     = pick_next_task_fair,
    .put_prev_task      = put_prev_task_fair,
    /* ... */
};
```

The scheduler classes are chained via the `next` pointer in priority order:

```
stop_sched_class → rt_sched_class → fair_sched_class → idle_sched_class
  (highest priority)                                      (lowest)
```

The core scheduler walks this chain to find the highest-priority class
that has a runnable task:

```c
/* Conceptual dispatch in kernel/sched.c */
for_each_class(class) {
    p = class->pick_next_task(rq);
    if (p)
        return p;
}
```

### Why This Is Strategy, Not Just Polymorphism

Strategy goes beyond basic polymorphism. The key characteristic: **the
algorithm can be selected and swapped at runtime**. Each `task_struct`
has a `sched_class` pointer that determines its scheduling behavior.
Changing a task's scheduling policy (e.g., `sched_setscheduler()`)
changes which strategy is used for that task.

### The C++ Equivalent

```cpp
class Scheduler {
public:
    virtual void enqueue(RunQueue &rq, Task &p) = 0;
    virtual Task *pick_next(RunQueue &rq) = 0;
    // ...
};

class CFSScheduler : public Scheduler { /* ... */ };
class RTScheduler  : public Scheduler { /* ... */ };

// Strategy selection:
task->scheduler = new CFSScheduler();  // normal tasks
task->scheduler = new RTScheduler();   // real-time tasks
```

---

## 5.4 Factory — Device Model & Slab Allocators

The **Factory** pattern provides a creation interface without exposing
the exact class being instantiated. The kernel uses this pattern
extensively through slab allocators.

### The Real Code

`fs/ext4/super.c`, lines 965–969:

```c
ext4_inode_cachep = kmem_cache_create("ext4_inode_cache",
                         sizeof(struct ext4_inode_info),
                         0,
                         (SLAB_RECLAIM_ACCOUNT|SLAB_MEM_SPREAD),
                         init_once);
```

This creates a **typed object pool** — a factory that produces
`ext4_inode_info` objects. The pool:

1. Pre-allocates memory in efficient slabs
2. Knows the exact size of each object
3. Calls `init_once` as a constructor for cache-cold objects
4. Reuses previously freed objects (avoiding repeated initialization)

### Using the Factory

```c
/* Allocate: like "new Ext4InodeInfo()" */
struct ext4_inode_info *ei =
    kmem_cache_alloc(ext4_inode_cachep, GFP_NOFS);

/* Free: like "delete ei" */
kmem_cache_free(ext4_inode_cachep, ei);
```

### The C++ Equivalent

```cpp
// kmem_cache ≈ a typed pool allocator with placement new
template<typename T>
class ObjectPool {
    // pre-allocate memory slabs
    // reuse freed objects
public:
    T *alloc() { /* ... */ }
    void free(T *obj) { /* ... */ }
};

ObjectPool<Ext4InodeInfo> inode_pool;
auto *ei = inode_pool.alloc();
inode_pool.free(ei);
```

### Why a Factory Instead of Bare `kmalloc`?

1. **Performance**: The slab allocator pre-slices memory into object-sized
   chunks, eliminating per-allocation overhead.
2. **Cache coloring**: Objects are offset within slabs to reduce cache
   line conflicts.
3. **Constructor caching**: The `init_once` callback runs only on truly
   new objects, not on recycled ones.
4. **Debugging**: SLAB/SLUB can detect use-after-free, buffer overflows,
   and memory leaks per object type.

---

## 5.5 Template Method — The VFS Layer

The **Template Method** pattern defines the skeleton of an algorithm in
a base class, deferring specific steps to subclasses.

### The Real Code

`fs/read_write.c`, lines 364–389 — `vfs_read()`:

```c
ssize_t
vfs_read(struct file *file, char __user *buf,
         size_t count, loff_t *pos)
{
    ssize_t ret;

    /* Step 1: Check permissions (framework) */
    if (!(file->f_mode & FMODE_READ))
        return -EBADF;

    /* Step 2: Validate the operation exists (framework) */
    if (!file->f_op || (!file->f_op->read && !file->f_op->aio_read))
        return -EINVAL;

    /* Step 3: Verify userspace buffer (framework) */
    if (unlikely(!access_ok(VERIFY_WRITE, buf, count)))
        return -EFAULT;

    /* Step 4: Security/LSM checks (framework) */
    ret = rw_verify_area(READ, file, pos, count);
    if (ret >= 0) {
        count = ret;

        /* Step 5: THE ACTUAL READ — delegated to the "subclass" */
        if (file->f_op->read)
            ret = file->f_op->read(file, buf, count, pos);
        else
            ret = do_sync_read(file, buf, count, pos);

        /* Step 6: Post-read accounting (framework) */
        if (ret > 0) {
            fsnotify_access(file);
            add_rchar(current, ret);
        }
        inc_syscr(current);
    }

    return ret;
}
```

The algorithm skeleton is fixed:
1. Check permissions
2. Validate operation exists
3. Verify userspace buffer
4. Security checks
5. **Delegate to concrete implementation** ← the customizable step
6. Post-read accounting

Steps 1–4 and 6 are the **invariant framework**. Step 5 is the
**variant hook** that each filesystem/driver implements differently.

### The C++ Equivalent

```cpp
class VFS {
public:
    ssize_t read(File *file, char *buf, size_t count) {
        if (!check_permissions(file))     // invariant
            return -EBADF;
        if (!validate_op(file))           // invariant
            return -EINVAL;
        if (!verify_buffer(buf, count))   // invariant
            return -EFAULT;

        ssize_t ret = do_read(file, buf, count);  // VIRTUAL

        if (ret > 0) {                    // invariant
            notify_access(file);
            account_read(ret);
        }
        return ret;
    }

protected:
    virtual ssize_t do_read(File *, char *, size_t) = 0;
};
```

---

## 5.6 Proxy / Decorator — `compat_ioctl`

The **Proxy** pattern provides a surrogate that controls access to
another object. The `compat_ioctl` layer is a clean example.

### The Problem

On a 64-bit kernel, 32-bit userspace programs issue `ioctl` calls with
32-bit struct layouts. The kernel needs to translate these into the 64-bit
native format before passing them to the real ioctl handler.

### The Solution

`struct file_operations` has both `unlocked_ioctl` and `compat_ioctl`:

```c
struct file_operations {
    /* ... */
    long (*unlocked_ioctl)(struct file *, unsigned int, unsigned long);
    long (*compat_ioctl)(struct file *, unsigned int, unsigned long);
    /* ... */
};
```

The `compat_ioctl` function acts as a **proxy**: it translates 32-bit
arguments into 64-bit format, then delegates to the real ioctl. The
caller doesn't know whether it's talking to the real implementation or
the compatibility wrapper.

```
32-bit userspace                     64-bit kernel
┌──────────────┐                    ┌──────────────────┐
│ ioctl(fd,    │ ──→  compat layer  │ compat_ioctl()   │
│   cmd, arg)  │      (proxy)       │   translate arg  │
└──────────────┘                    │   call real ioctl│
                                    └──────────────────┘
                                            │
                                            ▼
                                    ┌──────────────────┐
                                    │ unlocked_ioctl() │
                                    │   (real impl)    │
                                    └──────────────────┘
```

---

## 5.7 Pattern Summary

| GoF Pattern     | Kernel Implementation                    | Key Structs/Files                |
|-----------------|------------------------------------------|----------------------------------|
| Observer        | Notifier chains                          | `struct notifier_block`          |
| Iterator        | `list_for_each_entry`, `seq_file`        | `list.h`, `seq_file.h`           |
| Strategy        | Scheduler classes                        | `struct sched_class`             |
| Factory         | Slab allocators                          | `kmem_cache_create/alloc`        |
| Template Method | VFS algorithm skeletons                  | `vfs_read()`, `vfs_write()`      |
| Proxy/Decorator | `compat_ioctl`, compat syscall layer     | `file_operations.compat_ioctl`   |

---

## Exercise

Pick any one of these patterns. Find a **second instance** of it in the
kernel (not the one shown above). Describe:

1. Which structs are involved
2. Which file it lives in
3. How the pattern improves maintainability compared to a non-pattern
   approach (e.g., a giant switch statement)

**Suggestions for where to look:**

- **Observer**: `include/linux/reboot.h` — reboot notifier chains
- **Iterator**: `fs/proc/` — many `seq_file` implementations
- **Strategy**: `include/linux/tcp.h` — `struct tcp_congestion_ops`
- **Factory**: `net/core/sock.c` — socket slab caches
- **Template Method**: `fs/read_write.c` — `vfs_write()` mirrors `vfs_read()`

---

## Socratic Check

Before moving to Module 6, answer:

> The Observer and Strategy patterns both use function pointers. What is
> the fundamental difference between them?
>
> (Hint: How many callbacks does each pattern involve per "registration"?
> Who decides when the callback is invoked — the object itself, or an
> external event source?)

Proceed to [Module 6: Synthesis](06_synthesis.md).
