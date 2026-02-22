# Phase 4 — Memory and Ownership Model

## Why This Matters

Getting a struct layout right is the easy part. Getting lifetime right
is where kernel code becomes dangerous. Every use-after-free, every
double-free, every refcount leak is a lifetime bug. In kernel space,
these are not just crashes — they are exploitable security vulnerabilities,
data corruption, and system hangs.

This phase covers how the kernel manages object memory, who owns what,
and why destruction ordering is so critical.

---

## Slab Allocation: Object-Oriented Memory

### The Problem with `kmalloc`

`kmalloc` is the kernel's general-purpose allocator. It works, but it
has two problems for frequently allocated objects:

1. **Internal fragmentation.** `kmalloc` rounds up to the next power
   of two. A 192-byte struct wastes 64 bytes in a 256-byte allocation.

2. **No constructor caching.** Every allocation starts from uninitialized
   memory. For complex objects with locks, lists, and other fields that
   need initialization, this means repeating the same setup on every
   allocation.

### The Solution: `kmem_cache`

The slab allocator creates **typed pools** — caches of identically-sized,
pre-initialized objects:

```c
/* fs/ext2/super.c line 160 */
static struct kmem_cache *ext2_inode_cachep;
```

**Cache creation:**

```c
/* fs/ext2/super.c line 197 */
static int
init_inodecache(void)
{
    ext2_inode_cachep = kmem_cache_create("ext2_inode_cache",
                                          sizeof(struct ext2_inode_info),
                                          0,
                                          (SLAB_RECLAIM_ACCOUNT |
                                           SLAB_MEM_SPREAD),
                                          init_once);
    if (ext2_inode_cachep == NULL)
        return -ENOMEM;
    return 0;
}
```

**The constructor (`init_once`):**

```c
/* fs/ext2/super.c line 185 */
static void
init_once(void *foo)
{
    struct ext2_inode_info *ei = (struct ext2_inode_info *)foo;

    rwlock_init(&ei->i_meta_lock);
#ifdef CONFIG_EXT2_FS_XATTR
    init_rwsem(&ei->xattr_sem);
#endif
    mutex_init(&ei->truncate_mutex);
    inode_init_once(&ei->vfs_inode);
}
```

**Key properties:**

1. **Objects are pre-sized.** No rounding waste. Every slab page is
   carved into exact-size slots for `struct ext2_inode_info`.

2. **Constructor runs once per slab slot, not per allocation.** When an
   object is freed and re-allocated, the lock initializations from
   `init_once` are already done. This is the kernel's equivalent of
   placing constructors on the free list.

3. **Cache coloring.** The slab allocator offsets objects within pages
   to reduce cache-line conflicts across different CPUs.

**Allocation from the cache:**

```c
ei = (struct ext2_inode_info *)kmem_cache_alloc(ext2_inode_cachep,
                                                 GFP_KERNEL);
```

**Deallocation back to the cache:**

```c
kmem_cache_free(ext2_inode_cachep, EXT2_I(inode));
```

Note: you must pass the pointer to the **containing** (derived) object,
not the embedded base. `kmem_cache_free(ext2_inode_cachep, inode)` would
be a catastrophic bug — the slab allocator would interpret the
`struct inode` pointer as the start of the slab slot, compute wrong
slot boundaries, and corrupt the free list.

---

## Ownership Rules

### Rule 1: The Object That Embeds Owns the Embedded

```
  struct ext2_inode_info
  ┌──────────────────────┐
  │  ext2 fields         │  ← owned by ext2_inode_info
  │  struct inode vfs_inode │ ← owned by ext2_inode_info
  └──────────────────────┘
```

You never `kfree(&ei->vfs_inode)`. The `struct inode` is not separately
allocated — it lives inside the ext2 object. The ext2 destructor frees
the entire `ext2_inode_info`, which implicitly frees the embedded inode.

### Rule 2: `release()` Must Free the Containing Object

Every `kobj_type->release` callback must:
1. Use `container_of` to recover the derived type.
2. Free the derived type (which includes the embedded `kobject`).

```c
static void
kset_release(struct kobject *kobj)
{
    struct kset *kset = container_of(kobj, struct kset, kobj);
    kfree(kset);   /* frees kset, which includes embedded kobj */
}
```

If `release()` called `kfree(kobj)`, it would free from the wrong
address — partway through the `kset` structure — corrupting the heap.

### Rule 3: Refcount Determines Lifetime, Not Scope

Unlike stack variables in C++, kernel objects don't die when they go
out of scope. They die when their refcount hits zero:

```
  CPU 0                         CPU 1
  ─────                         ─────
  dev = get_device(dev);        dev = get_device(dev);
  /* refcount = 2 */            /* refcount = 2 */

  use(dev);                     use(dev);

  put_device(dev);              put_device(dev);
  /* refcount = 1 */            /* refcount = 0 → release() */
```

The object survives as long as any CPU holds a reference. This is
shared ownership — no single code path "owns" the object.

### Rule 4: Stack Allocation Is Forbidden for Refcounted Objects

You must never do:

```c
void bad_function(void)
{
    struct kobject kobj;        /* WRONG: stack-allocated */
    kobject_init(&kobj, &my_ktype);
    kobject_add(&kobj, parent, "name");
    /* ... */
}   /* kobj goes out of scope — but kobject_put might not have been
       called, or other code might still hold a reference */
```

`kobject_put()` calls `kfree()` on the object when the refcount reaches
zero. Calling `kfree()` on a stack address is undefined behavior and
will corrupt the kernel heap or panic.

Refcounted objects **must** be heap-allocated.

---

