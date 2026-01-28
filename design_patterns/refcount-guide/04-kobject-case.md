# Case 2: kobject Reference Counting

## Subsystem Background

```
+=============================================================================+
|                    KOBJECT REFERENCE COUNTING                                |
+=============================================================================+

    KOBJECT: THE KERNEL OBJECT
    ==========================

    struct kobject is the base "class" for kernel objects.
    Every object in /sys (sysfs) is backed by a kobject.

    kobject provides:
    - Reference counting (via embedded kref)
    - Sysfs representation
    - Hierarchy (parent/child)
    - Hotplug events


    KOBJECT STRUCTURE:
    ==================

    struct kobject {
        const char          *name;
        struct list_head    entry;      /* In parent's list */
        struct kobject      *parent;
        struct kset         *kset;
        struct kobj_type    *ktype;     /* Has release function */
        struct sysfs_dirent *sd;
        struct kref         kref;       /* EMBEDDED reference counter */
        /* ... */
    };


    DOUBLE INDIRECTION:
    ===================

    kobject uses kref internally:
    
    kobject_get()  -->  kref_get(&kobj->kref)
    kobject_put()  -->  kref_put(&kobj->kref, kobject_release)
    
    When kref hits 0, kobject_release() is called.
    kobject_release() calls kobj->ktype->release(kobj).
```

**中文说明：**

kobject是内核对象的基类，每个sysfs中的对象都有一个kobject。kobject通过内嵌的kref提供引用计数。当调用`kobject_put()`时，内部调用`kref_put()`；当引用计数为0时，调用`kobject_release()`，然后调用`kobj->ktype->release(kobj)`。

---

## kobject Reference Counting API

```c
/* include/linux/kobject.h */

/**
 * kobject_get - increment refcount
 * @kobj: object to increment
 *
 * Returns the kobject if successful, NULL otherwise.
 */
struct kobject *kobject_get(struct kobject *kobj)
{
    if (kobj)
        kref_get(&kobj->kref);
    return kobj;
}

/**
 * kobject_put - decrement refcount
 * @kobj: object to decrement
 *
 * If refcount hits 0, release function is called.
 */
void kobject_put(struct kobject *kobj)
{
    if (kobj) {
        if (!kobj->state_initialized)
            WARN(...);
        kref_put(&kobj->kref, kobject_release);
    }
}

/* Internal release function */
static void kobject_release(struct kref *kref)
{
    struct kobject *kobj = container_of(kref, struct kobject, kref);
    
    /* ... cleanup ... */
    
    /* Call type-specific release */
    if (kobj->ktype && kobj->ktype->release)
        kobj->ktype->release(kobj);
}
```

---

## Minimal C Code Simulation

