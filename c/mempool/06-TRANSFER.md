# TRANSFER | Designing Memory Pools in Real C Projects

## 1. Decision Framework: Pool vs malloc

```
Decision Tree
=============

                    START
                      |
                      v
    +-------------------------------------+
    | Is this performance-critical code? |
    +------------------+------------------+
                       |
              +--------+--------+
              |                 |
             NO                YES
              |                 |
              v                 v
    +-----------------+  +-------------------------------------+
    | Use malloc      |  | Are there many objects with         |
    | (simple is good)|  | shared/scoped lifetime?             |
    +-----------------+  +------------------+------------------+
                                            |
                               +------------+------------+
                               |                         |
                              NO                        YES
                               |                         |
                               v                         v
                    +-----------------+  +-------------------------------------+
                    | Use malloc      |  | Is the scope boundary clear?        |
                    | (lifetimes vary)|  | (request end, phase complete, etc.) |
                    +-----------------+  +------------------+------------------+
                                                            |
                                               +------------+------------+
                                               |                         |
                                              NO                        YES
                                               |                         |
                                               v                         v
                                    +-----------------+       +-------------------+
                                    | Reconsider      |       | USE MEMORY POOL   |
                                    | design or use   |       +-------------------+
                                    | hybrid approach |
                                    +-----------------+


Fixed vs Variable Size Decision
===============================

    +--------------------------------------------------+
    | Fixed-Size Pool                                  |
    +--------------------------------------------------+
    | Use when:                                        |
    |   - All objects same type                        |
    |   - Need fast allocation/deallocation            |
    |   - Object reuse valuable (e.g., connections)    |
    |                                                  |
    | Implementation:                                  |
    |   - Free list of identical slots                 |
    |   - O(1) alloc and free                          |
    |   - No fragmentation                             |
    +--------------------------------------------------+
    
    +--------------------------------------------------+
    | Variable-Size Pool (nginx style)                 |
    +--------------------------------------------------+
    | Use when:                                        |
    |   - Objects vary in size                         |
    |   - All freed at once (scope boundary)           |
    |   - Individual free not needed                   |
    |                                                  |
    | Implementation:                                  |
    |   - Bump allocator                               |
    |   - O(1) alloc, bulk free only                   |
    |   - Internal fragmentation accepted              |
    +--------------------------------------------------+
    
    Example: Choose pool type
    
    Connection objects (same size, reused):
        -> Fixed-size pool with free list
    
    HTTP headers (varied size, freed together):
        -> Variable-size bump allocator
    
    Configuration keys (varied, long-lived):
        -> malloc or global pool
```

**说明（中文）：**

**决策树**：
1. 是否性能关键？否 → malloc
2. 是否有多个共享/作用域生命周期的对象？否 → malloc
3. 作用域边界是否清晰？否 → 重新考虑设计；是 → 使用内存池

**固定 vs 可变大小决策**：
- 固定大小池：所有对象同类型、需要快速分配/释放、对象重用有价值
- 可变大小池：对象大小各异、全部一起释放、不需要单独释放

---

## 2. Rules of Thumb for Pool Design

