# WHERE | How Memory Pools Appear in Real Projects

## 1. Where Memory Pools Should Live in a Codebase

```
Codebase Architecture with Memory Pools
=======================================

    +---------------------------------------------------------------+
    |                     APPLICATION LAYER                         |
    |   +-------------------+   +-------------------+               |
    |   | Request Handler   |   | Business Logic    |               |
    |   +-------------------+   +-------------------+               |
    |           |                       |                           |
    |           | USE pool              | USE pool                  |
    |           v                       v                           |
    +---------------------------------------------------------------+
    |                    INFRASTRUCTURE LAYER                       |
    |   +-------------------+   +-------------------+               |
    |   | Memory Pool API   |   | Logging / Config  |               |
    |   |  ngx_palloc.c     |   |                   |               |
    |   +-------------------+   +-------------------+               |
    |           |                                                   |
    |           | OWNS pool implementation                          |
    |           v                                                   |
    +---------------------------------------------------------------+
    |                      PLATFORM LAYER                           |
    |   +-------------------+   +-------------------+               |
    |   | OS Abstraction    |   | Low-level alloc   |               |
    |   | ngx_alloc.c       |   | (memalign, etc.)  |               |
    +---------------------------------------------------------------+


Pool Code Location Rules:
=========================

    DO:
      [x] Place pool implementation in core/infrastructure
      [x] Keep pool API minimal and stable
      [x] Abstract platform differences in pool code
      [x] Pool code is dependency of app code, not reverse

    DON'T:
      [ ] Embed pool logic in business code
      [ ] Create multiple incompatible pool implementations
      [ ] Let app code depend on pool internals
      [ ] Use pool implementation details outside pool code


nginx Source Organization:
==========================

    src/
    +-- core/
    |   +-- ngx_palloc.h     <-- Pool API (public interface)
    |   +-- ngx_palloc.c     <-- Pool implementation
    |   +-- ngx_alloc.h      <-- Low-level allocation
    |   +-- ngx_alloc.c      <-- Platform abstraction
    |   +-- ngx_core.h       <-- Includes pool API
    |
    +-- http/
    |   +-- ngx_http_request.c   <-- Uses pool for requests
    |   +-- modules/
    |       +-- ...              <-- Modules use pool via request
    |
    +-- event/
        +-- ngx_event.c          <-- Uses pool for connections
```

**说明（中文）：**

此图展示了内存池在代码库中的正确位置：

**架构分层**：
- 应用层：请求处理器、业务逻辑 → 使用池
- 基础设施层：内存池 API、日志配置 → 拥有池实现
- 平台层：OS 抽象、底层分配 → 提供 memalign 等

**规则**：
- 池实现放在 core/基础设施层
- 池 API 保持最小和稳定
- 在池代码中抽象平台差异
- 应用代码不应依赖池内部实现

**nginx 源码组织**：core/ 目录包含池实现，http/ 和 event/ 目录使用池 API。

---

## 2. Typical Usage Patterns

