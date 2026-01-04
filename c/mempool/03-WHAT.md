# WHAT | Concrete Memory Pool Designs

## 1. Pool Taxonomy

```
Memory Pool Classification
==========================

+------------------+------------------+------------------+------------------+
| Fixed-Size Pool  | Variable-Size    | Arena/Region     | Slab Allocator   |
|                  | Pool             | Allocator        |                  |
+------------------+------------------+------------------+------------------+
|                  |                  |                  |                  |
| [==][==][==][==] | [===][=][=====]  | [>>>>>>>>>>>>]   | Cache1: [==][==] |
| [==][==][==][==] | [==][====][===]  | [>>>>>>>>>>>>]   | Cache2: [===][===|
|                  |                  |                  | Cache3: [====]   |
|                  |                  |                  |                  |
+------------------+------------------+------------------+------------------+
| All objects same | Objects vary in  | Linear bump      | Per-type caches  |
| size             | size             | allocation       | with free lists  |
+------------------+------------------+------------------+------------------+
| Free list of     | Free list with   | No individual    | Constructed      |
| same-size slots  | coalescing       | free             | objects cached   |
+------------------+------------------+------------------+------------------+
| Object pools,    | General purpose  | Request-scoped   | Kernel objects,  |
| connection pools | replacement for  | allocations      | frequently       |
|                  | malloc           | (nginx pools)    | allocated types  |
+------------------+------------------+------------------+------------------+

nginx Pool Type: HYBRID
=======================

    +-----------------------------------------------+
    | nginx pool = Arena + Large Object Escape      |
    +-----------------------------------------------+
    |                                               |
    |  Small allocations (<= max):                  |
    |    -> Arena-style bump pointer                |
    |    -> No individual free                      |
    |                                               |
    |  Large allocations (> max):                   |
    |    -> Separate malloc                         |
    |    -> Tracked in linked list                  |
    |    -> CAN be individually freed               |
    |                                               |
    +-----------------------------------------------+
```

**说明（中文）：**

此分类展示了四种主要的内存池类型：

1. **固定大小池**：所有对象相同大小，使用相同大小槽位的空闲链表，适用于对象池、连接池
2. **可变大小池**：对象大小各异，需要合并的空闲链表，用于替代 malloc
3. **Arena/Region 分配器**：线性递增分配，无单独释放，适用于请求范围分配（nginx 属于此类）
4. **Slab 分配器**：按类型缓存，带空闲链表，适用于内核对象、频繁分配类型

**nginx 池类型：混合型**
- 小分配（≤ max）：Arena 风格指针递增，无单独释放
- 大分配（> max）：单独 malloc，链表追踪，可以单独释放

---

## 2. nginx Pool Architecture

```
nginx Pool Data Structures
==========================

ngx_pool_t (ngx_pool_s)                 ngx_pool_data_t
+---------------------------+           +------------------+
| d: ngx_pool_data_t        | ------+   | last: u_char*    |  --> next free byte
|   .last                   |       |   | end: u_char*     |  --> block end
|   .end                    |       +-> | next: ngx_pool_t*|  --> next block
|   .next                   |           | failed: ngx_uint |  --> allocation failures
|   .failed                 |           +------------------+
+---------------------------+
| max: size_t               |  --> threshold for small vs large
| current: ngx_pool_t*      |  --> current block for allocation
| chain: ngx_chain_t*       |  --> buffer chain (for I/O)
| large: ngx_pool_large_t*  |  --> large allocation list
| cleanup: ngx_pool_cleanup*|  --> cleanup handler list
| log: ngx_log_t*           |  --> logging
+---------------------------+


ngx_pool_large_t                        ngx_pool_cleanup_t
+------------------+                    +----------------------+
| next: *large     |  --> linked list   | handler: function*   |  --> cleanup function
| alloc: void*     |  --> malloc'd ptr  | data: void*          |  --> handler argument
+------------------+                    | next: *cleanup       |  --> linked list
                                        +----------------------+


Memory Layout of a Pool Block:
==============================

    Low Address                                            High Address
        |                                                       |
        v                                                       v
    +---+-------------------------------------------------------+
    |HDR|                    USABLE MEMORY                      |
    +---+-------------------------------------------------------+
    ^   ^                                                       ^
    |   |                                                       |
    p   p->d.last (initial)                                 p->d.end
        |
        +--- sizeof(ngx_pool_t) for first block
        +--- sizeof(ngx_pool_data_t) for subsequent blocks


First Block vs Subsequent Blocks:
=================================

    First Block (created by ngx_create_pool):
    +-----------------------------------------------------------------+
    | ngx_pool_t header | usable memory                               |
    | (full struct)     | (pool_size - sizeof(ngx_pool_t))            |
    +-----------------------------------------------------------------+
    ^                   ^                                             ^
    |                   |                                             |
    pool             pool->d.last                                 pool->d.end


    Subsequent Blocks (created by ngx_palloc_block):
    +-----------------------------------------------------------------+
    | ngx_pool_data_t   | usable memory                               |
    | (smaller header)  | (block_size - sizeof(ngx_pool_data_t))      |
    +-----------------------------------------------------------------+
    ^                   ^                                             ^
    |                   |                                             |
    new              new->d.last                                  new->d.end

    Note: Subsequent blocks use smaller header (ngx_pool_data_t only)
          because they don't need max, current, large, cleanup, etc.
```

