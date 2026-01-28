# Core Concept: Factory Pattern

What Factory pattern means in kernel architecture for consistent object creation.

---

## What Problem Does Factory Solve?

```
+=============================================================================+
|                    THE FACTORY PROBLEM                                       |
+=============================================================================+

    THE PROBLEM:
    ============

    Creating complex kernel objects requires:
    - Memory allocation (kmalloc, alloc_pages, etc.)
    - Field initialization
    - Reference count setup
    - Linking to subsystems
    - Error handling

    If callers do this manually:
    - Inconsistent initialization
    - Missing steps
    - Code duplication
    - Hard to maintain


    BAD: Manual Creation
    ====================

    struct net_device *dev;
    
    dev = kmalloc(sizeof(*dev) + priv_size, GFP_KERNEL);
    if (!dev)
        return NULL;
    memset(dev, 0, sizeof(*dev) + priv_size);
    dev->name[0] = 0;
    dev->reg_state = NETREG_UNINITIALIZED;
    INIT_LIST_HEAD(&dev->napi_list);
    /* ... 50 more initialization steps ... */
    /* Easy to miss something! */


    GOOD: Factory Function
    ======================

    struct net_device *dev;
    
    dev = alloc_netdev(priv_size, "eth%d", ether_setup);
    if (!dev)
        return NULL;
    /* Everything properly initialized */
```

**Chinese Explanation:**

Factory pattern solves the problem of complex kernel object creation. Creating objects requires memory allocation, field initialization, reference counting, linking to subsystems. Manual creation leads to inconsistent initialization, missing steps, code duplication. Factory functions encapsulate all initialization steps to ensure consistency.

---

## How Factory Works

```
+=============================================================================+
|                    FACTORY MECHANISM                                         |
+=============================================================================+

    FACTORY FUNCTION STRUCTURE:
    ===========================

    struct my_obj *alloc_my_obj(params)
    {
        struct my_obj *obj;

        /* 1. Allocate memory */
        obj = kmalloc(sizeof(*obj), GFP_KERNEL);
        if (!obj)
            return NULL;

        /* 2. Initialize all fields */
        memset(obj, 0, sizeof(*obj));
        obj->field1 = default_value1;
        obj->field2 = default_value2;
        INIT_LIST_HEAD(&obj->list);

        /* 3. Set up reference counting */
        kref_init(&obj->kref);

        /* 4. Additional setup */
        setup_subsystem_links(obj);

        /* 5. Return ready-to-use object */
        return obj;
    }


    CALLER VIEW:
    ============

    Before Factory:
    ---------------
    obj = kmalloc();
    init_field1();
    init_field2();
    init_field3();

    After Factory:
    --------------
    obj = alloc_my_obj(params);
    /* Done! */
```

**Chinese Explanation:**

Factory mechanism: Factory function encapsulates 5 steps - allocate memory, initialize all fields, set up reference counting, additional setup, return ready object. Caller only needs one line to get a fully initialized object.

---

## Factory Components

```
    TYPICAL FACTORY PATTERN:
    ========================

    1. ALLOCATION FUNCTION
       alloc_xxx() - creates and initializes
    
    2. FREE FUNCTION
       free_xxx() - releases resources
    
    3. SETUP CALLBACK (optional)
       void (*setup)(struct xxx *)
       Allows customization during creation


    NAMING CONVENTIONS:
    ===================

    alloc_netdev()     / free_netdev()
    alloc_skb()        / kfree_skb()
    alloc_disk()       / put_disk()
    kmem_cache_alloc() / kmem_cache_free()
    
    Pattern: alloc_* / free_* or *_alloc / *_free
```

---

## Why Kernel Uses Factory

```
    1. CONSISTENCY
       Every object created the same way
       No missing initialization steps
    
    2. ENCAPSULATION
       Internal structure hidden
       Changes do not affect callers
    
    3. ERROR HANDLING
       Factory handles allocation failures
       Cleanup on partial failure
    
    4. SUBSYSTEM INTEGRATION
       Factory knows how to link to subsystems
       Caller does not need subsystem knowledge
    
    5. DEBUGGING
       Single place to add debug code
       Easy to track object creation
```

---

## Factory vs Direct Allocation

```
    DIRECT ALLOCATION:              FACTORY:
    ==================              ========
    
    kmalloc + manual init           Single function call
    Caller knows structure          Structure encapsulated
    Error-prone                     Reliable
    Code duplication                Single implementation
    Hard to change                  Easy to modify
```

---

## Version

Based on **Linux kernel v3.2** factory patterns.
