# Section 10: Hands-On Design Task

## 10.1 Design Challenge: QUIC-Inspired User-Space Protocol FSM

Design a state machine for a simplified secure transport protocol that incorporates the lessons learned from ngtcp2.

### Requirements

1. **Connection Lifecycle**
   - Handshake with key exchange
   - Data transfer phase
   - Graceful and abrupt shutdown

2. **Async Integration**
   - Non-blocking API
   - Timer management
   - Event-driven design

3. **Error Handling**
   - Rich error reporting
   - Terminal states
   - Cleanup path

4. **Extensibility**
   - Flag-based extensions
   - Sub-FSM support

---

## 10.2 FSM Design (ASCII Diagram)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    SECURE TRANSPORT PROTOCOL FSM                             │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  LAYER 1: PRIMARY STATES (Mutual Exclusive)                                  │
│  ═══════════════════════════════════════════                                 │
│                                                                              │
│      ┌───────────────┐                                                       │
│      │    IDLE       │  Initial state, no connection                        │
│      └───────┬───────┘                                                       │
│              │ connect() / accept()                                          │
│              ▼                                                               │
│      ┌───────────────┐                                                       │
│      │  HANDSHAKE    │  Key exchange in progress                            │
│      │               │                                                       │
│      │  Client:      │  ┌─────────────────────────────────────┐             │
│      │  send_init()  │  │ Sub-states (via flags):             │             │
│      │  recv_resp()  │  │  - INIT_SENT                        │             │
│      │  send_fin()   │  │  - RESP_RECEIVED                    │             │
│      │               │  │  - KEYS_DERIVED                     │             │
│      │  Server:      │  └─────────────────────────────────────┘             │
│      │  recv_init()  │                                                       │
│      │  send_resp()  │                                                       │
│      │  recv_fin()   │                                                       │
│      └───────┬───────┘                                                       │
│              │ handshake complete                                            │
│              ▼                                                               │
│      ┌───────────────┐                                                       │
│      │  ESTABLISHED  │  Normal data transfer                                │
│      │               │                                                       │
│      │  - send()     │  ┌─────────────────────────────────────┐             │
│      │  - recv()     │  │ Active Sub-FSMs:                    │             │
│      │  - timers     │  │  - Path Validation (optional)       │             │
│      │               │  │  - Keep-Alive (optional)            │             │
│      │               │  │  - Key Rotation (optional)          │             │
│      │               │  └─────────────────────────────────────┘             │
│      └───────┬───────┘                                                       │
│              │                                                               │
│      ┌───────┴───────┐                                                       │
│      │               │                                                       │
│      ▼               ▼                                                       │
│  ┌─────────┐   ┌───────────┐                                                │
│  │ CLOSING │   │ DRAINING  │                                                │
│  │         │   │           │                                                │
│  │ Sent    │   │ Received  │                                                │
│  │ CLOSE   │   │ CLOSE     │                                                │
│  │         │   │           │                                                │
│  │ May     │   │ No TX     │                                                │
│  │ resend  │   │ allowed   │                                                │
│  └────┬────┘   └─────┬─────┘                                                │
│       │              │                                                       │
│       │  3×RTO       │  3×RTO                                               │
│       ▼              ▼                                                       │
│      ┌───────────────┐                                                       │
│      │    CLOSED     │  Resources freed                                     │
│      └───────────────┘                                                       │
│                                                                              │
│                                                                              │
│  LAYER 2: FLAGS (Orthogonal Booleans)                                        │
│  ═════════════════════════════════════                                       │
│                                                                              │
│  ┌──────────────────────────────────────────────────────────────────────┐   │
│  │ Bit │ Name                      │ Purpose                            │   │
│  ├─────┼───────────────────────────┼────────────────────────────────────┤   │
│  │  0  │ FLAG_INIT_SENT            │ Initial message sent (client)      │   │
│  │  1  │ FLAG_INIT_RECEIVED        │ Initial message received (server)  │   │
│  │  2  │ FLAG_KEYS_DERIVED         │ Encryption keys are ready          │   │
│  │  3  │ FLAG_HANDSHAKE_CONFIRMED  │ Handshake fully acknowledged       │   │
│  │  4  │ FLAG_KEEP_ALIVE_PENDING   │ Keep-alive timer active            │   │
│  │  5  │ FLAG_KEY_UPDATE_PENDING   │ Key rotation in progress           │   │
│  │  6  │ FLAG_PATH_VALIDATED       │ Current path is validated          │   │
│  │  7  │ FLAG_CLOSE_SENT           │ Close message sent                 │   │
│  └─────┴───────────────────────────┴────────────────────────────────────┘   │
│                                                                              │
│                                                                              │
│  LAYER 3: SUB-FSMs (Optional, Independent)                                   │
│  ═════════════════════════════════════════                                   │
│                                                                              │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐             │
│  │ Path Validation │  │   Keep-Alive    │  │   Key Rotation  │             │
│  │                 │  │                 │  │                 │             │
│  │ ┌─────────────┐ │  │ ┌─────────────┐ │  │ ┌─────────────┐ │             │
│  │ │ IDLE        │ │  │ │ IDLE        │ │  │ │ CURRENT     │ │             │
│  │ │ CHALLENGING │ │  │ │ PENDING     │ │  │ │ UPDATING    │ │             │
│  │ │ VALIDATED   │ │  │ │ SENT        │ │  │ │ CONFIRMED   │ │             │
│  │ │ FAILED      │ │  │ │ CONFIRMED   │ │  │ │             │ │             │
│  │ └─────────────┘ │  │ └─────────────┘ │  │ └─────────────┘ │             │
│  └─────────────────┘  └─────────────────┘  └─────────────────┘             │
│                                                                              │
│                                                                              │
│  TIMERS (Aggregated Expiry)                                                  │
│  ══════════════════════════                                                  │
│                                                                              │
│  get_expiry() = min(                                                         │
│    retransmit_timer,    // RTO for unacked data                             │
│    idle_timer,          // Idle timeout                                      │
│    keep_alive_timer,    // Keep-alive if enabled                            │
│    handshake_timer,     // Handshake timeout                                │
│    path_validation_timer,// If PV active                                    │
│    key_rotation_timer   // If key update pending                            │
│  )                                                                           │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 中文解释 (Chinese Explanation)

