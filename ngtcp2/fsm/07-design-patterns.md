# Section 7: Design Patterns Used

## 7.1 Event-Driven FSM Pattern

### Overview

ngtcp2 implements a pure event-driven FSM where state transitions are triggered exclusively by external events rather than polling or busy-waiting.

```
┌───────────────────────────────────────────────────────────────┐
│                    EVENT-DRIVEN FSM PATTERN                    │
├───────────────────────────────────────────────────────────────┤
│                                                                │
│  External Event                  FSM Response                  │
│  ─────────────                  ────────────                   │
│                                                                │
│  ┌──────────────┐    trigger    ┌──────────────┐              │
│  │   Packet RX  │──────────────▶│  read_pkt()  │──┐           │
│  └──────────────┘               └──────────────┘  │           │
│                                                    │           │
│  ┌──────────────┐    trigger    ┌──────────────┐  │ State     │
│  │ Timer Expiry │──────────────▶│handle_expiry │──┤ Change    │
│  └──────────────┘               └──────────────┘  │           │
│                                                    │           │
│  ┌──────────────┐    trigger    ┌──────────────┐  │           │
│  │  App Request │──────────────▶│ write_pkt()  │──┘           │
│  └──────────────┘               └──────────────┘              │
│                                                                │
│  Key Characteristics:                                          │
│  • No polling loops                                            │
│  • No busy waiting                                             │
│  • State changes only on events                                │
│  • Deterministic behavior                                      │
│                                                                │
└───────────────────────────────────────────────────────────────┘
```

### Implementation Example

```c
// Event: Packet received
int ngtcp2_conn_read_pkt_versioned(...) {
  conn_update_timestamp(conn, ts);  // Sync time
  
  switch (conn->state) {
  case NGTCP2_CS_CLIENT_WAIT_HANDSHAKE:
    // Event triggers handshake processing
    nread = conn_read_handshake(conn, path, pi, pkt, pktlen, ts);
    // Potentially triggers state transition
    break;
  // ...
  }
}

// Event: Timer expired
int ngtcp2_conn_handle_expiry(ngtcp2_conn *conn, ngtcp2_tstamp ts) {
  if (ngtcp2_conn_get_idle_expiry(conn) <= ts) {
    return NGTCP2_ERR_IDLE_CLOSE;  // Triggers cleanup
  }
  // Process other timer expirations
}
```

### Trade-offs

| Advantage | Disadvantage |
|-----------|--------------|
| Low CPU usage | Requires event loop integration |
| Predictable behavior | More complex setup |
| No race conditions | Application must drive timers |
| Easy to reason about | Callback complexity |

---

## 7.2 Layered FSMs Pattern

### Overview

ngtcp2 uses multiple independent FSMs organized in layers, each managing a specific concern.

```
┌───────────────────────────────────────────────────────────────┐
│                      LAYERED FSM ARCHITECTURE                  │
├───────────────────────────────────────────────────────────────┤
│                                                                │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │ Layer 1: Connection Lifecycle FSM                        │  │
│  │ States: CLIENT_INITIAL, CLIENT_WAIT, POST_HANDSHAKE...   │  │
│  │ Scope: Overall connection state                          │  │
│  └─────────────────────────────────────────────────────────┘  │
│                            │                                   │
│                            │ contains                          │
│                            ▼                                   │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │ Layer 2: Sub-FSMs (Independent)                          │  │
│  │                                                           │  │
│  │  ┌───────────┐ ┌───────────┐ ┌───────────┐ ┌──────────┐  │  │
│  │  │Path Valid.│ │ECN Valid. │ │Key Update │ │  PMTUD   │  │  │
│  │  │           │ │           │ │           │ │          │  │  │
│  │  │ TESTING   │ │ TESTING   │ │ PENDING   │ │SEARCHING │  │  │
│  │  │ SUCCESS   │ │ CAPABLE   │ │ CONFIRMED │ │ COMPLETE │  │  │
│  │  │ FAILED    │ │ FAILED    │ │           │ │          │  │  │
│  │  └───────────┘ └───────────┘ └───────────┘ └──────────┘  │  │
│  └─────────────────────────────────────────────────────────┘  │
│                            │                                   │
│                            │ contains                          │
│                            ▼                                   │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │ Layer 3: Per-Stream FSMs                                  │  │
│  │                                                           │  │
│  │  Each stream has independent send/recv state machines    │  │
│  └─────────────────────────────────────────────────────────┘  │
│                                                                │
└───────────────────────────────────────────────────────────────┘
```

