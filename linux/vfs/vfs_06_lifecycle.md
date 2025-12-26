# VFS Architecture Study: Lifecycle Management and Implicit State Machines

## 1. File Lifecycle

```
+------------------------------------------------------------------+
|  struct file LIFECYCLE                                           |
+------------------------------------------------------------------+

    ┌─────────┐     open()      ┌─────────┐
    │ (none)  │ ───────────────►│  OPEN   │
    └─────────┘                 └────┬────┘
                                     │
                            read/write/mmap/ioctl
                                     │
                                     ▼
                                ┌─────────┐
                                │  IN USE │ ◄─── f_count > 1
                                └────┬────┘
                                     │
                               close() / fput()
                                     │
                                     ▼
                       f_count == 0? ───► ┌─────────────┐
                                          │  RELEASING  │
                                          └──────┬──────┘
                                                 │
                                        f_op->release()
                                                 │
                                                 ▼
                                          ┌─────────┐
                                          │  FREED  │
                                          └─────────┘
```

### State Encoding

```c
/* State is encoded in f_count (reference count) */

struct file {
    atomic_long_t   f_count;    /* Reference count */
    /* ... */
};

/* State transitions via refcount operations */

/* OPEN: f_count = 1 */
struct file *get_empty_filp(void) {
    struct file *f = kmem_cache_alloc(filp_cachep, GFP_KERNEL);
    atomic_long_set(&f->f_count, 1);  /* Initial reference */
    return f;
}

/* IN USE: f_count > 1 (multiple references) */
void get_file(struct file *f) {
    atomic_long_inc(&f->f_count);
}

/* RELEASING: f_count reaches 0 */
void fput(struct file *file) {
    if (atomic_long_dec_and_test(&file->f_count)) {
        /* Transition to RELEASED */
        __fput(file);  /* Calls f_op->release */
    }
}
```

---

## 2. Inode Lifecycle

```
+------------------------------------------------------------------+
|  struct inode LIFECYCLE                                          |
+------------------------------------------------------------------+

    ┌───────────────────────────────────────────────────────────────┐
    │                                                               │
    │  ┌─────────┐  alloc    ┌─────────┐                           │
    │  │ (none)  │ ─────────►│   NEW   │ ◄─── I_NEW flag set      │
    │  └─────────┘           └────┬────┘                           │
    │                              │                                │
    │                       unlock_new_inode()                      │
    │                              │                                │
    │                              ▼                                │
    │                        ┌─────────┐                           │
    │                        │  VALID  │ ◄─── I_NEW cleared        │
    │                        └────┬────┘                           │
    │                              │                                │
    │                    modification (write)                       │
    │                              │                                │
    │                              ▼                                │
    │                        ┌─────────┐                           │
    │                        │  DIRTY  │ ◄─── I_DIRTY_* flags     │
    │                        └────┬────┘                           │
    │                              │                                │
    │                      writeback / sync                         │
    │                              │                                │
    │                              ▼                                │
    │                        ┌─────────┐                           │
    │  ◄───────────────────  │  CLEAN  │                           │
    │  i_count > 0:          └────┬────┘                           │
    │  still in use               │                                │
    │                        i_count → 0                           │
    │                              │                                │
    │                              ▼                                │
    │                        ┌─────────┐                           │
    │                        │   LRU   │ ◄─── On unused list       │
    │                        └────┬────┘                           │
    │                              │                                │
    │                        memory pressure                        │
    │                              │                                │
    │                              ▼                                │
    │                        ┌─────────┐                           │
    │                        │EVICTING │ ◄─── I_FREEING flag       │
    │                        └────┬────┘                           │
    │                              │                                │
    │                     s_op->evict_inode()                       │
    │                              │                                │
    │                              ▼                                │
    │                        ┌─────────┐                           │
    │                        │  FREED  │                           │
    │                        └─────────┘                           │
    │                                                               │
    └───────────────────────────────────────────────────────────────┘
```

### Inode State Flags (from `fs.h` lines 1740-1752)