### FSM 设计图说明

上面的 ASCII 图展示了一个受 QUIC/ngtcp2 启发的安全传输协议状态机设计：

**第一层：主状态（互斥）**
- **IDLE**: 初始状态，无连接
- **HANDSHAKE**: 密钥交换进行中
  - 客户端: 发送初始消息、接收响应、完成握手
  - 服务端: 接收初始消息、发送响应、确认握手
- **ESTABLISHED**: 正常数据传输
  - 可发送/接收数据
  - 管理定时器
  - 可激活子状态机
- **CLOSING**: 已发送关闭消息，等待确认
- **DRAINING**: 已收到关闭消息，被动等待
- **CLOSED**: 资源已释放

**第二层：标志位（正交布尔值）**
- FLAG_INIT_SENT: 初始消息已发送
- FLAG_KEYS_DERIVED: 加密密钥已就绪
- FLAG_HANDSHAKE_CONFIRMED: 握手完全确认
- FLAG_KEEP_ALIVE_PENDING: 保活定时器活跃
- 等等...

**第三层：子状态机（可选、独立）**
- 路径验证: IDLE → CHALLENGING → VALIDATED/FAILED
- 保活: IDLE → PENDING → SENT → CONFIRMED
- 密钥轮换: CURRENT → UPDATING → CONFIRMED

**定时器（聚合到期）**
- 重传定时器
- 空闲定时器
- 保活定时器
- 握手超时
- 路径验证定时器
- 密钥轮换定时器

---

## 10.3 Implementation Structure

