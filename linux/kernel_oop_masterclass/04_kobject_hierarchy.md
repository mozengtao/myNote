# Module 4: The kobject Hierarchy — A Complete Object System

> **Core question**: The kernel doesn't just use ad-hoc OOP tricks. It has a
> formal object system with reference counting, a type system, hierarchical
> naming, and automatic cleanup. Where does it live?

---

## 4.1 `kobject` — The Universal Base Class

Every kernel object that participates in the device model or appears in
`/sys` derives from `struct kobject`. It is the kernel's `java.lang.Object`.

### The Real Code

`include/linux/kobject.h`, lines 60–73:

```c
struct kobject {
    const char          *name;
    struct list_head    entry;
    struct kobject      *parent;
    struct kset         *kset;
    struct kobj_type    *ktype;
    struct sysfs_dirent *sd;
    struct kref         kref;
    unsigned int state_initialized:1;
    unsigned int state_in_sysfs:1;
    unsigned int state_add_uevent_sent:1;
    unsigned int state_remove_uevent_sent:1;
    unsigned int uevent_suppress:1;
};
```

Each field serves a specific role in the object system:

| Field       | Role                                                       |
|-------------|------------------------------------------------------------|
| `name`      | Object identity — appears as directory name in `/sys`      |
| `parent`    | Tree hierarchy — forms the directory nesting in `/sys`     |
| `kset`      | Collection membership — which group this object belongs to |
| `ktype`     | Type descriptor — contains destructor and sysfs methods    |
| `kref`      | Reference counting — prevents premature destruction        |
| `sd`        | sysfs directory entry — representation in `/sys`           |
| `entry`     | Linked list node — for iterating within a kset             |
| `state_*`   | Lifecycle tracking — bit flags for initialization state    |

### The C++ Equivalent

```cpp
class KernelObject {
public:
    std::string name;
    KernelObject *parent;
    KernelObjectType *type;      // "class descriptor"
    std::shared_ptr<void> ref;   // reference counting
    virtual ~KernelObject();     // destructor via ktype->release
protected:
    KernelObjectSet *set;
    SysfsEntry *sysfs_dir;
};
```

---

## 4.2 `kobj_type` — The Class Descriptor

If `kobject` is the object instance, `kobj_type` is the **class definition**.
It tells the runtime how objects of this type behave.

### The Real Code

`include/linux/kobject.h`, lines 108–114:

```c
struct kobj_type {
    void (*release)(struct kobject *kobj);
    const struct sysfs_ops *sysfs_ops;
    struct attribute **default_attrs;
    const struct kobj_ns_type_operations *(*child_ns_type)(
        struct kobject *kobj);
    const void *(*namespace)(struct kobject *kobj);
};
```

| Field           | Role                                              |
|-----------------|---------------------------------------------------|
| `release`       | **Destructor** — called when refcount hits zero   |
| `sysfs_ops`     | vtable for reading/writing sysfs attributes       |
| `default_attrs` | Class-level metadata — attributes all instances share |

The `release` function is the most important. It is the **destructor** — the
function that frees the object when no one references it anymore.

### Relationship Between kobject and kobj_type

```
Many instances                    One class descriptor
┌──────────────┐                ┌───────────────────┐
│ kobject "sda"│─── .ktype ────→│ struct kobj_type  │
│              │                │   .release()      │ ← destructor
└──────────────┘                │   .sysfs_ops      │ ← attribute vtable
                                │   .default_attrs  │ ← class attributes
┌──────────────┐      ┌────────→│                   │
│ kobject "sdb"│──────┘         └───────────────────┘
│              │
└──────────────┘

Multiple instances share the same kobj_type,
just as multiple objects share the same class.
```

---

## 4.3 `kset` — A Collection of Same-Type Objects

A `kset` groups related kobjects together. Think of it as
`std::set<kobject*>` with type enforcement.

### The Real Code

`include/linux/kobject.h`, lines 159–164:

```c
struct kset {
    struct list_head list;
    spinlock_t list_lock;
    struct kobject kobj;
    const struct kset_uevent_ops *uevent_ops;
};
```

A kset is itself a kobject (it embeds `struct kobject kobj`). This means
ksets form a hierarchy — a kset can be a child of another kset, creating
the tree structure you see in `/sys`.

### The `/sys` Directory Tree

```
/sys/
├── block/              ← a kset containing block device kobjects
│   ├── sda             ← kobject (type: disk_type)
│   ├── sdb             ← kobject (type: disk_type)
│   └── sr0             ← kobject (type: disk_type)
├── bus/                ← a kset containing bus subsystem ksets
│   ├── pci/
│   │   └── devices/
│   │       ├── 0000:00:00.0  ← kobject (type: pci_dev_type)
│   │       └── 0000:00:1f.0
│   └── usb/
└── devices/            ← a kset: the device hierarchy
    ├── system/
    └── virtual/
```

