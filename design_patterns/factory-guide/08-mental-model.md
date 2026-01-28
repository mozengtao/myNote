# Final Mental Model: Factory Pattern

## One-Paragraph Summary

The Factory pattern encapsulates object creation into a dedicated function that handles memory allocation, field initialization, and setup. Instead of callers manually allocating and initializing complex structures, they call a factory function like `alloc_netdev()` or `alloc_skb()` that returns a fully initialized, ready-to-use object. Key elements: (1) factory function named `alloc_xxx()` that returns `struct xxx *`, (2) matching destructor `free_xxx()`, (3) optional setup callback for type-specific initialization, (4) size parameters for private data. Factory ensures consistency, hides internal details, handles errors properly, and makes changes easier.

**Chinese Summary:**

Factory pattern encapsulates object creation into a dedicated function handling memory allocation, field initialization, and setup. Instead of manual allocation and initialization, callers call factory functions like alloc_netdev() or alloc_skb() to get fully initialized objects. Key elements: alloc_xxx() function returning struct xxx *, matching free_xxx() destructor, optional setup callback, size parameters. Factory ensures consistency, hides details, handles errors, simplifies changes.

---

## Decision Flowchart

```
    Object creation needs multiple steps?
            |
    +-------+-------+
    |               |
   YES              NO
    |               |
    v               v
FACTORY         Simple kmalloc
    
    
    Multiple callers create same object type?
            |
    +-------+-------+
    |               |
   YES              NO
    |               |
    v               v
FACTORY         Inline code OK
```

---

## Quick Reference

```
    FACTORY STRUCTURE:
    ==================
    
    struct xxx *alloc_xxx(size, name, setup)
    {
        obj = kzalloc(sizeof + size);
        /* Initialize fields */
        if (setup)
            setup(obj);
        return obj;
    }
    
    void free_xxx(struct xxx *obj)
    {
        /* Cleanup */
        kfree(obj);
    }


    KERNEL EXAMPLES:
    ================
    
    alloc_netdev()  / free_netdev()   - Network devices
    alloc_skb()     / kfree_skb()     - Socket buffers
    alloc_disk()    / put_disk()      - Block devices
```

---

## Version

Based on **Linux kernel v3.2** factory patterns.