```c
/* ============================================================
 * SECURE TRANSPORT PROTOCOL - HEADER
 * Inspired by ngtcp2 design patterns
 * ============================================================ */

#ifndef SECURE_TRANSPORT_H
#define SECURE_TRANSPORT_H

#include <stdint.h>
#include <stddef.h>

/* ============================================================
 * TYPE DEFINITIONS
 * ============================================================ */

typedef uint64_t stp_tstamp;    /* Timestamp (nanoseconds) */
typedef int64_t stp_ssize;      /* Signed size for returns */
typedef uint64_t stp_duration;  /* Duration (nanoseconds) */

#define STP_TSTAMP_MAX UINT64_MAX

/* ============================================================
 * PRIMARY STATES (Layer 1)
 * ============================================================ */

typedef enum stp_state {
  STP_STATE_IDLE,
  STP_STATE_HANDSHAKE,
  STP_STATE_ESTABLISHED,
  STP_STATE_CLOSING,
  STP_STATE_DRAINING,
  STP_STATE_CLOSED
} stp_state;

/* ============================================================
 * FLAGS (Layer 2)
 * ============================================================ */

#define STP_FLAG_NONE                  0x00u
#define STP_FLAG_INIT_SENT             0x01u
#define STP_FLAG_INIT_RECEIVED         0x02u
#define STP_FLAG_KEYS_DERIVED          0x04u
#define STP_FLAG_HANDSHAKE_CONFIRMED   0x08u
#define STP_FLAG_KEEP_ALIVE_PENDING    0x10u
#define STP_FLAG_KEY_UPDATE_PENDING    0x20u
#define STP_FLAG_PATH_VALIDATED        0x40u
#define STP_FLAG_CLOSE_SENT            0x80u

/* ============================================================
 * ERROR CODES
 * ============================================================ */

#define STP_OK                         0
#define STP_ERR_WOULD_BLOCK           -1
#define STP_ERR_INVALID_STATE         -2
#define STP_ERR_CLOSING               -3
#define STP_ERR_DRAINING              -4
#define STP_ERR_IDLE_CLOSE            -5
#define STP_ERR_HANDSHAKE_TIMEOUT     -6
#define STP_ERR_PROTOCOL              -7
#define STP_ERR_CALLBACK_FAILURE      -8
#define STP_ERR_NOMEM                 -9

/* ============================================================
 * SUB-FSM: PATH VALIDATION (Layer 3)
 * ============================================================ */

typedef enum stp_pv_state {
  STP_PV_IDLE,
  STP_PV_CHALLENGING,
  STP_PV_VALIDATED,
  STP_PV_FAILED
} stp_pv_state;

typedef struct stp_path_validation {
  stp_pv_state state;
  stp_tstamp started_ts;
  stp_tstamp expiry;
  uint8_t challenge[8];
} stp_path_validation;

/* ============================================================
 * SUB-FSM: KEY ROTATION (Layer 3)
 * ============================================================ */

typedef enum stp_kr_state {
  STP_KR_CURRENT,
  STP_KR_UPDATING,
  STP_KR_CONFIRMED
} stp_kr_state;

typedef struct stp_key_rotation {
  stp_kr_state state;
  stp_tstamp started_ts;
  uint32_t generation;
} stp_key_rotation;

/* ============================================================
 * CALLBACKS
 * ============================================================ */

typedef struct stp_conn stp_conn;

typedef struct stp_callbacks {
  int (*on_handshake_completed)(stp_conn *conn, void *user_data);
  int (*on_recv_data)(stp_conn *conn, const uint8_t *data, 
                      size_t datalen, void *user_data);
  int (*on_connection_close)(stp_conn *conn, uint64_t error_code,
                             const uint8_t *reason, size_t reasonlen,
                             void *user_data);
  /* Add more as needed */
} stp_callbacks;

/* ============================================================
 * CONNECTION STRUCTURE
 * ============================================================ */

typedef struct stp_conn {
  /* Layer 1: Primary state */
  stp_state state;
  
  /* Layer 2: Flags */
  uint32_t flags;
  
  /* Layer 3: Sub-FSMs (nullable) */
  stp_path_validation *pv;
  stp_key_rotation *kr;
  
  /* Timers */
  stp_tstamp retransmit_expiry;
  stp_tstamp idle_expiry;
  stp_tstamp keep_alive_expiry;
  stp_tstamp handshake_expiry;
  
  /* Configuration */
  stp_duration idle_timeout;
  stp_duration handshake_timeout;
  stp_duration keep_alive_interval;
  
  /* Callbacks */
  stp_callbacks callbacks;
  void *user_data;
  
  /* Error info */
  struct {
    int code;
    uint8_t reason[256];
    size_t reason_len;
  } error;
  
  /* Role */
  int is_server;
  
} stp_conn;

/* ============================================================
 * API FUNCTIONS
 * ============================================================ */

/* Create/destroy */
stp_conn *stp_conn_new(int is_server, const stp_callbacks *cb, 
                       void *user_data);
void stp_conn_del(stp_conn *conn);

/* Packet processing (non-blocking, timestamp injected) */
stp_ssize stp_conn_read_pkt(stp_conn *conn, const uint8_t *pkt, 
                            size_t pktlen, stp_tstamp ts);
stp_ssize stp_conn_write_pkt(stp_conn *conn, uint8_t *buf, 
                             size_t buflen, stp_tstamp ts);

/* Timer handling */
stp_tstamp stp_conn_get_expiry(stp_conn *conn);
int stp_conn_handle_expiry(stp_conn *conn, stp_tstamp ts);

/* Data sending */
stp_ssize stp_conn_send(stp_conn *conn, const uint8_t *data, 
                        size_t datalen, stp_tstamp ts);

/* Connection close */
stp_ssize stp_conn_close(stp_conn *conn, uint64_t error_code,
                         const uint8_t *reason, size_t reasonlen,
                         uint8_t *buf, size_t buflen, stp_tstamp ts);

/* State inspection */
stp_state stp_conn_get_state(stp_conn *conn);
int stp_conn_is_closing(stp_conn *conn);
int stp_conn_is_draining(stp_conn *conn);

#endif /* SECURE_TRANSPORT_H */
```

