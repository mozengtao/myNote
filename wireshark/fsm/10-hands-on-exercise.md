# Section 10: Hands-On Exercise

## 10.1 Exercise: Design a Stateful Protocol Parser FSM

We'll design a parser for a simple hypothetical protocol called "MSGP" (Message Protocol):

```
+------------------------------------------------------------------+
|                    MSGP PROTOCOL SPECIFICATION                    |
+------------------------------------------------------------------+
|                                                                   |
|  Message Format:                                                  |
|  ===============                                                  |
|  +--------+--------+--------+--------+                            |
|  | Magic  | Type   | Length | Payload |                           |
|  | 2 bytes| 1 byte | 2 bytes| N bytes |                           |
|  +--------+--------+--------+--------+                            |
|                                                                   |
|  Magic: 0x4D53 ("MS")                                             |
|                                                                   |
|  Types:                                                           |
|  - 0x01: HELLO (initiate connection)                              |
|  - 0x02: HELLO_ACK (acknowledge connection)                       |
|  - 0x03: DATA (application data)                                  |
|  - 0x04: DATA_ACK (acknowledge data)                              |
|  - 0x05: BYE (close connection)                                   |
|  - 0x06: BYE_ACK (acknowledge close)                              |
|                                                                   |
|  Protocol Sequence:                                               |
|  Client -> Server: HELLO                                          |
|  Server -> Client: HELLO_ACK                                      |
|  Client <-> Server: DATA / DATA_ACK (multiple)                    |
|  Either -> Other: BYE                                             |
|  Other -> Either: BYE_ACK                                         |
|                                                                   |
+------------------------------------------------------------------+
```

### Protocol State Diagram

```
                          +-------------+
                          |    IDLE     |
                          +------+------+
                                 |
                           HELLO |
                                 v
                          +-------------+
                          |   HELLO_    |
                          |   SENT      |
                          +------+------+
                                 |
                         HELLO_ACK
                                 v
                          +-------------+
              +---------->| ESTABLISHED |<-----------+
              |           +------+------+            |
              |                  |                   |
         DATA_ACK           DATA |              DATA |
              |                  v                   |
              |           +-------------+            |
              +-----------+   DATA_     |------------+
                          |   PENDING   |
                          +------+------+
                                 |
                              BYE
                                 v
                          +-------------+
                          |   CLOSING   |
                          +------+------+
                                 |
                            BYE_ACK
                                 v
                          +-------------+
                          |   CLOSED    |
                          +-------------+
```

---

## 10.2 Implementation: Wireshark-Style Parser

### Header File (msgp_parser.h)

