# Linux Networking Subsystem: Anti-Patterns and Pitfalls

## 1. Excessive Copying

```
+------------------------------------------------------------------+
|  ANTI-PATTERN: COPY AT EVERY LAYER                               |
+------------------------------------------------------------------+

    BAD DESIGN:
    ┌─────────────────────────────────────────────────────────────┐
    │  Layer 3:  [copy] ─────→ new_buffer_3                       │
    │  Layer 2:  [copy] ─────→ new_buffer_2                       │
    │  Layer 1:  [copy] ─────→ new_buffer_1                       │
    │                                                             │
    │  Result: N layers = N copies = O(N × data_size)             │
    └─────────────────────────────────────────────────────────────┘

    CORRECT DESIGN (sk_buff style):
    ┌─────────────────────────────────────────────────────────────┐
    │  Layer 3:  [push header] ──→ same buffer                    │
    │  Layer 2:  [push header] ──→ same buffer                    │
    │  Layer 1:  [push header] ──→ same buffer                    │
    │                                                             │
    │  Result: N layers = 0 copies = O(header_size only)          │
    └─────────────────────────────────────────────────────────────┘
```

**中文说明：**
每层复制数据是最常见的性能杀手。Linux使用headroom/tailroom设计，通过移动指针而非复制数据来添加/删除头部。

### Bad Example

```c
/* BAD: Copy at each layer */
struct layer2_packet {
    struct layer2_header hdr;
    unsigned char payload[1500];
};

struct layer1_packet {
    struct layer1_header hdr;
    struct layer2_packet inner;  /* Embedded copy! */
};

void bad_encapsulate(const unsigned char *data, size_t len) {
    /* Layer 2: copy data */
    struct layer2_packet l2;
    memcpy(l2.payload, data, len);  /* Copy 1 */
    
    /* Layer 1: copy entire L2 packet */
    struct layer1_packet l1;
    memcpy(&l1.inner, &l2, sizeof(l2));  /* Copy 2: entire L2! */
}
```

### Correct Example

```c
/* GOOD: Use headroom, no copying */
void good_encapsulate(struct mbuf *m) {
    /* Data already in buffer */
    
    /* Layer 2: prepend header (no copy) */
    struct layer2_header *l2 = mbuf_push(m, sizeof(*l2));
    l2->type = L2_TYPE_DATA;
    
    /* Layer 1: prepend header (no copy) */
    struct layer1_header *l1 = mbuf_push(m, sizeof(*l1));
    l1->sync = 0xAA;
    l1->len = m->len;
}
```

---

## 2. Unclear Buffer Ownership

```
+------------------------------------------------------------------+
|  ANTI-PATTERN: AMBIGUOUS OWNERSHIP                               |
+------------------------------------------------------------------+

    SYMPTOMS:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Function receives buffer, unclear who frees it          │
    │  • Double-free bugs                                        │
    │  • Use-after-free bugs                                     │
    │  • Memory leaks (nobody frees)                             │
    └─────────────────────────────────────────────────────────────┘

    OWNERSHIP MODELS:
    
    1. BORROW (caller retains ownership):
       int process(const struct mbuf *m);  // const = borrow
       
    2. TAKE (callee takes ownership):
       void queue_enqueue(struct mbuf *m);  // Will free or store
       
    3. GIVE (caller transfers ownership):
       struct mbuf *create_packet(void);  // Caller must free
```

**中文说明：**
所有权不清晰导致内存错误。必须用命名约定、文档或const限定符明确所有权。

### Bad Example

```c
/* BAD: Who owns the buffer? */
void process_packet(unsigned char *buf, size_t len) {
    /* Does caller free buf? Or do we? */
    do_something(buf);
    /* If we free it, caller gets use-after-free */
    /* If we don't, maybe memory leak */
}

void caller(void) {
    unsigned char *buf = malloc(1500);
    process_packet(buf, 1500);
    /* Should I free buf here? Who knows! */
    free(buf);  /* Maybe double-free? */
}
```

### Correct Example

