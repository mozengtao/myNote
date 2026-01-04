# PITFALLS | Common Memory Pool Mistakes

## 1. Treating Pools as "Faster malloc"

```
THE MISTAKE
===========

    Developer thinking:
    
    "Pools are just faster malloc, I can use them the same way!"
    
                     WRONG
                       |
                       v
    +-----------------------------------------------------+
    | ngx_pool_t *global_pool;                            |
    |                                                     |
    | void init() {                                       |
    |     global_pool = ngx_create_pool(4096, log);       |
    | }                                                   |
    |                                                     |
    | void *my_alloc(size_t size) {                       |
    |     return ngx_palloc(global_pool, size);           |
    | }                                                   |
    |                                                     |
    | void my_free(void *p) {                             |
    |     ngx_pfree(global_pool, p);  // WRONG!           |
    |     // ngx_pfree only works for large allocations   |
    |     // small allocations are NOT freed              |
    | }                                                   |
    +-----------------------------------------------------+
    
    What happens:
    - Small allocations accumulate
    - Pool blocks grow indefinitely
    - Memory "leak" within pool
    - Eventually: out of memory


THE FIX
=======

    Redesign ownership around pool lifetime:
    
    +-----------------------------------------------------+
    | void handle_request(ngx_http_request_t *r) {        |
    |     // Pool created with request                    |
    |     // All allocations tied to request lifetime     |
    |                                                     |
    |     obj1 = ngx_palloc(r->pool, size);               |
    |     obj2 = ngx_palloc(r->pool, size);               |
    |     obj3 = ngx_palloc(r->pool, size);               |
    |                                                     |
    |     // No free calls needed                         |
    |     // All freed when request completes             |
    | }                                                   |
    +-----------------------------------------------------+

    Key Insight:
    
    malloc/free model:     Pool model:
    
    Object1 -> lifetime1   Object1 -+
    Object2 -> lifetime2   Object2 -+-> Pool lifetime
    Object3 -> lifetime3   Object3 -+
                           
    (independent)          (shared)
    
    If your objects have DIFFERENT lifetimes, pools may be wrong choice.
```

**说明（中文）：**

**错误**：将池当作更快的 malloc 使用，创建全局池，模拟 malloc/free 接口。

**问题**：
- ngx_pfree 只对大分配有效，小分配不会被释放
- 小分配在池内累积
- 池块无限增长
- 最终内存耗尽

**修复**：围绕池生命周期重新设计所有权。所有对象应该有相同的生命周期，与池生命周期匹配。如果对象有不同的生命周期，池可能不是正确的选择。

---

## 2. Mixing Pooled and Non-pooled Allocations

```
THE MISTAKE
===========

    +-----------------------------------------------------+
    | void process(ngx_pool_t *pool) {                    |
    |     // Pooled allocation                            |
    |     header = ngx_palloc(pool, sizeof(*header));     |
    |                                                     |
    |     // Non-pooled allocation (regular malloc)       |
    |     body = malloc(body_size);                       |
    |                                                     |
    |     // ... processing ...                           |
    |                                                     |
    |     // Cleanup - which to free?                     |
    |     free(body);     // Correct                      |
    |     // header?      // Do NOT free(header)!         |
    | }                                                   |
    +-----------------------------------------------------+
    
    Problems:
    
    1. Must remember which allocator was used
    2. Easy to call wrong free function
    3. Debugging nightmare: which pointer came from where?
    
    Confusion Matrix:
    
                     | free()    | ngx_pfree | pool destroy
    -----------------+-----------+-----------+--------------
    malloc'd ptr     | CORRECT   | UNDEFINED | LEAK
    ngx_palloc'd ptr | CRASH     | maybe OK* | CORRECT
    
    * ngx_pfree only works for large allocations


THE FIX
=======

    Rule: One allocation strategy per object graph
    
    Option A: All pooled
    +-----------------------------------------------------+
    | void process(ngx_pool_t *pool) {                    |
    |     header = ngx_palloc(pool, sizeof(*header));     |
    |     body = ngx_palloc(pool, body_size);             |
    |                                                     |
    |     // All freed by pool destroy                    |
    | }                                                   |
    +-----------------------------------------------------+
    
    Option B: All malloc (when pooling doesn't fit)
    +-----------------------------------------------------+
    | void process() {                                    |
    |     header = malloc(sizeof(*header));               |
    |     body = malloc(body_size);                       |
    |                                                     |
    |     // ... use ...                                  |
    |                                                     |
    |     free(header);                                   |
    |     free(body);                                     |
    | }                                                   |
    +-----------------------------------------------------+
    
    Option C: Clearly separated subsystems
    +-----------------------------------------------------+
    | void process(ngx_pool_t *pool) {                    |
    |     // Request data: pooled                         |
    |     request = ngx_palloc(pool, sizeof(*request));   |
    |                                                     |
    |     // Library that uses malloc internally          |
    |     lib_result = external_library_call();           |
    |     // Document: lib_result must be freed with      |
    |     //           library_free(lib_result)           |
    |                                                     |
    |     // NEVER mix: don't store lib_result in pool    |
    |     //            don't pass pool to library        |
    | }                                                   |
    +-----------------------------------------------------+
```

