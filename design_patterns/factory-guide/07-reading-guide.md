# Source Reading Guide: Factory Pattern

A guided path through Linux kernel v3.2 source code.

---

## Reading Path Overview

```
    PHASE 1: Network Device Factory
    ===============================
    include/linux/netdevice.h      <- struct net_device
    net/core/dev.c                 <- alloc_netdev()
    
    PHASE 2: Socket Buffer Factory
    ==============================
    include/linux/skbuff.h         <- struct sk_buff
    net/core/skbuff.c              <- alloc_skb()
    
    PHASE 3: Block Device Factory
    =============================
    include/linux/genhd.h          <- struct gendisk
    block/genhd.c                  <- alloc_disk()
```

---

## Phase 1: Network Device Factory

### File: include/linux/netdevice.h

```
    WHAT TO LOOK FOR:
    =================
    
    struct net_device:
    - Many fields that need initialization
    - Private data area at end
    
    Function declarations:
    - alloc_netdev()
    - free_netdev()
    - netdev_priv()
```

### File: net/core/dev.c

```
    WHAT TO LOOK FOR:
    =================
    
    alloc_netdev_mqs():
    - Size calculation with alignment
    - kzalloc for structure + private
    - Field initialization
    - Setup callback invocation
    
    free_netdev():
    - Resource cleanup
```

**Chinese Explanation:**

Phase 1: Network device factory. In netdevice.h, look at struct net_device with many fields needing initialization. In dev.c, study alloc_netdev_mqs for size calculation, allocation, initialization, and setup callback.

---

## Phase 2: Socket Buffer Factory

### File: include/linux/skbuff.h

```
    WHAT TO LOOK FOR:
    =================
    
    struct sk_buff:
    - Data pointers (head, data, tail, end)
    - Reference count (users)
```

### File: net/core/skbuff.c

```
    WHAT TO LOOK FOR:
    =================
    
    __alloc_skb():
    - Cache allocation for header
    - Data buffer allocation
    - Pointer initialization
```

---

## Key Functions to Trace

| Function | File | Purpose |
|----------|------|---------|
| `alloc_netdev_mqs()` | net/core/dev.c | Network device factory |
| `free_netdev()` | net/core/dev.c | Network device destructor |
| `__alloc_skb()` | net/core/skbuff.c | Socket buffer factory |
| `kfree_skb()` | net/core/skbuff.c | Socket buffer destructor |
| `alloc_disk()` | block/genhd.c | Block device factory |

---

## Tracing Exercise

```
    TRACE: Network Device Creation
    ==============================
    
    1. Start at a driver using alloc_netdev()
       (e.g., drivers/net/ethernet/intel/e1000/e1000_main.c)
    
    2. Trace into alloc_netdev_mqs()
    
    3. See size calculation and allocation
    
    4. Observe field initialization
    
    5. Find where setup callback is called
```

---

## Reading Checklist

```
    [ ] Read struct net_device definition
    [ ] Read alloc_netdev_mqs implementation
    [ ] Read struct sk_buff definition
    [ ] Read __alloc_skb implementation
    [ ] Understand private data handling
```

---

## Version

This reading guide is for **Linux kernel v3.2**.
