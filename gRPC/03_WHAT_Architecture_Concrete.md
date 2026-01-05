# WHAT | Architecture and Concrete Forms

## 1. Core Architectural Patterns Used by gRPC

```
+=============================================================================+
|                    gRPC ARCHITECTURAL PATTERNS                              |
+=============================================================================+

  A. STATE MACHINES
  =================
  
  Every RPC is a state machine:
  
  CLIENT CALL STATE MACHINE:
  
       +--------+
       | IDLE   |
       +---+----+
           |
           | create_call()
           v
       +--------+
       | INIT   |
       +---+----+
           |
           | start_batch(SEND_*)
           v
       +--------+
       | ACTIVE |<----+
       +---+----+     |
           |          | more data
           |          |
           +----------+
           |
           | RECV_STATUS
           v
       +--------+
       | CLOSED |
       +--------+

  grpc-c Context States (from grpc-c.h):
  
  +--------------------------------+----------------------------------------+
  | State                          | Meaning                                |
  +--------------------------------+----------------------------------------+
  | GRPC_C_OP_START                | Started executing operations           |
  | GRPC_C_SERVER_CALLBACK_WAIT    | Waiting to call RPC handler            |
  | GRPC_C_SERVER_CALLBACK_START   | Inside RPC handler                     |
  | GRPC_C_SERVER_CALLBACK_DONE    | Handler returned                       |
  | GRPC_C_SERVER_CONTEXT_CLEANUP  | Being cleaned up                       |
  | GRPC_C_READ_DATA_START         | Read operation started                 |
  | GRPC_C_READ_DATA_DONE          | Read completed                         |
  | GRPC_C_WRITE_DATA_START        | Write operation started                |
  | GRPC_C_WRITE_DATA_DONE         | Write completed                        |
  | GRPC_C_CLIENT_START            | Client RPC started                     |
  | GRPC_C_CLIENT_DONE             | Client RPC finished                    |
  +--------------------------------+----------------------------------------+

  B. PROACTOR / EVENT-DRIVEN DESIGN
  ==================================
  
  Reactor (Traditional):
  ----------------------
  
  while (true) {
      events = poll(fds);  // Wait for ANY event
      for (event in events) {
          handler = lookup(event.fd);
          handler(event);   // Dispatch synchronously
      }
  }
  
  Problem: Handler runs on event loop thread, blocking others.
  
  Proactor (gRPC):
  ----------------
  
  // Initiate async operation
  grpc_call_start_batch(call, ops, count, tag, NULL);
  
  // OS/runtime handles I/O in background
  // ...
  
  // Later, receive completion
  ev = grpc_completion_queue_next(cq, timeout, NULL);
  if (ev.type == GRPC_OP_COMPLETE) {
      callback(ev.tag);  // Handler runs with result ready
  }
  
  Advantage: I/O happens asynchronously, handler gets completed result.
  
  grpc-c Implementation:
  
       +----------------+
       | Your Code      |
       | start_batch()  |
       +-------+--------+
               |
               | enqueue operation
               v
       +-------+--------+
       | gRPC Core      |
       | (async I/O)    |
       +-------+--------+
               |
               | completion event
               v
       +-------+--------+
       | Completion     |
       | Queue          |
       +-------+--------+
               |
               | cq_next()
               v
       +-------+--------+
       | Your Callback  |
       +----------------+

  C. FILTER CHAINS (INTERCEPTORS)
  ================================
  
  gRPC Core uses filter chains for extensibility:
  
  Request Flow:
  
  +----------+     +----------+     +----------+     +----------+
  | Client   | --> | Compress | --> |  Retry   | --> | HTTP/2   |
  | Filter   |     | Filter   |     | Filter   |     | Transport|
  +----------+     +----------+     +----------+     +----------+
  
  Response Flow:
  
  +----------+     +----------+     +----------+     +----------+
  | HTTP/2   | --> |  Retry   | --> | Decomp   | --> | Client   |
  | Transport|     | Filter   |     | Filter   |     | Filter   |
  +----------+     +----------+     +----------+     +----------+
  
  Each filter can:
  - Transform data
  - Short-circuit the chain
  - Add metadata
  - Handle errors

  D. OPAQUE HANDLES
  ==================
  
  grpc-c uses opaque pointers to hide implementation:
  
  // Public header (grpc-c.h)
  typedef struct grpc_c_client_s grpc_c_client_t;  // Opaque!
  
  // Internal implementation (client.c)
  struct grpc_c_client_s {
      grpc_channel *gcc_channel;
      grpc_completion_queue *gcc_cq;
      gpr_slice gcc_host;
      char *gcc_id;
      // ... internal state
  };
  
  Benefits:
  - ABI stability (internals can change)
  - Encapsulation (users can't access internals)
  - Compile firewall (faster builds)

  E. REFERENCE COUNTING (INSTEAD OF GC)
  ======================================
  
  grpc-c Event:
  
  typedef struct grpc_c_event_s {
      grpc_c_event_type_t gce_type;
      void *gce_data;
      int gce_refcount;  // <-- Reference count
  } grpc_c_event_t;
  
  Usage pattern:
  
  // On submit:
  context->gcc_event.gce_refcount++;
  grpc_call_start_batch(..., &context->gcc_event, ...);
  
  // On completion:
  gpr_mu_lock(context->gcc_lock);
  gcev->gce_refcount--;
  gpr_mu_unlock(context->gcc_lock);
  
  // Safe to free when:
  if (context->gcc_event.gce_refcount == 0) {
      grpc_c_context_free(context);
  }
```