```c
#ifndef MSGP_PARSER_H
#define MSGP_PARSER_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* Protocol constants */
#define MSGP_MAGIC          0x4D53
#define MSGP_HEADER_LEN     5
#define MSGP_MAX_PAYLOAD    65535

/* Message types */
typedef enum {
    MSGP_TYPE_HELLO     = 0x01,
    MSGP_TYPE_HELLO_ACK = 0x02,
    MSGP_TYPE_DATA      = 0x03,
    MSGP_TYPE_DATA_ACK  = 0x04,
    MSGP_TYPE_BYE       = 0x05,
    MSGP_TYPE_BYE_ACK   = 0x06
} msgp_type_t;

/* Observation flags (Wireshark-style implicit FSM) */
#define MSGP_SEEN_HELLO         (1 << 0)
#define MSGP_SEEN_HELLO_ACK     (1 << 1)
#define MSGP_SEEN_DATA          (1 << 2)
#define MSGP_SEEN_DATA_ACK      (1 << 3)
#define MSGP_SEEN_BYE           (1 << 4)
#define MSGP_SEEN_BYE_ACK       (1 << 5)
#define MSGP_SEEN_MALFORMED     (1 << 6)
#define MSGP_SEEN_OUT_OF_ORDER  (1 << 7)

/* Error flags */
#define MSGP_ERR_BAD_MAGIC      (1 << 0)
#define MSGP_ERR_BAD_LENGTH     (1 << 1)
#define MSGP_ERR_BAD_TYPE       (1 << 2)
#define MSGP_ERR_TRUNCATED      (1 << 3)
#define MSGP_ERR_BAD_SEQUENCE   (1 << 4)

/* Per-direction flow state */
typedef struct {
    uint32_t packets_sent;
    uint32_t bytes_sent;
    uint32_t data_messages;
    uint32_t last_seq;      /* For tracking sequence */
} msgp_flow_t;

/* Per-connection state (Wireshark conversation equivalent) */
typedef struct {
    /* Implicit state via flags */
    uint32_t observed;      /* What we've seen (MSGP_SEEN_*) */
    uint32_t errors;        /* Errors encountered (MSGP_ERR_*) */
    
    /* Bidirectional flow tracking */
    msgp_flow_t client_flow;
    msgp_flow_t server_flow;
    
    /* Reassembly buffer for fragmented messages */
    uint8_t *reassembly_buf;
    size_t   reassembly_len;
    size_t   reassembly_expected;
    
    /* Statistics */
    uint32_t first_frame;
    uint32_t last_frame;
    
} msgp_session_t;

/* Parsed message structure */
typedef struct {
    uint16_t magic;
    uint8_t  type;
    uint16_t length;
    const uint8_t *payload;
    
    /* Parsing metadata */
    bool valid;
    uint32_t error_flags;
    
} msgp_message_t;

/* Parser context (packet-level) */
typedef struct {
    msgp_session_t *session;
    bool is_client;
    uint32_t frame_num;
    
} msgp_context_t;

/* API Functions */
msgp_session_t *msgp_session_create(void);
void msgp_session_destroy(msgp_session_t *session);

int msgp_parse_message(const uint8_t *data, size_t len, msgp_message_t *msg);
int msgp_dissect(msgp_context_t *ctx, const uint8_t *data, size_t len);

const char *msgp_type_name(uint8_t type);
const char *msgp_state_name(msgp_session_t *session);

#endif /* MSGP_PARSER_H */
```

### Implementation (msgp_parser.c)

