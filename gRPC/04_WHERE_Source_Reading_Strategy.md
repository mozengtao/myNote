# WHERE | Source Code Reading Strategy

## Overview: Top-Down Reading Approach

```
+=============================================================================+
|                    CODE READING ROADMAP                                     |
+=============================================================================+

                    +-----------------------------------+
                    |    PHASE 1: PUBLIC API            |
                    |    (What CAN you do?)             |
                    +-----------------------------------+
                                    |
                                    | Understand the contract
                                    v
                    +-----------------------------------+
                    |    PHASE 2: grpc-c WRAPPER        |
                    |    (How to use it in C?)          |
                    +-----------------------------------+
                                    |
                                    | Trace implementation
                                    v
                    +-----------------------------------+
                    |    PHASE 3: gRPC CORE             |
                    |    (How does it REALLY work?)     |
                    +-----------------------------------+
                                    |
                                    | Deep dive
                                    v
                    +-----------------------------------+
                    |    PHASE 4: INTERNALS             |
                    |    (State machines, hot paths)    |
                    +-----------------------------------+

  Time Investment:
  
  Phase 1: 1-2 hours (skim headers)
  Phase 2: 4-8 hours (read all grpc-c code)
  Phase 3: 8-16 hours (core concepts)
  Phase 4: Ongoing (as needed)
```

**图解说明 (Diagram Explanation):**

自顶向下阅读路线图：

| 阶段 | 目标 | 时间投入 |
|------|------|----------|
| Phase 1 | 公共API - 用户能做什么 | 1-2小时 |
| Phase 2 | grpc-c包装层 - 如何在C中使用 | 4-8小时 |
| Phase 3 | gRPC Core - 运行时实际工作原理 | 8-16小时 |
| Phase 4 | 内部实现 - 状态机、热点路径 | 按需 |

---

## Phase 1: Public API (User View)

```
+=============================================================================+
|                    PUBLIC API READING ORDER                                 |
+=============================================================================+

  GOAL: Understand what a C developer is ALLOWED to do

  START HERE:
  -----------
  
  1. grpc-c/lib/h/grpc-c/grpc-c.h  (30 min)
     |
     +-- Read typedefs and forward declarations
     |   - grpc_c_client_t, grpc_c_server_t, grpc_c_context_t
     |   - Understand these are opaque handles
     |
     +-- Read callback signatures
     |   - grpc_c_client_callback_t
     |   - grpc_c_service_callback_t
     |   - grpc_c_read_resolve_callback_t
     |
     +-- Read lifecycle functions
     |   - grpc_c_init / grpc_c_shutdown
     |   - grpc_c_client_init / grpc_c_client_free
     |   - grpc_c_server_create / grpc_c_server_destroy
     |
     +-- Read RPC request functions
     |   - grpc_c_client_request_unary
     |   - grpc_c_client_request_sync
     |   - grpc_c_client_request_async
     |
     +-- Read stream handlers
         - grpc_c_stream_handler_s
         - read, write, write_done, finish

  THEN:
  -----
  
  2. grpc/include/grpc/grpc.h  (20 min)
     |
     +-- Core types: grpc_channel, grpc_call, grpc_server
     +-- Completion queue: grpc_completion_queue
     +-- Operations: grpc_op, grpc_op_type
     +-- Status codes: grpc_status_code
  
  3. grpc/include/grpc/byte_buffer.h  (10 min)
     |
     +-- grpc_byte_buffer structure
     +-- Creation and destruction
     +-- Slice management
  
  4. grpc/include/grpc/support/*  (15 min)
     |
     +-- gpr_malloc / gpr_free
     +-- gpr_mu (mutex), gpr_cv (condition variable)
     +-- gpr_timespec, gpr_time_*
     +-- gpr_log

  CHECKPOINT QUESTIONS:
  ---------------------
  
  After Phase 1, you should answer:
  
  [ ] What are the main types a user works with?
  [ ] What is the lifecycle of a client? A server?
  [ ] What are the three RPC request modes?
  [ ] What does a stream handler provide?
  [ ] How are timeouts specified?
```