**说明（中文）：**

**错误**：在同一代码中混合使用池分配和普通 malloc。

**问题**：
- 必须记住每个指针使用了哪个分配器
- 容易调用错误的释放函数
- 调试噩梦：哪个指针来自哪里？

**混淆矩阵**：
- malloc 的指针用 free() 正确，用 ngx_pfree 未定义，池销毁时泄漏
- ngx_palloc 的指针用 free() 崩溃，用 ngx_pfree 可能 OK，池销毁时正确

**修复**：每个对象图使用一种分配策略。要么全部池分配，要么全部 malloc，要么清晰分离子系统并记录每个部分的释放方式。

---

## 3. Returning Pooled Memory to Wrong Pool

```
THE MISTAKE
===========

    Scenario: Two pools with different lifetimes
    
    Pool A (connection)          Pool B (request)
    +-------------------+        +-------------------+
    | Lives: long time  |        | Lives: short time |
    +-------------------+        +-------------------+
    
    +-----------------------------------------------------+
    | char *global_buffer;                                |
    |                                                     |
    | void connection_init(ngx_pool_t *conn_pool) {       |
    |     // This allocation lives with connection        |
    |     global_buffer = ngx_palloc(conn_pool, 1024);    |
    | }                                                   |
    |                                                     |
    | void handle_request(ngx_pool_t *req_pool) {         |
    |     // BUG: Overwriting connection-scoped pointer   |
    |     //      with request-scoped memory              |
    |     global_buffer = ngx_palloc(req_pool, 2048);     |
    | }                                                   |
    |                                                     |
    | void after_request() {                              |
    |     // BUG: global_buffer points to FREED memory    |
    |     //      (request pool was destroyed)            |
    |     use(global_buffer);  // USE-AFTER-FREE!         |
    | }                                                   |
    +-----------------------------------------------------+
    
    Timeline:
    
    Time ---->
    
    conn_pool: |<============== ALIVE ==================>|
    req_pool:  |      |<-- ALIVE -->|                    |
                      ^              ^
                      |              |
                   request        request
                   start          end
                   (alloc to      (pool
                    global)       destroyed)
                                      |
                                      v
                              global_buffer INVALID


THE FIX
=======

    Rule: Pointer storage location determines pool choice
    
    +-----------------------------------------------------+
    | Stored In              | Allocate From              |
    +------------------------+----------------------------+
    | Connection struct      | Connection pool            |
    | Request struct         | Request pool               |
    | Global variable        | Global/permanent pool      |
    | Stack variable         | Any pool (careful!)        |
    +-----------------------------------------------------+
    
    Correct Pattern:
    
    +-----------------------------------------------------+
    | typedef struct {                                    |
    |     ngx_pool_t *pool;      // My pool               |
    |     char *my_buffer;       // Allocated from my pool|
    | } my_context_t;                                     |
    |                                                     |
    | void init(my_context_t *ctx, ngx_pool_t *pool) {    |
    |     ctx->pool = pool;                               |
    |     ctx->my_buffer = ngx_palloc(pool, 1024);        |
    |     // Buffer stored in ctx, allocated from ctx's   |
    |     // pool - lifetimes match!                      |
    | }                                                   |
    +-----------------------------------------------------+
```