Each directory in `/sys` corresponds to a kobject. Each subdirectory is
a child kobject. The tree structure is built entirely from `kobject.parent`
pointers and `kset.list` membership.

### Full Relationship Diagram

```
  kset ("block devices")
  ┌───────────────────────┐
  │ struct kset           │
  │   .kobj ──────────────┼──→ appears as /sys/block/
  │   .list ──┐           │
  └───────────┼───────────┘
              │
              ▼
  ┌───────────────┐     ┌───────────────┐     ┌───────────────┐
  │ kobject "sda" │ ──→ │ kobject "sdb" │ ──→ │ kobject "sr0" │
  │  .ktype ──────┼─┐   │               │     │               │
  └───────────────┘ │   └───────────────┘     └───────────────┘
                    │
                    ▼
           ┌──────────────┐
           │ kobj_type    │
           │  .release()  │  ← destructor
           │  .sysfs_ops  │  ← vtable for attributes
           └──────────────┘
```

---

## 4.4 Reference Counting with `kref`

The kernel cannot use garbage collection. Instead, it uses **reference
counting** to manage object lifetimes. The `kref` struct is the standard
implementation.

### The Real Code

`include/linux/kref.h`, lines 20–28:

```c
struct kref {
    atomic_t refcount;
};

void kref_init(struct kref *kref);
void kref_get(struct kref *kref);
int  kref_put(struct kref *kref,
              void (*release)(struct kref *kref));
int  kref_sub(struct kref *kref, unsigned int count,
              void (*release)(struct kref *kref));
```

### The Lifecycle

```
kref_init()          Sets refcount to 1
    │
    ▼
kref_get()           Increments refcount (another user acquired a reference)
kref_get()           refcount is now 3
    │
    ▼
kref_put()           Decrements refcount → 2 (not zero, object lives)
kref_put()           Decrements refcount → 1
kref_put()           Decrements refcount → 0 → calls release() callback!
    │
    ▼
release()            Destructor runs — frees memory, cleans up
```

### The C++ Equivalent

```cpp
// kref ≈ std::shared_ptr with a custom deleter
std::shared_ptr<KernelObject> obj(
    new KernelObject(),
    [](KernelObject *p) { custom_destructor(p); }  // ← kref's release
);

auto ref2 = obj;    // ← kref_get (refcount: 1 → 2)
ref2.reset();        // ← kref_put (refcount: 2 → 1)
obj.reset();         // ← kref_put (refcount: 1 → 0 → destructor called)
```

### How kobject Uses kref

`lib/kobject.c`, lines 578–598:

```c
static void
kobject_release(struct kref *kref)
{
    kobject_cleanup(container_of(kref, struct kobject, kref));
}

void
kobject_put(struct kobject *kobj)
{
    if (kobj) {
        if (!kobj->state_initialized)
            WARN(1, KERN_WARNING "kobject: '%s' (%p): is not "
                   "initialized, yet kobject_put() is being "
                   "called.\n", kobject_name(kobj), kobj);
        kref_put(&kobj->kref, kobject_release);
    }
}
```

Notice `kobject_release` uses `container_of` — the same pattern from
Module 2! Given a `struct kref *` (the member), it recovers the enclosing
`struct kobject *` (the container). Then `kobject_cleanup` calls
`kobj->ktype->release()` — the user-defined destructor.

### What `kobject_cleanup` Does

`lib/kobject.c`, lines 538–568 (simplified):

```c
static void
kobject_cleanup(struct kobject *kobj)
{
    struct kobj_type *t = get_ktype(kobj);

    /* auto-send "remove" uevent if not sent */
    if (kobj->state_add_uevent_sent &&
        !kobj->state_remove_uevent_sent)
        kobject_uevent(kobj, KOBJ_REMOVE);

    /* auto-remove from sysfs */
    if (kobj->state_in_sysfs)
        kobject_del(kobj);

    /* call the type's destructor */
    if (t && t->release)
        t->release(kobj);

    /* clean up the name */
    /* ... */
}
```

The cleanup sequence is:
1. Send a uevent notification (so userspace `udev` knows the object is gone)
2. Remove the sysfs directory entry
3. Call the type-specific destructor (`ktype->release`)
4. Free the name string

This is a **template method** (Module 5) — the framework defines the
cleanup algorithm, and the concrete type fills in the `release` step.

---

## 4.5 The kobject API

`include/linux/kobject.h`, lines 85–104 — the public API:

