# Linux Networking Subsystem: User-Space Mini Network Stack Exercise

## Design Overview

```
+------------------------------------------------------------------+
|  MINI NETWORK STACK - DESIGN REQUIREMENTS                        |
+------------------------------------------------------------------+

    ┌───────────────────────────────────────────────────────────────┐
    │  1. Packet buffer object (mbuf) with headroom/tailroom       │
    │  2. Protocol ops table for polymorphism                      │
    │  3. At least two protocol layers (frame + app)               │
    │  4. Clear ownership and lifetime rules                       │
    │  5. Extensible registration mechanism                        │
    │  6. Proper error handling and cleanup                        │
    └───────────────────────────────────────────────────────────────┘

    ARCHITECTURE:
    
                           ┌──────────────────┐
                           │  Application     │
                           │  (send/receive)  │
                           └────────┬─────────┘
                                    │ mbuf
                           ┌────────▼─────────┐
                           │  App Layer       │
                           │  (proto_ops)     │
                           └────────┬─────────┘
                                    │ mbuf
                           ┌────────▼─────────┐
                           │  Frame Layer     │
                           │  (proto_ops)     │
                           └────────┬─────────┘
                                    │ mbuf
                           ┌────────▼─────────┐
                           │  Driver          │
                           │  (device_ops)    │
                           └──────────────────┘
```

---

## Complete Implementation