---

## 10.4 Key Implementation Patterns

### Pattern 1: State-Dependent Routing

```c
stp_ssize stp_conn_read_pkt(stp_conn *conn, const uint8_t *pkt, 
                            size_t pktlen, stp_tstamp ts) {
  /* Guard: Terminal states */
  switch (conn->state) {
  case STP_STATE_CLOSING:
    return STP_ERR_CLOSING;
  case STP_STATE_DRAINING:
    return STP_ERR_DRAINING;
  case STP_STATE_CLOSED:
    return STP_ERR_INVALID_STATE;
  default:
    break;
  }
  
  /* State-dependent processing */
  switch (conn->state) {
  case STP_STATE_IDLE:
    return handle_idle_recv(conn, pkt, pktlen, ts);
  case STP_STATE_HANDSHAKE:
    return handle_handshake_recv(conn, pkt, pktlen, ts);
  case STP_STATE_ESTABLISHED:
    return handle_established_recv(conn, pkt, pktlen, ts);
  default:
    return STP_ERR_INVALID_STATE;
  }
}
```

### Pattern 2: Aggregated Timer Expiry

```c
stp_tstamp stp_conn_get_expiry(stp_conn *conn) {
  stp_tstamp expiry = STP_TSTAMP_MAX;
  
  /* Check all timer sources */
  expiry = min_tstamp(expiry, conn->retransmit_expiry);
  expiry = min_tstamp(expiry, conn->idle_expiry);
  
  if (conn->state == STP_STATE_HANDSHAKE) {
    expiry = min_tstamp(expiry, conn->handshake_expiry);
  }
  
  if (conn->flags & STP_FLAG_KEEP_ALIVE_PENDING) {
    expiry = min_tstamp(expiry, conn->keep_alive_expiry);
  }
  
  /* Sub-FSM timers */
  if (conn->pv && conn->pv->state == STP_PV_CHALLENGING) {
    expiry = min_tstamp(expiry, conn->pv->expiry);
  }
  
  return expiry;
}
```

### Pattern 3: Guarded Transitions

```c
static int transition_to_established(stp_conn *conn, stp_tstamp ts) {
  /* Guard conditions */
  if (conn->state != STP_STATE_HANDSHAKE) {
    return STP_ERR_INVALID_STATE;
  }
  if (!(conn->flags & STP_FLAG_KEYS_DERIVED)) {
    return STP_ERR_INVALID_STATE;
  }
  
  /* Transition */
  conn->state = STP_STATE_ESTABLISHED;
  
  /* Entry actions */
  conn->flags |= STP_FLAG_HANDSHAKE_CONFIRMED;
  reset_idle_timer(conn, ts);
  
  /* Callback notification */
  if (conn->callbacks.on_handshake_completed) {
    int rv = conn->callbacks.on_handshake_completed(conn, conn->user_data);
    if (rv != 0) {
      return STP_ERR_CALLBACK_FAILURE;
    }
  }
  
  return STP_OK;
}
```

### Pattern 4: Error State Handling

```c
stp_ssize stp_conn_close(stp_conn *conn, uint64_t error_code,
                         const uint8_t *reason, size_t reasonlen,
                         uint8_t *buf, size_t buflen, stp_tstamp ts) {
  stp_ssize nwrite;
  
  /* Already in terminal state? */
  if (conn->state == STP_STATE_CLOSING ||
      conn->state == STP_STATE_DRAINING ||
      conn->state == STP_STATE_CLOSED) {
    return 0;  /* No action needed */
  }
  
  /* Build close packet */
  nwrite = build_close_packet(buf, buflen, error_code, reason, reasonlen);
  if (nwrite < 0) {
    return nwrite;
  }
  
  /* Transition to CLOSING */
  conn->state = STP_STATE_CLOSING;
  conn->flags |= STP_FLAG_CLOSE_SENT;
  
  /* Store error for inspection */
  conn->error.code = (int)error_code;
  if (reason && reasonlen > 0) {
    size_t copy_len = reasonlen < sizeof(conn->error.reason) 
                    ? reasonlen : sizeof(conn->error.reason) - 1;
    memcpy(conn->error.reason, reason, copy_len);
    conn->error.reason_len = copy_len;
  }
  
  return nwrite;
}
```

---

## 10.5 Integration Example

