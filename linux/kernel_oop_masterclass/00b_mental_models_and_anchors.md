# Mental Models and v3.2 Anchors — Kernel Design Patterns

> **Purpose**: Canonical "mind map" for each pattern: what it is, what problem it
> solves, the C mechanism, when to use it, and pitfalls. Use this with the
> [Mastery Instruction Prompt](00_overview_and_reference.md#system-prompt-for-llm-assisted-study)
> and the [Eight Questions](06_synthesis.md#61-the-analysis-framework) when
> reading kernel code.
>
> **Audience**: C systems programmer with ~5 years experience  
> **Kernel version**: 3.2

---

## 2. Mental Models for Kernel Design Patterns

Use these as the canonical reference for each pattern. The goal is to
internalize: *what it is*, *what problem it solves*, *the mapping* (OOP term →
kernel C), *when to use it*, and *pitfalls*.

### 2.1 Encapsulation

| Aspect | Mental model |
|--------|----------------|
| **What it is** | Hiding representation and internal helpers so callers depend on a stable surface. |
| **Kernel C mechanism** | Opaque `void *private_data`; `static` functions (file scope); public headers vs. internal/implementation. |
| **Why the kernel uses it** | Many subsystems (VFS, net, block) need a single struct (e.g. `struct file`) while each driver/filesystem needs its own data. A generic slot + `static` keeps internals out of the ABI. |
| **When to use it** | You have a generic object (file, inode, device) and a family of implementations that each need private state. |
| **Pitfalls** | Casting `private_data` without a single known owner; exposing internal structs in public headers and locking in layout. |

### 2.2 Inheritance (struct embedding + container_of)

| Aspect | Mental model |
|--------|----------------|
| **What it is** | "IS-A" by embedding the base struct inside the derived struct; the base is the *first* or a *named* member so pointers align. |
| **Kernel C mechanism** | `struct derived { struct base b; ... };`; `container_of(ptr_to_base, struct derived, b)` to get `struct derived *`. |
| **Why the kernel uses it** | One API works on `struct base *` (e.g. `kobject`, `device`, `inode`); many concrete types (cdev, net_device, ext4_inode_info) add fields and behavior. Embedding avoids a separate heap allocation and keeps layout predictable. |
| **When to use it** | You have a generic layer (VFS, kobject, device model) and many concrete types that must be passed to that layer as the base type. |
| **Pitfalls** | Wrong member name or type in `container_of`; embedding base not at a consistent offset; assuming pointer identity between base and container (they differ). |

### 2.3 Polymorphism (ops / vtable)

| Aspect | Mental model |
|--------|----------------|
| **What it is** | Behavior varies by concrete type; the caller holds a pointer to a table of function pointers and calls through it—no big switch on type. |
| **Kernel C mechanism** | `struct foo_operations { int (*open)(...); ssize_t (*read)(...); ... };` stored in the object (e.g. `file->f_op`); call `file->f_op->read(...)`. |
| **Why the kernel uses it** | One code path (e.g. VFS read) must work for files, sockets, pipes, devices; adding a new type = new ops struct, no change to the central path. Enables modules and avoids branch explosion. |
| **When to use it** | Many implementations of the same interface (many drivers, many filesystems, many protocols); you want one algorithm that calls "hooks" defined per type. |
| **Pitfalls** | NULL function pointer (must check or document); mixing up which ops (e.g. `i_op` vs `i_fop`); ABI if you add or reorder pointers. |

### 2.4 Object lifecycle (kobject / kref / release)

| Aspect | Mental model |
|--------|----------------|
| **What it is** | Objects are created, referenced, and destroyed when refcount drops to zero; a "type" describes how to release and optionally how to expose attributes. |
| **Kernel C mechanism** | `struct kobject` (name, parent, kref, ktype); `kref_get`/`kref_put`; `ktype->release()` when refcount hits zero; kset for grouping. |
| **Why the kernel uses it** | Many entities (devices, drivers, classes) share the same lifecycle rules and need to appear in sysfs; one implementation (kobject) gives refcounting, hierarchy, and type-based destructor. |
| **When to use it** | You have kernel objects that are shared, referenced from multiple places, and must not be freed until all users are done; optional sysfs representation. |
| **Pitfalls** | Releasing while someone still holds a reference; forgetting to put after get; calling into freed object from a sysfs callback. |

### 2.5 Observer / publish–subscribe (notifier chains)

| Aspect | Mental model |
|--------|----------------|
| **What it is** | One producer of events, many independent listeners; producer doesn't know listeners; listeners register callbacks and are invoked when the event happens. |
| **Kernel C mechanism** | `struct notifier_block` (callback + priority + list link); `register_*_notifier()` / `unregister_*_notifier()`; `call_*_notifiers()` walks the list and calls each. |
| **Why the kernel uses it** | Net device up/down, reboot, PM events, etc. must be observed by many subsystems without coupling the producer to all of them; adding a listener is a single registration. |
| **When to use it** | You have a clear event source and multiple, independent reactors that must not depend on each other. |
| **Pitfalls** | Callback doing too much or blocking; order (priority) dependencies; unregistering from inside a callback. |

### 2.6 Strategy (pluggable algorithm)

| Aspect | Mental model |
|--------|----------------|
| **What it is** | One role (e.g. "scheduler") with one algorithm slot; the active algorithm is chosen at runtime (or boot) and invoked through a single interface. |
| **Kernel C mechanism** | e.g. `struct sched_class` with `enqueue_task`, `pick_next_task`, etc.; core scheduler holds a pointer to the active class and calls through it. |
| **Why the kernel uses it** | Different scheduling policies (CFS, RT, deadline) coexist; one dispatch path, many strategies, no giant switch. |
| **When to use it** | You have one conceptual task (scheduling, congestion control) with multiple replaceable algorithms. |
| **Pitfalls** | Assuming one strategy is active; changing strategy while in use; inconsistent state between strategy and core. |

### 2.7 Template method (skeleton algorithm + hooks)

| Aspect | Mental model |
|--------|----------------|
| **What it is** | A fixed algorithm skeleton in one place; key steps are delegated to hooks provided by the concrete type (via ops or callbacks). |
| **Kernel C mechanism** | e.g. VFS: `vfs_read()` does locking, limits, then `file->f_op->read()`; the "template" is in fs/read_write.c, the "steps" are in each filesystem/driver. |
| **Why the kernel uses it** | Common flow (lock, check, call implementation, account, unlock) is written once; each filesystem/driver fills in only the part that differs. |
| **When to use it** | Many implementations share the same high-level flow but differ in a few steps. |
| **Pitfalls** | Bypassing the template and calling ops directly without the common steps; inconsistent locking or accounting. |

### 2.8 Iterator (abstract traversal)

| Aspect | Mental model |
|--------|----------------|
| **What it is** | Traverse a collection without exposing its structure; start / next / stop and optionally "show one element." |
| **Kernel C mechanism** | `list_for_each_entry()` macros; `seq_file` with `.start`, `.next`, `.stop`, `.show` for /proc-style iteration. |
| **Why the kernel uses it** | Many data structures (lists, trees, hashes) and many consumers (seq_file, netlink, ioctl); one iteration protocol keeps consumers simple and structure hidden. |
| **When to use it** | You expose a collection (e.g. /proc, sysfs, netlink) and want to add new collections or formats without changing the iteration protocol. |
| **Pitfalls** | Modifying the collection during iteration; not handling start/stop for locking or resource setup/teardown. |

### 2.9 Factory (typed creation / pool)

| Aspect | Mental model |
|--------|----------------|
| **What it is** | Creation of objects of a given "type" is centralized; often combined with a pool (slab) so layout and lifecycle are consistent. |
| **Kernel C mechanism** | `kmem_cache_create()` + `kmem_cache_alloc()`; constructor/destructor callbacks; one cache per logical type (e.g. inode cache for a filesystem). |
| **Why the kernel uses it** | Same size and layout, better cache use and fragmentation; one place to run constructor/destructor and enforce invariants. |
| **When to use it** | Many instances of the same struct, created and destroyed often; need consistent init/teardown. |
| **Pitfalls** | Using a cache for the wrong type; mixing cache and non-cache allocations for the same logical type. |

---

## 3. Quick "How & Why" Checklist

When studying any kernel example, fill this in:

| Question | Short answer |
|----------|--------------|
| **What C mechanism implements it?** | (structs, macros, function pointers, callbacks, …) |
| **What would break or get ugly without it?** | (e.g. "VFS would need a switch on every filesystem type.") |
| **What is the one-sentence mental model?** | (e.g. "Ops struct = this object's vtable; caller always dispatches through it.") |
| **One pitfall in kernel context** | (lifetime, NULL, ABI, locking, …) |

---

## 4. Recommended v3.2 Anchors (for "how" and "why")

| Pattern | Primary file(s) | What to look at |
|---------|------------------|------------------|
| Encapsulation | `include/linux/fs.h` | `struct file`, `private_data`; `struct inode`, `i_private` |
| Inheritance | `include/linux/cdev.h`, `include/linux/kernel.h` | `struct cdev` embedding `kobject`; `container_of` macro |
| Polymorphism | `include/linux/fs.h`, `drivers/char/mem.c` | `file_operations`, `null_fops` / `zero_fops` |
| Lifecycle | `include/linux/kobject.h`, `lib/kobject.c` | `kobject`, `kobj_type`, `kref`, `release` |
| Observer | `include/linux/notifier.h` | `notifier_block`, netdevice notifier registration |
| Strategy | `include/linux/sched.h`, `kernel/sched/core.c` | `sched_class`, CFS/RT class usage |
| Template method | `fs/read_write.c` | `vfs_read()` calling `f_op->read()` |
| Iterator | `include/linux/list.h`, `fs/seq_file.c` | `list_for_each_entry`, `seq_operations` |
| Factory | `mm/slab.c` / `mm/slub.c` | `kmem_cache_create`, `kmem_cache_alloc` (e.g. in ext4 inode cache) |