```
Pattern 1: Per-Request Pool (nginx HTTP)
========================================

    Client                      nginx
       |                          |
       | HTTP Request             |
       +------------------------->|
       |                          |
       |                    +-----+-----+
       |                    | Create    |
       |                    | request   |
       |                    | pool      |
       |                    +-----+-----+
       |                          |
       |                    +-----+-----+
       |                    | Parse     |
       |                    | headers   |<-- allocate from pool
       |                    +-----+-----+
       |                          |
       |                    +-----+-----+
       |                    | Process   |
       |                    | body      |<-- allocate from pool
       |                    +-----+-----+
       |                          |
       |                    +-----+-----+
       |                    | Build     |
       |                    | response  |<-- allocate from pool
       |                    +-----+-----+
       |                          |
       |       HTTP Response      |
       |<-------------------------+
       |                          |
       |                    +-----+-----+
       |                    | Destroy   |
       |                    | request   |
       |                    | pool      |<-- ALL memory freed
       |                    +-----+-----+

    Code Pattern:
    
    void ngx_http_process_request(ngx_http_request_t *r) {
        // r->pool already created when connection accepted
        
        headers = ngx_palloc(r->pool, sizeof(*headers));
        body = ngx_palloc(r->pool, body_size);
        
        // ... process request ...
        
        // Pool destroyed when request completes
        // No manual free() calls needed
    }


Pattern 2: Per-Connection Pool (nginx events)
=============================================

    +-------------------+
    | Connection Accept |
    +--------+----------+
             |
             v
    +-------------------+
    | Create conn pool  |  <-- Pool lives as long as connection
    +--------+----------+
             |
             v
    +-------------------+
    | Request 1         |  <-- Per-request pool (child)
    |   create/destroy  |
    +--------+----------+
             |
             v
    +-------------------+
    | Request 2         |  <-- Per-request pool (child)
    |   create/destroy  |
    +--------+----------+
             |
             v
    +-------------------+
    | Request N         |  <-- Per-request pool (child)
    |   create/destroy  |
    +--------+----------+
             |
             v
    +-------------------+
    | Connection Close  |
    | Destroy conn pool |  <-- Connection-level memory freed
    +-------------------+

    Hierarchy:
    
    Connection Pool (longer-lived)
    +--------------------------------+
    | SSL context                    |
    | Connection buffers             |
    | Peer address info              |
    +--------------------------------+
            |
            +----> Request Pool 1 (short-lived)
            |      +--------------------+
            |      | Headers, body      |
            |      +--------------------+
            |
            +----> Request Pool 2 (short-lived)
                   +--------------------+
                   | Headers, body      |
                   +--------------------+


Pattern 3: Per-Thread Pool
==========================

    +-------------------+     +-------------------+
    | Worker Thread 1   |     | Worker Thread 2   |
    +-------------------+     +-------------------+
            |                         |
            v                         v
    +---------------+         +---------------+
    | Thread Pool 1 |         | Thread Pool 2 |
    +---------------+         +---------------+
            |                         |
    +-------+-------+         +-------+-------+
    |               |         |               |
    v               v         v               v
    Task A          Task B    Task C          Task D
    (uses pool)     (uses pool)  (uses pool)  (uses pool)
    
    Benefits:
      - No locking between threads
      - Cache locality per thread
      - Simple ownership model
    
    Implementation:
    
    __thread ngx_pool_t *thread_pool;  // Thread-local storage
    
    void worker_init() {
        thread_pool = ngx_create_pool(4096, log);
    }
    
    void worker_destroy() {
        ngx_destroy_pool(thread_pool);
    }


Pattern 4: Global Object Pool
=============================

    +-------------------+
    | Application Start |
    +--------+----------+
             |
             v
    +-------------------+
    | Create global pool|
    | (configuration)   |
    +--------+----------+
             |
             v
    +-------------------+
    | Load config       |
    | Parse settings    |  <-- Allocate from global pool
    | Build routes      |
    +--------+----------+
             |
             v
    +-------------------+
    | Run server        |
    | (config is        |  <-- Config memory remains valid
    |  read-only now)   |
    +--------+----------+
             |
             v
    +-------------------+
    | Shutdown          |
    | Destroy global    |
    +-------------------+

    nginx Configuration Pool:
    
    ngx_cycle_t {
        ngx_pool_t  *pool;   // <-- Configuration pool
        ...
    }
    
    // Created at startup
    cycle->pool = ngx_create_pool(NGX_CYCLE_POOL_SIZE, log);
    
    // Lives for entire process lifetime
    // Destroyed at shutdown
```

**说明（中文）：**

此图展示了四种典型的池使用模式：

**模式1 - 每请求池**（nginx HTTP）：
- 请求到达时创建池
- 在池中分配 headers、body、response
- 响应发送后销毁池，所有内存自动释放
- 无需手动 free() 调用

**模式2 - 每连接池**：
- 连接接受时创建连接池（长生命周期）
- 每个请求创建子请求池（短生命周期）
- 连接池存储 SSL 上下文、连接缓冲区
- 请求池存储 headers、body

**模式3 - 每线程池**：
- 每个工作线程有独立池
- 无线程间锁竞争
- 每线程缓存局部性
- 使用线程本地存储

**模式4 - 全局对象池**：
- 应用启动时创建
- 用于配置、路由等长期数据
- 进程生命周期内保持有效
- 关闭时销毁

