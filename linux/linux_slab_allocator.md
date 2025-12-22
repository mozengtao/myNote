# Linux Kernel Slab/Slub Allocator Architecture (v3.2)

## Overview

This document explains **slab/slub allocator architecture** in Linux kernel v3.2, focusing on object caching and allocation paths.

---

## Why Slab Allocators Exist

```
+------------------------------------------------------------------+
|  THE PROBLEM WITH PAGE ALLOCATOR                                 |
+------------------------------------------------------------------+

    PAGE ALLOCATOR ISSUES FOR SMALL OBJECTS:
    +----------------------------------------------------------+
    | 1. Minimum allocation is PAGE_SIZE (4KB)                  |
    |    - 64-byte object → 98% memory waste                    |
    |                                                           |
    | 2. No object reuse                                        |
    |    - Every alloc/free touches page tables                 |
    |    - Constructor/destructor run every time                |
    |                                                           |
    | 3. No cache locality awareness                            |
    |    - Same-type objects scattered in memory                |
    +----------------------------------------------------------+

    SLAB SOLUTION:
    
    Page Allocator (buddy system)
           │
           │  Allocates pages
           ▼
    ┌─────────────────────────────────────────────────────────────┐
    │                    SLAB ALLOCATOR                            │
    │  ┌─────────────────────────────────────────────────────────┐│
    │  │                   SLAB (one page)                       ││
    │  │  ┌────┬────┬────┬────┬────┬────┬────┬────┬────┬────┐   ││
    │  │  │obj │obj │obj │obj │obj │obj │obj │obj │obj │obj │   ││
    │  │  │ 0  │ 1  │ 2  │ 3  │ 4  │ 5  │ 6  │ 7  │ 8  │ 9  │   ││
    │  │  └────┴────┴────┴────┴────┴────┴────┴────┴────┴────┘   ││
    │  └─────────────────────────────────────────────────────────┘│
    │                                                              │
    │  Benefits:                                                   │
    │  - Objects fit exactly (no waste)                           │
    │  - Freed objects reused without page ops                    │
    │  - Same-type objects are cache-adjacent                     │
    └─────────────────────────────────────────────────────────────┘
```

**中文解释：**
- 页分配器问题：最小分配 4KB（小对象浪费）、无对象复用、无缓存局部性
- Slab 解决方案：精确分配对象大小、复用已释放对象、同类对象缓存相邻

---

## Cache Creation and Object Lifecycle

From `include/linux/slab.h`:

```c
struct kmem_cache *kmem_cache_create(
    const char *name,     /* Cache name (for debugging) */
    size_t size,          /* Object size */
    size_t align,         /* Alignment requirement */
    unsigned long flags,  /* Behavior flags */
    void (*ctor)(void *)  /* Constructor (optional) */
);

void *kmem_cache_alloc(struct kmem_cache *cache, gfp_t flags);
void kmem_cache_free(struct kmem_cache *cache, void *obj);
void kmem_cache_destroy(struct kmem_cache *cache);
```

```
+------------------------------------------------------------------+
|  CACHE LIFECYCLE                                                 |
+------------------------------------------------------------------+

    1. CREATE CACHE
    ┌─────────────────────────────────────────────────────────────┐
    │ task_struct_cache = kmem_cache_create(                       │
    │     "task_struct",          /* Name */                       │
    │     sizeof(struct task_struct),                              │
    │     ARCH_MIN_TASKALIGN,     /* Alignment */                  │
    │     SLAB_PANIC | SLAB_NOTRACK,                               │
    │     NULL                    /* No constructor */             │
    │ );                                                           │
    └─────────────────────────────────────────────────────────────┘
           │
           ▼
    2. ALLOCATE OBJECT
    ┌─────────────────────────────────────────────────────────────┐
    │ struct task_struct *p = kmem_cache_alloc(                    │
    │     task_struct_cache,                                       │
    │     GFP_KERNEL              /* May sleep */                  │
    │ );                                                           │
    └─────────────────────────────────────────────────────────────┘
           │
           ▼
    3. USE OBJECT
    ┌─────────────────────────────────────────────────────────────┐
    │ p->pid = allocate_pid();                                     │
    │ p->state = TASK_RUNNING;                                     │
    │ /* ... */                                                    │
    └─────────────────────────────────────────────────────────────┘
           │
           ▼
    4. FREE OBJECT (returns to cache, not page allocator)
    ┌─────────────────────────────────────────────────────────────┐
    │ kmem_cache_free(task_struct_cache, p);                       │
    └─────────────────────────────────────────────────────────────┘
           │
           ▼
    5. DESTROY CACHE (when module unloads)
    ┌─────────────────────────────────────────────────────────────┐
    │ kmem_cache_destroy(task_struct_cache);                       │
    └─────────────────────────────────────────────────────────────┘
```

**中文解释：**
- 缓存生命周期：创建缓存 → 分配对象 → 使用对象 → 释放对象 → 销毁缓存
- 释放对象时返回缓存（不返回页分配器），下次分配直接复用

