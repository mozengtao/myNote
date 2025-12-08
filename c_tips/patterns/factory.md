# Factory Pattern (工厂模式)

## 1. Core Concept and Use Cases

### Core Concept
Encapsulate object creation logic in a separate function/module. The client code doesn't need to know the concrete creation details, only the interface.

### Typical Use Cases
- Creating protocol handlers (TCP/UDP/HTTP)
- Creating different message parsers
- Creating device drivers based on configuration
- Creating different encryption algorithms

---

## 2. Architecture Diagram

```
+--------------------------------------------------------------------------------------------------+
|                                      Factory Pattern                                              |
+--------------------------------------------------------------------------------------------------+

                                    +-------------------+
                                    |      Client       |
                                    +--------+----------+
                                             |
                                             | create(type)
                                             v
                        +------------------------------------------+
                        |              Factory                      |
                        |                                          |
                        |  product_create(type) {                  |
                        |      switch(type) {                      |
                        |          case TYPE_A: return create_A(); |
                        |          case TYPE_B: return create_B(); |
                        |          case TYPE_C: return create_C(); |
                        |      }                                   |
                        |  }                                       |
                        +------------------------------------------+
                                             |
                      +----------------------+----------------------+
                      |                      |                      |
                      v                      v                      v
               +-------------+        +-------------+        +-------------+
               | Product A   |        | Product B   |        | Product C   |
               +-------------+        +-------------+        +-------------+
               | - data_a    |        | - data_b    |        | - data_c    |
               | + process() |        | + process() |        | + process() |
               | + destroy() |        | + destroy() |        | + destroy() |
               +-------------+        +-------------+        +-------------+
                      |                      |                      |
                      +----------------------+----------------------+
                                             |
                                             v
                              +-----------------------------+
                              |     Common Interface        |
                              | (Function Pointers in C)    |
                              +-----------------------------+
                              | void (*process)(void* self) |
                              | void (*destroy)(void* self) |
                              +-----------------------------+
```

**中文说明：**

工厂模式的核心流程：

1. **客户端请求**：
   - 客户端调用工厂函数，传入产品类型
   - 客户端不需要知道具体的创建细节

2. **工厂创建**：
   - 工厂根据类型参数决定创建哪种产品
   - 封装了具体的创建逻辑
   - 返回统一接口的产品对象

3. **产品实现**：
   - 不同产品有不同的内部实现
   - 但都遵循相同的接口（函数指针）
   - 客户端通过接口使用产品，无需关心具体类型

---

## 3. Code Skeleton

```c
/* Common product interface */
typedef struct Product {
    void (*process)(struct Product* self, void* data);
    void (*destroy)(struct Product* self);
    void* private_data;
} Product;

/* Product type enumeration */
typedef enum {
    PRODUCT_TYPE_A,
    PRODUCT_TYPE_B,
    PRODUCT_TYPE_C
} ProductType;

/* Factory function */
Product* product_factory_create(ProductType type);

/* Concrete product creation (private) */
static Product* create_product_a(void);
static Product* create_product_b(void);
static Product* create_product_c(void);
```

**中文说明：**

代码骨架包含：
- `Product` 结构体：定义产品的通用接口
- `ProductType` 枚举：定义产品类型
- `product_factory_create()`：工厂函数，根据类型创建产品
- `create_product_x()`：具体产品的创建函数

---

## 4. Complete Example Code

