# TRANSFER｜将思想应用到实际项目

## 1. 用户空间中的虚拟 vs 物理抽象

```
VIRTUAL VS PHYSICAL ABSTRACTION IN USER-SPACE
+=============================================================================+
|                                                                              |
|  THE KERNEL PATTERN                                                          |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Linux kernel separates:                                                 │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Virtual Layer               Physical Layer                      │    │ |
|  │  │  ─────────────               ──────────────                      │    │ |
|  │  │  • Addresses (0-4GB)         • Pages (struct page)               │    │ |
|  │  │  • VMAs (regions)            • Zones (DMA, Normal)               │    │ |
|  │  │  • Page tables               • Buddy allocator                   │    │ |
|  │  │                                                                  │    │ |
|  │  │  Benefit: Manage complexity at each layer independently          │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  USER-SPACE APPLICATION: ARENA ALLOCATORS                                    |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐   │ |
|  │  │                                                                    │   │ |
|  │  │  "Virtual" Layer             "Physical" Layer                      │   │ |
|  │  │  ────────────────             ─────────────────                    │   │ |
|  │  │  • User allocations          • Memory arenas (chunks from OS)     │   │ |
|  │  │  • malloc() pointers         • mmap() regions                     │   │ |
|  │  │  • Logical organization      • Actual memory                      │   │ |
|  │  │                                                                    │   │ |
|  │  │  struct arena {                                                    │   │ |
|  │  │      void *base;           /* "Physical" - mmap'd region */       │   │ |
|  │  │      size_t size;          /* Total size */                       │   │ |
|  │  │      size_t used;          /* Current offset */                   │   │ |
|  │  │  };                                                                │   │ |
|  │  │                                                                    │   │ |
|  │  │  /* "Virtual" allocations within arena */                          │   │ |
|  │  │  void *arena_alloc(struct arena *a, size_t size) {                 │   │ |
|  │  │      void *ptr = a->base + a->used;                                │   │ |
|  │  │      a->used += ALIGN(size, 16);                                   │   │ |
|  │  │      return ptr;                                                   │   │ |
|  │  │  }                                                                 │   │ |
|  │  │                                                                    │   │ |
|  │  └──────────────────────────────────────────────────────────────────┘   │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  USER-SPACE APPLICATION: BUFFER POOL                                         |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Like kernel page cache, but for user-space:                             │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐   │ |
|  │  │                                                                    │   │ |
|  │  │  Database Buffer Pool (like InnoDB, PostgreSQL)                    │   │ |
|  │  │                                                                    │   │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐   │   │ |
|  │  │  │                                                             │   │   │ |
|  │  │  │  "Virtual" Layer: Page IDs (file_id, page_num)              │   │   │ |
|  │  │  │                                                             │   │   │ |
|  │  │  │  struct page_id {                                           │   │   │ |
|  │  │  │      uint32_t file_id;                                      │   │   │ |
|  │  │  │      uint32_t page_num;                                     │   │   │ |
|  │  │  │  };                                                         │   │   │ |
|  │  │  │                                                             │   │   │ |
|  │  │  └────────────────────────────────────────────────────────────┘   │   │ |
|  │  │                          │                                         │   │ |
|  │  │                          │ Hash table lookup                       │   │ |
|  │  │                          ▼                                         │   │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐   │   │ |
|  │  │  │                                                             │   │   │ |
|  │  │  │  "Physical" Layer: Buffer frames in memory                  │   │   │ |
|  │  │  │                                                             │   │   │ |
|  │  │  │  struct buffer_frame {                                      │   │   │ |
|  │  │  │      char data[PAGE_SIZE];                                  │   │   │ |
|  │  │  │      struct page_id id;                                     │   │   │ |
|  │  │  │      int pin_count;       /* Like _count in struct page */  │   │   │ |
|  │  │  │      bool dirty;          /* Like PG_dirty */               │   │   │ |
|  │  │  │      struct list_head lru; /* LRU list */                   │   │   │ |
|  │  │  │  };                                                         │   │   │ |
|  │  │  │                                                             │   │   │ |
|  │  │  │  Buffer frames[POOL_SIZE];  /* Fixed-size pool */           │   │   │ |
|  │  │  │                                                             │   │   │ |
|  │  │  └────────────────────────────────────────────────────────────┘   │   │ |
|  │  │                                                                    │   │ |
|  │  │  Same patterns as kernel:                                          │   │ |
|  │  │  • LRU for eviction (like vmscan)                                  │   │ |
|  │  │  • Pinning (like mlock)                                            │   │ |
|  │  │  • Dirty tracking (like PG_dirty)                                  │   │ |
|  │  │  • Writeback (like pdflush/kworker)                                │   │ |
|  │  │                                                                    │   │ |
|  │  └──────────────────────────────────────────────────────────────────┘   │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  USER-SPACE APPLICATION: MEMORY-MAPPED REGIONS                               |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Managing multiple memory regions like VMAs:                             │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐   │ |
|  │  │                                                                    │   │ |
|  │  │  struct memory_region {                                            │   │ |
|  │  │      uintptr_t start;                                              │   │ |
|  │  │      uintptr_t end;                                                │   │ |
|  │  │      unsigned int flags;    /* READ, WRITE, EXEC */                │   │ |
|  │  │      struct rb_node rb;     /* For fast lookup (like VMA) */       │   │ |
|  │  │      void *private_data;                                           │   │ |
|  │  │  };                                                                │   │ |
|  │  │                                                                    │   │ |
|  │  │  struct address_space {                                            │   │ |
|  │  │      struct rb_root regions; /* Like mm_rb */                      │   │ |
|  │  │      pthread_rwlock_t lock;  /* Like mmap_sem */                   │   │ |
|  │  │  };                                                                │   │ |
|  │  │                                                                    │   │ |
|  │  │  Use cases:                                                        │   │ |
|  │  │  • Custom allocators                                               │   │ |
|  │  │  • JIT compilers (managing code regions)                           │   │ |
|  │  │  • Memory-mapped file managers                                     │   │ |
|  │  │                                                                    │   │ |
|  │  └──────────────────────────────────────────────────────────────────┘   │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**内核模式**：
- Linux 分离虚拟层（地址、VMA、页表）和物理层（页、区域、伙伴分配器）
- 好处：独立管理每层复杂度

**用户空间应用 1：Arena 分配器**
- "虚拟"层：用户分配、malloc 指针
- "物理"层：内存 arena（从 OS 获取的 mmap 区域）

**用户空间应用 2：缓冲池**
- 像 InnoDB、PostgreSQL 的缓冲池
- "虚拟"层：页 ID（文件 ID + 页号）
- "物理"层：内存中的缓冲帧
- 相同模式：LRU 驱逐、固定（pin）、脏跟踪、写回

**用户空间应用 3：内存映射区域**
- 像 VMA 一样管理多个内存区域
- 使用红黑树快速查找

---

## 2. 生命周期所有权规则

```
LIFECYCLE OWNERSHIP RULES
+=============================================================================+
|                                                                              |
|  KERNEL PATTERN: REFERENCE COUNTING                                          |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐   │ |
|  │  │                                                                    │   │ |
|  │  │  struct page:                                                      │   │ |
|  │  │  • get_page() - take reference                                     │   │ |
|  │  │  • put_page() - release reference                                  │   │ |
|  │  │  • When _count reaches 0 → page is freed                           │   │ |
|  │  │                                                                    │   │ |
|  │  │  struct mm_struct:                                                 │   │ |
|  │  │  • mmget(mm) - take reference                                      │   │ |
|  │  │  • mmput(mm) - release reference                                   │   │ |
|  │  │  • When mm_count reaches 0 → mm is freed                           │   │ |
|  │  │                                                                    │   │ |
|  │  │  RULE: Never access an object without holding a reference!         │   │ |
|  │  │                                                                    │   │ |
|  │  └──────────────────────────────────────────────────────────────────┘   │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  USER-SPACE: REFERENCE-COUNTED OBJECTS                                       |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐   │ |
|  │  │                                                                    │   │ |
|  │  │  /* C implementation */                                            │   │ |
|  │  │  struct refcounted {                                               │   │ |
|  │  │      atomic_int refcount;                                          │   │ |
|  │  │      void (*destructor)(struct refcounted *);                      │   │ |
|  │  │  };                                                                │   │ |
|  │  │                                                                    │   │ |
|  │  │  static inline void ref_get(struct refcounted *obj) {              │   │ |
|  │  │      assert(atomic_load(&obj->refcount) > 0);                      │   │ |
|  │  │      atomic_fetch_add(&obj->refcount, 1);                          │   │ |
|  │  │  }                                                                 │   │ |
|  │  │                                                                    │   │ |
|  │  │  static inline void ref_put(struct refcounted *obj) {              │   │ |
|  │  │      if (atomic_fetch_sub(&obj->refcount, 1) == 1) {               │   │ |
|  │  │          obj->destructor(obj);                                     │   │ |
|  │  │      }                                                             │   │ |
|  │  │  }                                                                 │   │ |
|  │  │                                                                    │   │ |
|  │  │  /* Example: connection pool */                                    │   │ |
|  │  │  struct connection {                                               │   │ |
|  │  │      struct refcounted base;                                       │   │ |
|  │  │      int socket_fd;                                                │   │ |
|  │  │      // ...                                                        │   │ |
|  │  │  };                                                                │   │ |
|  │  │                                                                    │   │ |
|  │  │  /* Thread 1: get connection, do work, release */                  │   │ |
|  │  │  conn = get_connection_from_pool();                                │   │ |
|  │  │  ref_get(&conn->base);                                             │   │ |
|  │  │  // ... use connection ...                                         │   │ |
|  │  │  ref_put(&conn->base);  // Safe even if pool freed it             │   │ |
|  │  │                                                                    │   │ |
|  │  └──────────────────────────────────────────────────────────────────┘   │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  KERNEL PATTERN: EXPLICIT OWNERSHIP TRANSFER                                 |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐   │ |
|  │  │                                                                    │   │ |
|  │  │  In Linux kernel, ownership is often explicit:                     │   │ |
|  │  │                                                                    │   │ |
|  │  │  /* Caller owns page after this */                                 │   │ |
|  │  │  page = alloc_page(GFP_KERNEL);                                    │   │ |
|  │  │                                                                    │   │ |
|  │  │  /* Transfer ownership to page cache */                            │   │ |
|  │  │  add_to_page_cache(page, mapping, index);                          │   │ |
|  │  │  /* Page cache now owns page - we should not free it */            │   │ |
|  │  │                                                                    │   │ |
|  │  │  Comments document ownership:                                      │   │ |
|  │  │  • "Caller must hold reference"                                    │   │ |
|  │  │  • "This function takes ownership"                                 │   │ |
|  │  │  • "Caller is responsible for freeing"                             │   │ |
|  │  │                                                                    │   │ |
|  │  └──────────────────────────────────────────────────────────────────┘   │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  USER-SPACE: OWNERSHIP DOCUMENTATION                                         |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐   │ |
|  │  │                                                                    │   │ |
|  │  │  /* OWNERSHIP: Caller owns returned buffer */                      │   │ |
|  │  │  char *read_file(const char *path);                                │   │ |
|  │  │                                                                    │   │ |
|  │  │  /* OWNERSHIP: Takes ownership of 'data' */                        │   │ |
|  │  │  void queue_push(struct queue *q, void *data);                     │   │ |
|  │  │                                                                    │   │ |
|  │  │  /* OWNERSHIP: Borrows 'data', does not take ownership */          │   │ |
|  │  │  void queue_peek(struct queue *q, const void *data);               │   │ |
|  │  │                                                                    │   │ |
|  │  │  /* Modern C++/Rust approach: use type system */                   │   │ |
|  │  │  std::unique_ptr<Buffer> read_file(path);  // Caller owns          │   │ |
|  │  │  void process(std::shared_ptr<Data> data); // Shared ownership     │   │ |
|  │  │  void peek(const Data& data);              // Borrowed, no own     │   │ |
|  │  │                                                                    │   │ |
|  │  └──────────────────────────────────────────────────────────────────┘   │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  KERNEL PATTERN: HIERARCHICAL OWNERSHIP                                      |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐   │ |
|  │  │                                                                    │   │ |
|  │  │  task_struct ───► mm_struct ───► vm_area_struct                    │   │ |
|  │  │       │                │                │                          │   │ |
|  │  │       │ owns           │ owns           │ owns                     │   │ |
|  │  │       ▼                ▼                ▼                          │   │ |
|  │  │  When task exits   When mm is     VMAs freed with mm               │   │ |
|  │  │  mm is released    destroyed                                       │   │ |
|  │  │                                                                    │   │ |
|  │  │  Teardown is top-down: exit_mm() → exit_mmap() → remove VMAs       │   │ |
|  │  │                                                                    │   │ |
|  │  └──────────────────────────────────────────────────────────────────┘   │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  USER-SPACE: HIERARCHICAL CLEANUP                                            |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐   │ |
|  │  │                                                                    │   │ |
|  │  │  struct server {                                                   │   │ |
|  │  │      struct connection_pool *pool;   /* Server owns pool */        │   │ |
|  │  │      struct session_list *sessions;  /* Server owns sessions */    │   │ |
|  │  │  };                                                                │   │ |
|  │  │                                                                    │   │ |
|  │  │  void server_destroy(struct server *s) {                           │   │ |
|  │  │      /* Top-down teardown */                                       │   │ |
|  │  │      session_list_destroy(s->sessions);  /* First children */      │   │ |
|  │  │      connection_pool_destroy(s->pool);   /* Then resources */      │   │ |
|  │  │      free(s);                            /* Finally self */        │   │ |
|  │  │  }                                                                 │   │ |
|  │  │                                                                    │   │ |
|  │  └──────────────────────────────────────────────────────────────────┘   │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**内核模式：引用计数**
- `get_page()`/`put_page()`：获取/释放引用
- 规则：永远不要在没有引用的情况下访问对象