**图解说明 (Diagram Explanation):**

Phase 1阅读顺序：

1. `grpc-c.h`（30分钟）→ 理解不透明句柄、回调签名、生命周期函数
2. `grpc.h`（20分钟）→ Core类型：channel, call, server, CQ
3. `byte_buffer.h`（10分钟）→ 序列化数据容器
4. `support/*`（15分钟）→ 内存分配、同步原语、时间API

**检查点问题**：主要类型？生命周期？三种请求模式？stream handler提供什么？超时如何指定？

---

## Phase 2: grpc-c Wrapper Layer

```
+=============================================================================+
|                    grpc-c SOURCE READING ORDER                              |
+=============================================================================+

  GOAL: Understand how grpc-c maps gRPC Core concepts into C APIs

  DIRECTORY STRUCTURE:
  --------------------
  
  grpc-c/
  ├── lib/
  │   ├── h/
  │   │   └── grpc-c/
  │   │       ├── grpc-c.h        # Main public header
  │   │       └── config.h        # Build configuration
  │   ├── client.c                # Client implementation (READ THIS)
  │   ├── service.c               # Server implementation (READ THIS)
  │   ├── context.c               # Context management (READ THIS)
  │   ├── stream_ops.c            # Read/write operations (READ THIS)
  │   ├── hooks.c                 # Extension points
  │   ├── memory.c                # Custom allocators
  │   ├── metadata_array.c        # Metadata handling
  │   ├── trace.c                 # Debugging/logging
  │   └── thread_pool.c           # Worker threads
  │
  └── examples/
      ├── foo.proto               # Example protobuf
      ├── foo_client.c            # Example client (READ FIRST)
      ├── foo_server.c            # Example server (READ FIRST)
      ├── bidi_streaming*.c       # Streaming examples
      └── ...

  READING ORDER:
  --------------
  
  1. examples/foo.proto (5 min)
     |
     +-- Understand the simple Greeter service
     +-- See HelloRequest and HelloReply messages
  
  2. examples/foo_client.c (15 min)
     |
     +-- Follow main() flow
     |   - grpc_c_init()
     |   - grpc_c_client_init()
     |   - foo__greeter__say_hello()  <- generated function
     |
     +-- Note: No explicit CQ handling (hidden by wrapper)
  
  3. examples/foo_server.c (20 min)
     |
     +-- Follow main() flow
     |   - grpc_c_server_create()
     |   - foo__greeter__service_init()  <- generated
     |   - grpc_c_server_start()
     |   - grpc_c_server_wait()
     |
     +-- Study callback: foo__greeter__say_hello_cb()
         - context->gcc_stream->read()
         - context->gcc_stream->write()
         - context->gcc_stream->finish()
  
  4. lib/client.c (60 min) ★ MOST IMPORTANT
     |
     +-- gc_client_create_by_host()
     |   - Channel creation
     |   - CQ creation
     |   - Thread pool integration
     |
     +-- grpc_c_client_request_unary()
     |   - gc_client_prepare_unary_ops()
     |   - grpc_call_start_batch()
     |   - grpc_completion_queue_pluck()
     |
     +-- gc_handle_client_event()
     |   - Event loop structure
     |   - Event type switching
     |
     +-- grpc_c_client_free()
         - Shutdown sequence
         - Wait for callbacks
         - Drain CQ
  
  5. lib/service.c (60 min) ★ MOST IMPORTANT
     |
     +-- gc_server_create_internal()
     |   - Server creation
     |   - Port binding
     |
     +-- grpc_c_register_method()
     |   - Method registration with gRPC Core
     |   - Callback storage
     |
     +-- grpc_c_server_start()
     |   - grpc_server_start()
     |   - Pre-register contexts for each method
     |
     +-- gc_handle_server_event_internal()
     |   - Main server event loop
     |   - Event dispatch
     |
     +-- gc_prepare_server_callback()
         - How callbacks are invoked
  
  6. lib/context.c (30 min)
     |
     +-- grpc_c_context_init()
     |   - Metadata array setup
     |   - Stream handler allocation
     |
     +-- grpc_c_context_free()
     |   - Resource cleanup order
     |   - Memory deallocation
     |
     +-- grpc_c_ops_alloc()
         - Dynamic ops array management
  
  7. lib/stream_ops.c (45 min)
     |
     +-- gc_stream_read()
     |   - RECV_MESSAGE operation
     |   - CQ pluck for result
     |   - Protobuf unpacking
     |
     +-- gc_stream_write()
     |   - SEND_INITIAL_METADATA
     |   - SEND_MESSAGE
     |   - CQ pluck for completion
     |
     +-- gc_client_stream_finish()
     +-- gc_server_stream_finish()
         - Status handling
         - Context cleanup

  KEY PATTERNS TO IDENTIFY:
  -------------------------
  
  [ ] How does grpc-c wrap grpc_call_start_batch?
  [ ] How does grpc-c manage completion queues?
  [ ] How does grpc-c handle sync vs async?
  [ ] How does grpc-c manage context lifecycle?
  [ ] How does grpc-c integrate with thread pool?
```