### Implementation Example

```c
// Layer 1: Main connection FSM
struct ngtcp2_conn {
  ngtcp2_conn_state state;      // Layer 1 state
  
  // Layer 2: Sub-FSMs
  ngtcp2_pv *pv;                // Path validation FSM
  ngtcp2_pmtud *pmtud;          // PMTUD FSM
  struct {
    ngtcp2_ecn_state state;     // ECN validation FSM
  } tx.ecn;
  
  // Layer 3: Stream FSMs (in strms map)
  ngtcp2_map strms;
};

// Sub-FSM example: Path Validation
struct ngtcp2_pv {
  uint8_t flags;                // PV-specific flags
  ngtcp2_duration timeout;
  size_t round;
  // Independent lifecycle
};
```

### Trade-offs

| Advantage | Disadvantage |
|-----------|--------------|
| Separation of concerns | More complex data structures |
| Independent testing | Need to coordinate FSMs |
| Easier evolution | More memory per connection |
| Localized changes | Potential for inconsistency |

---

## 7.3 Guarded Transitions Pattern

### Overview

State transitions are protected by guard conditions that must be satisfied before the transition occurs.

```
┌───────────────────────────────────────────────────────────────┐
│                   GUARDED TRANSITION PATTERN                   │
├───────────────────────────────────────────────────────────────┤
│                                                                │
│                    Current State                               │
│                         │                                      │
│                         ▼                                      │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │                    Guard Check                           │  │
│  │                                                          │  │
│  │  if (condition1 && condition2 && !condition3) {          │  │
│  │    // Allow transition                                   │  │
│  │  } else {                                                │  │
│  │    // Block transition, return error or defer            │  │
│  │  }                                                       │  │
│  └─────────────────────────────────────────────────────────┘  │
│                         │                                      │
│          ┌──────────────┴──────────────┐                      │
│          │ Pass                         │ Fail                 │
│          ▼                              ▼                      │
│  ┌───────────────┐              ┌───────────────┐             │
│  │  New State    │              │  Stay/Error   │             │
│  └───────────────┘              └───────────────┘             │
│                                                                │
└───────────────────────────────────────────────────────────────┘
```

### Implementation Examples

```c
// Guard: Must have transport params before completing handshake
// lib/ngtcp2_conn.c:10511-10512
if (!(conn->flags & NGTCP2_CONN_FLAG_TRANSPORT_PARAM_RECVED)) {
  return NGTCP2_ERR_REQUIRED_TRANSPORT_PARAM;
}
// Only then: conn->state = NGTCP2_CS_POST_HANDSHAKE;

// Guard: Check multiple flags for key update
// lib/ngtcp2_conn.c:11117-11122
if (!(conn->flags & NGTCP2_CONN_FLAG_HANDSHAKE_CONFIRMED) ||
    (conn->flags & NGTCP2_CONN_FLAG_KEY_UPDATE_NOT_CONFIRMED) ||
    conn->crypto.key_update.new_tx_ckm ||
    conn->crypto.key_update.new_rx_ckm) {
  return NGTCP2_ERR_INVALID_STATE;
}

// Guard: TLS handshake must be complete
static int conn_is_tls_handshake_completed(ngtcp2_conn *conn) {
  return (conn->flags & NGTCP2_CONN_FLAG_TLS_HANDSHAKE_COMPLETED) &&
         (conn->flags & NGTCP2_CONN_FLAG_HANDSHAKE_COMPLETED);
}
```

### Trade-offs

| Advantage | Disadvantage |
|-----------|--------------|
| Prevents invalid states | More verbose code |
| Self-documenting preconditions | Can miss edge cases |
| Early error detection | Scattered guard logic |
| Safer state machine | Need comprehensive guards |

---

## 7.4 Flag-Augmented State Machine Pattern

### Overview

The primary state enum is augmented with boolean flags to represent sub-states without exponential state explosion.