**说明（中文）：**

此图展示了 nginx 内存池的数据结构：

**ngx_pool_t**：主结构体
- `d`：ngx_pool_data_t，包含 last（下一个空闲字节）、end（块结束）、next（下一块）、failed（分配失败次数）
- `max`：小/大分配的阈值
- `current`：当前用于分配的块
- `large`：大分配链表
- `cleanup`：清理处理器链表

**内存布局**：
- 第一个块：完整的 ngx_pool_t 头 + 可用内存
- 后续块：仅 ngx_pool_data_t 头 + 可用内存（头更小，因为不需要 max、current 等字段）

---

## 3. Control Flow

```
ngx_create_pool(size, log) - Pool Initialization
================================================

    +------------------+
    | ngx_memalign()   |  <-- Allocate aligned memory
    +--------+---------+
             |
             v
    +------------------+
    | Initialize d.last|  <-- Points after ngx_pool_t header
    | Initialize d.end |  <-- Points to end of block
    | d.next = NULL    |
    | d.failed = 0     |
    +--------+---------+
             |
             v
    +------------------+
    | Calculate max    |  <-- min(size - header, NGX_MAX_ALLOC_FROM_POOL)
    | current = self   |
    | large = NULL     |
    | cleanup = NULL   |
    +--------+---------+
             |
             v
         return pool

    Resulting Memory Layout:
    
    +--------+----------------------------------+
    |ngx_pool|       usable memory              |
    +--------+----------------------------------+
    ^        ^                                  ^
    |        |                                  |
    p      p->d.last                        p->d.end


ngx_palloc(pool, size) - Allocation Path
========================================

    +------------------+
    | size <= pool->max|
    +--------+---------+
             |
        +----+----+
        |         |
       YES        NO
        |         |
        v         v
    +--------+ +--------+
    |  SMALL | | LARGE  |
    +--------+ +--------+
        |         |
        v         v
    ngx_palloc   ngx_palloc
    _small()     _large()


ngx_palloc_small(pool, size, align) - Small Allocation
======================================================

    +------------------+
    | p = pool->current|  <-- Start from current block
    +--------+---------+
             |
             v
    +------------------+
    | m = p->d.last    |  <-- Get free pointer
    +--------+---------+
             |
             v
    +------------------+
    | if (align)       |
    |   m = align(m)   |  <-- Align to NGX_ALIGNMENT
    +--------+---------+
             |
             v
    +------------------+
    | p->d.end - m     |
    |   >= size?       |
    +--------+---------+
             |
        +----+----+
        |         |
       YES        NO
        |         |
        v         v
    +--------+ +--------+
    |p->d.last| |p=p->d. |
    | = m+size| |  next  |
    | return m| +---+----+
    +--------+     |
                   v
               +--------+
               | p==NULL|
               +---+----+
                   |
              +----+----+
              |         |
             YES        NO
              |         |
              v         v
         +--------+ +--------+
         |palloc_ | | retry  |
         | block()| | loop   |
         +--------+ +--------+


ngx_palloc_block(pool, size) - Block Growth
===========================================

    +------------------+
    |psize = pool->d.end|  <-- Same size as original pool
    |  - (u_char*)pool |
    +--------+---------+
             |
             v
    +------------------+
    | ngx_memalign()   |  <-- Allocate new block
    +--------+---------+
             |
             v
    +------------------+
    | new->d.end =     |
    |   m + psize      |
    | new->d.next=NULL |
    | new->d.failed=0  |
    +--------+---------+
             |
             v
    +------------------+
    | m += sizeof(     |  <-- Skip smaller header
    |  ngx_pool_data_t)|
    | m = align(m)     |
    | new->d.last =    |
    |   m + size       |
    +--------+---------+
             |
             v
    +------------------+
    | Update current   |  <-- If p->d.failed++ > 4
    | if block failed  |      then skip this block
    | too many times   |
    +--------+---------+
             |
             v
    +------------------+
    | Link new block   |
    | p->d.next = new  |
    +--------+---------+
             |
             v
         return m


ngx_palloc_large(pool, size) - Large Allocation
===============================================

    +------------------+
    | ngx_alloc(size)  |  <-- Regular malloc
    +--------+---------+
             |
             v
    +------------------+
    | Search for empty |  <-- Look for reusable large slot
    | large slot       |      (large->alloc == NULL)
    | (max 3 attempts) |
    +--------+---------+
             |
        +----+----+
        |         |
      FOUND    NOT FOUND
        |         |
        v         v
    +--------+ +--------+
    |large-> | |palloc_ |
    |alloc=p | |small() |  <-- Allocate large_t from pool
    |return p| +---+----+
    +--------+     |
                   v
               +--------+
               |large-> |
               |alloc=p |
               |large-> |
               | next=  |
               |pool->  |
               | large  |
               |pool->  |
               | large= |
               | large  |
               +--------+
                   |
                   v
               return p


ngx_destroy_pool(pool) - Teardown
=================================

    +------------------+
    | for c in cleanup |  <-- Run all cleanup handlers
    |   c->handler(    |
    |     c->data)     |
    +--------+---------+
             |
             v
    +------------------+
    | for l in large   |  <-- Free all large allocations
    |   ngx_free(      |
    |     l->alloc)    |
    +--------+---------+
             |
             v
    +------------------+
    | for p in blocks  |  <-- Free all pool blocks
    |   ngx_free(p)    |
    +--------+---------+
             |
             v
           done
```

