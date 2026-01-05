# HOW | Design Philosophy and Core Ideas

## Architecture Overview

```
+=============================================================================+
|                    gRPC DESIGN PHILOSOPHY STACK                             |
+=============================================================================+

                    +-----------------------------------------------+
                    |            EXPLICIT > CONVENIENT              |
                    |  "Make the right thing obvious, wrong costly" |
                    +-----------------------------------------------+
                                        |
          +-----------------------------+-----------------------------+
          |                             |                             |
          v                             v                             v
  +----------------+          +------------------+          +------------------+
  | IDL-First      |          | Compile-Time     |          | Explicit         |
  | Development    |          | Safety           |          | Lifecycle        |
  +----------------+          +------------------+          +------------------+
  | .proto file    |          | Generated code   |          | Create/Use/      |
  | before code    |          | catches errors   |          | Destroy pattern  |
  +----------------+          +------------------+          +------------------+
          |                             |                             |
          +-----------------------------+-----------------------------+
                                        |
                                        v
                    +-----------------------------------------------+
                    |            EVENT-DRIVEN CONCURRENCY           |
                    |  "Don't block, don't spawn, use the CQ"       |
                    +-----------------------------------------------+
```

**图解说明 (Diagram Explanation):**

gRPC设计哲学核心：**显式优于便利**

- **IDL优先**：先定义`.proto`，再写代码，编译器保证类型匹配
- **编译时安全**：生成的代码在编译期捕获错误，而非运行时崩溃
- **显式生命周期**：Create → Use → Destroy模式，资源所有权清晰
- **事件驱动并发**：不阻塞、不滥生线程，用完成队列（CQ）统一处理

---

## 1. gRPC's Fundamental Design Philosophy

### 1.1 IDL-First Development

```
+=============================================================================+
|                    IDL-FIRST DEVELOPMENT WORKFLOW                           |
+=============================================================================+

  Traditional (Code-First):
  -------------------------
  
  1. Write server code
  2. Write API documentation
  3. Write client code
  4. Hope they match
  5. Runtime errors when they don't
  
         [Server]                    [Client]
            |                           |
            v                           v
       "I think the                "I think the
        API looks like..."          API looks like..."
            |                           |
            +--------> MISMATCH <-------+
                          |
                          v
                    [Runtime Crash]

  IDL-First (gRPC):
  ------------------
  
  1. Define .proto file (contract)
  2. Generate server skeleton
  3. Generate client stub
  4. Both GUARANTEED to match
  
         [service.proto]
              |
              | protoc --grpc-c_out
              |
       +------+------+
       |             |
       v             v
   [Server]      [Client]
   Skeleton       Stub
       |             |
       | Compiler guarantees match
       +------+------+
              |
              v
       [Always Works]

  Example from grpc-c:
  --------------------
  
  foo.proto:
  +------------------------------------------+
  | service Greeter {                         |
  |   rpc SayHello (HelloRequest)             |
  |       returns (HelloReply) {}             |
  | }                                         |
  +------------------------------------------+
                    |
                    | protoc-gen-grpc-c
                    v
  foo.grpc-c.h:
  +------------------------------------------+
  | void foo__greeter__say_hello_cb(          |
  |     grpc_c_context_t *context);           |
  |                                           |
  | int foo__greeter__say_hello(              |
  |     grpc_c_client_t *client, ...);        |
  +------------------------------------------+
```

**图解说明 (Diagram Explanation):**

IDL优先 vs 代码优先对比：

| 代码优先 | IDL优先（gRPC）|
|----------|----------------|
| 服务端先写代码 | 先定义`.proto` |
| 手写API文档 | 自动生成文档 |
| 客户端猜测格式 | 生成类型安全存根 |
| 运行时发现不匹配 | 编译时保证匹配 |

示例：`foo.proto` → `protoc-gen-grpc-c` → `foo.grpc-c.h`（服务端回调）+ 客户端调用函数

### 1.2 Strong Contracts vs Flexible Payloads

```
+=============================================================================+
|                    CONTRACT STRENGTH SPECTRUM                               |
+=============================================================================+

  Weak Contracts (JSON)              Strong Contracts (Protobuf)
  =====================              ==========================
  
  { "user": { "name": "?" } }        message User { string name = 1; }
        |                                    |
        v                                    v
  - Any field can be any type        - Types are fixed at compile time
  - Nullable by default              - Required unless "optional"
  - No versioning rules              - Field numbers provide versioning
  - Schema optional                  - Schema required
  
  
  Evolution Comparison:
  ---------------------
  
  JSON v1: { "user": "alice" }
  JSON v2: { "user": { "name": "alice" } }  // BREAKING!
  
  Proto v1: message User { string name = 1; }
  Proto v2: message User { 
              string name = 1;
              int32 age = 2;  // New field, old clients ignore
            }
  
  Wire Format Stability:
  ----------------------
  
  Field #1 is ALWAYS wire tag 0x0A (for length-delimited string)
  
  v1 wire: [0x0A][length]["alice"]
  v2 wire: [0x0A][length]["alice"][0x10][age_varint]
  
  Old client reads v2: Sees unknown tag 0x10, skips it. Works!
```

**图解说明 (Diagram Explanation):**

强契约（Protobuf）vs 弱契约（JSON）：

- **JSON演进**：`"user": "alice"` → `"user": { "name": "alice" }` = **破坏性变更！**
- **Protobuf演进**：添加`int32 age = 2`，老客户端忽略未知字段，**无破坏**
- **线格式稳定**：字段1永远是`0x0A`标签，新字段使用新标签，老版本安全跳过

### 1.3 Compile-Time Code Generation vs Runtime Reflection