```
┌───────────────────────────────────────────────────────────────┐
│              FLAG-AUGMENTED STATE MACHINE PATTERN              │
├───────────────────────────────────────────────────────────────┤
│                                                                │
│  Traditional Approach (State Explosion):                       │
│  ─────────────────────────────────────                         │
│  States = Base × Flag1 × Flag2 × Flag3 × ...                   │
│         = 7 × 2 × 2 × 2 × ... = 7 × 2^18 = 1,835,008 states!  │
│                                                                │
│  Flag-Augmented Approach:                                      │
│  ────────────────────────                                      │
│  ┌─────────────────────────┐  ┌────────────────────────────┐  │
│  │   Primary State (7)     │  │   Flags (18 bits)          │  │
│  │                         │  │                            │  │
│  │   CLIENT_INITIAL        │  │  TLS_HANDSHAKE_COMPLETED   │  │
│  │   CLIENT_WAIT           │  │  TRANSPORT_PARAM_RECVED    │  │
│  │   SERVER_INITIAL        │  │  HANDSHAKE_CONFIRMED       │  │
│  │   SERVER_WAIT           │  │  KEY_UPDATE_NOT_CONFIRMED  │  │
│  │   POST_HANDSHAKE        │  │  EARLY_DATA_REJECTED       │  │
│  │   CLOSING               │  │  ...                       │  │
│  │   DRAINING              │  │                            │  │
│  └─────────────────────────┘  └────────────────────────────┘  │
│                                                                │
│  Effective State = Primary + Relevant Flags                    │
│  Example: POST_HANDSHAKE + HANDSHAKE_CONFIRMED + KEY_UPDATE... │
│                                                                │
└───────────────────────────────────────────────────────────────┘
```

### Implementation

```c
// Primary state
ngtcp2_conn_state state;

// Augmenting flags
uint32_t flags;

// Usage: Combine state and flags for decisions
switch (conn->state) {
case NGTCP2_CS_POST_HANDSHAKE:
  if (conn->flags & NGTCP2_CONN_FLAG_HANDSHAKE_CONFIRMED) {
    // Fully established connection
  } else {
    // Still waiting for handshake confirmation
  }
  break;
}

// Set flag
conn->flags |= NGTCP2_CONN_FLAG_TLS_HANDSHAKE_COMPLETED;

// Clear flag
conn->flags &= (uint32_t)~NGTCP2_CONN_FLAG_KEY_UPDATE_NOT_CONFIRMED;

// Check flag
if (conn->flags & NGTCP2_CONN_FLAG_RECV_RETRY) {
  // Handle retry case
}
```

### Trade-offs

| Advantage | Disadvantage |
|-----------|--------------|
| Avoids state explosion | Can lead to invalid combinations |
| Memory efficient | Harder to visualize all states |
| Flexible combinations | Need careful documentation |
| Easy to add new flags | Flag interactions can be subtle |

---

## 7.5 Callback Pattern for State Notifications

### Overview

State changes are communicated to the application via callback functions, decoupling the FSM from application logic.

```
┌───────────────────────────────────────────────────────────────┐
│                    CALLBACK NOTIFICATION PATTERN               │
├───────────────────────────────────────────────────────────────┤
│                                                                │
│  ┌─────────────────┐                  ┌─────────────────────┐ │
│  │    ngtcp2_conn  │                  │    Application      │ │
│  │                 │                  │                     │ │
│  │  State change   │                  │  Callback handlers  │ │
│  │       │         │                  │                     │ │
│  │       ▼         │   invoke         │  ┌───────────────┐  │ │
│  │  ┌──────────┐   │─────────────────▶│  │ handshake_    │  │ │
│  │  │ callback │   │                  │  │ completed()   │  │ │
│  │  └──────────┘   │                  │  └───────────────┘  │ │
│  │                 │                  │                     │ │
│  │                 │   invoke         │  ┌───────────────┐  │ │
│  │                 │─────────────────▶│  │ recv_stream_  │  │ │
│  │                 │                  │  │ data()        │  │ │
│  │                 │                  │  └───────────────┘  │ │
│  │                 │                  │                     │ │
│  └─────────────────┘                  └─────────────────────┘ │
│                                                                │
│  Callbacks are invoked at "safe points" only:                  │
│  • After state is fully consistent                             │
│  • Before returning to caller                                  │
│  • Never mid-transition                                        │
│                                                                │
└───────────────────────────────────────────────────────────────┘
```

### Implementation

```c
// Callback structure (abbreviated)
typedef struct ngtcp2_callbacks {
  ngtcp2_client_initial client_initial;
  ngtcp2_recv_client_initial recv_client_initial;
  ngtcp2_recv_crypto_data recv_crypto_data;
  ngtcp2_handshake_completed handshake_completed;
  ngtcp2_recv_stream_data recv_stream_data;
  ngtcp2_stream_close stream_close;
  // ... many more
} ngtcp2_callbacks;

// Invoking callback at safe point
static int conn_call_handshake_completed(ngtcp2_conn *conn) {
  int rv;
  
  if (!conn->callbacks.handshake_completed) {
    return 0;  // Optional callback
  }
  
  rv = conn->callbacks.handshake_completed(&conn->local.settings.qlog,
                                           conn, conn->user_data);
  if (rv != 0) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }
  
  return 0;
}
```

