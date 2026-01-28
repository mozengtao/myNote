# Core Concept: Object Lifecycle Management

## The Ownership Problem

```
+=============================================================================+
|                    THE OBJECT OWNERSHIP PROBLEM                              |
+=============================================================================+

    SCENARIO: Shared Object
    =======================

    Process A opens file "/tmp/data"    -->  struct file created
    Process B opens same file           -->  Same struct file? New one?
    Process A closes file               -->  Free struct file?
    Process B still using file!         -->  USE-AFTER-FREE!


    WITHOUT REFERENCE COUNTING:
    ===========================

    struct file *f = kmalloc(sizeof(*f));
    
    process_a_uses(f);
    process_b_uses(f);
    
    process_a_done(f);
    kfree(f);              /* Who decides this? */
    
    process_b_access(f);   /* CRASH! Use-after-free */


    WITH REFERENCE COUNTING:
    ========================

    struct file *f = kmalloc(sizeof(*f));
    atomic_set(&f->f_count, 1);    /* Initial reference */
    
    get_file(f);                   /* Process B: f_count = 2 */
    
    fput(f);                       /* Process A done: f_count = 1 */
                                   /* NOT freed - still in use */
    
    fput(f);                       /* Process B done: f_count = 0 */
                                   /* NOW freed - no users */
```

**中文说明：**

对象所有权问题：当多个进程共享对象时，谁负责释放？没有引用计数时，释放由单一所有者决定，但其他使用者可能还在使用，导致释放后使用。引用计数解决方案：每个使用者增加计数，释放时减少计数，只有最后一个使用者（计数变为0）才真正释放对象。

---

## Reference Counting Semantics

```
    REFERENCE = "I AM USING THIS OBJECT"
    ====================================

    When you get a reference:
    - You promise to release it when done
    - Object won't be freed while you hold it
    - You can safely access the object

    When you release a reference:
    - You promise not to access object anymore
    - If you were last user, object is freed
    - Any subsequent access is a BUG


    THE CONTRACT:
    =============

    +----------------+------------------------------------------+
    | Operation      | Meaning                                  |
    +----------------+------------------------------------------+
    | kref_init()    | Object created, creator has 1 reference  |
    | kref_get()     | "I want to use this object"              |
    | kref_put()     | "I'm done using this object"             |
    | release()      | "Object no longer needed by anyone"      |
    +----------------+------------------------------------------+


    INVARIANT:
    ==========

    At any time:  refcount = number of active users
    
    refcount > 0  -->  Object is valid, can be used
    refcount = 0  -->  Object is being/has been freed
    
    NEVER access object after your kref_put() returns!
```

**中文说明：**

引用计数语义：获取引用意味着"我在使用这个对象"，承诺用完后释放；释放引用意味着"我不再使用这个对象"，承诺不再访问。不变量：refcount始终等于活跃使用者数量。refcount>0时对象有效，=0时对象正在被或已经被释放。kref_put返回后绝不能再访问对象！

---

## Atomic Operations

```
+=============================================================================+
|                    WHY ATOMIC?                                               |
+=============================================================================+

    RACE CONDITION WITHOUT ATOMIC:
    ==============================

    CPU A                          CPU B
    -----                          -----
    read refcount (=1)             read refcount (=1)
    decrement (=0)                 decrement (=0)
    if 0, free                     if 0, free
         |                              |
         v                              v
    FREE!                          FREE!
    
    --> DOUBLE FREE!


    WITH ATOMIC OPERATIONS:
    =======================

    kref_put() uses atomic_dec_and_test():
    
    CPU A                          CPU B
    -----                          -----
    atomic_dec_and_test()          [blocked]
    refcount: 1 -> 0               
    returns TRUE                   atomic_dec_and_test()
    free()                         [sees refcount = -1, kernel panic
                                    OR properly serialized]

    Actually, proper implementation:
    
    CPU A                          CPU B
    -----                          -----
    atomic_dec_and_test()          atomic_dec_and_test()
    refcount: 2 -> 1               [waits for atomic to complete]
    returns FALSE                  refcount: 1 -> 0
    (don't free)                   returns TRUE
                                   free()
```

**中文说明：**

为什么需要原子操作：如果不用原子操作，两个CPU可能同时读取相同的计数值（如1），都认为自己将其减到0，都尝试释放，导致双重释放。原子操作确保读取-修改-写入是不可分割的，只有一个CPU能将计数减到0。