```
+=============================================================================+
|                    GENERATION VS REFLECTION                                 |
+=============================================================================+

  Runtime Reflection (e.g., JSON):
  --------------------------------
  
  char *json = "{\"name\": \"alice\", \"age\": 30}";
  
  +----------------+
  | Parse JSON     | <-- CPU cycles on every request
  +----------------+
         |
         v
  +----------------+
  | Find "name"    | <-- String comparison
  +----------------+
         |
         v
  +----------------+
  | Extract value  | <-- Type coercion
  +----------------+
         |
         v
  +----------------+
  | Validate type  | <-- Runtime check
  +----------------+
  
  Cost: ~1000 CPU cycles per field access

  Compile-Time Generation (Protobuf):
  ------------------------------------
  
  // Generated code (compile-time)
  typedef struct _Foo__HelloRequest {
      ProtobufCMessage base;
      char *name;           // Direct pointer
  } Foo__HelloRequest;
  
  // Runtime access
  char *name = request->name;  // ONE pointer dereference
  
  Cost: ~1 CPU cycle per field access
  
  1000x FASTER

  grpc-c Code Generation:
  -----------------------
  
  .proto                  protoc-gen-grpc-c           .grpc-c.h/.c
  +------+                +------------------+        +----------+
  |      | -------------> | Code Generator   | -----> | Stub     |
  |      |                |                  |        | Skeleton |
  +------+                +------------------+        +----------+
  
  Generated files include:
  - Message structs (from protobuf-c)
  - Pack/unpack functions
  - Service method signatures
  - Client request functions
  - Server callback registration
```

**图解说明 (Diagram Explanation):**

编译时生成 vs 运行时反射性能对比：

| 运行时反射（JSON） | 编译时生成（Protobuf） |
|--------------------|------------------------|
| 解析JSON字符串 | 直接访问结构体字段 |
| 字符串比较查找字段 | 指针解引用 |
| 类型强制转换 | 编译器已知类型 |
| **~1000 CPU周期/字段** | **~1 CPU周期/字段** |

**快1000倍！** 代码生成产物：消息结构体、pack/unpack函数、服务方法签名、客户端请求函数

### 1.4 Why gRPC Favors Explicit Structure Over Convenience

```
+=============================================================================+
|                    EXPLICIT VS CONVENIENT TRADE-OFFS                        |
+=============================================================================+

  Convenience-First Design (e.g., dynamic languages):
  ---------------------------------------------------
  
  client.call("SayHello", {"name": "alice"})  // Easy to write
       |
       +-- What if method name is wrong? Runtime error.
       +-- What if field is misspelled? Silent failure.
       +-- What if type is wrong? Undefined behavior.
  
  Explicit-First Design (gRPC):
  -----------------------------
  
  // You MUST use the generated types
  Foo__HelloRequest request = FOO__HELLO_REQUEST__INIT;
  request.name = "alice";
  
  // You MUST handle the return value
  int status = foo__greeter__say_hello(client, &request, ...);
  
  // You MUST clean up resources
  grpc_c_client_free(client);
  
  Errors caught at: COMPILE TIME
  
  The grpc-c Philosophy in Code:
  ------------------------------
  
  // Explicit initialization
  grpc_c_init(GRPC_THREADS, NULL);  // Must specify mode
  
  // Explicit resource creation
  grpc_c_server_t *server = grpc_c_server_create(name, NULL, NULL);
  
  // Explicit registration (from service.c)
  grpc_c_register_method(server, method_name, 
                         client_streaming, server_streaming,
                         handler,
                         input_packer, input_unpacker, input_free,
                         output_packer, output_unpacker, output_free);
  
  // Explicit lifecycle (from foo_server.c)
  grpc_c_server_start(server);
  grpc_c_server_wait(server);     // Explicit blocking
  grpc_c_server_destroy(server);  // Explicit cleanup
  
  Why This Matters:
  - Every resource has clear ownership
  - Every operation has explicit outcome
  - Debuggers can trace the exact flow
  - Memory profilers see real allocations
```

**图解说明 (Diagram Explanation):**

显式 vs 便利的权衡：

| 便利优先（动态语言） | 显式优先（gRPC） |
|----------------------|------------------|
| `client.call("SayHello", {"name": "alice"})` | 必须用生成的类型 |
| 方法名错误 → 运行时错误 | 编译时捕获 |
| 字段拼写错误 → 静默失败 | 编译时捕获 |
| 类型错误 → 未定义行为 | 编译时捕获 |

grpc-c显式模式：`grpc_c_init()` → `grpc_c_server_create()` → `grpc_c_register_method()` → `grpc_c_server_start()` → `grpc_c_server_destroy()`

---

## 2. Layered Architecture