```c
/*
 * KOBJECT REFERENCE COUNTING SIMULATION
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdatomic.h>

/* ==========================================================
 * KERNEL-STYLE DEFINITIONS
 * ========================================================== */

#define container_of(ptr, type, member) ({                      \
    const typeof(((type *)0)->member) *__mptr = (ptr);          \
    (type *)((char *)__mptr - offsetof(type, member));          \
})

/* kref */
struct kref {
    atomic_int refcount;
};

void kref_init(struct kref *kref) {
    atomic_store(&kref->refcount, 1);
}

void kref_get(struct kref *kref) {
    atomic_fetch_add(&kref->refcount, 1);
}

int kref_put(struct kref *kref, void (*release)(struct kref *)) {
    if (atomic_fetch_sub(&kref->refcount, 1) == 1) {
        release(kref);
        return 1;
    }
    return 0;
}

/* ==========================================================
 * KOBJECT STRUCTURES
 * ========================================================== */

struct kobject;

/* Type descriptor - contains release function */
struct kobj_type {
    void (*release)(struct kobject *kobj);
    const char *name;
};

/* kobject - base kernel object */
struct kobject {
    const char *name;
    struct kobject *parent;
    struct kobj_type *ktype;
    struct kref kref;           /* EMBEDDED */
};

/* Forward declaration */
static void kobject_release(struct kref *kref);

/* kobject_init_and_add */
void kobject_init(struct kobject *kobj, struct kobj_type *ktype)
{
    kobj->ktype = ktype;
    kobj->parent = NULL;
    kref_init(&kobj->kref);
    printf("[KOBJECT] Initialized '%s'\n", kobj->name);
}

struct kobject *kobject_get(struct kobject *kobj)
{
    if (kobj) {
        printf("[KOBJECT] get '%s'\n", kobj->name);
        kref_get(&kobj->kref);
    }
    return kobj;
}

void kobject_put(struct kobject *kobj)
{
    if (kobj) {
        printf("[KOBJECT] put '%s'\n", kobj->name);
        kref_put(&kobj->kref, kobject_release);
    }
}

static void kobject_release(struct kref *kref)
{
    struct kobject *kobj = container_of(kref, struct kobject, kref);
    
    printf("[KOBJECT] releasing '%s'\n", kobj->name);
    
    /* Call type-specific release */
    if (kobj->ktype && kobj->ktype->release) {
        kobj->ktype->release(kobj);
    }
}

/* ==========================================================
 * DEVICE BUILT ON KOBJECT
 * ========================================================== */

struct device {
    struct kobject kobj;    /* EMBEDDED - inherits from kobject */
    const char *init_name;
    int device_id;
    void *driver_data;
};

/* Get device from kobject */
#define to_dev(kobj) container_of(kobj, struct device, kobj)

/* Device release - called via kobject chain */
void device_release(struct kobject *kobj)
{
    struct device *dev = to_dev(kobj);
    printf("[DEVICE] Freeing device '%s' (id=%d)\n", 
           dev->init_name, dev->device_id);
    free(dev);
}

static struct kobj_type device_ktype = {
    .release = device_release,
    .name = "device",
};

/* Device get/put - wrapper around kobject */
struct device *device_get(struct device *dev)
{
    if (dev)
        kobject_get(&dev->kobj);
    return dev;
}

void device_put(struct device *dev)
{
    if (dev)
        kobject_put(&dev->kobj);
}

/* Create a device */
struct device *device_create(const char *name, int id)
{
    struct device *dev = malloc(sizeof(*dev));
    if (!dev) return NULL;
    
    dev->init_name = name;
    dev->device_id = id;
    dev->driver_data = NULL;
    dev->kobj.name = name;
    
    kobject_init(&dev->kobj, &device_ktype);
    
    printf("[DEVICE] Created device '%s'\n", name);
    return dev;
}

/* ==========================================================
 * NET_DEVICE BUILT ON DEVICE
 * ========================================================== */

struct net_device {
    struct device dev;      /* EMBEDDED - inherits from device */
    char name[16];
    unsigned char mac[6];
    int flags;
};

/* Get net_device from device */
#define to_net_dev(d) container_of(d, struct net_device, dev)

/* Net device release */
void netdev_release(struct kobject *kobj)
{
    struct device *dev = to_dev(kobj);
    struct net_device *ndev = to_net_dev(dev);
    
    printf("[NETDEV] Freeing net_device '%s'\n", ndev->name);
    free(ndev);
}

static struct kobj_type netdev_ktype = {
    .release = netdev_release,
    .name = "net_device",
};

/* Create net_device */
struct net_device *alloc_netdev(const char *name)
{
    struct net_device *ndev = malloc(sizeof(*ndev));
    if (!ndev) return NULL;
    
    strncpy(ndev->name, name, sizeof(ndev->name) - 1);
    ndev->dev.init_name = ndev->name;
    ndev->dev.device_id = 0;
    ndev->dev.kobj.name = ndev->name;
    ndev->flags = 0;
    
    kobject_init(&ndev->dev.kobj, &netdev_ktype);
    
    printf("[NETDEV] Created net_device '%s'\n", name);
    return ndev;
}

void dev_hold(struct net_device *ndev)
{
    device_get(&ndev->dev);
}

void dev_put(struct net_device *ndev)
{
    device_put(&ndev->dev);
}

/* ==========================================================
 * DEMONSTRATION
 * ========================================================== */

int main(void)
{
    printf("=================================================\n");
    printf("KOBJECT REFERENCE COUNTING DEMONSTRATION\n");
    printf("=================================================\n\n");
    
    /* Create network device (three levels of embedding) */
    printf("--- Creating net_device ---\n");
    struct net_device *eth0 = alloc_netdev("eth0");
    
    /* Show the layered structure */
    printf("\n--- Structure Layout ---\n");
    printf("net_device at:     %p\n", (void *)eth0);
    printf("  ->dev at:        %p\n", (void *)&eth0->dev);
    printf("    ->kobj at:     %p\n", (void *)&eth0->dev.kobj);
    printf("      ->kref at:   %p\n", (void *)&eth0->dev.kobj.kref);
    
    /* Multiple users get references */
    printf("\n--- Multiple users acquire references ---\n");
    printf("Initial refcount: %d\n", 
           atomic_load(&eth0->dev.kobj.kref.refcount));
    
    /* Network subsystem */
    printf("\n[NETWORK] Acquiring reference\n");
    dev_hold(eth0);
    printf("Refcount: %d\n", 
           atomic_load(&eth0->dev.kobj.kref.refcount));
    
    /* Driver */
    printf("\n[DRIVER] Acquiring reference\n");
    dev_hold(eth0);
    printf("Refcount: %d\n", 
           atomic_load(&eth0->dev.kobj.kref.refcount));
    
    /* Sysfs (via kobject) */
    printf("\n[SYSFS] Acquiring reference via kobject\n");
    kobject_get(&eth0->dev.kobj);
    printf("Refcount: %d\n", 
           atomic_load(&eth0->dev.kobj.kref.refcount));
    
    /* Users release */
    printf("\n--- Users release references ---\n");
    
    printf("\n[DRIVER] Releasing\n");
    dev_put(eth0);
    printf("Refcount: %d\n", 
           atomic_load(&eth0->dev.kobj.kref.refcount));
    
    printf("\n[SYSFS] Releasing\n");
    kobject_put(&eth0->dev.kobj);
    printf("Refcount: %d\n", 
           atomic_load(&eth0->dev.kobj.kref.refcount));
    
    printf("\n[NETWORK] Releasing\n");
    dev_put(eth0);
    printf("Refcount: %d\n", 
           atomic_load(&eth0->dev.kobj.kref.refcount));
    
    printf("\n[CREATOR] Final release\n");
    dev_put(eth0);  /* Last reference - object freed */
    
    printf("\n=================================================\n");
    printf("KEY INSIGHTS:\n");
    printf("- kobject embeds kref for reference counting\n");
    printf("- device embeds kobject (inherits refcounting)\n");
    printf("- net_device embeds device (inherits refcounting)\n");
    printf("- All use the same kref, different wrappers\n");
    printf("- Release chain: kref -> kobject -> ktype->release\n");
    printf("=================================================\n");
    
    return 0;
}
```