**图解说明 (Diagram Explanation):**

Phase 2阅读顺序（★表示最重要）：

| 优先级 | 文件 | 时间 | 关键函数 |
|--------|------|------|----------|
| 1 | `examples/foo_*.c` | 20分钟 | main流程 |
| 2 ★ | `lib/client.c` | 60分钟 | `grpc_c_client_request_unary`, `gc_handle_client_event` |
| 3 ★ | `lib/service.c` | 60分钟 | `grpc_c_register_method`, `gc_prepare_server_callback` |
| 4 | `lib/context.c` | 30分钟 | `grpc_c_context_init`, `grpc_c_ops_alloc` |
| 5 | `lib/stream_ops.c` | 45分钟 | `gc_stream_read`, `gc_stream_write` |

---

## Phase 3: gRPC Core Internals

```
+=============================================================================+
|                    gRPC CORE READING STRATEGY                               |
+=============================================================================+

  GOAL: Understand how gRPC actually executes at runtime

  NOTE: gRPC Core is in the main grpc repository
  Path: grpc/src/core/

  KEY DIRECTORIES:
  ----------------
  
  grpc/src/core/
  ├── lib/
  │   ├── surface/           # Public API implementation
  │   │   ├── call.cc        # grpc_call implementation
  │   │   ├── channel.cc     # grpc_channel implementation
  │   │   ├── server.cc      # grpc_server implementation
  │   │   └── completion_queue.cc  # CQ implementation
  │   │
  │   ├── transport/
  │   │   ├── transport.h    # Transport interface
  │   │   └── http2/         # HTTP/2 implementation
  │   │
  │   ├── iomgr/             # I/O manager
  │   │   ├── pollset.h      # Polling abstraction
  │   │   └── tcp_*.cc       # TCP implementation
  │   │
  │   └── channel/
  │       ├── channel_stack.cc  # Filter chain
  │       └── connected_channel.cc
  │
  └── ext/
      └── filters/           # Built-in filters

  READING ORDER:
  --------------
  
  1. src/core/lib/surface/completion_queue.cc (Critical!)
     |
     +-- Structure of completion_queue
     +-- grpc_completion_queue_create()
     +-- grpc_completion_queue_next() / _pluck()
     +-- Event delivery mechanism
  
  2. src/core/lib/surface/call.cc
     |
     +-- grpc_call structure
     +-- grpc_call_start_batch()
     |   - How ops are processed
     |   - State machine transitions
     |
     +-- Cancellation handling
  
  3. src/core/lib/surface/channel.cc
     |
     +-- Channel state machine
     +-- Connectivity states
     +-- grpc_channel_create_call()
  
  4. src/core/lib/transport/transport.h
     |
     +-- Transport interface definition
     +-- How transports are abstracted
  
  5. src/core/lib/transport/http2/
     |
     +-- HTTP/2 framing
     +-- Stream management
     +-- Flow control

  STATE MACHINES TO MAP:
  ----------------------
  
  Channel State Machine:
  
       IDLE --> CONNECTING --> READY --> SHUTDOWN
         ^           |           |
         |           v           |
         +--- TRANSIENT_FAILURE <+
  
  Call State Machine:
  
       PENDING --> STARTED --> SENDING --> RECEIVING --> FINISHED
  
  Completion Queue State:
  
       RUNNING --> SHUTTING_DOWN --> SHUTDOWN
```

