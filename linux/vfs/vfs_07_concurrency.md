# VFS Architecture Study: Concurrency, Safety, and Performance

## 1. VFS Locking Hierarchy

```
+------------------------------------------------------------------+
|  VFS LOCKING HIERARCHY (from coarse to fine)                     |
+------------------------------------------------------------------+

    Level 1: Global Locks
    ┌─────────────────────────────────────────────────────────────┐
    │  file_systems_lock (rwlock)  – protects fs_type list       │
    │  mount_lock (seqlock)        – protects mount tree         │
    │  bdev_lock (spinlock)        – protects block device list  │
    └─────────────────────────────────────────────────────────────┘
                    │
                    ▼
    Level 2: Superblock Locks
    ┌─────────────────────────────────────────────────────────────┐
    │  sb->s_umount (rw_semaphore) – mount/umount serialization  │
    │  sb->s_lock (spinlock)       – superblock fields           │
    └─────────────────────────────────────────────────────────────┘
                    │
                    ▼
    Level 3: Inode Locks
    ┌─────────────────────────────────────────────────────────────┐
    │  inode->i_mutex (mutex)      – directory operations        │
    │  inode->i_lock (spinlock)    – inode state and lists       │
    │  inode->i_rwsem (rw_sem)     – file content access         │
    └─────────────────────────────────────────────────────────────┘
                    │
                    ▼
    Level 4: Dentry Locks
    ┌─────────────────────────────────────────────────────────────┐
    │  dentry->d_lock (spinlock)   – dentry fields and lists     │
    │  d_inode->i_lock             – via dentry→inode            │
    └─────────────────────────────────────────────────────────────┘
                    │
                    ▼
    Level 5: File Locks
    ┌─────────────────────────────────────────────────────────────┐
    │  file->f_lock (spinlock)     – file position, flags        │
    │  file->f_pos_lock (mutex)    – sequential read/write       │
    └─────────────────────────────────────────────────────────────┘
                    │
                    ▼
    Level 6: Page Locks
    ┌─────────────────────────────────────────────────────────────┐
    │  page->flags (PG_locked)     – individual page protection  │
    │  address_space->tree_lock    – radix tree operations       │
    └─────────────────────────────────────────────────────────────┘
```

---

## 2. Locking Strategies by Operation Type

```
+------------------------------------------------------------------+
|  OPERATION-SPECIFIC LOCKING                                      |
+------------------------------------------------------------------+

    READ PATH (sys_read):
    ┌─────────────────────────────────────────────────────────────┐
    │  1. fget_light()     – RCU or refcount (no lock)           │
    │  2. f_pos_lock       – mutex for position update           │
    │  3. i_rwsem          – read lock (shared)                  │
    │  4. page lock        – individual pages as needed          │
    │                                                              │
    │  Total locks held: 1-2 (minimal contention)                │
    └─────────────────────────────────────────────────────────────┘

    WRITE PATH (sys_write):
    ┌─────────────────────────────────────────────────────────────┐
    │  1. fget_light()     – RCU or refcount                     │
    │  2. f_pos_lock       – mutex for position update           │
    │  3. i_rwsem          – write lock (exclusive)              │
    │  4. i_mutex          – if extending file                   │
    │  5. page lock        – for each dirty page                 │
    │                                                              │
    │  Total locks held: 2-4 (more contention possible)          │
    └─────────────────────────────────────────────────────────────┘

    DIRECTORY LOOKUP (sys_open pathname resolution):
    ┌─────────────────────────────────────────────────────────────┐
    │  1. mount_lock       – seqlock read for mount points       │
    │  2. d_lock           – spinlock per dentry component       │
    │  3. i_mutex          – only if creating/removing entry     │
    │                                                              │
    │  RCU-walk: No locks at all! (fast path)                    │
    │  REF-walk: d_lock per component (fallback)                 │
    └─────────────────────────────────────────────────────────────┘

    DIRECTORY MODIFICATION (mkdir, unlink):
    ┌─────────────────────────────────────────────────────────────┐
    │  1. parent->i_mutex  – exclusive access to directory       │
    │  2. child->i_mutex   – for rename operations               │
    │  3. d_lock           – dentry list manipulation            │
    │                                                              │
    │  Lock ordering: Parent before child, alphabetical order    │
    └─────────────────────────────────────────────────────────────┘
```

---

## 3. RCU in VFS

