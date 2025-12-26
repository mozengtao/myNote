# Linux Networking Subsystem: Extracting Reusable Patterns for User Space

## 1. Message-Buffer Objects (sk_buff-like Design)

```
+------------------------------------------------------------------+
|  PATTERN: MESSAGE BUFFER WITH HEADROOM/TAILROOM                  |
+------------------------------------------------------------------+

    PROBLEM SOLVED:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Multiple layers need to add/remove headers              │
    │  • Avoid copying data at each layer boundary               │
    │  • Efficient memory management for messages                │
    │  • Support for message cloning and sharing                 │
    └─────────────────────────────────────────────────────────────┘

    MINIMAL C FEATURES:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Pointer arithmetic (head, data, tail)                   │
    │  • Reference counting                                      │
    │  • Flexible buffer allocation                              │
    └─────────────────────────────────────────────────────────────┘

    WHEN NOT TO USE:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Single-layer message processing                         │
    │  • Fixed-size messages                                     │
    │  • When simplicity is more important than performance      │
    └─────────────────────────────────────────────────────────────┘

    COMPLETE USER-SPACE EXAMPLE:
```

```c
/* msgbuf.c - sk_buff-inspired message buffer */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <stdbool.h>

#define MSGBUF_HEADROOM 64  /* Reserve for headers */
#define MSGBUF_TAILROOM 16  /* Reserve for trailers */

struct msgbuf {
    atomic_int refcnt;       /* Reference count */
    unsigned char *head;     /* Start of buffer */
    unsigned char *data;     /* Start of data */
    unsigned char *tail;     /* End of data */
    unsigned char *end;      /* End of buffer */
    size_t len;              /* Data length */
};

/* Allocate message buffer with headroom */
struct msgbuf *msgbuf_alloc(size_t size) {
    struct msgbuf *mb;
    size_t total = MSGBUF_HEADROOM + size + MSGBUF_TAILROOM;
    
    mb = malloc(sizeof(*mb));
    if (!mb) return NULL;
    
    mb->head = malloc(total);
    if (!mb->head) {
        free(mb);
        return NULL;
    }
    
    atomic_init(&mb->refcnt, 1);
    mb->data = mb->head + MSGBUF_HEADROOM;
    mb->tail = mb->data;
    mb->end = mb->head + total;
    mb->len = 0;
    
    return mb;
}

/* Get reference */
struct msgbuf *msgbuf_get(struct msgbuf *mb) {
    if (mb)
        atomic_fetch_add(&mb->refcnt, 1);
    return mb;
}

/* Release reference */
void msgbuf_put(struct msgbuf *mb) {
    if (!mb) return;
    if (atomic_fetch_sub(&mb->refcnt, 1) == 1) {
        free(mb->head);
        free(mb);
    }
}

/* Prepend space (for adding headers) - moves data pointer left */
unsigned char *msgbuf_push(struct msgbuf *mb, size_t len) {
    if (mb->data - mb->head < (ptrdiff_t)len)
        return NULL;  /* No headroom */
    
    mb->data -= len;
    mb->len += len;
    return mb->data;
}

/* Remove from head (for stripping headers) - moves data pointer right */
unsigned char *msgbuf_pull(struct msgbuf *mb, size_t len) {
    if (mb->len < len)
        return NULL;  /* Not enough data */
    
    mb->data += len;
    mb->len -= len;
    return mb->data;
}

/* Append space (for adding payload) - moves tail pointer right */
unsigned char *msgbuf_put_data(struct msgbuf *mb, size_t len) {
    if (mb->end - mb->tail < (ptrdiff_t)len)
        return NULL;  /* No tailroom */
    
    unsigned char *old_tail = mb->tail;
    mb->tail += len;
    mb->len += len;
    return old_tail;
}

/* Available headroom */
size_t msgbuf_headroom(struct msgbuf *mb) {
    return mb->data - mb->head;
}

/* Available tailroom */
size_t msgbuf_tailroom(struct msgbuf *mb) {
    return mb->end - mb->tail;
}

/* Example: Protocol layering */
struct app_header {
    uint16_t msg_type;
    uint16_t msg_len;
};

struct frame_header {
    uint8_t sync;
    uint8_t version;
    uint16_t total_len;
};

void example_layered_send(void) {
    struct msgbuf *mb = msgbuf_alloc(256);
    
    /* Application layer: add payload */
    const char *payload = "Hello, World!";
    size_t plen = strlen(payload);
    unsigned char *p = msgbuf_put_data(mb, plen);
    memcpy(p, payload, plen);
    
    /* Application layer: add app header */
    struct app_header *app = (struct app_header *)msgbuf_push(mb, sizeof(*app));
    app->msg_type = 1;
    app->msg_len = plen;
    
    /* Frame layer: add frame header */
    struct frame_header *frm = (struct frame_header *)msgbuf_push(mb, sizeof(*frm));
    frm->sync = 0xAA;
    frm->version = 1;
    frm->total_len = mb->len;
    
    printf("Total message: %zu bytes\n", mb->len);
    printf("Headroom left: %zu bytes\n", msgbuf_headroom(mb));
    
    msgbuf_put(mb);
}

int main(void) {
    example_layered_send();
    return 0;
}
```