```c
/* GOOD: Explicit ownership via naming and types */

/* Borrow: const pointer, caller owns */
int process_packet_borrow(const struct mbuf *m) {
    /* We can read m, but caller still owns it */
    return m->len;
}

/* Take: ownership transfers to callee */
void queue_enqueue_take(struct mbuf *m) {
    /* We now own m, will free when done */
    enqueue(&queue, m);
    /* Caller must NOT use m after this */
}

/* Give: callee returns owned object */
struct mbuf *create_response(void) {
    struct mbuf *m = mbuf_alloc(256);
    /* Fill m... */
    return m;  /* Caller now owns m, must free */
}

void caller(void) {
    struct mbuf *m = mbuf_alloc(256);
    
    process_packet_borrow(m);  /* m still ours */
    
    struct mbuf *resp = create_response();  /* We own resp */
    
    queue_enqueue_take(m);  /* m transferred, don't use */
    m = NULL;  /* Defensive: set to NULL */
    
    mbuf_put(resp);  /* Free our owned buffer */
}
```

---

## 3. Mixing Protocol Logic Across Layers

```
+------------------------------------------------------------------+
|  ANTI-PATTERN: LAYER VIOLATION                                   |
+------------------------------------------------------------------+

    BAD:
    ┌─────────────────────────────────────────────────────────────┐
    │  TCP layer knows about Ethernet MTU                         │
    │  IP layer parses TCP options                                │
    │  Driver layer checks application protocol                   │
    │                                                             │
    │  Result: Tight coupling, cannot swap layers                 │
    └─────────────────────────────────────────────────────────────┘

    GOOD:
    ┌─────────────────────────────────────────────────────────────┐
    │  Each layer only knows:                                     │
    │    - Its own header format                                  │
    │    - Interface to adjacent layers                           │
    │    - Nothing about layers above or below                    │
    └─────────────────────────────────────────────────────────────┘
```

**中文说明：**
层次之间应该只通过明确的接口通信，不应该"偷看"其他层的数据。

### Bad Example

```c
/* BAD: Frame layer knows about app protocol */
int frame_decode(struct mbuf *m) {
    struct frame_hdr *fhdr = (struct frame_hdr *)m->head;
    
    mbuf_pull(m, sizeof(*fhdr));
    
    /* VIOLATION: Frame layer parses app header! */
    struct app_hdr *ahdr = (struct app_hdr *)m->head;
    if (ahdr->type == APP_TYPE_PRIORITY) {
        /* Frame layer making app-level decisions */
        set_high_priority(m);
    }
    
    return 0;
}

/* BAD: App layer knows about frame details */
int app_send(const char *data, size_t len) {
    struct mbuf *m = mbuf_alloc(len);
    memcpy(m->head, data, len);
    
    /* VIOLATION: App layer calculating frame overhead */
    if (len + FRAME_HDR_SIZE > MAX_FRAME_SIZE) {
        /* App shouldn't know frame header size */
        return -E_TOO_LARGE;
    }
    
    return frame_encode(m);
}
```

### Correct Example

```c
/* GOOD: Each layer only handles its own concerns */

int frame_decode(struct mbuf *m) {
    struct frame_hdr *hdr = (struct frame_hdr *)m->head;
    
    /* Validate frame-level concerns only */
    if (hdr->sync != FRAME_SYNC) return -1;
    if (calc_crc(m) != hdr->crc) return -1;
    
    /* Strip our header, pass up */
    mbuf_pull(m, sizeof(*hdr));
    return 0;  /* Next layer handles the rest */
}

int app_send(const char *data, size_t len) {
    /* App only knows app-level limits */
    if (len > APP_MAX_PAYLOAD) return -E_TOO_LARGE;
    
    struct mbuf *m = mbuf_alloc(len);
    memcpy(m->head, data, len);
    
    /* Let each layer handle its own encapsulation */
    return stack_transmit(&stack, m);
}
```

---

## 4. Oversized Ops Tables

