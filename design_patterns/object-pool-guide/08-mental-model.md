# Final Mental Model: Object Pool (SLAB)

## One-Paragraph Summary

The Object Pool pattern (SLAB allocator in Linux) provides efficient allocation for frequently created same-size objects. Instead of calling kmalloc repeatedly for common structures like task_struct, inode, or sk_buff, the kernel creates dedicated caches with `kmem_cache_create()`. Objects are allocated from these caches with `kmem_cache_alloc()` - which is faster because: (1) no fragmentation since all objects are same size, (2) freed objects stay in cache for reuse, (3) optional constructor pre-initializes objects. Use SLAB when you allocate many objects of the same type frequently.

**Chinese Summary:**

Object Pool pattern (SLAB allocator) provides efficient allocation for frequently created same-size objects. Instead of repeated kmalloc for common structures (task_struct, inode, sk_buff), kernel creates dedicated caches with kmem_cache_create. Objects allocated with kmem_cache_alloc which is faster: no fragmentation (same size), freed objects cached for reuse, optional constructor. Use SLAB when allocating many same-type objects frequently.

---

## Decision Flowchart

```
    Allocating same struct many times?
            |
    +-------+-------+
    |               |
   YES              NO
    |               |
    v               v
  SLAB            kmalloc
    
    
    Need guaranteed allocation?
            |
    +-------+-------+
    |               |
   YES              NO
    |               |
    v               v
  MEMPOOL         SLAB
```

---

## Quick Reference

```
    SLAB LIFECYCLE:
    ===============
    
    /* Create (once, at init) */
    cache = kmem_cache_create("name", size, align, flags, ctor);
    
    /* Use (many times) */
    obj = kmem_cache_alloc(cache, GFP_KERNEL);
    /* ... use obj ... */
    kmem_cache_free(cache, obj);
    
    /* Destroy (once, at exit) */
    kmem_cache_destroy(cache);


    KERNEL CACHES:
    ==============
    
    task_struct_cachep  - Processes
    inode_cachep        - Inodes
    dentry_cache        - Directory entries
    files_cachep        - Open files
```

---

## Version

Based on **Linux kernel v3.2** SLAB allocator.