```
+=============================================================================+
|                    gRPC LAYERED ARCHITECTURE                                |
+=============================================================================+

  +-----------------------------------------------------------------------+
  |  LAYER 1: APPLICATION / BUSINESS LOGIC                                |
  |                                                                       |
  |  void foo__greeter__say_hello_cb(grpc_c_context_t *context) {         |
  |      // YOUR CODE HERE                                                |
  |      Foo__HelloRequest *request;                                      |
  |      context->gcc_stream->read(context, (void**)&request, 0, -1);     |
  |      // Process request, build response                               |
  |      context->gcc_stream->write(context, &response, 0, -1);           |
  |  }                                                                    |
  |                                                                       |
  |  OWNS: Business logic, validation, orchestration                      |
  |  DOES NOT OWN: Serialization, transport, threading                    |
  +-----------------------------------------------------------------------+
                                    |
                                    v
  +-----------------------------------------------------------------------+
  |  LAYER 2: GENERATED STUB LAYER (from .proto)                          |
  |                                                                       |
  |  // Generated by protoc-gen-grpc-c                                    |
  |  void foo__greeter__service_init(grpc_c_server_t *server) {           |
  |      grpc_c_methods_alloc(server, 1);                                 |
  |      grpc_c_register_method(server, "/foo.Greeter/SayHello", ...);    |
  |  }                                                                    |
  |                                                                       |
  |  OWNS: Method registration, packer/unpacker wiring                    |
  |  DOES NOT OWN: RPC mechanics, transport                               |
  +-----------------------------------------------------------------------+
                                    |
                                    v
  +-----------------------------------------------------------------------+
  |  LAYER 3: grpc-c PUBLIC API                                           |
  |                                                                       |
  |  Key Types: grpc_c_client_t, grpc_c_server_t, grpc_c_context_t        |
  |                                                                       |
  |  Key Functions:                                                       |
  |  - grpc_c_client_init()      / grpc_c_client_free()                   |
  |  - grpc_c_server_create()    / grpc_c_server_destroy()                |
  |  - grpc_c_server_start()     / grpc_c_server_wait()                   |
  |  - context->gcc_stream->read() / write() / finish()                   |
  |                                                                       |
  |  OWNS: C-friendly API, resource lifecycle, metadata                   |
  |  DOES NOT OWN: Core RPC implementation                                |
  +-----------------------------------------------------------------------+
                                    |
                                    v
  +-----------------------------------------------------------------------+
  |  LAYER 4: gRPC CORE (libgrpc)                                         |
  |                                                                       |
  |  Key Types: grpc_channel, grpc_call, grpc_completion_queue            |
  |                                                                       |
  |  Key Functions:                                                       |
  |  - grpc_channel_create()                                              |
  |  - grpc_call_start_batch()                                            |
  |  - grpc_completion_queue_next() / grpc_completion_queue_pluck()       |
  |                                                                       |
  |  OWNS: Call lifecycle, operation batching, flow control               |
  |  DOES NOT OWN: Wire format, TCP sockets                               |
  +-----------------------------------------------------------------------+
                                    |
                                    v
  +-----------------------------------------------------------------------+
  |  LAYER 5: HTTP/2 TRANSPORT                                            |
  |                                                                       |
  |  - Frame parsing and generation                                       |
  |  - HPACK header compression                                           |
  |  - Stream multiplexing                                                |
  |  - Flow control windows                                               |
  |                                                                       |
  |  OWNS: HTTP/2 protocol, framing, header encoding                      |
  |  DOES NOT OWN: TCP connection, TLS                                    |
  +-----------------------------------------------------------------------+
                                    |
                                    v
  +-----------------------------------------------------------------------+
  |  LAYER 6: OS / TCP LAYER                                              |
  |                                                                       |
  |  - Socket creation and management                                     |
  |  - TCP connection lifecycle                                           |
  |  - TLS/SSL handshake                                                  |
  |  - Kernel buffers and interrupts                                      |
  |                                                                       |
  |  OWNS: Network I/O, encryption, OS integration                        |
  +-----------------------------------------------------------------------+

  Data Flow Example (Unary RPC):
  ==============================
  
  CLIENT                                          SERVER
  ------                                          ------
  
  [App] HelloRequest{name: "alice"}
     |
     v
  [Stub] foo__greeter__say_hello()
     |
     v
  [grpc-c] grpc_c_client_request_unary()
     |
     v
  [Core] grpc_call_start_batch(SEND_MESSAGE)
     |
     v
  [HTTP/2] DATA frame: [stream_id][flags][protobuf_bytes]
     |
     v
  [TCP] -----------------NETWORK------------------>
                                                  |
                                                  v
                                               [TCP]
                                                  |
                                                  v
                                               [HTTP/2] parse DATA frame
                                                  |
                                                  v
                                               [Core] GRPC_OP_COMPLETE
                                                  |
                                                  v
                                               [grpc-c] gc_handle_server_event()
                                                  |
                                                  v
                                               [Stub] invoke say_hello_cb
                                                  |
                                                  v
                                               [App] Process request
```

**图解说明 (Diagram Explanation):**

gRPC六层架构及各层职责：

| 层次 | 拥有 | 不拥有 |
|------|------|--------|
| 应用层 | 业务逻辑、验证 | 序列化、传输、线程 |
| Stub层 | 方法注册、打包器 | RPC机制 |
| grpc-c API | C友好接口、生命周期 | 核心RPC实现 |
| gRPC Core | 调用生命周期、操作批处理 | 线格式、TCP |
| HTTP/2传输 | 帧解析、HPACK、多路复用 | TLS、TCP连接 |
| OS/TCP层 | 网络I/O、加密、系统集成 | - |

数据流：App→Stub→grpc-c→Core→HTTP/2→TCP→网络→...→服务端逆向处理

---

## 3. Communication and Execution Model

### 3.1 Client-Server Responsibilities

```
+=============================================================================+
|                    CLIENT-SERVER RESPONSIBILITY MATRIX                      |
+=============================================================================+

  +---------------------------+---------------------------+
  |         CLIENT            |          SERVER           |
  +---------------------------+---------------------------+
  | Initiate connection       | Listen on port            |
  | Create channel            | Register methods          |
  | Create call               | Accept calls              |
  | Send request              | Receive request           |
  | Receive response          | Process & send response   |
  | Handle status             | Send status               |
  | Manage deadline           | Respect deadline          |
  | Cancel if needed          | Detect cancellation       |
  +---------------------------+---------------------------+

  In grpc-c Code:
  ---------------
  
  CLIENT (foo_client.c):
  +--------------------------------------------+
  | grpc_c_init(GRPC_THREADS, NULL);           | // Initialize
  | client = grpc_c_client_init(host, id, ..); | // Create channel
  | foo__greeter__say_hello(client, ...);      | // Make call
  | // Response received in output param       |
  +--------------------------------------------+

  SERVER (foo_server.c):
  +--------------------------------------------+
  | grpc_c_init(GRPC_THREADS, NULL);           | // Initialize
  | server = grpc_c_server_create(name, ...);  | // Create server
  | foo__greeter__service_init(server);        | // Register methods
  | grpc_c_server_start(server);               | // Start listening
  | grpc_c_server_wait(server);                | // Process calls
  +--------------------------------------------+
```

