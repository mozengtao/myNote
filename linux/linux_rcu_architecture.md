# Linux Kernel RCU (Read-Copy-Update) Architecture (v3.2)

## Table of Contents

1. [Phase 1 — Why RCU Exists](#phase-1--why-rcu-exists)
2. [Phase 2 — RCU Contracts](#phase-2--rcu-contracts)
3. [Phase 3 — RCU APIs & Semantics](#phase-3--rcu-apis--semantics)
4. [Phase 4 — Real Kernel Examples](#phase-4--real-kernel-examples)
5. [Phase 5 — Failure Modes](#phase-5--failure-modes)
6. [Phase 6 — User-Space Transfer](#phase-6--user-space-transfer)

---

## Phase 1 — Why RCU Exists

### 1.1 The Problem RCU Solves

```
+------------------------------------------------------------------------+
|                    THE PROBLEM: READER-WRITER CONTENTION                |
+------------------------------------------------------------------------+

Traditional Locking (rwlock):
    
    Reader 1 ──┬──[lock]────────[unlock]──
    Reader 2 ──┼──[lock]────────[unlock]──
    Reader 3 ──┼──[lock]────────[unlock]──    CONTENTION!
    Writer   ──┴────────[WAIT...][lock]───    (Cache bouncing)
    
    Problems:
    1. Readers contend with each other (cache line bouncing)
    2. Writer waits for ALL readers
    3. Reader count increment is EXPENSIVE (atomic)
    
RCU Solution:
    
    Reader 1 ──────[read]──────────────────    NO lock!
    Reader 2 ──────[read]──────────────────    NO contention!
    Reader 3 ──────[read]──────────────────    ZERO overhead!
    Writer   ──[update ptr]──[wait GP]────    Waits, but readers don't care
    
    How it works:
    1. Readers access data with NO synchronization overhead
    2. Writers update by publishing NEW version
    3. Writers wait for grace period before freeing OLD version
```

**中文解释：**
- **传统读写锁问题**：即使多个读者不修改数据，它们仍需竞争锁，导致缓存行跳动
- **RCU解决方案**：读者完全无锁，零开销；写者发布新版本，等待宽限期后释放旧版本
- **核心思想**：用时间换空间——延迟释放，确保所有老读者完成

### 1.2 Why Locks Are Insufficient

```
+------------------------------------------------------------------------+
|                    LOCK OVERHEAD ANALYSIS                               |
+------------------------------------------------------------------------+

Read-Heavy Workload (99% reads, 1% writes):

With rwlock:
    +---------+    +---------+    +---------+    +---------+
    | CPU 0   |    | CPU 1   |    | CPU 2   |    | CPU 3   |
    | READ    |    | READ    |    | READ    |    | READ    |
    +---------+    +---------+    +---------+    +---------+
         |              |              |              |
         +------+-------+------+-------+------+-------+
                |              |              |
                v              v              v
           +--------------------------------+
           |     SHARED LOCK COUNTER        |  ← Cache line
           |    (bouncing between CPUs)     |     contention!
           +--------------------------------+

    Each read_lock() causes:
    1. atomic_inc() on shared counter
    2. Cache line moved to local CPU
    3. Other CPUs must re-fetch
    
    Cost: ~100-300 cycles per read!

With RCU:
    +---------+    +---------+    +---------+    +---------+
    | CPU 0   |    | CPU 1   |    | CPU 2   |    | CPU 3   |
    | READ    |    | READ    |    | READ    |    | READ    |
    +---------+    +---------+    +---------+    +---------+
         |              |              |              |
    [local]        [local]        [local]        [local]
    
    rcu_read_lock() does:
    1. preempt_disable() (local CPU only)
    2. NO shared data modified
    3. NO cache line movement
    
    Cost: ~10-20 cycles per read!
    
SPEEDUP: 10-30x for read-heavy workloads!
```

### 1.3 The Read-Mostly Pattern

```
+------------------------------------------------------------------------+
|                    READ-MOSTLY DATA STRUCTURES                          |
+------------------------------------------------------------------------+

Examples in Linux kernel:
    
1. ROUTING TABLES
   - Millions of lookups per second
   - Rare updates (route change)
   - Perfect for RCU
   
2. DENTRY CACHE
   - Pathname lookups (every file access)
   - Directory modifications (rare)
   - RCU walk-mode for fast lookup
   
3. NETWORK DEVICE LIST
   - Packet processing (millions/sec)
   - Device add/remove (rare)
   - RCU-protected traversal

4. PROCESS LIST
   - Scheduler reads frequently
   - Fork/exit modifies rarely
   - RCU for safe traversal

PATTERN SIGNATURE:
    - Reads >> Writes (10:1 or higher)
    - Data must be consistent, not latest
    - Readers can tolerate stale data briefly
    - Writers can tolerate waiting
```

**中文解释：**
- **读多写少场景**：路由表、目录缓存、网络设备列表、进程列表
- **关键特征**：读操作远多于写操作；读者可以容忍短暂的旧数据
- **RCU优势**：读路径零开销，写路径付出延迟代价

---

## Phase 2 — RCU Contracts

### 2.1 The RCU Mental Model

```
+------------------------------------------------------------------------+
|                    RCU MENTAL MODEL                                     |
+------------------------------------------------------------------------+

KEY INSIGHT: Readers and writers operate in DIFFERENT time domains

                    GRACE PERIOD
                 <--------------->
    
    Reader 1  |====[reading]====|
    Reader 2       |====[reading]====|
    Reader 3            |====[reading]====|
              |                            |
              |                            |
    Writer    [update]                     [free old]
              ^                            ^
              |                            |
         call_rcu()                   callback runs
         
    GUARANTEE:
    - All readers that SAW the old pointer have COMPLETED
    - Before the callback frees the old data

TIME DOMAIN SEPARATION:
    
    [OLD DATA]  ────────────────> [FREED]
         ^                           ^
         |                           |
    Readers in                  After grace period
    old epoch                   (no old readers)
    
    [NEW DATA]  ────────────────> [CURRENT]
         ^
         |
    Readers in
    new epoch
```

### 2.2 Reader Contract

```
+------------------------------------------------------------------------+
|                    READER CONTRACT                                      |
+------------------------------------------------------------------------+

WHAT READERS ARE ALLOWED TO DO:

1. ✓ Access RCU-protected pointers
   rcu_read_lock();
   ptr = rcu_dereference(global_ptr);  /* Safe */
   data = ptr->field;                   /* Safe */
   rcu_read_unlock();

2. ✓ Traverse RCU-protected lists
   rcu_read_lock();
   list_for_each_entry_rcu(entry, &head, list) {
       /* Safe to read entry */
   }
   rcu_read_unlock();

3. ✓ Nest RCU read sections
   rcu_read_lock();
   rcu_read_lock();  /* OK: nesting is fine */
   /* ... */
   rcu_read_unlock();
   rcu_read_unlock();

4. ✓ Copy data out for later use
   rcu_read_lock();
   ptr = rcu_dereference(global_ptr);
   local_copy = *ptr;  /* Copy while protected */
   rcu_read_unlock();
   use(local_copy);    /* Use copy outside lock */

WHAT READERS MUST NEVER DO:

1. ✗ Block or sleep (in non-PREEMPT_RCU)
   rcu_read_lock();
   mutex_lock(&mutex);         /* BUG! Can sleep */
   kmalloc(GFP_KERNEL);        /* BUG! Can sleep */
   copy_from_user(buf, ...);   /* BUG! Can sleep */
   rcu_read_unlock();

2. ✗ Hold references beyond unlock
   rcu_read_lock();
   ptr = rcu_dereference(global_ptr);
   rcu_read_unlock();
   use(ptr);  /* BUG! ptr may be freed! */

3. ✗ Modify RCU-protected data
   rcu_read_lock();
   ptr = rcu_dereference(global_ptr);
   ptr->count++;  /* BUG! Readers must not modify */
   rcu_read_unlock();

4. ✗ Use without rcu_dereference()
   rcu_read_lock();
   ptr = global_ptr;           /* BUG! No memory barrier */
   data = ptr->field;          /* May see stale ptr */
   rcu_read_unlock();
```

### 2.3 Updater Contract

```
+------------------------------------------------------------------------+
|                    UPDATER CONTRACT                                     |
+------------------------------------------------------------------------+

WHAT UPDATERS MUST DO:

1. ✓ Publish new version atomically
   struct data *new = kmalloc(...);
   initialize(new);
   rcu_assign_pointer(global_ptr, new);  /* Atomic publish */

2. ✓ Wait for grace period before freeing old
   old = rcu_dereference_protected(global_ptr, lockdep_expr);
   rcu_assign_pointer(global_ptr, new);
   synchronize_rcu();  /* Wait for all readers */
   kfree(old);         /* Now safe to free */

   OR use deferred callback:
   old = xchg(&global_ptr, new);
   call_rcu(&old->rcu_head, my_free_callback);

3. ✓ Coordinate with other updaters
   spin_lock(&my_lock);          /* Protect against other writers */
   old = global_ptr;
   rcu_assign_pointer(global_ptr, new);
   spin_unlock(&my_lock);
   synchronize_rcu();
   kfree(old);

4. ✓ Maintain structural integrity
   - Never leave dangling pointers
   - Never create loops during update
   - Ensure new version is fully initialized

WHAT UPDATERS MUST NOT DO:

1. ✗ Free immediately after update
   rcu_assign_pointer(global_ptr, new);
   kfree(old);  /* BUG! Readers may still use old! */

2. ✗ Reuse old data without waiting
   rcu_assign_pointer(global_ptr, new);
   memset(old, 0, sizeof(*old));  /* BUG! */
```

### 2.4 RCU Guarantees Summary

```
+------------------------------------------------------------------------+
|                    RCU GUARANTEES                                       |
+------------------------------------------------------------------------+

GUARANTEE 1: PUBLICATION SAFETY
    Writer: rcu_assign_pointer(gp, new);
    Reader: ptr = rcu_dereference(gp);
    
    If reader sees new pointer, reader sees FULLY INITIALIZED data
    (memory barriers ensure ordering)

GUARANTEE 2: GRACE PERIOD COMPLETION
    synchronize_rcu() returns ONLY after:
    - ALL pre-existing RCU read-side critical sections complete
    - All CPUs have passed through a quiescent state

GUARANTEE 3: CALLBACK ORDERING
    call_rcu(&head, func) ensures:
    - func() runs after all pre-existing readers complete
    - func() may run concurrently with NEW readers

GUARANTEE 4: NO READER STARVATION
    - Readers never wait for writers
    - Readers never wait for other readers
    - Read-side overhead is near-zero

NON-GUARANTEES (what RCU does NOT promise):

1. ✗ Readers see latest version immediately
   (Writers publish, but readers may still traverse old)

2. ✗ Call_rcu callbacks run immediately
   (May be batched for efficiency)

3. ✗ Ordering between independent updates
   (Use additional synchronization if needed)
```

**中文解释：**
- **读者合约**：在`rcu_read_lock/unlock`之间可安全访问；不能阻塞；不能持有指针超出临界区
- **更新者合约**：必须原子发布新版本；必须等待宽限期后才能释放旧版本；必须与其他写者协调
- **RCU保证**：发布安全性（读者看到新指针时数据已初始化）；宽限期完成（所有老读者完成后才回调）

---

## Phase 3 — RCU APIs & Semantics

### 3.1 rcu_read_lock / rcu_read_unlock

```
+------------------------------------------------------------------------+
|                    rcu_read_lock() / rcu_read_unlock()                  |
+------------------------------------------------------------------------+

IMPLEMENTATION (non-preemptible RCU):
    static inline void rcu_read_lock(void)
    {
        preempt_disable();    /* [KEY] Prevent scheduling */
    }
    
    static inline void rcu_read_unlock(void)
    {
        preempt_enable();     /* [KEY] Allow scheduling */
    }

WHY preempt_disable()?
    - Quiescent state = context switch or idle
    - Disabling preemption prevents quiescent state
    - RCU knows reader is "active" as long as preemption disabled
    
USAGE PATTERN:
    rcu_read_lock();
    
    ptr = rcu_dereference(global_ptr);
    /* Safe to access *ptr here */
    /* CANNOT sleep or block */
    
    rcu_read_unlock();
    /* ptr is INVALID after this point! */

WHEN TO USE:
    ✓ Fast, read-only access to shared data
    ✓ Need to traverse lists without modification
    ✓ Performance-critical read paths

WHEN NOT TO USE:
    ✗ Need to modify data (use proper locking)
    ✗ Need to sleep or block
    ✗ Need reference beyond the critical section

COMMON MISUSE:
    /* BUG: Saving pointer beyond critical section */
    struct data *saved;
    rcu_read_lock();
    saved = rcu_dereference(global_ptr);
    rcu_read_unlock();
    use(saved);  /* CRASH! saved may be freed */
    
    /* FIX: Copy data or use refcount */
    struct data local_copy;
    rcu_read_lock();
    ptr = rcu_dereference(global_ptr);
    local_copy = *ptr;  /* Copy while protected */
    rcu_read_unlock();
    use(&local_copy);  /* Safe: local copy */
```

### 3.2 synchronize_rcu

```
+------------------------------------------------------------------------+
|                    synchronize_rcu()                                    |
+------------------------------------------------------------------------+

SEMANTICS:
    Blocks until all pre-existing RCU read-side critical sections complete.
    
TIMELINE:
    
    CPU 0: Reader   |=====[rcu_read_lock ... rcu_read_unlock]=====|
    CPU 1: Reader        |=====[rcu_read_lock ... rcu_read_unlock]=====|
    CPU 2: Writer   [update]                                           [done]
                        |<-------- synchronize_rcu() blocks -------->|
                        
    Writer waits for BOTH readers to exit their critical sections.

IMPLEMENTATION (conceptual):
    void synchronize_rcu(void)
    {
        /* Wait for all CPUs to pass through quiescent state */
        for_each_online_cpu(cpu) {
            wait_for_quiescent_state(cpu);
        }
    }
    
    /* Quiescent state = context switch, idle, or user mode */

USAGE PATTERN:
    /* Replace data and wait */
    old = global_ptr;
    rcu_assign_pointer(global_ptr, new);
    synchronize_rcu();  /* Wait for readers */
    kfree(old);         /* Safe to free */

WHEN TO USE:
    ✓ Infrequent updates where blocking is acceptable
    ✓ Simple free-after-update pattern
    ✓ When callback complexity is not justified

WHEN NOT TO USE:
    ✗ Hot path (can block for milliseconds)
    ✗ Interrupt/softirq context (cannot sleep!)
    ✗ High-frequency updates (use call_rcu instead)

COMMON MISUSE:
    /* BUG: In interrupt context */
    irqreturn_t my_handler(int irq, void *dev) {
        update_data();
        synchronize_rcu();  /* BUG! Cannot sleep in IRQ! */
        return IRQ_HANDLED;
    }
```

### 3.3 call_rcu

```
+------------------------------------------------------------------------+
|                    call_rcu()                                           |
+------------------------------------------------------------------------+

SEMANTICS:
    Schedules callback to run AFTER grace period completes.
    Does NOT block the caller.
    
TIMELINE:
    
    CPU 0: Reader   |=====[rcu_read_lock ... rcu_read_unlock]=====|
    CPU 1: Writer   [call_rcu]                                    [callback runs]
                        |<-------- grace period ----------------->|
                        
    Writer continues immediately; callback runs asynchronously.

USAGE PATTERN:
    struct my_data {
        struct rcu_head rcu;  /* [KEY] Must embed rcu_head */
        int value;
    };
    
    static void my_free_callback(struct rcu_head *head)
    {
        struct my_data *data = container_of(head, struct my_data, rcu);
        kfree(data);
    }
    
    void update_data(struct my_data *new)
    {
        struct my_data *old;
        
        spin_lock(&my_lock);
        old = global_ptr;
        rcu_assign_pointer(global_ptr, new);
        spin_unlock(&my_lock);
        
        call_rcu(&old->rcu, my_free_callback);  /* Non-blocking */
        /* Caller continues immediately */
    }

WHEN TO USE:
    ✓ Cannot block (interrupt context, holding locks)
    ✓ High-frequency updates
    ✓ Batch freeing for efficiency

WHEN NOT TO USE:
    ✗ Need synchronous guarantee (use synchronize_rcu)
    ✗ Cannot modify data structure (no rcu_head)
    ✗ Memory pressure concerns (callbacks may queue up)

COMMON MISUSE:
    /* BUG: Using stack-allocated data */
    void broken_update(void)
    {
        struct my_data local_data;
        call_rcu(&local_data.rcu, callback);
        /* CRASH! local_data goes out of scope */
    }
    
    /* BUG: Reusing before callback runs */
    void broken_reuse(struct my_data *old)
    {
        call_rcu(&old->rcu, callback);
        old->value = 42;  /* BUG! Readers may still access */
    }
```

### 3.4 rcu_dereference / rcu_assign_pointer

```
+------------------------------------------------------------------------+
|                    rcu_dereference() / rcu_assign_pointer()             |
+------------------------------------------------------------------------+

rcu_dereference(ptr):
    - Fetches RCU-protected pointer with appropriate barriers
    - Prevents compiler from reordering or optimizing
    - MUST be used inside rcu_read_lock/unlock
    
    /* Implementation */
    #define rcu_dereference(p) \
        ({ \
            typeof(p) _p = ACCESS_ONCE(p); \
            smp_read_barrier_depends(); \
            (_p); \
        })

rcu_assign_pointer(ptr, value):
    - Stores to RCU-protected pointer with write barrier
    - Ensures all prior writes are visible before pointer update
    
    /* Implementation */
    #define rcu_assign_pointer(p, v) \
        ({ \
            smp_wmb(); \
            ACCESS_ONCE(p) = (v); \
        })

WHY BARRIERS ARE NEEDED:
    
    Without barriers (BROKEN):
    
    Writer:                     Reader:
    new->field = 42;           ptr = global_ptr;
    global_ptr = new;          data = ptr->field;
    
    CPU may reorder! Reader might see new ptr but old field!
    
    With barriers (CORRECT):
    
    Writer:                     Reader:
    new->field = 42;           ptr = rcu_dereference(global_ptr);
    smp_wmb();          ←→     smp_read_barrier_depends();
    global_ptr = new;          data = ptr->field;
    
    Barriers enforce ordering: If reader sees new ptr, sees new field too.
```

### 3.5 API Decision Flowchart

```
+------------------------------------------------------------------------+
|                    RCU API DECISION FLOWCHART                           |
+------------------------------------------------------------------------+

    START: Need to protect shared data?
           |
           v
    Is it read-mostly? (reads >> writes)
           |
    NO ----+----> Use traditional locking (mutex, rwlock)
           |
    YES    v
    Can readers tolerate brief staleness?
           |
    NO ----+----> Use traditional locking
           |
    YES    v
    Use RCU!
           |
           v
    +----- Reader or Writer? -----+
    |                             |
    v                             v
  READER                        WRITER
    |                             |
    v                             v
  rcu_read_lock()            Need to free old data?
  ptr = rcu_dereference(p)        |
  ... use ptr ...           NO ---+---> Just rcu_assign_pointer()
  rcu_read_unlock()               |
                            YES   v
                            Can block?
                                  |
                            YES --+---> synchronize_rcu(); kfree(old);
                                  |
                            NO    v
                            call_rcu(&old->rcu, free_callback);
```

---

## Phase 4 — Real Kernel Examples

### 4.1 VFS: Dentry Cache (dcache)

```
+------------------------------------------------------------------------+
|                    DENTRY CACHE RCU USAGE                               |
+------------------------------------------------------------------------+

PROBLEM: Pathname lookup is extremely hot path
    - Every file operation starts with path lookup
    - Millions of lookups per second
    - Traditional locking = massive contention

SOLUTION: RCU "rcu-walk" mode

CODE (fs/dcache.c):
    /* Path lookup with RCU */
    struct dentry *__d_lookup_rcu(const struct dentry *parent,
                                   const struct qstr *name,
                                   unsigned *seqp)
    {
        /* Called with rcu_read_lock() held */
        
        struct hlist_bl_head *b = d_hash(parent, name->hash);
        struct hlist_bl_node *node;
        struct dentry *dentry;
        
        hlist_bl_for_each_entry_rcu(dentry, node, b, d_hash) {
            if (dentry->d_parent != parent)
                continue;
            if (d_unhashed(dentry))
                continue;
            if (!d_same_name(dentry, parent, name))
                continue;
                
            *seqp = read_seqcount_begin(&dentry->d_seq);
            return dentry;
        }
        return NULL;
    }

USAGE IN PATH LOOKUP:
    static int link_path_walk(const char *name, struct nameidata *nd)
    {
        /* ... */
        
        /* Try RCU-walk first (fast path) */
        rcu_read_lock();
        
        while (*name) {
            /* Traverse path components */
            dentry = __d_lookup_rcu(nd->path.dentry, &this, &seq);
            if (!dentry) {
                /* Fall back to ref-walk (slow path) */
                rcu_read_unlock();
                return walk_component(nd, ...);
            }
            /* Continue in RCU mode */
        }
        
        rcu_read_unlock();
        /* ... */
    }

WHY RCU IS PERFECT HERE:
    ✓ Lookup is read-only (doesn't modify dentry)
    ✓ Lookups >> modifications (most paths already cached)
    ✓ Stale data acceptable (will validate with sequence counter)
    ✓ Performance critical (every file access)

WHAT WOULD BREAK WITHOUT RCU:
    - Spin lock on every path component
    - Cache line bouncing on hash table lock
    - 10-100x slower path lookup
```

### 4.2 Network: Device List

```
+------------------------------------------------------------------------+
|                    NETWORK DEVICE RCU USAGE                             |
+------------------------------------------------------------------------+

PROBLEM: Packet processing must be fast
    - Millions of packets per second
    - Device list traversal on every packet
    - Device add/remove is rare

SOLUTION: RCU-protected device list

CODE (net/core/dev.c):
    /* RCU-protected RX handler registration */
    int netdev_rx_handler_register(struct net_device *dev,
                                   rx_handler_func_t *rx_handler,
                                   void *rx_handler_data)
    {
        ASSERT_RTNL();  /* Writer lock for updates */
        
        if (dev->rx_handler)
            return -EBUSY;
        
        /* [KEY] Atomic publish with barrier */
        rcu_assign_pointer(dev->rx_handler_data, rx_handler_data);
        rcu_assign_pointer(dev->rx_handler, rx_handler);
        
        return 0;
    }
    
    /* Unregistration with grace period */
    void netdev_rx_handler_unregister(struct net_device *dev)
    {
        ASSERT_RTNL();
        
        RCU_INIT_POINTER(dev->rx_handler, NULL);
        RCU_INIT_POINTER(dev->rx_handler_data, NULL);
    }

    /* Packet reception path (hot!) */
    static int __netif_receive_skb(struct sk_buff *skb)
    {
        rx_handler_func_t *rx_handler;
        
        rcu_read_lock();  /* [KEY] No lock, just preempt_disable */
        
        /* [KEY] Safe dereference */
        rx_handler = rcu_dereference(skb->dev->rx_handler);
        if (rx_handler) {
            switch (rx_handler(&skb)) {
            case RX_HANDLER_CONSUMED:
                goto out;
            /* ... */
            }
        }
        
        rcu_read_unlock();
        /* ... */
    }

    /* Grace period for safe cleanup */
    void synchronize_net(void)
    {
        might_sleep();
        if (rtnl_is_locked())
            synchronize_rcu_expedited();
        else
            synchronize_rcu();
    }

WHY RCU IS PERFECT HERE:
    ✓ Packet processing is read-only (device lookup)
    ✓ Packets >> device changes
    ✓ Cannot afford lock contention on fast path
    ✓ Device removal can wait for grace period
```

### 4.3 Process List (for_each_process)

```
+------------------------------------------------------------------------+
|                    PROCESS LIST RCU USAGE                               |
+------------------------------------------------------------------------+

PROBLEM: Need to traverse all processes safely
    - Scheduler needs to scan processes
    - /proc filesystem lists processes
    - Process creation/exit must not corrupt list

SOLUTION: RCU-protected task list

CODE (include/linux/sched.h):
    #define for_each_process(p) \
        for (p = &init_task ; (p = next_task(p)) != &init_task ; )

    /* RCU version */
    #define for_each_process_thread(p, t) \
        for_each_process(p) \
            for_each_thread(p, t)

    /* Safe list traversal */
    #define next_task(p) \
        list_entry_rcu((p)->tasks.next, struct task_struct, tasks)

USAGE EXAMPLE:
    void show_all_processes(void)
    {
        struct task_struct *p;
        
        rcu_read_lock();
        
        for_each_process(p) {
            printk("PID %d: %s\n", p->pid, p->comm);
            /* Safe: p cannot be freed during traversal */
        }
        
        rcu_read_unlock();
    }

PROCESS EXIT WITH RCU:
    void release_task(struct task_struct *p)
    {
        /* Remove from lists */
        list_del_rcu(&p->tasks);
        
        /* Use delayed free */
        call_rcu(&p->rcu, delayed_put_task_struct);
    }
    
    static void delayed_put_task_struct(struct rcu_head *rhp)
    {
        struct task_struct *tsk = container_of(rhp, 
                                               struct task_struct, rcu);
        put_task_struct(tsk);
    }

WHY RCU IS PERFECT HERE:
    ✓ Process list reads >> process fork/exit
    ✓ Readers just need consistent snapshot
    ✓ Cannot hold process lock during /proc reads
    ✓ Scheduler cannot afford lock overhead
```

---

## Phase 5 — Failure Modes

### 5.1 Use-After-Free Bugs

```
+------------------------------------------------------------------------+
|                    BUG: USE-AFTER-FREE                                  |
+------------------------------------------------------------------------+

SCENARIO 1: Missing grace period

    /* BROKEN CODE */
    void broken_update(struct data *new)
    {
        struct data *old = global_ptr;
        global_ptr = new;
        kfree(old);  /* BUG! Readers may still use old! */
    }
    
    TIMELINE:
    
    Reader:  [rcu_read_lock]----[use old ptr]----[CRASH!]----[unlock]
    Writer:  --------[update]-[free old]
                              ^
                              |
                     old freed while reader uses it!
    
    FIX:
    void correct_update(struct data *new)
    {
        struct data *old = global_ptr;
        rcu_assign_pointer(global_ptr, new);
        synchronize_rcu();  /* Wait for readers! */
        kfree(old);
    }

SCENARIO 2: Stack-allocated rcu_head

    /* BROKEN CODE */
    void broken_call_rcu(void)
    {
        struct data local;
        local.value = 42;
        call_rcu(&local.rcu, free_callback);
        return;  /* BUG! local goes out of scope */
    }
    
    TIMELINE:
    
    [call_rcu]---[function returns]---[stack overwritten]---[callback]
                                                                 |
                                                          CRASH! garbage

    FIX: Use heap allocation or static storage.

SCENARIO 3: Pointer escapes critical section

    /* BROKEN CODE */
    struct data *escaped_ptr;
    
    void reader_thread(void)
    {
        rcu_read_lock();
        escaped_ptr = rcu_dereference(global_ptr);
        rcu_read_unlock();
        
        /* Later... */
        use(escaped_ptr);  /* BUG! May be freed! */
    }
    
    FIX: Copy data or take reference before unlock.
```

### 5.2 Sleeping in RCU Read Section

```
+------------------------------------------------------------------------+
|                    BUG: SLEEPING IN RCU CRITICAL SECTION                |
+------------------------------------------------------------------------+

SCENARIO:
    void broken_reader(void)
    {
        rcu_read_lock();
        
        ptr = rcu_dereference(global_ptr);
        mutex_lock(&some_mutex);     /* BUG! May sleep! */
        copy_to_user(buf, ptr->data); /* BUG! May sleep! */
        
        rcu_read_unlock();
    }

WHY IT'S BROKEN:
    
    In non-PREEMPT_RCU:
    - rcu_read_lock() = preempt_disable()
    - Sleeping with preemption disabled = BUG
    - Kernel will warn: "scheduling while atomic"
    
    Even in PREEMPT_RCU:
    - Blocking extends grace period indefinitely
    - Writers wait forever
    - System hangs

TIMELINE (PREEMPT_RCU):
    
    Reader:  [lock]--[block on mutex.......................]--[unlock]
    Writer:  ----[update]----[synchronize_rcu HANGS FOREVER]
    
    Grace period never completes because reader never passes quiescent state.

FIX: Never sleep in RCU read section.

    void correct_reader(void)
    {
        /* Option 1: Copy while in RCU */
        rcu_read_lock();
        ptr = rcu_dereference(global_ptr);
        local_copy = *ptr;
        rcu_read_unlock();
        
        mutex_lock(&some_mutex);
        copy_to_user(buf, &local_copy);  /* OK: using copy */
        mutex_unlock(&some_mutex);
        
        /* Option 2: Take refcount, release RCU, then use */
        rcu_read_lock();
        ptr = rcu_dereference(global_ptr);
        refcount_inc(&ptr->refcount);
        rcu_read_unlock();
        
        mutex_lock(&some_mutex);
        use(ptr);  /* OK: refcount keeps ptr alive */
        mutex_unlock(&some_mutex);
        
        refcount_dec(&ptr->refcount);
    }
```

### 5.3 Incorrect Grace Period Assumptions

```
+------------------------------------------------------------------------+
|                    BUG: WRONG GRACE PERIOD ASSUMPTIONS                  |
+------------------------------------------------------------------------+

SCENARIO 1: Assuming immediate callback

    /* BROKEN ASSUMPTION */
    void update_with_notification(struct data *new)
    {
        old = xchg(&global_ptr, new);
        call_rcu(&old->rcu, notify_and_free);
        
        /* WRONG: Assuming old is freed "soon" */
        /* Callback may be delayed for milliseconds! */
    }
    
    REALITY:
    - RCU batches callbacks for efficiency
    - Grace period may take 10-100ms
    - Under load, may take longer

SCENARIO 2: Using wrong RCU flavor

    /* Softirq context uses rcu_read_lock_bh() */
    void softirq_reader(void)
    {
        rcu_read_lock_bh();
        ptr = rcu_dereference_bh(global_ptr);
        use(ptr);
        rcu_read_unlock_bh();
    }
    
    /* BROKEN: Using wrong synchronize */
    void broken_writer(void)
    {
        rcu_assign_pointer(global_ptr, new);
        synchronize_rcu();  /* WRONG! Should be synchronize_rcu_bh() */
        kfree(old);         /* BUG! softirq reader may still access */
    }
    
    RCU FLAVORS:
    - rcu_read_lock() ←→ synchronize_rcu() / call_rcu()
    - rcu_read_lock_bh() ←→ synchronize_rcu_bh() / call_rcu_bh()
    - rcu_read_lock_sched() ←→ synchronize_sched() / call_rcu_sched()

SCENARIO 3: Forgetting about other readers

    /* BROKEN: Only waiting for known readers */
    void broken_shutdown(void)
    {
        global_ptr = NULL;
        
        /* Wait for "our" reader */
        wait_for_my_reader_done();
        
        kfree(old);  /* BUG! Other modules may have readers! */
    }
    
    FIX:
    void correct_shutdown(void)
    {
        rcu_assign_pointer(global_ptr, NULL);
        synchronize_rcu();  /* Waits for ALL readers */
        kfree(old);
    }
```

---

## Phase 6 — User-Space Transfer

### 6.1 User-Space RCU Concepts

```
+------------------------------------------------------------------------+
|                    USER-SPACE RCU APPLICABILITY                         |
+------------------------------------------------------------------------+

CHALLENGE: No preempt_disable() in user space!
    - Cannot rely on scheduler for quiescent state detection
    - Must explicitly signal quiescent states
    
SOLUTIONS:

1. EPOCH-BASED RECLAMATION (Simple RCU alternative)
    - Readers increment global epoch on enter
    - Writers wait for all readers to advance epoch
    
2. HAZARD POINTERS
    - Readers publish which pointers they're using
    - Writers check hazard list before freeing
    
3. QUIESCENT-STATE-BASED RECLAMATION (QSBR)
    - Readers explicitly signal "I'm in quiescent state"
    - Writers wait for all threads to report QS
    
4. LIBURCU (User-space RCU library)
    - Full RCU implementation for user space
    - Multiple flavors for different tradeoffs

USER-SPACE RCU MENTAL MODEL:

    Thread 1:                Thread 2:
    [enter CS]               
    [reading...]             [update pointer]
    [exit CS]                [wait for CS exit]
    [signal QS]              [got signal: safe to free]
```

### 6.2 Read-Heavy Data Structures

```c
/*
 * Simple epoch-based RCU for user space
 * Suitable for read-heavy configuration data
 */

#include <stdatomic.h>
#include <stdlib.h>

/*
 * ============================================================
 * EPOCH-BASED RECLAMATION
 * ============================================================
 */

#define MAX_THREADS 64

typedef struct {
    _Atomic uint64_t global_epoch;
    _Atomic uint64_t thread_epoch[MAX_THREADS];
    int thread_count;
} epoch_state_t;

static epoch_state_t epoch = {
    .global_epoch = ATOMIC_VAR_INIT(0),
    .thread_count = 0
};

/* Reader enters critical section */
static inline void rcu_read_lock(int tid) {
    atomic_store(&epoch.thread_epoch[tid], 
                 atomic_load(&epoch.global_epoch));
    atomic_thread_fence(memory_order_seq_cst);
}

/* Reader exits critical section */
static inline void rcu_read_unlock(int tid) {
    atomic_store(&epoch.thread_epoch[tid], UINT64_MAX);
}

/* Writer waits for all readers */
static void synchronize_rcu(void) {
    uint64_t target = atomic_fetch_add(&epoch.global_epoch, 1) + 1;
    
    /* Wait for all threads to either:
     * - Be outside critical section (epoch = MAX)
     * - Have entered critical section AFTER our update */
    for (int i = 0; i < epoch.thread_count; i++) {
        while (atomic_load(&epoch.thread_epoch[i]) < target) {
            /* Spin or yield */
            sched_yield();
        }
    }
}

/*
 * ============================================================
 * EXAMPLE USAGE: Configuration Hot-Reload
 * ============================================================
 */

struct config {
    char server[256];
    int port;
    int timeout;
};

_Atomic(struct config *) current_config = NULL;

/* Reader: Get current config (fast path) */
struct config *get_config(int tid) {
    struct config *cfg;
    
    rcu_read_lock(tid);
    cfg = atomic_load(&current_config);
    /* Caller must copy if needed beyond this scope */
    return cfg;  /* Still in critical section! */
}

void release_config(int tid) {
    rcu_read_unlock(tid);
}

/* Writer: Update config (slow path) */
void update_config(struct config *new_cfg) {
    struct config *old = atomic_exchange(&current_config, new_cfg);
    
    if (old) {
        synchronize_rcu();  /* Wait for readers */
        free(old);          /* Safe to free */
    }
}
```

### 6.3 Pointer Swapping Pattern

```c
/*
 * Atomic pointer swap with RCU protection
 */

#include <stdatomic.h>
#include <pthread.h>

struct data {
    int value;
    char name[64];
};

_Atomic(struct data *) shared_data = NULL;
pthread_mutex_t writer_lock = PTHREAD_MUTEX_INITIALIZER;

/* Thread-local epoch for readers */
__thread int my_tid = -1;

/*
 * READER: Fast, lock-free access
 */
void reader_example(void) {
    struct data *ptr;
    int value;
    char name[64];
    
    rcu_read_lock(my_tid);
    
    ptr = atomic_load(&shared_data);
    if (ptr) {
        /* Copy data while protected */
        value = ptr->value;
        strncpy(name, ptr->name, sizeof(name));
    }
    
    rcu_read_unlock(my_tid);
    
    /* Use copied data outside critical section */
    printf("Value: %d, Name: %s\n", value, name);
}

/*
 * WRITER: Coordinated update with grace period
 */
void writer_example(int new_value, const char *new_name) {
    struct data *new_data = malloc(sizeof(*new_data));
    struct data *old_data;
    
    new_data->value = new_value;
    strncpy(new_data->name, new_name, sizeof(new_data->name));
    
    /* Coordinate with other writers */
    pthread_mutex_lock(&writer_lock);
    
    /* Atomic swap */
    old_data = atomic_exchange(&shared_data, new_data);
    
    pthread_mutex_unlock(&writer_lock);
    
    /* Wait for readers and free */
    if (old_data) {
        synchronize_rcu();
        free(old_data);
    }
}
```

### 6.4 User-Space RCU Decision Checklist

```
+------------------------------------------------------------------------+
|                    RCU APPLICABILITY CHECKLIST                          |
+------------------------------------------------------------------------+

STEP 1: Is it read-heavy?
    [ ] Reads significantly outnumber writes (10:1 or higher)
    [ ] Write frequency is low enough to tolerate waiting
    [ ] Read performance is critical
    
    If NO to all → Use traditional locking

STEP 2: Can readers tolerate staleness?
    [ ] Brief inconsistency is acceptable
    [ ] Readers don't need "latest" value
    [ ] Eventually consistent is okay
    
    If NO → Use traditional locking

STEP 3: Is data pointer-based?
    [ ] Data accessed via pointer indirection
    [ ] Can atomically swap pointers
    [ ] Data is self-contained (no external refs)
    
    If NO → May need different approach

STEP 4: Choose RCU variant

    +-------------------------------------------------+
    | Constraint            | Variant                 |
    |-----------------------|-------------------------|
    | Cannot block          | call_rcu() / epoch      |
    | Simple code           | synchronize_rcu()       |
    | User-space            | liburcu / epoch-based   |
    | Performance critical  | QSBR                    |
    +-------------------------------------------------+

STEP 5: Verify correctness
    [ ] Readers only access via rcu_dereference()
    [ ] Readers don't hold pointers beyond unlock
    [ ] Readers don't modify RCU-protected data
    [ ] Writers use rcu_assign_pointer()
    [ ] Writers wait for grace period before freeing
    [ ] Writers coordinate with each other
```

### 6.5 Complete User-Space Example

```c
/*
 * user_rcu_demo.c - Complete user-space RCU demonstration
 *
 * Compile: gcc -pthread -o rcu_demo user_rcu_demo.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>
#include <unistd.h>

/*
 * ============================================================
 * SIMPLE EPOCH-BASED RCU
 * ============================================================
 */

#define MAX_THREADS 16
#define EPOCH_INACTIVE UINT64_MAX

typedef struct {
    _Atomic uint64_t global_epoch;
    _Atomic uint64_t thread_epochs[MAX_THREADS];
} rcu_state_t;

static rcu_state_t rcu = {
    .global_epoch = ATOMIC_VAR_INIT(0),
};

static void rcu_init_thread(int tid) {
    atomic_store(&rcu.thread_epochs[tid], EPOCH_INACTIVE);
}

static void rcu_read_lock(int tid) {
    uint64_t epoch = atomic_load_explicit(&rcu.global_epoch, 
                                          memory_order_acquire);
    atomic_store_explicit(&rcu.thread_epochs[tid], epoch,
                          memory_order_release);
}

static void rcu_read_unlock(int tid) {
    atomic_store_explicit(&rcu.thread_epochs[tid], EPOCH_INACTIVE,
                          memory_order_release);
}

static void synchronize_rcu(void) {
    uint64_t current = atomic_fetch_add(&rcu.global_epoch, 1);
    uint64_t target = current + 1;
    
    for (int i = 0; i < MAX_THREADS; i++) {
        uint64_t e;
        while ((e = atomic_load(&rcu.thread_epochs[i])) != EPOCH_INACTIVE 
               && e < target) {
            sched_yield();
        }
    }
}

/*
 * ============================================================
 * APPLICATION: Route Table with Hot-Reload
 * ============================================================
 */

struct route_entry {
    char destination[32];
    char gateway[32];
    int metric;
};

struct route_table {
    struct route_entry *entries;
    int count;
};

_Atomic(struct route_table *) current_routes = NULL;
pthread_mutex_t route_lock = PTHREAD_MUTEX_INITIALIZER;

/* Reader: Lookup route (FAST PATH) */
int lookup_route(int tid, const char *dest, char *gateway_out) {
    struct route_table *table;
    int found = 0;
    
    rcu_read_lock(tid);
    
    table = atomic_load(&current_routes);
    if (table) {
        for (int i = 0; i < table->count; i++) {
            if (strcmp(table->entries[i].destination, dest) == 0) {
                strcpy(gateway_out, table->entries[i].gateway);
                found = 1;
                break;
            }
        }
    }
    
    rcu_read_unlock(tid);
    return found;
}

/* Writer: Reload routes (SLOW PATH) */
void reload_routes(struct route_entry *new_entries, int count) {
    struct route_table *new_table = malloc(sizeof(*new_table));
    struct route_table *old_table;
    
    new_table->entries = malloc(count * sizeof(struct route_entry));
    memcpy(new_table->entries, new_entries, count * sizeof(struct route_entry));
    new_table->count = count;
    
    pthread_mutex_lock(&route_lock);
    old_table = atomic_exchange(&current_routes, new_table);
    pthread_mutex_unlock(&route_lock);
    
    if (old_table) {
        printf("[Writer] Waiting for grace period...\n");
        synchronize_rcu();
        printf("[Writer] Grace period complete, freeing old table\n");
        free(old_table->entries);
        free(old_table);
    }
}

/*
 * ============================================================
 * DEMO THREADS
 * ============================================================
 */

volatile int running = 1;

void *reader_thread(void *arg) {
    int tid = (int)(long)arg;
    char gateway[32];
    int lookups = 0;
    
    rcu_init_thread(tid);
    
    while (running) {
        if (lookup_route(tid, "192.168.1.0/24", gateway)) {
            lookups++;
        }
        if (lookup_route(tid, "10.0.0.0/8", gateway)) {
            lookups++;
        }
        usleep(100);  /* Simulate work */
    }
    
    printf("[Reader %d] Total lookups: %d\n", tid, lookups);
    return NULL;
}

void *writer_thread(void *arg) {
    struct route_entry routes_v1[] = {
        {"192.168.1.0/24", "192.168.1.1", 10},
        {"10.0.0.0/8", "10.0.0.1", 20},
    };
    
    struct route_entry routes_v2[] = {
        {"192.168.1.0/24", "192.168.1.254", 5},
        {"10.0.0.0/8", "10.0.0.254", 15},
        {"172.16.0.0/12", "172.16.0.1", 25},
    };
    
    sleep(1);
    printf("[Writer] Loading initial routes...\n");
    reload_routes(routes_v1, 2);
    
    sleep(2);
    printf("[Writer] Updating routes...\n");
    reload_routes(routes_v2, 3);
    
    sleep(2);
    running = 0;
    
    return NULL;
}

int main(void) {
    pthread_t readers[4], writer;
    
    printf("=== User-Space RCU Demo ===\n\n");
    
    for (int i = 0; i < 4; i++) {
        pthread_create(&readers[i], NULL, reader_thread, (void *)(long)i);
    }
    pthread_create(&writer, NULL, writer_thread, NULL);
    
    pthread_join(writer, NULL);
    for (int i = 0; i < 4; i++) {
        pthread_join(readers[i], NULL);
    }
    
    printf("\n=== Demo Complete ===\n");
    return 0;
}
```

---

## Summary

### RCU Core Concepts

```
+------------------------------------------------------------------------+
|                    RCU MENTAL MODEL SUMMARY                             |
+------------------------------------------------------------------------+

1. SEPARATION OF CONCERNS
   - Readers: Zero-overhead access, no waiting
   - Writers: Pay cost of waiting for grace period
   
2. DEFERRED RECLAMATION
   - Never free immediately after update
   - Wait for all old readers to finish
   - Then safe to reclaim
   
3. PUBLICATION-SUBSCRIPTION
   - rcu_assign_pointer(): Publish new version (with barrier)
   - rcu_dereference(): Subscribe to current version (with barrier)
   
4. GRACE PERIOD
   - Time between call_rcu() and callback execution
   - All pre-existing readers complete during this time
   - New readers see new data
```

### Decision Framework

```
+------------------------------------------------------------------------+
|                    WHEN TO USE RCU                                      |
+------------------------------------------------------------------------+

USE RCU WHEN:
    ✓ Reads >> Writes (10:1 or higher)
    ✓ Readers can tolerate brief staleness
    ✓ Data is pointer-based
    ✓ Read performance is critical
    ✓ Writers can wait for grace period

DO NOT USE RCU WHEN:
    ✗ Writes are frequent
    ✗ Readers need latest value immediately
    ✗ Cannot defer freeing (strict memory limit)
    ✗ Data is not pointer-based
    ✗ Simpler locking suffices
```

### API Quick Reference

| API | Purpose | Context |
|-----|---------|---------|
| `rcu_read_lock()` | Enter read-side critical section | Cannot sleep |
| `rcu_read_unlock()` | Exit read-side critical section | Must pair |
| `rcu_dereference()` | Fetch protected pointer | Inside lock |
| `rcu_assign_pointer()` | Publish new pointer | Anytime |
| `synchronize_rcu()` | Wait for grace period | Can sleep |
| `call_rcu()` | Schedule deferred callback | Anytime |

These patterns have enabled Linux to achieve unprecedented performance on read-heavy workloads, from routing tables processing millions of lookups per second to VFS pathnames traversed on every file operation.