**图解说明 (Diagram Explanation):**

gRPC核心架构模式：

| 模式 | 作用 | grpc-c体现 |
|------|------|------------|
| **状态机** | 跟踪RPC生命周期 | `grpc_c_state_t`枚举 |
| **Proactor** | 异步提交+完成通知 | `start_batch()` + `cq_next()` |
| **过滤器链** | 可扩展处理管道 | 压缩/重试/追踪过滤器 |
| **不透明句柄** | 隐藏实现细节 | `typedef struct xxx_s xxx_t` |
| **引用计数** | 无GC的资源管理 | `gce_refcount` |

状态机流程：`IDLE → INIT → ACTIVE ⟲ → CLOSED`

---

## 2. Key grpc-c Objects and Their Roles

```
+=============================================================================+
|                    grpc-c OBJECT DEEP DIVE                                  |
+=============================================================================+

  A. grpc_c_client_t - CLIENT CONNECTION HANDLE
  =============================================
  
  WHY IT EXISTS:
  - Represents a connection to a gRPC server
  - Manages underlying grpc_channel
  - Tracks all active RPCs for cleanup
  
  STRUCTURE (from grpc-c.h):
  
  struct grpc_c_client_s {
      grpc_channel *gcc_channel;         // Underlying HTTP/2 channel
      grpc_completion_queue *gcc_cq;     // Events for this client
      grpc_completion_queue *gcc_channel_connectivity_cq;  // Connect/disconnect
      gpr_slice gcc_host;                // Server hostname
      char *gcc_id;                      // Client identification
      int gcc_channel_state;             // CONNECTING, READY, etc.
      int gcc_connected;                 // Boolean connection status
      int gcc_shutdown;                  // Shutdown in progress
      gpr_mu gcc_lock;                   // Thread safety
      LIST_HEAD(...) gcc_context_list_head;  // Active RPCs
  };
  
  LIFECYCLE:
  
  grpc_c_client_init()    // Allocate, create channel, init CQ
        |
        v
  [Use for RPCs]          // Create calls via this client
        |
        v
  grpc_c_client_free()    // Wait for RPCs, drain CQ, destroy channel

  B. grpc_c_server_t - SERVER INSTANCE
  ====================================
  
  WHY IT EXISTS:
  - Listens for incoming connections
  - Manages registered RPC methods
  - Dispatches requests to handlers
  
  STRUCTURE:
  
  struct grpc_c_server_s {
      char *gcs_host;                    // Listen address
      grpc_server *gcs_server;           // Underlying gRPC server
      grpc_completion_queue *gcs_cq;     // Server completion queue
      grpc_c_method_funcs_t *gcs_method_funcs;  // Method callbacks
      LIST_HEAD(...) gcs_method_list_head;      // Registered methods
      int gcs_method_count;              // Number of methods
      grpc_c_context_t **gcs_contexts;   // Per-method waiting contexts
      int gcs_running_cb;                // Active callbacks
      int gcs_shutdown;                  // Shutdown flag
      gpr_mu gcs_lock;                   // Thread safety
  };
  
  LIFECYCLE:
  
  grpc_c_server_create()    // Allocate, create grpc_server
        |
        v
  grpc_c_register_method()  // For each RPC
        |
        v
  grpc_c_server_start()     // Start accepting calls
        |
        v
  grpc_c_server_wait()      // Block until shutdown
        |
        v
  grpc_c_server_destroy()   // Cleanup everything

  C. grpc_c_context_t - RPC EXECUTION CONTEXT
  ==========================================
  
  WHY IT EXISTS:
  - Holds ALL state for a single RPC
  - Both client and server RPCs use this
  - Contains the grpc_call, metadata, payload
  
  STRUCTURE (key fields):
  
  struct grpc_c_context_s {
      // Method info
      struct grpc_c_method_t *gcc_method;
      grpc_c_method_funcs_t *gcc_method_funcs;
      
      // Core gRPC objects
      grpc_call *gcc_call;               // The actual call
      grpc_completion_queue *gcc_cq;     // Call's CQ
      grpc_op *gcc_ops;                  // Pending operations
      grpc_byte_buffer *gcc_payload;     // Request/Response data
      
      // Metadata
      grpc_c_metadata_array_t *gcc_metadata;
      grpc_c_metadata_array_t *gcc_initial_metadata;
      grpc_c_metadata_array_t *gcc_trailing_metadata;
      
      // State tracking
      grpc_c_state_t gcc_state;          // Current RPC state
      grpc_status_code gcc_status;       // Final status
      int gcc_is_client;                 // Client or server context
      int gcc_call_cancelled;            // Cancellation flag
      
      // Handlers
      grpc_c_stream_handler_t *gcc_stream;  // Read/write/finish
      
      // Events
      grpc_c_event_t gcc_event;          // RPC init event
      grpc_c_event_t gcc_read_event;     // Read completion
      grpc_c_event_t gcc_write_event;    // Write completion
  };
  
  LIFECYCLE:
  
  grpc_c_context_init()     // Allocate, init metadata arrays
        |
        v
  [Used during RPC]         // Read/write operations
        |
        v
  grpc_c_context_free()     // Destroy call, free everything

  D. grpc_completion_queue - EVENT HUB
  ====================================
  
  WHY IT EXISTS:
  - Collects completion events from async operations
  - Enables multiplexing many calls onto few threads
  - Core of the event-driven model
  
  KEY OPERATIONS:
  
  // Create
  cq = grpc_completion_queue_create(NULL);
  
  // Submit work (result will arrive on this CQ)
  grpc_call_start_batch(call, ops, count, tag, NULL);
  
  // Wait for any completion
  ev = grpc_completion_queue_next(cq, deadline, NULL);
  
  // Wait for specific completion (sync operations)
  ev = grpc_completion_queue_pluck(cq, tag, deadline, NULL);
  
  // Shutdown sequence
  grpc_completion_queue_shutdown(cq);
  while (cq_next().type != GRPC_QUEUE_SHUTDOWN);
  grpc_completion_queue_destroy(cq);

  E. grpc_slice - EFFICIENT MEMORY BUFFER
  =======================================
  
  WHY IT EXISTS:
  - Avoids copying data when possible
  - Reference counted for safe sharing
  - Can wrap existing buffers
  
  OPERATIONS:
  
  // Create from string (copies)
  gpr_slice s = grpc_slice_from_copied_string("hello");
  
  // Create from static string (no copy)
  gpr_slice s = grpc_slice_from_static_string("hello");
  
  // Access data
  char *data = GPR_SLICE_START_PTR(s);
  size_t len = GPR_SLICE_LENGTH(s);
  
  // Release
  grpc_slice_unref(s);

  F. grpc_metadata - KEY-VALUE HEADERS
  ====================================
  
  WHY IT EXISTS:
  - HTTP/2 headers for RPC metadata
  - Client ID, auth tokens, tracing
  - Initial metadata (before data) and trailing (with status)
  
  USAGE IN grpc-c:
  
  // Add metadata
  grpc_c_add_metadata(context, "client-id", "my-client");
  grpc_c_add_initial_metadata(context, "auth-token", "xxx");
  grpc_c_add_trailing_metadata(context, "trace-id", "abc123");
  
  // Read metadata
  const char *client_id = grpc_c_get_metadata_by_key(context, "client-id");

  G. grpc_byte_buffer - WIRE DATA CONTAINER
  =========================================
  
  WHY IT EXISTS:
  - Holds serialized protobuf data
  - Can contain multiple slices (gather I/O)
  - Used for both send and receive
  
  IN grpc-c:
  
  // Packer creates byte buffer from message
  size_t pack_func(void *input, grpc_byte_buffer **buffer) {
      // Serialize protobuf to buffer
  }
  
  // Unpacker extracts message from byte buffer
  void *unpack_func(grpc_c_context_t *ctx, grpc_byte_buffer *buffer) {
      // Deserialize protobuf from buffer
  }
```