**图解说明 (Diagram Explanation):**

客户端与服务端职责对比：

| 客户端 | 服务端 |
|--------|--------|
| 发起连接 | 监听端口 |
| 创建Channel | 注册方法 |
| 创建Call | 接受Call |
| 发送请求 | 接收请求 |
| 接收响应 | 处理并发送响应 |
| 管理截止时间 | 遵守截止时间 |

### 3.2 RPC Lifecycle

```
+=============================================================================+
|                    RPC LIFECYCLE STATE MACHINE                              |
+=============================================================================+

  CLIENT SIDE                              SERVER SIDE
  ===========                              ===========
  
      +---------------+                    +---------------+
      |  IDLE         |                    |  LISTENING    |
      +-------+-------+                    +-------+-------+
              |                                    |
              | create_call()                      | request_call()
              v                                    v
      +---------------+                    +---------------+
      |  PENDING      |                    |  WAITING      |
      +-------+-------+                    +-------+-------+
              |                                    |
              | start_batch(SEND_INITIAL_METADATA) | receive metadata
              | start_batch(SEND_MESSAGE)          |
              v                                    v
      +---------------+                    +---------------+
      |  ACTIVE       | -----------------> |  ACTIVE       |
      |               |     (request)      |               |
      +-------+-------+                    +-------+-------+
              |                                    |
              | RECV_MESSAGE                       | SEND_MESSAGE
              |                                    |
              v                                    v
      +---------------+                    +---------------+
      |  ACTIVE       | <----------------- |  ACTIVE       |
      |               |     (response)     |               |
      +-------+-------+                    +-------+-------+
              |                                    |
              | RECV_STATUS                        | SEND_STATUS
              v                                    v
      +---------------+                    +---------------+
      |  CLOSED       |                    |  CLOSED       |
      +---------------+                    +---------------+

  grpc-c State Enum (from grpc-c.h):
  ----------------------------------
  
  typedef enum grpc_c_state_s {
      GRPC_C_OP_START,              // Started executing operations
      GRPC_C_SERVER_CALLBACK_WAIT,  // Waiting to call RPC handler
      GRPC_C_SERVER_CALLBACK_START, // Called RPC handler
      GRPC_C_SERVER_CALLBACK_DONE,  // RPC handler returned
      GRPC_C_SERVER_CONTEXT_CLEANUP,// Context being cleaned
      GRPC_C_READ_DATA_START,       // Started reading
      GRPC_C_READ_DATA_DONE,        // Finished reading
      GRPC_C_WRITE_DATA_START,      // Started writing
      GRPC_C_WRITE_DATA_DONE,       // Finished writing
      GRPC_C_CLIENT_START,          // Client started RPC
      GRPC_C_CLIENT_DONE,           // Client finished RPC
  } grpc_c_state_t;
```

**图解说明 (Diagram Explanation):**

RPC状态机流程：

```
客户端：IDLE → PENDING → ACTIVE → CLOSED
服务端：LISTENING → WAITING → ACTIVE → CLOSED
```

grpc-c状态枚举覆盖完整生命周期：`GRPC_C_OP_START`（操作开始）→ `GRPC_C_SERVER_CALLBACK_*`（回调阶段）→ `GRPC_C_READ/WRITE_*`（数据传输）→ `GRPC_C_CLIENT_DONE`（完成）

### 3.3 Unary vs Streaming RPCs

```
+=============================================================================+
|                    RPC TYPES AND DATA FLOW                                  |
+=============================================================================+

  UNARY RPC (1:1)
  ===============
  
  Client ----[1 Request]----> Server
  Client <---[1 Response]---- Server
  
  rpc SayHello(HelloRequest) returns (HelloReply);
  
  CLIENT                                  SERVER
  ------                                  ------
  send(request)  -----------------------> read()
  recv()        <------------------------ write(response)
                <------------------------ finish(status)

  SERVER STREAMING (1:N)
  ======================
  
  Client ----[1 Request]----> Server
  Client <---[N Responses]--- Server
  
  rpc ListFeatures(Rectangle) returns (stream Feature);
  
  CLIENT                                  SERVER
  ------                                  ------
  send(request)  -----------------------> read()
  recv()        <------------------------ write(response1)
  recv()        <------------------------ write(response2)
  recv()        <------------------------ write(responseN)
  recv()        <------------------------ finish(status)

  CLIENT STREAMING (N:1)
  ======================
  
  Client ----[N Requests]---> Server
  Client <---[1 Response]---- Server
  
  rpc RecordRoute(stream Point) returns (RouteSummary);
  
  CLIENT                                  SERVER
  ------                                  ------
  send(request1) -----------------------> read()
  send(request2) -----------------------> read()
  send(requestN) -----------------------> read()
  write_done()   -----------------------> (EOF detected)
  recv()        <------------------------ write(response)
                <------------------------ finish(status)

  BIDIRECTIONAL STREAMING (N:M)
  ============================
  
  Client <---[Messages]---> Server
  
  rpc RouteChat(stream RouteNote) returns (stream RouteNote);
  
  CLIENT                                  SERVER
  ------                                  ------
  send(msg1)     -----------------------> read()
                <------------------------ write(resp1)
  send(msg2)     -----------------------> read()
  recv()        <------------------------ write(resp2)
  send(msg3)     -----------------------> read()
  recv()        <------------------------ write(resp3)
  write_done()   -----------------------> (EOF detected)
  recv()        <------------------------ finish(status)

  grpc-c Stream Handler (from grpc-c.h):
  --------------------------------------
  
  struct grpc_c_stream_handler_s {
      grpc_c_stream_read_t *read;           // Read from stream
      grpc_c_stream_write_t *write;         // Write to stream
      grpc_c_stream_write_done_t *write_done; // Signal write complete
      grpc_c_stream_finish_t *finish;       // Get/send status
  };
```