**图解说明 (Diagram Explanation):**

Phase 3阅读策略：

**关键目录**：
- `src/core/lib/surface/` → 公共API实现（call.cc, channel.cc, completion_queue.cc）
- `src/core/lib/transport/` → 传输接口和HTTP/2实现
- `src/core/lib/iomgr/` → I/O管理器（pollset等）

**状态机需要映射**：
- Channel: `IDLE → CONNECTING → READY → SHUTDOWN`（含`TRANSIENT_FAILURE`回退）
- Call: `PENDING → STARTED → SENDING → RECEIVING → FINISHED`
- CQ: `RUNNING → SHUTTING_DOWN → SHUTDOWN`

---

## 2. Architecture-Critical Data Structures

```
+=============================================================================+
|                    DATA STRUCTURE ANALYSIS                                  |
+=============================================================================+

  A. LONG-LIVED STATE STRUCTURES
  ==============================
  
  These persist across multiple RPCs:
  
  +---------------------------+----------------------------------------+
  | Structure                 | Lifetime                               |
  +---------------------------+----------------------------------------+
  | grpc_c_client_t           | Client init to free                    |
  | grpc_c_server_t           | Server create to destroy               |
  | grpc_channel              | Bound to grpc_c_client_t               |
  | grpc_server               | Bound to grpc_c_server_t               |
  | grpc_completion_queue     | Multiple: per client, server, context  |
  | grpc_c_thread_pool_t      | Global, created in grpc_c_init()       |
  +---------------------------+----------------------------------------+

  READING TIP: Search for "LIST_HEAD" to find collection structures
  
  Example from grpc-c.h:
  LIST_HEAD(grpc_c_context_list_head, grpc_c_context_s) gcc_context_list_head;

  B. STATE MACHINE STRUCTURES
  ===========================
  
  These encode state transitions:
  
  1. grpc_c_context_t
     - gcc_state field (grpc_c_state_t)
     - Transitions tracked in service.c:gc_handle_server_complete_op()
  
  2. grpc_c_event_t
     - gce_type field (grpc_c_event_type_t)
     - Used as tag in completion queue
  
  3. Channel connectivity (in grpc_channel)
     - grpc_connectivity_state
     - Watched via grpc_channel_watch_connectivity_state()
  
  READING TIP: Search for "switch.*state" to find state machine logic

  C. PERFORMANCE-CRITICAL STRUCTURES
  ==================================
  
  Designed for efficiency:
  
  1. grpc_op (ops array)
     - Pre-allocated and reused
     - Dynamic sizing with grpc_c_ops_alloc()
  
  2. grpc_slice
     - Reference counted buffer
     - Avoids copying when possible
  
  3. grpc_byte_buffer
     - Can hold multiple slices
     - Supports scatter-gather I/O
  
  4. gpr_mu / gpr_cv
     - Lightweight synchronization
     - Platform-optimized (futex on Linux)
  
  READING TIP: Look for "gpr_realloc" calls to find dynamic allocation
```

**图解说明 (Diagram Explanation):**

数据结构分类：

| 类别 | 示例 | 特点 |
|------|------|------|
| **长期存活** | `grpc_c_client_t`, `grpc_c_server_t` | 跨多个RPC |
| **状态机编码** | `gcc_state`, `gce_type` | 驱动状态转换 |
| **性能关键** | `grpc_op[]`, `grpc_slice` | 预分配/复用/零拷贝 |