**说明（中文）：**

**错误**：将从短生命周期池分配的内存指针存储到长生命周期的位置。

**场景**：连接池（长生命周期）和请求池（短生命周期）。全局变量指向请求池分配的内存，请求结束后池销毁，全局变量变成悬空指针。

**修复规则**：指针存储位置决定池选择。
- 存储在连接结构体 → 从连接池分配
- 存储在请求结构体 → 从请求池分配
- 存储在全局变量 → 从全局/永久池分配

正确模式：每个上下文结构体包含自己的池指针，从该池分配的内存存储在该结构体中。

---

## 4. Using Pools with Mismatched Lifetimes

```
THE MISTAKE
===========

    Pattern: Object needs to outlive its "natural" pool
    
    +-----------------------------------------------------+
    | void cache_result(ngx_pool_t *request_pool) {       |
    |     // Result should live across requests           |
    |     cached_data = ngx_palloc(request_pool, size);   |
    |                                                     |
    |     // Store in cache (lives longer than request)   |
    |     global_cache[key] = cached_data;                |
    |                                                     |
    |     // After request ends: cached_data is INVALID   |
    | }                                                   |
    +-----------------------------------------------------+
    
    The Lifetime Mismatch:
    
    cache lifetime:    |<=============== forever ===============>|
    request pool:      |<- R1 ->|  |<- R2 ->|  |<- R3 ->|
                              ^           ^           ^
                              |           |           |
                         cached_data   DANGLING    DANGLING
                         valid         POINTER     POINTER


THE FIX
=======

    Option A: Allocate from appropriate pool
    
    +-----------------------------------------------------+
    | // Cache has its own pool                           |
    | ngx_pool_t *cache_pool;                             |
    |                                                     |
    | void init_cache() {                                 |
    |     cache_pool = ngx_create_pool(65536, log);       |
    | }                                                   |
    |                                                     |
    | void cache_result(ngx_pool_t *request_pool) {       |
    |     // Copy to cache pool                           |
    |     cached_data = ngx_palloc(cache_pool, size);     |
    |     memcpy(cached_data, request_data, size);        |
    |                                                     |
    |     global_cache[key] = cached_data;                |
    | }                                                   |
    +-----------------------------------------------------+
    
    Option B: Use malloc for long-lived objects
    
    +-----------------------------------------------------+
    | void cache_result(ngx_pool_t *request_pool) {       |
    |     // Cache entries use malloc, not pool           |
    |     cached_data = malloc(size);                     |
    |     memcpy(cached_data, request_data, size);        |
    |                                                     |
    |     global_cache[key] = cached_data;                |
    | }                                                   |
    |                                                     |
    | void evict_cache_entry(key) {                       |
    |     free(global_cache[key]);                        |
    | }                                                   |
    +-----------------------------------------------------+
    
    Option C: ngx_reset_pool for reusable short-lived pools
    
    +-----------------------------------------------------+
    | // Worker pool, reused across requests              |
    | ngx_pool_t *worker_pool;                            |
    |                                                     |
    | void handle_request() {                             |
    |     // Use worker pool for request                  |
    |     data = ngx_palloc(worker_pool, size);           |
    |                                                     |
    |     // ... process ...                              |
    |                                                     |
    |     // Reset for next request (doesn't free blocks) |
    |     ngx_reset_pool(worker_pool);                    |
    | }                                                   |
    |                                                     |
    | // Note: ngx_reset_pool frees large allocations     |
    | //       and resets small allocation pointer        |
    | //       Pool blocks are kept for reuse             |
    +-----------------------------------------------------+
```

**说明（中文）：**

**错误**：对象需要比其"自然"池活得更久。例如，从请求池分配缓存数据，但缓存需要跨请求存活。

**生命周期不匹配**：缓存需要永久存在，但请求池在每个请求后销毁，导致缓存中的指针变成悬空指针。

