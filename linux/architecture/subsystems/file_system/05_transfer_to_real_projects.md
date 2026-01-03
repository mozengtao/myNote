# TRANSFER｜应用到实际项目

## 1. 插件式 I/O 抽象

```
PLUGIN-STYLE I/O ABSTRACTION
+=============================================================================+
|                                                                              |
|  VFS PATTERN: OPS-TABLE POLYMORPHISM                                         |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Linux VFS Design:                                                       │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  struct file_operations {                                        │    │ |
|  │  │      ssize_t (*read)(...);                                       │    │ |
|  │  │      ssize_t (*write)(...);                                      │    │ |
|  │  │      int (*open)(...);                                           │    │ |
|  │  │      int (*release)(...);                                        │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  // Runtime dispatch via function pointer                        │    │ |
|  │  │  file->f_op->read(file, buf, count, pos);                        │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  Key insight: Interface defined ONCE, implementations MANY               │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  TRANSFERRING TO USER-SPACE: STORAGE BACKEND ABSTRACTION                     |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Example: Database storage engine abstraction                            │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  // C: ops-table style (like VFS)                                │    │ |
|  │  │  typedef struct storage_ops {                                    │    │ |
|  │  │      int (*open)(struct storage *s, const char *path);           │    │ |
|  │  │      int (*close)(struct storage *s);                            │    │ |
|  │  │      int (*get)(struct storage *s, const void *key, size_t klen, │    │ |
|  │  │                 void **val, size_t *vlen);                       │    │ |
|  │  │      int (*put)(struct storage *s, const void *key, size_t klen, │    │ |
|  │  │                 const void *val, size_t vlen);                   │    │ |
|  │  │      int (*del)(struct storage *s, const void *key, size_t klen);│    │ |
|  │  │      int (*sync)(struct storage *s);                             │    │ |
|  │  │  } storage_ops_t;                                                │    │ |
|  │  │                                                                  │    │ |
|  │  │  typedef struct storage {                                        │    │ |
|  │  │      const storage_ops_t *ops;    // Like file->f_op             │    │ |
|  │  │      void *private_data;          // Backend-specific state      │    │ |
|  │  │  } storage_t;                                                    │    │ |
|  │  │                                                                  │    │ |
|  │  │  // Dispatch macro (like VFS vfs_read)                           │    │ |
|  │  │  #define storage_get(s, k, kl, v, vl) \                          │    │ |
|  │  │      ((s)->ops->get((s), (k), (kl), (v), (vl)))                  │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  Implementations:                                                        │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  // Memory backend (like ramfs)                                  │    │ |
|  │  │  static const storage_ops_t mem_storage_ops = {                  │    │ |
|  │  │      .open  = mem_open,                                          │    │ |
|  │  │      .close = mem_close,                                         │    │ |
|  │  │      .get   = mem_get,                                           │    │ |
|  │  │      .put   = mem_put,                                           │    │ |
|  │  │      .del   = mem_del,                                           │    │ |
|  │  │      .sync  = mem_sync,   // no-op for memory                    │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  // File backend (like ext4)                                     │    │ |
|  │  │  static const storage_ops_t file_storage_ops = {                 │    │ |
|  │  │      .open  = file_open,                                         │    │ |
|  │  │      .close = file_close,                                        │    │ |
|  │  │      .get   = file_get,                                          │    │ |
|  │  │      .put   = file_put,                                          │    │ |
|  │  │      .del   = file_del,                                          │    │ |
|  │  │      .sync  = file_sync,   // fsync()                            │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  // Network backend (like NFS)                                   │    │ |
|  │  │  static const storage_ops_t net_storage_ops = {                  │    │ |
|  │  │      .open  = net_open,                                          │    │ |
|  │  │      .close = net_close,                                         │    │ |
|  │  │      .get   = net_get,     // RPC call                           │    │ |
|  │  │      .put   = net_put,     // RPC call                           │    │ |
|  │  │      .del   = net_del,                                           │    │ |
|  │  │      .sync  = net_sync,                                          │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  C++ VERSION WITH VIRTUAL FUNCTIONS                                          |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  class IStorage {                                                        │ |
|  │  public:                                                                 │ |
|  │      virtual ~IStorage() = default;                                      │ |
|  │      virtual int open(const std::string& path) = 0;                      │ |
|  │      virtual int close() = 0;                                            │ |
|  │      virtual int get(const Key& k, Value* v) = 0;                        │ |
|  │      virtual int put(const Key& k, const Value& v) = 0;                  │ |
|  │      virtual int del(const Key& k) = 0;                                  │ |
|  │      virtual int sync() = 0;                                             │ |
|  │  };                                                                      │ |
|  │                                                                          │ |
|  │  class MemStorage : public IStorage { /* ... */ };                       │ |
|  │  class FileStorage : public IStorage { /* ... */ };                      │ |
|  │  class NetStorage : public IStorage { /* ... */ };                       │ |
|  │                                                                          │ |
|  │  // Factory pattern (like register_filesystem)                           │ |
|  │  std::unique_ptr<IStorage> createStorage(const std::string& type) {      │ |
|  │      if (type == "memory") return std::make_unique<MemStorage>();        │ |
|  │      if (type == "file") return std::make_unique<FileStorage>();         │ |
|  │      if (type == "network") return std::make_unique<NetStorage>();       │ |
|  │      throw std::runtime_error("Unknown storage type");                   │ |
|  │  }                                                                       │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**插件式 I/O 抽象**：

VFS 模式：操作表多态
- 接口定义一次，实现多个
- 运行时通过函数指针分发

**迁移到用户空间**：存储后端抽象
- C 风格：定义 `storage_ops_t`（类似 `file_operations`）
- 每个后端提供实现：内存后端（如 ramfs）、文件后端（如 ext4）、网络后端（如 NFS）

**C++ 版本**：
- 使用虚函数代替函数指针
- 工厂模式代替 `register_filesystem()`

---

```
PLUGIN REGISTRATION PATTERN
+=============================================================================+
|                                                                              |
|  VFS PATTERN: FILESYSTEM REGISTRATION                                        |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  // Linux kernel                                                         │ |
|  │  register_filesystem(&ext4_fs_type);  // Add ext4 to list               │ |
|  │  register_filesystem(&nfs_fs_type);   // Add NFS to list                │ |
|  │                                                                          │ |
|  │  // Mount lookup                                                         │ |
|  │  get_fs_type("ext4");  // Returns &ext4_fs_type                         │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  USER-SPACE TRANSFER: PLUGIN REGISTRY                                        |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  // C implementation                                                     │ |
|  │  typedef struct storage_type {                                           │ |
|  │      const char *name;                                                   │ |
|  │      storage_t *(*create)(const char *config);                           │ |
|  │      void (*destroy)(storage_t *s);                                      │ |
|  │      struct storage_type *next;    // Linked list                        │ |
|  │  } storage_type_t;                                                       │ |
|  │                                                                          │ |
|  │  static storage_type_t *storage_types = NULL;                            │ |
|  │  static pthread_mutex_t types_lock = PTHREAD_MUTEX_INITIALIZER;          │ |
|  │                                                                          │ |
|  │  int register_storage_type(storage_type_t *type) {                       │ |
|  │      pthread_mutex_lock(&types_lock);                                    │ |
|  │      type->next = storage_types;                                         │ |
|  │      storage_types = type;                                               │ |
|  │      pthread_mutex_unlock(&types_lock);                                  │ |
|  │      return 0;                                                           │ |
|  │  }                                                                       │ |
|  │                                                                          │ |
|  │  storage_type_t *find_storage_type(const char *name) {                   │ |
|  │      storage_type_t *t;                                                  │ |
|  │      pthread_mutex_lock(&types_lock);                                    │ |
|  │      for (t = storage_types; t; t = t->next) {                           │ |
|  │          if (strcmp(t->name, name) == 0) break;                          │ |
|  │      }                                                                   │ |
|  │      pthread_mutex_unlock(&types_lock);                                  │ |
|  │      return t;                                                           │ |
|  │  }                                                                       │ |
|  │                                                                          │ |
|  │  // Usage (like mount("ext4", "/mnt", ...))                              │ |
|  │  storage_type_t *type = find_storage_type("memory");                     │ |
|  │  storage_t *store = type->create("size=1G");                             │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**插件注册模式**：