**图解说明 (Diagram Explanation):**

grpc-c核心对象角色：

| 对象 | 为何存在 | 生命周期 |
|------|----------|----------|
| `grpc_c_client_t` | 管理到服务器的连接 | `init()` → `free()` |
| `grpc_c_server_t` | 监听+分发请求 | `create()` → `destroy()` |
| `grpc_c_context_t` | 单次RPC的所有状态 | 隐式创建 → RPC结束释放 |
| `grpc_completion_queue` | 事件中心 | 每client/server各一个 |
| `grpc_slice` | 高效零拷贝缓冲 | 引用计数管理 |
| `grpc_byte_buffer` | 序列化数据容器 | RPC期间使用 |
| `grpc_metadata` | HTTP/2头/尾元数据 | 认证、追踪等 |

---

## 3. End-to-End RPC Lifecycle (C Perspective)

```
+=============================================================================+
|                    COMPLETE RPC TRACE                                       |
+=============================================================================+

  CLIENT SIDE (foo_client.c)
  ==========================
  
  main() {
      // 1. INITIALIZE GRPC-C
      grpc_c_init(GRPC_THREADS, NULL);
          |
          +-- grpc_init()               // Init gRPC core
          +-- thread_pool_create()      // Create workers
      
      // 2. CREATE CLIENT
      client = grpc_c_client_init(server_name, client_id, NULL, NULL);
          |
          +-- grpc_insecure_channel_create(host, args)
          +-- grpc_completion_queue_create()
          +-- LIST_INIT(context_list)
      
      // 3. PREPARE REQUEST
      foo__HelloRequest h = FOO__HELLO_REQUEST__INIT;
      h.name = "world";
      
      // 4. MAKE RPC CALL
      foo__greeter__say_hello(client, NULL, 0, &h, &response, NULL, -1);
          |
          +-- gc_client_prepare_unary_ops()
          |       |
          |       +-- grpc_c_context_init()
          |       +-- allocate ops array
          |       +-- ops[0] = SEND_INITIAL_METADATA
          |       +-- ops[1] = SEND_MESSAGE (packed request)
          |       +-- ops[2] = SEND_CLOSE_FROM_CLIENT
          |       +-- ops[3] = RECV_INITIAL_METADATA
          |       +-- ops[4] = RECV_MESSAGE
          |       +-- ops[5] = RECV_STATUS
          |
          +-- grpc_channel_create_call()
          |       |
          |       +-- Creates grpc_call object
          |       +-- Associates with CQ
          |
          +-- grpc_call_start_batch(ops, 6, context)
          |       |
          |       +-- Submit all ops to gRPC core
          |       +-- Core handles HTTP/2 framing
          |       +-- Core sends over network
          |
          +-- grpc_completion_queue_pluck(cq, context)
          |       |
          |       +-- BLOCKS until ops complete
          |       +-- Returns when all 6 ops done
          |
          +-- Unpack response
          +-- Copy status to output
          +-- grpc_c_context_free()
  }

  SERVER SIDE (foo_server.c)
  ==========================
  
  main() {
      // 1. INITIALIZE
      grpc_c_init(GRPC_THREADS, NULL);
      
      // 2. CREATE SERVER
      server = grpc_c_server_create(name, NULL, NULL);
          |
          +-- grpc_server_create()
          +-- grpc_completion_queue_create()
          +-- grpc_server_add_insecure_http2_port()
      
      // 3. REGISTER SERVICE
      foo__greeter__service_init(server);
          |
          +-- grpc_c_methods_alloc(server, 1)
          +-- grpc_c_register_method(server, "/foo.Greeter/SayHello", ...)
                  |
                  +-- grpc_server_register_method()
                  +-- Store handler and pack/unpack functions
      
      // 4. START SERVER
      grpc_c_server_start(server);
          |
          +-- grpc_server_start()
          +-- For each method:
                  |
                  +-- grpc_c_context_init()  // Pre-allocate context
                  +-- grpc_server_request_registered_call()
                          |
                          +-- "Wake me when this method is called"
      
      // 5. WAIT FOR CALLS
      grpc_c_server_wait(server);
          |
          +-- while (running) { gpr_cv_wait(...); }
  }

  WHEN REQUEST ARRIVES:
  =====================
  
  Worker Thread:
  
  gc_run_rpc() {
      // 1. Get event from CQ
      ev = grpc_completion_queue_next(server->gcs_cq, ...);
      
      // 2. Extract context from tag
      context = ev.tag->gce_data;
      
      // 3. Prepare for callback
      gc_prepare_server_callback(context);
          |
          +-- Copy method funcs (packers, handler)
          +-- Create stream handler
          +-- Re-register method for next call
          +-- Start RECV_CLOSE operation
          +-- Call user handler:
      
      // 4. USER HANDLER (foo__greeter__say_hello_cb)
      void foo__greeter__say_hello_cb(context) {
          // Read request
          context->gcc_stream->read(context, &request, 0, -1);
              |
              +-- ops[0] = RECV_MESSAGE
              +-- grpc_call_start_batch()
              +-- grpc_completion_queue_pluck()
              +-- Unpack protobuf
          
          // Process
          Foo__HelloReply response = ...;
          
          // Write response
          context->gcc_stream->write(context, &response, 0, -1);
              |
              +-- ops[0] = SEND_INITIAL_METADATA (if not sent)
              +-- ops[1] = SEND_MESSAGE
              +-- grpc_call_start_batch()
              +-- grpc_completion_queue_pluck()
          
          // Finish
          context->gcc_stream->finish(context, &status, 0);
              |
              +-- ops[0] = SEND_STATUS_FROM_SERVER
              +-- grpc_call_start_batch()
              +-- context->gcc_state = CLEANUP
      }
  }

  WIRE FORMAT (HTTP/2):
  =====================
  
  CLIENT --> SERVER:
  
  [HEADERS frame: ":method: POST, :path: /foo.Greeter/SayHello, ..."]
  [DATA frame: length-prefixed protobuf of HelloRequest]
  [DATA frame: (empty, EOS flag)]
  
  SERVER --> CLIENT:
  
  [HEADERS frame: ":status: 200, content-type: application/grpc, ..."]
  [DATA frame: length-prefixed protobuf of HelloReply]
  [HEADERS frame: "grpc-status: 0, grpc-message: OK"]
```