**修复方案**：
- 方案 A：从适当的池分配（创建专用缓存池）
- 方案 B：对长生命周期对象使用 malloc
- 方案 C：使用 ngx_reset_pool 重用短期池

ngx_reset_pool 释放大分配，重置小分配指针，但保留池块供重用。

---

## 5. Over-engineering: Pools Where malloc Suffices

```
THE ANTI-PATTERN
================

    Situation: Simple utility program
    
    +-----------------------------------------------------+
    | // One-time config parsing                          |
    | ngx_pool_t *config_pool;                            |
    |                                                     |
    | void parse_config() {                               |
    |     config_pool = ngx_create_pool(4096, log);       |
    |     // ... parse config ...                         |
    | }                                                   |
    |                                                     |
    | // One-time data processing                         |
    | ngx_pool_t *process_pool;                           |
    |                                                     |
    | void process_data() {                               |
    |     process_pool = ngx_create_pool(4096, log);      |
    |     // ... process ...                              |
    | }                                                   |
    |                                                     |
    | // OVER-ENGINEERED: Pools for non-repetitive ops    |
    +-----------------------------------------------------+
    
    When malloc is BETTER:
    
    +-----------------------------------------------+
    | Scenario                   | Use              |
    +----------------------------+------------------+
    | One-time initialization    | malloc           |
    | Single object lifetime     | malloc           |
    | Objects with varied life   | malloc           |
    | Batch job, then exit       | malloc (or leak) |
    +----------------------------+------------------+
    | Request/response pattern   | pool             |
    | Many same-lifetime objects | pool             |
    | Latency-sensitive path     | pool             |
    | Long-running server        | pool             |
    +-----------------------------------------------+


THE FIX: Decision Framework
===========================

    Question 1: Is this a hot path?
                   |
            +------+------+
            |             |
           NO            YES
            |             |
            v             v
       Consider       Question 2
       malloc
    
    Question 2: Do objects share lifetime?
                   |
            +------+------+
            |             |
           NO            YES
            |             |
            v             v
        malloc        Question 3
    
    Question 3: Is the lifetime clear and scoped?
                   |
            +------+------+
            |             |
           NO            YES
            |             |
            v             v
        malloc         USE POOL
    
    
    Red Flags for Pool Usage:
    
    [ ] "I need to free individual objects frequently"
    [ ] "Object lifetimes are unpredictable"
    [ ] "This runs once at startup"
    [ ] "This is a simple utility"
    [ ] "I'm adding pools to fix a leak" (pools don't fix leaks!)
```

**说明（中文）：**

**反模式**：为非重复性操作使用池。例如，一次性配置解析、一次性数据处理都用池，这是过度设计。

**何时 malloc 更好**：
- 一次性初始化
- 单对象生命周期
- 对象生命周期各异
- 批处理作业后退出

**何时使用池**：
- 请求/响应模式
- 多个相同生命周期对象
- 延迟敏感路径
- 长时间运行服务器

**决策框架**：
1. 是热路径吗？否 → 考虑 malloc
2. 对象共享生命周期吗？否 → malloc
3. 生命周期清晰且有作用域吗？否 → malloc；是 → 使用池

**池使用的危险信号**：需要频繁释放单个对象、对象生命周期不可预测、只在启动时运行一次、添加池来修复泄漏。

---

## 6. Silent Memory Corruption

