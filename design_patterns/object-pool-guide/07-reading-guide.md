# Source Reading Guide: Object Pool (SLAB)

A guided path through Linux kernel v3.2 source code.

---

## Reading Path Overview

```
    PHASE 1: SLAB API
    =================
    include/linux/slab.h           <- API declarations
    
    PHASE 2: SLAB Implementation
    ============================
    mm/slab.c                      <- Classic SLAB
    mm/slub.c                      <- SLUB (alternative)
    
    PHASE 3: Memory Pool
    ====================
    include/linux/mempool.h        <- Mempool API
    mm/mempool.c                   <- Mempool implementation
```

---

## Phase 1: SLAB API

### File: include/linux/slab.h

```
    WHAT TO LOOK FOR:
    =================
    
    API functions:
    - kmem_cache_create()
    - kmem_cache_destroy()
    - kmem_cache_alloc()
    - kmem_cache_free()
    
    Flags:
    - SLAB_HWCACHE_ALIGN
    - SLAB_PANIC
    - SLAB_RECLAIM_ACCOUNT
```

**Chinese Explanation:**

Phase 1: In slab.h, look for API functions (create, destroy, alloc, free) and SLAB flags.

---

## Phase 2: SLAB Implementation

### File: mm/slab.c

```
    WHAT TO LOOK FOR:
    =================
    
    struct kmem_cache:
    - size, align, flags
    - constructor
    - slab lists (full, partial, empty)
    
    kmem_cache_alloc():
    - How objects are allocated from slabs
    - Cache coloring
    
    kmem_cache_free():
    - How objects return to slabs
```

### File: mm/slub.c

```
    SLUB is alternative to SLAB:
    - Simpler design
    - Better scalability
    - Same API
```

---

## Key Functions to Trace

| Function | File | Purpose |
|----------|------|---------|
| `kmem_cache_create()` | mm/slab.c | Create cache |
| `kmem_cache_alloc()` | mm/slab.c | Allocate object |
| `kmem_cache_free()` | mm/slab.c | Free object |
| `mempool_create()` | mm/mempool.c | Create mempool |
| `mempool_alloc()` | mm/mempool.c | Guaranteed alloc |

---

## Tracing Exercise

```
    TRACE: Object Allocation
    ========================
    
    1. Find a subsystem using SLAB
       (e.g., fs/inode.c for inode_cachep)
    
    2. Find kmem_cache_create call
    
    3. Trace kmem_cache_alloc calls
    
    4. See how objects are used
    
    5. Find kmem_cache_free calls
    
    6. Find kmem_cache_destroy in exit
```

---

## /proc/slabinfo

```
    RUNTIME INSPECTION:
    ===================
    
    cat /proc/slabinfo
    
    Shows:
    - Cache names
    - Active/total objects
    - Object size
    - Pages per slab
    - Statistics
```

---

## Reading Checklist

```
    [ ] Read kmem_cache structure in mm/slab.c
    [ ] Read kmem_cache_create implementation
    [ ] Read kmem_cache_alloc implementation
    [ ] Find real kernel cache (e.g., inode_cachep)
    [ ] Check /proc/slabinfo on running system
```

---

## Version

This reading guide is for **Linux kernel v3.2**.
