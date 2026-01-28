# Core Concept: Object Pool (SLAB Allocator)

What the Object Pool pattern means in kernel architecture for efficient memory allocation.

---

## What Problem Does Object Pool Solve?

```
+=============================================================================+
|                    THE OBJECT POOL PROBLEM                                   |
+=============================================================================+

    THE PROBLEM:
    ============

    Kernel frequently allocates many same-size objects:
    - task_struct for every process
    - inode for every file
    - sk_buff for every network packet
    - dentry for every directory entry

    Using kmalloc for each:
    - Allocation overhead per call
    - Memory fragmentation
    - No object reuse
    - Repeated constructor calls


    KMALLOC ISSUES:
    ===============

    for (i = 0; i < 1000000; i++) {
        obj = kmalloc(sizeof(struct my_obj), GFP_KERNEL);
        /* Use object */
        kfree(obj);
    }

    Problems:
    - 1 million allocations from general heap
    - Memory becomes fragmented
    - No caching of initialized objects
    - Slow!


    SLAB SOLUTION:
    ==============

    cache = kmem_cache_create("my_obj", sizeof(struct my_obj), ...);

    for (i = 0; i < 1000000; i++) {
        obj = kmem_cache_alloc(cache, GFP_KERNEL);
        /* Use object */
        kmem_cache_free(cache, obj);
    }

    Benefits:
    - Objects allocated from dedicated cache
    - No fragmentation (same-size objects)
    - Objects may be partially initialized (constructor)
    - Fast!
```

**Chinese Explanation:**

Object pool solves the problem of frequent same-size allocations. Kernel allocates millions of task_struct, inode, sk_buff objects. Using kmalloc causes allocation overhead, fragmentation, no reuse. SLAB allocator creates dedicated cache for same-size objects - no fragmentation, objects can be pre-initialized, very fast.

---

## How SLAB Allocator Works

```
+=============================================================================+
|                    SLAB ARCHITECTURE                                         |
+=============================================================================+

    CACHE STRUCTURE:
    ================

    kmem_cache "task_struct"
    +-----------------------+
    | object_size           |
    | align                 |
    | ctor (constructor)    |
    | slabs_full ---------->| SLAB (full)
    | slabs_partial ------->| SLAB (partial) --> SLAB
    | slabs_empty --------->| SLAB (empty)
    +-----------------------+


    SLAB STRUCTURE:
    ===============

    Each SLAB contains multiple objects:

    +------------------------------------------+
    | obj | obj | obj | obj | obj | obj | obj  |
    +------------------------------------------+
       ^                   ^
    allocated            free


    ALLOCATION PATH:
    ================

    kmem_cache_alloc(cache)
        |
        v
    Check slabs_partial
        |
    +---+---+
    |       |
   Has     Empty
  object    |
    |       v
    v    Get from slabs_empty or allocate new slab
  Return     |
  object     v
          Add to slabs_partial
              |
              v
          Return object


    FREE PATH:
    ==========

    kmem_cache_free(cache, obj)
        |
        v
    Find slab containing obj
        |
        v
    Mark object as free
        |
        v
    Update slab state (full -> partial -> empty)
```

**Chinese Explanation:**

SLAB architecture: Cache contains multiple slabs, each slab contains multiple same-size objects. Allocation from partial slabs first (fast), then empty slabs, or allocate new slab. Free returns object to slab, slab moves between full/partial/empty lists.

---

## SLAB API

```c
/* Create cache for objects */
struct kmem_cache *kmem_cache_create(
    const char *name,        /* Cache name (for /proc/slabinfo) */
    size_t size,             /* Object size */
    size_t align,            /* Alignment requirement */
    unsigned long flags,     /* SLAB flags */
    void (*ctor)(void *)     /* Constructor (optional) */
);

/* Allocate object from cache */
void *kmem_cache_alloc(struct kmem_cache *cache, gfp_t flags);

/* Free object to cache */
void kmem_cache_free(struct kmem_cache *cache, void *obj);

/* Destroy cache */
void kmem_cache_destroy(struct kmem_cache *cache);
```

---

## Why Kernel Uses Object Pool

```
    1. PERFORMANCE
       Objects allocated from dedicated pool
       Much faster than general allocator
    
    2. NO FRAGMENTATION
       Same-size objects pack perfectly
       No wasted space between objects
    
    3. CACHING
       Freed objects stay in cache
       Next allocation is nearly instant
    
    4. CONSTRUCTOR
       Objects pre-initialized once
       Subsequent allocations skip init
    
    5. DEBUGGING
       Per-cache statistics
       Red zones, poisoning
```

---

## Object Pool vs Factory

```
    FACTORY:                        OBJECT POOL:
    ========                        ============
    
    Creates new object              Returns pooled object
    Always allocates                May reuse memory
    One-time creation               Repeated alloc/free
    General purpose                 High-frequency allocation
```

---

## Version

Based on **Linux kernel v3.2** SLAB allocator.