**图解说明 (Diagram Explanation):**

端到端RPC追踪：

**客户端流程**：
1. `grpc_c_init()` → 初始化gRPC Core + 创建线程池
2. `grpc_c_client_init()` → 创建Channel和CQ
3. 准备ops数组（SEND_METADATA, SEND_MESSAGE, RECV_*, ...）
4. `grpc_call_start_batch()` → 提交到Core
5. `grpc_completion_queue_pluck()` → 阻塞等待完成

**服务端流程**：
1. `grpc_c_server_create()` + 注册方法
2. `grpc_c_server_start()` → 预注册上下文
3. 工作线程`gc_run_rpc()` → `cq_next()`获取事件
4. 调用用户处理器 → `read()` + `write()` + `finish()`

**线格式**：HEADERS帧 → DATA帧（length-prefixed protobuf）→ HEADERS帧（grpc-status）

---

## 4. grpc-c API Usage Patterns

```
+=============================================================================+
|                    COMMON USAGE PATTERNS                                    |
+=============================================================================+

  A. INITIALIZATION AND SHUTDOWN ORDER
  ====================================
  
  CORRECT ORDER (Critical!):
  
  int main() {
      // 1. Initialize grpc-c (must be first)
      grpc_c_init(GRPC_THREADS, NULL);
      
      // 2. Create clients/servers
      client = grpc_c_client_init(...);
      // OR
      server = grpc_c_server_create(...);
      
      // 3. Use...
      
      // 4. Cleanup in reverse order
      grpc_c_client_free(client);
      // OR
      grpc_c_server_destroy(server);
      
      // 5. Shutdown grpc-c (must be last)
      grpc_c_shutdown();
      
      return 0;
  }
  
  WRONG ORDER (Will crash or leak):
  
  grpc_c_client_free(client);
  grpc_c_shutdown();         // Client's CQ events still pending!
  
  // OR
  
  grpc_c_shutdown();
  grpc_c_client_free(client);  // gRPC already torn down!

  B. TYPICAL CLIENT STRUCTURE
  ===========================
  
  // Simple synchronous client
  int make_rpc(const char *server, const char *message) {
      int result = GRPC_C_FAIL;
      grpc_c_client_t *client = NULL;
      Foo__HelloRequest request = FOO__HELLO_REQUEST__INIT;
      Foo__HelloReply *reply = NULL;
      
      // Create client (reuse if possible!)
      client = grpc_c_client_init(server, "my-client", NULL, NULL);
      if (!client) {
          fprintf(stderr, "Failed to create client\n");
          return GRPC_C_FAIL;
      }
      
      // Prepare request
      request.name = (char *)message;
      
      // Make call
      result = foo__greeter__say_hello(client, NULL, 0, 
                                       &request, &reply, NULL, 5000);
      
      if (result == GRPC_C_OK && reply) {
          printf("Response: %s\n", reply->message);
      } else if (result == GRPC_C_TIMEOUT) {
          fprintf(stderr, "RPC timed out\n");
      } else {
          fprintf(stderr, "RPC failed: %d\n", result);
      }
      
      // Cleanup
      grpc_c_client_free(client);
      
      return result;
  }

  C. TYPICAL SERVER STRUCTURE
  ===========================
  
  static grpc_c_server_t *server = NULL;
  
  // Signal handler for graceful shutdown
  void sigint_handler(int sig) {
      if (server) {
          grpc_c_server_destroy(server);
      }
      exit(0);
  }
  
  // RPC handler
  void my_rpc_handler(grpc_c_context_t *context) {
      Foo__Request *request = NULL;
      Foo__Response response = FOO__RESPONSE__INIT;
      grpc_c_status_t status = {0};
      
      // 1. Read request
      if (context->gcc_stream->read(context, (void **)&request, 0, -1)) {
          status.gcs_code = GRPC_STATUS_INTERNAL;
          snprintf(status.gcs_message, sizeof(status.gcs_message),
                   "Failed to read request");
          context->gcc_stream->finish(context, &status, 0);
          return;
      }
      
      // 2. Process (your business logic)
      response.result = process(request);
      
      // 3. Write response
      if (context->gcc_stream->write(context, &response, 0, -1)) {
          // Write failed, still try to send status
      }
      
      // 4. Send status
      status.gcs_code = GRPC_STATUS_OK;
      context->gcc_stream->finish(context, &status, 0);
  }
  
  int main() {
      signal(SIGINT, sigint_handler);
      
      grpc_c_init(GRPC_THREADS, NULL);
      
      server = grpc_c_server_create("myservice", NULL, NULL);
      if (!server) {
          return 1;
      }
      
      grpc_c_server_add_insecure_http2_port(server, "0.0.0.0:50051");
      my_service__service_init(server);  // Generated
      
      grpc_c_server_start(server);
      grpc_c_server_wait(server);
      
      return 0;
  }

  D. ERROR HANDLING MODEL
  =======================
  
  // Return codes
  #define GRPC_C_OK      0  // Success
  #define GRPC_C_FAIL    1  // General failure
  #define GRPC_C_TIMEOUT 2  // Operation timed out
  
  // Status structure
  typedef struct grpc_c_status_s {
      int gcs_code;                     // gRPC status code
      char gcs_message[4 * BUFSIZ];     // Human-readable message
  } grpc_c_status_t;
  
  // Checking for errors
  int result = foo__service__method(client, ...);
  
  switch (result) {
      case GRPC_C_OK:
          // Success, check status for RPC-level result
          if (status.gcs_code != GRPC_STATUS_OK) {
              printf("RPC returned: %s\n", status.gcs_message);
          }
          break;
      case GRPC_C_TIMEOUT:
          // Network or server timeout
          printf("Timeout after %ld ms\n", timeout);
          break;
      case GRPC_C_FAIL:
          // Internal error
          printf("Internal failure\n");
          break;
  }

  E. STREAMING CONTROL FLOW
  =========================
  
  // Client Streaming Pattern
  void client_streaming_example(grpc_c_context_t *context) {
      Foo__Request *request;
      
      while (/* more data */) {
          // Send request
          context->gcc_stream->write(context, &request, 0, -1);
      }
      
      // Signal end of requests
      context->gcc_stream->write_done(context, 0, -1);
      
      // Read single response
      context->gcc_stream->read(context, &response, 0, -1);
      
      // Get status
      context->gcc_stream->finish(context, &status, 0);
  }
  
  // Server Streaming Pattern
  void server_streaming_handler(grpc_c_context_t *context) {
      Foo__Request *request;
      Foo__Response response;
      
      // Read single request
      context->gcc_stream->read(context, (void **)&request, 0, -1);
      
      // Send multiple responses
      while (/* more results */) {
          response = get_next_result();
          context->gcc_stream->write(context, &response, 0, -1);
      }
      
      // Finish with status
      context->gcc_stream->finish(context, &status, 0);
  }
  
  // Bidirectional Streaming
  void bidi_handler(grpc_c_context_t *context) {
      // Can interleave reads and writes
      while (!done) {
          // Check for input
          if (has_input) {
              context->gcc_stream->read(context, &in, 0, timeout);
          }
          
          // Send output
          if (has_output) {
              context->gcc_stream->write(context, &out, 0, -1);
          }
      }
      
      context->gcc_stream->finish(context, &status, 0);
  }
```

