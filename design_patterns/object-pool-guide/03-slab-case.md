# Case 1: SLAB Allocator

The SLAB allocator is the core implementation of Object Pool in Linux kernel.

---

## Subsystem Context

```
+=============================================================================+
|                    SLAB ALLOCATOR                                            |
+=============================================================================+

    KEY KERNEL CACHES:
    ==================

    task_struct_cachep  - Process descriptors
    inode_cachep        - Inode structures
    dentry_cache        - Directory entries
    files_cachep        - File structures
    sighand_cachep      - Signal handlers
    fs_cachep           - FS info structures


    CACHE LIFECYCLE:
    ================

    1. INITIALIZATION (boot time)
       cache = kmem_cache_create("name", size, ...)
    
    2. USAGE (runtime)
       obj = kmem_cache_alloc(cache, GFP_KERNEL)
       /* use object */
       kmem_cache_free(cache, obj)
    
    3. CLEANUP (module unload)
       kmem_cache_destroy(cache)
```

**Chinese Explanation:**

SLAB allocator: Key kernel caches include task_struct, inode, dentry, files. Cache lifecycle: create at init, alloc/free at runtime, destroy at cleanup.

---

## Key Structures

```c
/* mm/slab.h (simplified) */

struct kmem_cache {
    unsigned int size;           /* Object size */
    unsigned int align;          /* Alignment */
    unsigned long flags;         /* SLAB flags */
    const char *name;            /* Cache name */
    void (*ctor)(void *);        /* Constructor */
    
    struct list_head slabs_full;
    struct list_head slabs_partial;
    struct list_head slabs_free;
};

struct slab {
    struct list_head list;       /* Link in cache list */
    unsigned long colouroff;     /* Color offset */
    void *s_mem;                 /* Start of objects */
    unsigned int inuse;          /* Objects in use */
    unsigned int free;           /* First free object */
};
```

---

## Minimal Simulation

```c
/* Simplified SLAB allocator simulation */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OBJECTS_PER_SLAB 8

/* Slab structure */
struct slab {
    void *objects;
    int used[OBJECTS_PER_SLAB];
    int inuse;
    struct slab *next;
};

/* Cache structure */
struct kmem_cache {
    const char *name;
    size_t size;
    void (*ctor)(void *);
    struct slab *slabs;
    int total_allocs;
    int total_frees;
};

/* Create cache */
struct kmem_cache *kmem_cache_create(const char *name,
                                      size_t size,
                                      void (*ctor)(void *))
{
    struct kmem_cache *cache;
    
    cache = calloc(1, sizeof(*cache));
    cache->name = name;
    cache->size = size;
    cache->ctor = ctor;
    
    printf("[SLAB] Created cache '%s' (size=%zu)\n", name, size);
    return cache;
}

/* Allocate new slab */
static struct slab *alloc_slab(struct kmem_cache *cache)
{
    struct slab *slab = calloc(1, sizeof(*slab));
    slab->objects = calloc(OBJECTS_PER_SLAB, cache->size);
    
    /* Run constructor on all objects */
    if (cache->ctor) {
        int i;
        for (i = 0; i < OBJECTS_PER_SLAB; i++) {
            void *obj = (char *)slab->objects + i * cache->size;
            cache->ctor(obj);
        }
    }
    
    printf("  [SLAB] Allocated new slab (%d objects)\n", OBJECTS_PER_SLAB);
    return slab;
}

/* Allocate object from cache */
void *kmem_cache_alloc(struct kmem_cache *cache)
{
    struct slab *slab;
    int i;
    
    /* Find slab with free object */
    for (slab = cache->slabs; slab; slab = slab->next) {
        if (slab->inuse < OBJECTS_PER_SLAB)
            break;
    }
    
    /* Need new slab? */
    if (!slab) {
        slab = alloc_slab(cache);
        slab->next = cache->slabs;
        cache->slabs = slab;
    }
    
    /* Find free slot */
    for (i = 0; i < OBJECTS_PER_SLAB; i++) {
        if (!slab->used[i]) {
            slab->used[i] = 1;
            slab->inuse++;
            cache->total_allocs++;
            return (char *)slab->objects + i * cache->size;
        }
    }
    
    return NULL;
}

/* Free object to cache */
void kmem_cache_free(struct kmem_cache *cache, void *obj)
{
    struct slab *slab;
    int i;
    
    /* Find slab containing object */
    for (slab = cache->slabs; slab; slab = slab->next) {
        char *start = slab->objects;
        char *end = start + OBJECTS_PER_SLAB * cache->size;
        
        if ((char *)obj >= start && (char *)obj < end) {
            i = ((char *)obj - start) / cache->size;
            slab->used[i] = 0;
            slab->inuse--;
            cache->total_frees++;
            return;
        }
    }
}

/* Print cache stats */
void kmem_cache_stats(struct kmem_cache *cache)
{
    struct slab *slab;
    int slab_count = 0;
    int total_inuse = 0;
    
    for (slab = cache->slabs; slab; slab = slab->next) {
        slab_count++;
        total_inuse += slab->inuse;
    }
    
    printf("[STATS] Cache '%s':\n", cache->name);
    printf("  Slabs: %d, Objects in use: %d\n", slab_count, total_inuse);
    printf("  Total allocs: %d, Total frees: %d\n",
           cache->total_allocs, cache->total_frees);
}

/* Example object and constructor */
struct task_struct {
    int pid;
    int state;
    char name[16];
};

void task_struct_init(void *p)
{
    struct task_struct *task = p;
    task->pid = 0;
    task->state = 0;
    task->name[0] = 0;
}

int main(void)
{
    struct kmem_cache *task_cache;
    struct task_struct *tasks[20];
    int i;
    
    printf("=== SLAB ALLOCATOR SIMULATION ===\n\n");
    
    /* Create cache */
    task_cache = kmem_cache_create("task_struct",
                                    sizeof(struct task_struct),
                                    task_struct_init);
    
    /* Allocate objects */
    printf("\n[TEST] Allocating 12 tasks:\n");
    for (i = 0; i < 12; i++) {
        tasks[i] = kmem_cache_alloc(task_cache);
        tasks[i]->pid = i + 1;
        sprintf(tasks[i]->name, "task%d", i);
    }
    
    kmem_cache_stats(task_cache);
    
    /* Free some */
    printf("\n[TEST] Freeing 5 tasks:\n");
    for (i = 0; i < 5; i++) {
        kmem_cache_free(task_cache, tasks[i]);
    }
    
    kmem_cache_stats(task_cache);
    
    /* Reallocate */
    printf("\n[TEST] Allocating 3 more tasks:\n");
    for (i = 0; i < 3; i++) {
        tasks[i] = kmem_cache_alloc(task_cache);
    }
    
    kmem_cache_stats(task_cache);
    
    return 0;
}
```

---

## What SLAB Provides

```
    SLAB CORE PROVIDES:
    ===================
    
    [X] Slab management
    [X] Free list tracking
    [X] Memory allocation
    [X] Statistics
    [X] Debugging features

    USER PROVIDES:
    ==============
    
    [X] Object size
    [X] Constructor (optional)
    [X] Alignment requirements
```

---

## Version

Based on **Linux kernel v3.2** mm/slab.c.