```
Rule 1: Make Lifetime Explicit in API
=====================================

    BAD: Implicit lifetime
    +-----------------------------------------------------+
    | char *get_data();  // Who frees this? When?         |
    +-----------------------------------------------------+
    
    GOOD: Explicit pool parameter
    +-----------------------------------------------------+
    | char *get_data(ngx_pool_t *pool);                   |
    | // Caller provides pool, caller controls lifetime   |
    +-----------------------------------------------------+
    
    BETTER: Pool embedded in context
    +-----------------------------------------------------+
    | typedef struct {                                    |
    |     ngx_pool_t *pool;                               |
    |     // ... other fields ...                         |
    | } context_t;                                        |
    |                                                     |
    | char *get_data(context_t *ctx);                     |
    | // Allocates from ctx->pool                         |
    | // Lifetime tied to context                         |
    +-----------------------------------------------------+


Rule 2: Minimize Pool Types
===========================

    BAD: Many pool types
    +-----------------------------------------------------+
    | ngx_pool_t *header_pool;                            |
    | ngx_pool_t *body_pool;                              |
    | ngx_pool_t *response_pool;                          |
    | ngx_pool_t *temp_pool;                              |
    | // Hard to reason about interactions                |
    +-----------------------------------------------------+
    
    GOOD: Few, well-defined pool lifetimes
    +-----------------------------------------------------+
    | ngx_pool_t *connection_pool;  // Per-connection     |
    | ngx_pool_t *request_pool;     // Per-request        |
    |                                                     |
    | // All request data goes to request_pool            |
    | // Connection-persistent data to connection_pool    |
    +-----------------------------------------------------+
    
    Pool Hierarchy Pattern:
    
    Process Pool (cycle->pool)
         |
         +-- Configuration (lives forever)
         |
    Connection Pool (c->pool)
         |
         +-- Connection state
         |
    Request Pool (r->pool)
         |
         +-- Request headers, body, response


Rule 3: Document Invariants
===========================

    /**
     * Allocates header structure from request pool.
     *
     * INVARIANTS:
     *   - Returned pointer valid until request pool destroyed
     *   - Caller must NOT free returned pointer
     *   - Multiple calls return independent headers
     *
     * OWNERSHIP:
     *   - Pool owns memory
     *   - Caller owns header content (can modify)
     *
     * THREAD SAFETY:
     *   - NOT thread-safe (request pool is single-threaded)
     */
    ngx_http_header_t *ngx_alloc_header(ngx_pool_t *pool);
    
    
    Documentation Checklist:
    
    [ ] Who allocates?
    [ ] Who deallocates?
    [ ] What is the lifetime?
    [ ] Is it thread-safe?
    [ ] What happens on failure?
    [ ] Are there cleanup handlers?
```

**说明（中文）：**

**规则1 - 在 API 中显式表达生命周期**：
- 差：隐式生命周期，`char *get_data()` 谁释放？何时？
- 好：显式池参数，调用者控制生命周期
- 更好：池嵌入上下文，生命周期与上下文绑定

**规则2 - 最小化池类型**：
- 差：多种池类型难以理解交互
- 好：少数定义良好的池生命周期（连接池、请求池）

**规则3 - 记录不变式**：记录谁分配、谁释放、生命周期、线程安全性、失败行为、清理处理器。

---

## 3. Combining Pools with pthreads and Event Loops