---

## 2. Ops-Based Protocol Abstraction

```
+------------------------------------------------------------------+
|  PATTERN: PROTOCOL OPS TABLE                                     |
+------------------------------------------------------------------+

    PROBLEM SOLVED:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Multiple protocol implementations                       │
    │  • Runtime selection of protocol                           │
    │  • Framework code doesn't know specific protocols          │
    │  • Easy to add new protocols                               │
    └─────────────────────────────────────────────────────────────┘

    MINIMAL C FEATURES:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Function pointers                                       │
    │  • const ops table instances                               │
    │  • Registration list or array                              │
    └─────────────────────────────────────────────────────────────┘

    WHEN NOT TO USE:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Only one protocol implementation                        │
    │  • Protocol selected at compile time                       │
    │  • Extreme performance requirements (indirect call cost)   │
    └─────────────────────────────────────────────────────────────┘

    COMPLETE USER-SPACE EXAMPLE:
```

```c
/* protocol_ops.c - Protocol abstraction pattern */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward declarations */
struct connection;
struct protocol;

/* Protocol operations table */
struct protocol_ops {
    const char *name;
    int (*connect)(struct connection *conn, const char *addr);
    int (*send)(struct connection *conn, const void *data, size_t len);
    int (*recv)(struct connection *conn, void *buf, size_t len);
    void (*close)(struct connection *conn);
};

/* Connection object */
struct connection {
    const struct protocol_ops *ops;  /* Polymorphism via this pointer */
    void *private_data;
    int fd;
    char remote_addr[256];
};

/* Generic connection functions - dispatch via ops */
int conn_connect(struct connection *conn, const char *addr) {
    return conn->ops->connect(conn, addr);
}

int conn_send(struct connection *conn, const void *data, size_t len) {
    return conn->ops->send(conn, data, len);
}

int conn_recv(struct connection *conn, void *buf, size_t len) {
    return conn->ops->recv(conn, buf, len);
}

void conn_close(struct connection *conn) {
    conn->ops->close(conn);
}

/* ============ TCP PROTOCOL ============ */
static int tcp_connect(struct connection *conn, const char *addr) {
    printf("[TCP] Connecting to %s\n", addr);
    strncpy(conn->remote_addr, addr, sizeof(conn->remote_addr) - 1);
    conn->fd = 42;  /* Simulated FD */
    return 0;
}

static int tcp_send(struct connection *conn, const void *data, size_t len) {
    printf("[TCP] Sending %zu bytes to fd %d\n", len, conn->fd);
    return len;
}

static int tcp_recv(struct connection *conn, void *buf, size_t len) {
    printf("[TCP] Receiving up to %zu bytes from fd %d\n", len, conn->fd);
    return 0;
}

static void tcp_close(struct connection *conn) {
    printf("[TCP] Closing fd %d\n", conn->fd);
    conn->fd = -1;
}

static const struct protocol_ops tcp_ops = {
    .name    = "TCP",
    .connect = tcp_connect,
    .send    = tcp_send,
    .recv    = tcp_recv,
    .close   = tcp_close,
};

/* ============ UDP PROTOCOL ============ */
static int udp_connect(struct connection *conn, const char *addr) {
    printf("[UDP] Setting remote to %s\n", addr);
    strncpy(conn->remote_addr, addr, sizeof(conn->remote_addr) - 1);
    conn->fd = 43;
    return 0;
}

static int udp_send(struct connection *conn, const void *data, size_t len) {
    printf("[UDP] Sending datagram of %zu bytes to %s\n", len, conn->remote_addr);
    return len;
}

static int udp_recv(struct connection *conn, void *buf, size_t len) {
    printf("[UDP] Receiving datagram up to %zu bytes\n", len);
    return 0;
}

static void udp_close(struct connection *conn) {
    printf("[UDP] Closing socket\n");
    conn->fd = -1;
}

static const struct protocol_ops udp_ops = {
    .name    = "UDP",
    .connect = udp_connect,
    .send    = udp_send,
    .recv    = udp_recv,
    .close   = udp_close,
};

/* ============ PROTOCOL REGISTRY ============ */
static const struct protocol_ops *protocol_list[] = {
    &tcp_ops,
    &udp_ops,
    NULL
};

const struct protocol_ops *find_protocol(const char *name) {
    for (int i = 0; protocol_list[i]; i++) {
        if (strcmp(protocol_list[i]->name, name) == 0)
            return protocol_list[i];
    }
    return NULL;
}

struct connection *connection_create(const char *protocol) {
    const struct protocol_ops *ops = find_protocol(protocol);
    if (!ops) return NULL;
    
    struct connection *conn = calloc(1, sizeof(*conn));
    if (!conn) return NULL;
    
    conn->ops = ops;
    conn->fd = -1;
    return conn;
}

void connection_destroy(struct connection *conn) {
    if (conn->fd >= 0)
        conn_close(conn);
    free(conn);
}

/* ============ USAGE ============ */
int main(void) {
    /* Create connections with different protocols */
    struct connection *tcp = connection_create("TCP");
    struct connection *udp = connection_create("UDP");
    
    /* Same API, different implementations */
    conn_connect(tcp, "192.168.1.1:80");
    conn_connect(udp, "192.168.1.2:53");
    
    conn_send(tcp, "GET / HTTP/1.0\r\n", 16);
    conn_send(udp, "DNS query", 9);
    
    connection_destroy(tcp);
    connection_destroy(udp);
    
    return 0;
}
```