```
+------------------------------------------------------------------+
|  RCU-PROTECTED VFS STRUCTURES                                    |
+------------------------------------------------------------------+

    RCU-WALK PATH LOOKUP (fast path):
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │  sys_open("/home/user/file")                                │
    │                                                              │
    │    rcu_read_lock()                                          │
    │    ┌──────────────────────────────────────────────────────┐ │
    │    │  d_lookup_rcu("home")  ──► dentry (no lock!)         │ │
    │    │  d_lookup_rcu("user")  ──► dentry (no lock!)         │ │
    │    │  d_lookup_rcu("file")  ──► dentry (no lock!)         │ │
    │    │                                                        │ │
    │    │  At each step:                                         │ │
    │    │    • Read dentry via RCU                              │ │
    │    │    • Validate sequence counter                        │ │
    │    │    • If invalid: drop to REF-walk                     │ │
    │    └──────────────────────────────────────────────────────┘ │
    │    rcu_read_unlock()                                        │
    │                                                              │
    │  Result: Zero locks for successful lookup!                  │
    └─────────────────────────────────────────────────────────────┘

    WHY RCU WORKS FOR DENTRIES:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Dentry removal is rare (most lookups find existing)     │
    │  • Readers vastly outnumber writers                        │
    │  • Short critical sections (quick traverse)                │
    │  • Fallback path (REF-walk) handles races                  │
    └─────────────────────────────────────────────────────────────┘

    RCU FILE DESCRIPTOR LOOKUP:
    ┌─────────────────────────────────────────────────────────────┐
    │  /* Fast path: RCU lookup without locks */                 │
    │  struct file *fget_light(unsigned int fd, int *fput_needed)│
    │  {                                                          │
    │      struct files_struct *files = current->files;          │
    │      struct file *file;                                     │
    │                                                              │
    │      rcu_read_lock();                                       │
    │      file = fcheck_files(files, fd);                        │
    │      if (file) {                                            │
    │          if (atomic_long_inc_not_zero(&file->f_count)) {   │
    │              *fput_needed = 1;                              │
    │          } else {                                           │
    │              file = NULL;  /* Race: file being closed */   │
    │          }                                                   │
    │      }                                                       │
    │      rcu_read_unlock();                                     │
    │      return file;                                           │
    │  }                                                          │
    └─────────────────────────────────────────────────────────────┘
```

---

## 4. Refcounting vs Locking Responsibilities

```
+------------------------------------------------------------------+
|  REFCOUNT AND LOCK RESPONSIBILITIES                              |
+------------------------------------------------------------------+

    WHAT REFCOUNTS PROTECT:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Object existence (memory not freed)                     │
    │  • Safe to access via valid pointer                        │
    │  • Object won't be deallocated while held                  │
    │                                                              │
    │  REFCOUNT DOES NOT PROTECT:                                 │
    │  • Concurrent modification of object fields                │
    │  • Ordering of operations                                   │
    │  • Atomicity of multi-field updates                        │
    └─────────────────────────────────────────────────────────────┘

    WHAT LOCKS PROTECT:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Field consistency (atomic multi-field updates)          │
    │  • Operation ordering (serialization)                      │
    │  • Invariant maintenance (e.g., list consistency)          │
    │                                                              │
    │  LOCKS DO NOT PROTECT:                                      │
    │  • Object lifetime (object might be freed after unlock)    │
    └─────────────────────────────────────────────────────────────┘

    COMBINED USAGE PATTERN:
    ┌─────────────────────────────────────────────────────────────┐
    │  /* Step 1: Get reference (guarantees existence) */        │
    │  inode = iget(sb, ino);  /* i_count++ */                   │
    │  if (!inode)                                                │
    │      return -ENOENT;                                        │
    │                                                              │
    │  /* Step 2: Lock for exclusive access to fields */         │
    │  mutex_lock(&inode->i_mutex);                              │
    │                                                              │
    │  /* Step 3: Modify fields safely */                        │
    │  inode->i_size = new_size;                                 │
    │  inode->i_mtime = CURRENT_TIME;                            │
    │                                                              │
    │  /* Step 4: Unlock */                                       │
    │  mutex_unlock(&inode->i_mutex);                            │
    │                                                              │
    │  /* Step 5: Release reference */                           │
    │  iput(inode);  /* i_count-- */                             │
    └─────────────────────────────────────────────────────────────┘

    WHO OWNS WHAT:
    ┌────────────────────────────────────────────────────────────────┐
    │  Object      │  Refcount Field  │  Existence Lock  │  Data Lock │
    │─────────────────────────────────────────────────────────────────│
    │  file        │  f_count         │  (none)          │  f_lock    │
    │  inode       │  i_count         │  i_lock          │  i_mutex   │
    │  dentry      │  d_count         │  d_lock          │  (inode)   │
    │  super_block │  s_active        │  s_umount        │  s_lock    │
    └────────────────────────────────────────────────────────────────┘
```

