# Module 2: Inheritance — Struct Embedding and `container_of`

> **Core question**: C has no `class Derived : public Base`. Yet `struct cdev`
> somehow "inherits from" `struct kobject`. How?

---

## 2.1 Struct Embedding as Inheritance

In C++, you write:

```cpp
class CharDevice : public KernelObject {
    Module *owner;
    FileOperations *ops;
};
```

In the Linux kernel, you achieve the same relationship by **embedding** the
base struct as a member of the derived struct.

### The Real Code

`include/linux/cdev.h`, lines 12–19:

```c
struct cdev {
    struct kobject kobj;
    struct module *owner;
    const struct file_operations *ops;
    struct list_head list;
    dev_t dev;
    unsigned int count;
};
```

`struct cdev` contains a `struct kobject kobj` as its **first named member**.
This means:

- A `struct cdev *` can be treated as a `struct kobject *` (by taking `&cdev->kobj`)
- Any function that accepts `struct kobject *` can operate on a `cdev`
- This IS-A relationship is implemented via HAS-A composition

### Why Embedding Instead of a Pointer?

You might think: why not `struct kobject *kobj` (a pointer)? Two reasons:

1. **Locality**: The base object is allocated inline — no extra `malloc`,
   no pointer dereference, no cache miss. The `kobject` fields are physically
   adjacent to the `cdev` fields in memory.

2. **Lifetime coupling**: The base object lives and dies with the derived
   object. There's no risk of a dangling pointer to a separately-allocated
   base.

This is the kernel's version of **non-virtual inheritance** — the base class
is physically part of the derived class.

---

## 2.2 `container_of` — The Downcast

The key problem with embedding: given a `struct kobject *` pointer (which the
generic kobject API gives you), how do you recover the enclosing `struct cdev *`?

In C++, this is `static_cast<Derived*>(base_ptr)`. In the kernel, this is
`container_of`.

### The Real Code

`include/linux/kernel.h`, lines 659–661:

```c
#define container_of(ptr, type, member) ({                      \
    const typeof( ((type *)0)->member ) *__mptr = (ptr);        \
    (type *)( (char *)__mptr - offsetof(type,member) );})
```

### Step-by-Step Pointer Arithmetic

Let's trace `container_of(kp, struct cdev, kobj)` where `kp` is a
`struct kobject *`:

```
Step 1: typeof( ((struct cdev *)0)->kobj )
        → This is "struct kobject"
        → const struct kobject *__mptr = kp;
        (Type-check: ensures kp really is a kobject pointer)

Step 2: offsetof(struct cdev, kobj)
        → Computes the byte offset of the kobj member within struct cdev
        → If kobj is the first member, this is 0
        → If there were fields before it, this would be nonzero

Step 3: (struct cdev *)( (char *)__mptr - offset )
        → Subtract the offset from the member pointer
        → This yields a pointer to the START of the enclosing struct
```

Visually:

```
Memory:
        ┌──────────────────────────┐  ← (struct cdev *)result
        │  struct kobject kobj     │  ← kp points here (offset 0)
        │    .name                 │
        │    .entry                │
        │    .parent               │
        │    .kref                 │
        │    ...                   │
        ├──────────────────────────┤
        │  struct module *owner    │
        │  const struct file_      │
        │      operations *ops     │
        │  struct list_head list   │
        │  dev_t dev               │
        │  unsigned int count      │
        └──────────────────────────┘

container_of(kp, struct cdev, kobj):
    result = (char *)kp - offsetof(struct cdev, kobj)
           = (char *)kp - 0    (kobj is first member)
           = kp                 (same address!)
```

When the embedded member is **not** the first field, the subtraction is
non-trivial — and that's exactly the case that makes `container_of`
essential.

---

## 2.3 Real-World Inheritance: ext4 Inode

The most important inheritance chain in the VFS is how filesystems extend
`struct inode`.

### The Base Class

`include/linux/fs.h`, lines 749–838 — `struct inode` (70+ fields used by
the VFS layer).

### The Derived Class

`fs/ext4/ext4.h`, lines 784–845:

```c
struct ext4_inode_info {
    __le32  i_data[15];         /* ext4-specific block pointers */
    __u32   i_dtime;
    ext4_fsblk_t i_file_acl;

    /* ... 50+ ext4-specific fields ... */

    struct inode vfs_inode;     /* <-- THE BASE CLASS (line 844) */
    struct jbd2_inode *jinode;
};
```

Notice: `struct inode vfs_inode` is embedded **near the end** of the struct,
not at the beginning. This is perfectly valid — `container_of` handles any
offset.

### The Downcast Macro

`fs/ext4/ext4.h`, lines 1258–1261:

```c
static inline struct ext4_inode_info *
EXT4_I(struct inode *inode)
{
    return container_of(inode, struct ext4_inode_info, vfs_inode);
}
```

Whenever the VFS calls an ext4 function and passes a `struct inode *`, ext4
immediately downcasts:

```c
void
ext4_some_operation(struct inode *inode)
{
    struct ext4_inode_info *ei = EXT4_I(inode);
    /* now ei has access to ALL ext4-specific fields */
}
```

### Memory Layout