**图解说明 (Diagram Explanation):**

grpc-c API使用模式：

**初始化/关闭顺序**（关键！）：
```
正确：grpc_c_init() → client_init/server_create → 使用 → free/destroy → grpc_c_shutdown()
错误：grpc_c_shutdown() → client_free  // gRPC已销毁！
```

**典型客户端**：创建client → 准备request → 调用RPC → 处理response → 释放client

**典型服务端**：创建server → 注册服务 → 启动 → 等待 + 信号处理器关闭

**流式控制流**：
- 客户端流：多次`write()` → `write_done()` → `read()` → `finish()`
- 服务端流：`read()` → 多次`write()` → `finish()`
- 双向流：交错`read()`/`write()` → `finish()`

---

## 5. Costs and Boundaries of the Architecture

```
+=============================================================================+
|                    PERFORMANCE TRADE-OFFS                                   |
+=============================================================================+

  A. WHERE gRPC PAYS OVERHEAD
  ===========================
  
  1. SERIALIZATION (Protobuf)
  ---------------------------
  
  Cost: ~50 CPU cycles per field
  
  When it matters:
  - Millions of small messages
  - Latency-critical microsecond paths
  
  Compared to:
  - JSON: 500+ cycles per field
  - Raw binary: 0 cycles
  
  2. HTTP/2 FRAMING
  -----------------
  
  Per message overhead: ~9 bytes minimum
  
  [Length (5 bytes)][Compressed flag (1 byte)][Message length (4 bytes)]
  
  Plus HTTP/2 frame header: 9 bytes
  
  For 100-byte payload: ~18% overhead
  For 10KB payload: ~0.2% overhead
  
  3. TLS ENCRYPTION
  -----------------
  
  Cost: ~1000 cycles per 1KB (with hardware acceleration)
  
  First connection: Full handshake (~2-5 RTT)
  Session resumption: ~1 RTT
  
  4. COMPLETION QUEUE POLLING
  ---------------------------
  
  cq_next() with no events:
  - epoll_wait syscall: ~1 microsecond
  - Thread park/unpark: ~10 microseconds
  
  cq_pluck() (sync wait):
  - Adds futex syscall if no event ready

  B. MEMORY TRADE-OFFS
  ====================
  
  Per Client:
  - grpc_c_client_t: ~500 bytes
  - grpc_channel: ~2KB
  - Completion queue: ~4KB
  - Thread pool (shared): 4 threads × 1MB stack = 4MB
  
  Per Active RPC:
  - grpc_c_context_t: ~1KB
  - grpc_call: ~2KB
  - Metadata arrays: ~1KB (variable)
  - Payload buffers: size of message
  
  Scaling example:
  - 10,000 concurrent RPCs
  - Average message size: 1KB
  - Memory: ~40MB + 10MB payload = ~50MB
  
  Compared to thread-per-request:
  - 10,000 threads × 1MB = 10GB!

  C. CPU TRADE-OFFS
  =================
  
  gRPC CPU breakdown (typical):
  
  +------------------------+----------+
  | Component              | % CPU    |
  +------------------------+----------+
  | Protobuf serialization | 20-30%   |
  | HTTP/2 framing         | 10-15%   |
  | TLS encryption         | 15-25%   |
  | gRPC Core overhead     | 10-15%   |
  | Your business logic    | 20-40%   |
  +------------------------+----------+
  
  Optimization levers:
  - Reuse connections (avoid TLS handshake)
  - Batch messages (amortize framing)
  - Use streaming for many small messages

  D. SITUATIONS WHERE gRPC IS THE WRONG CHOICE
  ============================================
  
  1. ULTRA-LOW LATENCY (< 10 microseconds)
  ----------------------------------------
  
  Why: HTTP/2 framing + protobuf + CQ polling add overhead
  Alternative: Custom UDP protocol, shared memory
  
  2. EXTREME THROUGHPUT (> 10 million msg/sec per core)
  -----------------------------------------------------
  
  Why: Serialization becomes bottleneck
  Alternative: Zero-copy protocols, Flatbuffers
  
  3. HARD REAL-TIME
  -----------------
  
  Why: CQ polling has non-deterministic latency
  Alternative: Lock-free queues, dedicated cores
  
  4. BROWSER CLIENTS (directly)
  -----------------------------
  
  Why: Browsers don't support HTTP/2 trailers
  Alternative: gRPC-Web with proxy
  
  5. SIMPLE REQUEST-RESPONSE
  --------------------------
  
  Why: gRPC setup overhead > benefit
  Alternative: Plain HTTP REST
  
  6. EMBEDDED / RESOURCE-CONSTRAINED
  -----------------------------------
  
  Why: gRPC Core + dependencies > 10MB
  Alternative: MQTT, CoAP, custom protocol

  E. LIMITATIONS IN EMBEDDED/REAL-TIME
  ====================================
  
  MEMORY CONSTRAINTS:
  
  gRPC Core: ~2MB binary
  + protobuf: ~500KB
  + grpc-c: ~100KB
  + Your code: ~100KB
  = ~3MB minimum
  
  Many embedded devices: 256KB - 1MB total
  
  REAL-TIME CONSTRAINTS:
  
  gRPC operations have unbounded latency:
  - malloc() can trigger page faults
  - CQ polling involves kernel syscalls
  - HTTP/2 has flow control that can stall
  
  For hard real-time (< 1ms guaranteed):
  - Pre-allocate all memory
  - Use lock-free queues
  - Avoid kernel involvement
```

