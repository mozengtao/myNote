# Unified Object Pool Skeleton

A generic C skeleton capturing the Object Pool pattern.

---

## Complete Skeleton

```c
/*
 * Generic Object Pool Skeleton
 * Based on Linux kernel SLAB allocator
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 * PART 1: POOL CONFIGURATION
 * ================================================================ */

#define POOL_SIZE 16   /* Objects per pool */

/* ================================================================
 * PART 2: POOL STRUCTURE
 * ================================================================ */

struct object_pool {
    const char *name;
    size_t obj_size;
    void (*ctor)(void *);       /* Constructor */
    
    /* Pool storage */
    void *objects;              /* Object array */
    int free_map[POOL_SIZE];    /* 0=free, 1=used */
    int used_count;
    
    /* Statistics */
    int total_allocs;
    int total_frees;
};

/* ================================================================
 * PART 3: POOL MANAGEMENT
 * ================================================================ */

/* Create pool */
struct object_pool *pool_create(const char *name,
                                 size_t obj_size,
                                 void (*ctor)(void *))
{
    struct object_pool *pool;
    int i;
    
    pool = calloc(1, sizeof(*pool));
    pool->name = name;
    pool->obj_size = obj_size;
    pool->ctor = ctor;
    
    /* Allocate object array */
    pool->objects = calloc(POOL_SIZE, obj_size);
    
    /* Run constructor on all objects */
    if (ctor) {
        for (i = 0; i < POOL_SIZE; i++) {
            void *obj = (char *)pool->objects + i * obj_size;
            ctor(obj);
        }
    }
    
    printf("[POOL] Created '%s' (%d objects, %zu bytes each)\n",
           name, POOL_SIZE, obj_size);
    
    return pool;
}

/* Allocate from pool */
void *pool_alloc(struct object_pool *pool)
{
    int i;
    
    for (i = 0; i < POOL_SIZE; i++) {
        if (!pool->free_map[i]) {
            pool->free_map[i] = 1;
            pool->used_count++;
            pool->total_allocs++;
            return (char *)pool->objects + i * pool->obj_size;
        }
    }
    
    printf("[POOL] '%s' exhausted!\n", pool->name);
    return NULL;
}

/* Free to pool */
void pool_free(struct object_pool *pool, void *obj)
{
    int i;
    char *base = pool->objects;
    
    /* Find object index */
    i = ((char *)obj - base) / pool->obj_size;
    
    if (i >= 0 && i < POOL_SIZE && pool->free_map[i]) {
        pool->free_map[i] = 0;
        pool->used_count--;
        pool->total_frees++;
        
        /* Optionally re-run constructor */
        if (pool->ctor) {
            pool->ctor(obj);
        }
    }
}

/* Destroy pool */
void pool_destroy(struct object_pool *pool)
{
    printf("[POOL] Destroying '%s' (allocs=%d, frees=%d)\n",
           pool->name, pool->total_allocs, pool->total_frees);
    free(pool->objects);
    free(pool);
}

/* ================================================================
 * PART 4: EXAMPLE USAGE
 * ================================================================ */

struct my_object {
    int id;
    int value;
    char name[32];
};

void my_object_init(void *p)
{
    struct my_object *obj = p;
    obj->id = 0;
    obj->value = 0;
    obj->name[0] = 0;
}

int main(void)
{
    struct object_pool *pool;
    struct my_object *objs[10];
    int i;
    
    printf("=== OBJECT POOL SKELETON ===\n\n");
    
    /* Create pool */
    pool = pool_create("my_objects",
                       sizeof(struct my_object),
                       my_object_init);
    
    /* Allocate objects */
    printf("\n[TEST] Allocating 5 objects:\n");
    for (i = 0; i < 5; i++) {
        objs[i] = pool_alloc(pool);
        objs[i]->id = i + 1;
        printf("  Allocated object id=%d\n", objs[i]->id);
    }
    printf("  Used: %d/%d\n", pool->used_count, POOL_SIZE);
    
    /* Free some */
    printf("\n[TEST] Freeing 3 objects:\n");
    for (i = 0; i < 3; i++) {
        pool_free(pool, objs[i]);
    }
    printf("  Used: %d/%d\n", pool->used_count, POOL_SIZE);
    
    /* Reallocate */
    printf("\n[TEST] Allocating 2 more:\n");
    for (i = 0; i < 2; i++) {
        objs[i] = pool_alloc(pool);
        printf("  Allocated at %p\n", objs[i]);
    }
    
    /* Destroy */
    printf("\n");
    pool_destroy(pool);
    
    return 0;
}
```

---

## Mapping to Kernel

```
    SKELETON                KERNEL
    ========                ======
    
    object_pool             struct kmem_cache
    pool_create             kmem_cache_create
    pool_alloc              kmem_cache_alloc
    pool_free               kmem_cache_free
    pool_destroy            kmem_cache_destroy
    ctor                    cache constructor
```

---

## Key Implementation Points

```
    1. PRE-ALLOCATION
       Objects allocated in batch
       Ready for fast allocation
    
    2. FREE LIST
       Track which objects are free
       O(n) scan or free list
    
    3. CONSTRUCTOR
       Initialize objects once
       Reuse without full reinit
    
    4. SIZE UNIFORMITY
       All objects same size
       No fragmentation
```

---

## Version

Based on **Linux kernel v3.2** SLAB patterns.