---

## 3. Layered Packet Processing

```
+------------------------------------------------------------------+
|  PATTERN: LAYERED PROCESSING CHAIN                               |
+------------------------------------------------------------------+

    PROBLEM SOLVED:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Process data through multiple layers                    │
    │  • Each layer adds/removes its headers                     │
    │  • Layers can be added/removed dynamically                 │
    │  • Clear ownership transfer between layers                 │
    └─────────────────────────────────────────────────────────────┘

    COMPLETE USER-SPACE EXAMPLE:
```

```c
/* layers.c - Layered packet processing */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct packet {
    unsigned char *data;
    size_t len;
    size_t capacity;
};

/* Layer handler interface */
typedef int (*layer_encode_fn)(struct packet *pkt);
typedef int (*layer_decode_fn)(struct packet *pkt);

struct layer {
    const char *name;
    layer_encode_fn encode;  /* TX: add headers */
    layer_decode_fn decode;  /* RX: strip headers */
};

/* Simple packet operations */
struct packet *packet_alloc(size_t capacity) {
    struct packet *p = malloc(sizeof(*p));
    if (!p) return NULL;
    p->data = malloc(capacity);
    if (!p->data) { free(p); return NULL; }
    p->len = 0;
    p->capacity = capacity;
    return p;
}

void packet_free(struct packet *p) {
    if (p) { free(p->data); free(p); }
}

int packet_prepend(struct packet *p, const void *data, size_t len) {
    if (p->len + len > p->capacity) return -1;
    memmove(p->data + len, p->data, p->len);
    memcpy(p->data, data, len);
    p->len += len;
    return 0;
}

int packet_strip(struct packet *p, size_t len) {
    if (p->len < len) return -1;
    memmove(p->data, p->data + len, p->len - len);
    p->len -= len;
    return 0;
}

/* ============ LAYER IMPLEMENTATIONS ============ */

/* Frame layer: sync + length */
struct frame_hdr { uint8_t sync; uint16_t len; };

int frame_encode(struct packet *pkt) {
    struct frame_hdr hdr = { .sync = 0xAA, .len = pkt->len };
    printf("[FRAME] Adding header (sync=0x%02X, len=%u)\n", hdr.sync, hdr.len);
    return packet_prepend(pkt, &hdr, sizeof(hdr));
}

int frame_decode(struct packet *pkt) {
    if (pkt->len < sizeof(struct frame_hdr)) return -1;
    struct frame_hdr *hdr = (struct frame_hdr *)pkt->data;
    printf("[FRAME] Stripping header (sync=0x%02X, len=%u)\n", hdr->sync, hdr->len);
    return packet_strip(pkt, sizeof(*hdr));
}

/* Transport layer: msg_id + checksum */
struct transport_hdr { uint16_t msg_id; uint16_t checksum; };

int transport_encode(struct packet *pkt) {
    struct transport_hdr hdr = { .msg_id = 1, .checksum = 0x1234 };
    printf("[TRANSPORT] Adding header (msg_id=%u)\n", hdr.msg_id);
    return packet_prepend(pkt, &hdr, sizeof(hdr));
}

int transport_decode(struct packet *pkt) {
    if (pkt->len < sizeof(struct transport_hdr)) return -1;
    struct transport_hdr *hdr = (struct transport_hdr *)pkt->data;
    printf("[TRANSPORT] Stripping header (msg_id=%u)\n", hdr->msg_id);
    return packet_strip(pkt, sizeof(*hdr));
}

/* ============ LAYER STACK ============ */
static struct layer layers[] = {
    { "transport", transport_encode, transport_decode },
    { "frame",     frame_encode,     frame_decode     },
    { NULL, NULL, NULL }
};

int stack_encode(struct packet *pkt) {
    for (int i = 0; layers[i].name; i++) {
        if (layers[i].encode(pkt) < 0) {
            printf("Encode failed at %s\n", layers[i].name);
            return -1;
        }
    }
    return 0;
}

int stack_decode(struct packet *pkt) {
    /* Decode in reverse order */
    int count = 0;
    while (layers[count].name) count++;
    
    for (int i = count - 1; i >= 0; i--) {
        if (layers[i].decode(pkt) < 0) {
            printf("Decode failed at %s\n", layers[i].name);
            return -1;
        }
    }
    return 0;
}

int main(void) {
    struct packet *pkt = packet_alloc(256);
    
    /* Add payload */
    const char *payload = "Hello";
    memcpy(pkt->data, payload, strlen(payload));
    pkt->len = strlen(payload);
    
    printf("=== ENCODE (TX) ===\n");
    stack_encode(pkt);
    printf("Final packet: %zu bytes\n\n", pkt->len);
    
    printf("=== DECODE (RX) ===\n");
    stack_decode(pkt);
    printf("Payload: %.*s\n", (int)pkt->len, pkt->data);
    
    packet_free(pkt);
    return 0;
}
```

