# Module 1: Encapsulation — Hiding What Doesn't Need to Be Seen

> **Core question**: In C++ you have `private:`. The kernel has no such keyword.
> How does `struct file` in `include/linux/fs.h` hide its internals from code
> that shouldn't touch them?

---

## 1.1 Opaque Pointers — `void *private_data`

The most pervasive encapsulation technique in the kernel is the **opaque
pointer**: a `void *` field that means "the owner knows what's really here."

### The Real Code

`include/linux/fs.h`, lines 969–1009 — `struct file`:

```c
struct file {
    union {
        struct list_head    fu_list;
        struct rcu_head     fu_rcuhead;
    } f_u;
    struct path         f_path;
    const struct file_operations    *f_op;
    spinlock_t          f_lock;
    atomic_long_t       f_count;
    unsigned int        f_flags;
    fmode_t             f_mode;
    loff_t              f_pos;
    struct fown_struct  f_owner;
    const struct cred   *f_cred;
    struct file_ra_state f_ra;
    u64                 f_version;
    void                *f_security;

    /* needed for tty driver, and maybe others */
    void                *private_data;

    struct list_head    f_ep_links;
    struct address_space *f_mapping;
};
```

Look at `private_data` on line 999. It is declared as `void *` — the VFS layer
never dereferences it. Only the **driver** that owns this file knows the actual
type.

### How a Driver Uses It

When a driver's `open` method is called, it typically allocates its own state
and stashes it in `private_data`:

```c
static int
mydriver_open(struct inode *inode, struct file *filp)
{
    struct mydriver_state *state;

    state = kzalloc(sizeof(*state), GFP_KERNEL);
    if (!state)
        return -ENOMEM;

    /* ... initialize state ... */
    filp->private_data = state;
    return 0;
}
```

Later, in `read` or `write`, the driver recovers its state:

```c
static ssize_t
mydriver_read(struct file *filp, char __user *buf,
              size_t count, loff_t *ppos)
{
    struct mydriver_state *state = filp->private_data;
    /* state is now fully typed — only this driver knows the layout */
    ...
}
```

### The C++ Equivalent: Pimpl (Pointer to Implementation)

```cpp
// public header — client sees only this
class File {
public:
    void read(char *buf, size_t count);
private:
    class Impl;          // forward declaration only
    Impl *pimpl;         // opaque pointer — client can't see inside
};

// private implementation file — only the "driver" knows
class File::Impl {
    int internal_state;
    char buffer[4096];
};
```

The kernel's `void *private_data` serves the same purpose: it hides
implementation details behind a type-erased pointer that only the owner
can decode.

### Why Not a Typed Pointer?

You might ask: wouldn't `struct mydriver_state *private_data` be safer?

Yes — but the VFS is **generic infrastructure**. It cannot know every
possible driver type. The `void *` is the price of genericity in C.
The alternative (a union of every possible driver state) would create
massive coupling between the VFS and every driver in the tree.

This is the fundamental tension: **type safety vs. extensibility**. The
kernel chooses extensibility and relies on programmer discipline instead
of compiler enforcement.

---

## 1.2 Static Functions — `static` as Access Control

C has no `private:` keyword, but it has something close: the `static` keyword
on functions at file scope. A `static` function is invisible outside its
translation unit — it has **internal linkage**.

### The Real Code

`drivers/char/mem.c`, lines 616–695 — the `/dev/null` implementation:

```c
/* drivers/char/mem.c, line 616 */
static ssize_t
read_null(struct file *file, char __user *buf,
          size_t count, loff_t *ppos)
{
    return 0;
}

/* line 622 */
static ssize_t
write_null(struct file *file, const char __user *buf,
           size_t count, loff_t *ppos)
{
    return count;
}

/* line 692 */
static loff_t
null_lseek(struct file *file, loff_t offset, int orig)
{
    return file->f_pos = 0;
}
```

Every one of these functions is `static`. No other file in the kernel can
call `read_null()` directly. They are **private methods** of the `/dev/null`
"class."

The only way the rest of the kernel can reach them is through the vtable:

```c
/* drivers/char/mem.c, line 763 */
static const struct file_operations null_fops = {
    .llseek      = null_lseek,
    .read        = read_null,
    .write       = write_null,
    .splice_write = splice_write_null,
};
```

Even the vtable itself (`null_fops`) is `static` — it is only referenced
within `mem.c` and registered with the VFS through an explicit registration
call.

### The C++ Equivalent

```cpp
class NullDevice : public FileOperations {
private:                               // ← static in C achieves this
    ssize_t read_null(...);
    ssize_t write_null(...);
    loff_t  null_lseek(...);
public:
    // only the vtable is "public" — exposed via virtual dispatch
};
```

### The Access Control Spectrum

```
Most restrictive                              Least restrictive
     |                                                |
     v                                                v
  static       non-exported       EXPORT_SYMBOL     EXPORT_SYMBOL_GPL
  (file-      symbol (module-     (any module       (GPL modules
  private)    private, link-      can use)          only)
              time only)
```

The kernel provides **four levels of visibility** — finer-grained than most
OOP languages which typically offer only three (`private`, `protected`,
`public`).

---

## 1.3 Header vs. Implementation Split

