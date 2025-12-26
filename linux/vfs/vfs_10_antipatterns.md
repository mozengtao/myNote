# VFS Architecture Study: Anti-Patterns and Pitfalls

## 1. Leaking Abstraction Details

```
+------------------------------------------------------------------+
|  ANTI-PATTERN: LEAKING ABSTRACTION DETAILS                       |
+------------------------------------------------------------------+

    WHAT IT LOOKS LIKE:
    ┌─────────────────────────────────────────────────────────────┐
    │  /* BAD: Framework exposes backend-specific details */     │
    │                                                              │
    │  struct file_ops {                                          │
    │      int (*read)(struct file *f, void *buf, size_t n);     │
    │      int (*read_ext4)(struct file *f, ...);  /* LEAKED! */ │
    │      int (*read_nfs)(struct file *f, ...);   /* LEAKED! */ │
    │  };                                                         │
    │                                                              │
    │  /* Or: framework code checks filesystem type */           │
    │  if (file->fs_type == FS_EXT4) {                           │
    │      handle_ext4_special_case();  /* LEAKED! */            │
    │  }                                                          │
    └─────────────────────────────────────────────────────────────┘

    WHY IT FAILS:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Adding new backend requires changing framework          │
    │  • Framework becomes coupled to backend internals          │
    │  • Generic code becomes a mess of special cases            │
    │  • Impossible to test framework in isolation               │
    │  • Violates Open-Closed Principle                          │
    └─────────────────────────────────────────────────────────────┘

    HOW TO RECOGNIZE:
    ┌─────────────────────────────────────────────────────────────┐
    │  ✗ Framework code mentions specific backend names          │
    │  ✗ switch/if-else on filesystem type in generic code      │
    │  ✗ Backend-specific fields in generic structures          │
    │  ✗ Framework headers include backend headers              │
    └─────────────────────────────────────────────────────────────┘

    HOW VFS AVOIDS THIS:
    ┌─────────────────────────────────────────────────────────────┐
    │  /* GOOD: VFS core NEVER checks filesystem type */        │
    │                                                              │
    │  ssize_t vfs_read(struct file *file, ...) {                │
    │      /* Just dispatch via ops, no type checking */         │
    │      return file->f_op->read(file, ...);                   │
    │  }                                                          │
    │                                                              │
    │  /* Filesystem-specific behavior lives in ops impl */      │
    └─────────────────────────────────────────────────────────────┘

    HOW TO FIX:
    ┌─────────────────────────────────────────────────────────────┐
    │  1. Remove backend-specific code from framework            │
    │  2. Push special cases into backend implementations        │
    │  3. If behavior differs, add generic ops method            │
    │  4. Use callbacks for hooks, not conditionals              │
    └─────────────────────────────────────────────────────────────┘
```

---

## 2. Oversized Ops Tables

```
+------------------------------------------------------------------+
|  ANTI-PATTERN: OVERSIZED OPS TABLES                              |
+------------------------------------------------------------------+

    WHAT IT LOOKS LIKE:
    ┌─────────────────────────────────────────────────────────────┐
    │  /* BAD: 50+ function pointers in one ops table */         │
    │                                                              │
    │  struct file_operations {                                   │
    │      int (*open)(...);                                      │
    │      int (*read)(...);                                      │
    │      int (*write)(...);                                     │
    │      /* ... 20 more common operations ... */               │
    │      int (*special_ioctl_42)(...);  /* Rarely used */      │
    │      int (*legacy_operation)(...);  /* Deprecated */       │
    │      int (*debug_hook)(...);        /* Internal only */    │
    │      /* ... 30 more rarely used operations ... */          │
    │  };                                                         │
    │                                                              │
    │  /* Most backends implement only 5-10 of these */          │
    └─────────────────────────────────────────────────────────────┘

    WHY IT FAILS:
    ┌─────────────────────────────────────────────────────────────┐
    │  • New backends must understand entire interface           │
    │  • Memory waste: most pointers are NULL                    │
    │  • Hard to version: adding ops breaks ABI                  │
    │  • Unclear responsibilities: too many methods              │
    │  • Maintenance burden: documentation for 50+ ops           │
    └─────────────────────────────────────────────────────────────┘

    HOW TO RECOGNIZE:
    ┌─────────────────────────────────────────────────────────────┐
    │  ✗ Ops table has > 20 function pointers                    │
    │  ✗ Most backends set majority of pointers to NULL          │
    │  ✗ Some ops used by only 1-2 backends                      │
    │  ✗ Ops table keeps growing with each release               │
    └─────────────────────────────────────────────────────────────┘

    HOW VFS HANDLES THIS:
    ┌─────────────────────────────────────────────────────────────┐
    │  /* VFS splits ops into multiple focused tables */         │
    │                                                              │
    │  struct file_operations      /* ~25 ops: file access */    │
    │  struct inode_operations     /* ~20 ops: metadata */       │
    │  struct super_operations     /* ~15 ops: filesystem */     │
    │  struct address_space_ops    /* ~15 ops: page cache */     │
    │                                                              │
    │  /* Each table is cohesive: one responsibility */          │
    │  /* Backends implement only relevant tables */             │
    └─────────────────────────────────────────────────────────────┘

    HOW TO FIX:
    ┌─────────────────────────────────────────────────────────────┐
    │  1. Split ops by responsibility (ISP)                      │
    │  2. Remove rarely-used ops, use ioctl/callbacks            │
    │  3. Provide default implementations for common cases       │
    │  4. Version ops tables with size field for extension       │
    │  5. Use auxiliary ops tables for optional features         │
    └─────────────────────────────────────────────────────────────┘
```

