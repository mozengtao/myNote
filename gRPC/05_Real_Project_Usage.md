# Real Project Usage and Engineering Practices

## 1. Using gRPC in a Real C Project

```
+=============================================================================+
|                    PROJECT INTEGRATION STRATEGY                             |
+=============================================================================+

  A. ISOLATING gRPC FROM BUSINESS LOGIC
  =====================================
  
  WRONG APPROACH (Tight Coupling):
  
  void process_order(Order *order) {
      grpc_c_client_t *client = grpc_c_client_init(...);  // gRPC in business
      grpc_c_context_t *ctx;
      grpc_call_start_batch(...);                          // gRPC details leak
      // Business logic mixed with RPC mechanics
  }
  
  RIGHT APPROACH (Clean Separation):
  
  // Layer 1: Business Logic (knows nothing about gRPC)
  OrderResult *process_order(Order *order) {
      // Pure business logic
      return validate_and_process(order);
  }
  
  // Layer 2: Service Interface (abstracts RPC)
  typedef struct {
      int (*get_inventory)(const char *item_id, int *quantity);
      int (*submit_order)(Order *order, OrderResult **result);
  } InventoryService;
  
  // Layer 3: gRPC Implementation (hidden)
  static int grpc_get_inventory(const char *item_id, int *quantity) {
      // All gRPC details here
      grpc_c_client_t *client = get_cached_client();
      // ...
  }

  B. DESIGNING MODULE BOUNDARIES
  ==============================
  
  +------------------------------------------------------------------+
  |                    APPLICATION LAYER                              |
  |  +------------------+  +------------------+  +------------------+ |
  |  | Order Processing |  | User Management  |  | Reporting        | |
  |  +--------+---------+  +--------+---------+  +--------+---------+ |
  |           |                     |                     |           |
  +-----------|---------------------|---------------------|----------+
              |                     |                     |
  +-----------|---------------------|---------------------|----------+
  |           v                     v                     v           |
  |                    SERVICE ABSTRACTION LAYER                      |
  |  +------------------+  +------------------+  +------------------+ |
  |  | InventoryClient  |  | UserClient       |  | AnalyticsClient  | |
  |  | (interface)      |  | (interface)      |  | (interface)      | |
  |  +--------+---------+  +--------+---------+  +--------+---------+ |
  |           |                     |                     |           |
  +-----------|---------------------|---------------------|----------+
              |                     |                     |
  +-----------|---------------------|---------------------|----------+
  |           v                     v                     v           |
  |                    RPC IMPLEMENTATION LAYER                       |
  |  +------------------+  +------------------+  +------------------+ |
  |  | grpc_inventory.c |  | grpc_user.c      |  | grpc_analytics.c | |
  |  | (grpc-c calls)   |  | (grpc-c calls)   |  | (grpc-c calls)   | |
  |  +--------+---------+  +--------+---------+  +--------+---------+ |
  |           |                     |                     |           |
  +-----------|---------------------|---------------------|----------+
              |                     |                     |
  +-----------|---------------------|---------------------|----------+
  |           v                     v                     v           |
  |                    TRANSPORT LAYER                                |
  |  +----------------------------------------------------------+    |
  |  |              Shared Client Pool / Connection Manager       |    |
  |  +----------------------------------------------------------+    |
  +------------------------------------------------------------------+

  C. WHEN TO WRAP gRPC APIs
  =========================
  
  WRAP when:
  ----------
  
  1. You need consistent error handling
  
     // Wrapper
     typedef enum {
         SVC_OK,
         SVC_NETWORK_ERROR,
         SVC_TIMEOUT,
         SVC_INVALID_RESPONSE,
         SVC_SERVER_ERROR,
     } svc_status_t;
     
     svc_status_t inventory_get(const char *id, Item **item);
  
  2. You want to hide retry logic
  
     svc_status_t inventory_get_with_retry(const char *id, Item **item) {
         for (int i = 0; i < MAX_RETRIES; i++) {
             svc_status_t st = inventory_get(id, item);
             if (st != SVC_NETWORK_ERROR) return st;
             sleep_with_backoff(i);
         }
         return SVC_NETWORK_ERROR;
     }
  
  3. You need connection pooling
  
     // Pool manages multiple clients
     grpc_c_client_t *client_pool_get(const char *service);
     void client_pool_release(grpc_c_client_t *client);
  
  DON'T WRAP when:
  ----------------
  
  1. Simple prototyping
  2. Single service, single client
  3. No special error handling needs
```