**说明（中文）：**

此图详细展示了 nginx 内存池的控制流程：

**ngx_create_pool**：分配对齐内存 → 初始化 last/end/next/failed → 计算 max → 设置 current/large/cleanup 为 NULL

**ngx_palloc**：判断 size ≤ max 走小分配路径，否则走大分配路径

**ngx_palloc_small**：从 current 块开始 → 对齐指针 → 检查剩余空间 → 足够则递增 last 并返回；不够则尝试下一块或调用 ngx_palloc_block

**ngx_palloc_block**：分配相同大小的新块 → 使用较小的头（ngx_pool_data_t）→ 更新 current（如果失败次数 > 4）→ 链接新块

**ngx_palloc_large**：调用 malloc → 尝试找空闲 large 槽位（最多 3 次）→ 未找到则从池分配 large_t 结构体 → 链入 large 链表头部

**ngx_destroy_pool**：运行所有 cleanup 处理器 → 释放所有 large 分配 → 释放所有池块

---

## 4. Performance Considerations

```
Cache Alignment and Padding
===========================

    NGX_ALIGNMENT = sizeof(uintptr_t) = 8 bytes (64-bit)
    NGX_POOL_ALIGNMENT = 16 bytes

    Pool Block Alignment:
    +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
    | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 |10 |11 |12 |13 |14 |15 |
    +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
    ^                                                               ^
    |                                                               |
    Pool start (16-byte aligned)                                    |
                                                                    |
    Benefit: SSE/AVX operations work correctly

    Object Alignment (ngx_align_ptr):
    
    Before alignment: m = 0x7fff8001  (not 8-byte aligned)
    
    ngx_align_ptr(m, 8):
      ((uintptr_t)(m) + (8 - 1)) & ~(8 - 1)
      = (0x7fff8001 + 7) & ~7
      = 0x7fff8008 & 0xfffffff8
      = 0x7fff8008  (8-byte aligned)
    
    Padding bytes wasted: 7 bytes
    Trade-off: Speed vs memory efficiency


False Sharing Risk
==================

    Cache Line (64 bytes):
    +---------------------------------------------------------------+
    |  Thread 1 data  |  Thread 2 data  |  Thread 3 data  | ...     |
    +---------------------------------------------------------------+
    
    FALSE SHARING: Multiple threads modify same cache line
    
    nginx avoids this by:
      1. Per-request pools (no sharing)
      2. Worker process model (not threads)
      3. No shared pool state modification during request
    
    If you share pools across threads:
    
    Thread 1                          Thread 2
        |                                 |
        v                                 v
    +--------+                        +--------+
    | alloc  | <-- modifies d.last    | alloc  | <-- modifies d.last
    +--------+                        +--------+
        |                                 |
        +----> SAME cache line <----------+
                     |
                     v
              Cache invalidation ping-pong
              (performance disaster)


Allocation Path Performance
===========================

    Small Allocation (fast path):
    
    ngx_palloc_small():
      1. m = p->d.last           // 1 memory read
      2. m = align(m)            // arithmetic only
      3. check space             // 1 memory read (p->d.end)
      4. p->d.last = m + size    // 1 memory write
      5. return m                // done
    
    Total: O(1), ~3 memory operations, no locks, no syscalls
    
    vs malloc():
      1. Acquire lock (or atomic ops)
      2. Search free list (O(log n) or O(n))
      3. Split block if needed
      4. Update metadata
      5. Release lock
      6. return
    
    Total: O(log n), many more operations, possible contention

    Large Allocation:
      1. ngx_alloc() -> malloc()  // standard allocator
      2. Search for empty slot    // max 3 iterations
      3. Allocate tracking struct // from pool (fast)
      4. Link to large list       // O(1)
    
    Total: O(1) + malloc overhead


Fragmentation Inside Pool
=========================

    Internal Fragmentation (alignment padding):
    
    Request: 5 bytes
    After alignment: 8 bytes
    Wasted: 3 bytes (37.5%)
    
    +---+---+---+---+---+---+---+---+
    |<-- 5 bytes used -->|<-waste->|
    +---+---+---+---+---+---+---+---+
    
    nginx trade-off:
      - Accept internal fragmentation for speed
      - Pool destroyed = all fragmentation recovered
      - Short-lived pools minimize waste duration

    External Fragmentation (block switching):
    
    Block 1:                   Block 2:
    +-------------------+      +-------------------+
    |USED|USED| 3 bytes |      |USED|USED|        |
    +-------------------+      +-------------------+
                ^
                |
          Cannot fit 5-byte request
          even though 3 bytes free
    
    nginx mitigation:
      - 'current' pointer skips nearly-full blocks
      - New block allocated when needed
      - No coalescing needed (bulk free)


Summary: nginx Pool Performance Characteristics
===============================================

+------------------------+------------------+---------------------+
| Metric                 | nginx Pool       | General malloc      |
+------------------------+------------------+---------------------+
| Allocation time        | O(1) constant    | O(log n) typical    |
| Deallocation time      | O(1) (bulk)      | O(log n) per object |
| Lock contention        | None (per-req)   | Global or per-arena |
| Fragmentation          | Internal only    | Both types          |
| Fragmentation lifetime | Pool lifetime    | Process lifetime    |
| Cache behavior         | Excellent        | Variable            |
| Memory overhead        | ~64 bytes/block  | ~16-32 bytes/alloc  |
+------------------------+------------------+---------------------+
```

**说明（中文）：**

**缓存对齐和填充**：
- NGX_ALIGNMENT = 8 字节，NGX_POOL_ALIGNMENT = 16 字节
- ngx_align_ptr 将指针向上对齐到指定边界
- 填充字节被浪费，但换取了访问速度

**伪共享风险**：
- 多线程修改同一缓存行导致缓存失效乒乓
- nginx 通过每请求池和进程模型避免此问题

**分配路径性能**：
- 小分配：O(1)，约 3 次内存操作，无锁，无系统调用
- 对比 malloc：O(log n)，更多操作，可能存在竞争

**池内碎片化**：
- 内部碎片（对齐填充）：请求 5 字节，对齐后 8 字节，浪费 3 字节
- 外部碎片（块切换）：current 指针跳过几乎满的块
- nginx 权衡：接受内部碎片以换取速度；池销毁时所有碎片恢复

**性能特性总结**：分配 O(1)、批量释放 O(1)、无锁竞争、仅内部碎片、优秀的缓存行为。