---

## 3. Unclear Ownership

```
+------------------------------------------------------------------+
|  ANTI-PATTERN: UNCLEAR OWNERSHIP                                 |
+------------------------------------------------------------------+

    WHAT IT LOOKS LIKE:
    ┌─────────────────────────────────────────────────────────────┐
    │  /* BAD: Who owns this pointer? */                         │
    │                                                              │
    │  struct file *get_some_file(...);  /* Owned? Borrowed? */  │
    │                                                              │
    │  void process(struct file *f) {                            │
    │      /* Do I need to fput()? */                            │
    │      /* Will caller free it? */                            │
    │      /* Is it valid after this function? */                │
    │  }                                                          │
    │                                                              │
    │  struct cache {                                             │
    │      struct file *cached;  /* Who frees this? */           │
    │  };                                                         │
    └─────────────────────────────────────────────────────────────┘

    WHY IT FAILS:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Double free if both caller and callee free              │
    │  • Memory leak if neither frees                            │
    │  • Use-after-free if freed while still referenced          │
    │  • Impossible to audit code for correctness                │
    │  • Every caller guesses differently                        │
    └─────────────────────────────────────────────────────────────┘

    HOW TO RECOGNIZE:
    ┌─────────────────────────────────────────────────────────────┐
    │  ✗ No documentation on ownership transfer                  │
    │  ✗ Refcount operations in random places                    │
    │  ✗ "I think this is borrowed" comments                     │
    │  ✗ Inconsistent patterns across similar APIs               │
    │  ✗ Memory bugs that are hard to reproduce                  │
    └─────────────────────────────────────────────────────────────┘

    HOW VFS HANDLES THIS:
    ┌─────────────────────────────────────────────────────────────┐
    │  /* VFS has CLEAR ownership rules */                       │
    │                                                              │
    │  /* fget() returns OWNED reference */                      │
    │  struct file *fget(unsigned int fd);                       │
    │  /* Caller MUST call fput() */                             │
    │                                                              │
    │  /* dentry->d_inode is BORROWED */                         │
    │  /* Valid only while holding dentry reference */           │
    │                                                              │
    │  /* Naming: _get suffix = take reference */                │
    │  /* Pairing: every fget() has matching fput() */           │
    └─────────────────────────────────────────────────────────────┘

    HOW TO FIX:
    ┌─────────────────────────────────────────────────────────────┐
    │  1. Document ownership in comments or naming               │
    │     • _take: transfers ownership to callee                 │
    │     • _give / return: transfers ownership to caller        │
    │     • (no suffix): borrowed, no ownership change           │
    │                                                              │
    │  2. Use reference counting for shared ownership            │
    │     • Clear refcount rules documented                      │
    │     • Pair every get with put                              │
    │                                                              │
    │  3. Single owner pattern where possible                    │
    │     • One function allocates, one function frees           │
    │     • Transfer is explicit                                 │
    └─────────────────────────────────────────────────────────────┘
```

---

## 4. Mixing Policy and Mechanism

