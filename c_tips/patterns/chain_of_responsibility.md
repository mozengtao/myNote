# Chain of Responsibility Pattern (责任链模式)

## 1. Core Concept and Use Cases

### Core Concept
Chain handlers together and pass a request along the chain until one handler processes it. Each handler decides either to **process the request** or to **pass it to the next handler**.

### Typical Use Cases
- Logging with different severity levels
- Request filtering/validation
- Authentication/authorization chains
- Event handling systems
- Middleware pipelines

---

## 2. Architecture Diagram

```
+--------------------------------------------------------------------------------------------------+
|                               Chain of Responsibility Pattern                                     |
+--------------------------------------------------------------------------------------------------+

    Request                                                                        
       |                                                                           
       v                                                                           
+-------------+       +-------------+       +-------------+       +-------------+  
|  Handler 1  |------>|  Handler 2  |------>|  Handler 3  |------>|  Handler N  |  
| (Logging)   |       | (Auth)      |       | (Validation)|       | (Processing)|  
+-------------+       +-------------+       +-------------+       +-------------+  
       |                    |                     |                     |          
       v                    v                     v                     v          
   Can handle?         Can handle?           Can handle?           Can handle?    
   YES -> Process      YES -> Process        YES -> Process        YES -> Process 
   NO  -> Next         NO  -> Next           NO  -> Next           NO  -> End     


    Handler Structure:
    
    +---------------------------+
    |         Handler           |
    +---------------------------+
    | - next: Handler*          |  --> Points to next handler
    +---------------------------+
    | + handle(request)         |  --> Process or delegate
    | + set_next(handler)       |  --> Set successor
    +---------------------------+


    Processing Flow:
    
    +---------+     +---------+     +---------+     +---------+
    | Handler |---->| Handler |---->| Handler |---->|   NULL  |
    |    A    |     |    B    |     |    C    |     | (End)   |
    +---------+     +---------+     +---------+     +---------+
         |               |               |
         v               v               v
      handle()        handle()        handle()
      [PASS]          [PASS]         [PROCESS]
                                     Request handled!
```

**中文说明：**

责任链模式的核心流程：

1. **处理器链（Handler Chain）**：
   - 多个处理器通过 `next` 指针连接
   - 每个处理器有自己的处理逻辑

2. **请求处理**：
   - 请求从链头进入
   - 每个处理器检查是否能处理
   - 能处理则处理，否则传递给下一个

3. **终止条件**：
   - 某个处理器处理了请求
   - 到达链尾（所有处理器都不能处理）

---

## 3. Code Skeleton

```c
/* Handler interface */
typedef struct Handler Handler;

typedef int (*handler_fn)(Handler* self, void* request);

struct Handler {
    char name[32];
    Handler* next;
    handler_fn handle;
    void* context;
};

/* Chain operations */
void handler_set_next(Handler* h, Handler* next);
int handler_handle(Handler* h, void* request);
Handler* chain_build(Handler* handlers[], int count);
```

**中文说明：**

代码骨架包含：
- `Handler`：处理器结构，包含 next 指针和处理函数
- `handler_set_next()`：设置下一个处理器
- `handler_handle()`：处理请求或传递
- `chain_build()`：构建处理器链

---

## 4. Complete Example Code