---

## Per-CPU Caching

```
+------------------------------------------------------------------+
|  PER-CPU OBJECT CACHING                                          |
+------------------------------------------------------------------+

    CPU 0                   CPU 1                   CPU 2
    ┌─────────────┐        ┌─────────────┐        ┌─────────────┐
    │  cpu_cache  │        │  cpu_cache  │        │  cpu_cache  │
    │  ┌───────┐  │        │  ┌───────┐  │        │  ┌───────┐  │
    │  │ obj   │  │        │  │ obj   │  │        │  │ obj   │  │
    │  │ obj   │  │        │  │ obj   │  │        │  │ (empty)│  │
    │  │ obj   │  │        │  │ (empty)│  │        │  └───────┘  │
    │  └───────┘  │        │  └───────┘  │        └──────┬──────┘
    └──────┬──────┘        └──────┬──────┘               │
           │                      │                       │
           │                      │            refill from
           │                      │            shared cache
           │                      │                       │
           ▼                      ▼                       ▼
    ┌─────────────────────────────────────────────────────────────┐
    │                    SHARED / PARTIAL SLABS                    │
    │  ┌─────────────────────────────────────────────────────────┐│
    │  │  SLAB 1 (partial)   SLAB 2 (full)    SLAB 3 (empty)    ││
    │  │  ○●●●●●●●●●●●●●●    ●●●●●●●●●●●●●●●  ○○○○○○○○○○○○○○○    ││
    │  │  (○ = free, ● = used)                                   ││
    │  └─────────────────────────────────────────────────────────┘│
    └─────────────────────────────────────────────────────────────┘

    ALLOCATION FAST PATH (no lock):
    +----------------------------------------------------------+
    | 1. Try cpu_cache first (local, no lock needed)            |
    | 2. If not empty, pop object and return                    |
    | 3. Time: ~20 nanoseconds                                  |
    +----------------------------------------------------------+
    
    ALLOCATION SLOW PATH (needs lock):
    +----------------------------------------------------------+
    | 1. cpu_cache empty                                        |
    | 2. Refill from shared cache or partial slab               |
    | 3. If no partial slab, allocate new slab from page alloc  |
    | 4. Time: ~100-1000 nanoseconds                            |
    +----------------------------------------------------------+
```

**中文解释：**
- 每 CPU 缓存：每个 CPU 有本地对象缓存，分配时无需锁
- 快速路径：从 cpu_cache 弹出对象（约 20 纳秒）
- 慢速路径：cpu_cache 为空时，从共享缓存或部分 slab 补充（需要锁）

---

## Fast vs Slow Allocation Paths

```
+------------------------------------------------------------------+
|  ALLOCATION PATH DECISION TREE                                   |
+------------------------------------------------------------------+

    kmem_cache_alloc(cache, flags)
           │
           ▼
    ┌──────────────────────┐
    │ Check cpu_cache      │
    │ (per-CPU, no lock)   │
    └──────────┬───────────┘
               │
        ┌──────┴──────┐
        │             │
    Has object?   Empty?
        │             │
        ▼             ▼
    ┌────────┐  ┌─────────────────────┐
    │ Return │  │ Check partial slab  │  ← Slow path starts
    │ object │  │ (needs lock)        │
    └────────┘  └──────────┬──────────┘
                           │
                    ┌──────┴──────┐
                    │             │
               Has objects?    Empty?
                    │             │
                    ▼             ▼
              ┌──────────┐  ┌──────────────────────┐
              │ Refill   │  │ Allocate new slab    │
              │ cpu_cache│  │ from page allocator  │
              │ Return   │  │ (may sleep if        │
              │ object   │  │  GFP_KERNEL)         │
              └──────────┘  └──────────────────────┘

    TIMELINE COMPARISON:
    
    Fast path:   [alloc] ──── 20ns ────▶ [return]
    
    Slow path:   [alloc] ────────── 100-500ns ────────────▶ [return]
    
    Page alloc:  [alloc] ────────────── 1000-5000ns ──────────────▶ [return]
```

**中文解释：**
- 决策树：先检查 cpu_cache → 检查部分 slab → 分配新 slab
- 时间对比：快速路径 20ns、慢速路径 100-500ns、页分配 1000-5000ns

---

## Debugging vs Performance Tradeoffs

