# Object Pool Anti-Patterns

Common mistakes to avoid when using SLAB allocator.

---

## Anti-Pattern 1: Wrong Size Allocation

```c
/* BAD: Allocating wrong size from cache */
struct small_obj *s;
struct large_obj *l;

small_cache = kmem_cache_create("small", sizeof(struct small_obj), ...);

l = kmem_cache_alloc(small_cache, GFP_KERNEL);
/* large_obj doesn't fit in small cache! */
/* Memory corruption! */

/* CORRECT: Each struct gets its own cache */
small_cache = kmem_cache_create("small", sizeof(struct small_obj), ...);
large_cache = kmem_cache_create("large", sizeof(struct large_obj), ...);

s = kmem_cache_alloc(small_cache, GFP_KERNEL);
l = kmem_cache_alloc(large_cache, GFP_KERNEL);
```

**Chinese Explanation:**

Anti-pattern 1: Wrong size allocation - Don't allocate from cache meant for different sized objects. Each struct type needs its own cache.

---

## Anti-Pattern 2: Not Destroying Cache

```c
/* BAD: Forgetting to destroy cache */
static struct kmem_cache *my_cache;

void __init my_init(void)
{
    my_cache = kmem_cache_create("my_objs", ...);
}

void __exit my_exit(void)
{
    /* Forgot kmem_cache_destroy! */
    /* Memory leak! */
}

/* CORRECT: Always destroy */
void __exit my_exit(void)
{
    kmem_cache_destroy(my_cache);
}
```

---

## Anti-Pattern 3: Freeing to Wrong Cache

```c
/* BAD: Free to wrong cache */
obj = kmem_cache_alloc(cache_a, GFP_KERNEL);
kmem_cache_free(cache_b, obj);  /* Wrong cache! */

/* CORRECT: Free to same cache */
obj = kmem_cache_alloc(cache_a, GFP_KERNEL);
kmem_cache_free(cache_a, obj);  /* Same cache */
```

**Chinese Explanation:**

Anti-pattern 3: Freeing to wrong cache - Always free to the same cache you allocated from.

---

## Anti-Pattern 4: Using Object After Free

```c
/* BAD: Use after free */
obj = kmem_cache_alloc(cache, GFP_KERNEL);
kmem_cache_free(cache, obj);
obj->field = 1;  /* Object already freed! */

/* CORRECT: Don't use after free */
obj = kmem_cache_alloc(cache, GFP_KERNEL);
/* use obj */
kmem_cache_free(cache, obj);
obj = NULL;  /* Prevent accidental use */
```

---

## Anti-Pattern 5: Not Checking Allocation

```c
/* BAD: Not checking return */
obj = kmem_cache_alloc(cache, GFP_KERNEL);
obj->field = 1;  /* NULL dereference if allocation failed! */

/* CORRECT: Always check */
obj = kmem_cache_alloc(cache, GFP_KERNEL);
if (!obj)
    return -ENOMEM;
obj->field = 1;
```

---

## Summary Checklist

```
+=============================================================================+
|                    SLAB SAFE USAGE CHECKLIST                                 |
+=============================================================================+

    [X] Allocate from correct cache (matching size)
    [X] Destroy cache in module exit
    [X] Free to same cache as allocation
    [X] Don't use after free
    [X] Always check allocation return
    [X] Don't mix with regular kmalloc
```

---

## Version

Based on **Linux kernel v3.2** SLAB usage.