---

## 3. API Structure

```
nginx Pool API Design
=====================

    PUBLIC API (ngx_palloc.h):
    ==========================
    
    // Lifecycle
    ngx_pool_t *ngx_create_pool(size_t size, ngx_log_t *log);
    void ngx_destroy_pool(ngx_pool_t *pool);
    void ngx_reset_pool(ngx_pool_t *pool);
    
    // Allocation
    void *ngx_palloc(ngx_pool_t *pool, size_t size);   // aligned
    void *ngx_pnalloc(ngx_pool_t *pool, size_t size);  // unaligned
    void *ngx_pcalloc(ngx_pool_t *pool, size_t size);  // zero-filled
    void *ngx_pmemalign(ngx_pool_t *pool, size_t size, size_t alignment);
    
    // Deallocation (large only)
    ngx_int_t ngx_pfree(ngx_pool_t *pool, void *p);
    
    // Cleanup
    ngx_pool_cleanup_t *ngx_pool_cleanup_add(ngx_pool_t *p, size_t size);
    void ngx_pool_run_cleanup_file(ngx_pool_t *p, ngx_fd_t fd);
    void ngx_pool_cleanup_file(void *data);
    void ngx_pool_delete_file(void *data);


    INTERNAL API (static functions):
    ================================
    
    static ngx_inline void *ngx_palloc_small(...);  // not exported
    static void *ngx_palloc_block(...);             // not exported
    static void *ngx_palloc_large(...);             // not exported


API Design Principles:
======================

    1. Opaque Handle Pattern:
    
       ngx_pool_t *pool;  // User sees pointer, not internals
       
       // User CANNOT access:
       pool->d.last     // Implementation detail
       pool->current    // Implementation detail
       
       // User CAN only use:
       ngx_palloc(pool, size);


    2. Explicit Init/Destroy:
    
       // Creation
       pool = ngx_create_pool(4096, log);
       if (pool == NULL) {
           return ERROR;  // Must handle failure
       }
       
       // Destruction
       ngx_destroy_pool(pool);  // Mandatory, no implicit cleanup


    3. Consistent Error Handling:
    
       ptr = ngx_palloc(pool, size);
       if (ptr == NULL) {
           // Allocation failed - pool may be out of memory
           // or system malloc failed
           return ERROR;
       }


    4. Resource Registration Pattern:
    
       ngx_pool_cleanup_t *cln;
       
       cln = ngx_pool_cleanup_add(pool, sizeof(file_info));
       if (cln == NULL) {
           return ERROR;
       }
       
       cln->handler = close_file_handler;
       cln->data = file_info;
       
       // When pool destroyed: close_file_handler(file_info) called


Wrapper Patterns for Application Use:
=====================================

    // Type-safe allocation wrapper
    #define ngx_pool_alloc_type(pool, type) \
        ((type *)ngx_palloc(pool, sizeof(type)))
    
    // Usage
    ngx_http_request_t *r = ngx_pool_alloc_type(pool, ngx_http_request_t);


    // Array allocation wrapper
    #define ngx_pool_alloc_array(pool, n, type) \
        ((type *)ngx_palloc(pool, (n) * sizeof(type)))
    
    // Usage
    int *arr = ngx_pool_alloc_array(pool, 100, int);


    // String duplication using pool
    u_char *ngx_pstrdup(ngx_pool_t *pool, ngx_str_t *src) {
        u_char *dst;
        
        dst = ngx_pnalloc(pool, src->len);
        if (dst == NULL) {
            return NULL;
        }
        
        ngx_memcpy(dst, src->data, src->len);
        return dst;
    }
```

**说明（中文）：**

**公开 API**：
- 生命周期：create_pool、destroy_pool、reset_pool
- 分配：palloc（对齐）、pnalloc（未对齐）、pcalloc（清零）
- 释放：pfree（仅限大分配）
- 清理：cleanup_add、cleanup_file

**内部 API**：palloc_small、palloc_block、palloc_large 为静态函数，不导出

**API 设计原则**：
1. 不透明句柄模式：用户只看到指针，不访问内部结构
2. 显式初始化/销毁：必须调用 create_pool 和 destroy_pool
3. 一致的错误处理：返回 NULL 表示失败
4. 资源注册模式：cleanup_add 注册销毁时的清理回调