```
+------------------------------------------------------------------+
|  ANTI-PATTERN: BLOATED OPS TABLE                                 |
+------------------------------------------------------------------+

    BAD:
    ┌─────────────────────────────────────────────────────────────┐
    │  struct proto_ops {                                         │
    │      int (*connect)(...);                                   │
    │      int (*send)(...);                                      │
    │      int (*recv)(...);                                      │
    │      int (*setsockopt)(...);                                │
    │      int (*getsockopt)(...);                                │
    │      int (*ioctl)(...);                                     │
    │      ... 30 more functions ...                              │
    │  };                                                         │
    │                                                             │
    │  Most implementations: NULL, NULL, NULL, stub, stub...      │
    └─────────────────────────────────────────────────────────────┘

    PROBLEMS:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Implementers must provide stubs for unused functions     │
    │  • Hard to understand what's really needed                  │
    │  • Interface becomes unstable (temptation to add more)      │
    │  • Dispatch overhead for rarely-used operations             │
    └─────────────────────────────────────────────────────────────┘
```

**中文说明：**
ops表应该只包含核心操作，避免膨胀。可选功能应该用能力标志或单独的扩展ops表。

### Bad Example

```c
/* BAD: Kitchen-sink ops table */
struct protocol_ops {
    int (*connect)(struct conn *c, const char *addr);
    int (*bind)(struct conn *c, const char *addr);
    int (*listen)(struct conn *c, int backlog);
    int (*accept)(struct conn *c);
    int (*send)(struct conn *c, const void *buf, size_t len);
    int (*recv)(struct conn *c, void *buf, size_t len);
    int (*sendmsg)(struct conn *c, struct msg *m);
    int (*recvmsg)(struct conn *c, struct msg *m);
    int (*sendfile)(struct conn *c, int fd, size_t len);
    int (*splice)(struct conn *c, int fd);
    int (*poll)(struct conn *c);
    int (*ioctl)(struct conn *c, int cmd, void *arg);
    int (*setsockopt)(struct conn *c, int opt, void *val);
    int (*getsockopt)(struct conn *c, int opt, void *val);
    int (*shutdown)(struct conn *c, int how);
    int (*close)(struct conn *c);
    /* More... every protocol must implement all of these */
};

/* UDP implementation: half the functions are stubs */
static int udp_listen(struct conn *c, int backlog) {
    return -EOPNOTSUPP;  /* UDP doesn't listen */
}
static int udp_accept(struct conn *c) {
    return -EOPNOTSUPP;  /* UDP doesn't accept */
}
/* 10 more stubs... */
```

### Correct Example

```c
/* GOOD: Minimal core ops + capability flags */
struct protocol_ops {
    const char *name;
    unsigned int capabilities;  /* What this protocol supports */
    
    /* Core operations only */
    int (*connect)(struct conn *c, const char *addr);
    int (*send)(struct conn *c, const void *buf, size_t len);
    int (*recv)(struct conn *c, void *buf, size_t len);
    void (*close)(struct conn *c);
};

/* Capability flags */
#define PROTO_CAP_LISTEN   (1 << 0)
#define PROTO_CAP_CONNECT  (1 << 1)
#define PROTO_CAP_STREAM   (1 << 2)
#define PROTO_CAP_DGRAM    (1 << 3)

/* Optional extended ops (only for protocols that need it) */
struct stream_protocol_ops {
    const struct protocol_ops *base;
    int (*listen)(struct conn *c, int backlog);
    int (*accept)(struct conn *c);
};

/* Clean implementations */
static const struct protocol_ops udp_ops = {
    .name = "udp",
    .capabilities = PROTO_CAP_CONNECT | PROTO_CAP_DGRAM,
    .connect = udp_connect,
    .send = udp_send,
    .recv = udp_recv,
    .close = udp_close,
    /* No stubs needed! */
};

/* Check capability before calling extended ops */
int do_listen(struct conn *c, int backlog) {
    if (!(c->ops->capabilities & PROTO_CAP_LISTEN))
        return -EOPNOTSUPP;
    
    struct stream_protocol_ops *sops = get_stream_ops(c);
    return sops->listen(c, backlog);
}
```

---

## 5. No Backpressure Handling

