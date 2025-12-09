# Factory Pattern in Linux Kernel

## 1. Pattern Overview

```
+------------------------------------------------------------------+
|                      FACTORY PATTERN                              |
+------------------------------------------------------------------+
|                                                                   |
|    Client Code                                                    |
|    +------------------+                                           |
|    |   Application    |                                           |
|    +--------+---------+                                           |
|             |                                                     |
|             | request_object("type_name")                         |
|             v                                                     |
|    +--------+------------------+                                  |
|    |      Factory Function     |  <-- Hides creation details      |
|    +---------------------------+                                  |
|    | create_object(type, args) |                                  |
|    +--------+------------------+                                  |
|             |                                                     |
|             | (internally selects appropriate constructor)        |
|             v                                                     |
|    +--------+--------+    +----------------+   +----------------+ |
|    | Product Type A  |    | Product Type B |   | Product Type C | |
|    +-----------------+    +----------------+   +----------------+ |
|                                                                   |
|    All products share common interface but different impl.        |
|                                                                   |
+------------------------------------------------------------------+
```

**中文说明：** 工厂模式通过统一的工厂函数创建对象，隐藏具体实现细节，降低模块间耦合。客户端代码只需知道需要什么类型的对象，而不需要了解对象的创建过程。Linux内核广泛使用这种模式来创建网络套接字、加密算法实例、内存缓存等。

---

## 2. Linux Kernel Implementation

### 2.1 Kernel Example: Socket Creation Factory

```c
/* From: net/socket.c */

/**
 * __sock_create - creates a socket
 * @net: network namespace
 * @family: protocol family (PF_INET, PF_UNIX, etc.)
 * @type: socket type (SOCK_STREAM, SOCK_DGRAM, etc.)
 * @protocol: specific protocol
 * @res: pointer to store the created socket
 * @kern: kernel socket flag
 *
 * Factory function that creates sockets based on protocol family.
 * Each family has its own create() method registered in net_families[].
 */
int __sock_create(struct net *net, int family, int type, int protocol,
                  struct socket **res, int kern)
{
    int err;
    struct socket *sock;
    const struct net_proto_family *pf;

    /* Validate protocol family range */
    if (family < 0 || family >= NPROTO)
        return -EAFNOSUPPORT;
    if (type < 0 || type >= SOCK_MAX)
        return -EINVAL;

    /* Allocate the socket structure */
    sock = sock_alloc();
    if (!sock) {
        return -ENFILE;
    }

    sock->type = type;

    /* Look up the protocol family - this is the "factory registry" */
    rcu_read_lock();
    pf = rcu_dereference(net_families[family]);
    if (!pf)
        goto out_release;

    /* Call the family-specific create() method - polymorphic creation */
    err = pf->create(net, sock, protocol, kern);
    if (err < 0)
        goto out_module_put;

    *res = sock;
    return 0;
    /* ... error handling ... */
}
```

### 2.2 Kernel Example: Crypto Algorithm Factory

```c
/* From: crypto/api.c */

/**
 * crypto_alloc_tfm - Locate algorithm and allocate transform
 * @alg_name: Name of algorithm (e.g., "aes", "sha256")
 * @frontend: Frontend algorithm type
 * @type: Type of algorithm
 * @mask: Mask for type comparison
 *
 * Factory function for cryptographic algorithm instances.
 * Searches registered algorithms by name and creates appropriate transform.
 */
void *crypto_alloc_tfm(const char *alg_name,
                       const struct crypto_type *frontend, u32 type, u32 mask)
{
    void *tfm;
    int err;

    for (;;) {
        struct crypto_alg *alg;

        /* Find the algorithm by name - factory lookup */
        alg = crypto_find_alg(alg_name, frontend, type, mask);
        if (IS_ERR(alg)) {
            err = PTR_ERR(alg);
            goto err;
        }

        /* Create the transform instance - product creation */
        tfm = crypto_create_tfm(alg, frontend);
        if (!IS_ERR(tfm))
            return tfm;

        crypto_mod_put(alg);
        err = PTR_ERR(tfm);
        /* ... retry logic ... */
    }

    return ERR_PTR(err);
}

/**
 * crypto_create_tfm - Create transform from algorithm
 * @alg: Algorithm to use
 * @frontend: Frontend type information
 *
 * Allocates memory and initializes the transform structure.
 */
void *crypto_create_tfm(struct crypto_alg *alg,
                        const struct crypto_type *frontend)
{
    char *mem;
    struct crypto_tfm *tfm = NULL;
    unsigned int tfmsize;
    unsigned int total;

    /* Calculate sizes and allocate */
    tfmsize = frontend->tfmsize;
    total = tfmsize + sizeof(*tfm) + frontend->extsize(alg);

    mem = kzalloc(total, GFP_KERNEL);
    if (mem == NULL)
        goto out_err;

    tfm = (struct crypto_tfm *)(mem + tfmsize);
    tfm->__crt_alg = alg;

    /* Initialize through frontend - polymorphic initialization */
    err = frontend->init_tfm(tfm);
    if (err)
        goto out_free_tfm;

    return mem;
}
```

