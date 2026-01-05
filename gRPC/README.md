# gRPC Architecture Deep Dive

## For Senior Distributed Systems Engineers

This documentation provides a comprehensive, architectural understanding of gRPC through the grpc-c codebase. The goal is not to memorize APIs but to build execution-model-level understanding.

---

## Documentation Structure

Follow the **WHY → HOW → WHAT → WHERE** learning path:

| # | Document | Focus | Time |
|---|----------|-------|------|
| 1 | [01_WHY_Engineering_Motivation.md](./01_WHY_Engineering_Motivation.md) | Why gRPC exists | 1 hour |
| 2 | [02_HOW_Design_Philosophy.md](./02_HOW_Design_Philosophy.md) | Core design ideas | 2 hours |
| 3 | [03_WHAT_Architecture_Concrete.md](./03_WHAT_Architecture_Concrete.md) | Implementation patterns | 2 hours |
| 4 | [04_WHERE_Source_Reading_Strategy.md](./04_WHERE_Source_Reading_Strategy.md) | Code reading guide | 8+ hours |
| 5 | [05_Real_Project_Usage.md](./05_Real_Project_Usage.md) | Production practices | 1 hour |
| 6 | [06_Reflection_Knowledge_Transfer.md](./06_Reflection_Knowledge_Transfer.md) | Knowledge application | 1 hour |

---

## Quick Reference

### gRPC Layer Architecture

```
+------------------------------------------------------------------+
|                    APPLICATION (Your Code)                        |
+------------------------------------------------------------------+
                               |
+------------------------------------------------------------------+
|                    GENERATED STUBS (.grpc-c.h)                    |
+------------------------------------------------------------------+
                               |
+------------------------------------------------------------------+
|                    grpc-c PUBLIC API                              |
|    grpc_c_client_t | grpc_c_server_t | grpc_c_context_t           |
+------------------------------------------------------------------+
                               |
+------------------------------------------------------------------+
|                    gRPC CORE                                      |
|    grpc_channel | grpc_call | grpc_completion_queue               |
+------------------------------------------------------------------+
                               |
+------------------------------------------------------------------+
|                    HTTP/2 TRANSPORT                               |
+------------------------------------------------------------------+
                               |
+------------------------------------------------------------------+
|                    OS / TCP                                       |
+------------------------------------------------------------------+
```

### Key Objects Lifecycle

```
grpc_c_init()
    |
    +-- grpc_c_client_init() -----> grpc_c_client_free()
    |       |
    |       +-- [RPC via context]
    |
    +-- grpc_c_server_create() ---> grpc_c_server_destroy()
            |
            +-- grpc_c_register_method()
            |
            +-- grpc_c_server_start()
            |
            +-- grpc_c_server_wait()
    |
grpc_c_shutdown()
```

### Common Pitfalls Checklist

- [ ] Always call `grpc_c_client_free()` / `grpc_c_server_destroy()`
- [ ] Never block inside RPC handlers
- [ ] Reuse channels (don't create per-request)
- [ ] Drain completion queue before destroying
- [ ] Set appropriate timeouts
- [ ] Handle all return codes

---

## Learning Outcomes

After studying these documents, you should be able to:

1. **Explain** gRPC's architecture end-to-end
2. **Trace** a single RPC through grpc-c and gRPC Core
3. **Use** grpc-c correctly in a production C project
4. **Avoid** common concurrency and lifecycle bugs
5. **Apply** gRPC's architectural ideas in your own systems

---

## Code Examples Reference

### Simple Client
```c
grpc_c_init(GRPC_THREADS, NULL);

grpc_c_client_t *client = grpc_c_client_init(
    server_name, "my-client", NULL, NULL);

Foo__HelloRequest request = FOO__HELLO_REQUEST__INIT;
request.name = "world";

Foo__HelloReply *reply;
int status = foo__greeter__say_hello(
    client, NULL, 0, &request, &reply, NULL, 5000);

if (status == GRPC_C_OK && reply) {
    printf("Response: %s\n", reply->message);
}

grpc_c_client_free(client);
grpc_c_shutdown();
```

### Simple Server
```c
grpc_c_init(GRPC_THREADS, NULL);

grpc_c_server_t *server = grpc_c_server_create(
    "myservice", NULL, NULL);

grpc_c_server_add_insecure_http2_port(server, "0.0.0.0:50051");
foo__greeter__service_init(server);  // Generated

grpc_c_server_start(server);
grpc_c_server_wait(server);

grpc_c_server_destroy(server);
grpc_c_shutdown();
```

### RPC Handler
```c
void foo__greeter__say_hello_cb(grpc_c_context_t *context) {
    Foo__HelloRequest *request;
    
    // Read
    if (context->gcc_stream->read(context, (void **)&request, 0, -1)) {
        // Handle error
        return;
    }
    
    // Process
    Foo__HelloReply reply = FOO__HELLO_REPLY__INIT;
    reply.message = "Hello!";
    
    // Write
    context->gcc_stream->write(context, &reply, 0, -1);
    
    // Finish
    grpc_c_status_t status = { .gcs_code = 0 };
    context->gcc_stream->finish(context, &status, 0);
}
```

---

## File Structure

```
docs/
├── README.md                              # This file
├── 01_WHY_Engineering_Motivation.md       # Engineering problems gRPC solves
├── 02_HOW_Design_Philosophy.md            # Design philosophy and patterns
├── 03_WHAT_Architecture_Concrete.md       # Implementation details
├── 04_WHERE_Source_Reading_Strategy.md    # Code reading guide
├── 05_Real_Project_Usage.md               # Production practices
└── 06_Reflection_Knowledge_Transfer.md    # Knowledge application
```

---

## 中文概要

本文档系列提供gRPC架构的深度理解，遵循 **为什么 → 怎么做 → 是什么 → 在哪里** 的学习路径。

目标不是记忆API，而是建立执行模型级别的理解，以便：
- 理解gRPC的设计权衡
- 在真实C项目中自信使用gRPC
- 避免常见的性能、并发和生命周期陷阱
- 在自己的系统设计中复用gRPC的架构思想

每个文档都包含：
1. 纯英文ASCII图表
2. 图表下方的中文说明
