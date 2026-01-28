# Case 1: kref - The Kernel Reference Counter

## Subsystem Background

```
+=============================================================================+
|                    KREF: KERNEL REFERENCE COUNTER                            |
+=============================================================================+

    WHAT IS KREF:
    =============

    struct kref {
        atomic_t refcount;
    };

    - Simple wrapper around atomic counter
    - Provides standard get/put/init API
    - Used by kobject, many drivers
    - Designed to be embedded in user structures


    WHY KREF EXISTS:
    ================

    Before kref, every subsystem had its own refcount:
    - Different naming (count, refcnt, ref, users)
    - Different semantics
    - Code duplication
    - Bugs in custom implementations

    kref provides:
    - Standard pattern
    - Correct atomic semantics
    - Built-in release callback mechanism
    - Type checking via container_of
```

**中文说明：**

kref是内核的标准引用计数器。它是atomic_t的简单封装，提供标准的get/put/init API。在kref之前，每个子系统有自己的引用计数实现，命名不同、语义不同、代码重复、容易出错。kref提供了标准模式、正确的原子语义、内置释放回调机制、通过container_of实现类型检查。

---

## kref API

```c
/* include/linux/kref.h */

/**
 * kref_init - initialize a kref
 * @kref: object to initialize
 *
 * Sets the refcount to 1.
 */
void kref_init(struct kref *kref)
{
    atomic_set(&kref->refcount, 1);
}

/**
 * kref_get - increment refcount
 * @kref: object to increment
 *
 * Caller must already hold a reference (refcount must be > 0).
 */
void kref_get(struct kref *kref)
{
    WARN_ON(!atomic_read(&kref->refcount));
    atomic_inc(&kref->refcount);
}

/**
 * kref_put - decrement refcount and call release if zero
 * @kref: object to decrement
 * @release: function to call when refcount reaches 0
 *
 * Returns 1 if object was released, 0 otherwise.
 */
int kref_put(struct kref *kref, void (*release)(struct kref *kref))
{
    WARN_ON(release == NULL);
    if (atomic_dec_and_test(&kref->refcount)) {
        release(kref);
        return 1;
    }
    return 0;
}
```

---

## Minimal C Code Simulation

```c
/*
 * KREF PATTERN SIMULATION
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdatomic.h>

/* ==========================================================
 * KERNEL-STYLE DEFINITIONS
 * ========================================================== */

#define container_of(ptr, type, member) ({                      \
    const typeof(((type *)0)->member) *__mptr = (ptr);          \
    (type *)((char *)__mptr - offsetof(type, member));          \
})

/* Simplified kref */
struct kref {
    atomic_int refcount;
};

static inline void kref_init(struct kref *kref)
{
    atomic_store(&kref->refcount, 1);
    printf("[KREF] init: refcount = 1\n");
}

static inline void kref_get(struct kref *kref)
{
    int old = atomic_fetch_add(&kref->refcount, 1);
    printf("[KREF] get: refcount %d -> %d\n", old, old + 1);
    if (old <= 0) {
        printf("[KREF] WARNING: kref_get on zero refcount!\n");
    }
}

static inline int kref_put(struct kref *kref, 
                           void (*release)(struct kref *kref))
{
    int old = atomic_fetch_sub(&kref->refcount, 1);
    printf("[KREF] put: refcount %d -> %d\n", old, old - 1);
    
    if (old == 1) {
        /* Was 1, now 0 - time to release */
        printf("[KREF] refcount hit 0, calling release\n");
        release(kref);
        return 1;
    }
    return 0;
}

static inline int kref_read(struct kref *kref)
{
    return atomic_load(&kref->refcount);
}

/* ==========================================================
 * USER STRUCTURE
 * ========================================================== */

struct my_device {
    char name[32];
    int id;
    void *private_data;
    struct kref ref;    /* EMBEDDED kref */
};

/* Release function - uses container_of */
void my_device_release(struct kref *ref)
{
    struct my_device *dev = container_of(ref, struct my_device, ref);
    
    printf("[RELEASE] Freeing device '%s' (id=%d)\n", dev->name, dev->id);
    
    /* Clean up resources */
    if (dev->private_data) {
        free(dev->private_data);
    }
    
    /* Free the device itself */
    free(dev);
}

/* ==========================================================
 * DEVICE OPERATIONS
 * ========================================================== */

struct my_device *create_device(const char *name, int id)
{
    struct my_device *dev = malloc(sizeof(*dev));
    if (!dev) return NULL;
    
    snprintf(dev->name, sizeof(dev->name), "%s", name);
    dev->id = id;
    dev->private_data = malloc(1024);  /* Some resource */
    
    /* Initialize refcount to 1 */
    kref_init(&dev->ref);
    
    printf("[CREATE] Device '%s' created\n", name);
    return dev;
}

struct my_device *get_device(struct my_device *dev)
{
    if (dev) {
        kref_get(&dev->ref);
    }
    return dev;
}

void put_device(struct my_device *dev)
{
    if (dev) {
        kref_put(&dev->ref, my_device_release);
    }
}

/* ==========================================================
 * SIMULATED USERS
 * ========================================================== */

/* User A: Creates and uses device */
void user_a_work(struct my_device *dev)
{
    printf("\n[USER_A] Using device '%s'\n", dev->name);
    /* Do some work */
    printf("[USER_A] Done, releasing reference\n");
    put_device(dev);
}

/* User B: Gets reference and uses device */
void user_b_work(struct my_device *dev)
{
    printf("\n[USER_B] Getting reference to '%s'\n", dev->name);
    get_device(dev);
    
    printf("[USER_B] Using device\n");
    /* Do some work */
    
    printf("[USER_B] Done, releasing reference\n");
    put_device(dev);
}

/* User C: Gets reference and uses device */
void user_c_work(struct my_device *dev)
{
    printf("\n[USER_C] Getting reference to '%s'\n", dev->name);
    get_device(dev);
    
    printf("[USER_C] Using device\n");
    /* Do some work */
    
    printf("[USER_C] Done, releasing reference\n");
    put_device(dev);
}

/* ==========================================================
 * DEMONSTRATION
 * ========================================================== */

int main(void)
{
    printf("=================================================\n");
    printf("KREF REFERENCE COUNTING DEMONSTRATION\n");
    printf("=================================================\n\n");
    
    /* Create device - refcount = 1 */
    printf("--- Creating device ---\n");
    struct my_device *dev = create_device("eth0", 1);
    
    printf("\nCurrent refcount: %d\n", kref_read(&dev->ref));
    
    /* Users B and C get references */
    printf("\n--- Users B and C get references ---\n");
    get_device(dev);  /* For user B: refcount = 2 */
    get_device(dev);  /* For user C: refcount = 3 */
    
    printf("\nCurrent refcount: %d\n", kref_read(&dev->ref));
    
    /* User A finishes (creator) */
    printf("\n--- User A finishes ---\n");
    put_device(dev);  /* refcount = 2, NOT freed */
    
    printf("\nCurrent refcount: %d\n", kref_read(&dev->ref));
    
    /* User B finishes */
    printf("\n--- User B finishes ---\n");
    put_device(dev);  /* refcount = 1, NOT freed */
    
    printf("\nCurrent refcount: %d\n", kref_read(&dev->ref));
    
    /* User C finishes - LAST USER */
    printf("\n--- User C finishes (last user) ---\n");
    put_device(dev);  /* refcount = 0, FREED! */
    
    /* dev is now invalid - don't access it! */
    
    printf("\n=================================================\n");
    printf("KEY INSIGHTS:\n");
    printf("- kref_init sets count to 1 (creator has reference)\n");
    printf("- kref_get increments (new user)\n");
    printf("- kref_put decrements (user done)\n");
    printf("- When count hits 0, release callback is invoked\n");
    printf("- container_of recovers object in release callback\n");
    printf("=================================================\n");
    
    return 0;
}
```