```c
/*
 * Chain of Responsibility Pattern - Request Processing Example
 * 
 * This example demonstrates a request processing pipeline
 * with logging, authentication, validation, and business logic handlers.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================
 * Request Structure
 * ============================================ */
typedef enum {
    REQUEST_TYPE_READ,
    REQUEST_TYPE_WRITE,
    REQUEST_TYPE_DELETE,
    REQUEST_TYPE_ADMIN
} RequestType;

typedef struct {
    RequestType type;
    char user[32];
    char token[64];
    char resource[64];
    char data[256];
    int user_level;  /* 0=guest, 1=user, 2=admin */
} Request;

/* Response codes */
typedef enum {
    RESPONSE_OK = 0,
    RESPONSE_PASSED,        /* Passed to next handler */
    RESPONSE_DENIED,
    RESPONSE_INVALID,
    RESPONSE_ERROR
} ResponseCode;

/* ============================================
 * Handler Interface
 * ============================================ */
typedef struct Handler Handler;

typedef ResponseCode (*handler_fn)(Handler* self, Request* request);

struct Handler {
    char name[32];          /* Handler name for logging */
    Handler* next;          /* Next handler in chain */
    handler_fn handle;      /* Handler function */
    void* context;          /* Handler-specific data */
};

/* Set next handler in chain */
void handler_set_next(Handler* h, Handler* next)
{
    h->next = next;
}

/* Pass request to next handler */
ResponseCode handler_pass_to_next(Handler* h, Request* request)
{
    if (h->next != NULL) {
        printf("    [%s] Passing to next handler: %s\n", h->name, h->next->name);
        return h->next->handle(h->next, request);
    }
    printf("    [%s] End of chain reached\n", h->name);
    return RESPONSE_OK;
}

/* ============================================
 * Handler 1: Logging Handler
 * Logs all requests, always passes to next
 * ============================================ */
typedef struct {
    int request_count;
} LoggingContext;

ResponseCode logging_handle(Handler* self, Request* request)
{
    LoggingContext* ctx = (LoggingContext*)self->context;
    ctx->request_count++;
    
    const char* type_str = 
        (request->type == REQUEST_TYPE_READ) ? "READ" :
        (request->type == REQUEST_TYPE_WRITE) ? "WRITE" :
        (request->type == REQUEST_TYPE_DELETE) ? "DELETE" : "ADMIN";
    
    printf("\n  [%s] Request #%d: type=%s, user=%s, resource=%s\n",
           self->name, ctx->request_count, type_str, 
           request->user, request->resource);
    
    /* Always pass to next handler */
    return handler_pass_to_next(self, request);
}

Handler* create_logging_handler(void)
{
    Handler* h = (Handler*)malloc(sizeof(Handler));
    LoggingContext* ctx = (LoggingContext*)malloc(sizeof(LoggingContext));
    
    strncpy(h->name, "LoggingHandler", sizeof(h->name) - 1);
    h->next = NULL;
    h->handle = logging_handle;
    h->context = ctx;
    ctx->request_count = 0;
    
    return h;
}

/* ============================================
 * Handler 2: Authentication Handler
 * Verifies token, blocks if invalid
 * ============================================ */
typedef struct {
    char valid_tokens[10][64];
    int token_count;
} AuthContext;

ResponseCode auth_handle(Handler* self, Request* request)
{
    AuthContext* ctx = (AuthContext*)self->context;
    
    printf("  [%s] Checking authentication for user: %s\n", 
           self->name, request->user);
    
    /* Check if token is valid */
    int authenticated = 0;
    for (int i = 0; i < ctx->token_count; i++) {
        if (strcmp(request->token, ctx->valid_tokens[i]) == 0) {
            authenticated = 1;
            break;
        }
    }
    
    if (!authenticated) {
        printf("  [%s] BLOCKED: Invalid token!\n", self->name);
        return RESPONSE_DENIED;  /* Stop chain here */
    }
    
    printf("  [%s] Authentication successful\n", self->name);
    return handler_pass_to_next(self, request);
}

Handler* create_auth_handler(void)
{
    Handler* h = (Handler*)malloc(sizeof(Handler));
    AuthContext* ctx = (AuthContext*)malloc(sizeof(AuthContext));
    
    strncpy(h->name, "AuthHandler", sizeof(h->name) - 1);
    h->next = NULL;
    h->handle = auth_handle;
    h->context = ctx;
    
    /* Setup valid tokens */
    strcpy(ctx->valid_tokens[0], "token_user123");
    strcpy(ctx->valid_tokens[1], "token_admin456");
    strcpy(ctx->valid_tokens[2], "token_guest789");
    ctx->token_count = 3;
    
    return h;
}

/* ============================================
 * Handler 3: Authorization Handler
 * Checks user permissions for the operation
 * ============================================ */
ResponseCode authz_handle(Handler* self, Request* request)
{
    printf("  [%s] Checking authorization: user_level=%d, type=%d\n",
           self->name, request->user_level, request->type);
    
    /* Check permissions based on user level and request type */
    int allowed = 0;
    
    switch (request->type) {
        case REQUEST_TYPE_READ:
            allowed = 1;  /* Everyone can read */
            break;
        case REQUEST_TYPE_WRITE:
            allowed = (request->user_level >= 1);  /* Users and admins */
            break;
        case REQUEST_TYPE_DELETE:
            allowed = (request->user_level >= 2);  /* Admins only */
            break;
        case REQUEST_TYPE_ADMIN:
            allowed = (request->user_level >= 2);  /* Admins only */
            break;
    }
    
    if (!allowed) {
        printf("  [%s] BLOCKED: Insufficient permissions!\n", self->name);
        return RESPONSE_DENIED;
    }
    
    printf("  [%s] Authorization granted\n", self->name);
    return handler_pass_to_next(self, request);
}

Handler* create_authz_handler(void)
{
    Handler* h = (Handler*)malloc(sizeof(Handler));
    
    strncpy(h->name, "AuthzHandler", sizeof(h->name) - 1);
    h->next = NULL;
    h->handle = authz_handle;
    h->context = NULL;
    
    return h;
}

/* ============================================
 * Handler 4: Validation Handler
 * Validates request data
 * ============================================ */
ResponseCode validation_handle(Handler* self, Request* request)
{
    printf("  [%s] Validating request data\n", self->name);
    
    /* Check resource name */
    if (strlen(request->resource) == 0) {
        printf("  [%s] INVALID: Empty resource name!\n", self->name);
        return RESPONSE_INVALID;
    }
    
    /* Check for write operations - need data */
    if (request->type == REQUEST_TYPE_WRITE && strlen(request->data) == 0) {
        printf("  [%s] INVALID: Write request without data!\n", self->name);
        return RESPONSE_INVALID;
    }
    
    printf("  [%s] Validation passed\n", self->name);
    return handler_pass_to_next(self, request);
}

Handler* create_validation_handler(void)
{
    Handler* h = (Handler*)malloc(sizeof(Handler));
    
    strncpy(h->name, "ValidationHandler", sizeof(h->name) - 1);
    h->next = NULL;
    h->handle = validation_handle;
    h->context = NULL;
    
    return h;
}

/* ============================================
 * Handler 5: Business Logic Handler
 * Actually processes the request
 * ============================================ */
ResponseCode business_handle(Handler* self, Request* request)
{
    printf("  [%s] Processing request...\n", self->name);
    
    switch (request->type) {
        case REQUEST_TYPE_READ:
            printf("  [%s] Reading resource: %s\n", self->name, request->resource);
            break;
        case REQUEST_TYPE_WRITE:
            printf("  [%s] Writing to resource: %s, data: %s\n", 
                   self->name, request->resource, request->data);
            break;
        case REQUEST_TYPE_DELETE:
            printf("  [%s] Deleting resource: %s\n", self->name, request->resource);
            break;
        case REQUEST_TYPE_ADMIN:
            printf("  [%s] Executing admin operation on: %s\n", 
                   self->name, request->resource);
            break;
    }
    
    printf("  [%s] Request processed successfully!\n", self->name);
    return RESPONSE_OK;  /* Final handler, don't pass */
}

Handler* create_business_handler(void)
{
    Handler* h = (Handler*)malloc(sizeof(Handler));
    
    strncpy(h->name, "BusinessHandler", sizeof(h->name) - 1);
    h->next = NULL;
    h->handle = business_handle;
    h->context = NULL;
    
    return h;
}

/* ============================================
 * Chain Builder
 * ============================================ */
Handler* build_chain(void)
{
    Handler* logging = create_logging_handler();
    Handler* auth = create_auth_handler();
    Handler* authz = create_authz_handler();
    Handler* validation = create_validation_handler();
    Handler* business = create_business_handler();
    
    /* Build chain: logging -> auth -> authz -> validation -> business */
    handler_set_next(logging, auth);
    handler_set_next(auth, authz);
    handler_set_next(authz, validation);
    handler_set_next(validation, business);
    
    printf("[Chain] Built: Logging -> Auth -> Authz -> Validation -> Business\n");
    return logging;  /* Return head of chain */
}

void destroy_chain(Handler* head)
{
    Handler* current = head;
    while (current != NULL) {
        Handler* next = current->next;
        if (current->context) free(current->context);
        free(current);
        current = next;
    }
}

/* ============================================
 * Process Request through Chain
 * ============================================ */
void process_request(Handler* chain, Request* request)
{
    printf("\n========== Processing Request ==========\n");
    ResponseCode result = chain->handle(chain, request);
    
    const char* result_str = 
        (result == RESPONSE_OK) ? "OK" :
        (result == RESPONSE_DENIED) ? "DENIED" :
        (result == RESPONSE_INVALID) ? "INVALID" : "ERROR";
    
    printf("========== Result: %s ==========\n", result_str);
}

/* ============================================
 * Main Function - Demonstration
 * ============================================ */
int main(void)
{
    printf("=== Chain of Responsibility Pattern Demo ===\n\n");
    
    /* Build the handler chain */
    Handler* chain = build_chain();
    
    /* Test Case 1: Valid read request */
    printf("\n--- Test 1: Valid Read Request (Guest) ---");
    Request req1 = {
        .type = REQUEST_TYPE_READ,
        .user = "guest",
        .token = "token_guest789",
        .resource = "/api/data",
        .data = "",
        .user_level = 0
    };
    process_request(chain, &req1);
    
    /* Test Case 2: Valid write request */
    printf("\n--- Test 2: Valid Write Request (User) ---");
    Request req2 = {
        .type = REQUEST_TYPE_WRITE,
        .user = "john",
        .token = "token_user123",
        .resource = "/api/data",
        .data = "{\"value\": 42}",
        .user_level = 1
    };
    process_request(chain, &req2);
    
    /* Test Case 3: Invalid token */
    printf("\n--- Test 3: Invalid Token ---");
    Request req3 = {
        .type = REQUEST_TYPE_READ,
        .user = "hacker",
        .token = "invalid_token",
        .resource = "/api/secret",
        .data = "",
        .user_level = 0
    };
    process_request(chain, &req3);
    
    /* Test Case 4: Insufficient permissions */
    printf("\n--- Test 4: Insufficient Permissions ---");
    Request req4 = {
        .type = REQUEST_TYPE_DELETE,
        .user = "john",
        .token = "token_user123",
        .resource = "/api/data",
        .data = "",
        .user_level = 1  /* User level, not admin */
    };
    process_request(chain, &req4);
    
    /* Test Case 5: Admin operation */
    printf("\n--- Test 5: Admin Operation ---");
    Request req5 = {
        .type = REQUEST_TYPE_ADMIN,
        .user = "admin",
        .token = "token_admin456",
        .resource = "/api/system",
        .data = "",
        .user_level = 2
    };
    process_request(chain, &req5);
    
    /* Cleanup */
    destroy_chain(chain);
    
    printf("\n=== Demo Complete ===\n");
    return 0;
}
```

**中文说明：**

完整示例代码实现了请求处理管道：

1. **处理器链**：
   - **LoggingHandler**：记录所有请求，始终传递
   - **AuthHandler**：验证 token，无效则拒绝
   - **AuthzHandler**：检查用户权限
   - **ValidationHandler**：验证请求数据
   - **BusinessHandler**：执行实际业务逻辑

2. **请求处理流程**：
   - 请求从链头进入
   - 每个处理器决定处理或传递
   - 任何处理器可以终止链（拒绝/错误）

3. **测试场景**：
   - 有效的读请求
   - 有效的写请求
   - 无效 token（被 Auth 拒绝）
   - 权限不足（被 Authz 拒绝）
   - 管理员操作