```
  Memory layout of struct ext4_inode_info:
  ┌─────────────────────────────────┐  ← struct ext4_inode_info *ei
  │  __le32 i_data[15]              │
  │  __u32  i_dtime                 │
  │  ext4_fsblk_t i_file_acl        │
  │  ...                            │
  │  (50+ ext4-specific fields)     │
  │  ...                            │
  ├─────────────────────────────────┤  ← &ei->vfs_inode
  │  struct inode vfs_inode         │     (this pointer is what VFS sees)
  │    .i_mode                      │
  │    .i_op                        │
  │    .i_fop                       │
  │    .i_ino                       │
  │    ...                          │
  ├─────────────────────────────────┤
  │  struct jbd2_inode *jinode      │
  └─────────────────────────────────┘

  container_of(&ei->vfs_inode, struct ext4_inode_info, vfs_inode)
  subtracts offsetof(struct ext4_inode_info, vfs_inode) from the
  inode pointer, recovering ei.
```

### The C++ Equivalent

```cpp
class Ext4InodeInfo : public Inode {
    __le32  i_data[15];
    __u32   i_dtime;
    // ... ext4-specific fields ...
};

// Downcasting:
Ext4InodeInfo *ei = static_cast<Ext4InodeInfo*>(inode);
```

The critical difference: in C++, `static_cast` relies on the compiler knowing
the inheritance relationship. In C, `container_of` is **manual** — the
programmer must know which struct type embeds which member. There is no
compiler check that the cast is valid.

---

## 2.4 Other Inheritance Chains in v3.2

The embedding pattern is used throughout the kernel:

### `struct device` → `struct pci_dev`

`include/linux/pci.h`, lines 238, 439:

```c
struct pci_dev {
    struct list_head bus_list;
    struct pci_bus  *bus;
    /* ... 200 lines of PCI-specific fields ... */
    struct device   dev;          /* line 439 */
    /* ... more fields ... */
};
```

### `struct device` → `struct net_device`

`include/linux/netdevice.h`, line 1292:

```c
struct net_device {
    char            name[IFNAMSIZ];
    /* ... 300+ lines of networking fields ... */
    struct device   dev;          /* line 1292 */
    /* ... more fields ... */
};
```

### `struct kobject` → `struct cdev`

As shown in section 2.1:

```c
struct cdev {
    struct kobject kobj;          /* line 13 of cdev.h */
    /* ... cdev-specific fields ... */
};
```

### The Pattern

```
                    struct kobject
                    /           \
            struct device    struct cdev
            /         \
    struct pci_dev  struct net_device
```

Each arrow means "the child embeds the parent as a struct member."
Each `container_of` call traverses one arrow upward (from parent pointer
to child pointer).

---

## 2.5 Why Not a Union or Tagged Struct?

An alternative to embedding would be a single struct with a type tag
and a union of all possible derived types:

```c
/* HYPOTHETICAL — the kernel does NOT do this */
struct inode {
    enum { INODE_EXT4, INODE_XFS, INODE_BTRFS, ... } type;
    /* common fields */
    union {
        struct ext4_specific ext4;
        struct xfs_specific  xfs;
        struct btrfs_specific btrfs;
    };
};
```

This approach fails for three reasons:

1. **Coupling**: Adding a new filesystem would require modifying `struct inode`
   itself — every filesystem in the tree would need to be listed in the union.

2. **Wasted space**: The union is sized to the largest member. If ext4 needs
   500 bytes but tmpfs needs 8, every tmpfs inode wastes ~492 bytes.

3. **Out-of-tree modules**: External filesystems (loaded as modules) could
   never add their own variant to the union.

Struct embedding solves all three problems. Each filesystem defines its own
derived struct independently. The VFS never needs to know about it.

---

## 2.6 The `alloc_inode` Hook — Constructor for Derived Types

How does ext4 ensure that every inode allocated for its filesystem is
actually an `ext4_inode_info` (with the `vfs_inode` embedded inside)?

The `super_operations` vtable includes an `alloc_inode` hook:

`include/linux/fs.h`, lines 1658–1659:

```c
struct super_operations {
    struct inode *(*alloc_inode)(struct super_block *sb);
    void (*destroy_inode)(struct inode *);
    /* ... */
};
```

ext4's implementation (`fs/ext4/super.c`) allocates the **full derived
struct** and returns a pointer to the embedded base:

```c
static struct inode *
ext4_alloc_inode(struct super_block *sb)
{
    struct ext4_inode_info *ei;

    ei = kmem_cache_alloc(ext4_inode_cachep, GFP_NOFS);
    if (!ei)
        return NULL;
    /* ... initialize ext4-specific fields ... */
    return &ei->vfs_inode;    /* return pointer to embedded base */
}
```

The VFS receives a `struct inode *` and is none the wiser that it is
actually pointing into the middle of a larger `ext4_inode_info`. This
is **polymorphic construction** in pure C.

---

## Exercise

Write a `container_of` usage by hand:

`struct net_device` (defined in `include/linux/netdevice.h`) contains a
`struct device dev` member at line 1292.

Given a `struct device *d` that you know points to the `dev` member of a
`net_device`, write the macro invocation that recovers the `net_device *`.

```c
struct net_device *ndev = ???;
```

**Answer** (try before looking):

```c
struct net_device *ndev = container_of(d, struct net_device, dev);
```

Now verify: the kernel itself defines `to_net_dev()` in
`include/linux/netdevice.h` — search for it and confirm the pattern.

---

## Socratic Check

Before moving to Module 3, answer:

> The VFS calls `inode->i_op->lookup(inode, dentry, nd)` on an inode
> that belongs to an ext4 filesystem. Inside ext4's `lookup` function,
> how does ext4 access its private `i_data[15]` array?
>
> (The answer uses both `container_of` from this module AND the
> polymorphic dispatch from Module 3.)

Proceed to [Module 3: Polymorphism](03_polymorphism.md).
