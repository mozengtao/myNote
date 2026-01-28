# Case 2: Memory Pool (mempool)

Memory pools provide guaranteed allocations for critical kernel paths.

---

## Subsystem Context

```
+=============================================================================+
|                    MEMORY POOL                                               |
+=============================================================================+

    THE PROBLEM:
    ============

    Some allocations MUST succeed:
    - Block I/O path (writing dirty pages)
    - Memory reclaim path
    
    But memory may be exhausted!


    MEMPOOL SOLUTION:
    =================

    Pre-allocate minimum number of objects.
    
    Normal allocation:
    - Try regular allocator first
    - Fall back to pool if allocation fails
    
    Guarantee:
    - At least min_nr objects always available
    - Critical paths never fail allocation
```

**Chinese Explanation:**

Memory pool solves guaranteed allocation. Some allocations must succeed (block I/O, memory reclaim) but memory may be exhausted. Mempool pre-allocates minimum objects. Normal allocation tries regular allocator first, falls back to pool if needed. Critical paths never fail.

---

## Key Structures

```c
/* include/linux/mempool.h */

typedef struct mempool_s {
    spinlock_t lock;
    int min_nr;           /* Minimum pre-allocated */
    int curr_nr;          /* Currently available */
    void **elements;      /* Pre-allocated objects */
    
    /* Allocation functions */
    void *pool_data;
    mempool_alloc_t *alloc;
    mempool_free_t *free;
} mempool_t;

/* Create mempool */
mempool_t *mempool_create(int min_nr, mempool_alloc_t *alloc,
                          mempool_free_t *free, void *pool_data);

/* Allocate from pool */
void *mempool_alloc(mempool_t *pool, gfp_t gfp);

/* Free to pool */
void mempool_free(void *obj, mempool_t *pool);
```

---

## Minimal Simulation

```c
/* Simplified mempool simulation */

#include <stdio.h>
#include <stdlib.h>

struct mempool {
    int min_nr;
    int curr_nr;
    void **elements;
    size_t obj_size;
};

/* Simulated allocation failure rate */
static int alloc_fail_rate = 30;  /* 30% failure */

/* Simulate system allocator that may fail */
void *system_alloc(size_t size)
{
    if (rand() % 100 < alloc_fail_rate) {
        printf("    [SYS] Allocation FAILED\n");
        return NULL;
    }
    return malloc(size);
}

/* Create mempool */
struct mempool *mempool_create(int min_nr, size_t obj_size)
{
    struct mempool *pool;
    int i;
    
    pool = calloc(1, sizeof(*pool));
    pool->min_nr = min_nr;
    pool->obj_size = obj_size;
    pool->elements = calloc(min_nr, sizeof(void *));
    
    /* Pre-allocate minimum objects */
    printf("[MEMPOOL] Creating pool (min=%d)\n", min_nr);
    for (i = 0; i < min_nr; i++) {
        pool->elements[i] = malloc(obj_size);
        pool->curr_nr++;
        printf("  Pre-allocated object %d\n", i);
    }
    
    return pool;
}

/* Allocate from pool */
void *mempool_alloc(struct mempool *pool)
{
    void *obj;
    
    printf("[MEMPOOL] Alloc request\n");
    
    /* Try regular allocation first */
    obj = system_alloc(pool->obj_size);
    if (obj) {
        printf("    Regular alloc succeeded\n");
        return obj;
    }
    
    /* Fall back to pool */
    if (pool->curr_nr > 0) {
        pool->curr_nr--;
        obj = pool->elements[pool->curr_nr];
        pool->elements[pool->curr_nr] = NULL;
        printf("    Using pool reserve (remaining=%d)\n", pool->curr_nr);
        return obj;
    }
    
    printf("    FATAL: Pool exhausted!\n");
    return NULL;
}

/* Free to pool */
void mempool_free(void *obj, struct mempool *pool)
{
    /* Refill pool if below minimum */
    if (pool->curr_nr < pool->min_nr) {
        pool->elements[pool->curr_nr] = obj;
        pool->curr_nr++;
        printf("[MEMPOOL] Free: returned to pool (count=%d)\n", pool->curr_nr);
    } else {
        free(obj);
        printf("[MEMPOOL] Free: freed to system\n");
    }
}

int main(void)
{
    struct mempool *pool;
    void *objs[10];
    int i;
    
    printf("=== MEMPOOL SIMULATION ===\n\n");
    
    srand(42);
    
    /* Create pool with 3 reserved objects */
    pool = mempool_create(3, 64);
    
    /* Allocate several objects (some will fail and use pool) */
    printf("\n--- Allocating 6 objects ---\n");
    for (i = 0; i < 6; i++) {
        objs[i] = mempool_alloc(pool);
    }
    
    /* Free objects */
    printf("\n--- Freeing objects ---\n");
    for (i = 0; i < 6; i++) {
        if (objs[i])
            mempool_free(objs[i], pool);
    }
    
    return 0;
}
```

---

## Mempool vs SLAB

```
    SLAB:                           MEMPOOL:
    =====                           ========
    
    Performance optimization        Guaranteed allocation
    May fail under pressure         Pre-allocated reserve
    General purpose                 Critical paths only
    Caches objects                  Holds reserve objects
```

---

## Version

Based on **Linux kernel v3.2** mm/mempool.c.