```
+------------------------------------------------------------------+
|  ANTI-PATTERN: MIXING POLICY AND MECHANISM                       |
+------------------------------------------------------------------+

    WHAT IT LOOKS LIKE:
    ┌─────────────────────────────────────────────────────────────┐
    │  /* BAD: VFS core making filesystem-specific decisions */  │
    │                                                              │
    │  ssize_t vfs_write(struct file *file, ...) {               │
    │      /* Mechanism: writing data */                         │
    │      if (file->size > MAX_FILE_SIZE) {  /* Policy! */     │
    │          return -EFBIG;                                     │
    │      }                                                       │
    │      if (is_ext4(file) && needs_journal()) { /* Policy! */│
    │          start_journal();                                   │
    │      }                                                       │
    │      /* ... */                                              │
    │  }                                                          │
    │                                                              │
    │  /* BAD: Filesystem implementing generic caching */        │
    │  int ext4_read(...) {                                      │
    │      /* This should be VFS/page cache responsibility */   │
    │      if (in_cache(page)) {                                 │
    │          return cached_read();                             │
    │      }                                                       │
    │  }                                                          │
    └─────────────────────────────────────────────────────────────┘

    WHY IT FAILS:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Policy changes require mechanism changes                │
    │  • Different policies can't share same mechanism           │
    │  • Hard to test: policy and mechanism entangled            │
    │  • Duplicated code: each backend reimplements policy       │
    │  • Inflexible: can't swap policies at runtime              │
    └─────────────────────────────────────────────────────────────┘

    HOW TO RECOGNIZE:
    ┌─────────────────────────────────────────────────────────────┐
    │  ✗ Business rules hardcoded in infrastructure code         │
    │  ✗ Mechanism layer knows about specific use cases          │
    │  ✗ Same check duplicated in multiple backends              │
    │  ✗ Configuration values embedded in mechanism              │
    └─────────────────────────────────────────────────────────────┘

    HOW VFS HANDLES THIS:
    ┌─────────────────────────────────────────────────────────────┐
    │  VFS = Mechanism (how to route operations)                 │
    │  Filesystem = Policy (what specific behavior)              │
    │                                                              │
    │  /* VFS provides mechanisms: */                            │
    │  • Page cache infrastructure                               │
    │  • Dentry cache                                             │
    │  • Inode management                                         │
    │  • Path lookup algorithm                                   │
    │                                                              │
    │  /* Filesystems provide policies: */                       │
    │  • Block allocation strategy                               │
    │  • Journaling decisions                                    │
    │  • Extent vs block mapping                                 │
    │  • Consistency guarantees                                  │
    └─────────────────────────────────────────────────────────────┘

    HOW TO FIX:
    ┌─────────────────────────────────────────────────────────────┐
    │  1. Identify what is policy (rules, decisions, config)     │
    │  2. Identify what is mechanism (how to execute)            │
    │  3. Move policy to appropriate layer (higher or callback)  │
    │  4. Make mechanism configurable via parameters or ops      │
    │  5. Test mechanism with mock policies                      │
    └─────────────────────────────────────────────────────────────┘
```

---

## 5. Refcount Mismanagement