```c
/*
 * Factory Pattern - Protocol Handler Example
 * 
 * This example demonstrates creating different protocol
 * handlers (TCP, UDP, HTTP) using the factory pattern.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================
 * Protocol Handler Interface (Base Product)
 * ============================================ */
typedef struct ProtocolHandler {
    char name[32];                                          /* Handler name */
    int  port;                                              /* Port number */
    
    /* Virtual function table (interface) */
    int  (*connect)(struct ProtocolHandler* self, const char* host);
    int  (*send)(struct ProtocolHandler* self, const char* data, int len);
    int  (*receive)(struct ProtocolHandler* self, char* buf, int max_len);
    void (*disconnect)(struct ProtocolHandler* self);
    void (*destroy)(struct ProtocolHandler* self);
    
    void* private_data;                                     /* Protocol-specific data */
} ProtocolHandler;

/* Protocol type enumeration */
typedef enum {
    PROTOCOL_TCP,
    PROTOCOL_UDP,
    PROTOCOL_HTTP
} ProtocolType;

/* ============================================
 * TCP Handler Implementation
 * ============================================ */
typedef struct {
    int socket_fd;
    int is_connected;
} TcpPrivateData;

static int tcp_connect(ProtocolHandler* self, const char* host)
{
    TcpPrivateData* data = (TcpPrivateData*)self->private_data;
    printf("[TCP] Connecting to %s:%d...\n", host, self->port);
    data->is_connected = 1;
    data->socket_fd = 100;  /* Simulated socket */
    printf("[TCP] Connected successfully (socket_fd=%d)\n", data->socket_fd);
    return 0;
}

static int tcp_send(ProtocolHandler* self, const char* data, int len)
{
    printf("[TCP] Sending %d bytes: %s\n", len, data);
    return len;
}

static int tcp_receive(ProtocolHandler* self, char* buf, int max_len)
{
    const char* response = "TCP Response Data";
    strncpy(buf, response, max_len - 1);
    printf("[TCP] Received: %s\n", buf);
    return strlen(response);
}

static void tcp_disconnect(ProtocolHandler* self)
{
    TcpPrivateData* data = (TcpPrivateData*)self->private_data;
    printf("[TCP] Disconnecting (socket_fd=%d)...\n", data->socket_fd);
    data->is_connected = 0;
    data->socket_fd = -1;
}

static void tcp_destroy(ProtocolHandler* self)
{
    printf("[TCP] Destroying handler\n");
    if (self->private_data) {
        free(self->private_data);
    }
    free(self);
}

/* Factory function for TCP handler */
static ProtocolHandler* create_tcp_handler(int port)
{
    ProtocolHandler* handler = (ProtocolHandler*)malloc(sizeof(ProtocolHandler));
    if (handler == NULL) return NULL;
    
    TcpPrivateData* data = (TcpPrivateData*)malloc(sizeof(TcpPrivateData));
    if (data == NULL) {
        free(handler);
        return NULL;
    }
    
    /* Initialize handler */
    strncpy(handler->name, "TCP Handler", sizeof(handler->name) - 1);
    handler->port = port;
    handler->private_data = data;
    
    /* Assign function pointers (virtual functions) */
    handler->connect = tcp_connect;
    handler->send = tcp_send;
    handler->receive = tcp_receive;
    handler->disconnect = tcp_disconnect;
    handler->destroy = tcp_destroy;
    
    /* Initialize private data */
    data->socket_fd = -1;
    data->is_connected = 0;
    
    printf("[Factory] Created TCP handler on port %d\n", port);
    return handler;
}

/* ============================================
 * UDP Handler Implementation
 * ============================================ */
typedef struct {
    int socket_fd;
    char remote_addr[64];
} UdpPrivateData;

static int udp_connect(ProtocolHandler* self, const char* host)
{
    UdpPrivateData* data = (UdpPrivateData*)self->private_data;
    printf("[UDP] Setting remote endpoint to %s:%d...\n", host, self->port);
    strncpy(data->remote_addr, host, sizeof(data->remote_addr) - 1);
    data->socket_fd = 200;  /* Simulated socket */
    printf("[UDP] Ready to send (socket_fd=%d)\n", data->socket_fd);
    return 0;
}

static int udp_send(ProtocolHandler* self, const char* data, int len)
{
    printf("[UDP] Sending datagram (%d bytes): %s\n", len, data);
    return len;
}

static int udp_receive(ProtocolHandler* self, char* buf, int max_len)
{
    const char* response = "UDP Datagram Response";
    strncpy(buf, response, max_len - 1);
    printf("[UDP] Received datagram: %s\n", buf);
    return strlen(response);
}

static void udp_disconnect(ProtocolHandler* self)
{
    UdpPrivateData* data = (UdpPrivateData*)self->private_data;
    printf("[UDP] Closing socket (socket_fd=%d)...\n", data->socket_fd);
    data->socket_fd = -1;
}

static void udp_destroy(ProtocolHandler* self)
{
    printf("[UDP] Destroying handler\n");
    if (self->private_data) {
        free(self->private_data);
    }
    free(self);
}

/* Factory function for UDP handler */
static ProtocolHandler* create_udp_handler(int port)
{
    ProtocolHandler* handler = (ProtocolHandler*)malloc(sizeof(ProtocolHandler));
    if (handler == NULL) return NULL;
    
    UdpPrivateData* data = (UdpPrivateData*)malloc(sizeof(UdpPrivateData));
    if (data == NULL) {
        free(handler);
        return NULL;
    }
    
    strncpy(handler->name, "UDP Handler", sizeof(handler->name) - 1);
    handler->port = port;
    handler->private_data = data;
    
    handler->connect = udp_connect;
    handler->send = udp_send;
    handler->receive = udp_receive;
    handler->disconnect = udp_disconnect;
    handler->destroy = udp_destroy;
    
    data->socket_fd = -1;
    memset(data->remote_addr, 0, sizeof(data->remote_addr));
    
    printf("[Factory] Created UDP handler on port %d\n", port);
    return handler;
}

/* ============================================
 * HTTP Handler Implementation
 * ============================================ */
typedef struct {
    char base_url[256];
    char session_token[64];
    int  keep_alive;
} HttpPrivateData;

static int http_connect(ProtocolHandler* self, const char* host)
{
    HttpPrivateData* data = (HttpPrivateData*)self->private_data;
    snprintf(data->base_url, sizeof(data->base_url), "http://%s:%d", host, self->port);
    printf("[HTTP] Establishing connection to %s...\n", data->base_url);
    strncpy(data->session_token, "TOKEN123", sizeof(data->session_token) - 1);
    data->keep_alive = 1;
    printf("[HTTP] Connected with session: %s\n", data->session_token);
    return 0;
}

static int http_send(ProtocolHandler* self, const char* req_data, int len)
{
    HttpPrivateData* data = (HttpPrivateData*)self->private_data;
    printf("[HTTP] POST %s\n", data->base_url);
    printf("[HTTP] Headers: Content-Length: %d, Session: %s\n", len, data->session_token);
    printf("[HTTP] Body: %s\n", req_data);
    return len;
}

static int http_receive(ProtocolHandler* self, char* buf, int max_len)
{
    const char* response = "HTTP/1.1 200 OK\r\n\r\n{\"status\":\"success\"}";
    strncpy(buf, response, max_len - 1);
    printf("[HTTP] Response: %s\n", buf);
    return strlen(response);
}

static void http_disconnect(ProtocolHandler* self)
{
    HttpPrivateData* data = (HttpPrivateData*)self->private_data;
    printf("[HTTP] Closing connection (keep_alive=%d)...\n", data->keep_alive);
    data->keep_alive = 0;
    memset(data->session_token, 0, sizeof(data->session_token));
}

static void http_destroy(ProtocolHandler* self)
{
    printf("[HTTP] Destroying handler\n");
    if (self->private_data) {
        free(self->private_data);
    }
    free(self);
}

/* Factory function for HTTP handler */
static ProtocolHandler* create_http_handler(int port)
{
    ProtocolHandler* handler = (ProtocolHandler*)malloc(sizeof(ProtocolHandler));
    if (handler == NULL) return NULL;
    
    HttpPrivateData* data = (HttpPrivateData*)malloc(sizeof(HttpPrivateData));
    if (data == NULL) {
        free(handler);
        return NULL;
    }
    
    strncpy(handler->name, "HTTP Handler", sizeof(handler->name) - 1);
    handler->port = port;
    handler->private_data = data;
    
    handler->connect = http_connect;
    handler->send = http_send;
    handler->receive = http_receive;
    handler->disconnect = http_disconnect;
    handler->destroy = http_destroy;
    
    memset(data, 0, sizeof(HttpPrivateData));
    
    printf("[Factory] Created HTTP handler on port %d\n", port);
    return handler;
}

/* ============================================
 * Main Factory Function
 * This is the core of Factory pattern
 * ============================================ */
ProtocolHandler* protocol_handler_create(ProtocolType type, int port)
{
    printf("\n[Factory] Creating handler for type %d...\n", type);
    
    switch (type) {
        case PROTOCOL_TCP:
            return create_tcp_handler(port);       /* Create TCP handler */
            
        case PROTOCOL_UDP:
            return create_udp_handler(port);       /* Create UDP handler */
            
        case PROTOCOL_HTTP:
            return create_http_handler(port);      /* Create HTTP handler */
            
        default:
            printf("[Factory] Unknown protocol type: %d\n", type);
            return NULL;
    }
}

/* ============================================
 * Helper function to use handler uniformly
 * ============================================ */
void use_handler(ProtocolHandler* handler, const char* host, const char* message)
{
    char response[256];
    
    if (handler == NULL) {
        printf("Error: NULL handler\n");
        return;
    }
    
    printf("\n--- Using %s ---\n", handler->name);
    
    /* All handlers are used the same way through the interface */
    handler->connect(handler, host);
    handler->send(handler, message, strlen(message));
    handler->receive(handler, response, sizeof(response));
    handler->disconnect(handler);
}

/* ============================================
 * Main Function - Demonstration
 * ============================================ */
int main(void)
{
    printf("=== Factory Pattern Demo ===\n");
    
    /* Create different handlers using the factory */
    ProtocolHandler* tcp_handler = protocol_handler_create(PROTOCOL_TCP, 8080);
    ProtocolHandler* udp_handler = protocol_handler_create(PROTOCOL_UDP, 9090);
    ProtocolHandler* http_handler = protocol_handler_create(PROTOCOL_HTTP, 80);
    
    /* Use all handlers uniformly - client doesn't care about implementation */
    use_handler(tcp_handler, "192.168.1.100", "Hello TCP!");
    use_handler(udp_handler, "192.168.1.100", "Hello UDP!");
    use_handler(http_handler, "api.example.com", "{\"action\":\"test\"}");
    
    /* Cleanup - all handlers destroyed the same way */
    printf("\n--- Cleanup ---\n");
    tcp_handler->destroy(tcp_handler);
    udp_handler->destroy(udp_handler);
    http_handler->destroy(http_handler);
    
    printf("\n=== Demo Complete ===\n");
    return 0;
}
```

**中文说明：**

完整示例代码实现了一个协议处理器工厂：

1. **通用接口定义**：
   - `ProtocolHandler` 结构体定义了所有协议处理器的通用接口
   - 使用函数指针实现多态：`connect`、`send`、`receive`、`disconnect`、`destroy`

2. **具体产品实现**：
   - **TCP Handler**：实现 TCP 协议的连接和数据传输
   - **UDP Handler**：实现 UDP 无连接数据报传输
   - **HTTP Handler**：实现 HTTP 请求/响应处理

3. **工厂函数**：
   - `protocol_handler_create()`：根据类型参数创建对应的处理器
   - 封装了所有创建细节，客户端只需指定类型

4. **客户端使用**：
   - `use_handler()` 展示了统一使用方式
   - 客户端代码完全不知道具体使用的是哪种协议
   - 通过函数指针调用，实现运行时多态

