# WHAT｜具体架构

## 1. 架构模式

```
ARCHITECTURAL PATTERNS IN LINUX MEMORY MANAGEMENT
+=============================================================================+
|                                                                              |
|  PATTERN 1: LAYERED ABSTRACTION (分层抽象)                                   |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Linux MM uses strict layering to manage complexity:                     │ |
|  │                                                                          │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐   │ |
|  │  │                                                                    │   │ |
|  │  │  LAYER 4: User Space API                                           │   │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐   │   │ |
|  │  │  │  mmap(), brk(), mlock(), mprotect(), munmap()              │   │   │ |
|  │  │  │  System calls exposed to applications                       │   │   │ |
|  │  │  └────────────────────────────────────────────────────────────┘   │   │ |
|  │  │                              │                                     │   │ |
|  │  │                              ▼                                     │   │ |
|  │  │  LAYER 3: Virtual Memory Management                                │   │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐   │   │ |
|  │  │  │  mm_struct, vm_area_struct, page fault handling             │   │   │ |
|  │  │  │  Address space management, VMA operations                   │   │   │ |
|  │  │  └────────────────────────────────────────────────────────────┘   │   │ |
|  │  │                              │                                     │   │ |
|  │  │                              ▼                                     │   │ |
|  │  │  LAYER 2: Page Allocation                                          │   │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐   │   │ |
|  │  │  │  Buddy allocator, zone management, page reclaim             │   │   │ |
|  │  │  │  Physical page management                                   │   │   │ |
|  │  │  └────────────────────────────────────────────────────────────┘   │   │ |
|  │  │                              │                                     │   │ |
|  │  │                              ▼                                     │   │ |
|  │  │  LAYER 1: Object Allocation (SLAB/SLUB)                            │   │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐   │   │ |
|  │  │  │  kmalloc(), kmem_cache_alloc()                              │   │   │ |
|  │  │  │  Kernel object caching and allocation                       │   │   │ |
|  │  │  └────────────────────────────────────────────────────────────┘   │   │ |
|  │  │                              │                                     │   │ |
|  │  │                              ▼                                     │   │ |
|  │  │  LAYER 0: Hardware Abstraction                                     │   │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐   │   │ |
|  │  │  │  Arch-specific: page tables, TLB, cache operations          │   │   │ |
|  │  │  │  set_pte(), flush_tlb_*(), switch_mm()                      │   │   │ |
|  │  │  └────────────────────────────────────────────────────────────┘   │   │ |
|  │  │                                                                    │   │ |
|  │  └──────────────────────────────────────────────────────────────────┘   │ |
|  │                                                                          │ |
|  │  Benefits:                                                               │ |
|  │  • Each layer has clear responsibility                                  │ |
|  │  • Layers can be modified independently                                 │ |
|  │  • Testing is easier (mock lower layers)                                │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  PATTERN 2: OBJECT LIFECYCLE TRACKING (对象生命周期跟踪)                     |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Memory objects have explicit lifecycle with reference counting:         │ |
|  │                                                                          │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐   │ |
|  │  │                                                                    │   │ |
|  │  │  STRUCT PAGE LIFECYCLE:                                            │   │ |
|  │  │                                                                    │   │ |
|  │  │  ┌─────────────┐                                                   │   │ |
|  │  │  │   CREATION  │                                                   │   │ |
|  │  │  │             │                                                   │   │ |
|  │  │  │ alloc_page()│                                                   │   │ |
|  │  │  │ _count = 1  │                                                   │   │ |
|  │  │  └──────┬──────┘                                                   │   │ |
|  │  │         │                                                          │   │ |
|  │  │         ▼                                                          │   │ |
|  │  │  ┌─────────────┐      get_page()      ┌─────────────┐              │   │ |
|  │  │  │    USE      │◄────────────────────►│  SHARING    │              │   │ |
|  │  │  │             │                      │             │              │   │ |
|  │  │  │ _count >= 1 │      put_page()      │ _count > 1  │              │   │ |
|  │  │  └──────┬──────┘◄────────────────────►└─────────────┘              │   │ |
|  │  │         │                                                          │   │ |
|  │  │         │ put_page() when _count == 1                              │   │ |
|  │  │         ▼                                                          │   │ |
|  │  │  ┌─────────────┐                                                   │   │ |
|  │  │  │ DESTRUCTION │                                                   │   │ |
|  │  │  │             │                                                   │   │ |
|  │  │  │ __free_page │                                                   │   │ |
|  │  │  │ _count = 0  │                                                   │   │ |
|  │  │  └─────────────┘                                                   │   │ |
|  │  │                                                                    │   │ |
|  │  └──────────────────────────────────────────────────────────────────┘   │ |
|  │                                                                          │ |
|  │  SIMILAR PATTERN FOR OTHER STRUCTURES:                                   │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  mm_struct:     atomic_inc(&mm->mm_count) / mmput(mm)            │    │ |
|  │  │  vm_area_struct: Reference through mm (no separate refcount)     │    │ |
|  │  │  address_space: Reference through inode or anon_vma              │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  PATTERN 3: OPS-TABLE POLYMORPHISM (操作表多态)                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  VMAs use ops tables for different mapping types:                        │ |
|  │                                                                          │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐   │ |
|  │  │                                                                    │   │ |
|  │  │  struct vm_operations_struct {                                     │   │ |
|  │  │      void (*open)(struct vm_area_struct *);   /* VMA duplicated */ │   │ |
|  │  │      void (*close)(struct vm_area_struct *);  /* VMA removed */    │   │ |
|  │  │      int (*fault)(struct vm_area_struct *, struct vm_fault *);     │   │ |
|  │  │      int (*page_mkwrite)(struct vm_area_struct *, struct vm_fault *);│  │ |
|  │  │      ...                                                           │   │ |
|  │  │  };                                                                │   │ |
|  │  │                                                                    │   │ |
|  │  │  Different implementations:                                        │   │ |
|  │  │                                                                    │   │ |
|  │  │  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐    │   │ |
|  │  │  │ generic_file_   │  │  shmem_vm_ops   │  │  special_mapping │   │   │ |
|  │  │  │ vm_ops          │  │                 │  │  _vmops          │   │   │ |
|  │  │  │ (regular files) │  │ (tmpfs/shmem)   │  │  (vDSO, etc.)   │   │   │ |
|  │  │  └─────────────────┘  └─────────────────┘  └─────────────────┘    │   │ |
|  │  │                                                                    │   │ |
|  │  │  On page fault:                                                    │   │ |
|  │  │  vma->vm_ops->fault(vma, vmf);  /* Dispatch to correct handler */ │   │ |
|  │  │                                                                    │   │ |
|  │  └──────────────────────────────────────────────────────────────────┘   │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**模式 1：分层抽象**
- 第 4 层：用户空间 API（`mmap()`、`brk()` 等系统调用）
- 第 3 层：虚拟内存管理（`mm_struct`、`vm_area_struct`、页错误处理）
- 第 2 层：页分配（伙伴分配器、区域管理、页回收）
- 第 1 层：对象分配（SLAB/SLUB）
- 第 0 层：硬件抽象（页表、TLB、缓存操作）

**模式 2：对象生命周期跟踪**
- `struct page`：通过 `_count` 引用计数
- `get_page()`：增加引用
- `put_page()`：减少引用，到 0 时释放
- 类似模式用于 `mm_struct` 等

**模式 3：操作表多态**
- `vm_operations_struct`：VMA 的虚函数表
- 不同实现：`generic_file_vm_ops`（普通文件）、`shmem_vm_ops`（tmpfs）
- 页错误时分发：`vma->vm_ops->fault(vma, vmf)`

---

## 2. 核心数据结构

```
CORE DATA STRUCTURES
+=============================================================================+
|                                                                              |
|  MM_STRUCT: ADDRESS SPACE DESCRIPTOR                                         |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  struct mm_struct {                                                      │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐   │ |
|  │  │                                                                    │   │ |
|  │  │  /* VMA Management */                                              │   │ |
|  │  │  struct vm_area_struct *mmap;    /* VMA list head */               │   │ |
|  │  │  struct rb_root mm_rb;           /* VMA red-black tree */          │   │ |
|  │  │  struct vm_area_struct *mmap_cache;  /* Last find_vma result */    │   │ |
|  │  │                                                                    │   │ |
|  │  │  /* Page Table Root */                                             │   │ |
|  │  │  pgd_t *pgd;                     /* Top-level page table */        │   │ |
|  │  │                                                                    │   │ |
|  │  │  /* Reference Counts */                                            │   │ |
|  │  │  atomic_t mm_users;              /* Threads using this mm */       │   │ |
|  │  │  atomic_t mm_count;              /* Reference count */             │   │ |
|  │  │                                                                    │   │ |
|  │  │  /* Memory Regions */                                              │   │ |
|  │  │  unsigned long start_code, end_code;   /* Text segment */          │   │ |
|  │  │  unsigned long start_data, end_data;   /* Data segment */          │   │ |
|  │  │  unsigned long start_brk, brk;         /* Heap */                  │   │ |
|  │  │  unsigned long start_stack;            /* Stack start */           │   │ |
|  │  │  unsigned long arg_start, arg_end;     /* Arguments */             │   │ |
|  │  │  unsigned long env_start, env_end;     /* Environment */           │   │ |
|  │  │                                                                    │   │ |
|  │  │  /* Statistics */                                                  │   │ |
|  │  │  unsigned long total_vm;         /* Total pages mapped */          │   │ |
|  │  │  unsigned long locked_vm;        /* Locked pages */                │   │ |
|  │  │  unsigned long pinned_vm;        /* Pinned pages */                │   │ |
|  │  │                                                                    │   │ |
|  │  │  /* Locking */                                                     │   │ |
|  │  │  struct rw_semaphore mmap_sem;   /* VMA lock */                    │   │ |
|  │  │                                                                    │   │ |
|  │  └──────────────────────────────────────────────────────────────────┘   │ |
|  │  };                                                                      │ |
|  │                                                                          │ |
|  │  OWNERSHIP: task_struct->mm points to process's mm_struct               │ |
|  │             Threads share mm_struct (mm_users > 1)                      │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  VM_AREA_STRUCT: MEMORY REGION DESCRIPTOR                                    |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  struct vm_area_struct {                                                 │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐   │ |
|  │  │                                                                    │   │ |
|  │  │  /* Address Range */                                               │   │ |
|  │  │  unsigned long vm_start;         /* Start address (inclusive) */   │   │ |
|  │  │  unsigned long vm_end;           /* End address (exclusive) */     │   │ |
|  │  │                                                                    │   │ |
|  │  │  /* Linking */                                                     │   │ |
|  │  │  struct vm_area_struct *vm_next, *vm_prev;  /* List linkage */     │   │ |
|  │  │  struct rb_node vm_rb;           /* RB-tree node */                │   │ |
|  │  │                                                                    │   │ |
|  │  │  /* Ownership */                                                   │   │ |
|  │  │  struct mm_struct *vm_mm;        /* Owning address space */        │   │ |
|  │  │                                                                    │   │ |
|  │  │  /* Permissions and Flags */                                       │   │ |
|  │  │  pgprot_t vm_page_prot;          /* Access permissions for PTEs */ │   │ |
|  │  │  unsigned long vm_flags;         /* VM_READ, VM_WRITE, etc. */     │   │ |
|  │  │                                                                    │   │ |
|  │  │  /* Mapped File (optional) */                                      │   │ |
|  │  │  struct file *vm_file;           /* File being mapped */           │   │ |
|  │  │  unsigned long vm_pgoff;         /* Offset in file (in pages) */   │   │ |
|  │  │                                                                    │   │ |
|  │  │  /* Operations */                                                  │   │ |
|  │  │  const struct vm_operations_struct *vm_ops;  /* Handler ops */     │   │ |
|  │  │                                                                    │   │ |
|  │  │  /* Private Data */                                                │   │ |
|  │  │  void *vm_private_data;          /* Driver-specific data */        │   │ |
|  │  │                                                                    │   │ |
|  │  └──────────────────────────────────────────────────────────────────┘   │ |
|  │  };                                                                      │ |
|  │                                                                          │ |
|  │  VMA ORGANIZATION:                                                       │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  mm_struct                                                       │    │ |
|  │  │      │                                                           │    │ |
|  │  │      ├── mmap (list) ──► VMA1 ──► VMA2 ──► VMA3 ──► ...         │    │ |
|  │  │      │                                                           │    │ |
|  │  │      └── mm_rb (tree)                                            │    │ |
|  │  │              ┌────┐                                              │    │ |
|  │  │              │VMA2│                                              │    │ |
|  │  │            ┌─┴────┴─┐                                            │    │ |
|  │  │          ┌────┐  ┌────┐                                          │    │ |
|  │  │          │VMA1│  │VMA3│                                          │    │ |
|  │  │          └────┘  └────┘                                          │    │ |
|  │  │                                                                  │    │ |
|  │  │  List: For iteration (linear traversal)                          │    │ |
|  │  │  Tree: For lookup by address (O(log n))                          │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  STRUCT PAGE: PHYSICAL PAGE DESCRIPTOR                                       |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  struct page {                                                           │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐   │ |
|  │  │                                                                    │   │ |
|  │  │  /* State Flags */                                                 │   │ |
|  │  │  unsigned long flags;            /* PG_locked, PG_dirty, etc. */   │   │ |
|  │  │                                                                    │   │ |
|  │  │  /* Reference Counting */                                          │   │ |
|  │  │  atomic_t _count;                /* Usage count */                 │   │ |
|  │  │  atomic_t _mapcount;             /* Page table mapping count */    │   │ |
|  │  │                                                                    │   │ |
|  │  │  /* Context (union - meaning depends on page type) */              │   │ |
|  │  │  union {                                                           │   │ |
|  │  │      /* For file-backed or anon pages */                           │   │ |
|  │  │      struct {                                                      │   │ |
|  │  │          struct address_space *mapping;  /* File or anon_vma */   │   │ |
|  │  │          pgoff_t index;                  /* Offset in file */     │   │ |
|  │  │      };                                                            │   │ |
|  │  │                                                                    │   │ |
|  │  │      /* For slab allocator */                                      │   │ |
|  │  │      struct {                                                      │   │ |
|  │  │          struct kmem_cache *slab_cache;                            │   │ |
|  │  │          void *freelist;                                           │   │ |
|  │  │      };                                                            │   │ |
|  │  │                                                                    │   │ |
|  │  │      /* For compound pages */                                      │   │ |
|  │  │      struct {                                                      │   │ |
|  │  │          struct page *first_page;                                  │   │ |
|  │  │          unsigned int compound_order;                              │   │ |
|  │  │      };                                                            │   │ |
|  │  │  };                                                                │   │ |
|  │  │                                                                    │   │ |
|  │  │  /* LRU List */                                                    │   │ |
|  │  │  struct list_head lru;           /* For page reclaim lists */      │   │ |
|  │  │                                                                    │   │ |
|  │  └──────────────────────────────────────────────────────────────────┘   │ |
|  │  };                                                                      │ |
|  │                                                                          │ |
|  │  PAGE ARRAY (mem_map):                                                   │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  struct page *mem_map;   /* Array of all page descriptors */     │    │ |
|  │  │                                                                  │    │ |
|  │  │  Physical Frame Number (PFN) → struct page:                      │    │ |
|  │  │      pfn_to_page(pfn) = mem_map + pfn                            │    │ |
|  │  │                                                                  │    │ |
|  │  │  struct page → Physical Frame Number:                            │    │ |
|  │  │      page_to_pfn(page) = page - mem_map                          │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**mm_struct：地址空间描述符**
- VMA 管理：`mmap`（链表头）、`mm_rb`（红黑树）、`mmap_cache`（最后查找结果缓存）
- 页表根：`pgd`
- 引用计数：`mm_users`（线程数）、`mm_count`（引用计数）
- 内存区域：代码、数据、堆、栈的起止地址
- 锁：`mmap_sem`（VMA 操作锁）