```c
/* Inode state bits - Protected by inode->i_lock */

#define I_DIRTY_SYNC        (1 << 0)  /* Metadata needs sync */
#define I_DIRTY_DATASYNC    (1 << 1)  /* Data needs sync */
#define I_DIRTY_PAGES       (1 << 2)  /* Has dirty pages */
#define I_NEW               (1 << 3)  /* Being initialized */
#define I_WILL_FREE         (1 << 4)  /* About to be freed */
#define I_FREEING           (1 << 5)  /* Being freed */
#define I_CLEAR             (1 << 6)  /* Cleanup complete */
#define I_SYNC              (1 << 7)  /* Syncing in progress */
#define I_REFERENCED        (1 << 8)  /* Recently accessed */

#define I_DIRTY (I_DIRTY_SYNC | I_DIRTY_DATASYNC | I_DIRTY_PAGES)
```

### State Transitions

```c
/* fs/inode.c */

/* NEW → VALID */
void unlock_new_inode(struct inode *inode)
{
    spin_lock(&inode->i_lock);
    inode->i_state &= ~I_NEW;  /* Clear NEW flag */
    wake_up_bit(&inode->i_state, __I_NEW);  /* Wake waiters */
    spin_unlock(&inode->i_lock);
}

/* VALID → DIRTY */
void __mark_inode_dirty(struct inode *inode, int flags)
{
    spin_lock(&inode->i_lock);
    if ((inode->i_state & flags) != flags) {
        inode->i_state |= flags;  /* Set dirty flags */
        /* Add to writeback list */
    }
    spin_unlock(&inode->i_lock);
}

/* DIRTY → CLEAN (writeback) */
int write_inode_now(struct inode *inode, int sync)
{
    /* Write inode metadata */
    if (inode->i_sb->s_op->write_inode)
        ret = inode->i_sb->s_op->write_inode(inode, &wbc);
    /* Clear dirty flags */
}

/* VALID/CLEAN → LRU */
void iput(struct inode *inode)
{
    if (atomic_dec_and_lock(&inode->i_count, &inode->i_lock)) {
        /* i_count reached 0 */
        /* Move to LRU or free immediately */
    }
}

/* LRU → FREEING → FREED */
static void evict(struct inode *inode)
{
    inode->i_state |= I_FREEING;  /* Mark freeing */
    if (S_ISREG(inode->i_mode))
        truncate_inode_pages(&inode->i_data, 0);
    
    /* Call filesystem cleanup */
    if (inode->i_sb->s_op->evict_inode)
        inode->i_sb->s_op->evict_inode(inode);
    
    inode->i_state |= I_CLEAR;  /* Cleanup complete */
    destroy_inode(inode);
}
```

---

## 3. Dentry Lifecycle

```
+------------------------------------------------------------------+
|  struct dentry LIFECYCLE                                         |
+------------------------------------------------------------------+

    ┌─────────┐  d_alloc     ┌────────────┐
    │ (none)  │ ────────────►│ ALLOCATED  │ d_inode = NULL
    └─────────┘              └─────┬──────┘
                                   │
                          d_instantiate(inode)
                                   │
                                   ▼
                             ┌──────────┐
                             │ POSITIVE │ d_inode != NULL, d_count > 0
                             └────┬─────┘
                                  │
                            dput() when d_count → 0
                                  │
                                  ▼
                             ┌──────────┐
                             │ UNUSED   │ On LRU, d_count = 0
                             └────┬─────┘
                                  │
                    ┌─────────────┼────────────────┐
                    │             │                │
               Reused by    Memory pressure   File deleted
               d_lookup()        │                │
                    │             │                │
                    ▼             ▼                ▼
              ┌──────────┐  ┌──────────┐    ┌──────────┐
              │ POSITIVE │  │  SHRUNK  │    │ NEGATIVE │
              └──────────┘  └────┬─────┘    └──────────┘
                                 │         d_inode = NULL
                                 │         (cached non-existence)
                                 ▼
                            ┌──────────┐
                            │  FREED   │
                            └──────────┘

    NEGATIVE DENTRY:
    ┌─────────────────────────────────────────────────────────────┐
    │  Purpose: Cache "file does not exist" lookups              │
    │  Example: stat("/nonexistent") → ENOENT                    │
    │           Creates negative dentry for "/nonexistent"       │
    │           Next stat("/nonexistent") hits cache             │
    └─────────────────────────────────────────────────────────────┘
```

---

## 4. Superblock Lifecycle