VFS 模式：文件系统注册
- `register_filesystem()` 添加到列表
- `get_fs_type()` 按名称查找

用户空间迁移：插件注册表
- 定义 `storage_type_t`（类似 `file_system_type`）
- `register_storage_type()` 添加到链表
- `find_storage_type()` 按名称查找

---

## 2. 缓存 vs 一致性权衡

```
CACHE VS CONSISTENCY TRADE-OFFS
+=============================================================================+
|                                                                              |
|  VFS LESSONS ON CACHING                                                      |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Linux VFS caching philosophy:                                           │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  1. CACHE AGGRESSIVELY, PROVIDE ESCAPE HATCHES                   │    │ |
|  │  │     • Default: buffer writes in page cache                       │    │ |
|  │  │     • Escape: O_DIRECT, O_SYNC, fsync()                          │    │ |
|  │  │                                                                  │    │ |
|  │  │  2. NEGATIVE CACHING IS VALUABLE                                 │    │ |
|  │  │     • Cache "file does not exist" (negative dentry)              │    │ |
|  │  │     • Avoids repeated lookups for nonexistent files              │    │ |
|  │  │                                                                  │    │ |
|  │  │  3. LAYERED CACHES                                               │    │ |
|  │  │     • dcache (name → inode)                                      │    │ |
|  │  │     • icache (inode metadata)                                    │    │ |
|  │  │     • page cache (file data)                                     │    │ |
|  │  │     Each layer has different invalidation rules                  │    │ |
|  │  │                                                                  │    │ |
|  │  │  4. EXPLICIT DURABILITY                                          │    │ |
|  │  │     • write() returns before data on disk                        │    │ |
|  │  │     • fsync() for durability guarantee                           │    │ |
|  │  │     • Application must request durability                        │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  APPLYING TO USER-SPACE SYSTEMS                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  PATTERN 1: Multi-layer cache with consistency rules                     │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Example: Distributed key-value store                            │    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌──────────────┐                                                │    │ |
|  │  │  │ Client Cache │ ◄── Local cache (like page cache)              │    │ |
|  │  │  │   (LRU)      │     Fast but may be stale                      │    │ |
|  │  │  └──────────────┘                                                │    │ |
|  │  │         │                                                        │    │ |
|  │  │         ▼                                                        │    │ |
|  │  │  ┌──────────────┐                                                │    │ |
|  │  │  │ Server Cache │ ◄── Shared cache (like icache)                 │    │ |
|  │  │  │ (Consistent) │     Consistent within server                   │    │ |
|  │  │  └──────────────┘                                                │    │ |
|  │  │         │                                                        │    │ |
|  │  │         ▼                                                        │    │ |
|  │  │  ┌──────────────┐                                                │    │ |
|  │  │  │  Persistent  │ ◄── Durable storage (like disk)                │    │ |
|  │  │  │   Storage    │     Slow but guaranteed                        │    │ |
|  │  │  └──────────────┘                                                │    │ |
|  │  │                                                                  │    │ |
|  │  │  Consistency options (like VFS):                                 │    │ |
|  │  │  • Default: eventual consistency (like buffered writes)          │    │ |
|  │  │  • Strong read: bypass client cache (like O_DIRECT)              │    │ |
|  │  │  • Sync write: wait for persistence (like O_SYNC)                │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  PATTERN 2: Cache invalidation strategies                                │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  From VFS:                                                       │    │ |
|  │  │                                                                  │    │ |
|  │  │  1. Reference counting (like inode i_count)                      │    │ |
|  │  │     • Object stays in cache while in use                         │    │ |
|  │  │     • Evict only when refcount → 0                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  2. LRU eviction (like dcache LRU)                               │    │ |
|  │  │     • Track access order                                         │    │ |
|  │  │     • Evict least recently used on pressure                      │    │ |
|  │  │                                                                  │    │ |
|  │  │  3. Revalidation callbacks (like d_revalidate)                   │    │ |
|  │  │     • Before using cached item, check if still valid             │    │ |
|  │  │     • Useful for distributed caches                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  4. Time-based expiry                                            │    │ |
|  │  │     • NFS attribute cache timeout                                │    │ |
|  │  │     • Simple but may serve stale data                            │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  PATTERN 3: Explicit sync semantics                                      │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  // User-space API inspired by VFS                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  struct write_options {                                          │    │ |
|  │  │      bool sync;         // Like O_SYNC                           │    │ |
|  │  │      bool direct;       // Like O_DIRECT, bypass cache           │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  int put(storage_t *s, key, value, write_options *opts);         │    │ |
|  │  │                                                                  │    │ |
|  │  │  // Explicit flush (like fsync)                                  │    │ |
|  │  │  int storage_sync(storage_t *s);                                 │    │ |
|  │  │                                                                  │    │ |
|  │  │  // Barrier (like fdatasync)                                     │    │ |
|  │  │  int storage_barrier(storage_t *s);                              │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**缓存 vs 一致性权衡**：

**VFS 缓存哲学**：
1. **积极缓存，提供逃生口**：默认缓冲写入，提供 O_DIRECT、fsync()
2. **负缓存有价值**：缓存"文件不存在"，避免重复查找
3. **分层缓存**：dcache、icache、page cache，各层有不同失效规则
4. **显式持久性**：write() 可能在数据到磁盘前返回，fsync() 保证持久

**应用到用户空间**：
- **模式 1**：多层缓存（客户端缓存 → 服务器缓存 → 持久存储）
- **模式 2**：缓存失效策略（引用计数、LRU 驱逐、重验证回调、时间过期）
- **模式 3**：显式同步语义（sync 选项、direct 选项、storage_sync()）

---

## 3. 常见反模式

```
COMMON ANTI-PATTERNS
+=============================================================================+
|                                                                              |
|  ANTI-PATTERN 1: MONOLITHIC OPS-TABLE                                        |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  BAD: One giant ops table for everything                                 │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  // 50+ function pointers in one struct                          │    │ |
|  │  │  struct storage_ops {                                            │    │ |
|  │  │      int (*open)(...);                                           │    │ |
|  │  │      int (*close)(...);                                          │    │ |
|  │  │      int (*read)(...);                                           │    │ |
|  │  │      int (*write)(...);                                          │    │ |
|  │  │      int (*transaction_begin)(...);                              │    │ |
|  │  │      int (*transaction_commit)(...);                             │    │ |
|  │  │      int (*snapshot_create)(...);                                │    │ |
|  │  │      int (*snapshot_delete)(...);                                │    │ |
|  │  │      int (*replicate)(...);                                      │    │ |
|  │  │      int (*compress)(...);                                       │    │ |
|  │  │      // ... 40 more                                              │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  PROBLEM: Most backends implement only 10%, rest are NULL        │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  GOOD: VFS-style separation into focused ops tables                      │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  // Like VFS: file_operations, inode_operations, super_operations│    │ |
|  │  │  struct storage_io_ops {       // Core I/O                       │    │ |
|  │  │      int (*read)(...);                                           │    │ |
|  │  │      int (*write)(...);                                          │    │ |
|  │  │      int (*sync)(...);                                           │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  struct storage_tx_ops {       // Transactions (optional)        │    │ |
|  │  │      int (*begin)(...);                                          │    │ |
|  │  │      int (*commit)(...);                                         │    │ |
|  │  │      int (*rollback)(...);                                       │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  struct storage_snapshot_ops { // Snapshots (optional)           │    │ |
|  │  │      int (*create)(...);                                         │    │ |
|  │  │      int (*delete)(...);                                         │    │ |
|  │  │      int (*restore)(...);                                        │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  struct storage {                                                │    │ |
|  │  │      const struct storage_io_ops *io_ops;       // Required      │    │ |
|  │  │      const struct storage_tx_ops *tx_ops;       // NULL if unsupported│ |
|  │  │      const struct storage_snapshot_ops *snap_ops; // NULL if unsupported│ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  // Check capability before use                                  │    │ |
|  │  │  if (store->tx_ops && store->tx_ops->begin)                      │    │ |
|  │  │      store->tx_ops->begin(store);                                │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  ANTI-PATTERN 2: IGNORING CACHE COHERENCY                                    |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  BAD: Cache without invalidation strategy                                │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  // Just cache forever                                           │    │ |
|  │  │  Value* get(Key k) {                                             │    │ |
|  │  │      if (cache.has(k)) return cache.get(k);                      │    │ |
|  │  │      Value* v = backend.get(k);                                  │    │ |
|  │  │      cache.put(k, v);   // Never invalidated!                    │    │ |
|  │  │      return v;                                                   │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  PROBLEMS:                                                       │    │ |
|  │  │  • Memory grows unbounded                                        │    │ |
|  │  │  • Stale data if backend changes                                 │    │ |
|  │  │  • No way to force refresh                                       │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  GOOD: VFS-style cache management                                        │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  struct cache_entry {                                            │    │ |
|  │  │      Key key;                                                    │    │ |
|  │  │      Value *value;                                               │    │ |
|  │  │      atomic_int refcount;     // Like i_count                    │    │ |
|  │  │      time_t last_validated;   // For revalidation                │    │ |
|  │  │      bool dirty;              // Needs writeback?                │    │ |
|  │  │      struct list_head lru;    // For eviction                    │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  Value* get(Key k, bool force_revalidate) {                      │    │ |
|  │  │      cache_entry *e = cache_lookup(k);                           │    │ |
|  │  │      if (e) {                                                    │    │ |
|  │  │          // Revalidation check (like d_revalidate)               │    │ |
|  │  │          if (force_revalidate || needs_revalidation(e)) {        │    │ |
|  │  │              if (!revalidate(e)) {                               │    │ |
|  │  │                  cache_invalidate(e);                            │    │ |
|  │  │                  e = NULL;                                       │    │ |
|  │  │              }                                                   │    │ |
|  │  │          }                                                       │    │ |
|  │  │          if (e) {                                                │    │ |
|  │  │              lru_touch(e);  // Update access time                │    │ |
|  │  │              return cache_get_ref(e);                            │    │ |
|  │  │          }                                                       │    │ |
|  │  │      }                                                           │    │ |
|  │  │      // Cache miss - fetch and insert                            │    │ |
|  │  │      return cache_fetch_and_insert(k);                           │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  void cache_shrink(size_t target) {                              │    │ |
|  │  │      // LRU eviction (like dcache shrink)                        │    │ |
|  │  │      while (cache_size() > target) {                             │    │ |
|  │  │          cache_entry *e = lru_tail();                            │    │ |
|  │  │          if (e->refcount > 0) continue;  // Skip if in use       │    │ |
|  │  │          if (e->dirty) writeback(e);     // Flush before evict   │    │ |
|  │  │          cache_evict(e);                                         │    │ |
|  │  │      }                                                           │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  ANTI-PATTERN 3: SYNC-ONLY OR ASYNC-ONLY                                     |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  BAD: No flexibility in sync/async behavior                              │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  // Always sync - performance killer                             │    │ |
|  │  │  int put(Key k, Value v) {                                       │    │ |
|  │  │      write_to_disk(k, v);                                        │    │ |
|  │  │      fsync();  // Every write!                                   │    │ |
|  │  │      return 0;                                                   │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  // OR always async - durability nightmare                       │    │ |
|  │  │  int put(Key k, Value v) {                                       │    │ |
|  │  │      queue_for_background_write(k, v);                           │    │ |
|  │  │      return 0;  // Data may be lost on crash!                    │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  GOOD: VFS-style configurable durability                                 │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  enum write_mode {                                               │    │ |
|  │  │      WRITE_BUFFERED,    // Default, like normal write()          │    │ |
|  │  │      WRITE_SYNC,        // Like O_SYNC, wait for disk            │    │ |
|  │  │      WRITE_DSYNC,       // Like O_DSYNC, data only               │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  int put(Key k, Value v, enum write_mode mode) {                 │    │ |
|  │  │      write_to_cache(k, v);                                       │    │ |
|  │  │      mark_dirty(k);                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │      if (mode == WRITE_SYNC) {                                   │    │ |
|  │  │          flush_to_disk(k);                                       │    │ |
|  │  │          wait_for_completion();                                  │    │ |
|  │  │      } else if (mode == WRITE_DSYNC) {                           │    │ |
|  │  │          flush_data_only(k);                                     │    │ |
|  │  │          wait_for_completion();                                  │    │ |
|  │  │      }                                                           │    │ |
|  │  │      // WRITE_BUFFERED: background thread will flush             │    │ |
|  │  │      return 0;                                                   │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  // Explicit sync (like fsync)                                   │    │ |
|  │  │  int sync_all(void) {                                            │    │ |
|  │  │      flush_all_dirty();                                          │    │ |
|  │  │      wait_for_all_completions();                                 │    │ |
|  │  │      return 0;                                                   │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  ANTI-PATTERN 4: LIFECYCLE CONFUSION                                         |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  BAD: Unclear ownership and lifecycle                                    │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  // Who frees this?                                              │    │ |
|  │  │  Value* get(Key k) {                                             │    │ |
|  │  │      return malloc_and_copy(backend.get(k));                     │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  // Caller might:                                                │    │ |
|  │  │  // - Forget to free → memory leak                               │    │ |
|  │  │  // - Free while still cached → use-after-free                   │    │ |
|  │  │  // - Free twice → double-free                                   │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  GOOD: VFS-style reference counting                                      │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  // Like iget/iput pattern                                       │    │ |
|  │  │  Value* value_get(Key k) {                                       │    │ |
|  │  │      Value *v = lookup_or_create(k);                             │    │ |
|  │  │      atomic_inc(&v->refcount);  // Like iget incrementing i_count│    │ |
|  │  │      return v;                                                   │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  void value_put(Value *v) {                                      │    │ |
|  │  │      if (atomic_dec_and_test(&v->refcount)) {                    │    │ |
|  │  │          // Like iput when i_count reaches 0                     │    │ |
|  │  │          if (v->should_delete) {                                 │    │ |
|  │  │              // Like i_nlink == 0                                │    │ |
|  │  │              free_value(v);                                      │    │ |
|  │  │          } else {                                                │    │ |
|  │  │              // Keep in cache (like inode in icache)             │    │ |
|  │  │              add_to_lru(v);                                      │    │ |
|  │  │          }                                                       │    │ |
|  │  │      }                                                           │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  // Clear ownership model:                                       │    │ |
|  │  │  // - value_get() increments refcount                            │    │ |
|  │  │  // - value_put() decrements refcount                            │    │ |
|  │  │  // - Object freed only when refcount == 0 AND marked deleted    │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**常见反模式**：

**反模式 1：单体操作表**
- 坏：一个巨大 ops 表包含 50+ 函数指针，大多数后端只实现 10%
- 好：VFS 风格分离成专注的操作表（io_ops、tx_ops、snap_ops），可选能力为 NULL

**反模式 2：忽略缓存一致性**
- 坏：永久缓存，无失效策略（内存无界增长、过时数据）
- 好：VFS 风格缓存管理（引用计数、重验证、LRU 驱逐、脏跟踪）

**反模式 3：仅同步或仅异步**
- 坏：总是 fsync（性能杀手）或总是异步（持久性噩梦）
- 好：VFS 风格可配置持久性（WRITE_BUFFERED、WRITE_SYNC、WRITE_DSYNC）

**反模式 4：生命周期混乱**
- 坏：不清楚的所有权和生命周期（内存泄漏、use-after-free、double-free）
- 好：VFS 风格引用计数（value_get/value_put，类似 iget/iput）

---

```
SUMMARY: KEY LESSONS FROM VFS
+=============================================================================+
|                                                                              |
|  TRANSFERABLE PRINCIPLES                                                     |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  1. POLYMORPHISM VIA OPS-TABLES                                          │ |
|  │     • Define stable interfaces, let implementations vary                 │ |
|  │     • Separate interface tables by concern (I/O, transactions, etc.)     │ |
|  │     • NULL for unsupported operations                                    │ |
|  │                                                                          │ |
|  │  2. LAYERED CACHING                                                      │ |
|  │     • Cache at multiple levels (name cache, metadata cache, data cache)  │ |
|  │     • Each layer has its own invalidation rules                          │ |
|  │     • Provide bypass mechanisms (O_DIRECT equivalent)                    │ |
|  │                                                                          │ |
|  │  3. EXPLICIT DURABILITY                                                  │ |
|  │     • Default to performance (buffered writes)                           │ |
|  │     • Explicit sync for durability requirements                          │ |
|  │     • Document what guarantees each mode provides                        │ |
|  │                                                                          │ |
|  │  4. REFERENCE COUNTING FOR LIFECYCLE                                     │ |
|  │     • get() increments, put() decrements                                 │ |
|  │     • Object survives in cache until refcount → 0                        │ |
|  │     • Deletion deferred until safe                                       │ |
|  │                                                                          │ |
|  │  5. NAMESPACE AS FIRST-CLASS CONCEPT                                     │ |
|  │     • Separate naming from data (like dentry from inode)                 │ |
|  │     • Support hard links (multiple names → one object)                   │ |
|  │     • Cache negative lookups                                             │ |
|  │                                                                          │ |
|  │  6. PLUGGABLE BACKENDS                                                   │ |
|  │     • Registration mechanism for new backends                            │ |
|  │     • Discovery by name/type                                             │ |
|  │     • Consistent lifecycle (mount/umount equivalent)                     │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  WHEN NOT TO COPY VFS PATTERNS                                               |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  1. Simple single-backend systems                                        │ |
|  │     • If you only have one backend, skip the polymorphism                │ |
|  │     • Direct calls are simpler and faster                                │ |
|  │                                                                          │ |
|  │  2. Embedded/constrained environments                                    │ |
|  │     • VFS caching assumes available memory                               │ |
|  │     • Reference counting adds overhead                                   │ |
|  │                                                                          │ |
|  │  3. When you need strict ordering                                        │ |
|  │     • VFS allows reordering for performance                              │ |
|  │     • Some applications need strict sequential semantics                 │ |
|  │                                                                          │ |
|  │  4. When caching makes no sense                                          │ |
|  │     • One-shot read/write patterns                                       │ |
|  │     • Data changes on every access                                       │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**可迁移的原则**：

1. **通过操作表实现多态**：定义稳定接口，实现可变；按关注点分离接口表；不支持的操作为 NULL
2. **分层缓存**：多级缓存；各层有自己的失效规则；提供绕过机制
3. **显式持久性**：默认性能优先；显式同步用于持久性需求
4. **引用计数管理生命周期**：get() 增加，put() 减少；删除延迟到安全时
5. **命名作为一等概念**：名称与数据分离；支持硬链接；缓存负查找
6. **可插拔后端**：注册机制；按名称发现；一致的生命周期

**何时不应复制 VFS 模式**：
1. 简单单后端系统：直接调用更简单更快
2. 嵌入式/受限环境：VFS 缓存假设有可用内存
3. 需要严格排序时：VFS 允许重排序以提升性能
4. 缓存没有意义时：一次性读写模式；每次访问数据都变化
