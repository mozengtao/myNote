# Phase 3 — Case Study: Device Model and True Object Hierarchies

## Overview

While the VFS demonstrates polymorphism through flat function-pointer
dispatch, the device model (`kobject`, `kset`, `device`, `bus_type`)
demonstrates something deeper: **hierarchical object relationships**
with automatic lifetime management, parent-child trees, and runtime
introspection via sysfs.

If the VFS is the kernel's interface system, the device model is the
kernel's class hierarchy.

---

## `struct kobject` — The Universal Base Class

**Source:** `include/linux/kobject.h` lines 60–73

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

`kobject` is the kernel's `Object` — the root of the device model's
inheritance tree. It provides:

| Capability           | Mechanism                          |
|----------------------|------------------------------------|
| Identity             | `name`                             |
| Hierarchy            | `parent` pointer, `kset` membership|
| Reference counting   | `kref` (embedded `struct kref`)    |
| Type information     | `ktype` (points to `kobj_type`)    |
| sysfs representation | `sd` (sysfs directory entry)       |
| Lifecycle events     | uevent state bits                  |

### The Hierarchy

```
  /sys/
   ├── devices/           ← kset
   │   ├── platform/      ← kobject (parent)
   │   │   ├── serial0/   ← kobject (child)
   │   │   └── i2c-0/     ← kobject (child)
   │   └── pci0000:00/
   ├── bus/               ← kset
   │   ├── pci/
   │   └── usb/
   └── class/             ← kset
       ├── net/
       └── block/
```

Every directory in `/sys/` corresponds to a `kobject`. The tree
structure mirrors the `parent` pointers. This is not metadata — it is
the actual runtime object graph, exposed to userspace.

---

## `struct kobj_type` — The Type Descriptor

**Source:** `include/linux/kobject.h` lines 108–114

```c
struct kobj_type {
    void (*release)(struct kobject *kobj);
    const struct sysfs_ops *sysfs_ops;
    struct attribute **default_attrs;
    const struct kobj_ns_type_operations *(*child_ns_type)(struct kobject *kobj);
    const void *(*namespace)(struct kobject *kobj);
};
```

`kobj_type` is the **metaclass** — it defines:

- **`release`**: The destructor. Called when the reference count reaches
  zero. This is the single most important function pointer in the device
  model.
- **`sysfs_ops`**: How to read/write sysfs attributes for this type.
- **`default_attrs`**: Attributes automatically created in sysfs.

Every `kobject` must have a `ktype`. This is enforced at initialization:

```c
/* lib/kobject.c line 270 */
void kobject_init(struct kobject *kobj, struct kobj_type *ktype)
{
    /* ... */
    if (!ktype) {
        err_str = "must have a ktype to be initialized properly!\n";
        goto error;
    }
    /* ... */
    kref_init(&kobj->kref);
    /* ... */
}
```

---

## `struct kref` — The Reference Counting Primitive

**Source:** `include/linux/kref.h` lines 20–28

```c
struct kref {
    atomic_t refcount;
};

void kref_init(struct kref *kref);
void kref_get(struct kref *kref);
int kref_put(struct kref *kref, void (*release)(struct kref *kref));
```

### The Implementation

**Source:** `lib/kref.c`

```c
void
kref_init(struct kref *kref)
{
    atomic_set(&kref->refcount, 1);
    smp_mb();
}

void
kref_get(struct kref *kref)
{
    WARN_ON(!atomic_read(&kref->refcount));
    atomic_inc(&kref->refcount);
    smp_mb__after_atomic_inc();
}

int
kref_put(struct kref *kref, void (*release)(struct kref *kref))
{
    WARN_ON(release == NULL);
    WARN_ON(release == (void (*)(struct kref *))kfree);

    if (atomic_dec_and_test(&kref->refcount)) {
        release(kref);
        return 1;
    }
    return 0;
}
```

**Critical design decisions:**

1. **`kref_put` takes a function pointer, not a fixed destructor.**
   This is the C equivalent of a virtual destructor. The caller passes
   the release function because `kref` doesn't know what it's embedded
   in.