---

## 4. Registration Systems

```
+------------------------------------------------------------------+
|  PATTERN: PLUGIN REGISTRATION                                    |
+------------------------------------------------------------------+

    COMPLETE USER-SPACE EXAMPLE:
```

```c
/* registry.c - Plugin registration pattern */

#include <stdio.h>
#include <string.h>

#define MAX_HANDLERS 32

/* Handler descriptor */
struct handler {
    const char *name;
    int (*process)(const void *data, size_t len);
};

/* Global registry */
static struct {
    struct handler *list[MAX_HANDLERS];
    int count;
} registry;

int register_handler(struct handler *h) {
    if (registry.count >= MAX_HANDLERS) return -1;
    registry.list[registry.count++] = h;
    printf("Registered handler: %s\n", h->name);
    return 0;
}

struct handler *find_handler(const char *name) {
    for (int i = 0; i < registry.count; i++) {
        if (strcmp(registry.list[i]->name, name) == 0)
            return registry.list[i];
    }
    return NULL;
}

/* Example handlers */
static int json_process(const void *data, size_t len) {
    printf("[JSON] Processing %zu bytes\n", len);
    return 0;
}

static int xml_process(const void *data, size_t len) {
    printf("[XML] Processing %zu bytes\n", len);
    return 0;
}

static struct handler json_handler = { .name = "json", .process = json_process };
static struct handler xml_handler = { .name = "xml", .process = xml_process };

/* Auto-registration via init functions */
void __attribute__((constructor)) handlers_init(void) {
    register_handler(&json_handler);
    register_handler(&xml_handler);
}

int main(void) {
    struct handler *h = find_handler("json");
    if (h) h->process("test", 4);
    return 0;
}
```