### 2.3 Kernel Example: kmem_cache Factory

```c
/* From: mm/slob.c */

/**
 * kmem_cache_create - Create a cache for objects of a particular size
 * @name: Identifier for the cache
 * @size: Size of objects in this cache
 * @align: Alignment requirements
 * @flags: SLAB flags
 * @ctor: Constructor function called on each allocation
 *
 * Factory for creating memory caches that efficiently allocate
 * fixed-size objects.
 */
struct kmem_cache *kmem_cache_create(const char *name, size_t size,
    size_t align, unsigned long flags, void (*ctor)(void *))
{
    struct kmem_cache *c;

    /* Allocate the cache structure */
    c = slob_alloc(sizeof(struct kmem_cache),
        GFP_KERNEL, ARCH_KMALLOC_MINALIGN, -1);

    if (c) {
        c->name = name;
        c->size = size;
        c->flags = flags;
        c->ctor = ctor;   /* Store constructor for later use */
        c->align = (flags & SLAB_HWCACHE_ALIGN) ? SLOB_ALIGN : 0;
    }
    
    return c;
}

/**
 * kmem_cache_alloc_node - Allocate an object from the cache
 * @c: The cache to allocate from
 * @flags: Allocation flags
 * @node: NUMA node
 *
 * Factory method that creates objects using the registered constructor.
 */
void *kmem_cache_alloc_node(struct kmem_cache *c, gfp_t flags, int node)
{
    void *b;

    if (c->size < PAGE_SIZE) {
        b = slob_alloc(c->size, flags, c->align, node);
    } else {
        b = slob_new_pages(flags, get_order(c->size), node);
    }

    /* Call constructor if registered - product initialization */
    if (c->ctor)
        c->ctor(b);

    return b;
}
```

### 2.4 Architecture Diagram

```
+------------------------------------------------------------------+
|                 LINUX KERNEL FACTORY ARCHITECTURE                 |
+------------------------------------------------------------------+
|                                                                   |
|   User Request                   Factory Subsystem                |
|   +------------+                                                  |
|   | socket()   |                                                  |
|   | syscall    |                                                  |
|   +-----+------+                                                  |
|         |                                                         |
|         v                                                         |
|   +-----+-----------+    +----------------------------------+     |
|   | __sock_create() |--->|  net_families[] Registry         |     |
|   | (Factory Func)  |    +----------------------------------+     |
|   +-----+-----------+    | [PF_INET]  -> inet_create()      |     |
|         |                | [PF_UNIX]  -> unix_create()      |     |
|         |                | [PF_PACKET]-> packet_create()    |     |
|         |                +----------------------------------+     |
|         v                                                         |
|   +-----+-----------+                                             |
|   | pf->create()    |  <-- Polymorphic call to specific factory   |
|   +-----+-----------+                                             |
|         |                                                         |
|    +----+----+----+----+                                          |
|    |         |         |                                          |
|    v         v         v                                          |
| +------+ +------+ +--------+                                      |
| | INET | | UNIX | | PACKET |  <-- Different product types         |
| |Socket| |Socket| | Socket |                                      |
| +------+ +------+ +--------+                                      |
|                                                                   |
|   Common Interface: sock->ops->bind(), connect(), send(), recv()  |
|                                                                   |
+------------------------------------------------------------------+
```

**中文说明：** Linux内核的socket工厂使用注册表（net_families数组）存储不同协议族的创建函数。当用户调用socket()系统调用时，工厂函数根据协议族参数查找对应的创建函数，从而创建正确类型的socket。所有socket都实现相同的操作接口，但内部实现各不相同。

---

## 3. Advantages Analysis

| Advantage | Description |
|-----------|-------------|
| **Decoupling** | Clients don't need to know concrete product classes |
| **Extensibility** | New product types can be added without modifying client code |
| **Encapsulation** | Complex creation logic is hidden in factory functions |
| **Consistent Interface** | All products implement same interface, enabling polymorphism |
| **Runtime Selection** | Product type can be determined at runtime based on parameters |
| **Resource Management** | Factory can manage object pools and caching |

