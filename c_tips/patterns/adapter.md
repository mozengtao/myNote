# Adapter Pattern (适配器模式)

## 1. Core Concept and Use Cases

### Core Concept
Convert the interface of a class into another interface that clients expect. Adapter lets classes work together that couldn't otherwise because of **incompatible interfaces**.

### Typical Use Cases
- Integrating third-party libraries
- Legacy code compatibility
- Different protocol adaptation
- API version migration
- Hardware abstraction layers

---

## 2. Architecture Diagram

```
+--------------------------------------------------------------------------------------------------+
|                                      Adapter Pattern                                              |
+--------------------------------------------------------------------------------------------------+

    +-------------+                                              
    |   Client    |     Uses Target Interface                    
    +------+------+                                              
           |                                                     
           | calls target_operation()                           
           v                                                     
    +------------------+                              +------------------+
    | Target Interface |                              |     Adaptee      |
    +------------------+                              | (Incompatible)   |
    | + operation()    |                              +------------------+
    +--------+---------+                              | + specific_op()  |
             |                                        | + legacy_func()  |
             |                                        +--------+---------+
             v                                                  ^
    +---------------------------+                               |
    |         Adapter           |-------------------------------+
    +---------------------------+        delegates to
    | - adaptee: Adaptee*       |
    +---------------------------+
    | + operation() {           |
    |     adaptee->specific_op()|
    |   }                       |
    +---------------------------+


    Example: Old API to New API Adapter
    
    +-------------------+           +-------------------+           +-------------------+
    |    New Client     |           |      Adapter      |           |     Old Library   |
    +-------------------+           +-------------------+           +-------------------+
    | Uses new_send()   |---------->| new_send() {      |---------->| old_transmit()    |
    |                   |           |   old_transmit()  |           |                   |
    +-------------------+           | }                 |           +-------------------+
                                    +-------------------+
```

**中文说明：**

适配器模式的核心流程：

1. **目标接口（Target）**：
   - 客户端期望使用的接口
   - 定义客户端调用的方法

2. **被适配者（Adaptee）**：
   - 已存在的类/库，接口不兼容
   - 有实际功能但接口不同

3. **适配器（Adapter）**：
   - 包装 Adaptee
   - 实现 Target 接口
   - 内部调用 Adaptee 的方法

---

## 3. Code Skeleton

```c
/* Target interface (what client expects) */
typedef struct {
    int (*send)(void* self, const char* data, int len);
    int (*receive)(void* self, char* buf, int max_len);
    void (*close)(void* self);
} TargetInterface;

/* Adaptee (existing incompatible interface) */
typedef struct {
    int (*transmit)(const char* data);
    int (*fetch)(char* buf);
} LegacyInterface;

/* Adapter */
typedef struct {
    TargetInterface target;
    LegacyInterface* adaptee;
} Adapter;

Adapter* create_adapter(LegacyInterface* legacy);
```

**中文说明：**

代码骨架包含：
- `TargetInterface`：目标接口，客户端期望的接口
- `LegacyInterface`：被适配者，已存在的不兼容接口
- `Adapter`：适配器，包装被适配者并实现目标接口

---

## 4. Complete Example Code