---

## 5. Explicit Ownership Transfer

```
+------------------------------------------------------------------+
|  PATTERN: OWNERSHIP TRANSFER CONVENTIONS                         |
+------------------------------------------------------------------+

    COMPLETE USER-SPACE EXAMPLE:
```

```c
/* ownership.c - Explicit ownership transfer */

#include <stdio.h>
#include <stdlib.h>

struct message {
    int refcnt;
    char data[256];
};

/* Create: caller owns returned message */
struct message *message_create(const char *text) {
    struct message *m = malloc(sizeof(*m));
    if (!m) return NULL;
    m->refcnt = 1;
    snprintf(m->data, sizeof(m->data), "%s", text);
    return m;  /* Ownership transferred to caller */
}

/* Take: ownership transferred to callee */
void queue_enqueue_take(struct message *m) {
    printf("Queue takes ownership of: %s\n", m->data);
    /* Queue now owns m, will free later */
    free(m);
}

/* Borrow: no ownership change */
void process_borrow(const struct message *m) {
    printf("Processing (borrowed): %s\n", m->data);
    /* Caller still owns m */
}

int main(void) {
    struct message *m = message_create("Hello");  /* We own m */
    
    process_borrow(m);  /* Still own m */
    
    queue_enqueue_take(m);  /* Ownership transferred! */
    /* m is now invalid - do NOT use */
    
    return 0;
}
```

---

## Summary

```
+------------------------------------------------------------------+
|  REUSABLE PATTERNS SUMMARY                                       |
+------------------------------------------------------------------+

    MESSAGE BUFFER (sk_buff-like):
    • Headroom/tailroom for layer headers
    • Push/pull for zero-copy header manipulation
    • Reference counting for sharing

    PROTOCOL OPS:
    • Function pointer table per protocol
    • Registration for runtime discovery
    • Generic dispatch without switch

    LAYERED PROCESSING:
    • encode/decode functions per layer
    • Stack executes layers in order
    • Clear direction (TX vs RX)

    REGISTRATION SYSTEM:
    • Central registry list/table
    • register/find functions
    • Optional auto-registration

    OWNERSHIP TRANSFER:
    • Naming: _take suffix = ownership transferred
    • No suffix = borrowed reference
    • Reference counting for shared ownership
```

**中文总结：**
- **消息缓冲区**：头部/尾部空间、push/pull零拷贝操作、引用计数共享
- **协议ops表**：每协议函数指针表、注册发现、无switch分发
- **分层处理**：每层encode/decode函数、按顺序执行、TX/RX方向清晰
- **注册系统**：中央注册表、register/find函数、可选自动注册
- **所有权传递**：_take后缀表示所有权转移、无后缀表示借用