---

## 4. Auditing Pool Usage

```
Ownership Tracing
=================

    Question: Who owns this pool?
    
    Code Audit Checklist:
    +-------------------------------------------------------+
    | [ ] Pool creator identified                           |
    | [ ] Pool destroyer is same component (or documented)  |
    | [ ] No pool pointer stored in global state            |
    | [ ] No pool pointer passed to unrelated modules       |
    | [ ] Cleanup handlers registered for resources         |
    +-------------------------------------------------------+

    Ownership Diagram:
    
    +-------------------+
    | ngx_http_request  |
    +-------------------+
            |
            | OWNS
            v
    +-------------------+
    | request->pool     |
    +-------------------+
            |
            | ALLOCATED FROM
            v
    +-------------------+   +-------------------+   +-------------------+
    | headers_in        |   | headers_out       |   | request_body      |
    +-------------------+   +-------------------+   +-------------------+
    
    Rule: request owns pool, pool owns allocations
    Consequence: destroying request destroys all


Lifetime Boundary Analysis:
===========================

    Function Analysis Template:
    
    void process_request(ngx_http_request_t *r) {
        // ENTRY: r->pool is ACTIVE
        
        temp = ngx_palloc(r->pool, size);
        // temp is valid because:
        //   1. Pool was created before this function
        //   2. Pool outlives this function
        
        // EXIT: r->pool still ACTIVE
        // Caller responsible for pool lifetime
    }
    
    
    Dangerous Patterns to Find:
    
    // Pattern 1: Storing pool pointer globally
    static ngx_pool_t *global_pool;
    void init() {
        global_pool = ngx_create_pool(4096, log);
    }
    void process() {
        // Who destroys global_pool? When?
        // Answer must be documented.
    }
    
    // Pattern 2: Returning pooled memory
    char *get_name(ngx_pool_t *pool) {
        char *name = ngx_palloc(pool, 64);
        strcpy(name, "example");
        return name;  // Caller must know pool lifetime!
    }
    
    // Pattern 3: Passing pool to callbacks
    void set_callback(callback_t cb, ngx_pool_t *pool) {
        saved_cb = cb;
        saved_pool = pool;
        // When is callback invoked? Is pool still alive?
    }


Code Review Questions:
======================

    For each pool usage, ask:
    
    1. CREATION:
       [ ] Where is this pool created?
       [ ] What size/log are used?
       [ ] Is creation failure handled?
    
    2. USAGE:
       [ ] Is every allocation checked for NULL?
       [ ] Are large allocations intentional?
       [ ] Is alignment needed?
    
    3. LIFETIME:
       [ ] When is this pool destroyed?
       [ ] Is it destroyed on all code paths (including errors)?
       [ ] Are there circular references?
    
    4. CLEANUP:
       [ ] Are file descriptors registered for cleanup?
       [ ] Are external resources tracked?
       [ ] Is cleanup order correct?
```

**说明（中文）：**

**所有权追踪**：
- 识别池创建者
- 确认销毁者是同一组件
- 检查池指针未存储在全局状态
- 确认清理处理器已注册

**生命周期边界分析**：
- 函数入口时池是否 ACTIVE
- 函数出口时谁负责池生命周期
- 识别危险模式：全局存储池指针、返回池分配的内存、将池传递给回调

**代码审查问题**：
- 创建：在哪创建？大小？失败处理？
- 使用：分配是否检查 NULL？对齐需求？
- 生命周期：何时销毁？所有路径都销毁？
- 清理：资源是否注册清理？

---

## 5. Error and Cleanup Paths