**搜索技巧**：
- `LIST_HEAD` → 集合结构
- `switch.*state` → 状态机逻辑
- `gpr_realloc` → 动态分配

---

## 3. Hot Paths (Execution-Critical Functions)

```
+=============================================================================+
|                    HOT PATH ANALYSIS                                        |
+=============================================================================+

  A. CALL CREATION AND DISPATCH
  =============================
  
  CLIENT HOT PATH:
  
  grpc_c_client_request_unary()
       |
       +-- gc_client_prepare_unary_ops()      [~20% time]
       |       - grpc_c_context_init()
       |       - grpc_c_ops_alloc()
       |       - Setup 5-6 operations
       |
       +-- grpc_channel_create_call()         [~10% time]
       |       - Allocate grpc_call
       |       - Associate with CQ
       |
       +-- grpc_call_start_batch()            [~30% time]
       |       - Submit to gRPC Core
       |       - Triggers actual I/O
       |
       +-- grpc_completion_queue_pluck()      [~30% time - includes I/O]
       |       - Block for result
       |
       +-- Unpack + cleanup                   [~10% time]

  SERVER HOT PATH:
  
  gc_handle_server_event_internal()
       |
       +-- grpc_completion_queue_next()       [Blocking]
       |
       +-- gc_handle_server_complete_op()     [Dispatch]
       |       |
       |       +-- gc_prepare_server_callback()
       |               |
       |               +-- User handler()
       |                       |
       |                       +-- gc_stream_read()
       |                       +-- gc_stream_write()
       |                       +-- gc_server_stream_finish()

  B. COMPLETION QUEUE POLLING
  ===========================
  
  grpc_completion_queue_next():
  
       +-- Check for pending events (fast path)
       |   If event ready: return immediately
       |
       +-- epoll_wait() / poll() (slow path)
       |   Block until event or timeout
       |
       +-- Process event
       |
       +-- Return to caller

  grpc_completion_queue_pluck():
  
       +-- Scan queue for specific tag
       |   If found: return
       |
       +-- Wait using futex
       |
       +-- Return when tag matches

  C. EVENT DELIVERY
  =================
  
  When I/O completes:
  
  1. Kernel signals via epoll/kqueue
  2. gRPC I/O thread wakes
  3. HTTP/2 frame parsed
  4. Event added to completion queue
  5. Waiting thread (pluck) or poller (next) returns
  6. Tag dispatched to handler

  D. SHUTDOWN AND CLEANUP
  =======================
  
  Client shutdown critical path:
  
  grpc_c_client_free()
       |
       +-- client->gcc_shutdown = 1
       |
       +-- Wait for gcc_running_cb == 0  [May block]
       |
       +-- Free all contexts             [Loop]
       |
       +-- grpc_channel_destroy()
       |
       +-- grpc_completion_queue_shutdown()
       |
       +-- Drain CQ until GRPC_QUEUE_SHUTDOWN
       |
       +-- grpc_completion_queue_destroy()
       |
       +-- gpr_free(client)

  PROFILING TIP: Instrument these functions to measure overhead
```

**图解说明 (Diagram Explanation):**

热点路径分析：

**客户端热点（时间分布）**：
- `gc_client_prepare_unary_ops()` → 20%（初始化context、分配ops）
- `grpc_channel_create_call()` → 10%（分配call对象）
- `grpc_call_start_batch()` → 30%（提交到Core，触发I/O）
- `grpc_completion_queue_pluck()` → 30%（阻塞等待，含I/O）
- 解包+清理 → 10%

**服务端热点**：
- `grpc_completion_queue_next()` → 阻塞等待事件
- `gc_handle_server_complete_op()` → 分发到用户处理器
- 用户处理器中的`read()`/`write()`/`finish()`

---

## 4. Validating WHY / HOW / WHAT in Code