The kernel separates **public interface** from **private implementation**
using the same technique as large C++ projects: headers expose the API,
`.c` files hide the details.

### The Layered Header Pattern

```
include/linux/fs.h          ← Public API: struct file, struct inode,
                               struct file_operations (used by ALL drivers)

include/linux/fs_struct.h   ← Semi-private: struct fs_struct
                               (used by a smaller subset of the kernel)

fs/internal.h               ← Private: internal helpers for the VFS
                               implementation only

fs/read_write.c             ← Implementation: the actual code
```

Code in `fs/internal.h` is invisible to drivers — they simply cannot
`#include` it (build system enforces this). This is **physical encapsulation**:
the compiler literally cannot see the private declarations.

### struct inode — Public Interface vs. Private Storage

`include/linux/fs.h`, lines 749–838 — `struct inode`:

```c
struct inode {
    /* === PUBLIC INTERFACE (used by VFS layer) === */
    umode_t                 i_mode;
    uid_t                   i_uid;
    gid_t                   i_gid;
    unsigned int            i_flags;
    const struct inode_operations   *i_op;
    struct super_block      *i_sb;
    struct address_space    *i_mapping;
    unsigned long           i_ino;
    dev_t                   i_rdev;
    struct timespec         i_atime;
    struct timespec         i_mtime;
    struct timespec         i_ctime;
    loff_t                  i_size;
    const struct file_operations    *i_fop;

    /* === PRIVATE STORAGE (filesystem-specific) === */
    union {
        struct pipe_inode_info  *i_pipe;
        struct block_device     *i_bdev;
        struct cdev             *i_cdev;
    };

    void                    *i_private;  /* fs or device private pointer */
};
```

The `i_private` field (line 837) is the filesystem's escape hatch. The VFS
never touches it. Only the filesystem that created this inode knows what
`i_private` points to.

But `i_private` is actually the **older, simpler** mechanism. The modern
approach is struct embedding, which we cover in Module 2.

---

## 1.4 The Three Layers of Encapsulation

Putting it all together, the kernel uses three complementary mechanisms:

```
┌──────────────────────────────────────────────────────┐
│                  ENCAPSULATION IN C                  │
├──────────────────────────────────────────────────────┤
│                                                      │
│  1. TYPE ERASURE (void * pointers)                   │
│     ┌─────────────────────────────┐                  │
│     │ struct file {               │                  │
│     │     ...                     │                  │
│     │     void *private_data; ◄───┼── only the       │
│     │ };                          │   driver knows   │
│     └─────────────────────────────┘   the real type  │
│                                                      │
│  2. LINKAGE CONTROL (static keyword)                 │
│     ┌─────────────────────────────┐                  │
│     │ static ssize_t              │                  │
│     │ read_null(...)  ◄───────────┼── invisible      │
│     │ { return 0; }               │   outside this   │
│     │                             │   .c file        │
│     └─────────────────────────────┘                  │
│                                                      │
│  3. PHYSICAL SEPARATION (header hierarchy)           │
│     include/linux/fs.h  ← everyone can see           │
│     fs/internal.h       ← only VFS impl can see      │
│     fs/*.c              ← implementation details     │
│                                                      │
└──────────────────────────────────────────────────────┘
```

---

## 1.5 Why the Kernel Chose This Approach

**Why not just use `void *` for everything?**

Because `void *` sacrifices all type safety. The kernel uses it
**sparingly** — only at boundaries where true genericity is needed.
Within a subsystem, fully typed structs are preferred.

**Why not expose all struct fields and trust programmers?**

The kernel *does* expose struct definitions in headers (unlike the
opaque-struct pattern used in some C libraries). This is a pragmatic
trade-off: hiding the struct definition behind `struct foo;` would
prevent stack allocation and inline access, both of which matter for
performance in a kernel.

Instead, the kernel uses **social encapsulation** (documentation and
naming conventions like `__` prefixes) combined with **mechanical
encapsulation** (`static`, header hierarchy) to guide correct usage.

---

## Exercise

Examine `struct inode` in `include/linux/fs.h` (lines 749–838).

1. **Identify the "public interface" fields** — which fields does the
   VFS layer use directly? (Hint: look for fields read by `stat()`,
   used in path lookup, or referenced by VFS functions in `fs/*.c`.)

2. **Identify the "private implementation" fields** — which fields are
   only meaningful to a specific filesystem? (Hint: look for `void *`,
   unions, and the word "private" in comments.)

3. **What mechanism does the kernel use to let ext4 store its own data
   inside a generic inode?** (Hint: look at `fs/ext4/ext4.h` for
   `struct ext4_inode_info`. The answer leads directly to Module 2.)

---

## Socratic Check

Before moving to Module 2, answer this question:

> If a new driver wants to store 200 bytes of private state per open
> file descriptor, it cannot modify `struct file` (that would affect
> every driver). It cannot add fields to a shared header. What are
> its two options?
>
> **Option A**: Allocate a struct, store a pointer in `private_data`
>
> **Option B**: ??? (This is what Module 2 teaches)

If you can answer Option A confidently, you've mastered this module.
Option B will become clear in [Module 2: Inheritance](02_inheritance.md).
