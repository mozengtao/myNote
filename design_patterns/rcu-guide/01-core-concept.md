# Core Concept: RCU (Read-Copy-Update) Pattern

What RCU means in kernel architecture and why it is essential for read-heavy concurrent data structures.

---

## What Problem Does RCU Solve?

```
+=============================================================================+
|                    THE RCU PROBLEM                                           |
+=============================================================================+

    READ-HEAVY DATA STRUCTURES:
    ===========================

    Many kernel data structures are:
    - Read VERY frequently (thousands/second)
    - Written rarely (occasional updates)
    
    Examples:
    - Routing tables (constant lookups, rare route changes)
    - Dcache (constant path resolution, rare file ops)
    - Process lists (constant iteration, rare fork/exit)
    - Module list (constant symbol lookup, rare load/unload)


    TRADITIONAL LOCKING PROBLEM:
    ============================

    rwlock_t lock;
    
    /* Reader */                    /* Writer */
    read_lock(&lock);               write_lock(&lock);
    data = ptr->field;              ptr->field = new_data;
    read_unlock(&lock);             write_unlock(&lock);
    
    PROBLEM: Even readers must acquire lock!
    - Cache line bouncing between CPUs
    - Contention on multi-core systems
    - Readers block each other
    - Expensive for read-heavy workloads


    RCU SOLUTION:
    =============

    /* Reader - NO LOCKS! */        /* Writer */
    rcu_read_lock();                new = kmalloc(...);
    p = rcu_dereference(ptr);       *new = *old;
    data = p->field;                modify(new);
    rcu_read_unlock();              rcu_assign_pointer(ptr, new);
                                    synchronize_rcu();
                                    kfree(old);
    
    BENEFIT: Readers pay almost zero cost!
    - No cache line bouncing
    - No contention
    - Readers never block
```

**中文说明：**

RCU解决的问题：读密集的数据结构（路由表、目录缓存）每次读都加锁太昂贵——缓存行在CPU间跳动、多核竞争、读者互相阻塞。RCU解决方案：读者几乎零成本——无锁、无竞争、读者永不阻塞。写者负责复制、修改、替换、等待、释放。

---

## How RCU Works

```
+=============================================================================+
|                    RCU MECHANISM                                             |
+=============================================================================+

    CORE IDEA: Writers wait for readers, not the other way around
    
    
    TIME ------>
    
    CPU0 (Reader)    CPU1 (Writer)         Memory
    =============    =============         ======
    
    rcu_read_lock()
    p = rcu_dereference(ptr)                ptr --> [old_data]
    use(p)                                         
    rcu_read_unlock()
                     new = copy(old)               [new_data]
                     modify(new)
                     rcu_assign_pointer(ptr, new)  ptr --> [new_data]
                                                   (old still exists)
                     synchronize_rcu()
                     /* WAITS for all readers */
                     kfree(old)                    [old_data freed]


    KEY INSIGHT:
    ============
    
    - Readers see EITHER old OR new data (never torn/partial)
    - Writers ensure old data isn't freed until ALL readers done
    - "Grace period" = time until all pre-existing readers finish
    
    
    GRACE PERIOD:
    =============
    
    |<----------- grace period ----------->|
    |                                       |
    v                                       v
    +---+---+---+---+---+---+---+---+---+---+
    | R | R | R |   |   | R | R |   |   |   |  <- Readers
    +---+---+---+---+---+---+---+---+---+---+
              ^                           ^
              |                           |
         rcu_assign_pointer()      synchronize_rcu() returns
              (publish new)         (old can be freed)
```

**中文说明：**

RCU机制：核心思想是写者等待读者，而非相反。读者看到旧数据或新数据（永不看到部分/撕裂数据）。写者确保旧数据在所有读者完成前不被释放。"宽限期"是所有预先存在的读者完成的时间。

---

## RCU API

```c
/* ================================================================
 * READER-SIDE API
 * ================================================================ */

rcu_read_lock();           /* Mark start of read-side critical section */
                           /* Prevents grace period from ending */
                           /* CANNOT sleep while held! */

ptr = rcu_dereference(p);  /* Safe pointer dereference */
                           /* Ensures proper memory ordering */
                           /* Returns the pointer value */

rcu_read_unlock();         /* Mark end of read-side critical section */


/* ================================================================
 * WRITER-SIDE API
 * ================================================================ */

rcu_assign_pointer(p, v);  /* Publish new pointer value */
                           /* Ensures proper memory ordering */
                           /* Readers may see old or new after this */

synchronize_rcu();         /* Wait for all pre-existing readers */
                           /* Blocks until grace period ends */
                           /* After return, safe to free old data */

call_rcu(&head, callback); /* Async version - callback when safe */
                           /* Non-blocking */
                           /* callback(head) called after grace period */
```

---

## RCU vs Traditional Locking

```
    RWLOCK:                         RCU:
    =======                         ====
    
    Readers acquire lock            Readers NO lock
    Writer waits for readers        Writer waits for readers
    Readers wait for writer         Readers NEVER wait
    Fair (bounded wait)             Writer may wait long
    
    +--------+--------+--------+    +--------+--------+--------+
    |   R    |   R    | WAIT   |    |   R    |   R    |   R    |
    +--------+--------+--------+    +--------+--------+--------+
             WRITER HOLDING                  |
                                     Writer working (readers continue)
    
    Best for:                       Best for:
    - Write-heavy                   - Read-heavy
    - Short critical sections       - Long-lived data
    - Bounded latency needed        - Low read latency critical
```

---

## Why RCU in Kernel

```
    KERNEL USE CASES:
    =================
    
    1. DCACHE (directory entry cache)
       - Path lookups: millions/second
       - File creates/deletes: occasional
       - RCU enables lock-free path walk
    
    2. ROUTING TABLE
       - Route lookups: every packet
       - Route updates: rare
       - RCU enables lock-free routing
    
    3. MODULE LIST
       - Symbol lookups: frequent
       - Module load/unload: rare
       - RCU enables lock-free symbol resolution
    
    4. PID HASH
       - Process lookups: very frequent
       - Process create/exit: less frequent
       - RCU enables fast process lookup
```

---

## Version

Based on **Linux kernel v3.2** RCU implementation.