```
Error Path Handling
===================

    ANTI-PATTERN: Leaking pool on error
    
    int process() {
        ngx_pool_t *pool = ngx_create_pool(4096, log);
        if (pool == NULL) return ERROR;
        
        obj1 = ngx_palloc(pool, size1);
        if (obj1 == NULL) return ERROR;  // LEAK! pool not destroyed
        
        obj2 = ngx_palloc(pool, size2);
        if (obj2 == NULL) return ERROR;  // LEAK! pool not destroyed
        
        ngx_destroy_pool(pool);
        return OK;
    }


    CORRECT: Single cleanup point
    
    int process() {
        ngx_pool_t *pool = ngx_create_pool(4096, log);
        int rc = ERROR;
        
        if (pool == NULL) {
            return ERROR;
        }
        
        obj1 = ngx_palloc(pool, size1);
        if (obj1 == NULL) {
            goto cleanup;
        }
        
        obj2 = ngx_palloc(pool, size2);
        if (obj2 == NULL) {
            goto cleanup;
        }
        
        // ... more processing ...
        
        rc = OK;
        
    cleanup:
        ngx_destroy_pool(pool);
        return rc;
    }


    ALTERNATIVE: Early return with cleanup
    
    int process() {
        ngx_pool_t *pool = ngx_create_pool(4096, log);
        if (pool == NULL) return ERROR;
        
        if (do_work(pool) != OK) {
            ngx_destroy_pool(pool);  // Explicit cleanup before return
            return ERROR;
        }
        
        ngx_destroy_pool(pool);
        return OK;
    }


Cleanup Handler Usage:
======================

    Scenario: Open file, process, close on any exit
    
    int process_file(ngx_pool_t *pool, const char *path) {
        ngx_pool_cleanup_t *cln;
        ngx_pool_cleanup_file_t *clnf;
        int fd;
        
        // Step 1: Open file
        fd = open(path, O_RDONLY);
        if (fd < 0) {
            return ERROR;
        }
        
        // Step 2: Register cleanup IMMEDIATELY after opening
        cln = ngx_pool_cleanup_add(pool, sizeof(ngx_pool_cleanup_file_t));
        if (cln == NULL) {
            close(fd);  // Must close manually if cleanup registration fails
            return ERROR;
        }
        
        cln->handler = ngx_pool_cleanup_file;
        clnf = cln->data;
        clnf->fd = fd;
        clnf->name = path;
        clnf->log = pool->log;
        
        // Step 3: Process file
        // ... any code here can fail ...
        // ... no need to worry about closing fd ...
        
        // Step 4: Pool destroy will call cleanup handler
        return OK;
    }
    
    Cleanup Execution Order:
    
    ngx_destroy_pool(pool):
        1. for each cleanup in reverse registration order:
               cleanup->handler(cleanup->data)
        2. free large allocations
        3. free pool blocks
    
    +---+---+---+
    | C | B | A |  Registration order: A, B, C
    +---+---+---+
        ^
        |
    cleanup list head
    
    Execution order: C, B, A (LIFO - stack order)


Integration with nginx Event Model:
===================================

    +-------------------+
    | Event Loop        |
    +--------+----------+
             |
             | event triggered
             v
    +-------------------+
    | Event Handler     |
    +--------+----------+
             |
             v
    +-------------------+
    | ngx_http_request  |
    |   r->pool         |  <-- Pool created with request
    +--------+----------+
             |
    +--------+--------+--------+
    |                 |        |
    v                 v        v
    [Phase 1]     [Phase 2]   [Phase N]
    handlers      handlers    handlers
    (use pool)    (use pool)  (use pool)
             |
             v
    +-------------------+
    | Request Complete  |
    +--------+----------+
             |
             v
    +-------------------+
    | ngx_destroy_pool  |  <-- Cleanup runs here
    +-------------------+
    
    Key Insight: Pool lifetime matches request lifetime
                 Cleanup handlers run at natural boundary
                 No manual resource tracking needed in handlers
```

**说明（中文）：**

**错误路径处理**：
- 反模式：错误返回前未销毁池，导致泄漏
- 正确方式 1：使用 goto cleanup 统一清理点
- 正确方式 2：每个早期返回前显式调用 destroy_pool

**清理处理器使用**：
1. 打开资源后立即注册清理处理器
2. 如果注册失败，手动清理资源
3. 处理过程中无需担心资源泄漏
4. 池销毁时自动调用清理处理器

**执行顺序**：清理处理器按 LIFO（后进先出）顺序执行，即最后注册的最先执行。

**与 nginx 事件模型集成**：池生命周期与请求生命周期匹配，清理处理器在自然边界运行，处理器中无需手动资源追踪。