```
+------------------------------------------------------------------+
|  ANTI-PATTERN: UNBOUNDED BUFFERING                               |
+------------------------------------------------------------------+

    BAD:
    ┌─────────────────────────────────────────────────────────────┐
    │  Producer ──────→ [infinite queue] ──────→ Consumer         │
    │                                                             │
    │  Result: OOM when producer faster than consumer             │
    └─────────────────────────────────────────────────────────────┘

    GOOD:
    ┌─────────────────────────────────────────────────────────────┐
    │  Producer ──────→ [bounded queue] ──────→ Consumer          │
    │       ↑              │                                      │
    │       └──────────────┘ (backpressure when full)             │
    └─────────────────────────────────────────────────────────────┘
```

**中文说明：**
无界缓冲区是内存耗尽的常见原因。必须有背压机制通知生产者减速。

### Bad Example

```c
/* BAD: Unbounded queue */
struct queue {
    struct node *head;
    struct node *tail;
    /* No limit! */
};

int enqueue(struct queue *q, struct mbuf *m) {
    struct node *n = malloc(sizeof(*n));
    n->mbuf = m;
    n->next = NULL;
    
    /* Always succeeds - no limit check */
    if (q->tail) q->tail->next = n;
    else q->head = n;
    q->tail = n;
    
    return 0;  /* Never fails, keeps growing */
}

void producer(void) {
    while (1) {
        struct mbuf *m = mbuf_alloc(1500);
        enqueue(&queue, m);  /* Eventually OOM */
    }
}
```

### Correct Example

```c
/* GOOD: Bounded queue with backpressure */
struct queue {
    struct node *head;
    struct node *tail;
    size_t count;
    size_t max_count;
};

int queue_init(struct queue *q, size_t max) {
    q->head = q->tail = NULL;
    q->count = 0;
    q->max_count = max;
    return 0;
}

int enqueue(struct queue *q, struct mbuf *m) {
    /* Backpressure: reject when full */
    if (q->count >= q->max_count) {
        return -EAGAIN;  /* Tell producer to slow down */
    }
    
    struct node *n = malloc(sizeof(*n));
    if (!n) return -ENOMEM;
    
    n->mbuf = m;
    n->next = NULL;
    
    if (q->tail) q->tail->next = n;
    else q->head = n;
    q->tail = n;
    q->count++;
    
    return 0;
}

void producer(void) {
    while (1) {
        struct mbuf *m = mbuf_alloc(1500);
        
        int ret = enqueue(&queue, m);
        if (ret == -EAGAIN) {
            /* Queue full - apply backpressure */
            mbuf_put(m);  /* Drop or */
            usleep(1000); /* Wait and retry */
        }
    }
}
```

---

## 6. Ignoring Partial Operations

```
+------------------------------------------------------------------+
|  ANTI-PATTERN: ASSUMING COMPLETE I/O                             |
+------------------------------------------------------------------+

    BAD:
    ┌─────────────────────────────────────────────────────────────┐
    │  send(fd, buf, 1000) ──→ returns 500 ──→ half data lost!  │
    └─────────────────────────────────────────────────────────────┘

    GOOD:
    ┌─────────────────────────────────────────────────────────────┐
    │  Loop until all data sent or error                         │
    └─────────────────────────────────────────────────────────────┘
```

**中文说明：**
网络I/O可能只完成部分操作，必须循环直到全部完成。

### Bad Example

```c
/* BAD: Assumes complete send */
int send_message(int fd, const char *msg, size_t len) {
    ssize_t ret = send(fd, msg, len, 0);
    if (ret < 0) return -1;
    return 0;  /* What if ret < len? Data lost! */
}
```

### Correct Example

```c
/* GOOD: Loop until complete */
int send_all(int fd, const char *buf, size_t len) {
    size_t sent = 0;
    
    while (sent < len) {
        ssize_t ret = send(fd, buf + sent, len - sent, 0);
        if (ret < 0) {
            if (errno == EINTR) continue;  /* Retry on interrupt */
            return -1;  /* Real error */
        }
        if (ret == 0) return -1;  /* Connection closed */
        sent += ret;
    }
    return 0;
}
```

---

