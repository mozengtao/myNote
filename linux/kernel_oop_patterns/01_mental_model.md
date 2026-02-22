# Phase 1 — Mental Model: OOP Without Language Support

## The Linux Kernel as a Manually Constructed Object System

The Linux kernel is, by any honest assessment, one of the largest
object-oriented systems ever built — and it is written entirely in C.
There are no classes, no virtual methods, no constructors, no destructors,
no templates, no RTTI, no exceptions. Yet the kernel achieves
encapsulation, inheritance, polymorphism, and lifetime management with
a discipline that most C++ codebases envy.

This is not accidental. It is a deliberate architectural choice.

---

## The Mapping

| OOP Concept        | Kernel Mechanism                                   | Key File(s)                          |
|--------------------|----------------------------------------------------|--------------------------------------|
| Class              | `struct` definition                                | `include/linux/fs.h`                 |
| Instance           | `kmalloc`/`kmem_cache_alloc` + initializer         | `fs/ext2/super.c`                    |
| Methods            | Function pointers inside `struct`                  | `struct file_operations`             |
| vtable             | `const struct *_operations` pointer in object      | `file->f_op`, `inode->i_op`         |
| Inheritance        | Struct embedding (containment)                     | `struct ext2_inode_info`             |
| Polymorphism       | Indirect call through function pointer table       | `file->f_op->read(...)`             |
| RTTI / Downcast    | `container_of(ptr, type, member)`                  | `include/linux/kernel.h`            |
| Destructor         | `release()` callback via `kref` / `kobject`        | `lib/kobject.c`, `lib/kref.c`       |
| Reference counting | `kref`, `atomic_t`, `kobject`                      | `include/linux/kref.h`              |
| Abstract class     | `struct` with `NULL` function pointers             | `struct super_operations`            |
| Interface          | Operations struct with selective implementation    | `struct inode_operations`            |

---

## Why C, Not C++?

This question comes up constantly. The answer is not ideological — it is
engineering:

1. **ABI stability.** C has a stable, predictable ABI. C++ name mangling,
   vtable layout, exception handling, and RTTI are all compiler-dependent.
   The kernel must compile across dozens of architectures with multiple
   compiler versions. C gives total control over layout.

2. **No hidden control flow.** In C++, constructors, destructors, copy
   operators, implicit conversions, and exceptions create invisible
   code paths. In a kernel context — where you might be in interrupt
   context, holding a spinlock, or running with IRQs disabled — hidden
   allocations or exception unwinding are fatal.

3. **Explicit lifetime management.** The kernel cannot rely on RAII or
   garbage collection. Objects live across process boundaries, survive
   module unloads, and are accessed concurrently by multiple CPUs. The
   programmer must explicitly manage every reference. Making this explicit
   in the source is a feature, not a limitation.

4. **Transparent memory layout.** `container_of` only works because C
   guarantees struct member offsets are deterministic and accessible via
   `offsetof`. C++ virtual inheritance, for example, breaks this guarantee.

5. **Compile-time cost.** C++ templates and heavy header inclusion
   would significantly increase kernel build times — already measured in
   minutes for a full build.

---

## The Three Pillars in Practice

### Pillar 1: Encapsulation

State is bundled into a `struct`. Behavior is not free-floating — it is
attached to the object via a pointer to an operations table:

```c
struct file {
    const struct file_operations    *f_op;   /* vtable */
    loff_t                          f_pos;   /* state  */
    unsigned int                    f_flags;
    /* ... */
};
```

The `f_op` pointer is the object's vtable. Different file types (regular
files, pipes, sockets, device nodes) each provide a different
`file_operations` instance. The `struct file` itself does not know or
care which filesystem created it.

**Source:** `include/linux/fs.h` lines 964–1009

### Pillar 2: Inheritance via Embedding

A filesystem-specific inode "inherits from" the VFS inode by embedding
it as a member:

```c
struct ext2_inode_info {
    __le32  i_data[15];
    __u32   i_flags;
    /* ... ext2-specific fields ... */
    struct inode    vfs_inode;      /* <-- base class */
};
```

This is containment, not first-member embedding. The base (`struct inode`)
sits at an arbitrary offset. Recovery of the derived type requires
`container_of`.

**Source:** `fs/ext2/ext2.h` lines 16–61

### Pillar 3: Polymorphism via Dispatch

When the VFS layer calls `vfs_read()`, it does not check the filesystem
type. It dispatches through the function pointer:

```c
if (file->f_op->read)
    ret = file->f_op->read(file, buf, count, pos);
```

This is late binding. The actual function executed depends on which
`file_operations` table was installed when the file was opened. An ext2
file dispatches to `do_sync_read`. A proc file dispatches to its own
`read`. A socket dispatches to `sock_read`. There is zero branching on
type — the dispatch is O(1) via pointer indirection.

**Source:** `fs/read_write.c` lines 364–389

---

## Architectural Consequences

This model scales because:

- **Adding a new filesystem** requires zero changes to VFS core code.
  You define your operations structs, register them, and the VFS
  dispatches to you.

- **Subsystems are decoupled.** The VFS doesn't know about ext2. ext2
  doesn't know about NFS. They share abstract interfaces.

- **No type switches anywhere.** You never see
  `if (fs_type == EXT2) { ... } else if (fs_type == NFS) { ... }`.
  The kernel eliminated this anti-pattern through function-pointer
  dispatch decades ago.

- **The cost is discipline, not performance.** The runtime cost of
  indirect function calls is one pointer dereference. The engineering
  cost is that every developer must understand the pattern.

---

## What to Inspect

Before proceeding, open these files and orient yourself:

| File                         | What to look for                                    |
|------------------------------|-----------------------------------------------------|
| `include/linux/fs.h`        | `struct file`, `struct inode`, `struct file_operations`, `struct inode_operations`, `struct super_operations` |
| `include/linux/kernel.h`    | `container_of` macro (line 659)                     |
| `include/linux/kref.h`      | `struct kref` and its API                           |
| `include/linux/kobject.h`   | `struct kobject`, `struct kobj_type`, `struct kset`  |
| `include/linux/cdev.h`      | `struct cdev` — a kobject-based character device    |
| `include/linux/device.h`    | `struct device`, `struct bus_type`                  |

---

## Key Insight

> The Linux kernel is an object system where the programmer is the
> compiler. There is no language magic — every vtable, every reference
> count, every destructor dispatch, every downcast is written explicitly
> in C. This is both the source of its power and the source of its bugs.

In Phase 2, we will trace these patterns through the VFS in detail.