```c
#include "msgp_parser.h"
#include <stdio.h>

/*
 * Session Management
 */
msgp_session_t *msgp_session_create(void) {
    msgp_session_t *session = calloc(1, sizeof(msgp_session_t));
    if (!session) return NULL;
    
    /* Initialize to unknown state */
    session->observed = 0;
    session->errors = 0;
    
    return session;
}

void msgp_session_destroy(msgp_session_t *session) {
    if (session) {
        free(session->reassembly_buf);
        free(session);
    }
}

/*
 * Message Type Names
 */
const char *msgp_type_name(uint8_t type) {
    static const char *names[] = {
        [MSGP_TYPE_HELLO]     = "HELLO",
        [MSGP_TYPE_HELLO_ACK] = "HELLO_ACK",
        [MSGP_TYPE_DATA]      = "DATA",
        [MSGP_TYPE_DATA_ACK]  = "DATA_ACK",
        [MSGP_TYPE_BYE]       = "BYE",
        [MSGP_TYPE_BYE_ACK]   = "BYE_ACK"
    };
    
    if (type > 0 && type <= MSGP_TYPE_BYE_ACK) {
        return names[type];
    }
    return "UNKNOWN";
}

/*
 * Derive State Name from Flags (Implicit FSM)
 */
const char *msgp_state_name(msgp_session_t *session) {
    uint32_t obs = session->observed;
    
    /* Check error conditions first */
    if (session->errors) {
        return "ERROR";
    }
    
    /* Derive state from observations */
    if (obs & MSGP_SEEN_BYE_ACK) {
        return "CLOSED";
    }
    if (obs & MSGP_SEEN_BYE) {
        return "CLOSING";
    }
    if ((obs & (MSGP_SEEN_HELLO | MSGP_SEEN_HELLO_ACK)) == 
        (MSGP_SEEN_HELLO | MSGP_SEEN_HELLO_ACK)) {
        if (obs & MSGP_SEEN_DATA) {
            return "DATA_TRANSFER";
        }
        return "ESTABLISHED";
    }
    if (obs & MSGP_SEEN_HELLO) {
        return "HELLO_SENT";
    }
    
    return "IDLE";
}

/*
 * Parse Message Header
 * Returns: bytes consumed, or -1 on error
 */
int msgp_parse_message(const uint8_t *data, size_t len, msgp_message_t *msg) {
    /* Initialize output */
    memset(msg, 0, sizeof(*msg));
    msg->valid = false;
    
    /* Check minimum length */
    if (len < MSGP_HEADER_LEN) {
        msg->error_flags |= MSGP_ERR_TRUNCATED;
        return -1;
    }
    
    /* Parse header */
    msg->magic = (data[0] << 8) | data[1];
    msg->type = data[2];
    msg->length = (data[3] << 8) | data[4];
    
    /* Validate magic */
    if (msg->magic != MSGP_MAGIC) {
        msg->error_flags |= MSGP_ERR_BAD_MAGIC;
        /* Continue parsing - Wireshark philosophy */
    }
    
    /* Validate type */
    if (msg->type < MSGP_TYPE_HELLO || msg->type > MSGP_TYPE_BYE_ACK) {
        msg->error_flags |= MSGP_ERR_BAD_TYPE;
    }
    
    /* Validate length */
    if (msg->length > MSGP_MAX_PAYLOAD) {
        msg->error_flags |= MSGP_ERR_BAD_LENGTH;
    }
    
    /* Check if full payload available */
    if (len < MSGP_HEADER_LEN + msg->length) {
        msg->error_flags |= MSGP_ERR_TRUNCATED;
        msg->payload = data + MSGP_HEADER_LEN;
        msg->length = len - MSGP_HEADER_LEN;  /* Clamp to available */
        msg->valid = (msg->error_flags == 0);
        return len;  /* Consumed all available */
    }
    
    /* Complete message */
    msg->payload = data + MSGP_HEADER_LEN;
    msg->valid = (msg->error_flags == 0);
    
    return MSGP_HEADER_LEN + msg->length;
}

/*
 * Check Message Sequence Validity
 */
static void check_sequence(msgp_context_t *ctx, msgp_message_t *msg) {
    msgp_session_t *s = ctx->session;
    uint32_t obs = s->observed;
    
    switch (msg->type) {
        case MSGP_TYPE_HELLO:
            /* HELLO should be first */
            if (obs & (MSGP_SEEN_HELLO_ACK | MSGP_SEEN_DATA)) {
                s->observed |= MSGP_SEEN_OUT_OF_ORDER;
            }
            break;
            
        case MSGP_TYPE_HELLO_ACK:
            /* HELLO_ACK requires HELLO */
            if (!(obs & MSGP_SEEN_HELLO)) {
                s->observed |= MSGP_SEEN_OUT_OF_ORDER;
            }
            break;
            
        case MSGP_TYPE_DATA:
        case MSGP_TYPE_DATA_ACK:
            /* DATA requires established connection */
            if (!((obs & MSGP_SEEN_HELLO) && (obs & MSGP_SEEN_HELLO_ACK))) {
                s->observed |= MSGP_SEEN_OUT_OF_ORDER;
            }
            break;
            
        case MSGP_TYPE_BYE:
            /* BYE can come anytime after HELLO */
            break;
            
        case MSGP_TYPE_BYE_ACK:
            /* BYE_ACK requires BYE */
            if (!(obs & MSGP_SEEN_BYE)) {
                s->observed |= MSGP_SEEN_OUT_OF_ORDER;
            }
            break;
    }
}

/*
 * Update Session State
 */
static void update_session_state(msgp_context_t *ctx, msgp_message_t *msg) {
    msgp_session_t *s = ctx->session;
    msgp_flow_t *flow = ctx->is_client ? &s->client_flow : &s->server_flow;
    
    /* Update observation flags */
    switch (msg->type) {
        case MSGP_TYPE_HELLO:     s->observed |= MSGP_SEEN_HELLO; break;
        case MSGP_TYPE_HELLO_ACK: s->observed |= MSGP_SEEN_HELLO_ACK; break;
        case MSGP_TYPE_DATA:      s->observed |= MSGP_SEEN_DATA; break;
        case MSGP_TYPE_DATA_ACK:  s->observed |= MSGP_SEEN_DATA_ACK; break;
        case MSGP_TYPE_BYE:       s->observed |= MSGP_SEEN_BYE; break;
        case MSGP_TYPE_BYE_ACK:   s->observed |= MSGP_SEEN_BYE_ACK; break;
    }
    
    /* Update error flags */
    if (msg->error_flags) {
        s->observed |= MSGP_SEEN_MALFORMED;
        s->errors |= msg->error_flags;
    }
    
    /* Update flow statistics */
    flow->packets_sent++;
    flow->bytes_sent += MSGP_HEADER_LEN + msg->length;
    if (msg->type == MSGP_TYPE_DATA) {
        flow->data_messages++;
    }
    
    /* Track frame numbers */
    if (s->first_frame == 0) {
        s->first_frame = ctx->frame_num;
    }
    s->last_frame = ctx->frame_num;
}

/*
 * Main Dissection Entry Point
 */
int msgp_dissect(msgp_context_t *ctx, const uint8_t *data, size_t len) {
    msgp_message_t msg;
    int consumed;
    
    /* Handle reassembly if in progress */
    if (ctx->session->reassembly_buf) {
        /* TODO: Add reassembly logic */
    }
    
    /* Parse message */
    consumed = msgp_parse_message(data, len, &msg);
    
    if (consumed < 0) {
        /* Parse failed - still update state with what we know */
        printf("Frame %u: Parse error (truncated)\n", ctx->frame_num);
        return -1;
    }
    
    /* Check protocol sequence */
    check_sequence(ctx, &msg);
    
    /* Update session state */
    update_session_state(ctx, &msg);
    
    /* Output (would be tree building in Wireshark) */
    printf("Frame %u: %s [%s] magic=%04x len=%u",
           ctx->frame_num,
           msgp_type_name(msg.type),
           msgp_state_name(ctx->session),
           msg.magic,
           msg.length);
    
    if (msg.error_flags) {
        printf(" ERRORS:");
        if (msg.error_flags & MSGP_ERR_BAD_MAGIC) printf(" BAD_MAGIC");
        if (msg.error_flags & MSGP_ERR_BAD_TYPE) printf(" BAD_TYPE");
        if (msg.error_flags & MSGP_ERR_TRUNCATED) printf(" TRUNCATED");
    }
    
    if (ctx->session->observed & MSGP_SEEN_OUT_OF_ORDER) {
        printf(" [OUT_OF_ORDER]");
    }
    
    printf("\n");
    
    return consumed;
}
```