**图解说明 (Diagram Explanation):**

四种RPC类型对比：

| 类型 | 请求:响应 | 典型用例 |
|------|-----------|----------|
| Unary | 1:1 | 简单查询 |
| Server Streaming | 1:N | 列表查询、日志流 |
| Client Streaming | N:1 | 批量上传、聚合 |
| Bidirectional | N:M | 聊天、实时协作 |

grpc-c stream handler提供统一接口：`read()`、`write()`、`write_done()`、`finish()`

### 3.4 Synchronous vs Asynchronous APIs

```
+=============================================================================+
|                    SYNC VS ASYNC EXECUTION MODELS                           |
+=============================================================================+

  SYNCHRONOUS (grpc_c_client_request_unary)
  =========================================
  
  Thread 1 (Caller)
  -----------------
  |                                    |
  | grpc_c_client_request_unary()      |
  |    |                               |
  |    +-- grpc_call_start_batch()     | <-- Submit to kernel
  |    |                               |
  |    +-- grpc_completion_queue_pluck | <-- BLOCKS until complete
  |    |        (blocks here)          |
  |    |            .                  |
  |    |            .                  |
  |    |            .                  |
  |    +-- return result               |
  |                                    |
  v                                    v
  
  Thread is BLOCKED while waiting for network!

  ASYNCHRONOUS (grpc_c_client_request_async)
  ==========================================
  
  Thread 1 (Caller)          Thread 2 (Worker Pool)
  -----------------          ---------------------
  |                          |
  | request_async()          |
  |    |                     |
  |    +-- start_batch()     |
  |    |                     |
  |    +-- thread_pool_add() | -----> gc_run_rpc()
  |    |                     |           |
  |    +-- RETURN IMMEDIATELY|           +-- cq_next() [BLOCKS]
  |                          |           |
  | (do other work)          |           +-- callback(result)
  |                          |           |
  v                          v           v
  
  Caller thread FREE to do other work!

  grpc-c Implementation (from client.c):
  --------------------------------------
  
  // Sync version - blocks
  ev = grpc_completion_queue_pluck(context->gcc_cq, context, tout, NULL);
  if (ev.type == GRPC_QUEUE_TIMEOUT) {
      // Handle timeout
  }
  
  // Async version - callback
  if (grpc_c_get_thread_pool()) {
      grpc_c_thread_pool_add(grpc_c_get_thread_pool(), gc_run_rpc, 
                             context->gcc_cq);
  }
  
  // gc_run_rpc runs in background:
  static void gc_run_rpc(void *arg) {
      grpc_completion_queue *cq = (grpc_completion_queue *)arg;
      gc_handle_client_event_internal(cq, gpr_inf_future(GPR_CLOCK_REALTIME));
      // When complete, invokes user callback
  }
```

**图解说明 (Diagram Explanation):**

同步 vs 异步执行模型：

| 同步（sync） | 异步（async） |
|--------------|---------------|
| 调用线程阻塞等待 | 立即返回 |
| `grpc_completion_queue_pluck()` | `thread_pool_add()` + `cq_next()` |
| 网络I/O期间线程被占用 | 调用线程可做其他工作 |
| 简单但不可扩展 | 复杂但高并发友好 |

grpc-c实现：同步版用`pluck()`阻塞；异步版通过线程池在后台`gc_run_rpc()`轮询CQ

---

## 4. Concurrency and Execution

### 4.1 Why Completion Queues?

```
+=============================================================================+
|                    COMPLETION QUEUE ARCHITECTURE                            |
+=============================================================================+

  The Problem: Thread-Per-Request
  ================================
  
  1000 concurrent RPCs = 1000 threads
  
  +--------+  +--------+  +--------+       +--------+
  | Thread |  | Thread |  | Thread |  ...  | Thread |
  |   1    |  |   2    |  |   3    |       |  1000  |
  +--------+  +--------+  +--------+       +--------+
       |           |           |                |
       | blocked   | blocked   | blocked        | blocked
       v           v           v                v
  [waiting]   [waiting]   [waiting]   ...  [waiting]
  
  Memory: 1000 × 1MB stack = 1GB RAM just for stacks!
  Context switches: Thousands per second
  Cache thrashing: Terrible

  The Solution: Completion Queue
  ================================
  
  +-------------------+
  |   Completion      |
  |     Queue         | <-- Central event hub
  +-------------------+
     ^   ^   ^   ^
     |   |   |   |
  [evt][evt][evt][evt]  <-- Events from all operations
  
  +--------+  +--------+  +--------+  +--------+
  | Worker |  | Worker |  | Worker |  | Worker |
  |   1    |  |   2    |  |   3    |  |   4    |
  +--------+  +--------+  +--------+  +--------+
       |           |           |           |
       +-----+-----+-----+-----+-----------+
             |
             v
       cq_next() polls for ANY ready event
  
  4 threads handle 1000 concurrent RPCs!
  Memory: 4 × 1MB = 4MB
  Efficiency: ~99%

**图解说明 (Diagram Explanation):**

完成队列解决的问题：

| 线程每请求模式 | 完成队列模式 |
|----------------|--------------|
| 1000并发RPC = 1000线程 | 1000并发RPC = 4线程 |
| 1GB RAM（仅栈空间） | 4MB RAM |
| 大量上下文切换 | 事件驱动，高效率 |
| 10K连接时OOM | 轻松扩展到100K+ |

  Completion Queue Flow in grpc-c:
  ================================
  
  1. SUBMIT OPERATION
  -------------------
  grpc_call_start_batch(call, ops, op_count, tag, NULL);
                                               |
                                               v
                                        +-------------+
                                        | CQ receives |
                                        | event when  |
                                        | complete    |
                                        +-------------+
  
  2. POLL FOR COMPLETION
  ----------------------
  ev = grpc_completion_queue_next(cq, deadline, NULL);
       |
       v
  switch (ev.type) {
      case GRPC_OP_COMPLETE:
          context = (grpc_c_context_t *)ev.tag;
          // Process result
          break;
      case GRPC_QUEUE_TIMEOUT:
          // No events ready
          break;
      case GRPC_QUEUE_SHUTDOWN:
          // Queue is shutting down
          break;
  }
  
  3. PLUCK SPECIFIC EVENT
  -----------------------
  ev = grpc_completion_queue_pluck(cq, specific_tag, deadline, NULL);
       |
       v
  // Returns when THIS tag's operation completes
  // Used for synchronous APIs
```