**图解说明 (Diagram Explanation):**

项目集成策略：

**正确分层（Clean Separation）**：
```
应用层（纯业务逻辑）
    ↓
服务抽象层（接口定义，隔离RPC）
    ↓
RPC实现层（grpc-c调用，隐藏细节）
    ↓
传输层（共享连接池）
```

**何时封装gRPC API**：
- ✅ 需要统一错误处理
- ✅ 需要重试/退避逻辑
- ✅ 需要连接池

**何时不封装**：
- ❌ 简单原型开发
- ❌ 单服务单客户端

---

## 2. Recommended Project Structure

```
+=============================================================================+
|                    PROJECT DIRECTORY LAYOUT                                 |
+=============================================================================+

  my_project/
  │
  ├── proto/                          # Protocol definitions
  │   ├── inventory.proto
  │   ├── user.proto
  │   └── common.proto
  │
  ├── gen/                            # Generated code (don't edit!)
  │   ├── inventory.grpc-c.h
  │   ├── inventory.grpc-c.c
  │   ├── inventory.pb-c.h
  │   ├── inventory.pb-c.c
  │   └── ...
  │
  ├── rpc/                            # RPC wrappers
  │   ├── rpc_common.h                # Shared types and utilities
  │   ├── rpc_common.c
  │   ├── inventory_client.h          # Client interface
  │   ├── inventory_client.c          # gRPC implementation
  │   ├── user_client.h
  │   ├── user_client.c
  │   └── client_pool.c               # Connection pooling
  │
  ├── transport/                      # Low-level transport utilities
  │   ├── channel_factory.h           # Channel creation
  │   ├── channel_factory.c
  │   ├── credentials.h               # TLS/auth helpers
  │   └── credentials.c
  │
  ├── service/                        # Server-side implementations
  │   ├── inventory_service.h
  │   ├── inventory_service.c         # Business logic handlers
  │   ├── user_service.h
  │   └── user_service.c
  │
  ├── app/                            # Application entry points
  │   ├── server_main.c               # Server application
  │   ├── client_main.c               # Client application
  │   └── config.h                    # Configuration
  │
  ├── tests/                          # Tests
  │   ├── test_inventory_client.c
  │   ├── test_user_service.c
  │   └── mock_services.c
  │
  ├── Makefile
  └── README.md

  BUILD FLOW:
  ===========
  
  1. proto/*.proto
         |
         | protoc --grpc-c_out --c_out
         v
  2. gen/*.grpc-c.{h,c}, gen/*.pb-c.{h,c}
         |
         | compile with rpc/*.c, transport/*.c
         v
  3. librpc.a (static library)
         |
         | link with service/*.c or app/*.c
         v
  4. server_binary, client_binary

**图解说明 (Diagram Explanation):**

推荐项目结构：

| 目录 | 内容 | 注意事项 |
|------|------|----------|
| `proto/` | `.proto`文件 | 接口定义 |
| `gen/` | 生成的代码 | **不要手动编辑** |
| `rpc/` | RPC封装层 | 客户端接口+实现 |
| `transport/` | 传输工具 | Channel工厂、认证 |
| `service/` | 服务端业务逻辑 | 处理器实现 |
| `app/` | 应用入口 | main函数 |
| `tests/` | 测试 | 单元+集成测试 |

**构建流程**：`.proto` → protoc → `gen/*.grpc-c.*` → 编译 → `librpc.a` → 链接 → 二进制

  MAKEFILE SNIPPET:
  =================
  
  PROTO_DIR = proto
  GEN_DIR = gen
  PROTOS = $(wildcard $(PROTO_DIR)/*.proto)
  
  # Generate gRPC-C code
  $(GEN_DIR)/%.grpc-c.c $(GEN_DIR)/%.grpc-c.h: $(PROTO_DIR)/%.proto
  	protoc --plugin=protoc-gen-grpc-c=$(GRPC_C_PLUGIN) \
  	       --grpc-c_out=$(GEN_DIR) \
  	       --proto_path=$(PROTO_DIR) $<
  
  # Generate protobuf-c code
  $(GEN_DIR)/%.pb-c.c $(GEN_DIR)/%.pb-c.h: $(PROTO_DIR)/%.proto
  	protoc --c_out=$(GEN_DIR) --proto_path=$(PROTO_DIR) $<
```

---

## 3. Common Pitfalls

```
+=============================================================================+
|                    COMMON PITFALLS AND SOLUTIONS                            |
+=============================================================================+

  A. FORGETTING SHUTDOWN → MEMORY LEAKS
  =====================================
  
  WRONG:
  
  void send_notification(const char *msg) {
      grpc_c_client_t *client = grpc_c_client_init(...);
      // Use client
      // EXIT WITHOUT CLEANUP!
  }  // <- Memory leak: channel, CQ, contexts
  
  RIGHT:
  
  void send_notification(const char *msg) {
      grpc_c_client_t *client = grpc_c_client_init(...);
      // Use client
      grpc_c_client_free(client);  // Always cleanup
  }
  
  BETTER (RAII-like pattern in C):
  
  #define WITH_CLIENT(name, server, id) \
      for (grpc_c_client_t *name = grpc_c_client_init(server, id, NULL, NULL), \
           *_guard = name; \
           _guard; \
           grpc_c_client_free(name), _guard = NULL)
  
  void send_notification(const char *msg) {
      WITH_CLIENT(client, "notify", "sender") {
          // Use client
      }  // Automatically freed
  }

  B. MISUSING COMPLETION QUEUES
  =============================
  
  WRONG: Polling wrong CQ
  
  grpc_c_client_t *client1 = grpc_c_client_init(...);
  grpc_c_client_t *client2 = grpc_c_client_init(...);
  
  // Start batch on client1
  grpc_call_start_batch(call1, ops, count, tag, NULL);
  
  // Pluck from client2's CQ - WRONG!
  grpc_completion_queue_pluck(client2->gcc_cq, tag, ...);  // Will hang!
  
  RIGHT: Use the correct CQ
  
  // Each call is associated with a specific CQ via context
  grpc_completion_queue_pluck(context->gcc_cq, tag, ...);
  
  WRONG: Not draining CQ before destroy
  
  grpc_completion_queue_shutdown(cq);
  grpc_completion_queue_destroy(cq);  // May crash or leak!
  
  RIGHT: Drain all events first
  
  grpc_completion_queue_shutdown(cq);
  while (grpc_completion_queue_next(cq, 
         gpr_inf_past(GPR_CLOCK_REALTIME), 
         NULL).type != GRPC_QUEUE_SHUTDOWN) {
      // Drain pending events
  }
  grpc_completion_queue_destroy(cq);

  C. BLOCKING INSIDE RPC HANDLERS
  ================================
  
  WRONG: Long blocking in handler
  
  void my_handler(grpc_c_context_t *context) {
      Request *req;
      context->gcc_stream->read(context, (void **)&req, 0, -1);
      
      sleep(60);  // Blocks worker thread for 60 seconds!
      
      // Other RPCs starved
      context->gcc_stream->write(context, &response, 0, -1);
  }
  
  RIGHT: Offload heavy work
  
  void my_handler(grpc_c_context_t *context) {
      Request *req;
      context->gcc_stream->read(context, (void **)&req, 0, -1);
      
      // Quick validation
      if (!validate(req)) {
          send_error(context);
          return;
      }
      
      // Queue work for processing
      work_queue_add(req, context);
      // Handler returns quickly, response sent later
  }
  
  // Or use async processing
  void my_handler(grpc_c_context_t *context) {
      // Use non-blocking I/O
      context->gcc_stream->read(context, &req, 0, 100);  // 100ms timeout
  }

**图解说明 (Diagram Explanation):**

常见陷阱对比：

| 陷阱 | 错误做法 | 正确做法 |
|------|----------|----------|
| 忘记关闭 | 函数结束不调用`free()` | 使用RAII模式或`WITH_CLIENT`宏 |
| CQ错误 | pluck错误的CQ | 使用`context->gcc_cq` |
| 处理器阻塞 | `sleep(60)`在处理器中 | 排队给工作线程 |

  D. CREATING CHANNELS PER REQUEST
  =================================
  
  WRONG: New channel for every RPC
  
  void get_user(const char *user_id, User **user) {
      // Creates new TCP connection, TLS handshake each time!
      grpc_c_client_t *client = grpc_c_client_init(server, "tmp", NULL, NULL);
      user_service__get_user(client, ...);
      grpc_c_client_free(client);
  }
  // ~100ms overhead per request
  
  RIGHT: Reuse channels
  
  static grpc_c_client_t *user_client = NULL;
  
  void init_services(void) {
      user_client = grpc_c_client_init(server, "user-svc", NULL, NULL);
  }
  
  void get_user(const char *user_id, User **user) {
      // Reuse existing connection
      user_service__get_user(user_client, ...);
  }
  // ~1ms overhead per request

  E. MIXING SYNC AND ASYNC INCORRECTLY
  =====================================
  
  WRONG: Blocking in async callback
  
  void my_callback(grpc_c_context_t *ctx, void *tag, int success) {
      // This runs in worker thread!
      
      // DON'T DO THIS - blocks the worker
      sleep(10);
      
      // DON'T DO THIS - deadlock risk
      grpc_c_client_wait(client);
  }
  
  RIGHT: Keep callbacks fast
  
  void my_callback(grpc_c_context_t *ctx, void *tag, int success) {
      // Quick processing only
      log_result(success);
      
      // Queue for later if needed
      result_queue_add(ctx, tag, success);
      
      // Signal waiting thread if needed
      pthread_cond_signal(&result_ready);
  }
```

**图解说明 (Diagram Explanation):**

更多常见陷阱：

| 陷阱 | 错误做法 | 正确做法 | 性能差异 |
|------|----------|----------|----------|
| 每请求创建Channel | 每次RPC创建新client | 复用静态client | **100倍差异** |
| 混用同步异步 | 回调中阻塞 | 回调保持快速 | 死锁风险 |

**关键原则**：
- CQ关闭前必须排空
- 处理器中避免阻塞I/O
- 回调中只做快速处理

---

## 4. Anti-Patterns

```
+=============================================================================+
|                    ANTI-PATTERNS TO AVOID                                   |
+=============================================================================+

  A. TREATING RPC AS LOCAL FUNCTION CALL
  ======================================
  
  ANTI-PATTERN:
  
  int calculate_total(Order *order) {
      int total = 0;
      for (int i = 0; i < order->item_count; i++) {
          // Calling RPC in a loop!
          Price *price = get_price_rpc(order->items[i].id);  // Network call
          total += price->amount * order->items[i].quantity;
      }
      return total;
  }
  // 100 items = 100 network round trips = 100 × 5ms = 500ms!
  
  RIGHT APPROACH:
  
  int calculate_total(Order *order) {
      // Batch request
      PriceRequest req;
      req.item_ids = extract_ids(order);
      req.count = order->item_count;
      
      // Single RPC for all prices
      PriceResponse *prices = get_prices_batch_rpc(&req);  // 1 network call
      
      int total = 0;
      for (int i = 0; i < order->item_count; i++) {
          total += prices->amounts[i] * order->items[i].quantity;
      }
      return total;
  }
  // 100 items = 1 network round trip = 1 × 5ms = 5ms!

  B. PERFORMING HEAVY I/O INSIDE HANDLERS
  ========================================
  
  ANTI-PATTERN:
  
  void upload_handler(grpc_c_context_t *context) {
      FileUploadRequest *req;
      context->gcc_stream->read(context, (void **)&req, 0, -1);
      
      // Blocking file I/O in handler!
      FILE *f = fopen(req->path, "wb");
      fwrite(req->data, 1, req->size, f);  // May take seconds
      fclose(f);
      
      // Worker thread blocked, other RPCs waiting
      context->gcc_stream->finish(context, &status, 0);
  }
  
  RIGHT APPROACH:
  
  void upload_handler(grpc_c_context_t *context) {
      FileUploadRequest *req;
      context->gcc_stream->read(context, (void **)&req, 0, -1);
      
      // Queue for async I/O thread
      io_queue_write(req->path, req->data, req->size, 
                     upload_complete_callback, context);
      
      // Handler returns immediately
      // Response sent when I/O completes
  }

  C. OVERUSING STREAMING WITHOUT FLOW CONTROL
  ============================================
  
  ANTI-PATTERN:
  
  void stream_all_data(grpc_c_context_t *context) {
      for (int i = 0; i < 1000000; i++) {
          DataChunk chunk = get_chunk(i);
          // Writing as fast as possible!
          context->gcc_stream->write(context, &chunk, 0, -1);
      }
  }
  // Can overwhelm receiver, cause OOM
  
  RIGHT APPROACH:
  
  void stream_all_data(grpc_c_context_t *context) {
      int batch_size = 100;
      for (int i = 0; i < 1000000; i += batch_size) {
          for (int j = 0; j < batch_size && i+j < 1000000; j++) {
              DataChunk chunk = get_chunk(i + j);
              context->gcc_stream->write(context, &chunk, 0, -1);
          }
          
          // Check if receiver can keep up
          if (grpc_c_is_write_pending(context)) {
              // Back off, let receiver catch up
              usleep(1000);
          }
      }
  }

  D. HIDING gRPC LIFECYCLE BEHIND GLOBALS
  =======================================
  
  ANTI-PATTERN:
  
  // Global client (hidden)
  static grpc_c_client_t *_client = NULL;
  
  void init_module(void) {
      _client = grpc_c_client_init(...);
      // What if init_module() called twice?
      // How to shutdown?
      // Thread safety?
  }
  
  Result *do_rpc(void) {
      // User has no idea there's a global
      return call_service(_client);
  }
  
  RIGHT APPROACH:
  
  // Explicit dependency injection
  typedef struct {
      grpc_c_client_t *client;
      // Other dependencies
  } ServiceContext;
  
  ServiceContext *service_context_create(const char *server) {
      ServiceContext *ctx = malloc(sizeof(ServiceContext));
      ctx->client = grpc_c_client_init(server, "svc", NULL, NULL);
      return ctx;
  }
  
  void service_context_destroy(ServiceContext *ctx) {
      grpc_c_client_free(ctx->client);
      free(ctx);
  }
  
  Result *do_rpc(ServiceContext *ctx) {
      return call_service(ctx->client);
  }
  
  // Usage is explicit about lifecycle
  int main() {
      ServiceContext *ctx = service_context_create("server:50051");
      do_rpc(ctx);
      service_context_destroy(ctx);
  }
```

**图解说明 (Diagram Explanation):**

反模式总结：

| 反模式 | 问题 | 解决方案 |
|--------|------|----------|
| 把RPC当本地调用 | 循环中调用RPC = N次网络往返 | **批量请求** |
| 处理器中重I/O | 阻塞工作线程 | 排队给I/O线程 |
| 无流控的流式 | 压垮接收方 | 检查`write_pending` |
| 全局隐藏生命周期 | 线程安全?重复初始化? | **依赖注入** |

**性能对比（100项查询）**：
- 循环RPC: 100×5ms = **500ms**
- 批量RPC: 1×5ms = **5ms**

---

## 5. Production Checklist

```
+=============================================================================+
|                    PRODUCTION READINESS CHECKLIST                           |
+=============================================================================+

  INITIALIZATION
  ==============
  [ ] grpc_c_init() called before any gRPC operations
  [ ] Thread pool size configured appropriately
  [ ] Signal handlers registered for graceful shutdown
  
  CONNECTION MANAGEMENT
  =====================
  [ ] Channels are reused, not created per-request
  [ ] Connection pool implemented for high concurrency
  [ ] Reconnection logic handles transient failures
  [ ] Channel keep-alive configured if needed
  
  ERROR HANDLING
  ==============
  [ ] All RPC return codes checked
  [ ] Timeout values set appropriately
  [ ] Retry logic with exponential backoff
  [ ] Circuit breaker for failing services
  
  RESOURCE MANAGEMENT
  ===================
  [ ] All clients freed before shutdown
  [ ] All servers destroyed before shutdown
  [ ] grpc_c_shutdown() called at end
  [ ] No memory leaks under load (use valgrind)
  
  CONCURRENCY
  ===========
  [ ] No blocking in RPC handlers
  [ ] Shared state protected by mutex
  [ ] Callback execution thread understood
  [ ] Deadlock scenarios analyzed
  
  MONITORING
  ==========
  [ ] Latency metrics collected
  [ ] Error rates tracked
  [ ] Connection state monitored
  [ ] Resource usage (memory, FD) tracked
  
  SECURITY
  ========
  [ ] TLS enabled for production
  [ ] Certificate validation configured
  [ ] Authentication/authorization in place
  [ ] Sensitive data not logged
  
  TESTING
  =======
  [ ] Unit tests for business logic
  [ ] Integration tests with real gRPC
  [ ] Load tests to find breaking point
  [ ] Chaos tests for failure scenarios
```

**图解说明 (Diagram Explanation):**

生产就绪检查清单分类：

| 类别 | 关键检查项 |
|------|------------|
| **初始化** | `grpc_c_init()`在最前、信号处理器注册 |
| **连接管理** | Channel复用、连接池、重连逻辑 |
| **错误处理** | 检查返回值、设置超时、重试+熔断 |
| **资源管理** | 全部释放、valgrind检查无泄漏 |
| **并发** | 不在处理器阻塞、共享状态加锁 |
| **监控** | 延迟、错误率、连接状态、资源使用 |
| **安全** | TLS、证书验证、认证授权 |
| **测试** | 单元测试、集成测试、负载测试、混沌测试 |

---

## 中文说明

### 实际项目使用指南

#### 1. 模块隔离策略

**正确的分层架构：**
- **应用层**：纯业务逻辑，不知道gRPC存在
- **服务抽象层**：定义接口，隔离RPC细节
- **RPC实现层**：grpc-c调用，隐藏在接口后面
- **传输层**：连接池、认证等基础设施

**何时封装gRPC API：**
- 需要统一错误处理时
- 需要重试逻辑时
- 需要连接池时

**何时不封装：**
- 简单原型开发
- 单一服务、单一客户端

#### 2. 推荐项目结构

```
my_project/
├── proto/          # .proto文件
├── gen/            # 生成的代码（不要编辑）
├── rpc/            # RPC封装层
├── transport/      # 传输工具
├── service/        # 服务端业务逻辑
├── app/            # 应用入口
└── tests/          # 测试
```

#### 3. 常见陷阱

| 陷阱 | 后果 | 解决方案 |
|------|------|----------|
| 忘记关闭 | 内存泄漏 | 使用RAII模式或checklist |
| CQ使用错误 | 挂起或崩溃 | 使用正确的context CQ |
| 处理器中阻塞 | 其他RPC饿死 | 异步处理或工作队列 |
| 每请求创建Channel | 性能差100倍 | 复用Channel |
| 混用同步异步 | 死锁 | 保持回调快速 |

#### 4. 反模式

1. **把RPC当本地调用**：循环中调用RPC应改为批量请求
2. **处理器中做重I/O**：应该排队给专门的I/O线程
3. **无流控的流式传输**：可能压垮接收方
4. **全局隐藏生命周期**：应该使用依赖注入模式

#### 5. 生产检查清单

- 初始化：grpc_c_init在最前，信号处理器注册
- 连接：复用Channel，配置连接池和重连
- 错误：检查返回值，设置超时，实现重试和熔断
- 资源：全部释放，无内存泄漏
- 并发：不在处理器阻塞，保护共享状态
- 监控：延迟、错误率、连接状态
- 安全：TLS、认证、敏感数据不记录日志