**中文说明：** 工厂模式的优势包括：解耦（客户端不需要知道具体产品类）、可扩展性（添加新产品类型无需修改客户端）、封装性（隐藏复杂创建逻辑）、一致接口（所有产品实现相同接口）、运行时选择（根据参数动态决定产品类型）、资源管理（工厂可以管理对象池和缓存）。

---

## 4. User-Space Implementation Example

```c
/*
 * Factory Pattern - User Space Implementation
 * Mimics Linux Kernel's socket creation factory pattern
 * 
 * Compile: gcc -o factory factory.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * Product Interface - Common operations for all products
 * Similar to kernel's struct proto_ops for sockets
 * ============================================================ */

/* Forward declaration */
struct connection;

/* Operations structure - virtual function table */
struct connection_ops {
    int (*connect)(struct connection *conn, const char *address);
    int (*send)(struct connection *conn, const char *data, int len);
    int (*receive)(struct connection *conn, char *buf, int len);
    void (*close)(struct connection *conn);
    const char *name;
};

/* Base connection structure - like kernel's struct socket */
struct connection {
    const struct connection_ops *ops;   /* Operations vtable */
    int fd;                              /* File descriptor */
    char address[256];                   /* Connection address */
    void *private_data;                  /* Protocol-specific data */
};

/* ============================================================
 * Concrete Products - Different connection types
 * Similar to INET, UNIX, PACKET sockets in kernel
 * ============================================================ */

/* ------------ TCP Connection Implementation ------------ */

struct tcp_private {
    int port;
    int keep_alive;
};

static int tcp_connect(struct connection *conn, const char *address)
{
    struct tcp_private *priv = conn->private_data;
    printf("[TCP] Connecting to %s (keep_alive=%d)\n", address, priv->keep_alive);
    strncpy(conn->address, address, sizeof(conn->address) - 1);
    return 0;
}

static int tcp_send(struct connection *conn, const char *data, int len)
{
    printf("[TCP] Sending %d bytes to %s: %s\n", len, conn->address, data);
    return len;
}

static int tcp_receive(struct connection *conn, char *buf, int len)
{
    const char *response = "TCP_RESPONSE";
    strncpy(buf, response, len - 1);
    printf("[TCP] Received from %s: %s\n", conn->address, buf);
    return strlen(response);
}

static void tcp_close(struct connection *conn)
{
    printf("[TCP] Closing connection to %s\n", conn->address);
    free(conn->private_data);
}

/* TCP operations vtable */
static const struct connection_ops tcp_ops = {
    .connect = tcp_connect,
    .send = tcp_send,
    .receive = tcp_receive,
    .close = tcp_close,
    .name = "TCP"
};

/* ------------ UDP Connection Implementation ------------ */

struct udp_private {
    int broadcast_enabled;
};

static int udp_connect(struct connection *conn, const char *address)
{
    struct udp_private *priv = conn->private_data;
    printf("[UDP] Setting peer to %s (broadcast=%d)\n", 
           address, priv->broadcast_enabled);
    strncpy(conn->address, address, sizeof(conn->address) - 1);
    return 0;
}

static int udp_send(struct connection *conn, const char *data, int len)
{
    printf("[UDP] Sending datagram %d bytes to %s: %s\n", 
           len, conn->address, data);
    return len;
}

static int udp_receive(struct connection *conn, char *buf, int len)
{
    const char *response = "UDP_DATAGRAM";
    strncpy(buf, response, len - 1);
    printf("[UDP] Received datagram from %s: %s\n", conn->address, buf);
    return strlen(response);
}

static void udp_close(struct connection *conn)
{
    printf("[UDP] Closing socket\n");
    free(conn->private_data);
}

/* UDP operations vtable */
static const struct connection_ops udp_ops = {
    .connect = udp_connect,
    .send = udp_send,
    .receive = udp_receive,
    .close = udp_close,
    .name = "UDP"
};

/* ------------ Unix Domain Socket Implementation ------------ */

struct unix_private {
    char socket_path[256];
};

static int unix_connect(struct connection *conn, const char *address)
{
    struct unix_private *priv = conn->private_data;
    printf("[UNIX] Connecting to socket %s\n", address);
    strncpy(priv->socket_path, address, sizeof(priv->socket_path) - 1);
    strncpy(conn->address, address, sizeof(conn->address) - 1);
    return 0;
}

static int unix_send(struct connection *conn, const char *data, int len)
{
    printf("[UNIX] Sending %d bytes via domain socket: %s\n", len, data);
    return len;
}

static int unix_receive(struct connection *conn, char *buf, int len)
{
    const char *response = "UNIX_LOCAL_DATA";
    strncpy(buf, response, len - 1);
    printf("[UNIX] Received: %s\n", buf);
    return strlen(response);
}

static void unix_close(struct connection *conn)
{
    struct unix_private *priv = conn->private_data;
    printf("[UNIX] Closing socket %s\n", priv->socket_path);
    free(conn->private_data);
}

/* Unix operations vtable */
static const struct connection_ops unix_ops = {
    .connect = unix_connect,
    .send = unix_send,
    .receive = unix_receive,
    .close = unix_close,
    .name = "UNIX"
};

/* ============================================================
 * Factory Registration System
 * Similar to kernel's net_families[] registry
 * ============================================================ */

/* Connection types - like kernel's PF_INET, PF_UNIX, etc. */
enum connection_type {
    CONN_TYPE_TCP = 0,
    CONN_TYPE_UDP,
    CONN_TYPE_UNIX,
    CONN_TYPE_MAX
};

/* Factory function type */
typedef struct connection *(*conn_create_fn)(void);

/* Factory registry entry */
struct connection_factory {
    conn_create_fn create;
    const char *name;
};

/* TCP factory function */
static struct connection *create_tcp_connection(void)
{
    struct connection *conn = malloc(sizeof(struct connection));
    struct tcp_private *priv = malloc(sizeof(struct tcp_private));
    
    if (!conn || !priv) {
        free(conn);
        free(priv);
        return NULL;
    }
    
    /* Initialize TCP-specific data */
    priv->port = 0;
    priv->keep_alive = 1;
    
    /* Set up the connection */
    conn->ops = &tcp_ops;
    conn->fd = -1;
    conn->private_data = priv;
    memset(conn->address, 0, sizeof(conn->address));
    
    printf("[FACTORY] Created TCP connection\n");
    return conn;
}

/* UDP factory function */
static struct connection *create_udp_connection(void)
{
    struct connection *conn = malloc(sizeof(struct connection));
    struct udp_private *priv = malloc(sizeof(struct udp_private));
    
    if (!conn || !priv) {
        free(conn);
        free(priv);
        return NULL;
    }
    
    priv->broadcast_enabled = 0;
    
    conn->ops = &udp_ops;
    conn->fd = -1;
    conn->private_data = priv;
    memset(conn->address, 0, sizeof(conn->address));
    
    printf("[FACTORY] Created UDP connection\n");
    return conn;
}

/* Unix domain socket factory function */
static struct connection *create_unix_connection(void)
{
    struct connection *conn = malloc(sizeof(struct connection));
    struct unix_private *priv = malloc(sizeof(struct unix_private));
    
    if (!conn || !priv) {
        free(conn);
        free(priv);
        return NULL;
    }
    
    memset(priv->socket_path, 0, sizeof(priv->socket_path));
    
    conn->ops = &unix_ops;
    conn->fd = -1;
    conn->private_data = priv;
    memset(conn->address, 0, sizeof(conn->address));
    
    printf("[FACTORY] Created UNIX connection\n");
    return conn;
}

/* Factory registry - like kernel's net_families[] */
static struct connection_factory conn_factories[CONN_TYPE_MAX] = {
    [CONN_TYPE_TCP]  = { .create = create_tcp_connection,  .name = "TCP" },
    [CONN_TYPE_UDP]  = { .create = create_udp_connection,  .name = "UDP" },
    [CONN_TYPE_UNIX] = { .create = create_unix_connection, .name = "UNIX" },
};

/* ============================================================
 * Public Factory Function
 * Similar to kernel's __sock_create()
 * ============================================================ */

/**
 * connection_create - Factory function to create connections
 * @type: Type of connection (TCP, UDP, UNIX)
 *
 * This is the main factory interface. Clients call this function
 * with a type, and receive a fully initialized connection object.
 * The client doesn't need to know about the specific implementations.
 *
 * Returns: Pointer to new connection, or NULL on error
 */
struct connection *connection_create(enum connection_type type)
{
    /* Validate type */
    if (type < 0 || type >= CONN_TYPE_MAX) {
        fprintf(stderr, "[FACTORY] Invalid connection type: %d\n", type);
        return NULL;
    }
    
    /* Look up factory in registry */
    if (conn_factories[type].create == NULL) {
        fprintf(stderr, "[FACTORY] No factory registered for type: %d\n", type);
        return NULL;
    }
    
    printf("[FACTORY] Creating %s connection...\n", conn_factories[type].name);
    
    /* Call the registered factory function */
    return conn_factories[type].create();
}

/**
 * connection_destroy - Cleanup a connection
 * @conn: Connection to destroy
 *
 * Calls the appropriate close method and frees the connection.
 */
void connection_destroy(struct connection *conn)
{
    if (conn) {
        if (conn->ops && conn->ops->close) {
            conn->ops->close(conn);
        }
        free(conn);
    }
}

/* ============================================================
 * Client Code - Uses factory without knowing implementation details
 * ============================================================ */

void test_connection(struct connection *conn, const char *address)
{
    char buffer[256];
    
    if (!conn) return;
    
    printf("\n--- Testing %s Connection ---\n", conn->ops->name);
    
    /* Use common interface - works with any connection type */
    conn->ops->connect(conn, address);
    conn->ops->send(conn, "Hello, World!", 13);
    conn->ops->receive(conn, buffer, sizeof(buffer));
    
    printf("----------------------------\n\n");
}

int main(void)
{
    struct connection *tcp_conn;
    struct connection *udp_conn;
    struct connection *unix_conn;

    printf("=== Factory Pattern Demo ===\n\n");

    /* Create different connection types using the same factory interface */
    tcp_conn = connection_create(CONN_TYPE_TCP);
    udp_conn = connection_create(CONN_TYPE_UDP);
    unix_conn = connection_create(CONN_TYPE_UNIX);

    /* Test each connection - using polymorphic interface */
    test_connection(tcp_conn, "192.168.1.1:8080");
    test_connection(udp_conn, "192.168.1.1:9000");
    test_connection(unix_conn, "/var/run/myapp.sock");

    /* Cleanup */
    connection_destroy(tcp_conn);
    connection_destroy(udp_conn);
    connection_destroy(unix_conn);

    printf("=== Demo Complete ===\n");
    return 0;
}
```