---

## 5. Performance Considerations

```
+------------------------------------------------------------------+
|  VFS PERFORMANCE OPTIMIZATIONS                                   |
+------------------------------------------------------------------+

    OPTIMIZATION 1: RCU-Walk Path Lookup
    ┌─────────────────────────────────────────────────────────────┐
    │  Problem:  Lock per path component = N locks for N-deep path│
    │  Solution: RCU read-side with seqcount validation          │
    │  Result:   Zero locks in common case                       │
    │                                                              │
    │  Benchmark: 3-10x faster than REF-walk for deep paths      │
    └─────────────────────────────────────────────────────────────┘

    OPTIMIZATION 2: Per-CPU Dentry/Inode Caches
    ┌─────────────────────────────────────────────────────────────┐
    │  Problem:  Global LRU list = lock contention               │
    │  Solution: Per-CPU partial caches + batched operations     │
    │  Result:   Reduced global lock traffic                     │
    └─────────────────────────────────────────────────────────────┘

    OPTIMIZATION 3: Lightweight File Reference (fget_light)
    ┌─────────────────────────────────────────────────────────────┐
    │  Problem:  fget() takes files->file_lock every time        │
    │  Solution: RCU + atomic increment without lock             │
    │  Result:   Lockless fast path for single-threaded case     │
    └─────────────────────────────────────────────────────────────┘

    OPTIMIZATION 4: Read-Write Separation
    ┌─────────────────────────────────────────────────────────────┐
    │  Problem:  Readers block each other with exclusive locks   │
    │  Solution: rw_semaphore for i_rwsem (multiple readers)     │
    │  Result:   Parallel reads to same file                     │
    └─────────────────────────────────────────────────────────────┘

    OPTIMIZATION 5: Lock Elision for Uncontended Cases
    ┌─────────────────────────────────────────────────────────────┐
    │  Problem:  Mutex overhead even when no contention          │
    │  Solution: Fast path checks before locking                 │
    │  Example:  Check f_count before taking lock                │
    └─────────────────────────────────────────────────────────────┘
```

---

## 6. Common Failure Modes

```
+------------------------------------------------------------------+
|  VFS CONCURRENCY BUGS                                            |
+------------------------------------------------------------------+

    BUG 1: Use-After-Free
    ┌─────────────────────────────────────────────────────────────┐
    │  WRONG:                                                     │
    │    inode = d_inode(dentry);  /* Get inode pointer */       │
    │    dput(dentry);             /* Release dentry */          │
    │    use(inode);               /* BUG: inode may be freed */ │
    │                                                              │
    │  RIGHT:                                                     │
    │    inode = d_inode(dentry);                                │
    │    ihold(inode);             /* Take own reference */      │
    │    dput(dentry);                                            │
    │    use(inode);               /* Safe: we hold reference */ │
    │    iput(inode);                                             │
    └─────────────────────────────────────────────────────────────┘

    BUG 2: Refcount Leak
    ┌─────────────────────────────────────────────────────────────┐
    │  WRONG:                                                     │
    │    file = fget(fd);                                        │
    │    if (some_condition)                                      │
    │        return -EINVAL;       /* BUG: file refcount leaked */│
    │    fput(file);                                              │
    │                                                              │
    │  RIGHT:                                                     │
    │    file = fget(fd);                                        │
    │    if (some_condition) {                                    │
    │        fput(file);           /* Drop reference */          │
    │        return -EINVAL;                                      │
    │    }                                                        │
    │    fput(file);                                              │
    └─────────────────────────────────────────────────────────────┘

    BUG 3: Lock Order Violation
    ┌─────────────────────────────────────────────────────────────┐
    │  WRONG:                                                     │
    │    mutex_lock(&child->i_mutex);                            │
    │    mutex_lock(&parent->i_mutex);  /* DEADLOCK possible */  │
    │                                                              │
    │  RIGHT:                                                     │
    │    mutex_lock(&parent->i_mutex);  /* Parent first */       │
    │    mutex_lock(&child->i_mutex);                            │
    │                                                              │
    │  Or use lock_rename() for cross-directory operations       │
    └─────────────────────────────────────────────────────────────┘

    BUG 4: Missing Lock Around Invariant
    ┌─────────────────────────────────────────────────────────────┐
    │  WRONG:                                                     │
    │    if (inode->i_size > 0)                                  │
    │        /* Another thread truncates here */                 │
    │        read_data(inode);     /* BUG: size may be 0 now */  │
    │                                                              │
    │  RIGHT:                                                     │
    │    mutex_lock(&inode->i_mutex);                            │
    │    if (inode->i_size > 0)                                  │
    │        read_data(inode);     /* Safe: holding lock */      │
    │    mutex_unlock(&inode->i_mutex);                          │
    └─────────────────────────────────────────────────────────────┘

    BUG 5: Stale Pointer After Sleep
    ┌─────────────────────────────────────────────────────────────┐
    │  WRONG:                                                     │
    │    dentry = lookup(name);                                  │
    │    wait_for_io();            /* May sleep */               │
    │    inode = dentry->d_inode;  /* BUG: dentry may be stale */│
    │                                                              │
    │  RIGHT:                                                     │
    │    dentry = lookup(name);                                  │
    │    dget(dentry);             /* Hold reference */          │
    │    wait_for_io();                                          │
    │    inode = d_inode(dentry);  /* Safe: we hold dentry */   │
    │    dput(dentry);                                            │
    └─────────────────────────────────────────────────────────────┘
```