```
+------------------------------------------------------------------+
|  SLAB FLAGS AND TRADEOFFS                                        |
+------------------------------------------------------------------+

    PERFORMANCE FLAGS:
    +----------------------------------------------------------+
    | SLAB_HWCACHE_ALIGN  - Align to CPU cache line             |
    |                       + Better performance                |
    |                       - More memory waste                 |
    +----------------------------------------------------------+
    
    DEBUG FLAGS:
    +----------------------------------------------------------+
    | SLAB_RED_ZONE       - Add guard bytes around objects      |
    |                       + Detect buffer overflows           |
    |                       - Memory overhead                   |
    |                                                           |
    | SLAB_POISON         - Fill freed objects with pattern     |
    |                       + Detect use-after-free             |
    |                       - CPU overhead on free              |
    |                                                           |
    | SLAB_STORE_USER     - Track last alloc/free caller        |
    |                       + Better debugging                  |
    |                       - Memory + CPU overhead             |
    +----------------------------------------------------------+

    OBJECT LAYOUT WITH DEBUG:
    
    ┌──────────┬─────────────────────────┬──────────┐
    │ RED ZONE │      OBJECT DATA        │ RED ZONE │
    │ (guard)  │                         │ (guard)  │
    └──────────┴─────────────────────────┴──────────┘
         │                                     │
         │  Overwritten? → DETECTED!           │
         └─────────────────────────────────────┘

    FREED OBJECT (with SLAB_POISON):
    
    ┌─────────────────────────────────────────────┐
    │ 0x6b 0x6b 0x6b 0x6b 0x6b 0x6b 0x6b 0x6b ... │
    │ (poison pattern)                            │
    └─────────────────────────────────────────────┘
         │
         │  Accessed after free? → Pattern corrupted → DETECTED!
```

**中文解释：**
- 性能标志：SLAB_HWCACHE_ALIGN（缓存行对齐，更好性能但更多内存）
- 调试标志：
  - SLAB_RED_ZONE：添加保护字节检测缓冲区溢出
  - SLAB_POISON：填充释放对象检测 use-after-free
  - SLAB_STORE_USER：记录分配/释放调用者

---

## User-Space Memory Pool

```
+------------------------------------------------------------------+
|  USER-SPACE TRANSLATION                                          |
+------------------------------------------------------------------+

    KERNEL CONCEPT      →    USER-SPACE EQUIVALENT
    ───────────────────────────────────────────────────
    kmem_cache          →    Object pool
    kmem_cache_alloc    →    pool_alloc()
    kmem_cache_free     →    pool_free()
    Per-CPU cache       →    Thread-local free list
    Slab                →    Memory chunk
```

```c
/* User-space object pool inspired by slab allocator */

#include <stdlib.h>
#include <pthread.h>
#include <string.h>

#define OBJECTS_PER_SLAB 32

struct slab {
    void *objects[OBJECTS_PER_SLAB];
    int free_count;
    struct slab *next;
};

struct object_pool {
    const char *name;
    size_t object_size;
    void (*constructor)(void *);
    
    pthread_mutex_t lock;
    struct slab *partial_slabs;
    struct slab *full_slabs;
    struct slab *empty_slabs;
    
    /* Per-thread cache */
    pthread_key_t thread_cache_key;
};

struct thread_cache {
    void *objects[8];  /* Small local cache */
    int count;
};

/* Fast path: thread-local allocation */
void *pool_alloc(struct object_pool *pool)
{
    struct thread_cache *tc = pthread_getspecific(pool->thread_cache_key);
    
    if (tc && tc->count > 0) {
        /* Fast path: pop from thread cache */
        return tc->objects[--tc->count];
    }
    
    /* Slow path: get from shared pool */
    pthread_mutex_lock(&pool->lock);
    
    struct slab *s = pool->partial_slabs;
    if (!s) {
        /* Allocate new slab */
        s = malloc(sizeof(struct slab));
        for (int i = 0; i < OBJECTS_PER_SLAB; i++) {
            s->objects[i] = malloc(pool->object_size);
            if (pool->constructor) {
                pool->constructor(s->objects[i]);
            }
        }
        s->free_count = OBJECTS_PER_SLAB;
        s->next = pool->partial_slabs;
        pool->partial_slabs = s;
    }
    
    void *obj = s->objects[--s->free_count];
    
    if (s->free_count == 0) {
        /* Move to full list */
        pool->partial_slabs = s->next;
        s->next = pool->full_slabs;
        pool->full_slabs = s;
    }
    
    pthread_mutex_unlock(&pool->lock);
    
    /* Refill thread cache */
    if (tc && tc->count < 8 && pool->partial_slabs) {
        /* Batch refill */
    }
    
    return obj;
}

/* Fast path: thread-local free */
void pool_free(struct object_pool *pool, void *obj)
{
    struct thread_cache *tc = pthread_getspecific(pool->thread_cache_key);
    
    if (tc && tc->count < 8) {
        /* Fast path: push to thread cache */
        tc->objects[tc->count++] = obj;
        return;
    }
    
    /* Slow path: return to shared pool */
    pthread_mutex_lock(&pool->lock);
    /* ... return object to slab ... */
    pthread_mutex_unlock(&pool->lock);
}
```

**中文解释：**
- 用户态对象池：模拟内核 slab 分配器
- 快速路径：从线程本地缓存分配/释放（无锁）
- 慢速路径：从共享池分配（需要锁）
- 设计原则：线程本地缓存 + 共享 slab + 批量补充