---

## The Release Chain

```
+=============================================================================+
|              KOBJECT RELEASE CHAIN                                           |
+=============================================================================+

    When last reference is released:

    kobject_put(&kobj)
         |
         v
    kref_put(&kobj->kref, kobject_release)
         |
         v
    [refcount == 0?]
         |
         v
    kobject_release(&kobj->kref)
         |
         v
    kobj = container_of(kref, struct kobject, kref)
         |
         v
    kobj->ktype->release(kobj)
         |
         v
    [Type-specific release, e.g., device_release]
         |
         v
    dev = container_of(kobj, struct device, kobj)
         |
         v
    kfree(dev) or call further release chain
```

**中文说明：**

kobject释放链：当最后一个引用被释放时，`kobject_put`调用`kref_put`，当引用计数为0时调用`kobject_release`，它通过container_of获取kobject，然后调用`kobj->ktype->release(kobj)`，这是类型特定的释放函数，可以通过container_of获取更外层的结构体并释放。

---

## Key Takeaways

1. **kobject wraps kref**: Provides higher-level refcounting API
2. **ktype->release**: Type-specific cleanup when object freed
3. **Multiple embedding levels**: net_device -> device -> kobject -> kref
4. **All wrappers use same kref**: dev_hold/dev_put eventually call kref_get/put
5. **container_of chain**: Each level uses container_of to recover outer type