```
+------------------------------------------------------------------+
|  ANTI-PATTERN: REFCOUNT MISMANAGEMENT                            |
+------------------------------------------------------------------+

    PATTERN A: Forgotten Put
    ┌─────────────────────────────────────────────────────────────┐
    │  /* BAD: Missing fput() on error path */                   │
    │                                                              │
    │  int process_file(int fd) {                                │
    │      struct file *f = fget(fd);  /* refcount++ */          │
    │      if (!f)                                                │
    │          return -EBADF;                                     │
    │                                                              │
    │      if (validate(f) < 0)                                  │
    │          return -EINVAL;  /* BUG: refcount leaked! */      │
    │                                                              │
    │      /* ... */                                              │
    │      fput(f);                                               │
    │      return 0;                                              │
    │  }                                                          │
    └─────────────────────────────────────────────────────────────┘

    PATTERN B: Extra Get
    ┌─────────────────────────────────────────────────────────────┐
    │  /* BAD: Unnecessary reference increment */                │
    │                                                              │
    │  void use_file(struct file *f) {                           │
    │      get_file(f);  /* Caller already holds reference! */   │
    │      /* ... */                                              │
    │      fput(f);                                               │
    │  }                                                          │
    │                                                              │
    │  /* WORSE: Missing the matching fput */                    │
    │  void use_file_buggy(struct file *f) {                     │
    │      get_file(f);  /* Extra reference */                   │
    │      /* ... */                                              │
    │      /* Forgot fput! Refcount leaked forever */            │
    │  }                                                          │
    └─────────────────────────────────────────────────────────────┘

    PATTERN C: Use After Put
    ┌─────────────────────────────────────────────────────────────┐
    │  /* BAD: Using object after releasing reference */         │
    │                                                              │
    │  void close_and_log(struct file *f) {                      │
    │      fput(f);  /* Release reference */                     │
    │      printf("Closed: %s\n", f->path);  /* UAF BUG! */      │
    │  }                                                          │
    └─────────────────────────────────────────────────────────────┘

    WHY IT FAILS:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Leaked refcount: object never freed (memory leak)       │
    │  • Missing refcount: object freed too early (UAF)          │
    │  • Both are hard to debug (symptoms appear elsewhere)      │
    │  • Can cause system-wide resource exhaustion               │
    └─────────────────────────────────────────────────────────────┘

    HOW TO RECOGNIZE:
    ┌─────────────────────────────────────────────────────────────┐
    │  ✗ get/put not visually balanced in function               │
    │  ✗ Error paths don't have cleanup                          │
    │  ✗ Object used after put call                              │
    │  ✗ Conditional put without matching conditional get        │
    └─────────────────────────────────────────────────────────────┘

    HOW TO FIX:
    ┌─────────────────────────────────────────────────────────────┐
    │  /* Use goto-based cleanup pattern */                      │
    │                                                              │
    │  int process_file(int fd) {                                │
    │      struct file *f;                                        │
    │      int ret;                                               │
    │                                                              │
    │      f = fget(fd);                                         │
    │      if (!f)                                                │
    │          return -EBADF;                                     │
    │                                                              │
    │      ret = validate(f);                                    │
    │      if (ret < 0)                                          │
    │          goto out_put;  /* Single cleanup path */          │
    │                                                              │
    │      ret = do_work(f);                                     │
    │                                                              │
    │  out_put:                                                   │
    │      fput(f);  /* ALWAYS executed */                       │
    │      return ret;                                            │
    │  }                                                          │
    └─────────────────────────────────────────────────────────────┘
```

---

## 6. Incorrect Layering

```
+------------------------------------------------------------------+
|  ANTI-PATTERN: INCORRECT LAYERING                                |
+------------------------------------------------------------------+

    PATTERN A: Upward Dependency
    ┌─────────────────────────────────────────────────────────────┐
    │  /* BAD: Lower layer depends on higher layer */            │
    │                                                              │
    │  /* In filesystem (lower layer): */                        │
    │  #include <vfs_internal.h>  /* WRONG! */                   │
    │                                                              │
    │  int ext4_read(...) {                                      │
    │      /* Calling VFS internal function */                   │
    │      vfs_internal_cache_update();  /* WRONG! */            │
    │  }                                                          │
    │                                                              │
    │  /* This creates circular dependency */                    │
    └─────────────────────────────────────────────────────────────┘

    PATTERN B: Layer Bypass
    ┌─────────────────────────────────────────────────────────────┐
    │  /* BAD: Skipping layers */                                │
    │                                                              │
    │  /* Application directly calls filesystem: */              │
    │  ext4_direct_read(fd, buf, n);  /* Bypassing VFS! */       │
    │                                                              │
    │  /* This breaks: */                                        │
    │  • Permission checks                                       │
    │  • Caching                                                  │
    │  • Auditing                                                 │
    │  • Abstraction guarantees                                  │
    └─────────────────────────────────────────────────────────────┘

    PATTERN C: Cross-Layer Knowledge
    ┌─────────────────────────────────────────────────────────────┐
    │  /* BAD: Layer knows about non-adjacent layer */           │
    │                                                              │
    │  /* Application layer: */                                   │
    │  if (get_block_size() == 4096) {  /* Knows disk layer! */ │
    │      use_optimized_path();                                 │
    │  }                                                          │
    └─────────────────────────────────────────────────────────────┘

    WHY IT FAILS:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Cannot replace layers independently                     │
    │  • Changes ripple across layer boundaries                  │
    │  • Testing requires whole stack                            │
    │  • Circular dependencies make builds complex               │
    └─────────────────────────────────────────────────────────────┘

    HOW VFS ENFORCES LAYERING:
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │  Application ──► VFS ──► Filesystem ──► Block Device       │
    │                                                              │
    │  RULES:                                                     │
    │  • Each layer only knows adjacent layers                   │
    │  • Lower layer provides ops, upper layer calls them        │
    │  • No layer includes headers from non-adjacent layer       │
    │  • Callbacks (ops) point downward, calls go downward       │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘

    HOW TO FIX:
    ┌─────────────────────────────────────────────────────────────┐
    │  1. Audit include files for layer violations               │
    │  2. Use interfaces (ops tables) between layers             │
    │  3. Push functionality to correct layer                    │
    │  4. Add missing abstraction if layer needs to be bypassed  │
    └─────────────────────────────────────────────────────────────┘
```