```
Double Free Inside Pool
=======================

    +-----------------------------------------------------+
    | void process(ngx_pool_t *pool) {                    |
    |     large_buf = ngx_palloc(pool, 8192);             |
    |                                                     |
    |     // ... some processing ...                      |
    |                                                     |
    |     ngx_pfree(pool, large_buf);  // Free once       |
    |                                                     |
    |     // ... more processing ...                      |
    |                                                     |
    |     ngx_pfree(pool, large_buf);  // Free again!     |
    |                                                     |
    |     // Result: Corrupts large allocation list       |
    |     // May crash later in pool destroy              |
    | }                                                   |
    +-----------------------------------------------------+
    
    What happens internally:
    
    After first ngx_pfree:
    
    large list: [large1] -> [large2] -> [large3] -> NULL
                           ^
                           | large_buf points here
                           | large2->alloc = NULL (freed)
    
    After second ngx_pfree:
    
    large list: [large1] -> [large2] -> [large3] -> NULL
                           ^
                           | already NULL!
                           | ngx_free(NULL) - may be OK
                           | but list integrity questionable


Use-After-Free Masked by Pooling
================================

    +-----------------------------------------------------+
    | void bug() {                                        |
    |     obj = ngx_palloc(pool, 64);                     |
    |     obj->value = 42;                                |
    |                                                     |
    |     // "Free" by not using anymore                  |
    |     // (no actual free in pool)                     |
    |                                                     |
    |     // ... allocate more ...                        |
    |     new_obj = ngx_palloc(pool, 64);                 |
    |                                                     |
    |     // BUG: Access old object                       |
    |     printf("%d\n", obj->value);                     |
    |                                                     |
    |     // With pool: May print 42 (memory not reused)  |
    |     //            Or garbage (memory reused)        |
    |     //            But NO CRASH!                     |
    |     //                                              |
    |     // With malloc/free: Would likely crash         |
    |     //                   Or AddressSanitizer catch  |
    | }                                                   |
    +-----------------------------------------------------+
    
    The Masking Problem:
    
    With malloc/free:
    +---------+
    | obj     | -> free() -> [invalid] -> access -> CRASH (good!)
    +---------+
    
    With pool:
    +---------+
    | obj     | -> (no free) -> [still valid] -> access -> WORKS (bad!)
    +---------+
                              or
                  -> [reused] -> access -> garbage (bad!)
    
    Pool MASKS the bug:
    - No crash means no detection
    - Bug may appear only under specific allocation patterns
    - Hard to reproduce, hard to debug


Detection Strategies
====================

    1. Sanitizers (compile-time instrumentation)
    
       $ gcc -fsanitize=address,undefined -g program.c
       
       AddressSanitizer may not catch pool use-after-free
       because memory is still technically valid
    
    2. Valgrind (runtime instrumentation)
    
       $ valgrind --track-origins=yes ./program
       
       Same limitation: pool memory appears valid
    
    3. Debug Pool Mode (custom)
    
       #ifdef DEBUG_POOL
       void *ngx_palloc_debug(pool, size) {
           void *p = ngx_palloc(pool, size + GUARD_SIZE);
           memset(p, POISON_PATTERN, size);
           record_allocation(p, size);
           return p;
       }
       #endif
    
    4. Pool Reset Poisoning
    
       void ngx_reset_pool_debug(pool) {
           // Poison all memory before reset
           memset(pool->d.start, 0xDE, pool->d.end - pool->d.start);
           ngx_reset_pool(pool);
       }
       
       // Now use-after-reset will read garbage pattern
```

**说明（中文）：**

**池内重复释放**：对大分配调用两次 ngx_pfree 会破坏大分配链表，可能在池销毁时崩溃。

**池掩盖的释放后使用**：
- 使用 malloc/free：访问已释放内存通常会崩溃（这是好的，能发现 bug）
- 使用池：内存仍然有效或被重用，不崩溃但结果错误（这是坏的，bug 被掩盖）

**检测策略**：
1. Sanitizers：可能无法捕获池的释放后使用，因为内存技术上仍有效
2. Valgrind：同样的限制
3. 调试池模式：自定义实现，记录分配、填充毒模式
4. 池重置毒化：重置前填充特殊模式，释放后使用会读到垃圾

---

## 7. Debugging Difficulty

