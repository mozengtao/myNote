# HOW | Core Design Model of Memory Pools

## 1. Conceptual Model of a Memory Pool

```
Memory Pool Conceptual Model
============================

                     POOL ABSTRACTION
    +--------------------------------------------------+
    |                                                  |
    |   +-------------------------------------------+  |
    |   |            Pre-allocated Region           |  |
    |   |  +------+------+------+------+------+     |  |
    |   |  | Obj1 | Obj2 | Obj3 | .... | ObjN |     |  |
    |   |  +------+------+------+------+------+     |  |
    |   |            ^                   ^          |  |
    |   |            |                   |          |  |
    |   |         used                 free         |  |
    |   +-------------------------------------------+  |
    |                                                  |
    |   Metadata:                                      |
    |   - capacity: total bytes available              |
    |   - used: bytes allocated                        |
    |   - alignment: object boundary requirement       |
    |   - lifetime: when this pool will be destroyed   |
    |                                                  |
    +--------------------------------------------------+

Key Properties:
  1. Pre-allocated: Memory obtained upfront, not per-object
  2. Contiguous: Objects laid out sequentially
  3. Scoped Lifetime: All objects share pool's lifetime
  4. Fast Allocation: Bump pointer, no free-list search
```

**说明（中文）：**

此图展示了内存池的核心概念模型：

1. **预分配区域**：内存在池创建时一次性获取，而非每个对象分别分配
2. **连续布局**：对象在内存中顺序排列
3. **共享生命周期**：所有对象与池共享相同的生命周期
4. **快速分配**：使用指针递增（bump pointer）方式，无需搜索空闲列表

元数据包括：容量、已用量、对齐要求、生命周期信息。

---

## 2. Allocation Contract: Pool vs malloc/free

```
malloc/free Contract
====================

    Caller                          Allocator
      |                                 |
      |  malloc(size) ----------------> |
      |                                 | search free list
      |                                 | split block
      |                                 | update metadata
      | <--------------- ptr            |
      |                                 |
      |  [use memory for ANY duration]  |
      |                                 |
      |  free(ptr) -------------------> |
      |                                 | validate ptr
      |                                 | coalesce blocks
      |                                 | update free list
      |                                 |

    Guarantees:
      + Memory valid until explicit free()
      + Can be freed in any order
      + No relationship between allocations

    Costs:
      - Search overhead
      - Fragmentation
      - Coalescing overhead


Memory Pool Contract
====================

    Caller                          Pool
      |                                |
      |  ngx_palloc(pool, size) -----> |
      |                                | align pointer
      |                                | bump last pointer
      | <-------------- ptr            | (no search!)
      |                                |
      |  [use memory until POOL dies]  |
      |                                |
      |  ngx_destroy_pool(pool) -----> |
      |                                | run cleanup handlers
      |                                | free all blocks
      |                                |

    Guarantees:
      + Memory valid until pool destroyed
      + All allocations freed together
      + Cleanup handlers called in order

    Constraints:
      - Cannot free individual small objects
      - All objects share pool lifetime
      - Caller must know lifetime at allocation time

Contract Comparison:
+-------------------+----------------+------------------+
| Aspect            | malloc/free    | Memory Pool      |
+-------------------+----------------+------------------+
| Allocation        | O(n) search    | O(1) bump        |
| Deallocation      | Per-object     | Bulk only        |
| Lifetime          | Arbitrary      | Pool-scoped      |
| Fragmentation     | Accumulates    | Reset on destroy |
| Ownership         | Caller tracks  | Pool owns all    |
+-------------------+----------------+------------------+
```

**说明（中文）：**

此对比图展示了两种分配契约的差异：

**malloc/free 契约**：
- 分配时搜索空闲列表、分割块、更新元数据
- 内存在显式 free() 前一直有效
- 可以任意顺序释放
- 代价：搜索开销、碎片化、合并开销

**内存池契约**：
- 分配仅需对齐指针、递增指针（无搜索）
- 内存在池销毁前一直有效
- 所有分配一起释放
- 约束：无法单独释放小对象、所有对象共享生命周期

核心差异：malloc/free 是 O(n) 搜索 + 逐对象释放；内存池是 O(1) 递增 + 批量释放。

---

## 3. Invariants That Must Always Hold