2. **`WARN_ON(release == kfree)`** — You must never pass `kfree`
   directly. The release function must use `container_of` to recover the
   enclosing object and free *that*. Passing `kfree` would free only the
   `kref` member — a guaranteed memory corruption.

3. **`WARN_ON(!atomic_read(&kref->refcount))` in `kref_get`** — Taking
   a reference on a zero-refcount object is a use-after-free bug. This
   is a debug assertion, not a fix.

4. **Memory barriers.** `smp_mb()` after init and
   `smp_mb__after_atomic_inc()` after get ensure visibility across CPUs.
   Without these, one CPU could see a stale refcount and free the object
   while another CPU still holds a reference.

### RAII-Like Semantics Without RAII

The pattern is:

```
    kobject_get(kobj)       ← acquire reference (like shared_ptr copy)
    /* ... use kobj ... */
    kobject_put(kobj)       ← release reference (like shared_ptr destructor)
```

When the last `kobject_put` drops the refcount to zero, the `release()`
callback fires. This is deterministic destruction — no garbage collector,
no deferred finalization.

---

## The Destruction Chain

**Source:** `lib/kobject.c` lines 535–598

When `kobject_put()` drops the refcount to zero:

```
  kobject_put(kobj)
       │
       ▼
  kref_put(&kobj->kref, kobject_release)
       │
       │  if (atomic_dec_and_test(&kref->refcount))
       │
       ▼
  kobject_release(kref)
       │
       │  kobj = container_of(kref, struct kobject, kref)
       │
       ▼
  kobject_cleanup(kobj)
       │
       ├── Send KOBJ_REMOVE uevent (if add was sent)
       ├── sysfs_remove_dir(kobj)     ← remove /sys entry
       ├── kobject_del(kobj)          ← unlink from parent
       │
       ▼
       t = get_ktype(kobj)
       if (t && t->release)
           t->release(kobj)           ← VIRTUAL DESTRUCTOR
       │
       ▼
       kfree(name)                    ← free name string
```

The `release` function in `kobj_type` is the virtual destructor. Each
concrete type provides its own:

```c
/* lib/kobject.c line 600 */
static void
dynamic_kobj_release(struct kobject *kobj)
{
    pr_debug("kobject: (%p): %s\n", kobj, __func__);
    kfree(kobj);
}
```

For a `kset`:

```c
/* lib/kobject.c line 802 */
static void
kset_release(struct kobject *kobj)
{
    struct kset *kset = container_of(kobj, struct kset, kobj);
    pr_debug("kobject: '%s' (%p): %s\n",
             kobject_name(kobj), kobj, __func__);
    kfree(kset);
}
```

The pattern is always: `container_of` to recover the derived type,
then `kfree` the derived type. This is C's manual equivalent of a
C++ virtual destructor chain.

---

## `struct kset` — Subclassing kobject

**Source:** `include/linux/kobject.h` lines 159–164

```c
struct kset {
    struct list_head list;
    spinlock_t list_lock;
    struct kobject kobj;                    /* <-- embedded base class */
    const struct kset_uevent_ops *uevent_ops;
};
```

`kset` is a **derived class** of `kobject`. It adds:
- A list of member kobjects
- A lock for that list
- uevent filtering operations

### The Downcast Helper

```c
/* include/linux/kobject.h line 173 */
static inline struct kset *to_kset(struct kobject *kobj)
{
    return kobj ? container_of(kobj, struct kset, kobj) : NULL;
}
```

### The get/put Wrappers

```c
static inline struct kset *kset_get(struct kset *k)
{
    return k ? to_kset(kobject_get(&k->kobj)) : NULL;
}

static inline void kset_put(struct kset *k)
{
    kobject_put(&k->kobj);
}
```

Note how `kset_get` chains: it calls `kobject_get` on the embedded
kobject (incrementing the refcount), then uses `to_kset` (i.e.,
`container_of`) to return the derived type. This is a single operation
that both increments the refcount and returns the correctly-typed pointer.

---

## `struct device` — The Full Inheritance Chain

**Source:** `include/linux/device.h` lines 560–612