## 7. Wrong Locking Granularity

```
+------------------------------------------------------------------+
|  ANTI-PATTERN: LOCK GRANULARITY MISMATCH                         |
+------------------------------------------------------------------+

    TOO COARSE (one lock for everything):
    ┌─────────────────────────────────────────────────────────────┐
    │  All operations serialize on one lock                      │
    │  Result: Poor scalability                                  │
    └─────────────────────────────────────────────────────────────┘

    TOO FINE (lock per field):
    ┌─────────────────────────────────────────────────────────────┐
    │  Many locks, complex ordering                              │
    │  Result: Deadlocks, overhead                               │
    └─────────────────────────────────────────────────────────────┘

    RIGHT: Lock per logical subsystem or object
```

**中文说明：**
锁粒度要恰当：太粗影响并发性能，太细导致复杂死锁。

### Bad Examples

```c
/* BAD: Global lock for everything */
static pthread_mutex_t global_lock = PTHREAD_MUTEX_INITIALIZER;

int send_on_socket(struct socket *s, struct mbuf *m) {
    pthread_mutex_lock(&global_lock);  /* Everything blocks */
    /* ... */
    pthread_mutex_unlock(&global_lock);
}

int recv_on_socket(struct socket *s, struct mbuf *m) {
    pthread_mutex_lock(&global_lock);  /* Still blocks send! */
    /* ... */
    pthread_mutex_unlock(&global_lock);
}

/* BAD: Lock per field */
struct connection {
    pthread_mutex_t state_lock;
    pthread_mutex_t queue_lock;
    pthread_mutex_t stats_lock;
    /* Must lock in correct order or deadlock! */
};
```

### Correct Example

```c
/* GOOD: Lock per logical object */
struct connection {
    pthread_mutex_t lock;  /* Protects this connection's state */
    int state;
    struct queue send_q;
    struct queue recv_q;
    struct stats stats;
};

int send_on_connection(struct connection *c, struct mbuf *m) {
    pthread_mutex_lock(&c->lock);  /* Only this connection */
    enqueue(&c->send_q, m);
    pthread_mutex_unlock(&c->lock);
    return 0;
}

/* Different connections can operate in parallel */
```

---

## Summary: Common Pitfalls Checklist

```
+------------------------------------------------------------------+
|  NETWORKING CODE REVIEW CHECKLIST                                |
+------------------------------------------------------------------+

    MEMORY & COPYING:
    [ ] Are headers added via pointer manipulation, not copying?
    [ ] Is headroom reserved for encapsulation?
    [ ] Are large payloads zero-copied where possible?

    OWNERSHIP:
    [ ] Is buffer ownership clearly documented?
    [ ] Are ownership transfers explicit (_take suffix)?
    [ ] Is reference counting used for shared buffers?
    [ ] Are double-free and use-after-free impossible?

    LAYERING:
    [ ] Does each layer only access its own headers?
    [ ] Are layer interfaces clean and minimal?
    [ ] Can layers be replaced without changing others?

    OPS TABLES:
    [ ] Are ops tables minimal (only essential operations)?
    [ ] Are optional operations in separate tables or flagged?
    [ ] Do implementations avoid stub functions?

    FLOW CONTROL:
    [ ] Are queues bounded?
    [ ] Is backpressure properly propagated?
    [ ] Are partial I/O operations handled correctly?

    CONCURRENCY:
    [ ] Is locking at the right granularity?
    [ ] Is lock ordering documented and enforced?
    [ ] Are critical sections as short as possible?
```

---

**中文总结：**

| 反模式 | 问题 | 解决方案 |
|--------|------|----------|
| 过度复制 | 性能差 | 使用headroom/tailroom |
| 所有权不清 | 内存错误 | 明确命名约定 |
| 层次混乱 | 耦合紧密 | 严格层次边界 |
| ops表膨胀 | 维护困难 | 最小核心+扩展 |
| 无背压 | 内存耗尽 | 有界队列 |
| 忽略部分I/O | 数据丢失 | 循环直到完成 |
| 锁粒度错误 | 性能或死锁 | 每对象一锁 |