```
+=============================================================================+
|                    CODE VALIDATION GUIDE                                    |
+=============================================================================+

  A. FINDING DESIGN TRADE-OFFS IN CODE
  =====================================
  
  1. EXPLICIT LIFECYCLE (from WHY section)
  ----------------------------------------
  
  Look in: lib/client.c, lib/service.c
  
  Find: grpc_c_client_free(), grpc_c_server_destroy()
  
  Validate: No automatic cleanup. User MUST call free/destroy.
  
  Code evidence:
  ```c
  // From client.c
  void grpc_c_client_free(grpc_c_client_t *client) {
      // Wait for callbacks to complete
      while (client->gcc_running_cb > 0 || client->gcc_wait) {
          gpr_cv_wait(...);
      }
      // Explicit cleanup of each resource
      grpc_channel_destroy(client->gcc_channel);
      grpc_completion_queue_shutdown(client->gcc_cq);
      // ...
  }
  ```
  
  2. COMPLETION QUEUE PATTERN (from HOW section)
  ----------------------------------------------
  
  Look in: lib/stream_ops.c
  
  Find: gc_stream_read(), gc_stream_write()
  
  Validate: Every operation uses CQ for completion notification.
  
  Code evidence:
  ```c
  // From stream_ops.c
  grpc_call_start_batch(context->gcc_call, 
                        context->gcc_ops + op_count, 
                        1, &context->gcc_read_event, NULL);
  
  ev = grpc_completion_queue_pluck(context->gcc_cq, 
                                   &context->gcc_read_event,
                                   deadline, NULL);
  ```
  
  3. OPAQUE HANDLE PATTERN (from WHAT section)
  --------------------------------------------
  
  Look in: lib/h/grpc-c/grpc-c.h vs lib/context.c
  
  Find: typedef vs struct definition
  
  Validate: Public header only has typedef, implementation has full struct.
  
  Code evidence:
  ```c
  // grpc-c.h (public)
  typedef struct grpc_c_context_s grpc_c_context_t;
  
  // context.c (private)
  struct grpc_c_context_s {
      struct grpc_c_method_t *gcc_method;
      grpc_byte_buffer *gcc_payload;
      // ... full implementation
  };
  ```

  B. PERFORMANCE OVER READABILITY
  ================================
  
  1. Ops array reuse (avoid malloc per RPC)
  -----------------------------------------
  
  Look in: lib/context.c:grpc_c_ops_alloc()
  
  ```c
  if (context->gcc_ops == NULL) {
      context->gcc_ops = gpr_malloc(sizeof(grpc_op) * count);
  } else {
      // Realloc to grow, not recreate
      context->gcc_ops = gpr_realloc(context->gcc_ops, ...);
  }
  ```
  
  2. Reference counting for events
  ---------------------------------
  
  Look in: lib/service.c:gc_handle_server_event_internal()
  
  ```c
  gpr_mu_lock(context->gcc_lock);
  gcev->gce_refcount--;
  gpr_mu_unlock(context->gcc_lock);
  ```
  
  3. Inline functions for hot paths
  ----------------------------------
  
  Look in: lib/h/grpc-c/grpc-c.h
  
  ```c
  static inline void 
  grpc_c_set_write_done(grpc_c_context_t *context, ...) {
      if (context != NULL) {
          context->gcc_write_resolve_cb = cb;
          context->gcc_write_resolve_arg = data;
      }
  }
  ```

  C. ABSTRACTION BOUNDARIES AND LEAKS
  ====================================
  
  1. CLEAN BOUNDARY: grpc-c hides CQ from user
  --------------------------------------------
  
  User code (foo_server.c):
  ```c
  context->gcc_stream->read(context, &request, 0, -1);
  ```
  
  Implementation (stream_ops.c):
  ```c
  grpc_call_start_batch(...);
  ev = grpc_completion_queue_pluck(context->gcc_cq, ...);
  ```
  
  2. ABSTRACTION LEAK: Direct access to gcc_status
  -------------------------------------------------
  
  Look in: lib/stream_ops.c:gc_client_stream_finish()
  
  ```c
  // Status code exposed directly
  status->gcs_code = context->gcc_status;
  ```
  
  This leaks gRPC status codes into grpc-c abstraction.
  
  3. BOUNDARY: Thread pool is internal
  ------------------------------------
  
  User never sees thread pool:
  
  ```c
  // hooks.c
  static grpc_c_thread_pool_t *gc_tpool;  // Static, hidden
  
  // User API
  grpc_c_init(GRPC_THREADS, NULL);  // Pool created internally
  ```

  D. EXERCISE: TRACE AN RPC
  ==========================
  
  Pick examples/foo_client.c and trace:
  
  1. foo__greeter__say_hello()
     └── Where is this generated? (foo.grpc-c.c)
  
  2. grpc_c_client_request_unary()
     └── How are ops built? (gc_client_prepare_unary_ops)
  
  3. grpc_call_start_batch()
     └── What happens in gRPC Core?
  
  4. grpc_completion_queue_pluck()
     └── How long does this block?
  
  5. Response unpacking
     └── Where is protobuf decoded?
```