### 4.2 Event-Driven vs Thread-Per-Request

```
+=============================================================================+
|                    CONCURRENCY MODEL COMPARISON                             |
+=============================================================================+

  THREAD-PER-REQUEST MODEL
  ========================
  
       +----------+
       | Listener |
       +----+-----+
            |
            | accept()
            v
       +----+-----+     +----------+     +----------+
       | spawn    |---->| Thread 1 |     | Thread 2 |
       | thread   |     +----+-----+     +----+-----+
       +----------+          |                |
                             v                v
                        [handle()]       [handle()]
                             |                |
                             v                v
                        [blocked on I/O] [blocked on I/O]
  
  Scaling: O(connections) threads
  Memory: O(connections) × stack_size
  At 10K connections: OOM or kernel limits

  EVENT-DRIVEN MODEL (gRPC)
  =========================
  
       +-----------------+
       |   Event Loop    | <-- Single or few threads
       +--------+--------+
                |
         +------+------+
         |      |      |
         v      v      v
       [CQ1]  [CQ2]  [CQ3]  <-- Completion queues
         |      |      |
         +------+------+
                |
                v
       +--------+--------+
       |  Worker Pool    | <-- Fixed number of threads
       | [T1][T2][T3][T4]|
       +-----------------+
  
  Scaling: O(1) threads
  Memory: Fixed
  At 10K connections: Just more events in queue

**图解说明 (Diagram Explanation):**

两种并发模型对比：

- **线程每请求**：`accept() → spawn thread → handle() → blocked on I/O`，线程数 = O(连接数)
- **事件驱动**：固定工作线程池轮询CQ，连接数增加只增加队列事件，线程数 = O(1)

  grpc-c Thread Pool (from thread_pool.c):
  ========================================
  
  grpc_c_thread_pool_t *
  grpc_c_thread_pool_create(int num_threads) {
      // Create fixed pool of worker threads
      // Each thread runs event loop
  }
  
  void
  grpc_c_thread_pool_add(pool, callback, arg) {
      // Queue work item for any available thread
      // Non-blocking
  }
```

### 4.3 Thread Ownership and Callback Execution

```
+=============================================================================+
|                    THREAD OWNERSHIP MODEL                                   |
+=============================================================================+

  WHO OWNS THE THREADS?
  =====================
  
  +---------------------------+-----------------------------+
  | Component                 | Thread Ownership            |
  +---------------------------+-----------------------------+
  | Application               | May create own threads      |
  | grpc-c                    | Creates thread pool         |
  | gRPC Core                 | Uses provided CQs           |
  | OS                        | Owns I/O completion         |
  +---------------------------+-----------------------------+

  WHERE DO CALLBACKS EXECUTE?
  ===========================
  
  Scenario 1: Synchronous Client (foo_client.c)
  ---------------------------------------------
  
  main() thread:
  |
  +-- grpc_c_client_request_unary()
  |       |
  |       +-- [blocks on CQ pluck]
  |       |
  |       +-- returns with result
  |
  +-- process result (still main thread)
  
  Callback: NONE (result returned directly)

  Scenario 2: Asynchronous Client
  --------------------------------
  
  main() thread:                    worker thread:
  |                                 |
  +-- grpc_c_client_request_async() |
  |       |                         |
  |       +-- [returns immediately] |
  |                                 |
  +-- do other work                 +-- gc_run_rpc()
  |                                 |       |
  |                                 |       +-- [blocks on CQ]
  |                                 |       |
  |                                 |       +-- callback(result)
  |                                 |              |
  |                                 |              v
  |                                 |       [USER CODE RUNS HERE]
  
  Callback: Runs on WORKER thread, not main!

  Scenario 3: Server (foo_server.c)
  ---------------------------------
  
  main() thread:             worker pool:
  |                          |
  +-- grpc_c_server_start()  |
  |                          |
  +-- grpc_c_server_wait()   +-- gc_schedule_callback()
  |       [blocks]           |       |
  |                          |       +-- gc_run_rpc()
  |                          |              |
  |                          |              +-- [CQ event received]
  |                          |              |
  |                          |              +-- foo__say_hello_cb()
  |                          |                    |
  |                          |                    v
  |                          |              [YOUR HANDLER HERE]
  
  Handler: Runs on WORKER thread

  CRITICAL IMPLICATIONS:
  ======================
  
  1. Don't block in callbacks (starves thread pool)
  2. Use mutex for shared state (callbacks can race)
  3. Don't assume main thread (for GUI/reactor patterns)
  4. Callback can be called after "async" function returns
```