## Concurrency Hazards in Lifetime Management

### Hazard 1: Use-After-Free via Missing `get`

```c
/* Thread A */                  /* Thread B */
ptr = global_kobj;              kobject_put(global_kobj);
/* ptr still valid? */          /* refcount → 0 → kfree */
kobject_get(ptr);               /* ptr is now dangling */
/* CRASH: use-after-free */
```

**Fix:** Always take a reference *before* storing a pointer, not after.
The pattern is:

```c
ptr = kobject_get(global_kobj);     /* atomic: check + increment */
if (!ptr)
    return -ENODEV;
```

### Hazard 2: Refcount Underflow

If code calls `kobject_put` more times than `kobject_get`, the refcount
can wrap around (or trigger a `WARN_ON` in debug builds):

```c
kobject_init(&kobj, &ktype);    /* refcount = 1 */
kobject_put(&kobj);             /* refcount = 0 → freed */
kobject_put(&kobj);             /* refcount underflow → corruption */
```

### Hazard 3: Ordering Constraints in Destruction

Consider a `struct device` embedded in a driver-specific structure:

```c
struct my_device {
    struct device dev;
    struct my_hardware_state hw;
};
```

The destructor must be ordered:

1. Tear down `hw` (hardware-specific cleanup)
2. Unregister from sysfs
3. Drop parent references
4. Free `my_device`

If the order is wrong — say, the parent's refcount drops before `hw`
cleanup — the parent might be freed while the child still references it.

The kernel enforces this through the `kobject_cleanup` sequence in
`lib/kobject.c`:

```c
static void
kobject_cleanup(struct kobject *kobj)
{
    struct kobj_type *t = get_ktype(kobj);

    /* 1. Send uevent to userspace */
    if (kobj->state_add_uevent_sent &&
        !kobj->state_remove_uevent_sent)
        kobject_uevent(kobj, KOBJ_REMOVE);

    /* 2. Remove from sysfs */
    if (kobj->state_in_sysfs)
        kobject_del(kobj);

    /* 3. Call type-specific destructor */
    if (t && t->release)
        t->release(kobj);

    /* 4. Free the name */
    kfree(name);
}
```

The `kobject_del` step decrements the parent's refcount:

```c
void
kobject_del(struct kobject *kobj)
{
    sysfs_remove_dir(kobj);
    kobj->state_in_sysfs = 0;
    kobject_put(kobj->parent);
    kobj->parent = NULL;
}
```

This ensures: child destroyed → parent refcount decremented → parent
destroyed (if refcount hits zero). The destruction propagates up the tree.

---

## Destruction Ordering Diagram

```
  my_device_release(kobj)
       │
       │  my_dev = container_of(kobj, struct my_device, dev.kobj)
       │
       ├── my_hardware_teardown(&my_dev->hw)    ← driver cleanup
       │
       ├── (kobject_del already called:)
       │       sysfs_remove_dir()               ← /sys entry gone
       │       kobject_put(parent)              ← parent ref dropped
       │
       └── kfree(my_dev)                        ← memory freed
```

---

## RCU and Deferred Destruction

Some objects cannot be freed immediately because readers might still
be traversing them lock-free. The kernel uses Read-Copy-Update (RCU)
to defer destruction:

```c
/* fs/ext2/super.c line 173 */
static void
ext2_i_callback(struct rcu_head *head)
{
    struct inode *inode = container_of(head, struct inode, i_rcu);
    INIT_LIST_HEAD(&inode->i_dentry);
    kmem_cache_free(ext2_inode_cachep, EXT2_I(inode));
}

static void
ext2_destroy_inode(struct inode *inode)
{
    call_rcu(&inode->i_rcu, ext2_i_callback);
}
```

`call_rcu` does not free immediately. It queues the callback to run
after all CPUs have passed through a quiescent state (i.e., after all
current RCU read-side critical sections have completed). Only then is
it safe to free the memory.

This is the kernel's equivalent of garbage collection — but deterministic,
bounded, and zero-overhead for the readers.

---

## Summary: Ownership Model Rules

| Rule | Description |
|------|-------------|
| 1    | Embedded objects are owned by their container — never freed separately |
| 2    | `release()` must free the derived type via `container_of` |
| 3    | Refcount determines lifetime, not lexical scope |
| 4    | Refcounted objects must be heap-allocated |
| 5    | Take references before storing pointers, not after |
| 6    | Destruction order: children before parents |
| 7    | Use RCU for lock-free read-side reclamation |
| 8    | Slab caches for frequently allocated typed objects |

---

## What to Inspect

| File                      | Focus                                        |
|---------------------------|----------------------------------------------|
| `lib/kref.c`             | Full `kref_put` implementation with barriers |
| `lib/kobject.c:535–598`  | `kobject_cleanup` destruction sequence        |
| `fs/ext2/super.c:160–200`| Slab cache setup and `init_once` constructor |
| `mm/slab.c` or `mm/slub.c` | Slab allocator internals                  |

---

## Reflection Questions

1. **Why does `kref_put` use `atomic_dec_and_test` instead of
   `atomic_dec` followed by `atomic_read`?** What race condition
   would the two-step version create?

2. **Why does `init_once` initialize locks but not data fields?**
   Under what circumstances would a re-allocated slab object have
   stale lock state?

3. **If `ext2_destroy_inode` used `kfree` instead of `call_rcu`,
   what specific failure mode would occur under concurrent path
   lookup (RCU walk)?**

4. **Why is `container_of` applied in the destructor rather than
   storing the derived pointer in the `kobject`?** What would
   break if `kobject` had a `void *private` field for this purpose?