```
pthreads Integration
====================

    Pattern 1: Thread-local pools
    
    +-----------------------------------------------------+
    | __thread ngx_pool_t *tls_pool = NULL;               |
    |                                                     |
    | void *worker_thread(void *arg) {                    |
    |     tls_pool = ngx_create_pool(4096, log);          |
    |     if (tls_pool == NULL) {                         |
    |         return NULL;                                |
    |     }                                               |
    |                                                     |
    |     while (running) {                               |
    |         process_task();                             |
    |         ngx_reset_pool(tls_pool);  // Reuse pool    |
    |     }                                               |
    |                                                     |
    |     ngx_destroy_pool(tls_pool);                     |
    |     return NULL;                                    |
    | }                                                   |
    +-----------------------------------------------------+
    
    Thread 1           Thread 2           Thread 3
        |                  |                  |
        v                  v                  v
    +-------+          +-------+          +-------+
    |Pool 1 |          |Pool 2 |          |Pool 3 |
    +-------+          +-------+          +-------+
        |                  |                  |
    No sharing = No locking needed
    
    
    Pattern 2: Pool passed to task
    
    +-----------------------------------------------------+
    | typedef struct {                                    |
    |     ngx_pool_t *pool;  // Task owns pool            |
    |     void *data;                                     |
    | } task_t;                                           |
    |                                                     |
    | void submit_task(task_t *task) {                    |
    |     // Pool created by submitter                    |
    |     // Pool destroyed by task after completion      |
    |     task->pool = ngx_create_pool(4096, log);        |
    |     pthread_pool_submit(task);                      |
    | }                                                   |
    |                                                     |
    | void execute_task(task_t *task) {                   |
    |     // Use task's pool for allocations              |
    |     result = ngx_palloc(task->pool, size);          |
    |     // ...                                          |
    |     ngx_destroy_pool(task->pool);                   |
    | }                                                   |
    +-----------------------------------------------------+


Event Loop Integration
======================

    Pattern: Pool per event context
    
    +----------------------------------------------------------+
    |                     Event Loop                           |
    +----------------------------------------------------------+
    |                          |                               |
    |    +---------------------|---------------------+         |
    |    |                     v                     |         |
    |    |   +-------------+       +-------------+   |         |
    |    |   | Connection  |       | Connection  |   |         |
    |    |   | Event       |       | Event       |   |         |
    |    |   +------+------+       +------+------+   |         |
    |    |          |                     |          |         |
    |    |          v                     v          |         |
    |    |   +------+------+       +------+------+   |         |
    |    |   | conn->pool  |       | conn->pool  |   |         |
    |    |   +-------------+       +-------------+   |         |
    |    |                                           |         |
    |    +-------------------------------------------+         |
    +----------------------------------------------------------+
    
    Each connection has its own pool
    Pool lives as long as connection
    No cross-connection pool access
    
    
    nginx Event Model:
    
    +-----------------------------------------------------+
    | void ngx_event_accept(ngx_event_t *ev) {            |
    |     // Accept new connection                        |
    |     c = ngx_get_connection(s, ev->log);             |
    |                                                     |
    |     // Create connection pool                       |
    |     c->pool = ngx_create_pool(64, ev->log);         |
    |                                                     |
    |     // Pool destroyed on connection close           |
    | }                                                   |
    |                                                     |
    | void ngx_close_connection(ngx_connection_t *c) {    |
    |     // Destroy pool, freeing all connection memory  |
    |     ngx_destroy_pool(c->pool);                      |
    | }                                                   |
    +-----------------------------------------------------+


Hybrid Pattern: Request Pool + Connection Pool
==============================================

    Connection
    +------------------------------------------+
    | c->pool (connection-scoped)              |
    |   - SSL context                          |
    |   - Connection metadata                  |
    |                                          |
    |   Request 1                              |
    |   +----------------------------------+   |
    |   | r->pool (request-scoped)         |   |
    |   |   - Headers                      |   |
    |   |   - Body                         |   |
    |   +----------------------------------+   |
    |                                          |
    |   Request 2 (HTTP keepalive)             |
    |   +----------------------------------+   |
    |   | r->pool (new pool per request)   |   |
    |   |   - Headers                      |   |
    |   |   - Body                         |   |
    |   +----------------------------------+   |
    +------------------------------------------+
    
    Lifetime:
      c->pool: lives for entire connection
      r->pool: lives for one request, destroyed before next request
```

**说明（中文）：**

**pthreads 集成**：
- 模式1：线程本地池，每个线程有独立池，无共享 = 无需锁
- 模式2：池传递给任务，提交者创建池，执行者销毁池

**事件循环集成**：
- 每个连接事件有自己的池
- 池与连接同生命周期
- 无跨连接池访问

**混合模式**：连接池 + 请求池
- 连接池存储 SSL 上下文、连接元数据
- 请求池存储 headers、body，每个请求独立

---

## 4. Testing Strategies