### Trade-offs

| Advantage | Disadvantage |
|-----------|--------------|
| Clean separation | Callback overhead |
| Application flexibility | Can't block in callbacks |
| Easy to extend | Error handling complexity |
| Non-intrusive | Callback ordering matters |

---

## 7.6 Timestamp Injection Pattern

### Overview

All time-dependent operations use externally provided timestamps rather than internal clock calls.

```
┌───────────────────────────────────────────────────────────────┐
│                   TIMESTAMP INJECTION PATTERN                  │
├───────────────────────────────────────────────────────────────┤
│                                                                │
│  Application                         ngtcp2                    │
│  ───────────                         ──────                    │
│                                                                │
│  ┌─────────────┐                                              │
│  │  Get Time   │  ts = clock_gettime()                        │
│  └──────┬──────┘                                              │
│         │                                                      │
│         │  pass timestamp                                      │
│         ▼                                                      │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │  ngtcp2_conn_read_pkt(conn, path, pi, buf, len, ts)     │  │
│  │  ngtcp2_conn_handle_expiry(conn, ts)                    │  │
│  │  ngtcp2_conn_write_pkt(conn, path, pi, buf, len, ts)    │  │
│  └─────────────────────────────────────────────────────────┘  │
│         │                                                      │
│         │  returns expiry                                      │
│         ▼                                                      │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │  expiry = ngtcp2_conn_get_expiry(conn)                  │  │
│  │  timeout = expiry - ts                                  │  │
│  │  schedule_timer(timeout)                                │  │
│  └─────────────────────────────────────────────────────────┘  │
│                                                                │
│  Benefits:                                                     │
│  • Testable (can control time in tests)                        │
│  • Consistent (all operations use same timestamp)              │
│  • Flexible (works with any clock source)                      │
│  • Reproducible (same inputs = same behavior)                  │
│                                                                │
└───────────────────────────────────────────────────────────────┘
```

### Implementation

```c
// Every API function takes timestamp parameter
ngtcp2_ssize ngtcp2_conn_read_pkt(..., ngtcp2_tstamp ts);
ngtcp2_ssize ngtcp2_conn_write_pkt(..., ngtcp2_tstamp ts);
int ngtcp2_conn_handle_expiry(ngtcp2_conn *conn, ngtcp2_tstamp ts);

// Internal timestamp update
static void conn_update_timestamp(ngtcp2_conn *conn, ngtcp2_tstamp ts) {
  // Store for internal use
}
```

---

## 7.7 Pattern Summary

```
┌─────────────────────────────────────────────────────────────────┐
│                      DESIGN PATTERNS SUMMARY                     │
├───────────────────┬─────────────────────────────────────────────┤
│ Pattern           │ Purpose                                      │
├───────────────────┼─────────────────────────────────────────────┤
│ Event-Driven FSM  │ React to external events, no polling        │
│                   │                                              │
│ Layered FSMs      │ Separate concerns, independent evolution    │
│                   │                                              │
│ Guarded Trans.    │ Prevent invalid state transitions           │
│                   │                                              │
│ Flag-Augmented    │ Avoid state explosion with flexible flags   │
│                   │                                              │
│ Callback Notif.   │ Decouple FSM from application logic         │
│                   │                                              │
│ Timestamp Inject. │ Testable, deterministic time handling       │
└───────────────────┴─────────────────────────────────────────────┘
```

---

## 中文解释 (Chinese Explanation)

### 使用的设计模式

1. **事件驱动 FSM**: 状态转换仅由外部事件触发，无轮询或忙等待

2. **分层 FSM**: 多个独立 FSM 分层组织，每层管理特定关注点
   - 第1层：连接生命周期
   - 第2层：子FSM（路径验证、ECN、密钥更新等）
   - 第3层：流级别 FSM

3. **守卫转换**: 状态转换受守卫条件保护
   - 必须满足所有前置条件
   - 防止进入无效状态

4. **标志增强状态机**: 用标志位扩展主状态
   - 避免状态爆炸（7个状态 × 2^18 标志 = 可管理）
   - 灵活组合

5. **回调通知**: 通过回调函数通知应用程序状态变化
   - 在"安全点"调用
   - 解耦 FSM 和应用逻辑

6. **时间戳注入**: 所有时间相关操作使用外部提供的时间戳
   - 可测试
   - 一致性
   - 可重现
