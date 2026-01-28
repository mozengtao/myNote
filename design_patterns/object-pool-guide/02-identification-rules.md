# Identification Rules: Object Pool (SLAB)

Five concrete rules to identify Object Pool pattern in kernel code.

---

## Rule 1: Look for kmem_cache Variables

```c
/* Cache variable declaration */
static struct kmem_cache *task_struct_cachep;
static struct kmem_cache *inode_cachep;
static struct kmem_cache *dentry_cache;

/* SIGNAL: struct kmem_cache * variable */
```

**Chinese Explanation:**

Rule 1: Look for kmem_cache variables - static struct kmem_cache * indicates object pool.

---

## Rule 2: Look for kmem_cache_create

```c
/* Cache creation in init */
void __init some_init(void)
{
    my_cache = kmem_cache_create("my_objects",
                                  sizeof(struct my_obj),
                                  0, SLAB_HWCACHE_ALIGN,
                                  NULL);
}

/* SIGNAL: kmem_cache_create call during init */
```

---

## Rule 3: Look for kmem_cache_alloc/free Pairs

```c
/* Allocation */
obj = kmem_cache_alloc(my_cache, GFP_KERNEL);

/* Freeing */
kmem_cache_free(my_cache, obj);

/* SIGNAL: Paired alloc/free from same cache */
```

**Chinese Explanation:**

Rule 3: Look for kmem_cache_alloc/free pairs - objects allocated and freed from same cache.

---

## Rule 4: Look for Constructor Function

```c
/* Constructor initializes object */
static void my_obj_init(void *p)
{
    struct my_obj *obj = p;
    INIT_LIST_HEAD(&obj->list);
    spin_lock_init(&obj->lock);
}

/* Used in cache creation */
kmem_cache_create("my_obj", sizeof(struct my_obj),
                  0, 0, my_obj_init);

/* SIGNAL: Constructor function passed to kmem_cache_create */
```

---

## Rule 5: Look for SLAB Flags

```c
/* Common SLAB flags */
SLAB_HWCACHE_ALIGN  /* Align to cache line */
SLAB_PANIC          /* Panic on failure */
SLAB_RECLAIM_ACCOUNT /* Account for reclaim */

/* SIGNAL: SLAB_* flags in kmem_cache_create */
```

---

## Summary Checklist

```
+=============================================================================+
|                    OBJECT POOL IDENTIFICATION CHECKLIST                      |
+=============================================================================+

    [ ] 1. CACHE VARIABLE
        struct kmem_cache * pointer
    
    [ ] 2. CACHE CREATION
        kmem_cache_create() call
    
    [ ] 3. ALLOC/FREE PAIRS
        kmem_cache_alloc/free from same cache
    
    [ ] 4. CONSTRUCTOR
        Optional init function
    
    [ ] 5. SLAB FLAGS
        SLAB_* flags usage

    SCORING:
    3+ indicators = Object Pool pattern
```

---

## Red Flags: NOT Object Pool

```
    THESE ARE NOT OBJECT POOL:
    ==========================

    1. Regular kmalloc/kfree
       No dedicated cache
    
    2. One-time allocation
       Object pool for repeated alloc/free
    
    3. Variable-size allocations
       Object pool for same-size objects
```

---

## Version

Based on **Linux kernel v3.2**.