**图解说明 (Diagram Explanation):**

线程所有权与回调执行位置：

| 场景 | 回调执行线程 |
|------|--------------|
| 同步客户端 | 无回调，结果直接返回main线程 |
| 异步客户端 | **工作线程**（非main线程！）|
| 服务端处理器 | **工作线程** |

**关键影响**：
1. 不要在回调中阻塞（会饿死线程池）
2. 共享状态需要mutex保护（回调可能并发）
3. 异步回调可能在函数返回后才执行

---

## 5. Resource and Lifecycle Management

### 5.1 Object Lifetimes

```
+=============================================================================+
|                    OBJECT LIFETIME DIAGRAM                                  |
+=============================================================================+

  +------------------------------------------------------------------+
  |                     GRPC-C OBJECT HIERARCHY                       |
  +------------------------------------------------------------------+
  
  grpc_c_init()
  |
  +-- grpc_c_client_t              grpc_c_server_t
  |   |                            |
  |   +-- gcc_channel              +-- gcs_server (grpc_server)
  |   |                            |
  |   +-- gcc_cq                   +-- gcs_cq (completion queue)
  |   |                            |
  |   +-- gcc_context_list  <-->   +-- gcs_contexts[]
  |       |                            |
  |       v                            v
  |   grpc_c_context_t             grpc_c_context_t
  |   |                            |
  |   +-- gcc_call                 +-- gcc_call
  |   +-- gcc_cq                   +-- gcc_cq
  |   +-- gcc_ops[]                +-- gcc_ops[]
  |   +-- gcc_metadata             +-- gcc_metadata
  |   +-- gcc_stream               +-- gcc_stream
  |
  grpc_c_shutdown()

  LIFETIME RULES:
  ===============
  
  +-------------------+----------------------------------+
  | Object            | Lifetime                         |
  +-------------------+----------------------------------+
  | grpc_c_client_t   | grpc_c_client_init() to          |
  |                   | grpc_c_client_free()             |
  +-------------------+----------------------------------+
  | grpc_c_server_t   | grpc_c_server_create() to        |
  |                   | grpc_c_server_destroy()          |
  +-------------------+----------------------------------+
  | grpc_c_context_t  | Implicitly created per RPC,      |
  |                   | freed after finish() or on error |
  +-------------------+----------------------------------+
  | grpc_channel      | Owned by grpc_c_client_t         |
  +-------------------+----------------------------------+
  | grpc_call         | Owned by grpc_c_context_t        |
  +-------------------+----------------------------------+
  | grpc_cq           | One per client/server + per call |
  +-------------------+----------------------------------+
```

**图解说明 (Diagram Explanation):**

grpc-c对象层次结构：

```
grpc_c_init()
├── grpc_c_client_t ─── gcc_channel, gcc_cq, gcc_context_list
│   └── grpc_c_context_t ─── gcc_call, gcc_cq, gcc_ops[], gcc_metadata
└── grpc_c_server_t ─── gcs_server, gcs_cq, gcs_contexts[]
    └── grpc_c_context_t ─── gcc_call, gcc_cq, gcc_ops[], gcc_metadata
```

生命周期规则：Client从`init`到`free`，Server从`create`到`destroy`，Context隐式创建，RPC结束释放

### 5.2 Reference Counting

```
+=============================================================================+
|                    REFERENCE COUNTING IN grpc-c                             |
+=============================================================================+

  Event Reference Counting:
  ========================
  
  typedef struct grpc_c_event_s {
      grpc_c_event_type_t gce_type;
      void *gce_data;
      int gce_refcount;      // <-- Reference count
  } grpc_c_event_t;
  
  Flow:
  -----
  
  1. Start operation:
     context->gcc_event.gce_refcount++;
     grpc_call_start_batch(..., &context->gcc_event, ...);
  
  2. Receive completion:
     ev = grpc_completion_queue_next(cq, ...);
     gcev = (grpc_c_event_t *)ev.tag;
     // ... handle event ...
     gcev->gce_refcount--;
  
  3. Safe to free when:
     if (gcev->gce_refcount == 0) {
         // Safe to free context
     }

  Callback Reference Counting:
  ============================
  
  // From client.c
  gpr_mu_lock(&client->gcc_lock);
  client->gcc_running_cb++;        // Increment on start
  gpr_mu_unlock(&client->gcc_lock);
  
  // ... callback executes ...
  
  gpr_mu_lock(&client->gcc_lock);
  client->gcc_running_cb--;        // Decrement on complete
  if (client->gcc_running_cb == 0 && client->gcc_shutdown) {
      gpr_cv_signal(&client->gcc_shutdown_cv);
  }
  gpr_mu_unlock(&client->gcc_lock);
  
  Prevents:
  - Client freed while callbacks running
  - Server shutdown with pending RPCs
```

**图解说明 (Diagram Explanation):**

引用计数机制：

```c
// 事件引用计数
gce_refcount++ → 提交操作
gce_refcount-- → 完成事件
if (gce_refcount == 0) → 安全释放context

// 回调引用计数
gcc_running_cb++ → 回调开始
gcc_running_cb-- → 回调结束
if (gcc_running_cb == 0 && shutdown) → 通知关闭等待者
```

防止：客户端在回调运行时被释放、服务端在有pending RPC时关闭

### 5.3 Shutdown Semantics