```
Memory Pool Invariants
======================

INVARIANT 1: Alignment
----------------------

    Pool Block Memory Layout:
    
    +--------+--------+--------+--------+--------+--------+
    |  Meta  | Align  |  Obj1  | Align  |  Obj2  | Align  |
    +--------+--------+--------+--------+--------+--------+
    ^        ^        ^        ^        ^
    |        |        |        |        |
    0      16       24       32       40    (example offsets)
    
    MUST HOLD:
      - (uintptr_t)obj % NGX_ALIGNMENT == 0
      - Objects start at aligned boundaries
      - Misaligned access = SIGBUS on some architectures

    nginx implementation:
      m = ngx_align_ptr(m, NGX_ALIGNMENT);  // line 160


INVARIANT 2: Ownership
----------------------

    +------------------+
    |      Pool        |
    +--------+---------+
             |
             | OWNS (exclusively)
             v
    +--------+--------+--------+
    |  Obj1  |  Obj2  |  Obj3  |
    +--------+--------+--------+
    
    MUST HOLD:
      - Pool owns ALL memory allocated from it
      - Objects do NOT own their memory
      - Only pool can free the memory
      - No object can outlive its pool

    Violation Example:
      obj = ngx_palloc(pool, sizeof(*obj));
      ngx_destroy_pool(pool);
      obj->field = 1;  // USE-AFTER-FREE!


INVARIANT 3: Lifetime Hierarchy
-------------------------------

    Time --->
    
    Pool Lifetime:    |<============ POOL ALIVE ============>|
                      |                                       |
    Object Lifetime:  |  |<-- Obj1 -->|                       |
                      |       |<---- Obj2 ---->|              |
                      |            |<-------- Obj3 -------->| |
                      |                                       |
                    create                                 destroy
    
    MUST HOLD:
      - Object lifetime ⊆ Pool lifetime (subset)
      - No object can be used after pool destroy
      - Pool create MUST happen before any allocation
      - Pool destroy MUST happen after last object use


INVARIANT 4: Pointer Validity
-----------------------------

    Pool State Machine:
    
        +----------+    create    +----------+
        |  UNINIT  | -----------> |  ACTIVE  |
        +----------+              +----+-----+
                                       |
                                       | destroy
                                       v
                                  +----------+
                                  |  FREED   |
                                  +----------+
    
    MUST HOLD:
      - Allocations ONLY in ACTIVE state
      - All pointers become INVALID after FREED state
      - No transition back to ACTIVE after FREED
```

**说明（中文）：**

此图展示了内存池必须始终保持的四个不变式：

**不变式1 - 对齐**：对象必须在对齐边界上开始。`(uintptr_t)obj % NGX_ALIGNMENT == 0`。在某些架构上，未对齐访问会导致 SIGBUS 信号。

**不变式2 - 所有权**：池独占拥有从它分配的所有内存。对象不拥有自己的内存。只有池可以释放内存。任何对象都不能比池活得更久。

**不变式3 - 生命周期层次**：对象生命周期必须是池生命周期的子集。池创建必须在任何分配之前，池销毁必须在最后一次对象使用之后。

**不变式4 - 指针有效性**：池状态机只有三个状态：UNINIT → ACTIVE → FREED。分配只能在 ACTIVE 状态进行，FREED 后所有指针都无效。

---

## 4. Lifetime Definition and Enforcement

```
Pool Lifetime vs Object Lifetime
================================

POOL LIFETIME (Explicit):
+----------------------------------------------------------+
|                                                          |
|  ngx_create_pool()     ...operations...   ngx_destroy_pool()
|        |                                        |        |
|        +<------ POOL LIFETIME SCOPE ----------->+        |
|                                                          |
+----------------------------------------------------------+

OBJECT LIFETIME (Implicit - Inherited from Pool):
+----------------------------------------------------------+
|                                                          |
|  ngx_palloc()     ...use object...    (no explicit free) |
|        |                    |               |            |
|        +<-- OBJECT VALID -->+               |            |
|        +<------------- OBJECT MEMORY VALID ---------->+  |
|                                                       ^  |
|                                      Becomes invalid here|
+----------------------------------------------------------+


nginx Per-Request Pool Lifetime:
================================

    Client            nginx                    Pool
      |                 |                        |
      |  HTTP Request   |                        |
      | --------------> |                        |
      |                 |  ngx_create_pool()     |
      |                 | ---------------------> |  POOL BORN
      |                 |                        |
      |                 |  ngx_palloc(headers)   |
      |                 | ---------------------> |  alloc
      |                 |  ngx_palloc(body)      |
      |                 | ---------------------> |  alloc
      |                 |  ngx_palloc(response)  |
      |                 | ---------------------> |  alloc
      |                 |                        |
      |  HTTP Response  |                        |
      | <-------------- |                        |
      |                 |                        |
      |                 |  ngx_destroy_pool()    |
      |                 | ---------------------> |  POOL DIES
      |                 |                        |  (all objects freed)
      |                 |                        |


Enforcement Mechanisms:
=======================

    1. API Design (Compile-time):
       - ngx_palloc() requires pool pointer
       - No standalone free() for small objects
       - Cleanup handlers for resources

    2. Scope Discipline (Programmer):
       - Pool created at scope entry
       - Pool destroyed at scope exit
       - Never store pool pointers globally

    3. Cleanup Handlers (Runtime):
       +--------------------------------------------------+
       |  ngx_pool_cleanup_add(pool, size)                |
       |    - Register handler to run at pool destroy     |
       |    - For closing files, sockets, etc.            |
       |    - Handles resources that outlive objects      |
       +--------------------------------------------------+

       Example:
         ngx_pool_cleanup_t *c = ngx_pool_cleanup_add(pool, sizeof(fd));
         c->handler = close_file_handler;
         c->data = fd_info;
         // When pool destroyed: close_file_handler(fd_info) called
```

