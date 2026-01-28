# Final Mental Model: Reference Counting Pattern

## One-Paragraph Summary

Reference counting is the kernel's mechanism for managing shared object lifetimes. Each object maintains a counter of active users: `kref_init()` sets it to 1 when created, `kref_get()` increments when a new user acquires access, and `kref_put()` decrements when a user is done. When the counter reaches zero, a release callback (found via `container_of`) frees the object and its resources. This provides deterministic cleanup (immediately when last user releases), works for any resource type (not just memory), and requires no garbage collector. The key contract: hold a reference while using an object, release when done, never access after release.

**中文总结：**

引用计数是内核管理共享对象生命周期的机制。每个对象维护一个活跃用户计数器：`kref_init()`在创建时设为1，`kref_get()`在新用户获取访问时增加，`kref_put()`在用户使用完时减少。当计数器变为0时，通过`container_of`找到的释放回调函数释放对象及其资源。这提供了确定性清理（最后一个用户释放时立即执行）、适用于任何资源类型（不仅仅是内存）、不需要垃圾回收器。核心约定：使用对象时持有引用，用完释放，释放后绝不访问。

---

## Decision Flowchart

```
+=============================================================================+
|              REFERENCE COUNTING DECISION FLOWCHART                           |
+=============================================================================+

    START: Do I need reference counting?
    ====================================

                    +---------------------+
                    | Is object shared    |
                    | by multiple users?  |
                    +----------+----------+
                               |
              +----------------+----------------+
              |                                 |
              v                                 v
            [YES]                             [NO]
              |                                 |
              v                                 v
    +-------------------+              Single owner can
    | Use reference     |              free when done
    | counting          |              (no refcount needed)
    +--------+----------+
             |
             v
    +-------------------+
    | Embed kref or     |
    | atomic counter    |
    +--------+----------+
             |
             v
    +-------------------+
    | Implement:        |
    | - xxx_create()    |  Initialize refcount to 1
    | - xxx_get()       |  Increment refcount
    | - xxx_put()       |  Decrement, release if 0
    | - xxx_release()   |  Cleanup and free
    +-------------------+


    WHEN TO GET:
    ============

    Creating new object?
        --> No get needed, init gives you count=1
    
    Receiving from function return?
        --> Function should have done get; you must put
    
    Sharing with another user/thread?
        --> Get before sharing, they must put
    
    Storing in data structure?
        --> Get when adding, put when removing


    WHEN TO PUT:
    ============

    Done using object?
        --> Put and never access again
    
    Error path in function?
        --> Put if you did get
    
    Removing from data structure?
        --> Put after removal
```

---

## The Three Laws

```
+=============================================================================+
|              THREE LAWS OF REFERENCE COUNTING                                |
+=============================================================================+

    LAW 1: BALANCE
    ==============
    
    Every get must have a matching put.
    No exceptions.
    
        get();
        use();
        put();
    
    
    LAW 2: VALIDITY
    ===============
    
    Only access object while you hold a reference.
    
        get();       /* Now valid */
        use();       /* OK */
        put();       /* No longer valid for you */
        use();       /* BUG! */
    
    
    LAW 3: ATOMICITY
    ================
    
    Counter operations must be atomic.
    
        atomic_inc()           /* Thread-safe get */
        atomic_dec_and_test()  /* Thread-safe put + check */
```

---

## Visual Mental Model

```
+=============================================================================+
|              REFERENCE COUNTING LIFECYCLE                                    |
+=============================================================================+

    OBJECT CREATION:
    ================
    
    create()  -->  [ Object ]  refcount = 1
                       ^
                       |
                   Creator holds reference


    SHARING:
    ========
    
    get()     -->  [ Object ]  refcount = 2
                       ^
                      / \
                     /   \
              Creator    User B
              holds      holds
              reference  reference


    RELEASE (not last):
    ===================
    
    put()     -->  [ Object ]  refcount = 1
                       ^
                       |
                   Only User B remains


    FINAL RELEASE:
    ==============
    
    put()     -->  [ Object ]  refcount = 0
                       |
                       v
               release() called
                       |
                       v
                  Object freed
                       |
                       v
                   [   X   ]  Object gone


    MEMORY VIEW:
    ============

    +------------------+
    | struct my_object |
    |  .id             |
    |  .name           |
    |  .ref.refcount --+--> 2 (atomic integer)
    |  .data           |
    +------------------+
    
    After all puts:
    
    +------------------+
    |   [FREED]        |  <-- Memory returned to allocator
    +------------------+
```

---

## Quick Reference Card

```
+=============================================================================+
|              REFERENCE COUNTING QUICK REFERENCE                              |
+=============================================================================+

    STRUCTURE:
    ----------
    struct my_object {
        /* data */
        struct kref ref;    /* EMBEDDED counter */
    };


    OPERATIONS:
    -----------
    kref_init(&obj->ref)              Set count to 1
    kref_get(&obj->ref)               Increment count
    kref_put(&obj->ref, release)      Decrement; release if 0


    RELEASE FUNCTION:
    -----------------
    void release(struct kref *ref) {
        struct my_object *obj = container_of(ref, struct my_object, ref);
        /* cleanup */
        kfree(obj);
    }


    COMMON WRAPPER PATTERN:
    -----------------------
    struct my_object *my_object_get(struct my_object *obj) {
        if (obj) kref_get(&obj->ref);
        return obj;
    }
    
    void my_object_put(struct my_object *obj) {
        if (obj) kref_put(&obj->ref, my_object_release);
    }


    LOOKUP PATTERN:
    ---------------
    struct my_object *lookup(int id) {
        obj = find(id);
        if (obj) kref_get(&obj->ref);  /* Get before return! */
        return obj;
    }
```

---

## Common Patterns Summary

| Operation | What to Do |
|-----------|------------|
| **Create object** | `kref_init()` - count starts at 1 |
| **Share object** | `kref_get()` - new user gets reference |
| **Done using** | `kref_put()` - always, no exceptions |
| **Return from lookup** | `kref_get()` before return |
| **Add to list** | `kref_get()` when adding |
| **Remove from list** | `kref_put()` after removal |
| **Release callback** | `container_of()` + cleanup + free |

---

## Final Checklist

Before releasing a reference:

- [ ] Have I completed all access to this object?
- [ ] Will I never access this object again?
- [ ] Have I accounted for all references I acquired?

Before acquiring a reference:

- [ ] Do I already have a valid reference (or trusted source)?
- [ ] Will I eventually release this reference?
- [ ] Is the object guaranteed to exist right now?

If all yes, proceed safely with reference counting.
