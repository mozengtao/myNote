# Source Reading Guide: RCU Pattern

A guided path through Linux kernel v3.2 source code for understanding RCU.

---

## Reading Path Overview

```
    PHASE 1: Core RCU API
    =====================
    include/linux/rcupdate.h   <- API declarations
    kernel/rcupdate.c          <- Core implementation
    
    PHASE 2: Usage Examples
    =======================
    fs/dcache.c                <- Dcache RCU usage
    net/ipv4/route.c           <- Routing table RCU
    kernel/module.c            <- Module list RCU
    
    PHASE 3: RCU List Operations
    ============================
    include/linux/rculist.h    <- RCU-protected list macros
```

---

## Phase 1: Core RCU API

### File: include/linux/rcupdate.h

```
    WHAT TO LOOK FOR:
    =================
    
    Read-side API:
        rcu_read_lock()
        rcu_read_unlock()
        rcu_dereference()
    
    Write-side API:
        rcu_assign_pointer()
        synchronize_rcu()
        call_rcu()
    
    Data structures:
        struct rcu_head
```

### File: kernel/rcupdate.c

```
    WHAT TO LOOK FOR:
    =================
    
    synchronize_rcu() implementation:
    - How grace periods are tracked
    - How it waits for readers
    
    call_rcu() implementation:
    - How callbacks are queued
    - When callbacks are invoked
```

**中文说明：**

阶段1：核心RCU API。在rcupdate.h中查找读侧API（rcu_read_lock等）和写侧API（synchronize_rcu等）。在rcupdate.c中学习宽限期如何跟踪、如何等待读者。

---

## Phase 2: Usage Examples

### File: fs/dcache.c

```
    WHAT TO LOOK FOR:
    =================
    
    Search for: rcu_read_lock
    
    Functions using RCU:
        __d_lookup_rcu() - RCU-protected dentry lookup
        d_lookup() - May use RCU path
    
    Study how:
        - Path walk uses RCU
        - Dentry hash is RCU-protected
```

### File: net/ipv4/route.c

```
    WHAT TO LOOK FOR:
    =================
    
    Search for: rcu_dereference
    
    Functions:
        ip_route_input_slow()
        __ip_route_output_key()
    
    Study how:
        - Route cache is RCU-protected
        - Lookups are lock-free
```

---

## Phase 3: RCU List Operations

### File: include/linux/rculist.h

```
    WHAT TO LOOK FOR:
    =================
    
    List operations:
        list_add_rcu()
        list_del_rcu()
        list_for_each_entry_rcu()
    
    Hash list operations:
        hlist_add_head_rcu()
        hlist_del_rcu()
        hlist_for_each_entry_rcu()
```

---

## Key Functions to Trace

| Function | File | Purpose |
|----------|------|---------|
| `rcu_read_lock()` | include/linux/rcupdate.h | Start read section |
| `rcu_dereference()` | include/linux/rcupdate.h | Safe pointer access |
| `synchronize_rcu()` | kernel/rcupdate.c | Wait for readers |
| `call_rcu()` | kernel/rcupdate.c | Async callback |
| `__d_lookup_rcu()` | fs/dcache.c | RCU dentry lookup |

---

## Tracing Exercise

```
    TRACE: Dcache RCU Path Walk
    ===========================
    
    1. Start at do_lookup() in fs/namei.c
    
    2. Find where rcu_read_lock() is called
    
    3. Trace __d_lookup_rcu() in fs/dcache.c
    
    4. See how dentry hash is traversed with RCU
    
    5. Find where rcu_read_unlock() is called
    
    6. Understand the complete lock-free path
```

---

## Reading Checklist

```
    [ ] Read rcu_read_lock/unlock definitions
    [ ] Read rcu_dereference macro
    [ ] Read synchronize_rcu implementation
    [ ] Study dcache RCU usage
    [ ] Study route cache RCU usage
    [ ] Understand list_for_each_entry_rcu
```

---

## Version

This reading guide is for **Linux kernel v3.2**.