**图解说明 (Diagram Explanation):**

性能开销分析：

| 组件 | CPU占比 | 优化建议 |
|------|---------|----------|
| Protobuf序列化 | 20-30% | 使用Arena分配 |
| HTTP/2帧封装 | 10-15% | 批量发送 |
| TLS加密 | 15-25% | 复用连接 |
| gRPC Core | 10-15% | 调整CQ数量 |
| 业务逻辑 | 20-40% | 应用层优化 |

**gRPC不适用场景**：
- 超低延迟(<10μs) → 共享内存
- 极高吞吐(>1000万msg/s) → Flatbuffers
- 硬实时 → 无锁队列
- 嵌入式(<1MB) → MQTT/CoAP
- 浏览器直连 → gRPC-Web

---

## 中文说明

### gRPC架构具体实现详解

#### 1. 核心架构模式

**状态机模式**
- 每个RPC都是一个状态机
- grpc-c定义了完整的状态枚举（GRPC_C_SERVER_CALLBACK_WAIT等）
- 状态转换由事件驱动

**Proactor模式（事件驱动）**
- 提交异步操作后立即返回
- 完成时通过完成队列通知
- 处理器直接获得结果，无需等待I/O

**过滤器链**
- 请求/响应流经多个过滤器
- 可用于压缩、重试、追踪等