```
Stress Tests
============

    Test 1: Allocation Storm
    +-----------------------------------------------------+
    | void test_allocation_storm() {                      |
    |     ngx_pool_t *pool = ngx_create_pool(4096, log);  |
    |                                                     |
    |     for (int i = 0; i < 100000; i++) {              |
    |         size_t size = random() % 1024 + 1;          |
    |         void *p = ngx_palloc(pool, size);           |
    |         assert(p != NULL);                          |
    |         memset(p, 0xAA, size);  // Touch memory     |
    |     }                                               |
    |                                                     |
    |     ngx_destroy_pool(pool);                         |
    | }                                                   |
    +-----------------------------------------------------+
    
    Test 2: Pool Cycling
    +-----------------------------------------------------+
    | void test_pool_cycling() {                          |
    |     for (int cycle = 0; cycle < 10000; cycle++) {   |
    |         ngx_pool_t *pool;                           |
    |         pool = ngx_create_pool(4096, log);          |
    |                                                     |
    |         for (int i = 0; i < 100; i++) {             |
    |             ngx_palloc(pool, random() % 512 + 1);   |
    |         }                                           |
    |                                                     |
    |         ngx_destroy_pool(pool);                     |
    |     }                                               |
    |     // Check: no memory growth after test           |
    | }                                                   |
    +-----------------------------------------------------+
    
    Test 3: Large Allocation Mix
    +-----------------------------------------------------+
    | void test_large_small_mix() {                       |
    |     ngx_pool_t *pool = ngx_create_pool(4096, log);  |
    |                                                     |
    |     for (int i = 0; i < 1000; i++) {                |
    |         // Small allocation                         |
    |         ngx_palloc(pool, 64);                       |
    |                                                     |
    |         // Large allocation                         |
    |         void *large = ngx_palloc(pool, 8192);       |
    |                                                     |
    |         // Free large (should work)                 |
    |         ngx_pfree(pool, large);                     |
    |     }                                               |
    |                                                     |
    |     ngx_destroy_pool(pool);                         |
    | }                                                   |
    +-----------------------------------------------------+


Allocation Pattern Tests
========================

    Test: Request Simulation
    +-----------------------------------------------------+
    | void test_request_pattern() {                       |
    |     // Simulate realistic request processing        |
    |                                                     |
    |     for (int req = 0; req < 10000; req++) {         |
    |         ngx_pool_t *pool;                           |
    |         pool = ngx_create_pool(4096, log);          |
    |                                                     |
    |         // Simulate headers (many small allocs)     |
    |         for (int h = 0; h < 50; h++) {              |
    |             ngx_palloc(pool, 32 + random() % 96);   |
    |         }                                           |
    |                                                     |
    |         // Simulate body (one large alloc)          |
    |         ngx_palloc(pool, 1024 + random() % 8192);   |
    |                                                     |
    |         // Simulate response building               |
    |         for (int r = 0; r < 20; r++) {              |
    |             ngx_palloc(pool, 64 + random() % 256);  |
    |         }                                           |
    |                                                     |
    |         ngx_destroy_pool(pool);                     |
    |     }                                               |
    | }                                                   |
    +-----------------------------------------------------+


Failure Injection
=================

    Test: Allocation Failure Handling
    +-----------------------------------------------------+
    | // Mock allocator that fails after N allocations    |
    | static int alloc_count = 0;                         |
    | static int fail_after = 0;                          |
    |                                                     |
    | void *mock_memalign(size_t align, size_t size) {    |
    |     if (fail_after > 0 && alloc_count >= fail_after)|
    |         return NULL;                                |
    |     }                                               |
    |     alloc_count++;                                  |
    |     return real_memalign(align, size);              |
    | }                                                   |
    |                                                     |
    | void test_failure_handling() {                      |
    |     for (int fail_point = 1; fail_point < 100;      |
    |          fail_point++) {                            |
    |         alloc_count = 0;                            |
    |         fail_after = fail_point;                    |
    |                                                     |
    |         ngx_pool_t *pool;                           |
    |         pool = ngx_create_pool(4096, log);          |
    |                                                     |
    |         if (pool == NULL) {                         |
    |             // Pool creation failed - OK            |
    |             continue;                               |
    |         }                                           |
    |                                                     |
    |         // Try allocations, expect some to fail     |
    |         for (int i = 0; i < 50; i++) {              |
    |             void *p = ngx_palloc(pool, 128);        |
    |             // p may be NULL - that's OK            |
    |         }                                           |
    |                                                     |
    |         // Must not crash on destroy                |
    |         ngx_destroy_pool(pool);                     |
    |     }                                               |
    | }                                                   |
    +-----------------------------------------------------+


Memory Sanitizer Integration
============================

    Makefile:
    +-----------------------------------------------------+
    | CFLAGS_DEBUG = -fsanitize=address,undefined \       |
    |                -fno-omit-frame-pointer \            |
    |                -g                                   |
    |                                                     |
    | CFLAGS_POOL_DEBUG = -DNGX_DEBUG_PALLOC=1            |
    |                                                     |
    | test_debug: CFLAGS += $(CFLAGS_DEBUG)               |
    | test_debug: CFLAGS += $(CFLAGS_POOL_DEBUG)          |
    | test_debug: test                                    |
    +-----------------------------------------------------+
    
    When NGX_DEBUG_PALLOC=1:
    - All allocations go through large path
    - Each allocation visible to sanitizers
    - Slower but catches more bugs
```

**说明（中文）：**

**压力测试**：
- 分配风暴：大量随机大小分配
- 池循环：重复创建/销毁池
- 大小混合：混合小分配和大分配

**分配模式测试**：模拟真实请求处理，包括 headers（多个小分配）、body（一个大分配）、response（中等分配）。

**故障注入**：模拟分配器在 N 次分配后失败，测试失败处理是否正确。

