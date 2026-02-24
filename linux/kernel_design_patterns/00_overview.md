# Design Patterns in the Linux Kernel (v3.2) — Pattern-Aware Reading Guide

> **Audience**: C systems programmer with ~5 years experience.
> You understand kernel APIs, data structures, and low-level concurrency.
> The goal is to internalize *how* and *why* design patterns appear in the
> kernel and to build a reusable mental model.
>
> **Kernel version**: 3.2 (January 2012)

---

## Learning Objectives

By the end of this study, you should be able to:

1. **Name** the design pattern (or closest analogue) when you see a kernel idiom.
2. **Explain** *why* that pattern is used there (problem it solves, constraints
   of C and kernel lifecycles).
3. **Predict** where the same idea appears elsewhere in the kernel.
4. **Reuse** the mental model: when you design a subsystem, you can consciously
   choose a pattern and implement it in C the way the kernel does.

---

## How to Use This Guide

- **Per pattern:** Read the "Mental model" first, then the "In the kernel"
  section. Each module includes a **"Real Code Path Walkthrough"** that
  traces a concrete scenario through actual v3.2 source lines — follow
  along by opening the cited files (or browse at
  [elixir.bootlin.com/linux/v3.2/source](https://elixir.bootlin.com/linux/v3.2/source)).
  Answer the "Check your understanding" questions in your own words.
- **Don't rush:** One pattern per session is enough. The goal is durable mental
  models, not coverage speed.
- **Compare with GoF:** If you know the GoF version (C++/Java), note what stays
  the same (roles, intent) and what changes (no classes → structs + function
  pointers; no garbage collection → explicit init/cleanup and ownership).

---

## Module Index

| # | File | Patterns | Core Kernel Examples |
|---|------|----------|---------------------|
| 0 | [This file](00_overview.md) | Overview, meta | — |
| 1 | [01_strategy_and_adapter.md](01_strategy_and_adapter.md) | Strategy, Adapter | `file_operations`, `proto_ops`, `sched_class` |
| 2 | [02_observer.md](02_observer.md) | Observer / Pub-Sub | `notifier_block`, `notifier_head` |
| 3 | [03_template_method.md](03_template_method.md) | Template Method | `vfs_read`, `fork.c`, initcalls |
| 4 | [04_iterator.md](04_iterator.md) | Iterator | `seq_file`, `list_for_each_entry` |
| 5 | [05_state.md](05_state.md) | State | TCP states, device PM |
| 6 | [06_factory_and_singleton.md](06_factory_and_singleton.md) | Factory, Singleton | `kmem_cache`, `alloc_netdev`, scheduler |
| 7 | [07_facade.md](07_facade.md) | Facade | `kmalloc`, `schedule()`, `dev_queue_xmit` |
| 8 | [08_composition_decorator_builder.md](08_composition_decorator_builder.md) | Composition, Decorator, Builder | `task_struct`, netfilter, net device init |

---

## The C-to-OOP Translation Table

Before diving in, anchor these equivalences:

```
GoF / OOP Term              Kernel C Idiom
─────────────────────────── ──────────────────────────────────────
Interface                   struct of function pointers (*_ops)
Concrete class              static const *_ops filled in per type
Virtual method dispatch     obj->ops->method(obj, ...)
Abstract base class         struct with ops pointer + embedded data
Observer registration       register_*_notifier / list_add
Factory method              *_alloc / *_create / *_register
Singleton                   global struct or per-CPU variable
Iterator                    start/next/stop or list_for_each_entry
State variable              enum + switch or table of transitions
Constructor                 *_alloc + *_init (often paired)
Destructor                  *_free / *_release / *_destroy
```

---

## Meta: Why Patterns in a C Kernel?

The kernel has no classes, no `virtual` keyword, no templates, no exceptions.
And yet the same design pressures exist:

- **Many implementations of one interface** (thousands of drivers, dozens
  of filesystems, multiple scheduling policies).
- **Decoupled subsystems** that must react to each other's events without
  hard-wiring dependencies.
- **Complex lifecycles** where objects are created, shared, referenced,
  and destroyed across contexts and CPUs.
- **Stable APIs** that must not break when a new driver or filesystem is added.

These pressures produce the same patterns that GoF catalogued. The *form*
changes — struct of function pointers instead of virtual methods, registration
functions instead of constructor injection — but the *intent* is identical.

### C Constraints That Shape the Form

| Constraint | Effect on Pattern Form |
|------------|----------------------|
| No classes/inheritance | Structs + `container_of` for "inheritance"; ops structs for vtables |
| No garbage collection | Explicit `kref`/`kobject` refcounting; init/cleanup pairs |
| No exceptions | Return codes; goto-based cleanup; no RAII |
| No templates/generics | `void *` + macros for generic containers (`list_head`, `hlist_head`) |
| Interrupt context | Some patterns (notifiers, iterators) need lockless or atomic variants |
| Module loading | Registration/unregistration must be symmetric and safe against races |

---

## Suggested Study Order

Follow this order to build each pattern on top of the previous:

```
Phase 1: Foundation (most visible, immediately useful)
  1. Strategy + Adapter ──── ops structs and VFS
                              ↓
Phase 2: Event-Driven
  2. Observer ─────────────── notifier chains
  3. Template Method ──────── initcalls, fork, VFS skeleton
                              ↓
Phase 3: Traversal and State
  4. Iterator ─────────────── seq_file, list macros
  5. State ────────────────── TCP, driver PM
                              ↓
Phase 4: Creation and Access
  6. Factory + Singleton ──── slab caches, global subsystems
  7. Facade ───────────────── kmalloc, schedule(), networking
                              ↓
Phase 5: Structure and Construction
  8. Composition + Decorator + Builder
```

---

## Mapping Back to GoF

For each pattern in this guide, write one paragraph in this form:

> "In the kernel, the **[GoF Role A]** is `[kernel struct/function]`;
> the **[GoF Role B]** is `[kernel struct/function]`;
> the **[GoF Role C]** is `[kernel struct/function]`."

Example for Strategy:

> "In the kernel, the **Strategy interface** is `struct sched_class`;
> the **ConcreteStrategy** is `fair_sched_class` / `rt_sched_class`;
> the **Context** is `struct task_struct` (which holds `sched_class *`)."

This exercise locks in the mental model.

---

## Key Source Files to Keep Open

| File | Why |
|------|-----|
| `include/linux/fs.h` | `file_operations`, `inode_operations`, `super_operations` |
| `include/linux/sched.h` | `task_struct`, `sched_class` |
| `include/linux/notifier.h` | Notifier chain infrastructure |
| `include/linux/list.h` | Intrusive linked list + iterator macros |
| `include/linux/seq_file.h` | Formal iterator protocol |
| `include/linux/netdevice.h` | `net_device`, `net_device_ops`, `alloc_netdev` |
| `include/linux/blkdev.h` | `block_device_operations`, request queue |
| `include/linux/net.h` | `struct proto_ops`, socket layer |
| `include/linux/netfilter.h` | `nf_hook_ops`, hook chains |
| `include/net/tcp_states.h` | TCP state enum |
| `include/net/tcp.h` | `tcp_congestion_ops` |
| `fs/read_write.c` | `vfs_read`, `vfs_write` — template method |
| `fs/ramfs/inode.c` | Minimal filesystem — adapter example |
| `kernel/fork.c` | Process creation — template method |
| `mm/slab.c` or `mm/slub.c` | Slab allocator internals |

---

Proceed to [Module 1: Strategy and Adapter](01_strategy_and_adapter.md).