**图解说明 (Diagram Explanation):**

代码验证技巧：

**验证设计决策**：
1. **显式生命周期**：查看`grpc_c_client_free()`，没有自动清理
2. **完成队列模式**：每个操作都用`start_batch()` + `pluck()`
3. **不透明句柄**：公共头typedef vs 实现文件struct定义

**性能优于可读性的例子**：
- ops数组复用（避免每RPC malloc）
- 事件引用计数（避免提前释放）
- 热点路径内联函数

**抽象边界**：grpc-c隐藏CQ（用户只见`stream->read()`），但暴露`gcc_status`（抽象泄漏）

---

## 中文说明

### 源代码阅读策略

#### 阅读阶段

**第一阶段：公共API（1-2小时）**
- 目标：了解用户可以做什么
- 文件：`grpc-c.h`, `grpc.h`, `byte_buffer.h`
- 关注：类型定义、生命周期函数、RPC请求函数

**第二阶段：grpc-c包装层（4-8小时）**
- 目标：理解grpc-c如何封装gRPC Core
- 文件优先级：
  1. `examples/foo_*.c` - 先看示例
  2. `lib/client.c` - 客户端实现（★最重要）
  3. `lib/service.c` - 服务端实现（★最重要）
  4. `lib/context.c` - 上下文管理
  5. `lib/stream_ops.c` - 读写操作

**第三阶段：gRPC Core内部（8-16小时）**
- 目标：理解运行时实际执行
- 关键目录：
  - `src/core/lib/surface/` - 公共API实现
  - `src/core/lib/transport/` - 传输层
  - `src/core/lib/iomgr/` - I/O管理

#### 关键数据结构分类

| 类型 | 示例 | 特点 |
|------|------|------|
| 长期存活 | `grpc_c_client_t`, `grpc_c_server_t` | 跨多个RPC存在 |
| 状态机 | `grpc_c_context_t.gcc_state` | 编码状态转换 |
| 性能关键 | `grpc_slice`, `grpc_op[]` | 为效率优化 |

#### 热点路径

**客户端热点：**
1. `gc_client_prepare_unary_ops()` - 20%时间
2. `grpc_channel_create_call()` - 10%时间
3. `grpc_call_start_batch()` - 30%时间
4. `grpc_completion_queue_pluck()` - 30%时间（含I/O等待）
5. 解包和清理 - 10%时间

**服务端热点：**
1. `grpc_completion_queue_next()` - 阻塞等待
2. `gc_handle_server_complete_op()` - 事件分发
3. 用户处理器中的read/write/finish

#### 代码验证技巧

- 搜索"LIST_HEAD"找集合结构
- 搜索"switch.*state"找状态机逻辑
- 搜索"gpr_realloc"找动态分配
- 搜索"gpr_mu_lock"找并发控制点

#### 设计决策验证

1. **显式生命周期**：查看`grpc_c_client_free()`，没有自动清理
2. **完成队列模式**：每个操作都使用CQ通知完成
3. **不透明句柄**：公共头文件只有typedef，实现文件有完整struct