---

## 7. Incomplete Error Handling

```
+------------------------------------------------------------------+
|  ANTI-PATTERN: INCOMPLETE ERROR HANDLING                         |
+------------------------------------------------------------------+

    PATTERN A: Ignoring Errors
    ┌─────────────────────────────────────────────────────────────┐
    │  /* BAD: Error from ops ignored */                         │
    │                                                              │
    │  void flush_file(struct file *f) {                         │
    │      f->f_op->fsync(f, ...);  /* Return value ignored! */ │
    │      /* Data may not be on disk, but caller thinks ok */   │
    │  }                                                          │
    └─────────────────────────────────────────────────────────────┘

    PATTERN B: Partial Cleanup
    ┌─────────────────────────────────────────────────────────────┐
    │  /* BAD: Error path doesn't undo all operations */         │
    │                                                              │
    │  int setup_file(struct file *f) {                          │
    │      alloc_buffers(f);                                     │
    │      init_cache(f);                                        │
    │      if (register_file(f) < 0)                             │
    │          return -ENOMEM;  /* Buffers and cache leaked! */  │
    │      return 0;                                              │
    │  }                                                          │
    └─────────────────────────────────────────────────────────────┘

    PATTERN C: Wrong Error Propagation
    ┌─────────────────────────────────────────────────────────────┐
    │  /* BAD: Specific error replaced with generic */           │
    │                                                              │
    │  int open_file(...) {                                      │
    │      int ret = backend_open(...);                          │
    │      if (ret < 0)                                          │
    │          return -1;  /* Lost: was it ENOENT? EACCES? */   │
    │      return 0;                                              │
    │  }                                                          │
    └─────────────────────────────────────────────────────────────┘

    WHY IT FAILS:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Silent data corruption (ignored write errors)           │
    │  • Resource leaks (incomplete cleanup)                     │
    │  • Misleading error messages (wrong error code)            │
    │  • Difficult debugging (error source lost)                 │
    └─────────────────────────────────────────────────────────────┘

    HOW VFS HANDLES THIS:
    ┌─────────────────────────────────────────────────────────────┐
    │  /* VFS propagates errors correctly */                     │
    │                                                              │
    │  ssize_t vfs_read(struct file *file, ...) {                │
    │      if (!file->f_op->read)                                │
    │          return -EINVAL;  /* Specific error */             │
    │                                                              │
    │      ret = file->f_op->read(file, ...);                    │
    │      return ret;  /* Propagate backend's error */          │
    │  }                                                          │
    │                                                              │
    │  /* Cleanup uses goto pattern */                           │
    │  if (step3_fails)                                          │
    │      goto undo_step2;                                       │
    │  if (step4_fails)                                          │
    │      goto undo_step3;                                       │
    └─────────────────────────────────────────────────────────────┘

    HOW TO FIX:
    ┌─────────────────────────────────────────────────────────────┐
    │  1. Check ALL return values from ops calls                 │
    │  2. Use goto cleanup pattern for multi-step operations     │
    │  3. Preserve original error codes, don't generalize        │
    │  4. Log errors at appropriate verbosity level              │
    │  5. Test error paths explicitly                            │
    └─────────────────────────────────────────────────────────────┘
```

---

## 8. State Leakage Across Operations

