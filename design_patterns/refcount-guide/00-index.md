# Reference Counting Pattern in Linux Kernel (v3.2)

Object lifecycle management through explicit reference tracking.

## Table of Contents

| File | Topic |
|------|-------|
| [01-core-concept.md](01-core-concept.md) | Core Concept: Object Lifecycle Management |
| [02-identification-rules.md](02-identification-rules.md) | Identification Rules |
| [03-kref-case.md](03-kref-case.md) | Case 1: kref - The Kernel Reference Counter |
| [04-kobject-case.md](04-kobject-case.md) | Case 2: kobject Reference Counting |
| [05-file-case.md](05-file-case.md) | Case 3: struct file Reference Counting |
| [06-module-case.md](06-module-case.md) | Case 4: Module Reference Counting |
| [07-unified-skeleton.md](07-unified-skeleton.md) | Unified Skeleton |
| [08-vs-garbage-collection.md](08-vs-garbage-collection.md) | Reference Counting vs GC |
| [09-antipatterns.md](09-antipatterns.md) | Anti-Patterns |
| [10-reading-guide.md](10-reading-guide.md) | Source Reading Guide |
| [11-mental-model.md](11-mental-model.md) | Final Mental Model |

---

## Overview Diagram

```
+=============================================================================+
|                    REFERENCE COUNTING PATTERN                                |
+=============================================================================+

    THE PROBLEM:
    ============

    Object Creation                  Object Destruction
    ===============                  ==================
    
    A creates object                 When to free?
         |                                |
         v                                v
    B gets reference                 B still using? --> Can't free!
    C gets reference                 C still using? --> Can't free!
    D gets reference                 All done?      --> Safe to free

    WITHOUT REFCOUNT: Race conditions, use-after-free, double-free


    THE SOLUTION:
    =============

    struct my_object {
        struct kref ref;      /* Reference counter */
        /* ... data ... */
    };

    Operations:
    -----------
    kref_init(&obj->ref);     /* Set count to 1 */
    kref_get(&obj->ref);      /* Increment: "I'm using this" */
    kref_put(&obj->ref, release); /* Decrement: "I'm done" */
                                  /* If count hits 0, call release */


    LIFECYCLE:
    ==========

         kref_init()      kref_get()      kref_get()      kref_put()
              |               |               |               |
              v               v               v               v
    refcount: 1       ->      2       ->      3       ->      2
                                                              |
                                              kref_put()      |
                                                   |          |
                                                   v          v
                                              refcount:       1
                                                              |
                                              kref_put()      |
                                                   |          |
                                                   v          v
                                              refcount:       0  --> release()
```

**中文说明：**

引用计数模式解决的问题：当多个使用者共享一个对象时，何时释放对象？没有引用计数会导致竞态条件、释放后使用、双重释放等问题。解决方案：每个对象维护一个计数器，获取引用时增加，释放引用时减少，当计数降为0时调用释放函数。内核使用`kref`结构体实现此模式。

---

## Why Reference Counting

### The Fundamental Problem

```
    SHARED OWNERSHIP:
    =================

    In kernel, objects are often shared:
    - A file can be opened by multiple processes
    - A device can be used by multiple drivers
    - A network socket can be accessed by multiple threads

    Questions:
    1. WHO is responsible for freeing the object?
    2. WHEN is it safe to free?
    3. HOW to prevent use-after-free?

    Answer: Reference Counting
    - EVERYONE who uses the object takes a reference
    - LAST user (count -> 0) frees it
    - ATOMIC operations prevent races
```

### Alternative: Garbage Collection

```
    GARBAGE COLLECTION (Java/Go):        REFERENCE COUNTING (Kernel):
    =============================        ===========================

    - Runtime traces all objects         - Manual inc/dec calls
    - Automatic memory reclamation       - Explicit resource release
    - Non-deterministic timing           - Deterministic: 0 = now
    - Works for memory only              - Works for any resource
    - CPU overhead for tracing           - CPU overhead for atomics

    KERNEL CHOICE: Reference Counting
    - Deterministic destruction
    - Works for non-memory resources (files, devices)
    - No GC runtime needed
    - Explicit control
```

**中文说明：**

为什么选择引用计数而不是垃圾回收：垃圾回收有运行时开销、非确定性时机、只能处理内存。引用计数是确定性的（计数为0时立即释放）、可处理任何资源（文件、设备）、无需GC运行时、控制更明确。内核需要确定性和对非内存资源的控制，因此选择引用计数。

---

## Key Terminology

| Term | Meaning |
|------|---------|
| **Reference** | A usage claim on an object |
| **refcount** | The counter tracking active references |
| **get/inc** | Acquire a reference (increment count) |
| **put/dec** | Release a reference (decrement count) |
| **release** | Callback invoked when count reaches 0 |
| **kref** | Kernel's standard reference counting structure |

---

## The kref Structure

```c
/* include/linux/kref.h */

struct kref {
    atomic_t refcount;
};

/* Initialize to 1 */
void kref_init(struct kref *kref);

/* Increment (get a reference) */
void kref_get(struct kref *kref);

/* Decrement (release a reference) */
/* Calls release() if count hits 0 */
/* Returns 1 if released, 0 otherwise */
int kref_put(struct kref *kref, void (*release)(struct kref *kref));
```

---

## Connection to container_of

```
    kref IS EMBEDDED, container_of RECOVERS OBJECT:
    ===============================================

    struct my_device {
        int id;
        char name[32];
        struct kref ref;     /* EMBEDDED */
    };

    void my_release(struct kref *ref)
    {
        /* Use container_of to get the object */
        struct my_device *dev = container_of(ref, struct my_device, ref);
        
        /* Now free the object */
        kfree(dev);
    }

    /* Decrement reference */
    kref_put(&dev->ref, my_release);
```

---

## Version

This guide targets **Linux kernel v3.2**.