```
+------------------------------------------------------------------+
|  struct super_block LIFECYCLE                                    |
+------------------------------------------------------------------+

    ┌─────────┐   mount()    ┌─────────────┐
    │ (none)  │ ────────────►│  MOUNTING   │
    └─────────┘              └──────┬──────┘
                                    │
                           fill_super() success
                                    │
                                    ▼
                             ┌─────────────┐
                             │   ACTIVE    │ s_active > 0
                             └──────┬──────┘
                                    │
                                  sync
                                    │
                                    ▼
                             ┌─────────────┐
                             │  SYNCING    │ I_SYNC in inodes
                             └──────┬──────┘
                                    │
                                    ▼
                             ┌─────────────┐
                             │   ACTIVE    │
                             └──────┬──────┘
                                    │
                              umount()
                                    │
                                    ▼
                             ┌─────────────┐
                             │  UNMOUNTING │ s_active → 0
                             └──────┬──────┘
                                    │
                           kill_sb() callback
                                    │
                                    ▼
                             ┌─────────────┐
                             │    FREED    │
                             └─────────────┘
```

---

## 5. State Encoding Patterns

```
+------------------------------------------------------------------+
|  HOW VFS ENCODES STATE                                           |
+------------------------------------------------------------------+

    PATTERN 1: Reference Counts
    ┌─────────────────────────────────────────────────────────────┐
    │  f_count > 0  →  File is open and in use                   │
    │  i_count > 0  →  Inode is referenced                       │
    │  d_count > 0  →  Dentry is referenced                      │
    │  s_active > 0 →  Superblock has active mounts              │
    │                                                              │
    │  Transitions happen atomically on count changes            │
    └─────────────────────────────────────────────────────────────┘

    PATTERN 2: Flags/State Fields
    ┌─────────────────────────────────────────────────────────────┐
    │  i_state flags:  I_NEW, I_DIRTY, I_FREEING, I_SYNC        │
    │  dentry flags:   DCACHE_DISCONNECTED, DCACHE_REFERENCED   │
    │  file flags:     f_flags (O_RDONLY, O_NONBLOCK, etc.)     │
    │                                                              │
    │  Multiple flags can be set simultaneously                  │
    │  Protected by spinlock                                      │
    └─────────────────────────────────────────────────────────────┘

    PATTERN 3: List Membership
    ┌─────────────────────────────────────────────────────────────┐
    │  On sb->s_inodes    →  Inode belongs to this superblock   │
    │  On i_sb_list       →  Inode is active                    │
    │  On s_dentry_lru    →  Dentry is unused (evictable)       │
    │  On s_inode_lru     →  Inode is unused (evictable)        │
    │                                                              │
    │  List add/remove = state transition                        │
    └─────────────────────────────────────────────────────────────┘

    PATTERN 4: Pointer State
    ┌─────────────────────────────────────────────────────────────┐
    │  d_inode == NULL    →  Negative dentry                     │
    │  d_inode != NULL    →  Positive dentry                     │
    │  i_mapping != NULL  →  Has associated address_space       │
    │  f_path.dentry != NULL → File has valid path              │
    └─────────────────────────────────────────────────────────────┘
```

---

## 6. Who Triggers Transitions

```
+------------------------------------------------------------------+
|  STATE TRANSITION TRIGGERS                                       |
+------------------------------------------------------------------+

    FILE TRANSITIONS:
    ┌─────────────────────────────────────────────────────────────┐
    │  (none) → OPEN:     do_sys_open() / sys_socket()           │
    │  OPEN → IN USE:     fget() from another context            │
    │  IN USE → RELEASE:  fput() when last ref dropped           │
    │  RELEASE → FREED:   __fput() via task_work or direct       │
    └─────────────────────────────────────────────────────────────┘

    INODE TRANSITIONS:
    ┌─────────────────────────────────────────────────────────────┐
    │  (none) → NEW:      iget_locked() / new_inode()            │
    │  NEW → VALID:       unlock_new_inode() after FS init       │
    │  VALID → DIRTY:     __mark_inode_dirty() on modification   │
    │  DIRTY → CLEAN:     writeback thread / sync                │
    │  VALID → LRU:       iput() when i_count → 0                │
    │  LRU → FREEING:     prune_icache() under memory pressure   │
    └─────────────────────────────────────────────────────────────┘

    DENTRY TRANSITIONS:
    ┌─────────────────────────────────────────────────────────────┐
    │  (none) → ALLOC:    d_alloc() during lookup                │
    │  ALLOC → POSITIVE:  d_instantiate() when inode found       │
    │  ALLOC → NEGATIVE:  d_add(dentry, NULL) when not found     │
    │  POSITIVE → LRU:    dput() when d_count → 0                │
    │  LRU → SHRUNK:      prune_dcache() under memory pressure   │
    └─────────────────────────────────────────────────────────────┘

    SUPERBLOCK TRANSITIONS:
    ┌─────────────────────────────────────────────────────────────┐
    │  (none) → MOUNT:    mount() syscall                        │
    │  MOUNT → ACTIVE:    fill_super() success                   │
    │  ACTIVE → UMOUNT:   umount() when s_active → 0             │
    │  UMOUNT → FREED:    kill_sb() cleanup                      │
    └─────────────────────────────────────────────────────────────┘
```