```
+------------------------------------------------------------------+
|  ANTI-PATTERN: STATE LEAKAGE ACROSS OPERATIONS                   |
+------------------------------------------------------------------+

    WHAT IT LOOKS LIKE:
    ┌─────────────────────────────────────────────────────────────┐
    │  /* BAD: Operation leaves object in inconsistent state */  │
    │                                                              │
    │  int extend_file(struct file *f, size_t new_size) {        │
    │      f->size = new_size;  /* Committed early */            │
    │      if (allocate_blocks(f) < 0) {                         │
    │          /* f->size is now wrong! */                       │
    │          return -ENOSPC;                                    │
    │      }                                                       │
    │      return 0;                                              │
    │  }                                                          │
    │                                                              │
    │  /* BAD: Global state modified before local validation */  │
    │  static int total_open;                                     │
    │  int open_file(...) {                                      │
    │      total_open++;  /* Incremented early */                │
    │      if (validate() < 0) {                                 │
    │          return -EINVAL;  /* total_open now wrong! */      │
    │      }                                                       │
    │  }                                                          │
    └─────────────────────────────────────────────────────────────┘

    WHY IT FAILS:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Object left in half-modified state                      │
    │  • Subsequent operations see inconsistent data             │
    │  • Recovery code must know exact failure point             │
    │  • Concurrent access sees partial updates                  │
    └─────────────────────────────────────────────────────────────┘

    HOW VFS HANDLES THIS:
    ┌─────────────────────────────────────────────────────────────┐
    │  /* VFS validates before modifying */                      │
    │                                                              │
    │  int extend_file(struct file *f, size_t new_size) {        │
    │      /* Phase 1: Validate (no modifications) */            │
    │      if (!can_extend(f, new_size))                         │
    │          return -EPERM;                                     │
    │                                                              │
    │      /* Phase 2: Acquire resources (can fail) */           │
    │      blocks = allocate_blocks(new_size);                   │
    │      if (!blocks)                                           │
    │          return -ENOSPC;                                    │
    │                                                              │
    │      /* Phase 3: Commit (cannot fail) */                   │
    │      f->blocks = blocks;                                    │
    │      f->size = new_size;                                   │
    │      return 0;                                              │
    │  }                                                          │
    └─────────────────────────────────────────────────────────────┘

    HOW TO FIX:
    ┌─────────────────────────────────────────────────────────────┐
    │  1. Validate before modifying any state                    │
    │  2. Acquire all resources before committing                │
    │  3. Commit phase should not fail                           │
    │  4. Use transactions or copy-on-write for complex ops      │
    │  5. Test operations with injected failures                 │
    └─────────────────────────────────────────────────────────────┘
```

---

## Summary

```
+------------------------------------------------------------------+
|  ANTI-PATTERNS SUMMARY                                           |
+------------------------------------------------------------------+

    1. LEAKING ABSTRACTION DETAILS
       • Framework mentions specific backends
       • Fix: Dispatch via ops, never check type

    2. OVERSIZED OPS TABLES
       • 50+ function pointers, most NULL
       • Fix: Split by responsibility, use defaults

    3. UNCLEAR OWNERSHIP
       • Who allocates/frees is ambiguous
       • Fix: Document, naming conventions, refcount

    4. MIXING POLICY AND MECHANISM
       • Business rules in infrastructure
       • Fix: Push policy to appropriate layer

    5. REFCOUNT MISMANAGEMENT
       • Leaked/missing/unbalanced refcounts
       • Fix: goto cleanup, pair get/put visually

    6. INCORRECT LAYERING
       • Upward deps, bypassing, cross-layer knowledge
       • Fix: Strict include rules, ops interfaces

    7. INCOMPLETE ERROR HANDLING
       • Ignored errors, partial cleanup
       • Fix: Check all returns, goto cleanup

    8. STATE LEAKAGE
       • Partial modifications before validation
       • Fix: Validate → Acquire → Commit pattern
```

**中文总结：**
- **抽象泄漏**：框架不应知道具体后端，通过ops分发
- **过大的ops表**：按职责拆分，提供默认实现
- **不清晰的所有权**：明确文档、命名约定、引用计数规则
- **混合策略与机制**：策略在上层，机制在下层
- **引用计数错误**：使用goto清理模式，视觉上配对get/put
- **层次违反**：严格的include规则，只通过ops跨层
- **错误处理不完整**：检查所有返回值，goto统一清理
- **状态泄漏**：先验证再修改，提交阶段不能失败