```c
/* Example: Event loop integration */

#include <sys/epoll.h>
#include <time.h>

uint64_t get_timestamp(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

void run_event_loop(stp_conn *conn, int sockfd) {
  int epfd = epoll_create1(0);
  struct epoll_event ev, events[10];
  uint8_t buf[65536];
  
  ev.events = EPOLLIN;
  ev.data.fd = sockfd;
  epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &ev);
  
  while (!stp_conn_is_closing(conn) && !stp_conn_is_draining(conn)) {
    /* Calculate timeout from FSM */
    uint64_t now = get_timestamp();
    uint64_t expiry = stp_conn_get_expiry(conn);
    int timeout_ms = -1;
    
    if (expiry != STP_TSTAMP_MAX) {
      if (expiry <= now) {
        timeout_ms = 0;
      } else {
        timeout_ms = (expiry - now) / 1000000;  /* ns to ms */
      }
    }
    
    /* Wait for events */
    int nfds = epoll_wait(epfd, events, 10, timeout_ms);
    now = get_timestamp();
    
    /* Handle timer expiry */
    if (stp_conn_get_expiry(conn) <= now) {
      int rv = stp_conn_handle_expiry(conn, now);
      if (rv == STP_ERR_IDLE_CLOSE) {
        printf("Connection timed out\n");
        break;
      }
    }
    
    /* Handle socket events */
    for (int i = 0; i < nfds; i++) {
      if (events[i].data.fd == sockfd) {
        ssize_t n = recv(sockfd, buf, sizeof(buf), 0);
        if (n > 0) {
          stp_ssize rv = stp_conn_read_pkt(conn, buf, n, now);
          if (rv < 0) {
            printf("Error: %d\n", (int)rv);
          }
        }
      }
    }
    
    /* Write pending packets */
    now = get_timestamp();
    stp_ssize nwrite;
    while ((nwrite = stp_conn_write_pkt(conn, buf, sizeof(buf), now)) > 0) {
      send(sockfd, buf, nwrite, 0);
    }
  }
  
  close(epfd);
}
```

---

## 10.6 Summary: Lessons Applied

```
┌─────────────────────────────────────────────────────────────────┐
│                    NGTCP2 LESSONS APPLIED                        │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  1. LAYERED STATE MODEL                                          │
│     ✓ Primary states (6) for lifecycle                          │
│     ✓ Flags (8+) for orthogonal concerns                        │
│     ✓ Sub-FSMs for complex features                             │
│                                                                  │
│  2. ASYNC-FRIENDLY DESIGN                                        │
│     ✓ Non-blocking API                                           │
│     ✓ Timestamp injection                                        │
│     ✓ Aggregated timer expiry                                    │
│     ✓ No internal I/O                                            │
│                                                                  │
│  3. SAFE TRANSITIONS                                             │
│     ✓ Guard conditions                                           │
│     ✓ Terminal states (CLOSING, DRAINING)                       │
│     ✓ Entry/exit actions                                         │
│     ✓ Callback notifications                                     │
│                                                                  │
│  4. ERROR HANDLING                                               │
│     ✓ Rich error codes                                           │
│     ✓ Error detail storage                                       │
│     ✓ Graceful vs abrupt shutdown                                │
│                                                                  │
│  5. EXTENSIBILITY                                                │
│     ✓ Flag space for future features                             │
│     ✓ Sub-FSM extension points                                   │
│     ✓ Callback versioning potential                              │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## 中文解释 (Chinese Explanation)

### 设计任务：应用 ngtcp2 经验

本节展示了如何将从 ngtcp2 学到的经验应用到新协议设计中：

**分层状态模型**
- 第1层：6个主状态（IDLE, HANDSHAKE, ESTABLISHED, CLOSING, DRAINING, CLOSED）
- 第2层：8+个标志位（正交关注点）
- 第3层：子FSM（路径验证、密钥轮换等）

**异步友好设计**
- 所有API非阻塞
- 时间戳注入
- 聚合定时器到期
- 无内部I/O

**安全转换**
- 守卫条件
- 终态（CLOSING, DRAINING）
- 入口/出口动作
- 回调通知

**错误处理**
- 丰富的错误码
- 错误详情存储
- 优雅vs突然关闭

**可扩展性**
- 标志位预留
- 子FSM扩展点
- 回调版本化潜力

### 集成示例

示例代码展示了如何将FSM与epoll事件循环集成：
1. 从FSM获取下一个到期时间
2. 使用该超时等待事件
3. 处理定时器到期
4. 处理socket事件
5. 写入待发送数据包

这种模式直接来自ngtcp2的设计，将状态管理与I/O完全分离。
