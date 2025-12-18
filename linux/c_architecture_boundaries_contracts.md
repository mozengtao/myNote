# Boundaries and Contracts in C Architectures

A professional guide for systems architects on designing, identifying, and enforcing architectural boundaries in long-lived C codebases.

---

## Table of Contents

1. [What a Boundary Is in C](#step-1--what-a-boundary-is-in-c)
2. [Common Architectural Layers](#step-2--common-architectural-layers-in-c)
3. [Boundary Types and Contracts](#step-3--boundary-types-and-contracts)
4. [Contracts as C Interfaces](#step-4--contracts-as-c-interfaces)
5. [Allowed Interaction Patterns](#step-5--allowed-interaction-patterns-between-layers)
6. [Forbidden Interactions](#step-6--forbidden-interactions-anti-patterns)
7. [Contracts Over Time](#step-7--contracts-over-time-evolution)
8. [Mapping to Real Systems](#step-8--mapping-boundaries-to-real-systems)
9. [Boundary Review Checklist](#step-9--boundary-review-checklist)

---

## Step 1 — What a Boundary Is in C

### The Fundamental Problem

C provides no language-level enforcement of architectural boundaries. Unlike languages with modules, namespaces, or access modifiers, C offers only:

- File-scope `static` for internal linkage
- Header files as informal interface declarations
- Naming conventions (purely voluntary)

**A boundary in C is a contractual agreement enforced by discipline, not by the compiler.**

### Why Boundaries Exist

Boundaries exist to solve fundamental problems in software systems:

| Problem | How Boundaries Help |
|---------|---------------------|
| **Complexity management** | Divide system into comprehensible units |
| **Change isolation** | Modifications in one area don't cascade |
| **Team coordination** | Different teams can own different components |
| **Testing** | Units can be tested in isolation |
| **Reasoning** | Developers can understand behavior locally |

### What Problems Boundaries Prevent

Without boundaries, systems suffer from:

1. **Dependency spaghetti** — Any file can depend on any other file
2. **Modification amplification** — Small changes require modifications across the codebase
3. **Cognitive overload** — Understanding one component requires understanding everything
4. **Testing paralysis** — Unit testing becomes impossible
5. **Ownership ambiguity** — No clear responsibility for correctness

### Conceptual vs Code-Level Boundaries

```
+------------------------------------------------------------------+
|                    CONCEPTUAL BOUNDARIES                          |
|                                                                   |
|   Exist in architecture documents, team agreements,              |
|   and developers' mental models.                                  |
|                                                                   |
|   Example: "The storage engine shall not know about SQL syntax"  |
+------------------------------------------------------------------+
                              |
                              | Implemented via
                              v
+------------------------------------------------------------------+
|                    CODE-LEVEL BOUNDARIES                          |
|                                                                   |
|   Manifest in directory structure, header visibility,            |
|   symbol naming, and build system rules.                          |
|                                                                   |
|   Example: storage/ cannot #include sql_parser.h                 |
+------------------------------------------------------------------+
```

**说明 (Chinese Explanation):**
- 概念边界 (Conceptual Boundaries): 存在于架构文档、团队约定和开发者的心智模型中
- 代码级边界 (Code-Level Boundaries): 通过目录结构、头文件可见性、符号命名和构建系统规则来实现
- 概念边界必须通过代码级机制来落地，否则只是空洞的意图

### The Boundary Enforcement Spectrum

```
     WEAK                                                    STRONG
       |                                                        |
       v                                                        v
+----------+----------+----------+----------+----------+----------+
|  Verbal  | Comments |  Naming  |  Header  | Build    | Separate |
|  Agree-  | in Code  |  Conven- |  File    | System   | Process/ |
|  ment    |          |  tions   |  Control | Rules    | Library  |
+----------+----------+----------+----------+----------+----------+
                                                               
   "Don't    // INTERNAL   db_*     No public  -Werror   libcore.so
   call       DO NOT USE   prefix   header     on        with
   this"      DIRECTLY              exposed    violations defined ABI
```

**说明:**
- 从左到右，边界强制力逐渐增强
- 口头约定最弱，因为没有任何技术手段保证
- 独立进程/库最强，违反边界会导致链接失败或运行时错误

---

## Step 2 — Common Architectural Layers in C

### The Four-Layer Model

```
+================================================================+
|                    APPLICATION / POLICY LAYER                   |
|                                                                 |
|  main(), CLI parsing, configuration, orchestration              |
|  "WHAT the system does and HOW it is configured"                |
+================================================================+
                              |
                              | calls
                              v
+================================================================+
|                    DOMAIN / SERVICE LAYER                       |
|                                                                 |
|  Business logic, workflows, transaction management              |
|  "HOW the system accomplishes its purpose"                      |
+================================================================+
                              |
                              | calls
                              v
+================================================================+
|                    CORE / MECHANISM LAYER                       |
|                                                                 |
|  Data structures, algorithms, utilities                         |
|  "WHAT primitives are available to build with"                  |
+================================================================+
                              |
                              | calls
                              v
+================================================================+
|                    INFRASTRUCTURE / OS LAYER                    |
|                                                                 |
|  System calls, hardware abstraction, platform specifics         |
|  "WHAT the underlying platform provides"                        |
+================================================================+
```

**说明:**
- 应用层 (Application): 决定系统做什么，包含配置和策略决策
- 领域层 (Domain): 实现业务逻辑，处理用户请求的实际工作
- 核心层 (Core): 提供可复用的机制，如数据结构和算法
- 基础设施层 (Infrastructure): 封装平台差异，提供系统服务

### Layer Responsibilities and Rules

#### Application / Policy Layer

| Attribute | Description |
|-----------|-------------|
| **Primary Responsibility** | Configuration, CLI parsing, lifecycle management, policy decisions |
| **Allowed Dependencies** | Domain layer, Core layer (sparingly) |
| **Forbidden Dependencies** | Direct access to Infrastructure internals |
| **Typical Volatility** | HIGH — changes with requirements, user feedback |
| **Typical Files** | `main.c`, `config.c`, `cli.c`, `app_*.c` |

**Key Invariant:** Policy decisions (timeouts, limits, feature flags) live here, not below.

#### Domain / Service Layer

| Attribute | Description |
|-----------|-------------|
| **Primary Responsibility** | Business logic, workflows, state management, validation |
| **Allowed Dependencies** | Core layer, Infrastructure via abstraction |
| **Forbidden Dependencies** | Application layer, UI concerns |
| **Typical Volatility** | MEDIUM — changes with feature additions |
| **Typical Files** | `*_service.c`, `*_manager.c`, `transaction.c` |

**Key Invariant:** Must not know how it is invoked (CLI vs server vs library).

#### Core / Mechanism Layer

| Attribute | Description |
|-----------|-------------|
| **Primary Responsibility** | Reusable algorithms, data structures, utilities |
| **Allowed Dependencies** | Standard library, Infrastructure abstractions |
| **Forbidden Dependencies** | Domain layer, Application layer |
| **Typical Volatility** | LOW — stable foundation, changes carefully |
| **Typical Files** | `hashtable.c`, `buffer.c`, `list.c`, `util_*.c` |

**Key Invariant:** Zero knowledge of business concepts. Pure mechanism.

#### Infrastructure / OS Layer

| Attribute | Description |
|-----------|-------------|
| **Primary Responsibility** | Platform abstraction, system calls, I/O |
| **Allowed Dependencies** | OS headers, hardware interfaces |
| **Forbidden Dependencies** | All higher layers |
| **Typical Volatility** | LOW — changes only for new platforms or OS updates |
| **Typical Files** | `os_*.c`, `platform.c`, `io_*.c`, `mem_*.c` |

**Key Invariant:** Higher layers must work identically regardless of platform.

### Dependency Direction Rule

```
                    ALLOWED DEPENDENCY DIRECTION
                              
                              |
                              v
              +---------------+---------------+
              |                               |
              |   APPLICATION / POLICY        |
              |                               |
              +---------------+---------------+
                              |
                              v
              +---------------+---------------+
              |                               |
              |   DOMAIN / SERVICE            |
              |                               |
              +---------------+---------------+
                              |
                              v
              +---------------+---------------+
              |                               |
              |   CORE / MECHANISM            |
              |                               |
              +---------------+---------------+
                              |
                              v
              +---------------+---------------+
              |                               |
              |   INFRASTRUCTURE / OS         |
              |                               |
              +---------------+---------------+

              FORBIDDEN: Any arrow pointing upward
```

**说明:**
- 依赖方向永远向下，从高层到低层
- 禁止向上依赖：低层代码不得引用高层的头文件或调用高层函数
- 这保证了低层代码的稳定性和可复用性

---

## Step 3 — Boundary Types and Contracts

### 3.1 API Boundaries

**Definition:** The set of functions, types, and constants that a module exposes for external use.

```
+------------------+     Public API     +------------------+
|                  | =================> |                  |
|   Client Code    |                    |   Module         |
|                  | <================= |                  |
+------------------+     Return values  +------------------+
                              
        Public: module.h (visible)
        Private: module_internal.h (hidden)
                 module.c (implementation)
```

| Contract Element | Rule |
|------------------|------|
| **What is enforced** | Only declared functions can be called |
| **What is allowed** | Calling public functions with valid arguments |
| **What is forbidden** | Accessing undeclared symbols, casting to internal types |
| **What breaks** | ABI compatibility, encapsulation, testability |

### 3.2 Data Ownership Boundaries

**Definition:** Rules about which code owns, allocates, mutates, and frees data.

```
OWNERSHIP TRANSFER PATTERNS:

Pattern A: Caller Owns                Pattern B: Callee Owns
+-------------+                       +-------------+
|   Caller    |                       |   Caller    |
|             |                       |             |
|  buf = malloc()                     |  result = func()
|  func(buf)  |                       |             |
|  free(buf)  |                       |  // use result
+-------------+                       |  free_result(result)
                                      +-------------+

Pattern C: Borrowed Reference         Pattern D: Copy-In/Copy-Out
+-------------+                       +-------------+
|   Caller    |                       |   Caller    |
|             |                       |             |
|  func(&data)|                       |  func(input, &output)
|  // data    |                       |  // input unchanged
|  // unchanged                       |  // output is copy
+-------------+                       +-------------+
```

**说明:**
- 调用者拥有 (Caller Owns): 调用者负责分配和释放，被调用者只是借用
- 被调用者拥有 (Callee Owns): 被调用者负责分配，但调用者负责释放
- 借用引用 (Borrowed): 被调用者只读取，不修改，不保留引用
- 复制模式 (Copy): 被调用者复制输入数据，输出也是独立副本

| Contract Element | Rule |
|------------------|------|
| **What is enforced** | Clear ownership at every interface |
| **What is allowed** | Transfer, borrow, or copy with explicit semantics |
| **What is forbidden** | Ambiguous ownership, storing borrowed references |
| **What breaks** | Memory leaks, use-after-free, double-free |

### 3.3 Control Flow Boundaries

**Definition:** Rules about how execution enters, exits, and transitions between modules.

```
ALLOWED CONTROL FLOW:

    Synchronous Call          Callback (Layer-Preserving)
    +-----------+             +-----------+
    |  Caller   |             |  Higher   |
    |           |             |  Layer    |
    |  func()---|--+          |           |
    |           |  |          |  register_callback(cb)
    |   <-------|--+          |           |
    +-----------+             |   cb() <--|-- Lower Layer calls
                              +-----------+   back into Higher


FORBIDDEN CONTROL FLOW:

    Upcall (Layer Inversion)
    +-----------+
    |  Lower    |
    |  Layer    |
    |           |
    |  higher_layer_func()  <-- VIOLATION
    |           |
    +-----------+
```

**说明:**
- 同步调用：高层调用低层，然后返回，这是最简单的模式
- 回调：低层通过函数指针调用高层提供的函数，但回调由高层注册
- 禁止向上调用：低层直接引用和调用高层函数是严格禁止的

| Contract Element | Rule |
|------------------|------|
| **What is enforced** | No direct upward calls; callbacks must be registered |
| **What is allowed** | Downward calls, callbacks with clear ownership |
| **What is forbidden** | Lower layers calling higher layers directly |
| **What breaks** | Layer independence, testability, circular dependencies |

### 3.4 Error Propagation Boundaries

**Definition:** Rules about how errors are represented, communicated, and handled across boundaries.

```
ERROR PROPAGATION STRATEGIES:

Strategy A: Return Codes        Strategy B: Output Parameter
+----------------------+        +----------------------+
| int result;          |        | error_t err;         |
| result = do_work();  |        | data_t data;         |
| if (result < 0) {    |        | err = do_work(&data);|
|     handle_error();  |        | if (err != OK) {     |
| }                    |        |     handle_error();  |
+----------------------+        | }                    |
                                +----------------------+

Strategy C: Thread-Local        Strategy D: Error Context
+----------------------+        +----------------------+
| do_work();           |        | ctx_t ctx;           |
| if (errno != 0) {    |        | ctx_init(&ctx);      |
|     handle_error();  |        | do_work(&ctx);       |
| }                    |        | if (ctx_has_error()) |
+----------------------+        +----------------------+
```

| Contract Element | Rule |
|------------------|------|
| **What is enforced** | Consistent error representation within a boundary |
| **What is allowed** | Defined error codes, documented failure modes |
| **What is forbidden** | Silent failures, ambiguous return values |
| **What breaks** | Error handling, debugging, system reliability |

### 3.5 Configuration / Policy Boundaries

**Definition:** Separation between what the system does (policy) and how it does it (mechanism).

```
POLICY vs MECHANISM:

+----------------------------------+
|          POLICY LAYER            |
|                                  |
|  max_connections = 100           |
|  timeout_ms = 5000               |
|  retry_count = 3                 |
|                                  |
+----------------------------------+
           |         |
           |  passes |  configuration
           v         v
+----------------------------------+
|         MECHANISM LAYER          |
|                                  |
|  connection_pool_create(config)  |
|  // Uses config.max_connections  |
|  // Knows nothing about "100"    |
|                                  |
+----------------------------------+
```

**说明:**
- 策略层决定具体的数值、限制和行为选择
- 机制层只提供能力，不决定如何使用这些能力
- 这允许在不修改机制代码的情况下改变系统行为

| Contract Element | Rule |
|------------------|------|
| **What is enforced** | Mechanism code accepts configuration, doesn't define it |
| **What is allowed** | Passing policy as parameters or configuration structs |
| **What is forbidden** | Hardcoded policy in mechanism code |
| **What breaks** | Flexibility, testability, deployment options |

### 3.6 Visibility / Symbol Boundaries

**Definition:** Rules about which symbols are visible to which parts of the codebase.

```
SYMBOL VISIBILITY LEVELS:

+--------------------------------------------------+
|                   COMPILATION UNIT               |
|                                                  |
|  static void internal_helper(void);    <-- FILE SCOPE
|  static int file_local_counter;             ONLY
|                                                  |
+--------------------------------------------------+
                        |
                        v
+--------------------------------------------------+
|                   MODULE                         |
|                                                  |
|  // In module_internal.h                         |
|  void module_internal_func(void);      <-- MODULE SCOPE
|                                                  |
+--------------------------------------------------+
                        |
                        v
+--------------------------------------------------+
|                   PUBLIC API                     |
|                                                  |
|  // In module.h                                  |
|  void module_public_func(void);        <-- PUBLIC SCOPE
|                                                  |
+--------------------------------------------------+
```

**说明:**
- 文件作用域 (File Scope): 使用 static 关键字，只在单个 .c 文件内可见
- 模块作用域 (Module Scope): 通过内部头文件暴露给模块内的其他文件
- 公共作用域 (Public Scope): 通过公共头文件暴露给模块外部

| Contract Element | Rule |
|------------------|------|
| **What is enforced** | Symbol visibility matches intended scope |
| **What is allowed** | Public symbols in public headers, internal elsewhere |
| **What is forbidden** | Internal symbols in public headers |
| **What breaks** | ABI stability, encapsulation, upgrade paths |

---

## Step 4 — Contracts as C Interfaces

### 4.1 Header Files as Contracts

The public header is the contract. Everything in it is a promise.

```c
/* connection_pool.h - Public API Contract
 *
 * Thread Safety: All functions are thread-safe unless noted.
 * Memory: Caller owns config, pool owns internal state.
 * Errors: All functions return negative errno on failure.
 */

#ifndef CONNECTION_POOL_H
#define CONNECTION_POOL_H

#include <stddef.h>

/* Opaque handle - internal structure hidden */
typedef struct conn_pool conn_pool_t;

/* Configuration - caller provides, pool copies */
typedef struct {
    const char *host;
    int port;
    size_t max_connections;
    int timeout_ms;
} conn_pool_config_t;

/*
 * Create a connection pool.
 *
 * @param config  Pool configuration (copied, caller retains ownership)
 * @return        Pool handle on success, NULL on failure (sets errno)
 *
 * INVARIANT: config->max_connections must be > 0
 * INVARIANT: config->host must not be NULL
 */
conn_pool_t *conn_pool_create(const conn_pool_config_t *config);

/*
 * Destroy a connection pool.
 *
 * @param pool  Pool handle (may be NULL, no-op if so)
 *
 * POSTCONDITION: All connections closed, all memory freed
 */
void conn_pool_destroy(conn_pool_t *pool);

/*
 * Acquire a connection from the pool.
 *
 * @param pool     Pool handle
 * @param timeout  Max wait time in milliseconds (-1 for infinite)
 * @return         Connection handle, or NULL on timeout/error
 *
 * CONTRACT: Caller MUST call conn_pool_release() on returned connection
 */
struct connection *conn_pool_acquire(conn_pool_t *pool, int timeout);

/*
 * Release a connection back to the pool.
 *
 * @param pool  Pool handle
 * @param conn  Connection to release (must have been acquired from this pool)
 */
void conn_pool_release(conn_pool_t *pool, struct connection *conn);

#endif /* CONNECTION_POOL_H */
```

### 4.2 Opaque Structs for Encapsulation

```c
/* In public header: connection_pool.h */
typedef struct conn_pool conn_pool_t;  /* Opaque - size unknown to clients */

/* In private header: connection_pool_internal.h */
#include "connection_pool.h"
#include "list.h"
#include "mutex.h"

struct conn_pool {
    char *host;                    /* Owned copy of hostname */
    int port;
    size_t max_connections;
    size_t active_connections;
    list_t *free_list;             /* Available connections */
    list_t *busy_list;             /* In-use connections */
    mutex_t lock;                  /* Protects all fields */
    cond_t available;              /* Signaled when connection freed */
};

/* Internal functions - not in public header */
static int conn_pool_grow(conn_pool_t *pool);
static void conn_pool_shrink(conn_pool_t *pool);
```

**Why This Works:**

1. Clients cannot access `pool->active_connections` — they don't know it exists
2. Internal structure can change without breaking clients
3. Clients cannot stack-allocate the struct — enforces use of constructor

### 4.3 Function Signatures as Contracts

```c
/*
 * Function signature components as contract elements:
 *
 * Return type    : What the function produces
 * Parameters     : What the function requires
 * const          : What the function will not modify
 * restrict       : Aliasing guarantees
 * Naming         : Module membership and semantics
 */

/* Example: Each element carries meaning */

int                           /* Returns error code (0 = success) */
db_table_insert(              /* Module: db, Component: table, Action: insert */
    db_table_t *table,        /* Mutable: will be modified */
    const db_row_t *row,      /* Immutable: row data not modified */
    db_txn_t *txn             /* Mutable: transaction state updated */
);

/*
 * Contrast with ambiguous signature:
 */
void *process(void *data, int flags);  /* BAD: What does it do? Own? Return? */
```

### 4.4 Naming Conventions as Boundary Markers

```c
/*
 * Naming Convention System:
 *
 * module_component_action     Public API
 * module__internal_func       Module-internal (double underscore)
 * _module_private             File-private (leading underscore, or use static)
 */

/* Public API - clients may call these */
int db_conn_open(const char *path, db_conn_t **conn);
int db_conn_close(db_conn_t *conn);
int db_conn_execute(db_conn_t *conn, const char *sql);

/* Module Internal - other db_*.c files may call, clients must not */
int db__parse_connection_string(const char *str, db_conn_params_t *params);
int db__validate_schema(db_conn_t *conn);

/* File Private - only this .c file */
static int validate_path(const char *path);
static int open_database_file(const char *path, int flags);
```

### 4.5 Documentation Invariants

```c
/*
 * buffer.h - Growable byte buffer
 *
 * INVARIANTS (always true after any public function returns):
 *   1. buf->data is either NULL (empty) or points to valid allocation
 *   2. buf->len <= buf->cap
 *   3. If buf->data != NULL, then buf->cap > 0
 *   4. buf->data[0..len-1] contains valid data
 *   5. buf->data[len..cap-1] is allocated but undefined
 *
 * THREAD SAFETY:
 *   - Single buffer instance is NOT thread-safe
 *   - Different buffer instances may be used concurrently
 *
 * MEMORY:
 *   - buffer_init() does not allocate
 *   - buffer_reserve()/buffer_append() may allocate
 *   - buffer_free() releases all memory
 *   - After buffer_free(), buffer may be reused with buffer_init()
 */

typedef struct {
    uint8_t *data;
    size_t len;
    size_t cap;
} buffer_t;

/* Initialize buffer (no allocation) */
void buffer_init(buffer_t *buf);

/* Ensure capacity for at least `needed` more bytes */
int buffer_reserve(buffer_t *buf, size_t needed);

/* Append data to buffer (may reallocate) */
int buffer_append(buffer_t *buf, const void *data, size_t len);

/* Release all memory */
void buffer_free(buffer_t *buf);
```

---

## Step 5 — Allowed Interaction Patterns Between Layers

### 5.1 Direct Downward Calls

The simplest and most common pattern: higher layers call lower layers.

```c
/* Application layer calls Domain layer */

#include "user_service.h"  /* Domain layer API */

int main(int argc, char **argv) {
    config_t cfg;
    parse_args(argc, argv, &cfg);
    
    user_service_t *svc = user_service_create(&cfg);
    if (!svc) {
        return EXIT_FAILURE;
    }
    
    /* Direct downward call - Application -> Domain */
    int result = user_service_process_request(svc, &cfg.request);
    
    user_service_destroy(svc);
    return result;
}
```

**Rules:**
- Higher layer includes lower layer header
- Higher layer calls lower layer functions
- Return values flow back up
- No lower layer changes required

### 5.2 Dependency Injection via Function Pointers

When a lower layer needs behavior from above without depending on it:

```c
/* Core layer: storage_engine.h */

/* Storage operations abstraction */
typedef struct {
    int (*read)(void *ctx, uint64_t offset, void *buf, size_t len);
    int (*write)(void *ctx, uint64_t offset, const void *buf, size_t len);
    int (*sync)(void *ctx);
    void *ctx;  /* Opaque context for implementation */
} storage_ops_t;

/* Storage engine accepts operations, doesn't implement them */
typedef struct storage_engine storage_engine_t;

storage_engine_t *storage_engine_create(const storage_ops_t *ops);
void storage_engine_destroy(storage_engine_t *engine);
int storage_engine_put(storage_engine_t *engine, const void *key, 
                       size_t key_len, const void *val, size_t val_len);

/* ------------------------------------------------------------- */

/* Infrastructure layer: file_storage.c */
#include "storage_engine.h"

static int file_read(void *ctx, uint64_t offset, void *buf, size_t len) {
    int fd = *(int *)ctx;
    return pread(fd, buf, len, offset);
}

static int file_write(void *ctx, uint64_t offset, const void *buf, size_t len) {
    int fd = *(int *)ctx;
    return pwrite(fd, buf, len, offset);
}

storage_ops_t file_storage_ops(int *fd) {
    return (storage_ops_t){
        .read = file_read,
        .write = file_write,
        .sync = file_sync,
        .ctx = fd
    };
}

/* ------------------------------------------------------------- */

/* Application layer: main.c */
#include "storage_engine.h"
#include "file_storage.h"

int main(void) {
    int fd = open("data.db", O_RDWR | O_CREAT, 0644);
    storage_ops_t ops = file_storage_ops(&fd);
    
    /* Inject implementation into core */
    storage_engine_t *engine = storage_engine_create(&ops);
    
    /* ... use engine ... */
}
```

```
DEPENDENCY INJECTION FLOW:

+---------------------+
|   Application       |
|   (main.c)          |
|                     |
|  1. Create file_ops |
|  2. Pass to engine  |
+---------+-----------+
          |
          | creates & injects ops
          v
+---------+-----------+     +---------------------+
|   Core              |     |   Infrastructure    |
|   (storage_engine)  |     |   (file_storage)    |
|                     |     |                     |
|  Calls ops->read()  |---->|  file_read()        |
|  Calls ops->write() |---->|  file_write()       |
|                     |     |                     |
+---------------------+     +---------------------+

Note: Core does NOT #include file_storage.h
      Core only knows about storage_ops_t interface
```

**说明:**
- 核心层定义接口 (storage_ops_t)，不实现它
- 基础设施层提供具体实现 (file_read, file_write)
- 应用层组装：创建实现并注入核心层
- 这样核心层不依赖任何具体的存储实现

**Rules:**
- Interface defined in lower layer header
- Implementation provided by infrastructure or injected
- Lower layer never includes higher layer headers

### 5.3 Callbacks Without Layer Inversion

When lower layers need to notify higher layers of events:

```c
/* Core layer: event_loop.h */

typedef void (*event_callback_t)(int fd, int events, void *user_data);

typedef struct event_loop event_loop_t;

event_loop_t *event_loop_create(void);
void event_loop_destroy(event_loop_t *loop);

/* Register callback - higher layer provides function */
int event_loop_register(event_loop_t *loop, int fd, int events,
                        event_callback_t callback, void *user_data);

/* Run loop - will call registered callbacks */
int event_loop_run(event_loop_t *loop);

/* ------------------------------------------------------------- */

/* Domain layer: connection_handler.c */
#include "event_loop.h"

static void on_readable(int fd, int events, void *user_data) {
    connection_t *conn = user_data;
    handle_incoming_data(conn);
}

void connection_handler_register(connection_t *conn, event_loop_t *loop) {
    /* Higher layer registers itself with lower layer */
    event_loop_register(loop, conn->fd, EVENT_READ, on_readable, conn);
}
```

```
CALLBACK REGISTRATION FLOW:

Time -->

1. Domain creates callback function
2. Domain calls event_loop_register()
3. Core stores callback pointer
4. Core calls callback when event occurs
5. Execution temporarily enters Domain, then returns to Core

+-----------+                     +-----------+
|  Domain   |                     |  Core     |
|           |                     |           |
|  on_readable()                  |           |
|      ^                          |           |
|      |                          |           |
|      +---- register(on_readable)|           |
|      |            |             |           |
|      |            +------------>| store cb  |
|      |                          |           |
|      |                          | ...later..|
|      |                          |           |
|      +--------------------------+ call cb   |
|           return                |           |
|                                 |           |
+-----------+                     +-----------+
```

**说明:**
- 领域层定义回调函数 (on_readable)
- 领域层主动向核心层注册回调
- 核心层存储回调指针，在适当时机调用
- 虽然核心层调用了领域层代码，但不依赖领域层——它只知道函数指针

**Rules:**
- Lower layer defines callback signature
- Higher layer provides callback implementation
- Higher layer initiates registration
- No #include of higher layer in lower layer

### 5.4 Data Passed Across Layers Safely

```c
/* Safe data transfer patterns */

/* Pattern 1: Value Copy (small structs) */
typedef struct {
    int32_t x, y;
} point_t;

point_t transform_point(point_t p);  /* Pass and return by value */

/* Pattern 2: Immutable Reference */
int process_data(const buffer_t *buf);  /* Read-only access */

/* Pattern 3: Explicit Ownership Transfer */
/*
 * Caller allocates, callee takes ownership.
 * Documentation must be explicit.
 */
int queue_push(queue_t *q, item_t *item);  /* q now owns item */

/* Pattern 4: Output Parameter with Caller Allocation */
int fetch_record(db_t *db, const char *key, 
                 record_t *out,      /* Caller provides storage */
                 size_t out_size);   /* Caller specifies capacity */

/* Pattern 5: Factory with Clear Ownership */
/*
 * Callee allocates, returns ownership to caller.
 * Paired with destructor.
 */
record_t *record_create(void);
void record_destroy(record_t *rec);
```

**Cross-Layer Data Rules:**

| Pattern | Who Allocates | Who Frees | When to Use |
|---------|---------------|-----------|-------------|
| Value copy | Stack | Automatic | Small, fixed-size data |
| Immutable ref | Caller | Caller | Read-only access to large data |
| Ownership transfer | Caller or Callee | Receiver | Hand-off semantics |
| Output parameter | Caller | Caller | Caller controls memory |
| Factory | Callee | Caller (via destructor) | Complex object creation |

---

## Step 6 — Forbidden Interactions (Anti-Patterns)

### 6.1 Upward Dependencies

**Violation:** Lower layer includes or calls higher layer.

```c
/* BAD: Core layer depending on Application layer */

/* core/database.c */
#include "config.h"         /* VIOLATION: Application layer header */
#include "cli.h"            /* VIOLATION: Application layer header */

int db_open(const char *path) {
    /* VIOLATION: Core layer reading application configuration */
    int cache_size = get_app_config()->cache_mb * 1024 * 1024;
    
    /* VIOLATION: Core layer calling UI function */
    cli_print_status("Opening database...");
    
    return do_open(path, cache_size);
}
```

**Why Harmful:**
- Core cannot be used without Application layer
- Creates circular dependency risk
- Cannot test Core in isolation
- Changes to Application may break Core

**Correct Approach:**

```c
/* GOOD: Core receives what it needs as parameters */

/* core/database.c */

int db_open(const char *path, const db_options_t *opts) {
    /* Core uses provided options, doesn't fetch them */
    int cache_size = opts->cache_size;
    
    /* Core returns status, doesn't print */
    return do_open(path, cache_size);
}

/* Application layer: */
int main(void) {
    db_options_t opts = {
        .cache_size = get_app_config()->cache_mb * 1024 * 1024
    };
    cli_print_status("Opening database...");
    return db_open("data.db", &opts);
}
```

### 6.2 Leaking Internal Structs

**Violation:** Public header exposes internal implementation details.

```c
/* BAD: Internal details in public header */

/* hash_table.h (public) */
typedef struct {
    size_t size;
    size_t capacity;
    float load_factor;          /* Exposes implementation detail */
    struct bucket *buckets;     /* Clients can access internals */
    int (*hash_fn)(const void *);
    int resize_threshold;       /* Exposes tuning parameter */
} hash_table_t;

hash_table_t *ht_create(void);

/* Clients can now do: */
void bad_client_code(hash_table_t *ht) {
    /* Direct access to internals - bypasses API */
    for (size_t i = 0; i < ht->capacity; i++) {
        if (ht->buckets[i].occupied) {
            /* Walking internal structure */
        }
    }
    
    /* Modifying internal state */
    ht->load_factor = 0.99;  /* Breaks invariants */
}
```

**Why Harmful:**
- Cannot change internal structure without breaking clients
- Clients may corrupt internal state
- No encapsulation — no ability to enforce invariants

**Correct Approach:**

```c
/* GOOD: Opaque type in public header */

/* hash_table.h (public) */
typedef struct hash_table hash_table_t;  /* Opaque */

hash_table_t *ht_create(void);
void ht_destroy(hash_table_t *ht);
int ht_insert(hash_table_t *ht, const void *key, const void *value);
void *ht_lookup(hash_table_t *ht, const void *key);

/* Iteration via callback, not internal access */
typedef int (*ht_iterator_fn)(const void *key, const void *value, void *ctx);
void ht_foreach(hash_table_t *ht, ht_iterator_fn fn, void *ctx);

/* hash_table_internal.h (module-private) */
struct hash_table {
    size_t size;
    size_t capacity;
    float load_factor;
    struct bucket *buckets;
    /* ... */
};
```

### 6.3 Shared Global State

**Violation:** Multiple modules share mutable global variables.

```c
/* BAD: Global state shared across modules */

/* globals.h */
extern int g_debug_level;
extern FILE *g_log_file;
extern config_t g_config;
extern stats_t g_stats;

/* module_a.c */
#include "globals.h"
void module_a_work(void) {
    if (g_debug_level > 2) {
        fprintf(g_log_file, "Working...\n");
    }
    g_stats.operations++;
}

/* module_b.c */
#include "globals.h"
void module_b_process(void) {
    if (g_config.feature_enabled) {
        /* Uses same global state */
        g_stats.operations++;
    }
}

/* Problems:
 * - Order of initialization undefined
 * - Race conditions in multithreaded code
 * - Cannot instantiate multiple instances
 * - Hidden dependencies between modules
 * - Testing requires global setup/teardown
 */
```

**Correct Approach:**

```c
/* GOOD: Explicit context passing */

/* context.h */
typedef struct {
    int debug_level;
    FILE *log_file;
    config_t config;
    stats_t stats;
} context_t;

context_t *context_create(void);
void context_destroy(context_t *ctx);

/* module_a.c */
void module_a_work(context_t *ctx) {
    if (ctx->debug_level > 2) {
        fprintf(ctx->log_file, "Working...\n");
    }
    ctx->stats.operations++;
}

/* module_b.c */
void module_b_process(context_t *ctx) {
    if (ctx->config.feature_enabled) {
        ctx->stats.operations++;
    }
}

/* Benefits:
 * - Explicit dependencies
 * - Multiple instances possible
 * - Thread-safe by design
 * - Easy to test
 */
```

### 6.4 Policy Decisions in Low Layers

**Violation:** Core/Mechanism layer makes policy decisions.

```c
/* BAD: Policy hardcoded in mechanism layer */

/* core/connection_pool.c */

#define MAX_CONNECTIONS 100     /* POLICY in mechanism! */
#define CONNECTION_TIMEOUT 5000 /* POLICY in mechanism! */

conn_pool_t *conn_pool_create(const char *host, int port) {
    conn_pool_t *pool = malloc(sizeof(*pool));
    
    /* Hardcoded policy decisions */
    pool->max = MAX_CONNECTIONS;
    pool->timeout = CONNECTION_TIMEOUT;
    
    /* Hardcoded retry policy */
    for (int i = 0; i < 3; i++) {  /* Magic number policy */
        if (connect_to_host(pool, host, port) == 0) {
            return pool;
        }
        sleep(1);  /* Hardcoded backoff */
    }
    
    /* Hardcoded logging policy */
    fprintf(stderr, "Failed to connect\n");  /* Where to log = policy */
    
    return NULL;
}
```

**Why Harmful:**
- Cannot adjust behavior without modifying mechanism code
- Different deployments need different settings
- Testing requires actual waits
- Changes to defaults require recompilation

**Correct Approach:**

```c
/* GOOD: Policy passed in from above */

/* core/connection_pool.h */
typedef struct {
    size_t max_connections;
    int timeout_ms;
    int retry_count;
    int retry_delay_ms;
    void (*on_error)(const char *msg, void *ctx);
    void *error_ctx;
} conn_pool_config_t;

conn_pool_t *conn_pool_create(const char *host, int port,
                               const conn_pool_config_t *config);

/* Application layer sets policy: */
int main(void) {
    conn_pool_config_t config = {
        .max_connections = 100,  /* Policy here */
        .timeout_ms = 5000,      /* Policy here */
        .retry_count = 3,        /* Policy here */
        .retry_delay_ms = 1000,  /* Policy here */
        .on_error = log_error,   /* Policy here */
        .error_ctx = &logger
    };
    
    conn_pool_t *pool = conn_pool_create("db.example.com", 5432, &config);
}
```

### 6.5 Type Punning Across Boundaries

**Violation:** Casting to internal types across module boundaries.

```c
/* BAD: Client casts to internal type */

/* In client code */
#include "queue.h"

/* Client has access to internal header somehow */
#include "queue_internal.h"

void bad_optimization(queue_t *q) {
    /* Cast to internal type */
    struct queue_impl *impl = (struct queue_impl *)q;
    
    /* Directly access internals */
    if (impl->head == impl->tail) {
        /* "Optimized" empty check */
        return;
    }
    
    /* Corrupt internal state */
    impl->size = 0;  /* Force empty without dequeue */
}
```

**Why Harmful:**
- Breaks when internal structure changes
- May violate internal invariants
- Undefined behavior if padding/alignment differs
- Creates implicit, undocumented dependencies

---

## Step 7 — Contracts Over Time (Evolution)

### How Boundaries Help System Evolution

```
WITHOUT BOUNDARIES:                   WITH BOUNDARIES:
                                     
+-----------------------------+       +----------+  +----------+
|                             |       | Module A |  | Module B |
|  Interconnected spaghetti   |       +----+-----+  +----+-----+
|  Any change affects all     |            |             |
|                             |            v             v
+-----------------------------+       +----+-------------+----+
                                      |    Stable Core API   |
                                      +-----------------------+
                                      
Change impact: UNKNOWN                Change impact: LOCALIZED
```

**说明:**
- 无边界系统: 任何修改都可能影响整个系统，变更影响无法预测
- 有边界系统: 变更被限制在模块内部，只要 API 不变，其他模块不受影响

### Feature Addition

With clear boundaries:

```c
/* Adding a new feature: connection pooling timeout callback */

/* OLD API (core/connection_pool.h): */
conn_pool_t *conn_pool_create(const conn_pool_config_t *config);
struct connection *conn_pool_acquire(conn_pool_t *pool, int timeout);

/* NEW API - backwards compatible addition: */
/* Add callback to config struct (at end, for ABI compatibility) */
typedef struct {
    size_t max_connections;
    int timeout_ms;
    /* NEW: Optional callback when acquire times out */
    void (*on_timeout)(void *ctx);  /* New field at end */
    void *timeout_ctx;               /* New field at end */
} conn_pool_config_t;

/* Contract preserved:
 * - Existing code works (new fields zero-initialized)
 * - New code can use new feature
 * - Core implementation changes, clients don't recompile
 */
```

### Performance Optimization

Boundaries allow internal changes without affecting clients:

```c
/* Optimization: Replace linear search with hash lookup */

/* PUBLIC API UNCHANGED: */
void *cache_get(cache_t *cache, const char *key);

/* INTERNAL CHANGE ONLY: */

/* Before (cache_internal.h): */
struct cache {
    struct entry *entries;  /* Linear array */
    size_t count;
};

/* After (cache_internal.h): */
struct cache {
    struct hash_table *index;  /* Hash table */
    struct entry *entries;     /* Still array for iteration */
    size_t count;
};

/* cache_get() implementation changes, signature doesn't */
```

### Refactoring

Boundaries define what can change and what cannot:

```
SAFE REFACTORING ZONES:

+--------------------------------------------------+
|                    PUBLIC API                     |
|           (Cannot change without notice)          |
|                                                   |
|   int module_func(const input_t *in, output_t *out);
|                                                   |
+--------------------------------------------------+
                        |
                        | boundary
                        v
+--------------------------------------------------+
|                  IMPLEMENTATION                   |
|              (Safe to refactor freely)            |
|                                                   |
|   - Change algorithms                             |
|   - Reorganize internal functions                 |
|   - Modify internal data structures               |
|   - Split/merge internal files                    |
|                                                   |
+--------------------------------------------------+
```

### Team Changes

When teams change, boundaries provide:

| Benefit | How |
|---------|-----|
| **Onboarding** | New developers understand one module at a time |
| **Handoff** | Module ownership can transfer without system knowledge |
| **Code review** | Reviewers check boundary compliance, not full system |
| **Documentation** | API documentation captures contracts, not entire system |

### When Contracts Are Vague or Undocumented

```c
/* VAGUE CONTRACT: */
int process(void *data, int flags);  /* What is data? What flags? */

/* Consequences:
 * 1. Different callers make different assumptions
 * 2. Implementation changes break unknown callers  
 * 3. Debugging becomes archaeology
 * 4. Testing is incomplete (unknown edge cases)
 * 5. Paralyzed development (fear of breaking things)
 */

/* CLEAR CONTRACT: */
/*
 * Process a data record.
 *
 * @param record  Pointer to record_t (not NULL)
 * @param flags   Bitwise OR of PROCESS_* flags
 *                - PROCESS_VALIDATE: Check constraints before processing
 *                - PROCESS_ASYNC: Return immediately, complete in background
 *
 * @return 0 on success
 *         -EINVAL if record is NULL or flags are invalid
 *         -EAGAIN if PROCESS_ASYNC and queue is full
 *
 * Thread safety: Safe to call from multiple threads.
 * Ownership: record is borrowed, caller retains ownership.
 */
int process_record(const record_t *record, int flags);
```

---

## Step 8 — Mapping Boundaries to Real Systems

### Case Study: SQLite Architecture

SQLite is an exemplary C codebase with clear boundaries despite being a single-file amalgamation for distribution.

```
SQLITE ARCHITECTURE:

+---------------------------------------------------------------+
|                     APPLICATION LAYER                          |
|                                                                |
|   sqlite3_open(), sqlite3_exec(), sqlite3_close()              |
|   (Public API - stable, documented, versioned)                 |
+---------------------------------------------------------------+
                              |
                              v
+---------------------------------------------------------------+
|                      SQL COMPILER                              |
|                                                                |
|   Tokenizer -> Parser -> Code Generator                        |
|   (Transforms SQL text to bytecode)                            |
+---------------------------------------------------------------+
                              |
                              v
+---------------------------------------------------------------+
|                   VIRTUAL MACHINE (VDBE)                       |
|                                                                |
|   Bytecode interpreter                                         |
|   (Pure mechanism - knows nothing about SQL)                   |
+---------------------------------------------------------------+
                              |
                              v
+---------------------------------------------------------------+
|                      B-TREE ENGINE                             |
|                                                                |
|   btree.c - On-disk B-tree implementation                      |
|   (Pure data structure - knows nothing about VDBE)             |
+---------------------------------------------------------------+
                              |
                              v
+---------------------------------------------------------------+
|                         PAGER                                  |
|                                                                |
|   Page cache, transaction management, crash recovery           |
|   (Manages pages - knows nothing about B-trees)                |
+---------------------------------------------------------------+
                              |
                              v
+---------------------------------------------------------------+
|                       OS INTERFACE                             |
|                                                                |
|   VFS (Virtual File System) abstraction                        |
|   (Platform-specific I/O - replaceable)                        |
+---------------------------------------------------------------+
```

**说明:**
- 应用层: 公共 API，向后兼容，版本控制
- SQL 编译器: 将 SQL 文本转换为字节码，不关心如何执行
- 虚拟机: 执行字节码，不关心 SQL 语法
- B-树引擎: 纯数据结构实现，不关心虚拟机
- 页面管理器: 管理磁盘页面，不关心 B-树
- OS 接口: 平台抽象，可替换实现

### SQLite Key Contracts

#### 1. VFS Contract (OS Abstraction)

```c
/* sqlite3.h - VFS interface contract */
struct sqlite3_vfs {
    int iVersion;                    /* Structure version */
    int szOsFile;                    /* Size of subclassed sqlite3_file */
    int mxPathname;                  /* Maximum pathname length */
    
    /* Methods - platform provides implementations */
    int (*xOpen)(sqlite3_vfs*, const char *zName, sqlite3_file*,
                 int flags, int *pOutFlags);
    int (*xDelete)(sqlite3_vfs*, const char *zName, int syncDir);
    int (*xAccess)(sqlite3_vfs*, const char *zName, int flags, int *pResOut);
    /* ... more methods ... */
};

/* Contract:
 * - Pager calls VFS methods, never OS directly
 * - VFS implementations are pluggable at runtime
 * - Different platforms provide different VFS implementations
 * - Custom VFS possible for encryption, testing, etc.
 */
```

#### 2. B-Tree/Pager Contract

```c
/* The B-tree module never does I/O directly.
 * It requests pages from the pager.
 */

/* pager.h contract: */
int sqlite3PagerGet(Pager *pPager, Pgno pgno, DbPage **ppPage);
int sqlite3PagerWrite(DbPage *pPg);
int sqlite3PagerCommit(Pager *pPager);

/* Invariants:
 * - B-tree requests pages by number
 * - Pager handles caching, locking, journaling
 * - B-tree never sees file handles
 * - Transaction boundaries controlled by higher layer
 */
```

#### 3. Public API Stability Contract

SQLite's public API demonstrates exemplary boundary maintenance:

| Contract Element | SQLite Practice |
|------------------|-----------------|
| **ABI stability** | Stable since 2004, structures padded for future fields |
| **Deprecation** | Old interfaces kept working indefinitely |
| **Versioning** | `SQLITE_VERSION_NUMBER` for conditional compilation |
| **Documentation** | Every public function fully documented with contracts |

### Where SQLite Is Strict vs Pragmatic

| Aspect | Approach | Rationale |
|--------|----------|-----------|
| **Public API** | STRICT | Millions of applications depend on stability |
| **Internal layers** | STRICT | Clear separation enables single-developer maintenance |
| **Code organization** | PRAGMATIC | Single amalgamation file for distribution |
| **Error handling** | STRICT | Every allocation checked, every error propagated |
| **Memory ownership** | STRICT | Clear alloc/free pairs, documented transfer |

### Case Study: Redis Architecture

```
REDIS ARCHITECTURE:

+---------------------------------------------------------------+
|                     NETWORKING LAYER                           |
|                                                                |
|   networking.c - Client connections, protocol parsing          |
|   (Handles I/O, knows nothing about data structures)           |
+---------------------------------------------------------------+
                              |
                              v
+---------------------------------------------------------------+
|                    COMMAND DISPATCH                            |
|                                                                |
|   server.c - Command table, execution                          |
|   (Routes commands, enforces ACL)                              |
+---------------------------------------------------------------+
                              |
                              v
+---------------------------------------------------------------+
|                    DATA STRUCTURES                             |
|                                                                |
|   t_string.c, t_list.c, t_hash.c, t_set.c, t_zset.c           |
|   (Pure data operations - know nothing about networking)       |
+---------------------------------------------------------------+
                              |
                              v
+---------------------------------------------------------------+
|                   ABSTRACT DATA TYPES                          |
|                                                                |
|   dict.c, adlist.c, intset.c, ziplist.c, skiplist.c           |
|   (Generic containers - know nothing about Redis commands)     |
+---------------------------------------------------------------+
                              |
                              v
+---------------------------------------------------------------+
|                     MEMORY MANAGEMENT                          |
|                                                                |
|   zmalloc.c - Allocation tracking, jemalloc wrapper           |
|   (Platform abstraction for memory)                            |
+---------------------------------------------------------------+
```

**说明:**
- 网络层: 处理连接和协议，不了解数据结构
- 命令调度: 路由命令，执行访问控制
- 数据结构层: Redis 类型的操作（字符串、列表、哈希等）
- 抽象数据类型: 通用容器，与 Redis 概念无关
- 内存管理: 封装分配器，提供追踪

### Redis Key Contracts

```c
/* Redis command signature contract */
typedef void redisCommandProc(client *c);

/* Every command follows this pattern:
 * 1. Read arguments from c->argv
 * 2. Validate inputs
 * 3. Perform operation
 * 4. Write response via addReply*()
 */

/* Data structure independence:
 * - t_hash.c doesn't know about networking
 * - dict.c doesn't know about Redis types
 * - Each layer testable in isolation
 */
```

---

## Step 9 — Boundary Review Checklist

### Checklist for Code Review

Use this checklist when reviewing code changes to evaluate boundary health:

#### A. Dependency Direction

| Check | Pass | Fail |
|-------|------|------|
| All #includes point to same or lower layer? | ✓ | ✗ Lower layer including higher layer header |
| No function calls to higher layer? | ✓ | ✗ Core calling Application functions |
| Callbacks registered by higher layer, defined in lower? | ✓ | ✗ Lower layer defines callback that references higher layer types |

#### B. API Boundary Integrity

| Check | Pass | Fail |
|-------|------|------|
| Public header contains only public interface? | ✓ | ✗ Internal struct definitions leaked |
| Opaque types used for internal state? | ✓ | ✗ Full struct definition in public header |
| Public functions documented with contracts? | ✓ | ✗ Parameters undocumented, invariants unclear |
| Error returns documented? | ✓ | ✗ Function can fail but return values undefined |

#### C. Data Ownership

| Check | Pass | Fail |
|-------|------|------|
| Ownership clear at every interface? | ✓ | ✗ Ambiguous who frees returned pointer |
| Borrowed references not stored? | ✓ | ✗ Function stores pointer without documented lifetime |
| Deep copy when crossing boundary? | ✓ | ✗ Shared mutable reference across layers |
| Allocation/free pairs matched? | ✓ | ✗ Allocator mismatch (malloc vs custom) |

#### D. Configuration and Policy

| Check | Pass | Fail |
|-------|------|------|
| Policy decisions in application layer? | ✓ | ✗ Hardcoded limits in core |
| Magic numbers passed as parameters? | ✓ | ✗ `#define TIMEOUT 5000` in mechanism code |
| Logging/output controlled by callbacks? | ✓ | ✗ Core layer writes to stderr |

#### E. Symbol Visibility

| Check | Pass | Fail |
|-------|------|------|
| Helper functions static? | ✓ | ✗ Internal helper visible across compilation units |
| Naming convention followed? | ✓ | ✗ Public function without module prefix |
| Internal headers not installed? | ✓ | ✗ `*_internal.h` in public include path |

#### F. Error Handling Consistency

| Check | Pass | Fail |
|-------|------|------|
| Error representation consistent in module? | ✓ | ✗ Mix of errno, return codes, and exceptions |
| Errors propagated, not swallowed? | ✓ | ✗ Function catches error and continues |
| Cleanup on error path? | ✓ | ✗ Memory/resource leak on failure |

### Architecture Health Questions

When reviewing architecture (not just code), ask:

1. **Can I test this module in isolation?**
   - If no: boundary violation or hidden dependency

2. **Can I explain what this module does without explaining others?**
   - If no: responsibilities are tangled

3. **If I change the internal implementation, what else must change?**
   - If more than the module itself: leaky abstraction

4. **Can a new team member understand this module first?**
   - If no: dependency graph is wrong

5. **Where would I add a new feature of type X?**
   - If unclear: boundaries don't match domain

### Red Flags in Code Review

| Red Flag | Likely Problem |
|----------|----------------|
| `#include "../other_module/internal.h"` | Layer violation |
| Casting `void *` to internal struct type | Breaking encapsulation |
| Global variable accessed from multiple modules | Shared mutable state |
| Function with 10+ parameters | Missing abstraction |
| `extern` declarations in .c files | Bypassing headers |
| `#ifdef` for business logic variations | Policy in wrong layer |
| Comments saying "don't call this directly" | Missing enforcement |

### Architecture Decay Warning Signs

Over time, watch for:

```
HEALTHY ARCHITECTURE:           DECAYING ARCHITECTURE:

  +-----+                         +-----+
  |  A  |                         |  A  |<--+
  +--+--+                         +--+--+   |
     |                               |   +--+
     v                               v   |
  +--+--+                         +--+--+|
  |  B  |                         |  B  |+
  +--+--+                         +--++-++
     |                               | X |
     v                               v/ \v
  +--+--+                         +--+--+--+
  |  C  |                         |  C  |  |
  +-----+                         +-----+--+

  Dependencies flow down          Dependencies form cycles
  Changes are localized           Changes cascade unpredictably
```

**说明:**
- 健康架构: 依赖单向向下流动，变更局部化
- 腐化架构: 依赖形成循环，变更级联传播
- 定期审查依赖图，发现违规及时修复

---

## Summary: The Five Laws of C Boundaries

1. **The Direction Law**
   > Dependencies point downward. Higher layers know about lower layers. Lower layers know nothing about higher layers.

2. **The Opacity Law**
   > Internals are invisible. If it's not in the public header, it doesn't exist to clients.

3. **The Ownership Law**
   > Every allocation has exactly one owner. Ownership transfers are explicit. Borrowed references don't outlive lenders.

4. **The Policy Law**
   > Mechanism is below. Policy is above. Mechanism never decides, only executes.

5. **The Documentation Law**
   > If the contract isn't written, it doesn't exist. Invariants, ownership, thread-safety, and error handling are documented or they're undefined.

---

## Appendix: Quick Reference

### Header File Template

```c
/* module_name.h - Brief description
 *
 * OWNERSHIP: Describe allocation/free responsibilities
 * THREAD SAFETY: Describe concurrency guarantees  
 * ERROR HANDLING: Describe error representation
 */

#ifndef MODULE_NAME_H
#define MODULE_NAME_H

#include <stddef.h>  /* Standard dependencies only */

/* Opaque types */
typedef struct module_handle module_handle_t;

/* Configuration/Input types (can be stack-allocated) */
typedef struct {
    /* fields */
} module_config_t;

/* Lifecycle */
module_handle_t *module_create(const module_config_t *config);
void module_destroy(module_handle_t *handle);

/* Operations */
int module_operation(module_handle_t *handle, /* params */);

#endif /* MODULE_NAME_H */
```

### Function Documentation Template

```c
/*
 * Brief one-line description.
 *
 * Detailed description if needed.
 *
 * @param name    Description [constraints]
 * @param output  Output parameter [caller provides storage]
 *
 * @return Description of return value
 *         - SUCCESS_VALUE: Normal completion
 *         - ERROR_VALUE: When/why this occurs
 *
 * OWNERSHIP: Who owns what after call
 * THREAD SAFETY: Safe/unsafe, requirements
 * PRECONDITIONS: What must be true before call
 * POSTCONDITIONS: What is true after successful call
 */
```

---

*This document represents architectural principles applicable to any long-lived C codebase. The patterns described are derived from production systems that have evolved over decades while maintaining clarity and modifiability.*