---

## 5. Factory Pattern Flow Diagram

```
+------------------------------------------------------------------+
|                    FACTORY PATTERN FLOW                           |
+------------------------------------------------------------------+
|                                                                   |
|    Client Request                                                 |
|    +--------------------+                                         |
|    | connection_create  |                                         |
|    | (CONN_TYPE_TCP)    |                                         |
|    +--------+-----------+                                         |
|             |                                                     |
|             v                                                     |
|    +--------+-----------+                                         |
|    | Validate type      |                                         |
|    | (0 <= type < MAX)  |                                         |
|    +--------+-----------+                                         |
|             |                                                     |
|             v                                                     |
|    +--------+------------------+                                  |
|    |  Lookup in Registry       |                                  |
|    |  conn_factories[type]     |                                  |
|    +--------+------------------+                                  |
|             |                                                     |
|             v                                                     |
|    +--------+------------------+                                  |
|    | Call factory function     |                                  |
|    | create_tcp_connection()   |                                  |
|    +--------+------------------+                                  |
|             |                                                     |
|             v                                                     |
|    +--------+------------------+                                  |
|    | Allocate connection       |                                  |
|    | Allocate private data     |                                  |
|    | Set operations vtable     |                                  |
|    | Initialize fields         |                                  |
|    +--------+------------------+                                  |
|             |                                                     |
|             v                                                     |
|    +--------+------------------+                                  |
|    | Return connection ptr     |                                  |
|    | (struct connection *)     |                                  |
|    +---------------------------+                                  |
|                                                                   |
|    Client uses common interface:                                  |
|    conn->ops->connect()                                           |
|    conn->ops->send()                                              |
|    conn->ops->receive()                                           |
|    conn->ops->close()                                             |
|                                                                   |
+------------------------------------------------------------------+
```

**中文说明：** 工厂模式的执行流程：客户端调用工厂函数并传入类型参数，工厂验证参数有效性后在注册表中查找对应的创建函数，调用创建函数分配内存并初始化对象，最后返回指向产品的指针。客户端通过统一接口使用产品，无需关心具体类型。

---

## 6. Key Implementation Points

1. **Registry Pattern**: Maintain a table mapping type IDs to factory functions
2. **Common Interface**: All products implement the same operations structure
3. **Encapsulated Creation**: Complex initialization hidden in factory functions
4. **Type-Specific Data**: Use private_data pointer for type-specific fields
5. **Error Handling**: Factory returns NULL or error code on failure
6. **Extensibility**: New types can be added by registering new factory functions

**中文说明：** 实现工厂模式的关键点：使用注册表模式维护类型到工厂函数的映射、所有产品实现相同的操作接口、复杂初始化逻辑封装在工厂函数中、使用 private_data 存储类型特定数据、工厂返回NULL或错误码处理失败情况、通过注册新工厂函数实现扩展。