```c
/* Constructor: initialize + register in sysfs hierarchy */
int kobject_init_and_add(struct kobject *kobj,
                         struct kobj_type *ktype,
                         struct kobject *parent,
                         const char *fmt, ...);

/* Reference counting */
struct kobject *kobject_get(struct kobject *kobj);  /* increment */
void kobject_put(struct kobject *kobj);             /* decrement */

/* Hierarchy manipulation */
int kobject_rename(struct kobject *, const char *new_name);
int kobject_move(struct kobject *, struct kobject *new_parent);

/* Destruction */
void kobject_del(struct kobject *kobj);   /* remove from sysfs */
```

### Lifecycle Flow

```
  kobject_init_and_add()
  ┌────────────────────────────┐
  │ 1. Set ktype               │
  │ 2. Set parent              │
  │ 3. kref_init (refcount=1)  │
  │ 4. Create sysfs directory  │
  │ 5. Set state_in_sysfs=1    │
  └────────────────────────────┘
           │
           ▼
  Normal operation: kobject_get() / kobject_put()
  (refcount goes up and down as users acquire/release references)
           │
           ▼
  Last kobject_put() → refcount reaches 0
  ┌────────────────────────────┐
  │ kobject_release()          │
  │   → kobject_cleanup()      │
  │     → uevent(KOBJ_REMOVE)  │
  │     → kobject_del() (sysfs)│
  │     → ktype->release()     │ ← YOUR destructor runs here
  └────────────────────────────┘
```

---

## 4.6 Why sysfs Files Don't Cause Use-After-Free

A common pitfall for kernel module authors: removing a device while a sysfs
file referencing it is still open.

The kobject system prevents this because:

1. **Opening a sysfs file calls `kobject_get()`** — incrementing the
   refcount. The object cannot be freed while the file is open.

2. **Closing the sysfs file calls `kobject_put()`** — decrementing the
   refcount. If this was the last reference, the destructor runs.

3. **`kobject_del()` removes the sysfs entry** but does NOT free the
   object. It merely makes the object invisible. The object lives until
   the last `kobject_put()`.

This is the same pattern as `shared_ptr` preventing use-after-free in C++:
the object cannot be destroyed while any reference exists.

---

## 4.7 The Object System Hierarchy

Putting it all together:

```
┌──────────────────────────────────────────────────────────┐
│              THE KERNEL OBJECT SYSTEM                    │
│                                                          │
│  Concepts:                   C++ Equivalent:             │
│                                                          │
│  struct kobject              java.lang.Object            │
│    ├── name                  .toString()                 │
│    ├── parent                tree hierarchy              │
│    ├── kref                  shared_ptr refcount         │
│    └── ktype ──→ kobj_type   Class<T> / type_info        │
│                   ├── release()    destructor            │
│                   ├── sysfs_ops    reflection / toString │
│                   └── default_attrs class-level fields   │
│                                                          │
│  struct kset                 Collection / Container      │
│    ├── list                  iterator over members       │
│    ├── kobj                  itself a kobject (recursive)│
│    └── uevent_ops            event hooks                 │
│                                                          │
│  struct kref                 shared_ptr core             │
│    └── refcount              atomic reference counter    │
│                                                          │
│  /sys filesystem             reflection / toString       │
│    reads kobj→name           for object identity         │
│    reads kobj→parent         for hierarchy               │
│    reads ktype→default_attrs for attributes              │
│    calls sysfs_ops→show()    for attribute values        │
│                                                          │
└──────────────────────────────────────────────────────────┘
```

---

## Exercise

Trace the lifecycle of a kobject from birth to death:

1. **Construction**: Find `kobject_init_and_add()` in `lib/kobject.c`.
   What does it set up? What is the initial refcount?

2. **Usage**: How would a driver acquire a temporary reference to an
   existing kobject? What function does it call? What must it do when
   finished?

3. **Destruction**: When `kobject_put()` drops the refcount to zero,
   trace the call chain:
   `kobject_put → kref_put → kobject_release → kobject_cleanup → ktype->release`

4. **Safety question**: A sysfs file at `/sys/block/sda/size` is open in
   userspace. The SCSI driver tries to remove the disk. What prevents a
   use-after-free on the `kobject`?

---

## Socratic Check

Before moving to Module 5, answer:

> `struct kset` itself contains a `struct kobject kobj` member. This
> means a kset IS-A kobject (Module 2 inheritance). The kset appears
> as a directory in sysfs, and its member kobjects appear as
> subdirectories.
>
> If I create a kset called "my_devices" and add three kobjects to it,
> what would the sysfs tree look like? What happens to the member
> kobjects if I call `kset_unregister()` on the kset?

Proceed to [Module 5: Classic Design Patterns](05_design_patterns.md).