```c
/*
 * Adapter Pattern - Legacy Network Library Example
 * 
 * This example demonstrates adapting an old network
 * library to work with a new unified interface.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================
 * Target Interface (New API that clients use)
 * ============================================ */
typedef struct NetworkDevice NetworkDevice;

struct NetworkDevice {
    char name[32];
    
    /* New unified interface */
    int  (*connect)(NetworkDevice* self, const char* address, int port);
    int  (*send)(NetworkDevice* self, const void* data, int length);
    int  (*receive)(NetworkDevice* self, void* buffer, int max_length);
    void (*disconnect)(NetworkDevice* self);
    void (*destroy)(NetworkDevice* self);
    
    void* private_data;
};

/* ============================================
 * Adaptee 1: Legacy TCP Library
 * (Old interface that doesn't match new API)
 * ============================================ */
typedef struct {
    int  socket_fd;
    char host[64];
    int  port;
    int  connected;
} LegacyTcpSocket;

/* Old TCP library functions (incompatible signatures) */
LegacyTcpSocket* legacy_tcp_create(void)
{
    LegacyTcpSocket* sock = (LegacyTcpSocket*)malloc(sizeof(LegacyTcpSocket));
    if (sock) {
        sock->socket_fd = -1;
        sock->connected = 0;
        memset(sock->host, 0, sizeof(sock->host));
        sock->port = 0;
    }
    printf("    [LegacyTCP] Socket created\n");
    return sock;
}

int legacy_tcp_open(LegacyTcpSocket* sock, const char* host, int port)
{
    /* Simulate connection */
    strncpy(sock->host, host, sizeof(sock->host) - 1);
    sock->port = port;
    sock->socket_fd = 100;  /* Simulated FD */
    sock->connected = 1;
    printf("    [LegacyTCP] Opened connection to %s:%d (fd=%d)\n",
           host, port, sock->socket_fd);
    return 0;
}

int legacy_tcp_transmit(LegacyTcpSocket* sock, const char* buffer, int size)
{
    if (!sock->connected) return -1;
    printf("    [LegacyTCP] Transmitting %d bytes\n", size);
    return size;
}

int legacy_tcp_fetch(LegacyTcpSocket* sock, char* buffer, int max_size)
{
    if (!sock->connected) return -1;
    const char* response = "Legacy TCP Response";
    int len = strlen(response);
    if (len > max_size) len = max_size;
    memcpy(buffer, response, len);
    printf("    [LegacyTCP] Fetched %d bytes\n", len);
    return len;
}

void legacy_tcp_shutdown(LegacyTcpSocket* sock)
{
    printf("    [LegacyTCP] Shutting down connection\n");
    sock->connected = 0;
    sock->socket_fd = -1;
}

void legacy_tcp_destroy(LegacyTcpSocket* sock)
{
    printf("    [LegacyTCP] Destroying socket\n");
    free(sock);
}

/* ============================================
 * Adapter for Legacy TCP
 * Adapts old TCP interface to new NetworkDevice
 * ============================================ */
typedef struct {
    LegacyTcpSocket* legacy_socket;  /* The adaptee */
} TcpAdapterData;

int tcp_adapter_connect(NetworkDevice* self, const char* address, int port)
{
    TcpAdapterData* data = (TcpAdapterData*)self->private_data;
    printf("  [TcpAdapter] Adapting connect() -> legacy_tcp_open()\n");
    return legacy_tcp_open(data->legacy_socket, address, port);
}

int tcp_adapter_send(NetworkDevice* self, const void* buf, int length)
{
    TcpAdapterData* data = (TcpAdapterData*)self->private_data;
    printf("  [TcpAdapter] Adapting send() -> legacy_tcp_transmit()\n");
    return legacy_tcp_transmit(data->legacy_socket, (const char*)buf, length);
}

int tcp_adapter_receive(NetworkDevice* self, void* buffer, int max_length)
{
    TcpAdapterData* data = (TcpAdapterData*)self->private_data;
    printf("  [TcpAdapter] Adapting receive() -> legacy_tcp_fetch()\n");
    return legacy_tcp_fetch(data->legacy_socket, (char*)buffer, max_length);
}

void tcp_adapter_disconnect(NetworkDevice* self)
{
    TcpAdapterData* data = (TcpAdapterData*)self->private_data;
    printf("  [TcpAdapter] Adapting disconnect() -> legacy_tcp_shutdown()\n");
    legacy_tcp_shutdown(data->legacy_socket);
}

void tcp_adapter_destroy(NetworkDevice* self)
{
    TcpAdapterData* data = (TcpAdapterData*)self->private_data;
    printf("  [TcpAdapter] Destroying adapter and legacy socket\n");
    legacy_tcp_destroy(data->legacy_socket);
    free(data);
    free(self);
}

/* Factory function to create TCP adapter */
NetworkDevice* create_tcp_adapter(void)
{
    NetworkDevice* device = (NetworkDevice*)malloc(sizeof(NetworkDevice));
    TcpAdapterData* data = (TcpAdapterData*)malloc(sizeof(TcpAdapterData));
    
    if (device == NULL || data == NULL) {
        free(device);
        free(data);
        return NULL;
    }
    
    /* Create the legacy socket (adaptee) */
    data->legacy_socket = legacy_tcp_create();
    if (data->legacy_socket == NULL) {
        free(device);
        free(data);
        return NULL;
    }
    
    /* Setup adapter */
    strncpy(device->name, "TCP-Adapter", sizeof(device->name) - 1);
    device->connect = tcp_adapter_connect;
    device->send = tcp_adapter_send;
    device->receive = tcp_adapter_receive;
    device->disconnect = tcp_adapter_disconnect;
    device->destroy = tcp_adapter_destroy;
    device->private_data = data;
    
    printf("[Create] TCP Adapter wrapping Legacy TCP Socket\n");
    return device;
}

/* ============================================
 * Adaptee 2: Third-party HTTP Library
 * (Another incompatible interface)
 * ============================================ */
typedef struct {
    char base_url[256];
    int  timeout_ms;
    int  is_open;
} ThirdPartyHttpClient;

/* Third-party HTTP functions (different interface) */
ThirdPartyHttpClient* http_client_new(int timeout)
{
    ThirdPartyHttpClient* client = (ThirdPartyHttpClient*)malloc(sizeof(ThirdPartyHttpClient));
    if (client) {
        client->timeout_ms = timeout;
        client->is_open = 0;
        memset(client->base_url, 0, sizeof(client->base_url));
    }
    printf("    [HttpClient] Created with timeout=%dms\n", timeout);
    return client;
}

int http_client_set_base_url(ThirdPartyHttpClient* client, const char* url, int port)
{
    snprintf(client->base_url, sizeof(client->base_url), "http://%s:%d", url, port);
    client->is_open = 1;
    printf("    [HttpClient] Base URL set to %s\n", client->base_url);
    return 0;
}

int http_client_post(ThirdPartyHttpClient* client, const char* body, int body_len)
{
    if (!client->is_open) return -1;
    printf("    [HttpClient] POST to %s with %d bytes\n", client->base_url, body_len);
    return body_len;
}

int http_client_get(ThirdPartyHttpClient* client, char* response, int max_len)
{
    if (!client->is_open) return -1;
    const char* mock_response = "{\"status\":\"ok\",\"data\":\"HTTP Response\"}";
    int len = strlen(mock_response);
    if (len > max_len) len = max_len;
    memcpy(response, mock_response, len);
    printf("    [HttpClient] GET from %s, received %d bytes\n", client->base_url, len);
    return len;
}

void http_client_close(ThirdPartyHttpClient* client)
{
    printf("    [HttpClient] Closing connection\n");
    client->is_open = 0;
}

void http_client_free(ThirdPartyHttpClient* client)
{
    printf("    [HttpClient] Freeing client\n");
    free(client);
}

/* ============================================
 * Adapter for HTTP Client
 * Adapts third-party HTTP to NetworkDevice
 * ============================================ */
typedef struct {
    ThirdPartyHttpClient* http_client;
} HttpAdapterData;

int http_adapter_connect(NetworkDevice* self, const char* address, int port)
{
    HttpAdapterData* data = (HttpAdapterData*)self->private_data;
    printf("  [HttpAdapter] Adapting connect() -> http_client_set_base_url()\n");
    return http_client_set_base_url(data->http_client, address, port);
}

int http_adapter_send(NetworkDevice* self, const void* buf, int length)
{
    HttpAdapterData* data = (HttpAdapterData*)self->private_data;
    printf("  [HttpAdapter] Adapting send() -> http_client_post()\n");
    return http_client_post(data->http_client, (const char*)buf, length);
}

int http_adapter_receive(NetworkDevice* self, void* buffer, int max_length)
{
    HttpAdapterData* data = (HttpAdapterData*)self->private_data;
    printf("  [HttpAdapter] Adapting receive() -> http_client_get()\n");
    return http_client_get(data->http_client, (char*)buffer, max_length);
}

void http_adapter_disconnect(NetworkDevice* self)
{
    HttpAdapterData* data = (HttpAdapterData*)self->private_data;
    printf("  [HttpAdapter] Adapting disconnect() -> http_client_close()\n");
    http_client_close(data->http_client);
}

void http_adapter_destroy(NetworkDevice* self)
{
    HttpAdapterData* data = (HttpAdapterData*)self->private_data;
    printf("  [HttpAdapter] Destroying adapter and HTTP client\n");
    http_client_free(data->http_client);
    free(data);
    free(self);
}

NetworkDevice* create_http_adapter(int timeout_ms)
{
    NetworkDevice* device = (NetworkDevice*)malloc(sizeof(NetworkDevice));
    HttpAdapterData* data = (HttpAdapterData*)malloc(sizeof(HttpAdapterData));
    
    if (device == NULL || data == NULL) {
        free(device);
        free(data);
        return NULL;
    }
    
    data->http_client = http_client_new(timeout_ms);
    if (data->http_client == NULL) {
        free(device);
        free(data);
        return NULL;
    }
    
    strncpy(device->name, "HTTP-Adapter", sizeof(device->name) - 1);
    device->connect = http_adapter_connect;
    device->send = http_adapter_send;
    device->receive = http_adapter_receive;
    device->disconnect = http_adapter_disconnect;
    device->destroy = http_adapter_destroy;
    device->private_data = data;
    
    printf("[Create] HTTP Adapter wrapping Third-party HTTP Client\n");
    return device;
}

/* ============================================
 * Client Code - Uses unified interface
 * ============================================ */
void client_use_device(NetworkDevice* device, const char* host, int port)
{
    char buffer[256];
    const char* message = "Hello from unified interface!";
    
    printf("\n>>> Using device: %s <<<\n", device->name);
    
    /* All devices used the same way through unified interface */
    device->connect(device, host, port);
    device->send(device, message, strlen(message));
    
    memset(buffer, 0, sizeof(buffer));
    device->receive(device, buffer, sizeof(buffer));
    printf("  Received: %s\n", buffer);
    
    device->disconnect(device);
}

/* ============================================
 * Main Function - Demonstration
 * ============================================ */
int main(void)
{
    printf("=== Adapter Pattern Demo ===\n\n");
    
    /* Create adapters for different legacy systems */
    printf("--- Creating Adapters ---\n");
    NetworkDevice* tcp_device = create_tcp_adapter();
    NetworkDevice* http_device = create_http_adapter(5000);
    
    /* Use both devices through the same unified interface */
    printf("\n--- Using Devices (Same Interface) ---\n");
    
    client_use_device(tcp_device, "192.168.1.100", 8080);
    client_use_device(http_device, "api.example.com", 443);
    
    /* Cleanup */
    printf("\n--- Cleanup ---\n");
    tcp_device->destroy(tcp_device);
    http_device->destroy(http_device);
    
    printf("\n=== Demo Complete ===\n");
    return 0;
}
```

**中文说明：**

完整示例代码实现了网络设备适配器：

1. **目标接口（NetworkDevice）**：
   - 统一的网络设备接口
   - 定义 `connect`、`send`、`receive`、`disconnect` 等方法

2. **被适配者**：
   - **Legacy TCP Library**：旧的 TCP 库，使用 `open`、`transmit`、`fetch` 等方法
   - **Third-party HTTP Client**：第三方 HTTP 库，使用 `set_base_url`、`post`、`get` 等方法

3. **适配器实现**：
   - **TcpAdapter**：将 Legacy TCP 接口适配到 NetworkDevice
   - **HttpAdapter**：将 HTTP Client 接口适配到 NetworkDevice

4. **客户端代码**：
   - `client_use_device()` 使用统一接口
   - 不关心底层是 TCP 还是 HTTP
   - 通过适配器透明访问不同的实现