**不透明句柄**
- 公共头文件只声明类型名
- 内部结构对用户隐藏
- 保证ABI稳定性

#### 2. 关键对象详解

| 对象 | 用途 | 生命周期 |
|------|------|----------|
| `grpc_c_client_t` | 客户端连接句柄 | init → free |
| `grpc_c_server_t` | 服务端实例 | create → destroy |
| `grpc_c_context_t` | 单次RPC上下文 | 隐式创建 → RPC结束释放 |
| `grpc_completion_queue` | 事件中心 | 每个client/server各一个 |
| `grpc_slice` | 高效内存缓冲 | 引用计数管理 |
| `grpc_byte_buffer` | 序列化数据容器 | RPC期间使用 |

#### 3. RPC完整流程

**客户端流程：**
1. 初始化grpc-c
2. 创建客户端（创建channel和CQ）
3. 准备操作数组（发送元数据、发送消息、接收等）
4. 调用start_batch提交所有操作
5. 阻塞等待完成（pluck）或异步回调
6. 解包响应，释放资源

**服务端流程：**
1. 初始化grpc-c
2. 创建服务器并注册方法
3. 启动服务器（request_registered_call）
4. 工作线程从CQ获取事件
5. 调用用户处理器
6. 读取请求、处理、写入响应、发送状态

#### 4. 性能开销分析

| 组件 | CPU占比 | 优化建议 |
|------|---------|----------|
| Protobuf序列化 | 20-30% | 使用Arena分配 |
| HTTP/2帧封装 | 10-15% | 批量发送 |
| TLS加密 | 15-25% | 复用连接 |
| gRPC Core | 10-15% | 调整CQ数量 |
| 业务逻辑 | 20-40% | 应用层优化 |

#### 5. gRPC不适用场景

- **超低延迟**（<10微秒）：协议开销太大
- **极高吞吐**（>1000万msg/秒）：序列化成为瓶颈
- **硬实时系统**：CQ轮询延迟不确定
- **嵌入式设备**：二进制太大（>3MB）
- **浏览器直连**：需要gRPC-Web代理