---

## 10.3 Test Program

```c
#include "msgp_parser.h"
#include <stdio.h>

/* Test data: simulated capture */
typedef struct {
    uint32_t frame_num;
    bool is_client;
    size_t len;
    uint8_t data[32];
} test_packet_t;

static test_packet_t test_capture[] = {
    /* Normal connection */
    {1, true,  7, {0x4D, 0x53, 0x01, 0x00, 0x02, 'H', 'i'}},      /* HELLO */
    {2, false, 5, {0x4D, 0x53, 0x02, 0x00, 0x00}},                 /* HELLO_ACK */
    {3, true,  9, {0x4D, 0x53, 0x03, 0x00, 0x04, 'T','e','s','t'}},/* DATA */
    {4, false, 5, {0x4D, 0x53, 0x04, 0x00, 0x00}},                 /* DATA_ACK */
    {5, true,  5, {0x4D, 0x53, 0x05, 0x00, 0x00}},                 /* BYE */
    {6, false, 5, {0x4D, 0x53, 0x06, 0x00, 0x00}},                 /* BYE_ACK */
    
    /* End marker */
    {0, false, 0, {0}}
};

static test_packet_t malformed_capture[] = {
    /* Malformed: wrong magic */
    {1, true,  7, {0xFF, 0xFF, 0x01, 0x00, 0x02, 'H', 'i'}},
    /* Malformed: unknown type */
    {2, true,  5, {0x4D, 0x53, 0xFF, 0x00, 0x00}},
    /* Out of order: DATA before HELLO_ACK */
    {3, true,  7, {0x4D, 0x53, 0x01, 0x00, 0x02, 'H', 'i'}},      /* HELLO */
    {4, true,  9, {0x4D, 0x53, 0x03, 0x00, 0x04, 'T','e','s','t'}},/* DATA - out of order! */
    
    {0, false, 0, {0}}
};

int main(void) {
    msgp_session_t *session;
    msgp_context_t ctx;
    
    /* Test 1: Normal connection */
    printf("=== Test 1: Normal Connection ===\n");
    session = msgp_session_create();
    ctx.session = session;
    
    for (int i = 0; test_capture[i].frame_num != 0; i++) {
        ctx.frame_num = test_capture[i].frame_num;
        ctx.is_client = test_capture[i].is_client;
        msgp_dissect(&ctx, test_capture[i].data, test_capture[i].len);
    }
    
    printf("Final state: %s\n", msgp_state_name(session));
    printf("Client packets: %u, bytes: %u\n", 
           session->client_flow.packets_sent, 
           session->client_flow.bytes_sent);
    printf("Server packets: %u, bytes: %u\n",
           session->server_flow.packets_sent,
           session->server_flow.bytes_sent);
    msgp_session_destroy(session);
    
    /* Test 2: Malformed packets */
    printf("\n=== Test 2: Malformed Packets ===\n");
    session = msgp_session_create();
    ctx.session = session;
    
    for (int i = 0; malformed_capture[i].frame_num != 0; i++) {
        ctx.frame_num = malformed_capture[i].frame_num;
        ctx.is_client = malformed_capture[i].is_client;
        msgp_dissect(&ctx, malformed_capture[i].data, malformed_capture[i].len);
    }
    
    printf("Final state: %s\n", msgp_state_name(session));
    printf("Errors: 0x%08x\n", session->errors);
    msgp_session_destroy(session);
    
    return 0;
}
```