---

## The Release Function

```c
/*
 * RELEASE FUNCTION PATTERN
 * ========================
 */

struct my_object {
    int data;
    struct kref ref;
};

/*
 * Release function is called when refcount hits 0.
 * It must:
 * 1. Use container_of to get the actual object
 * 2. Clean up any resources (close files, unmap memory)
 * 3. Free the object itself
 */
void my_release(struct kref *ref)
{
    struct my_object *obj = container_of(ref, struct my_object, ref);
    
    /* Step 1: Clean up resources */
    cleanup_resources(obj);
    
    /* Step 2: Free the object */
    kfree(obj);
    
    /* After this, obj pointer is INVALID */
}

/* Usage */
void done_with_object(struct my_object *obj)
{
    /*
     * After kref_put returns:
     * - If it returned 1: obj has been freed, DON'T ACCESS IT
     * - If it returned 0: obj still valid (others have references)
     *                     but YOUR reference is gone, don't use obj
     * 
     * SAFEST: Never access obj after kref_put, regardless of return
     */
    kref_put(&obj->ref, my_release);
    
    /* obj may be freed here - don't touch it! */
}
```

---

## Reference Counting Rules

```
+=============================================================================+
|                    REFERENCE COUNTING RULES                                  |
+=============================================================================+

    RULE 1: BALANCED GET/PUT
    ========================
    
    Every kref_get() must have a matching kref_put()
    
        kref_get(&obj->ref);
        use_object(obj);
        kref_put(&obj->ref, release);
    
    If you get reference from:
        - Creating object: Initial count is 1 (no explicit get)
        - Receiving from someone: They did get for you
        - Looking up object: Lookup function does get


    RULE 2: DON'T ACCESS AFTER PUT
    ==============================
    
    After kref_put(), treat your pointer as invalid:
    
        kref_put(&obj->ref, release);
        obj->data = 5;    /* BUG! obj may be freed */


    RULE 3: DOCUMENT REFERENCE TRANSFERS
    ====================================
    
    When passing object to another function/thread:
    
    /* Option A: Caller keeps reference, callee must get its own */
    void process(struct my_obj *obj) {
        kref_get(&obj->ref);   /* Callee gets reference */
        /* use obj */
        kref_put(&obj->ref);   /* Callee releases */
    }
    
    /* Option B: Caller transfers reference to callee */
    /* Caller must NOT access obj after call */
    void process_and_release(struct my_obj *obj) {
        /* use obj */
        kref_put(&obj->ref);   /* Callee releases caller's ref */
    }


    RULE 4: BEWARE OF CACHES AND LOOKUPS
    ====================================
    
    If object is in a lookup structure (hash, list):
    - Lookup must increment refcount before returning
    - Prevents object from being freed while caller uses it
    
        struct my_obj *lookup(int id) {
            struct my_obj *obj = find_in_hash(id);
            if (obj)
                kref_get(&obj->ref);  /* Increment before return */
            return obj;
        }
```

**中文说明：**

引用计数规则：(1) get/put必须平衡——每个kref_get必须有对应的kref_put；(2) put之后不要访问——kref_put后指针视为无效；(3) 记录引用转移——传递对象时明确谁负责引用；(4) 注意缓存和查找——从查找结构返回对象前必须增加引用计数。

---

## Connection to container_of

```
    EMBEDDED KREF + CONTAINER_OF:
    ============================

    struct my_device {
        char name[32];
        int id;
        void *data;
        struct kref ref;     /* EMBEDDED */
    };

    void release(struct kref *ref)
    {
        /*
         * release() receives kref pointer.
         * Need to get back to my_device to free it.
         * container_of does this!
         */
        struct my_device *dev = container_of(ref, struct my_device, ref);
        
        kfree(dev);
    }

    This is why kref is EMBEDDED, not a pointer.
    container_of enables type recovery in release callback.
```

---

## Summary

Reference counting provides:

1. **Deterministic cleanup**: Object freed exactly when last reference released
2. **Safe sharing**: Multiple users can share object safely
3. **No GC overhead**: Simple atomic increment/decrement
4. **Works for any resource**: Not just memory (files, devices, sockets)
5. **Explicit ownership**: Clear contract about who owns what