---

## What kref Enables

```
+=============================================================================+
|              WHAT KREF ENABLES                                               |
+=============================================================================+

    1. SAFE SHARED OWNERSHIP
       =====================
       Multiple subsystems can hold references to same object.
       Object lives until ALL users are done.

    2. DETERMINISTIC DESTRUCTION
       =========================
       Object freed immediately when last reference released.
       No garbage collection delay.
       Resources (files, memory, devices) cleaned up promptly.

    3. AUTOMATIC RELEASE
       ==================
       kref_put automatically calls release callback.
       No manual "if count == 0 then free" everywhere.

    4. TYPE-SAFE RELEASE
       ==================
       Release callback uses container_of.
       Compiler-checked type recovery.
```

**中文说明：**

kref实现了什么：(1) 安全的共享所有权——多个子系统可以持有同一对象的引用；(2) 确定性销毁——最后一个引用释放时立即释放对象；(3) 自动释放——kref_put自动调用释放回调；(4) 类型安全的释放——释放回调使用container_of进行编译器检查的类型恢复。

---

## Real Kernel Examples

### kobject (lib/kobject.c)

```c
struct kobject {
    /* ... */
    struct kref kref;
    /* ... */
};

struct kobject *kobject_get(struct kobject *kobj)
{
    if (kobj)
        kref_get(&kobj->kref);
    return kobj;
}

void kobject_put(struct kobject *kobj)
{
    if (kobj)
        kref_put(&kobj->kref, kobject_release);
}

static void kobject_release(struct kref *kref)
{
    struct kobject *kobj = container_of(kref, struct kobject, kref);
    /* ... cleanup ... */
    kobj->ktype->release(kobj);
}
```

### USB Device (drivers/usb/core/*)

```c
struct usb_device {
    /* ... */
    struct kref kref;
    /* ... */
};

struct usb_device *usb_get_dev(struct usb_device *dev)
{
    if (dev)
        kref_get(&dev->kref);
    return dev;
}

void usb_put_dev(struct usb_device *dev)
{
    if (dev)
        kref_put(&dev->kref, usb_release_dev);
}
```

---

## Key Takeaways

1. **kref is embedded**: Part of the user structure, not separate
2. **container_of in release**: Recovers object from kref pointer
3. **Atomic operations**: Thread-safe increment/decrement
4. **Standard pattern**: Consistent API across kernel
5. **Init to 1**: Creator has first reference