**用户空间：引用计数对象**
- 实现 `ref_get()`/`ref_put()`
- 适用于连接池等共享资源

**内核模式：显式所有权转移**
- 注释记录所有权："调用者必须持有引用"、"此函数获取所有权"
- 用户空间：使用 `std::unique_ptr`（唯一所有权）、`std::shared_ptr`（共享所有权）

**内核模式：层次所有权**
- `task_struct` → `mm_struct` → `vm_area_struct`
- 自顶向下销毁

**用户空间：层次清理**
- 先销毁子对象，再销毁自身

---

## 3. 何时不应复制内核内存模式

```
WHEN NOT TO COPY KERNEL MEMORY PATTERNS
+=============================================================================+
|                                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  PATTERN 1: OVER-ENGINEERING ALLOCATION                                  │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  BAD: Building a full slab allocator for simple needs            │    │ |
|  │  │                                                                  │    │ |
|  │  │  // 1000 lines of slab-like code for...                          │    │ |
|  │  │  struct user {                                                   │    │ |
|  │  │      char name[64];                                              │    │ |
|  │  │      int age;                                                    │    │ |
|  │  │  };                                                              │    │ |
|  │  │  // ...when you create 100 users total                           │    │ |
|  │  │                                                                  │    │ |
|  │  │  GOOD: Just use malloc()                                         │    │ |
|  │  │                                                                  │    │ |
|  │  │  struct user *users[100];                                        │    │ |
|  │  │  for (int i = 0; i < 100; i++) {                                 │    │ |
|  │  │      users[i] = malloc(sizeof(struct user));                     │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  WHY kernel needs slab:                                          │    │ |
|  │  │  • Allocates millions of objects per second                      │    │ |
|  │  │  • Cannot afford malloc() overhead                               │    │ |
|  │  │  • Needs cache coloring for performance                          │    │ |
|  │  │                                                                  │    │ |
|  │  │  WHY you probably don't:                                         │    │ |
|  │  │  • Modern malloc() is very fast                                  │    │ |
|  │  │  • Your allocation rate is much lower                            │    │ |
|  │  │  • Maintenance cost exceeds benefit                              │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  PATTERN 2: COPYING PAGE CACHE FOR SMALL DATA                            │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  BAD: Building page cache for config files                       │    │ |
|  │  │                                                                  │    │ |
|  │  │  // LRU cache, dirty tracking, writeback threads...              │    │ |
|  │  │  // For 10 config files of 1KB each                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  GOOD: Just read files into memory                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  char *config = read_entire_file("config.json");                 │    │ |
|  │  │  // OS page cache handles caching for you!                       │    │ |
|  │  │                                                                  │    │ |
|  │  │  WHEN page cache pattern IS useful:                              │    │ |
|  │  │  • Database buffer pool (bypassing OS cache)                     │    │ |
|  │  │  • Managing GBs of data with custom eviction                     │    │ |
|  │  │  • Direct I/O (O_DIRECT) scenarios                               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  PATTERN 3: UNNECESSARY REFERENCE COUNTING                               │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  BAD: Reference counting everything                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  struct config {                                                 │    │ |
|  │  │      atomic_int refcount;  // WHY? Only one owner!               │    │ |
|  │  │      char *values;                                               │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  GOOD: Clear single ownership                                    │    │ |
|  │  │                                                                  │    │ |
|  │  │  struct config {                                                 │    │ |
|  │  │      char *values;                                               │    │ |
|  │  │  };                                                              │    │ |
|  │  │  // main() owns config, frees it on exit                         │    │ |
|  │  │                                                                  │    │ |
|  │  │  USE refcount when:                                              │    │ |
|  │  │  • Multiple threads/owners need access                           │    │ |
|  │  │  • Ownership is truly shared                                     │    │ |
|  │  │  • Object lifetime is unpredictable                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  DON'T USE refcount when:                                        │    │ |
|  │  │  • Single owner is clear                                         │    │ |
|  │  │  • Lifetime matches scope (RAII)                                 │    │ |
|  │  │  • Can use unique_ptr/ownership transfer                         │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  PATTERN 4: PREMATURE NUMA OPTIMIZATION                                  │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  BAD: NUMA-aware allocation for small application                │    │ |
|  │  │                                                                  │    │ |
|  │  │  // Complex NUMA topology detection                              │    │ |
|  │  │  // Per-node allocators                                          │    │ |
|  │  │  // Thread-to-node affinity                                      │    │ |
|  │  │  // ... for a web server with 100MB working set                  │    │ |
|  │  │                                                                  │    │ |
|  │  │  GOOD: Let the OS handle it                                      │    │ |
|  │  │                                                                  │    │ |
|  │  │  // OS first-touch policy usually works fine                     │    │ |
|  │  │  // Profile first, optimize if needed                            │    │ |
|  │  │                                                                  │    │ |
|  │  │  WHEN NUMA matters:                                              │    │ |
|  │  │  • Working set >> L3 cache (many GBs)                            │    │ |
|  │  │  • Memory bandwidth limited                                      │    │ |
|  │  │  • Measured remote access overhead                               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**何时不应复制内核模式**：

**模式 1：过度工程分配器**
- 错误：为简单需求构建完整的 slab 分配器
- 正确：直接使用 `malloc()`
- 内核需要 slab 因为每秒分配数百万对象，你可能不需要

**模式 2：为小数据复制页缓存**
- 错误：为 10 个 1KB 配置文件构建 LRU 缓存
- 正确：直接读取文件，OS 页缓存已经为你处理
- 何时有用：数据库缓冲池、管理 GB 级数据

**模式 3：不必要的引用计数**
- 错误：对只有一个所有者的对象进行引用计数
- 正确：明确的单一所有权
- 使用引用计数：多线程共享、所有权真正共享、生命周期不可预测

**模式 4：过早的 NUMA 优化**
- 错误：为 100MB 工作集的小应用做 NUMA 感知分配
- 正确：让 OS 处理（首次触摸策略通常工作良好）
- 何时需要 NUMA：工作集 >> L3 缓存、内存带宽受限

---

## 4. 常见误用模式

```
COMMON MISUSE PATTERNS
+=============================================================================+
|                                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  MISUSE 1: USE-AFTER-FREE (释放后使用)                                   │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  /* BAD: Classic use-after-free */                               │    │ |
|  │  │  void process_request(struct request *req) {                     │    │ |
|  │  │      queue_work(req);                                            │    │ |
|  │  │      free(req);           // Worker still using req!             │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* GOOD: Worker frees when done */                              │    │ |
|  │  │  void process_request(struct request *req) {                     │    │ |
|  │  │      queue_work(req);     // Worker takes ownership              │    │ |
|  │  │      // Don't free here!                                         │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  void worker(struct request *req) {                              │    │ |
|  │  │      handle(req);                                                │    │ |
|  │  │      free(req);           // Worker frees                        │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  MISUSE 2: FORGOTTEN REFERENCE (忘记引用)                                │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  /* BAD: Accessing without reference */                          │    │ |
|  │  │  struct connection *conn = pool_get_connection();                │    │ |
|  │  │  // Don't take reference                                         │    │ |
|  │  │  //                                                              │    │ |
|  │  │  // ... another thread closes connection ...                     │    │ |
|  │  │  //                                                              │    │ |
|  │  │  conn->socket;  // CRASH! conn was freed                         │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* GOOD: Take reference while using */                          │    │ |
|  │  │  struct connection *conn = pool_get_connection();                │    │ |
|  │  │  ref_get(conn);           // Take reference                      │    │ |
|  │  │  //                                                              │    │ |
|  │  │  // ... safe to use ...                                          │    │ |
|  │  │  //                                                              │    │ |
|  │  │  ref_put(conn);           // Release reference                   │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  MISUSE 3: REFERENCE LEAK (引用泄漏)                                     │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  /* BAD: Forgetting to release */                                │    │ |
|  │  │  void handle_data(struct data *d) {                              │    │ |
|  │  │      ref_get(d);                                                 │    │ |
|  │  │      if (error) {                                                │    │ |
|  │  │          return;          // LEAK! Forgot ref_put()              │    │ |
|  │  │      }                                                           │    │ |
|  │  │      process(d);                                                 │    │ |
|  │  │      ref_put(d);                                                 │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* GOOD: Single exit point or cleanup label */                  │    │ |
|  │  │  void handle_data(struct data *d) {                              │    │ |
|  │  │      ref_get(d);                                                 │    │ |
|  │  │      if (error)                                                  │    │ |
|  │  │          goto out;                                               │    │ |
|  │  │      process(d);                                                 │    │ |
|  │  │  out:                                                            │    │ |
|  │  │      ref_put(d);                                                 │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* BETTER: RAII in C++ */                                       │    │ |
|  │  │  void handle_data(std::shared_ptr<Data> d) {                     │    │ |
|  │  │      if (error) return;   // Destructor handles cleanup          │    │ |
|  │  │      process(d);                                                 │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  MISUSE 4: LOCK INVERSION WITH MEMORY (内存锁反转)                       │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  /* BAD: Allocating while holding lock */                        │    │ |
|  │  │  pthread_mutex_lock(&global_lock);                               │    │ |
|  │  │  data = malloc(size);     // malloc may need to reclaim memory   │    │ |
|  │  │                           // reclaim may need global_lock!       │    │ |
|  │  │  // DEADLOCK potential                                           │    │ |
|  │  │  pthread_mutex_unlock(&global_lock);                             │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* GOOD: Allocate before taking lock */                         │    │ |
|  │  │  data = malloc(size);                                            │    │ |
|  │  │  pthread_mutex_lock(&global_lock);                               │    │ |
|  │  │  // Use data...                                                  │    │ |
|  │  │  pthread_mutex_unlock(&global_lock);                             │    │ |
|  │  │                                                                  │    │ |
|  │  │  This is why kernel has GFP_ATOMIC:                              │    │ |
|  │  │  • GFP_KERNEL: Can sleep, may reclaim (not under spinlock)       │    │ |
|  │  │  • GFP_ATOMIC: Cannot sleep, from interrupt/spinlock context     │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  SUMMARY: BEST PRACTICES                                                     |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  1. Document ownership clearly (comments, types, naming)                │ |
|  │                                                                          │ |
|  │  2. Use reference counting only when truly shared                       │ |
|  │                                                                          │ |
|  │  3. Prefer single ownership with explicit transfer                      │ |
|  │                                                                          │ |
|  │  4. Use RAII or cleanup labels for error paths                          │ |
|  │                                                                          │ |
|  │  5. Don't allocate under locks when possible                            │ |
|  │                                                                          │ |
|  │  6. Match complexity to actual problem scale                            │ |
|  │                                                                          │ |
|  │  7. Use debugging tools (ASan, valgrind, KASAN)                         │ |
|  │                                                                          │ |
|  │  8. Profile before optimizing - don't copy kernel patterns blindly      │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**常见误用模式**：

**误用 1：释放后使用**
- 错误：释放请求后工作线程仍在使用
- 正确：工作线程完成后释放，或明确所有权转移

**误用 2：忘记引用**
- 错误：不获取引用就访问共享对象
- 正确：使用时获取引用，使用后释放

**误用 3：引用泄漏**
- 错误：错误路径忘记释放引用
- 正确：单一退出点或 goto cleanup 标签
- 更好：C++ RAII（析构函数自动清理）

**误用 4：内存锁反转**
- 错误：持有锁时分配内存（malloc 可能需要回收，回收可能需要锁）
- 正确：先分配，再获取锁

**最佳实践总结**：
1. 清晰记录所有权
2. 只在真正共享时使用引用计数
3. 优先单一所有权和显式转移
4. 使用 RAII 或 cleanup 标签
5. 尽量不在锁下分配
6. 匹配复杂度与问题规模
7. 使用调试工具（ASan、valgrind）
8. 先分析再优化