---

## 7. Implicit vs Explicit State Machines

```
+------------------------------------------------------------------+
|  VFS STATE MACHINES ARE IMPLICIT                                 |
+------------------------------------------------------------------+

    EXPLICIT STATE MACHINE (not used in VFS):
    ┌─────────────────────────────────────────────────────────────┐
    │  enum file_state { STATE_NEW, STATE_OPEN, STATE_CLOSED };  │
    │                                                              │
    │  struct file {                                              │
    │      enum file_state state;                                 │
    │  };                                                         │
    │                                                              │
    │  void file_close(struct file *f) {                         │
    │      if (f->state != STATE_OPEN)                           │
    │          return -EINVAL;                                    │
    │      f->state = STATE_CLOSED;                              │
    │      ...                                                    │
    │  }                                                          │
    └─────────────────────────────────────────────────────────────┘

    IMPLICIT STATE MACHINE (VFS pattern):
    ┌─────────────────────────────────────────────────────────────┐
    │  struct file {                                              │
    │      atomic_long_t f_count;  /* State encoded in refcount */│
    │  };                                                         │
    │                                                              │
    │  void fput(struct file *f) {                               │
    │      if (atomic_long_dec_and_test(&f->f_count)) {          │
    │          /* Implicit transition: IN_USE → RELEASING */     │
    │          __fput(f);                                         │
    │      }                                                       │
    │  }                                                          │
    │                                                              │
    │  /* State is inferred from refcount value */               │
    │  /* No explicit state field needed */                      │
    └─────────────────────────────────────────────────────────────┘

    WHY IMPLICIT?
    ┌─────────────────────────────────────────────────────────────┐
    │  • Refcount already tracks "in use" state                  │
    │  • Avoids redundant state + refcount synchronization       │
    │  • Atomic operations on single field (faster)              │
    │  • State transitions are automatic on refcount changes     │
    └─────────────────────────────────────────────────────────────┘
```

---

## Summary

```
+------------------------------------------------------------------+
|  LIFECYCLE MANAGEMENT SUMMARY                                    |
+------------------------------------------------------------------+

    CORE OBJECTS AND THEIR LIFECYCLES:
    • file:       OPEN → IN_USE → RELEASING → FREED
    • inode:      NEW → VALID → DIRTY → CLEAN → LRU → FREED
    • dentry:     ALLOCATED → POSITIVE/NEGATIVE → LRU → FREED
    • superblock: MOUNTING → ACTIVE → UNMOUNTING → FREED

    STATE ENCODING PATTERNS:
    • Reference counts (primary state indicator)
    • Flags/state fields (secondary states like DIRTY)
    • List membership (LRU, dirty lists)
    • Pointer nullness (positive vs negative dentry)

    TRANSITION TRIGGERS:
    • User actions (open, close, mount, umount)
    • Kernel subsystems (writeback, cache shrinker)
    • Reference counting (automatic cleanup)

    IMPLICIT STATE MACHINES:
    • State encoded in refcount values and flags
    • No explicit enum state field
    • Transitions happen via refcount operations
    • Simplifies synchronization
```

**中文总结：**
- **四个核心对象的生命周期**：file、inode、dentry、superblock
- **状态编码方式**：引用计数、标志位、列表成员、指针空值
- **状态转换触发者**：用户操作、内核子系统、引用计数变化
- **隐式状态机**：状态编码在引用计数和标志中，而非显式枚举

