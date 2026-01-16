# Section 12: Memory Management

PostgreSQL uses a sophisticated memory management system that differs
significantly from typical malloc/free patterns.

---

## 12.1 Shared Memory

```
Shared Memory Layout:
+------------------------------------------------------------------+
|                       Shared Memory                              |
+------------------------------------------------------------------+
|                                                                  |
|  +------------------------+                                      |
|  |    Shared Buffers      |  (configurable, e.g., 256MB-8GB+)    |
|  |    (Buffer Pool)       |  Caches data pages from disk         |
|  +------------------------+                                      |
|                                                                  |
|  +------------------------+                                      |
|  |    WAL Buffers         |  (wal_buffers, e.g., 16MB)           |
|  |                        |  Caches WAL before flush             |
|  +------------------------+                                      |
|                                                                  |
|  +------------------------+                                      |
|  |    Lock Tables         |  Stores all lock information         |
|  +------------------------+                                      |
|                                                                  |
|  +------------------------+                                      |
|  |    Proc Array          |  Active backend process info         |
|  +------------------------+                                      |
|                                                                  |
|  +------------------------+                                      |
|  |    Other Structures    |  CLOG buffers, etc.                  |
|  +------------------------+                                      |
|                                                                  |
+------------------------------------------------------------------+

共享内存布局：

Shared Buffers（共享缓冲区）：
- 最大的共享内存区域
- 缓存磁盘数据页
- 所有后端共享访问
- 典型配置：系统内存的25%

WAL Buffers：
- 缓存待写入的WAL记录
- 相对较小（默认约16MB）

Lock Tables：
- 存储所有锁信息
- 支持锁冲突检测和死锁检测

Proc Array：
- 活跃后端进程信息
- 用于快照管理
```

### Buffer Pool Management

```
Buffer Pool Operations:
+------------------------------------------------------------------+
|                                                                  |
|  Read Request:                                                   |
|  1. Hash page identifier (relfilenode, block#)                   |
|  2. Look up in buffer hash table                                 |
|  3. If found: pin buffer, return                                 |
|  4. If not found:                                                |
|     a. Get free buffer (from freelist or clock sweep)            |
|     b. If buffer dirty: write to disk first                      |
|     c. Read page from disk into buffer                           |
|     d. Insert into hash table                                    |
|     e. Pin buffer, return                                        |
|                                                                  |
|  Buffer States:                                                  |
|  +--------+  +--------+  +--------+  +--------+                  |
|  | Free   |  | Clean  |  | Dirty  |  | Pinned |                  |
|  | (empty)|  | (data) |  | (mod)  |  | (in use)|                 |
|  +--------+  +--------+  +--------+  +--------+                  |
|                                                                  |
+------------------------------------------------------------------+

缓冲池操作：

读取流程：
1. 计算页面哈希值
2. 在缓冲哈希表中查找
3. 如果找到：固定缓冲区，返回
4. 如果没找到：
   - 获取空闲缓冲区
   - 如果缓冲区是脏的，先写回磁盘
   - 从磁盘读取页面
   - 插入哈希表

缓冲区状态：
- Free：空闲，可分配
- Clean：包含数据，与磁盘一致
- Dirty：包含数据，已修改
- Pinned：正在使用中
```

Source: `src/backend/storage/buffer/bufmgr.c`

---

## 12.2 Memory Contexts

```
Memory Context Hierarchy:
+------------------------------------------------------------------+
|                                                                  |
|  TopMemoryContext (lives for server lifetime)                    |
|  |                                                               |
|  +-- PostmasterContext                                           |
|  |                                                               |
|  +-- TopTransactionContext (per transaction)                     |
|  |   |                                                           |
|  |   +-- CurTransactionContext                                   |
|  |   |                                                           |
|  |   +-- Per-statement contexts                                  |
|  |                                                               |
|  +-- MessageContext (per client message)                         |
|  |                                                               |
|  +-- CacheMemoryContext (catalog caches)                         |
|                                                                  |
+------------------------------------------------------------------+

内存上下文层次：

TopMemoryContext：
- 服务器生命周期
- 永不释放

TopTransactionContext：
- 事务生命周期
- COMMIT/ROLLBACK时释放所有内容

MessageContext：
- 单个客户端消息生命周期
- 处理完消息后释放

设计优点：
- 批量释放（释放上下文=释放所有内存）
- 无内存泄漏（生命周期明确）
- 无需跟踪每个分配
```