**说明（中文）：**

此图展示了生命周期的定义和强制机制：

**池生命周期（显式）**：从 `ngx_create_pool()` 到 `ngx_destroy_pool()` 之间的时间段。

**对象生命周期（隐式）**：从 `ngx_palloc()` 到池销毁，对象没有显式释放。对象继承池的生命周期。

**nginx 请求池示例**：
1. 收到 HTTP 请求时创建池
2. 为 headers、body、response 等分配内存
3. 发送响应后销毁池
4. 所有对象随池一起释放

**强制机制**：
1. API 设计（编译时）：`ngx_palloc()` 必须传入池指针
2. 作用域纪律（程序员）：池在作用域入口创建、出口销毁
3. 清理处理器（运行时）：注册在池销毁时运行的回调，用于关闭文件、套接字等

---

## 5. Pool Interaction with System Concerns

```
Concurrency: Thread Safety
==========================

    APPROACH 1: Per-Thread Pools (Recommended)
    +-----------------------------------------+
    |  Thread 1        Thread 2        Thread 3
    |     |               |               |
    |     v               v               v
    |  +------+        +------+        +------+
    |  |Pool 1|        |Pool 2|        |Pool 3|
    |  +------+        +------+        +------+
    |     |               |               |
    |  No locks needed - complete isolation
    +-----------------------------------------+

    APPROACH 2: Per-Request Pools (nginx model)
    +-----------------------------------------+
    |  Request 1       Request 2       Request 3
    |     |               |               |
    |     v               v               v
    |  +------+        +------+        +------+
    |  |Pool 1|        |Pool 2|        |Pool 3|
    |  +------+        +------+        +------+
    |     |               |               |
    |  Requests processed by single thread
    |  Pool never shared across threads
    +-----------------------------------------+

    APPROACH 3: Shared Pool with Locking (Avoid if possible)
    +-----------------------------------------+
    |  Thread 1        Thread 2        Thread 3
    |     |               |               |
    |     +-------+-------+-------+-------+
    |             |
    |             v
    |          +------+
    |          | Lock |
    |          +------+
    |             |
    |             v
    |        +--------+
    |        | Pool   |
    |        +--------+
    |
    |  Requires: mutex_lock before any pool operation
    |  Problem: Serializes allocations
    +-----------------------------------------+


Cache Locality
==============

    Pool Memory Layout:
    
    +----------------------------------------------------------+
    |                    Cache Line (64 bytes)                 |
    +----------------------------------------------------------+
    |  Obj1  |  Obj2  |  Obj3  |  Obj4  |  Obj5  |  Obj6  |... |
    +----------------------------------------------------------+
    ^                                                          ^
    |                                                          |
    One cache line fetch brings multiple objects
    
    BENEFIT: Sequential allocation = sequential access pattern
    
    Compare to malloc:
    
    +------+     +------+     +------+     +------+
    | Obj1 |     | Obj2 |     | Obj3 |     | Obj4 |
    +------+     +------+     +------+     +------+
       ^            ^            ^            ^
       |            |            |            |
    Different cache lines, possible cache misses
    
    Pool Advantage:
      - Objects allocated together are stored together
      - Sequential processing benefits from prefetching
      - Fewer cache misses for related objects


NUMA Considerations
===================

    NUMA Node 0                    NUMA Node 1
    +-------------------+          +-------------------+
    |  CPU 0    CPU 1   |          |  CPU 2    CPU 3   |
    |     |       |     |          |     |       |     |
    |     v       v     |          |     v       v     |
    |  +------------+   |          |  +------------+   |
    |  | Memory 0   |   |          |  | Memory 1   |   |
    |  +------------+   |          |  +------------+   |
    +-------------------+          +-------------------+
    
    NUMA-Aware Pool Strategy:
    
    1. Allocate pool from LOCAL memory node
       pool = numa_alloc_local(pool_size);
       
    2. Thread affinity matches memory location
       - CPU 0 uses pools in Memory 0
       - CPU 2 uses pools in Memory 1
    
    3. Avoid cross-node pool sharing
       - Each thread/request gets node-local pool
    
    nginx doesn't explicitly handle NUMA, but:
      - Per-request pools naturally achieve locality
      - Worker process affinity helps
```

**说明（中文）：**

此图展示了内存池与系统关注点的交互：

**并发性**：
- 方法1（推荐）：每线程一个池，完全隔离，无需锁
- 方法2（nginx 模型）：每请求一个池，请求由单线程处理
- 方法3（尽量避免）：共享池加锁，会导致分配串行化

**缓存局部性**：
- 池内存布局连续，一次缓存行获取可带来多个对象
- 顺序分配 = 顺序访问模式，有利于预取
- 对比 malloc：对象可能分散在不同缓存行，导致缓存未命中

**NUMA 考虑**：
- 从本地内存节点分配池
- 线程亲和性与内存位置匹配
- 避免跨节点池共享
- nginx 的每请求池自然实现了局部性