```
Hidden Leaks
============

    Symptom: Memory grows but no leak detected
    
    +-----------------------------------------------------+
    | ngx_pool_t *pool;                                   |
    |                                                     |
    | void init() {                                       |
    |     pool = ngx_create_pool(4096, log);              |
    | }                                                   |
    |                                                     |
    | void process_request() {                            |
    |     // Allocate from pool                           |
    |     data = ngx_palloc(pool, 256);                   |
    |                                                     |
    |     // Never freed (pool never destroyed)           |
    | }                                                   |
    +-----------------------------------------------------+
    
    After 1000 requests:
    
    Pool state:
    +--------+--------+--------+--------+--------+
    | Block1 | Block2 | Block3 | Block4 | Block5 | ...
    +--------+--------+--------+--------+--------+
    
    Valgrind: "No leaks detected"
    Reality: 256KB wasted, growing indefinitely
    
    
    Diagnosis Tools:
    
    1. Pool statistics (add to pool struct in debug build):
    
       typedef struct {
           size_t total_allocated;
           size_t total_blocks;
           size_t large_count;
       } ngx_pool_stats_t;
       
       void ngx_pool_dump_stats(ngx_pool_t *pool) {
           // Print allocation statistics
       }
    
    2. Allocation tracking:
    
       #define ngx_palloc(pool, size) \
           ngx_palloc_tracked(pool, size, __FILE__, __LINE__)


Masked Bugs
===========

    Pool Behavior That Hides Bugs:
    
    +-------------------------------------------------------------+
    | Bug Type           | malloc Behavior    | Pool Behavior     |
    +--------------------+--------------------+-------------------+
    | Use-after-free     | Crash/corruption   | May work          |
    | Double free        | Crash              | Silent corruption |
    | Buffer overflow    | Crash/corruption   | May corrupt pool  |
    | Memory leak        | Detected by tools  | "Valid" retention |
    +--------------------+--------------------+-------------------+
    
    
    Bug Amplification Pattern:
    
    Development:
    +--------+
    | Pool   |-----> allocations freed on each request
    | Reset  |       bugs masked by quick reset
    +--------+
    
    Production:
    +--------+
    | Pool   |-----> allocations accumulate
    | No     |       bugs manifest after hours/days
    | Reset  |       hard to reproduce
    +--------+


Debugging Checklist
===================

    When investigating pool issues:
    
    1. Pool Lifecycle:
       [ ] When is pool created?
       [ ] When is pool destroyed?
       [ ] Is pool ever reset?
       [ ] Can pool live too long?
    
    2. Allocation Patterns:
       [ ] What's the allocation rate?
       [ ] Are allocations bounded per request?
       [ ] Are there runaway allocations?
    
    3. Pointer Tracking:
       [ ] Where are pooled pointers stored?
       [ ] Do any pointers outlive pool?
       [ ] Are pointers used after reset?
    
    4. Resource Cleanup:
       [ ] Are cleanup handlers registered?
       [ ] Do cleanup handlers run?
       [ ] Are all resources covered?
    
    
    Debug Techniques:
    
    // Add to pool.h for debug builds
    #if (NGX_DEBUG_PALLOC)
    
    // Force all allocations through large path
    // This makes every allocation individually
    // visible to malloc debuggers
    #undef NGX_MAX_ALLOC_FROM_POOL
    #define NGX_MAX_ALLOC_FROM_POOL 0
    
    #endif
    
    // Now Valgrind/ASAN can track each allocation
```

**说明（中文）：**

**隐藏的泄漏**：
- 症状：内存增长但无泄漏检测
- 原因：池永不销毁，分配累积
- Valgrind 显示 "无泄漏" 但实际上内存在浪费
- 诊断工具：池统计、分配追踪

**被掩盖的 Bug**：

| Bug 类型 | malloc 行为 | 池行为 |
|---------|------------|-------|
| 释放后使用 | 崩溃/损坏 | 可能正常工作 |
| 重复释放 | 崩溃 | 静默损坏 |
| 缓冲区溢出 | 崩溃/损坏 | 可能损坏池 |
| 内存泄漏 | 工具检测到 | "有效"保留 |

**Bug 放大模式**：
- 开发环境：池经常重置，bug 被掩盖
- 生产环境：分配累积，bug 在数小时/天后才显现，难以重现

**调试技巧**：定义 NGX_DEBUG_PALLOC 将 NGX_MAX_ALLOC_FROM_POOL 设为 0，强制所有分配走大分配路径，这样 Valgrind/ASAN 可以追踪每个分配。