```
+=============================================================================+
|                    EXPLICIT SHUTDOWN PROTOCOL                               |
+=============================================================================+

  CLIENT SHUTDOWN (grpc_c_client_free):
  =====================================
  
  void grpc_c_client_free(grpc_c_client_t *client) {
      //
      // STEP 1: Signal shutdown intent
      //
      client->gcc_shutdown = 1;
      
      //
      // STEP 2: Wait for running callbacks
      //
      gpr_mu_lock(&client->gcc_lock);
      while (client->gcc_running_cb > 0 || client->gcc_wait) {
          gpr_cv_wait(&client->gcc_shutdown_cv, &client->gcc_lock, ...);
      }
      gpr_mu_unlock(&client->gcc_lock);
      
      //
      // STEP 3: Free active contexts
      //
      while (!LIST_EMPTY(&client->gcc_context_list_head)) {
          ctx = LIST_FIRST(&client->gcc_context_list_head);
          grpc_c_context_free(ctx);
      }
      
      //
      // STEP 4: Destroy channel
      //
      grpc_channel_destroy(client->gcc_channel);
      
      //
      // STEP 5: Shutdown and drain completion queue
      //
      grpc_completion_queue_shutdown(client->gcc_cq);
      while (grpc_completion_queue_next(cq, ...).type != GRPC_QUEUE_SHUTDOWN)
          ;
      grpc_completion_queue_destroy(client->gcc_cq);
      
      //
      // STEP 6: Free client struct
      //
      gpr_free(client);
  }

  SERVER SHUTDOWN (grpc_c_server_destroy):
  ========================================
  
       [Running Server]
              |
              v
       grpc_c_server_destroy()
              |
              +-- server->gcs_shutdown = 1
              |
              +-- Wait for all callbacks
              |       while (server->gcs_running_cb > 0)
              |           wait()
              |
              +-- grpc_server_shutdown_and_notify()
              |       |
              |       v
              |   [Reject new calls]
              |   [Finish pending calls]
              |
              +-- grpc_completion_queue_shutdown()
              |       |
              |       v
              |   [Drain all events]
              |
              +-- grpc_server_destroy()
              |
              +-- Free all resources
              |
              v
       [Server Destroyed]

  WHY EXPLICIT SHUTDOWN MATTERS:
  ==============================
  
  Without explicit shutdown:
  
  1. Memory leaks (allocated buffers never freed)
  2. Orphaned threads (workers still running)
  3. Kernel resources (sockets, file descriptors)
  4. Remote side hangs (never receives close)
  
  gRPC requires YOU to:
  - Call grpc_c_client_free()
  - Call grpc_c_server_destroy()
  - Handle SIGINT for graceful shutdown
```

**图解说明 (Diagram Explanation):**

显式关闭协议（以客户端为例）：

1. 设置`gcc_shutdown = 1`（信号关闭意图）
2. 等待`gcc_running_cb == 0`（回调全部完成）
3. 释放所有context
4. `grpc_channel_destroy()`（销毁Channel）
5. `grpc_completion_queue_shutdown()` + 排空CQ
6. `grpc_completion_queue_destroy()` + `gpr_free()`

**为何显式关闭重要**：防止内存泄漏、孤儿线程、内核资源泄漏、远端挂起

---

## 中文说明

### gRPC设计哲学详解

#### 1. IDL优先开发

**核心理念：**
- 先定义接口（.proto文件），再写代码
- 所有客户端和服务端都从同一份定义生成
- 编译时保证类型匹配，而非运行时发现错误

**工作流程：**
```
foo.proto → protoc编译器 → foo.grpc-c.h + foo.grpc-c.c
                               ↓
                          客户端和服务端
                          都使用这些生成的代码
```

#### 2. 六层架构设计

| 层次 | 职责 | 不负责 |
|------|------|--------|
| 应用层 | 业务逻辑 | 序列化、传输 |
| 生成的Stub层 | 方法注册、打包解包 | RPC机制 |
| grpc-c公共API | C语言友好接口、生命周期 | 核心RPC实现 |
| gRPC Core | 调用生命周期、操作批处理 | 线格式、TCP |
| HTTP/2传输层 | 帧解析、头压缩、多路复用 | TCP连接 |
| OS/TCP层 | 网络I/O、加密 | - |

#### 3. 完成队列（Completion Queue）机制

**为什么不用线程池？**
- 1000个并发RPC需要1000个线程 = 1GB内存（仅栈空间）
- 大量上下文切换、缓存失效

**完成队列方案：**
- 4个工作线程处理1000个并发RPC
- 所有操作异步提交，事件到达时统一处理
- 内存使用固定，扩展性极佳

**关键API：**
```c
// 提交操作
grpc_call_start_batch(call, ops, count, tag, NULL);

// 等待任意事件
ev = grpc_completion_queue_next(cq, deadline, NULL);

// 等待特定事件（同步API使用）
ev = grpc_completion_queue_pluck(cq, specific_tag, deadline, NULL);
```

#### 4. 同步与异步API对比

| 同步API | 异步API |
|---------|---------|
| 调用线程阻塞等待结果 | 调用立即返回 |
| 结果通过返回值获得 | 结果通过回调获得 |
| 简单但不可扩展 | 复杂但高并发友好 |
| 适合简单客户端 | 适合高性能服务端 |

#### 5. 资源生命周期管理

**关键原则：**
- 每个资源有明确的创建和销毁函数
- 引用计数防止提前释放
- 显式关闭确保资源正确回收

**关闭顺序（服务端）：**
1. 设置关闭标志
2. 等待所有回调完成
3. 拒绝新请求，完成现有请求
4. 清空完成队列
5. 销毁服务器和资源
6. 释放内存

**为什么必须显式关闭？**
- 防止内存泄漏
- 确保远端收到正确关闭通知
- 释放操作系统资源（套接字、文件描述符）
- 避免孤儿线程