**vm_area_struct：内存区域描述符**
- 地址范围：`vm_start`、`vm_end`
- 链接：链表（`vm_next`/`vm_prev`）和红黑树（`vm_rb`）
- 权限：`vm_page_prot`、`vm_flags`
- 映射文件：`vm_file`、`vm_pgoff`
- 操作表：`vm_ops`

**struct page：物理页描述符**
- 状态标志：`flags`（`PG_locked`、`PG_dirty` 等）
- 引用计数：`_count`、`_mapcount`
- 上下文（联合体）：文件/匿名页、slab、复合页
- LRU 列表：用于页回收

---

## 3. 控制流：页错误路径

```
PAGE FAULT HANDLING PATH
+=============================================================================+
|                                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  USER SPACE ACCESS TO UNMAPPED ADDRESS                                   │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐   │ |
|  │  │                                                                    │   │ |
|  │  │  mov [0x12345678], eax    ◄── CPU tries to access this address    │   │ |
|  │  │                                                                    │   │ |
|  │  │  MMU: "No valid PTE for this address!"                             │   │ |
|  │  │                                                                    │   │ |
|  │  │        ─────────────────────────────────────────                   │   │ |
|  │  │                      │                                             │   │ |
|  │  │                      ▼                                             │   │ |
|  │  │           HARDWARE EXCEPTION (INT 14 on x86)                       │   │ |
|  │  │                                                                    │   │ |
|  │  └──────────────────────────────────────────────────────────────────┘   │ |
|  │                                                                          │ |
|  │  ARCH-SPECIFIC ENTRY                                                     │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐   │ |
|  │  │                                                                    │   │ |
|  │  │  arch/x86/mm/fault.c:                                              │   │ |
|  │  │                                                                    │   │ |
|  │  │  do_page_fault(regs, error_code)                                   │   │ |
|  │  │      │                                                             │   │ |
|  │  │      ├── Read CR2 register (faulting address)                      │   │ |
|  │  │      │                                                             │   │ |
|  │  │      ├── Decode error_code:                                        │   │ |
|  │  │      │   • Bit 0: Present (0=not present, 1=protection)            │   │ |
|  │  │      │   • Bit 1: Write (0=read, 1=write)                          │   │ |
|  │  │      │   • Bit 2: User (0=kernel, 1=user)                          │   │ |
|  │  │      │                                                             │   │ |
|  │  │      ├── Check if kernel fault (special handling)                  │   │ |
|  │  │      │                                                             │   │ |
|  │  │      └── Call generic handler                                      │   │ |
|  │  │              │                                                     │   │ |
|  │  │              ▼                                                     │   │ |
|  │  └──────────────────────────────────────────────────────────────────┘   │ |
|  │                                                                          │ |
|  │  GENERIC FAULT HANDLING (mm/memory.c)                                    │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐   │ |
|  │  │                                                                    │   │ |
|  │  │  handle_mm_fault(mm, vma, address, flags)                          │   │ |
|  │  │      │                                                             │   │ |
|  │  │      ├── down_read(&mm->mmap_sem)   ◄── Take read lock             │   │ |
|  │  │      │                                                             │   │ |
|  │  │      ├── find_vma(mm, address)       ◄── Find VMA containing addr  │   │ |
|  │  │      │       │                                                     │   │ |
|  │  │      │       ├── VMA not found → SIGSEGV                           │   │ |
|  │  │      │       │                                                     │   │ |
|  │  │      │       └── VMA found, check permissions                      │   │ |
|  │  │      │                                                             │   │ |
|  │  │      ├── __handle_mm_fault(mm, vma, address, flags)                │   │ |
|  │  │      │       │                                                     │   │ |
|  │  │      │       ├── Allocate page table levels if needed              │   │ |
|  │  │      │       │   (PGD, PUD, PMD, PTE)                              │   │ |
|  │  │      │       │                                                     │   │ |
|  │  │      │       └── handle_pte_fault(mm, vma, address, pte, pmd, flags)│  │ |
|  │  │      │                                                             │   │ |
|  │  │      └── up_read(&mm->mmap_sem)      ◄── Release lock              │   │ |
|  │  │                                                                    │   │ |
|  │  └──────────────────────────────────────────────────────────────────┘   │ |
|  │                                                                          │ |
|  │  PTE FAULT HANDLING                                                      │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐   │ |
|  │  │                                                                    │   │ |
|  │  │  handle_pte_fault(mm, vma, address, pte, pmd, flags)               │   │ |
|  │  │      │                                                             │   │ |
|  │  │      │  if (pte_none(entry)) {                                     │   │ |
|  │  │      │      /* No PTE at all - first access */                     │   │ |
|  │  │      │                                                             │   │ |
|  │  │      │      if (vma->vm_ops && vma->vm_ops->fault)                  │   │ |
|  │  │      │          ──► do_fault() ──► vma->vm_ops->fault()            │   │ |
|  │  │      │              (file-backed: read from file)                  │   │ |
|  │  │      │                                                             │   │ |
|  │  │      │      else                                                   │   │ |
|  │  │      │          ──► do_anonymous_page()                            │   │ |
|  │  │      │              (allocate zero-filled page)                    │   │ |
|  │  │      │  }                                                          │   │ |
|  │  │      │                                                             │   │ |
|  │  │      │  else if (!pte_present(entry)) {                            │   │ |
|  │  │      │      /* PTE exists but page not in memory - swapped */      │   │ |
|  │  │      │                                                             │   │ |
|  │  │      │      ──► do_swap_page()                                     │   │ |
|  │  │      │          (read page from swap)                              │   │ |
|  │  │      │  }                                                          │   │ |
|  │  │      │                                                             │   │ |
|  │  │      │  else if (flags & FAULT_FLAG_WRITE && !pte_write(entry)) {  │   │ |
|  │  │      │      /* Write to read-only page - COW */                    │   │ |
|  │  │      │                                                             │   │ |
|  │  │      │      ──► do_wp_page()                                       │   │ |
|  │  │      │          (copy-on-write: copy page, make writable)          │   │ |
|  │  │      │  }                                                          │   │ |
|  │  │      │                                                             │   │ |
|  │  │      ▼                                                             │   │ |
|  │  │  Return to user space, retry instruction                           │   │ |
|  │  │                                                                    │   │ |
|  │  └──────────────────────────────────────────────────────────────────┘   │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**页错误处理路径**：

1. **硬件异常**：CPU 尝试访问地址 → MMU 发现无有效 PTE → 触发异常（x86 上 INT 14）

2. **架构特定入口**（`arch/x86/mm/fault.c`）：
   - `do_page_fault()`
   - 读取 CR2 寄存器（错误地址）
   - 解码错误码（存在、写、用户/内核）
   - 调用通用处理器

3. **通用错误处理**（`mm/memory.c`）：
   - `handle_mm_fault()`
   - 获取 `mmap_sem` 读锁
   - `find_vma()` 查找包含地址的 VMA
   - VMA 未找到 → SIGSEGV
   - 分配页表级别（如果需要）
   - 调用 `handle_pte_fault()`

4. **PTE 错误处理**：
   - **无 PTE**（首次访问）：
     - 文件映射 → `vma->vm_ops->fault()`（从文件读取）
     - 匿名映射 → `do_anonymous_page()`（分配零填充页）
   - **PTE 存在但页不在内存**（已换出）→ `do_swap_page()`
   - **写只读页**（COW）→ `do_wp_page()`（复制页，使其可写）

5. **返回用户空间**，重试指令

---

## 4. 扩展点：分配器

```
EXTENSION POINTS: ALLOCATORS (SLAB/SLUB)
+=============================================================================+
|                                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  KERNEL OBJECT ALLOCATION PROBLEM                                        │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Kernel frequently allocates small objects of fixed sizes:       │    │ |
|  │  │                                                                  │    │ |
|  │  │  • task_struct (~2KB)                                            │    │ |
|  │  │  • inode (~400 bytes)                                            │    │ |
|  │  │  • dentry (~200 bytes)                                           │    │ |
|  │  │  • sk_buff (~200 bytes)                                          │    │ |
|  │  │                                                                  │    │ |
|  │  │  Problem: Page allocator gives 4KB pages                         │    │ |
|  │  │           Internal fragmentation wastes memory                   │    │ |
|  │  │                                                                  │    │ |
|  │  │  Solution: Slab allocator carves pages into fixed-size objects   │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  SLAB/SLUB ARCHITECTURE                                                  │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  struct kmem_cache (one per object type):                        │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  name: "task_struct"                                        │ │    │ |
|  │  │  │  size: 2048 bytes                                           │ │    │ |
|  │  │  │  align: 64 bytes (cache line)                               │ │    │ |
|  │  │  │  ctor: task_struct_init  (optional constructor)             │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  ┌──────────────────────────────────────────────────────┐  │ │    │ |
|  │  │  │  │  PER-CPU SLABS (fast path, no locking)               │  │ │    │ |
|  │  │  │  │                                                       │  │ │    │ |
|  │  │  │  │  CPU 0: [free][free][used][free]...                  │  │ │    │ |
|  │  │  │  │  CPU 1: [used][free][used][used]...                  │  │ │    │ |
|  │  │  │  │  CPU 2: [free][free][free][used]...                  │  │ │    │ |
|  │  │  │  │                                                       │  │ │    │ |
|  │  │  │  └──────────────────────────────────────────────────────┘  │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  ┌──────────────────────────────────────────────────────┐  │ │    │ |
|  │  │  │  │  PARTIAL SLABS (shared pool)                         │  │ │    │ |
|  │  │  │  │                                                       │  │ │    │ |
|  │  │  │  │  Slab 1: [used][free][used][free]...                 │  │ │    │ |
|  │  │  │  │  Slab 2: [free][used][free][free]...                 │  │ │    │ |
|  │  │  │  │                                                       │  │ │    │ |
|  │  │  │  └──────────────────────────────────────────────────────┘  │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  API:                                                                    │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  /* Create a cache for specific object type */                   │    │ |
|  │  │  struct kmem_cache *kmem_cache_create(                           │    │ |
|  │  │      const char *name, size_t size, size_t align,                │    │ |
|  │  │      unsigned long flags, void (*ctor)(void *));                 │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* Allocate object from cache */                                │    │ |
|  │  │  void *kmem_cache_alloc(struct kmem_cache *cache, gfp_t flags);  │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* Free object back to cache */                                 │    │ |
|  │  │  void kmem_cache_free(struct kmem_cache *cache, void *obj);      │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* Generic allocation (uses size-based caches) */               │    │ |
|  │  │  void *kmalloc(size_t size, gfp_t flags);                        │    │ |
|  │  │  void kfree(void *ptr);                                          │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**内核对象分配问题**：
- 内核频繁分配小的固定大小对象（`task_struct` ~2KB、`inode` ~400B 等）
- 页分配器给 4KB 页，内部碎片浪费内存
- 解决方案：Slab 分配器将页划分为固定大小对象