**内存 Sanitizer 集成**：使用 -fsanitize=address,undefined，配合 NGX_DEBUG_PALLOC=1 使所有分配走大分配路径，便于调试工具捕获问题。

---

## 5. When to Abandon Pooling

```
Signs That Pooling is Wrong
===========================

    RED FLAGS:
    
    [ ] Frequent individual object deallocation needed
        -> Pool model doesn't support this efficiently
    
    [ ] Objects have vastly different lifetimes
        -> Pool forces shared lifetime
    
    [ ] Debugging becomes impossible
        -> Bugs hidden by pool behavior
    
    [ ] More pool management code than actual logic
        -> Complexity outweighs benefit
    
    [ ] Performance not actually improved
        -> Measure before/after, not just assume


Migration Path: Pool -> malloc
==============================

    Step 1: Identify pool usage points
    
    $ grep -r "ngx_palloc" src/
    $ grep -r "ngx_pcalloc" src/
    
    Step 2: Categorize by necessity
    
    +-------------------------------------------+
    | Essential (keep pooled)                   |
    |   - High-frequency, short-lived           |
    |   - Matches request lifecycle             |
    +-------------------------------------------+
    | Questionable (evaluate)                   |
    |   - Medium frequency                      |
    |   - Lifetime unclear                      |
    +-------------------------------------------+
    | Unnecessary (convert to malloc)           |
    |   - Low frequency                         |
    |   - Long-lived or permanent               |
    +-------------------------------------------+
    
    Step 3: Convert gradually
    
    // Before
    config = ngx_palloc(cycle->pool, sizeof(*config));
    
    // After
    config = malloc(sizeof(*config));
    // Add to shutdown cleanup


Simplification Patterns
=======================

    Pattern 1: Remove unnecessary pool
    
    // Before: Pool for one-time operation
    void init() {
        ngx_pool_t *pool = ngx_create_pool(4096, log);
        data = ngx_palloc(pool, sizeof(*data));
        // ... use data forever ...
        // Pool never destroyed - LEAK
    }
    
    // After: Simple malloc
    void init() {
        data = malloc(sizeof(*data));
        // Freed in shutdown()
    }
    
    
    Pattern 2: Use stack instead of pool
    
    // Before: Pool for temporary
    void process(ngx_pool_t *pool) {
        char *temp = ngx_palloc(pool, 256);
        snprintf(temp, 256, "...");
        // ...
    }
    
    // After: Stack allocation
    void process() {
        char temp[256];  // On stack
        snprintf(temp, 256, "...");
        // Auto cleanup
    }
    
    
    Pattern 3: Static allocation
    
    // Before: Pool for singleton
    ngx_pool_t *global_pool;
    config_t *config;
    
    void init() {
        global_pool = ngx_create_pool(4096, log);
        config = ngx_palloc(global_pool, sizeof(*config));
    }
    
    // After: Static singleton
    static config_t config;
    
    void init() {
        // config already allocated (static)
        config.field = value;
    }


Final Checklist: Keep or Remove Pool?
=====================================

    KEEP POOL IF:
    [x] High allocation frequency (>1000/sec)
    [x] Clear scope boundary (request, connection)
    [x] Many objects freed together
    [x] Latency-sensitive path
    [x] Design naturally fits pool model
    
    REMOVE POOL IF:
    [x] Low allocation frequency
    [x] Objects have independent lifetimes
    [x] Need individual deallocation
    [x] Pool lifetime unclear
    [x] Debugging difficulty exceeds benefit
    [x] Code complexity increased
    
    WHEN IN DOUBT:
    -> Start with malloc
    -> Profile before optimizing
    -> Add pools only where measured benefit
```

**说明（中文）：**

**池使用错误的信号**：
- 需要频繁单独释放对象
- 对象生命周期差异大
- 调试变得不可能
- 池管理代码比实际逻辑还多
- 性能实际上没有提升

**迁移路径：池 → malloc**：
1. 识别池使用点
2. 按必要性分类
3. 逐步转换

**简化模式**：
- 移除不必要的池（一次性操作用 malloc）
- 使用栈替代池（临时变量）
- 使用静态分配（单例）

**最终检查清单**：
- 保留池：高频分配、清晰作用域、批量释放、延迟敏感
- 移除池：低频分配、独立生命周期、需要单独释放
- 存疑时：从 malloc 开始，先分析再优化