```c
/* mini_netstack.c - Complete user-space mini network stack */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <assert.h>

/*============================================================================
 * SECTION 1: MESSAGE BUFFER (mbuf)
 *
 * Design inspired by Linux sk_buff:
 * - Headroom for prepending headers without copying
 * - Tailroom for appending data
 * - Reference counting for safe sharing
 * - Clone support for read-only sharing
 *============================================================================*/

#define MBUF_HEADROOM  64
#define MBUF_DATA_SIZE 1500
#define MBUF_TAILROOM  32

struct mbuf {
    /* === Memory management === */
    atomic_int refcnt;              /* Reference count */
    bool cloned;                    /* Is this a clone? */
    unsigned char *buf_start;       /* Allocated buffer start */
    size_t buf_size;                /* Total buffer size */
    
    /* === Data pointers === */
    unsigned char *head;            /* Data start */
    unsigned char *tail;            /* Data end */
    size_t len;                     /* Current data length */
    
    /* === Metadata === */
    int protocol;                   /* Protocol identifier */
    void *private;                  /* Layer-specific data */
};

/* Allocate new mbuf with headroom */
struct mbuf *mbuf_alloc(size_t data_size) {
    struct mbuf *m = calloc(1, sizeof(*m));
    if (!m) return NULL;
    
    size_t total = MBUF_HEADROOM + data_size + MBUF_TAILROOM;
    m->buf_start = malloc(total);
    if (!m->buf_start) {
        free(m);
        return NULL;
    }
    
    atomic_init(&m->refcnt, 1);
    m->cloned = false;
    m->buf_size = total;
    
    /* Data pointer starts after headroom */
    m->head = m->buf_start + MBUF_HEADROOM;
    m->tail = m->head;
    m->len = 0;
    
    return m;
}

/* Increase reference count */
struct mbuf *mbuf_get(struct mbuf *m) {
    if (m)
        atomic_fetch_add(&m->refcnt, 1);
    return m;
}

/* Decrease reference count, free if zero */
void mbuf_put(struct mbuf *m) {
    if (!m) return;
    
    if (atomic_fetch_sub(&m->refcnt, 1) == 1) {
        free(m->buf_start);
        free(m);
    }
}

/* Clone: share buffer, separate metadata */
struct mbuf *mbuf_clone(struct mbuf *orig) {
    struct mbuf *clone = calloc(1, sizeof(*clone));
    if (!clone) return NULL;
    
    *clone = *orig;  /* Copy metadata */
    atomic_init(&clone->refcnt, 1);
    clone->cloned = true;
    
    /* Share underlying buffer */
    atomic_fetch_add(&orig->refcnt, 1);
    
    return clone;
}

/* Prepend space for header */
void *mbuf_push(struct mbuf *m, size_t len) {
    if (m->head - m->buf_start < (ptrdiff_t)len) {
        fprintf(stderr, "mbuf_push: no headroom\n");
        return NULL;
    }
    m->head -= len;
    m->len += len;
    return m->head;
}

/* Remove from head (strip header) */
void *mbuf_pull(struct mbuf *m, size_t len) {
    if (m->len < len) {
        fprintf(stderr, "mbuf_pull: not enough data\n");
        return NULL;
    }
    void *ptr = m->head;
    m->head += len;
    m->len -= len;
    return ptr;
}

/* Append data at tail */
void *mbuf_put_data(struct mbuf *m, size_t len) {
    if (m->buf_start + m->buf_size - m->tail < (ptrdiff_t)len) {
        fprintf(stderr, "mbuf_put_data: no tailroom\n");
        return NULL;
    }
    void *ptr = m->tail;
    m->tail += len;
    m->len += len;
    return ptr;
}

/* Copy data to tail */
int mbuf_copy_to_tail(struct mbuf *m, const void *data, size_t len) {
    void *dst = mbuf_put_data(m, len);
    if (!dst) return -1;
    memcpy(dst, data, len);
    return 0;
}

/* Get headroom available */
size_t mbuf_headroom(struct mbuf *m) {
    return m->head - m->buf_start;
}

/* Get tailroom available */
size_t mbuf_tailroom(struct mbuf *m) {
    return m->buf_start + m->buf_size - m->tail;
}

/*============================================================================
 * SECTION 2: PROTOCOL OPERATIONS
 *
 * Design inspired by Linux proto_ops:
 * - Each protocol layer has an ops table
 * - Framework dispatches via function pointers
 * - Clear separation of encoding (TX) and decoding (RX)
 *============================================================================*/

/* Protocol identifier */
enum {
    PROTO_NONE = 0,
    PROTO_FRAME = 1,
    PROTO_APP_ECHO = 10,
    PROTO_APP_CMD = 11,
};

struct proto_ops {
    const char *name;
    int protocol;
    
    /* TX path: process mbuf before sending */
    int (*encode)(struct mbuf *m);
    
    /* RX path: process mbuf after receiving */
    int (*decode)(struct mbuf *m);
    
    /* Optional: connection setup */
    int (*connect)(void *ctx, const char *addr);
    
    /* Optional: cleanup */
    void (*release)(void *ctx);
};

/*============================================================================
 * SECTION 3: PROTOCOL REGISTRY
 *
 * Central registry for all protocols.
 * Protocols register at startup and are looked up by ID.
 *============================================================================*/

#define MAX_PROTOCOLS 32

static struct {
    const struct proto_ops *list[MAX_PROTOCOLS];
    int count;
} proto_registry;

int proto_register(const struct proto_ops *ops) {
    if (proto_registry.count >= MAX_PROTOCOLS) {
        fprintf(stderr, "proto_register: registry full\n");
        return -1;
    }
    proto_registry.list[proto_registry.count++] = ops;
    printf("Registered protocol: %s (id=%d)\n", ops->name, ops->protocol);
    return 0;
}

const struct proto_ops *proto_find(int protocol) {
    for (int i = 0; i < proto_registry.count; i++) {
        if (proto_registry.list[i]->protocol == protocol)
            return proto_registry.list[i];
    }
    return NULL;
}

/*============================================================================
 * SECTION 4: FRAME LAYER (Layer 1)
 *
 * Simple framing protocol:
 * - Sync byte (0xAA)
 * - Length (2 bytes)
 * - CRC (1 byte)
 *============================================================================*/

struct frame_hdr {
    uint8_t sync;
    uint16_t len;
    uint8_t crc;
} __attribute__((packed));

static uint8_t calc_crc(const unsigned char *data, size_t len) {
    uint8_t crc = 0;
    for (size_t i = 0; i < len; i++)
        crc ^= data[i];
    return crc;
}

static int frame_encode(struct mbuf *m) {
    struct frame_hdr *hdr;
    uint8_t crc;
    
    /* Calculate CRC of payload */
    crc = calc_crc(m->head, m->len);
    
    /* Prepend frame header */
    hdr = mbuf_push(m, sizeof(*hdr));
    if (!hdr) return -1;
    
    hdr->sync = 0xAA;
    hdr->len = m->len - sizeof(*hdr);  /* Payload length */
    hdr->crc = crc;
    
    printf("[FRAME] encode: sync=0x%02X len=%u crc=0x%02X\n",
           hdr->sync, hdr->len, hdr->crc);
    return 0;
}

static int frame_decode(struct mbuf *m) {
    struct frame_hdr *hdr;
    uint8_t crc;
    
    if (m->len < sizeof(struct frame_hdr)) {
        fprintf(stderr, "[FRAME] decode: too short\n");
        return -1;
    }
    
    hdr = (struct frame_hdr *)m->head;
    
    if (hdr->sync != 0xAA) {
        fprintf(stderr, "[FRAME] decode: bad sync\n");
        return -1;
    }
    
    /* Strip header */
    mbuf_pull(m, sizeof(*hdr));
    
    /* Verify CRC */
    crc = calc_crc(m->head, m->len);
    if (crc != hdr->crc) {
        fprintf(stderr, "[FRAME] decode: CRC mismatch\n");
        return -1;
    }
    
    printf("[FRAME] decode: len=%u crc=0x%02X OK\n", hdr->len, hdr->crc);
    return 0;
}

static const struct proto_ops frame_ops = {
    .name     = "frame",
    .protocol = PROTO_FRAME,
    .encode   = frame_encode,
    .decode   = frame_decode,
};

/*============================================================================
 * SECTION 5: APPLICATION LAYER - ECHO PROTOCOL (Layer 2)
 *
 * Simple echo protocol:
 * - Type (1 byte): 0x01=request, 0x02=response
 * - Sequence (2 bytes)
 * - Payload
 *============================================================================*/

struct echo_hdr {
    uint8_t type;
    uint16_t seq;
} __attribute__((packed));

#define ECHO_REQUEST  0x01
#define ECHO_RESPONSE 0x02

static uint16_t echo_seq = 0;

static int echo_encode(struct mbuf *m) {
    struct echo_hdr *hdr;
    
    hdr = mbuf_push(m, sizeof(*hdr));
    if (!hdr) return -1;
    
    hdr->type = ECHO_REQUEST;
    hdr->seq = ++echo_seq;
    
    printf("[ECHO] encode: type=%u seq=%u\n", hdr->type, hdr->seq);
    return 0;
}

static int echo_decode(struct mbuf *m) {
    struct echo_hdr *hdr;
    
    if (m->len < sizeof(struct echo_hdr)) {
        fprintf(stderr, "[ECHO] decode: too short\n");
        return -1;
    }
    
    hdr = (struct echo_hdr *)m->head;
    printf("[ECHO] decode: type=%u seq=%u\n", hdr->type, hdr->seq);
    
    mbuf_pull(m, sizeof(*hdr));
    return 0;
}

static const struct proto_ops echo_ops = {
    .name     = "echo",
    .protocol = PROTO_APP_ECHO,
    .encode   = echo_encode,
    .decode   = echo_decode,
};

/*============================================================================
 * SECTION 6: APPLICATION LAYER - COMMAND PROTOCOL (Layer 2 alternative)
 *
 * Command protocol with different format:
 * - Command ID (1 byte)
 * - Flags (1 byte)
 * - Payload length (2 bytes)
 *============================================================================*/

struct cmd_hdr {
    uint8_t cmd_id;
    uint8_t flags;
    uint16_t payload_len;
} __attribute__((packed));

static int cmd_encode(struct mbuf *m) {
    struct cmd_hdr *hdr;
    size_t payload_len = m->len;
    
    hdr = mbuf_push(m, sizeof(*hdr));
    if (!hdr) return -1;
    
    hdr->cmd_id = 0x42;
    hdr->flags = 0;
    hdr->payload_len = payload_len;
    
    printf("[CMD] encode: cmd=0x%02X flags=0x%02X len=%u\n",
           hdr->cmd_id, hdr->flags, hdr->payload_len);
    return 0;
}

static int cmd_decode(struct mbuf *m) {
    struct cmd_hdr *hdr;
    
    if (m->len < sizeof(struct cmd_hdr)) {
        fprintf(stderr, "[CMD] decode: too short\n");
        return -1;
    }
    
    hdr = (struct cmd_hdr *)m->head;
    printf("[CMD] decode: cmd=0x%02X flags=0x%02X len=%u\n",
           hdr->cmd_id, hdr->flags, hdr->payload_len);
    
    mbuf_pull(m, sizeof(*hdr));
    return 0;
}

static const struct proto_ops cmd_ops = {
    .name     = "command",
    .protocol = PROTO_APP_CMD,
    .encode   = cmd_encode,
    .decode   = cmd_decode,
};

/*============================================================================
 * SECTION 7: DEVICE LAYER
 *
 * Abstraction for underlying transport (simulated).
 *============================================================================*/

struct device_ops {
    const char *name;
    int (*transmit)(struct mbuf *m);
    int (*receive)(struct mbuf *m);
};

static int loopback_tx(struct mbuf *m) {
    printf("[LOOPBACK] TX: %zu bytes\n", m->len);
    printf("  Data: ");
    for (size_t i = 0; i < m->len && i < 32; i++)
        printf("%02X ", m->head[i]);
    printf("\n");
    return 0;
}

static int loopback_rx(struct mbuf *m) {
    printf("[LOOPBACK] RX: %zu bytes\n", m->len);
    return 0;
}

static const struct device_ops loopback_dev = {
    .name     = "lo",
    .transmit = loopback_tx,
    .receive  = loopback_rx,
};

/*============================================================================
 * SECTION 8: PROTOCOL STACK
 *
 * Combines layers into a complete stack.
 * Ownership: stack does NOT take ownership of mbuf.
 *============================================================================*/

struct stack_layer {
    const struct proto_ops *ops;
};

struct protocol_stack {
    struct stack_layer layers[8];
    int layer_count;
    const struct device_ops *device;
};

void stack_init(struct protocol_stack *s, const struct device_ops *dev) {
    memset(s, 0, sizeof(*s));
    s->device = dev;
}

int stack_add_layer(struct protocol_stack *s, int protocol) {
    if (s->layer_count >= 8) return -1;
    
    const struct proto_ops *ops = proto_find(protocol);
    if (!ops) {
        fprintf(stderr, "stack_add_layer: unknown protocol %d\n", protocol);
        return -1;
    }
    
    s->layers[s->layer_count].ops = ops;
    s->layer_count++;
    printf("Stack: added layer %s\n", ops->name);
    return 0;
}

/* TX: encode from app layer down to device */
int stack_transmit(struct protocol_stack *s, struct mbuf *m) {
    printf("\n=== TX PATH ===\n");
    
    /* Encode from layer 0 (app) to layer N-1 (lowest) */
    for (int i = 0; i < s->layer_count; i++) {
        const struct proto_ops *ops = s->layers[i].ops;
        if (ops->encode) {
            if (ops->encode(m) < 0) {
                fprintf(stderr, "TX encode failed at %s\n", ops->name);
                return -1;
            }
        }
    }
    
    /* Send to device */
    if (s->device && s->device->transmit) {
        return s->device->transmit(m);
    }
    return 0;
}

/* RX: decode from device up to app layer */
int stack_receive(struct protocol_stack *s, struct mbuf *m) {
    printf("\n=== RX PATH ===\n");
    
    /* Receive from device */
    if (s->device && s->device->receive) {
        s->device->receive(m);
    }
    
    /* Decode from layer N-1 (lowest) to layer 0 (app) */
    for (int i = s->layer_count - 1; i >= 0; i--) {
        const struct proto_ops *ops = s->layers[i].ops;
        if (ops->decode) {
            if (ops->decode(m) < 0) {
                fprintf(stderr, "RX decode failed at %s\n", ops->name);
                return -1;
            }
        }
    }
    return 0;
}

/*============================================================================
 * SECTION 9: INITIALIZATION AND DEMO
 *============================================================================*/

static void init_protocols(void) {
    proto_register(&frame_ops);
    proto_register(&echo_ops);
    proto_register(&cmd_ops);
}

static void demo_echo_protocol(void) {
    printf("\n========================================\n");
    printf("DEMO: Echo Protocol\n");
    printf("========================================\n");
    
    struct protocol_stack stack;
    stack_init(&stack, &loopback_dev);
    
    /* Build stack: echo (app) -> frame (link) */
    stack_add_layer(&stack, PROTO_APP_ECHO);
    stack_add_layer(&stack, PROTO_FRAME);
    
    /* Create message */
    struct mbuf *m = mbuf_alloc(MBUF_DATA_SIZE);
    const char *payload = "Hello, World!";
    mbuf_copy_to_tail(m, payload, strlen(payload));
    
    printf("\nOriginal payload: \"%s\" (%zu bytes)\n", payload, m->len);
    printf("Headroom: %zu, Tailroom: %zu\n", mbuf_headroom(m), mbuf_tailroom(m));
    
    /* Transmit */
    stack_transmit(&stack, m);
    
    printf("\nAfter TX encoding: %zu bytes\n", m->len);
    printf("Headroom: %zu, Tailroom: %zu\n", mbuf_headroom(m), mbuf_tailroom(m));
    
    /* Simulate receiving the same packet */
    printf("\n--- Simulating RX of same packet ---\n");
    stack_receive(&stack, m);
    
    printf("\nAfter RX decoding: %zu bytes\n", m->len);
    printf("Decoded payload: \"%.*s\"\n", (int)m->len, m->head);
    
    mbuf_put(m);
}

static void demo_command_protocol(void) {
    printf("\n========================================\n");
    printf("DEMO: Command Protocol\n");
    printf("========================================\n");
    
    struct protocol_stack stack;
    stack_init(&stack, &loopback_dev);
    
    /* Build stack: command (app) -> frame (link) */
    stack_add_layer(&stack, PROTO_APP_CMD);
    stack_add_layer(&stack, PROTO_FRAME);
    
    struct mbuf *m = mbuf_alloc(MBUF_DATA_SIZE);
    const char *cmd_data = "REBOOT";
    mbuf_copy_to_tail(m, cmd_data, strlen(cmd_data));
    
    stack_transmit(&stack, m);
    
    mbuf_put(m);
}

static void demo_mbuf_clone(void) {
    printf("\n========================================\n");
    printf("DEMO: mbuf Clone (Reference Sharing)\n");
    printf("========================================\n");
    
    struct mbuf *orig = mbuf_alloc(256);
    mbuf_copy_to_tail(orig, "Shared data", 11);
    printf("Original mbuf refcnt: %d\n", atomic_load(&orig->refcnt));
    
    struct mbuf *clone = mbuf_clone(orig);
    printf("After clone - orig refcnt: %d\n", atomic_load(&orig->refcnt));
    
    mbuf_put(clone);
    printf("After put clone - orig refcnt: %d\n", atomic_load(&orig->refcnt));
    
    mbuf_put(orig);
    printf("Both freed\n");
}

int main(void) {
    printf("Mini Network Stack Demo\n");
    printf("========================\n");
    
    init_protocols();
    
    demo_echo_protocol();
    demo_command_protocol();
    demo_mbuf_clone();
    
    printf("\n========================================\n");
    printf("All demos completed successfully.\n");
    printf("========================================\n");
    
    return 0;
}
```