**SLAB/SLUB 架构**：
- `kmem_cache`：每种对象类型一个
- 包含：名称、大小、对齐、构造函数
- per-CPU slabs：快速路径，无锁
- 部分 slabs：共享池

**API**：
- `kmem_cache_create()`：创建缓存
- `kmem_cache_alloc()`：从缓存分配
- `kmem_cache_free()`：释放回缓存
- `kmalloc()`/`kfree()`：通用分配

---

## 5. 限制和代价

```
LIMITS AND COSTS
+=============================================================================+
|                                                                              |
|  TLB SHOOTDOWNS (TLB 击落)                                                   |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  PROBLEM: TLB caches translations on each CPU                            │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  When page table entry changes (e.g., munmap):                   │    │ |
|  │  │                                                                  │    │ |
|  │  │  CPU 0                 CPU 1                 CPU 2               │    │ |
|  │  │  ┌─────────┐          ┌─────────┐          ┌─────────┐          │    │ |
|  │  │  │   TLB   │          │   TLB   │          │   TLB   │          │    │ |
|  │  │  │ VA→PA X │          │ VA→PA X │          │ VA→PA X │          │    │ |
|  │  │  └────┬────┘          └────┬────┘          └────┬────┘          │    │ |
|  │  │       │                    │                    │               │    │ |
|  │  │       │ stale!             │ stale!             │ stale!        │    │ |
|  │  │       │                    │                    │               │    │ |
|  │  │       └────────────────────┴────────────────────┘               │    │ |
|  │  │                            │                                    │    │ |
|  │  │                            ▼                                    │    │ |
|  │  │           ALL TLBs must be invalidated!                         │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  TLB SHOOTDOWN MECHANISM:                                                │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  1. CPU 0 modifies page table                                    │    │ |
|  │  │                                                                  │    │ |
|  │  │  2. CPU 0 sends IPI (Inter-Processor Interrupt) to CPUs 1, 2     │    │ |
|  │  │                                                                  │    │ |
|  │  │  3. CPUs 1, 2 receive interrupt, flush their TLBs                │    │ |
|  │  │                                                                  │    │ |
|  │  │  4. CPUs 1, 2 acknowledge completion                             │    │ |
|  │  │                                                                  │    │ |
|  │  │  5. CPU 0 continues                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  COST: ~1000-10000 cycles per shootdown                          │    │ |
|  │  │        Blocks all participating CPUs                             │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  MITIGATION:                                                             │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  • Batch TLB flushes (flush once for multiple pages)             │    │ |
|  │  │  • Use ASID/PCID to tag TLB entries per address space            │    │ |
|  │  │  • Lazy TLB (kernel threads borrow prev mm, no switch)           │    │ |
|  │  │  • Defer shootdown until necessary                               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  NUMA EFFECTS (NUMA 效应)                                                    |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  NON-UNIFORM MEMORY ACCESS:                                              │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  NUMA Node 0                      NUMA Node 1                    │    │ |
|  │  │  ┌───────────────────────┐       ┌───────────────────────┐      │    │ |
|  │  │  │  CPU 0    CPU 1      │       │  CPU 2    CPU 3      │      │    │ |
|  │  │  │    │        │        │       │    │        │        │      │    │ |
|  │  │  │    └────┬───┘        │       │    └────┬───┘        │      │    │ |
|  │  │  │         │            │       │         │            │      │    │ |
|  │  │  │  ┌──────▼──────┐     │       │  ┌──────▼──────┐     │      │    │ |
|  │  │  │  │   LOCAL     │     │       │  │   LOCAL     │     │      │    │ |
|  │  │  │  │   MEMORY    │     │◄─────►│  │   MEMORY    │     │      │    │ |
|  │  │  │  │   (fast)    │     │ slow! │  │   (fast)    │     │      │    │ |
|  │  │  │  └─────────────┘     │       │  └─────────────┘     │      │    │ |
|  │  │  │                      │       │                      │      │    │ |
|  │  │  └───────────────────────┘       └───────────────────────┘      │    │ |
|  │  │                                                                  │    │ |
|  │  │  Access latency:                                                 │    │ |
|  │  │  • Local: ~70ns                                                  │    │ |
|  │  │  • Remote: ~120ns (1.7x slower!)                                 │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  LINUX NUMA AWARENESS:                                                   │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  • Memory zones per NUMA node                                    │    │ |
|  │  │  • Prefer local allocation (MPOL_PREFERRED)                      │    │ |
|  │  │  • numactl for user control                                      │    │ |
|  │  │  • Automatic NUMA balancing (migrate hot pages)                  │    │ |
|  │  │  • Memory policy: MPOL_BIND, MPOL_INTERLEAVE                     │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  OTHER COSTS:                                                                |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  • Page table memory: ~0.2% of mapped memory                            │ |
|  │  • struct page overhead: 64 bytes per 4KB page (~1.5%)                  │ |
|  │  • Context switch: TLB flush + cache pollution                          │ |
|  │  • Page fault latency: 1000-10000 cycles                                │ |
|  │  • Memory fragmentation: limits huge page availability                  │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**TLB 击落**：
- 问题：每个 CPU 的 TLB 缓存转换，页表条目更改时所有 TLB 必须失效
- 机制：CPU 0 修改页表 → 发送 IPI → 其他 CPU 刷新 TLB → 确认完成
- 成本：每次击落 ~1000-10000 周期，阻塞所有参与的 CPU
- 缓解：批量刷新、ASID/PCID、延迟 TLB、延迟击落

**NUMA 效应**：
- 非统一内存访问：本地 ~70ns，远程 ~120ns（慢 1.7 倍）
- Linux NUMA 感知：
  - 每 NUMA 节点内存区域
  - 优先本地分配
  - `numactl` 用户控制
  - 自动 NUMA 平衡

**其他成本**：
- 页表内存：映射内存的 ~0.2%
- `struct page` 开销：每 4KB 页 64 字节（~1.5%）
- 上下文切换：TLB 刷新 + 缓存污染
- 页错误延迟：1000-10000 周期