### Why Memory Contexts Instead of malloc/free

```
Problem with malloc/free:
+------------------------------------------------------------------+
|                                                                  |
|  void process_query() {                                          |
|      char *str1 = malloc(100);                                   |
|      char *str2 = malloc(200);                                   |
|      // ... complex processing ...                               |
|      if (error) {                                                |
|          // Need to free str1, str2, and everything else!        |
|          // Easy to miss something = MEMORY LEAK                 |
|      }                                                           |
|      free(str1);                                                 |
|      free(str2);                                                 |
|  }                                                               |
|                                                                  |
+------------------------------------------------------------------+

Memory Context Solution:
+------------------------------------------------------------------+
|                                                                  |
|  void process_query() {                                          |
|      MemoryContext ctx = AllocSetContextCreate(                  |
|          TopTransactionContext, "query_ctx", ...);               |
|      MemoryContext old = MemoryContextSwitchTo(ctx);             |
|                                                                  |
|      char *str1 = palloc(100);  // allocated in ctx              |
|      char *str2 = palloc(200);  // allocated in ctx              |
|      // ... complex processing ...                               |
|                                                                  |
|      MemoryContextSwitchTo(old);                                 |
|      MemoryContextDelete(ctx);  // Frees EVERYTHING in ctx       |
|  }                                                               |
|                                                                  |
+------------------------------------------------------------------+

为什么使用内存上下文：

malloc/free的问题：
- 需要跟踪每次分配
- 错误路径容易漏释放
- 内存泄漏难以发现

内存上下文的优点：
- 批量释放：释放上下文=释放所有内容
- 自动清理：事务结束自动释放事务内存
- 简单可靠：无需逐个跟踪
- 性能好：避免频繁malloc/free
```

### Memory Context API

```c
/* Key functions in src/backend/utils/mmgr/mcxt.c */

/* Create a new context */
MemoryContext AllocSetContextCreate(
    MemoryContext parent,
    const char *name,
    Size minContextSize,
    Size initBlockSize,
    Size maxBlockSize);

/* Allocate memory in current context */
void *palloc(Size size);
void *palloc0(Size size);  /* zero-initialized */

/* Free memory (rarely needed) */
void pfree(void *pointer);

/* Switch context */
MemoryContext MemoryContextSwitchTo(MemoryContext context);

/* Delete context and all children */
void MemoryContextDelete(MemoryContext context);

/* Reset context (free all memory but keep context) */
void MemoryContextReset(MemoryContext context);
```

---

## Summary

```
+------------------------------------------------------------------+
|              Memory Management Summary                           |
+------------------------------------------------------------------+
|                                                                  |
|  Shared Memory:                                                  |
|  - Shared Buffers: Cache for data pages                          |
|  - WAL Buffers: Cache for WAL records                            |
|  - Lock Tables: Lock information                                 |
|  - Proc Array: Process information                               |
|                                                                  |
|  Memory Contexts:                                                |
|  - Hierarchical memory management                                |
|  - Bulk deallocation by deleting context                         |
|  - Use palloc/pfree instead of malloc/free                       |
|  - Lifetime tied to operation (transaction, query, etc.)         |
|                                                                  |
|  Key Benefits:                                                   |
|  - No memory leaks (automatic cleanup)                           |
|  - Simple error handling                                         |
|  - Good performance (batch operations)                           |
|                                                                  |
+------------------------------------------------------------------+

内存管理总结：

共享内存：
- Shared Buffers：数据页缓存
- WAL Buffers：WAL记录缓存
- 配置参数：shared_buffers, wal_buffers

内存上下文：
- 层次化内存管理
- 批量释放
- 使用palloc/pfree
- 生命周期与操作绑定

源代码：
- src/backend/storage/buffer/ - 缓冲管理
- src/backend/utils/mmgr/ - 内存上下文
```