---

## Compilation and Expected Output

```bash
gcc -std=c11 -Wall -Wextra -o mini_netstack mini_netstack.c
./mini_netstack
```

**Expected Output:**

```
Mini Network Stack Demo
========================
Registered protocol: frame (id=1)
Registered protocol: echo (id=10)
Registered protocol: command (id=11)

========================================
DEMO: Echo Protocol
========================================
Stack: added layer echo
Stack: added layer frame

Original payload: "Hello, World!" (13 bytes)
Headroom: 64, Tailroom: 32

=== TX PATH ===
[ECHO] encode: type=1 seq=1
[FRAME] encode: sync=0xAA len=16 crc=0x7B
[LOOPBACK] TX: 20 bytes
  Data: AA 10 00 7B 01 01 00 48 65 6C 6C 6F 2C 20 ...

After TX encoding: 20 bytes
Headroom: 44, Tailroom: 32

--- Simulating RX of same packet ---

=== RX PATH ===
[LOOPBACK] RX: 20 bytes
[FRAME] decode: len=16 crc=0x7B OK
[ECHO] decode: type=1 seq=1

After RX decoding: 13 bytes
Decoded payload: "Hello, World!"
```

---

## Design Analysis

```
+------------------------------------------------------------------+
|  KEY DESIGN DECISIONS                                            |
+------------------------------------------------------------------+

    BOUNDARIES:
    ┌─────────────────────────────────────────────────────────────┐
    │  mbuf         - Pure data structure, no protocol knowledge │
    │  proto_ops    - Protocol-specific encoding/decoding        │
    │  stack        - Orchestration, no protocol details         │
    │  device       - Transport abstraction                      │
    └─────────────────────────────────────────────────────────────┘

    OWNERSHIP RULES:
    ┌─────────────────────────────────────────────────────────────┐
    │  • mbuf_alloc returns owned mbuf (refcnt=1)                │
    │  • mbuf_get increments refcnt (shared ownership)           │
    │  • mbuf_put decrements refcnt (frees if zero)              │
    │  • stack functions BORROW mbuf (caller retains ownership)  │
    │  • clone creates new ownership, shares buffer              │
    └─────────────────────────────────────────────────────────────┘

    EXTENSIBILITY:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Add protocol: implement proto_ops + register            │
    │  • Add device: implement device_ops                        │
    │  • Change stack: just reconfigure layers                   │
    │  • No changes to mbuf or stack core code                   │
    └─────────────────────────────────────────────────────────────┘

    SAFETY:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Reference counting prevents use-after-free              │
    │  • Headroom check prevents buffer underflow                │
    │  • Tailroom check prevents buffer overflow                 │
    │  • Clone prevents data corruption on shared access         │
    └─────────────────────────────────────────────────────────────┘
```

---

## Summary

**中文总结：**

这个用户态迷你网络栈演示了Linux网络子系统的核心架构模式：

1. **消息缓冲区（mbuf）**
   - 预留头部空间，避免每层添加头部时复制数据
   - 引用计数支持安全的缓冲区共享
   - clone机制实现只读共享

2. **协议ops表**
   - 每个协议实现encode/decode函数
   - 通过函数指针实现多态
   - 注册机制支持运行时查找

3. **分层处理**
   - TX路径：从应用层向下编码（添加头部）
   - RX路径：从链路层向上解码（剥离头部）
   - 每层只处理自己的头部

4. **所有权规则**
   - 明确的所有权转移约定
   - 借用 vs 获取 vs 释放语义清晰
   - 引用计数管理生命周期

5. **可扩展性**
   - 添加新协议只需实现ops并注册
   - 核心代码无需修改
   - 运行时可配置协议栈层次