---

## 7. Trade-offs Summary

```
+------------------------------------------------------------------+
|  CONCURRENCY TRADE-OFFS IN VFS                                   |
+------------------------------------------------------------------+

    FINE-GRAINED LOCKING:
    ┌──────────────────────────┬──────────────────────────────────┐
    │  Pros                    │  Cons                            │
    │─────────────────────────────────────────────────────────────│
    │  High parallelism        │  Complex lock ordering           │
    │  Low contention          │  More lock acquire/release       │
    │  Scales with CPUs        │  Harder to reason about          │
    │                          │  More potential for bugs         │
    └──────────────────────────┴──────────────────────────────────┘

    RCU vs LOCKS:
    ┌──────────────────────────┬──────────────────────────────────┐
    │  RCU                     │  Locks                           │
    │─────────────────────────────────────────────────────────────│
    │  Readers never block     │  Writers can block readers       │
    │  Writers defer work      │  Writers have immediate effect   │
    │  Memory overhead (grace) │  CPU overhead (contention)       │
    │  Best: read-heavy        │  Best: write-heavy               │
    └──────────────────────────┴──────────────────────────────────┘

    REFCOUNT vs LOCK:
    ┌──────────────────────────┬──────────────────────────────────┐
    │  Refcount                │  Lock                            │
    │─────────────────────────────────────────────────────────────│
    │  Protects existence      │  Protects consistency            │
    │  No ordering guarantees  │  Provides ordering               │
    │  Atomic increment only   │  Critical section needed         │
    │  Can't prevent races     │  Prevents races                  │
    └──────────────────────────┴──────────────────────────────────┘

    PERFORMANCE vs CORRECTNESS:
    ┌──────────────────────────┬──────────────────────────────────┐
    │  Aggressive              │  Conservative                    │
    │─────────────────────────────────────────────────────────────│
    │  RCU-walk everywhere     │  REF-walk always                 │
    │  Lockless fast paths     │  Always take locks               │
    │  Complex fallbacks       │  Simple reasoning                │
    │  Higher throughput       │  Lower bug risk                  │
    └──────────────────────────┴──────────────────────────────────┘
```

---

## Summary

```
+------------------------------------------------------------------+
|  VFS CONCURRENCY SUMMARY                                         |
+------------------------------------------------------------------+

    LOCKING HIERARCHY:
    • Global → Superblock → Inode → Dentry → File → Page
    • Always acquire in this order to avoid deadlock

    RCU USAGE:
    • Path lookup (RCU-walk for zero-lock traversal)
    • File descriptor lookup (fget_light)
    • Dentry cache traversal

    REFCOUNT RULES:
    • Refcount guarantees existence, not consistency
    • Always pair get/put operations
    • Take reference before sleeping

    PERFORMANCE PATTERNS:
    • RCU-walk for read-heavy path lookups
    • Read-write locks for shared/exclusive access
    • Per-CPU caches to reduce global contention

    COMMON BUGS:
    • Use-after-free (missing reference)
    • Refcount leaks (missing put on error)
    • Lock order violation (deadlock)
    • Stale pointers after sleep
```

**中文总结：**
- **锁层次结构**：从全局锁到页面锁，必须按顺序获取避免死锁
- **RCU使用**：路径查找、文件描述符查找使用RCU实现无锁快速路径
- **引用计数规则**：保证存在性而非一致性，必须配对get/put
- **常见并发错误**：释放后使用、引用泄漏、锁顺序违反、休眠后指针过期