```c
struct device {
    struct device           *parent;
    struct device_private   *p;
    struct kobject kobj;                    /* <-- embedded kobject */
    const char              *init_name;
    const struct device_type *type;
    struct mutex            mutex;
    struct bus_type         *bus;
    struct device_driver    *driver;
    void                    *platform_data;
    struct dev_pm_info      power;
    dev_t                   devt;
    struct class            *class;
    void    (*release)(struct device *dev);
    /* ... */
};
```

**The inheritance chain:**

```
  struct device
    └── embeds struct kobject
          └── contains struct kref
                └── contains atomic_t refcount
```

This is three levels deep. When `kobject_put` drops the device's
refcount to zero, the chain is:

```
  kref_put()
    → kobject_release()
      → kobject_cleanup()
        → kobj_type->release(kobj)
          → container_of(kobj, struct device, kobj)
            → device->release(dev)    OR    type->release(dev)
```

### `struct bus_type` — Function Tables for Driver Binding

**Source:** `include/linux/device.h` lines 87–100

```c
struct bus_type {
    const char      *name;
    struct bus_attribute    *bus_attrs;
    struct device_attribute *dev_attrs;
    struct driver_attribute *drv_attrs;

    int (*match)(struct device *dev, struct device_driver *drv);
    int (*uevent)(struct device *dev, struct kobj_uevent_env *env);
    int (*probe)(struct device *dev);
    int (*remove)(struct device *dev);
    void (*shutdown)(struct device *dev);
    int (*suspend)(struct device *dev, pm_message_t state);
    int (*resume)(struct device *dev);
    /* ... */
};
```

`bus_type` is another vtable — but for the bus layer, not for individual
devices. The `match` function implements the "which driver handles which
device" logic:

- PCI bus: matches by vendor/device ID
- USB bus: matches by interface class/subclass
- Platform bus: matches by name

This is polymorphism at the bus level. The driver core calls
`bus->match(dev, drv)` without knowing what kind of bus it is.

---

## Object Relationship Diagram

```
  struct bus_type                    struct device_driver
  ┌──────────────┐                  ┌──────────────────┐
  │  name         │                  │  name             │
  │  *match()     │◄────────────────│  *bus              │
  │  *probe()     │                  │  *probe()         │
  │  *remove()    │                  │  *remove()        │
  └──────┬───────┘                  └────────┬─────────┘
         │  has_many                          │  binds_to
         ▼                                    ▼
  struct device
  ┌──────────────────────────────────────────────┐
  │  *parent                                      │
  │  kobject kobj  ───► struct kobj_type           │
  │    └── kref                                    │
  │         └── atomic_t refcount                  │
  │  *bus  ────────────► struct bus_type            │
  │  *driver ──────────► struct device_driver       │
  │  *class                                        │
  │  (*release)(dev)                               │
  └──────────────────────────────────────────────┘
```

---

## `struct cdev` — Character Device as kobject Subclass

**Source:** `include/linux/cdev.h` lines 12–19

```c
struct cdev {
    struct kobject kobj;                /* <-- embedded kobject */
    struct module *owner;
    const struct file_operations *ops;  /* <-- vtable */
    struct list_head list;
    dev_t dev;
    unsigned int count;
};
```

`cdev` is a `kobject` subclass that bridges the device model with the
VFS. It carries:
- A `kobject` for lifetime management and sysfs
- A `file_operations` pointer for VFS dispatch

When a character device file is opened, the VFS finds the `cdev` via
the device number, and installs `cdev->ops` as the file's `f_op`.
This is where the device model and VFS object systems connect.

---

## Reflection Questions

1. **Why does `kref_put` require the caller to pass the `release`
   function every time?** Why not store it in the `kref` struct?

2. **Why does `kobject_cleanup` send a `KOBJ_REMOVE` uevent
   automatically?** What would break in userspace if it didn't?

3. **In the `kset_release` function, why must it call
   `container_of(kobj, struct kset, kobj)` before `kfree`?**
   Why can't it just `kfree(kobj)`?

4. **The `struct device` has both `kobj_type->release` and its own
   `device->release` callback. Why two levels of destruction?**

5. **How does the `bus_type->match()` function pointer achieve the same
   result as visitor pattern or double dispatch in OOP?**