---

## 10.4 Key Patterns Demonstrated

```
+------------------------------------------------------------------+
|         WIRESHARK PATTERNS USED IN THIS EXERCISE                  |
+------------------------------------------------------------------+

1. IMPLICIT FSM VIA FLAGS
   - observed field tracks what messages we've seen
   - State derived from flag combinations
   - No explicit state enum for connection phase

2. BEST-EFFORT PARSING
   - Continue parsing even with errors
   - Clamp length to available data
   - Report errors but don't abort

3. BIDIRECTIONAL TRACKING
   - Separate flow state for client and server
   - Context includes direction indicator
   - Statistics tracked per-direction

4. ERROR FLAGGING NOT REJECTION
   - msg.error_flags captures problems
   - session->errors accumulates issues
   - Processing continues after errors

5. SEQUENCE VALIDATION
   - check_sequence() validates message order
   - Out-of-order flagged but not rejected
   - Useful for debugging protocol issues

+------------------------------------------------------------------+
```

---

## 中文解释

### 实践练习

**1. 练习目标**
设计一个简单的有状态协议解析器（MSGP），使用Wireshark的设计模式。

**2. 协议规范**
- MSGP有6种消息类型：HELLO、HELLO_ACK、DATA、DATA_ACK、BYE、BYE_ACK
- 连接序列：HELLO → HELLO_ACK → DATA交换 → BYE → BYE_ACK
- 消息格式：Magic(2字节) + Type(1字节) + Length(2字节) + Payload

**3. 实现的Wireshark模式**
- 隐式FSM：使用标志位追踪观察到的事件
- 尽力解析：即使有错误也继续解析
- 双向追踪：分别跟踪客户端和服务器流
- 标记而非拒绝：记录错误但不中止处理
- 序列验证：检查消息顺序但不拒绝乱序

**4. 代码结构**
- `msgp_session_t`：每连接状态（对应Wireshark的conversation）
- `msgp_message_t`：解析后的消息结构
- `msgp_context_t`：每包上下文
- `msgp_dissect()`：主解析函数
- `msgp_state_name()`：从标志推导状态名称

**5. 测试场景**
- 正常连接：验证完整握手和数据传输
- 畸形数据包：验证错误处理和优雅降级
