# Section 4: Transition Implementation

## 4.1 Core FSM Logic Location

The core FSM logic in ngtcp2 is distributed across several key functions:

### Primary State Transition Points

```c
// lib/ngtcp2_conn.c - Key functions

// Packet reading (state-dependent routing)
conn_read_handshake()          // Line 10014
ngtcp2_conn_read_pkt_versioned()  // Line 10208

// Packet writing (state-dependent generation)  
conn_write_handshake()         // Line 10402
ngtcp2_conn_write_vmsg()       // Line 12071

// Timer handling
ngtcp2_conn_handle_expiry()    // Line 11215
ngtcp2_conn_on_loss_detection_timer()  // Line 13230
```

### State Transition Map

```
Function                          | Transition
----------------------------------|------------------------------------------
conn_write_client_initial()       | CLIENT_INITIAL → CLIENT_WAIT_HANDSHAKE
conn_write_handshake() [client]   | CLIENT_WAIT_HANDSHAKE → POST_HANDSHAKE
conn_write_handshake() [server]   | SERVER_INITIAL → SERVER_WAIT_HANDSHAKE
conn_read_handshake() [server]    | SERVER_WAIT_HANDSHAKE → POST_HANDSHAKE
conn_recv_connection_close()      | * → DRAINING
conn_write_connection_close()     | * → CLOSING
conn_on_retry()                   | CLIENT_WAIT_HANDSHAKE → CLIENT_INITIAL
```

---

## 4.2 How Transitions are Distributed Across Functions

### The Distributed Switch Pattern

ngtcp2 uses state checks at multiple levels rather than a centralized FSM:

```c
// Level 1: API Entry Point
int ngtcp2_conn_read_pkt_versioned(...) {
  switch (conn->state) {
  case NGTCP2_CS_CLIENT_INITIAL:
  case NGTCP2_CS_CLIENT_WAIT_HANDSHAKE:
    nread = conn_read_handshake(conn, path, pi, pkt, pktlen, ts);
    break;
  case NGTCP2_CS_SERVER_INITIAL:
  case NGTCP2_CS_SERVER_WAIT_HANDSHAKE:
    nread = conn_read_handshake(conn, path, pi, pkt, pktlen, ts);
    break;
  case NGTCP2_CS_CLOSING:
    return NGTCP2_ERR_CLOSING;
  case NGTCP2_CS_DRAINING:
    return NGTCP2_ERR_DRAINING;
  case NGTCP2_CS_POST_HANDSHAKE:
    rv = conn_prepare_key_update(conn, ts);
    break;
  }
  // Further processing...
}

// Level 2: Handler Functions
static ngtcp2_ssize conn_read_handshake(...) {
  switch (conn->state) {
  case NGTCP2_CS_CLIENT_INITIAL:
    // Handle Initial packet as client
    return (ngtcp2_ssize)pktlen;
  case NGTCP2_CS_CLIENT_WAIT_HANDSHAKE:
    // Handle handshake continuation
    break;
  // ...
  }
}

// Level 3: Specific Handlers
static ngtcp2_ssize conn_recv_handshake_pkt(...) {
  // Packet-type specific processing
  // May trigger state transitions
}
```

### Benefits of Distribution

1. **Localized Logic**: Each function handles its specific concerns
2. **Reduced Coupling**: State checks are close to relevant code
3. **Easier Testing**: Individual functions can be tested
4. **Clear Responsibility**: Each layer has defined scope

---

## 4.3 Why QUIC Avoids a Single Giant Switch Statement

### Problems with Centralized FSM

```c
// ANTI-PATTERN: Giant centralized switch
void process_event(conn, event) {
  switch (conn->state) {
  case STATE_A:
    switch (event) {
    case EVENT_1: /* 100s of lines */ break;
    case EVENT_2: /* 100s of lines */ break;
    // ... 50+ events
    }
    break;
  case STATE_B:
    switch (event) {
    // ... repeat for each state
    }
    break;
  // ... 7 states × 50 events = 350 cases
  }
}
```

### ngtcp2's Approach: Functional Decomposition

```c
// PATTERN: Distributed state handling

// Entry point does coarse routing
ngtcp2_conn_read_pkt() {
  switch (conn->state) {
  case NGTCP2_CS_CLIENT_INITIAL:
  case NGTCP2_CS_CLIENT_WAIT_HANDSHAKE:
    return conn_read_handshake();  // Delegate
  // ...
  }
}

// Handler does fine-grained work
conn_read_handshake() {
  // State-specific logic only
}

// Frame handlers are state-agnostic
conn_recv_stream() {
  // Works the same in any non-terminal state
}
```

### Comparison Table

| Aspect | Giant Switch | Distributed (ngtcp2) |
|--------|--------------|---------------------|
| Lines per function | 1000s | 100-300 |
| State locality | All in one place | Spread appropriately |
| Event handling | Nested switches | Specialized handlers |
| Testability | Hard | Easy |
| Maintainability | Poor | Good |
| Adding new state | Many changes | Localized changes |

---

## 4.4 Entry/Exit Actions

### Entry Actions

State entry actions are performed immediately after transition:

```c
// Entering POST_HANDSHAKE state (client)
// lib/ngtcp2_conn.c:10528-10568
conn->state = NGTCP2_CS_POST_HANDSHAKE;  // Transition

// Entry actions follow immediately:
assert(conn->remote.transport_params);

// Process preferred address if present
if (conn->remote.transport_params->preferred_addr_present) {
  paddr = &conn->remote.transport_params->preferred_addr;
  ngtcp2_dcidtr_push_unused(&conn->dcid.dtr, 1, &paddr->cid,
                            paddr->stateless_reset_token);
}

// Process stateless reset token
if (conn->remote.transport_params->stateless_reset_token_present) {
  ngtcp2_dcid_set_token(&conn->dcid.current,
    conn->remote.transport_params->stateless_reset_token);
}

// Activate DCID
rv = conn_call_activate_dcid(conn, &conn->dcid.current);

// Process early RTB
conn_process_early_rtb(conn);

// Start PMTUD
if (!conn->local.settings.no_pmtud) {
  rv = conn_start_pmtud(conn);
}
```

### Exit Actions

Exit actions clean up resources when leaving a state:

```c
// Exiting Initial state - discard Initial keys
// lib/ngtcp2_conn.c:2852-2867
void ngtcp2_conn_discard_initial_state(ngtcp2_conn *conn, ngtcp2_tstamp ts) {
  if (!conn->in_pktns) {
    return;
  }
  // Exit action: discard Initial packet number space
  conn_discard_pktns(conn, &conn->in_pktns, ts);
  
  // Also discard version negotiation crypto if present
  conn_vneg_crypto_free(conn);
}

// Exiting Handshake state - discard Handshake keys
// lib/ngtcp2_conn.c:2868-2881
void ngtcp2_conn_discard_handshake_state(ngtcp2_conn *conn, ngtcp2_tstamp ts) {
  if (!conn->hs_pktns) {
    return;
  }
  conn_discard_pktns(conn, &conn->hs_pktns, ts);
}
```

---

## 4.5 Deferred Transitions

### Pattern: Delayed State Change

Some transitions are deferred until conditions are met:

```c
// Handshake completion is deferred
// lib/ngtcp2_conn.c:10506-10530
if (!(conn->flags & NGTCP2_CONN_FLAG_HANDSHAKE_COMPLETED)) {
  return res;  // Defer transition, return current result
}

if (!(conn->flags & NGTCP2_CONN_FLAG_TRANSPORT_PARAM_RECVED)) {
  return NGTCP2_ERR_REQUIRED_TRANSPORT_PARAM;  // Cannot transition yet
}

// All conditions met, now transition
conn->state = NGTCP2_CS_POST_HANDSHAKE;
```

### Pattern: Multi-Phase Transition

```c
// Server handshake confirmation is multi-phase
// Phase 1: TLS handshake completes (sets flag)
conn->flags |= NGTCP2_CONN_FLAG_TLS_HANDSHAKE_COMPLETED;

// Phase 2: Handshake DONE frame sent (later)
conn->flags |= NGTCP2_CONN_FLAG_HANDSHAKE_COMPLETED;

// Phase 3: Handshake confirmed by ACK (even later)
conn->flags |= NGTCP2_CONN_FLAG_HANDSHAKE_CONFIRMED;

// State only changes after all phases complete
```

### Pattern: Conditional Callback

```c
// Deferred key discard after timeout
// lib/ngtcp2_conn.c:11268-11271
if (conn->server && conn->early.ckm &&
    ngtcp2_tstamp_elapsed(conn->early.discard_started_ts, 3 * pto, ts)) {
  conn_discard_early_key(conn);
}
```

---

## 4.6 Transition Implementation Examples

### Example 1: Client Initial to Wait Handshake

```c
// lib/ngtcp2_conn.c:10413-10451
case NGTCP2_CS_CLIENT_INITIAL:
  // Action: Write Initial packet
  nwrite = conn_write_client_initial(conn, pi, dest, destlen, 
                                     write_datalen, ts);
  if (nwrite <= 0) {
    return nwrite;
  }
  
  // Optionally write 0-RTT data
  if (pending_early_datalen) {
    early_spktlen = conn_retransmit_retry_early(...);
  }
  
  // Transition
  conn->state = NGTCP2_CS_CLIENT_WAIT_HANDSHAKE;
  
  return nwrite + early_spktlen;
```

### Example 2: Receiving CONNECTION_CLOSE

```c
// lib/ngtcp2_conn.c:5952-5982
static int conn_recv_connection_close(ngtcp2_conn *conn,
                                      ngtcp2_connection_close *fr) {
  ngtcp2_ccerr *ccerr = &conn->rx.ccerr;
  
  // Immediate transition to DRAINING
  conn->state = NGTCP2_CS_DRAINING;
  
  // Store error information
  if (fr->type == NGTCP2_FRAME_CONNECTION_CLOSE) {
    ccerr->type = NGTCP2_CCERR_TYPE_TRANSPORT;
  } else {
    ccerr->type = NGTCP2_CCERR_TYPE_APPLICATION;
  }
  ccerr->error_code = fr->error_code;
  ccerr->frame_type = fr->frame_type;
  
  // Store reason phrase
  if (fr->reasonlen) {
    ccerr->reasonlen = ngtcp2_min_size(fr->reasonlen, 
                                        NGTCP2_CCERR_MAX_REASONLEN);
    ngtcp2_cpymem((uint8_t *)ccerr->reason, fr->reason, ccerr->reasonlen);
  }
  
  return 0;
}
```

### Example 3: Retry Handling (Back Transition)

```c
// lib/ngtcp2_conn.c:5462-5469
// Receiving Retry packet causes back-transition
conn->flags |= NGTCP2_CONN_FLAG_RECV_RETRY;

// Reset state to allow retransmission
conn->state = NGTCP2_CS_CLIENT_INITIAL;

// Clean up and prepare for retry
// (Memory cleanup handled carefully to avoid double-free)
```

---

## 4.7 Transition State Diagram (Detailed)

```
                    ┌─────────────────────────────┐
                    │      CLIENT_INITIAL         │
                    │                             │
                    │  Entry: Initialize conn     │
                    │  Exit: (none)               │
                    └──────────────┬──────────────┘
                                   │ write_client_initial()
                                   │
              Retry received       ▼
         ┌────────────────────────────────────────┐
         │                                        │
         │    ┌───────────────────────────────┐   │
         │    │   CLIENT_WAIT_HANDSHAKE       │   │
         │    │                               │   │
         └────│  Entry: Start 0-RTT if avail  │   │
              │  Exit: (none)                 │   │
              └──────────────┬────────────────┘   │
                             │ Handshake complete │
                             │ + transport params │
                             ▼                    │
              ┌───────────────────────────────┐   │
              │      POST_HANDSHAKE           │───┘
              │                               │ (error paths)
              │  Entry: Activate DCID,        │
              │         Start PMTUD,          │
              │         Process early RTB     │
              │  Exit: (none)                 │
              └──────────────┬────────────────┘
                             │
         ┌───────────────────┴───────────────────┐
         │ Send CONNECTION_CLOSE                 │ Recv CONNECTION_CLOSE
         ▼                                       ▼
┌─────────────────┐                 ┌─────────────────┐
│    CLOSING      │                 │    DRAINING     │
│                 │                 │                 │
│ Entry: Send CC  │                 │ Entry: Store    │
│ Exit: Cleanup   │                 │        error    │
└────────┬────────┘                 │ Exit: Cleanup   │
         │                          └────────┬────────┘
         │ Timeout                           │ Timeout
         ▼                                   ▼
    ┌─────────────────────────────────────────────┐
    │               [DELETED]                      │
    │                                             │
    │  ngtcp2_conn_del() cleans up all resources  │
    └─────────────────────────────────────────────┘
```

---

## 中文解释 (Chinese Explanation)

### FSM 逻辑分布

ngtcp2 的状态转换逻辑分布在多个层次：

1. **API 入口点**: 粗粒度的状态路由
2. **处理函数**: 细粒度的状态特定逻辑
3. **帧处理器**: 状态无关的帧处理

### 为什么避免单一大型 switch 语句

问题：
- 数千行代码在一个函数中
- 难以测试
- 难以维护
- 添加新状态需要修改多处

ngtcp2 的方案：
- 每个函数处理特定职责
- 状态检查靠近相关代码
- 单独的函数可以独立测试
- 每层有明确的范围

### 入口/出口动作

**入口动作**：状态转换后立即执行
- 例如：进入 POST_HANDSHAKE 后激活 DCID、启动 PMTUD

**出口动作**：离开状态时清理资源
- 例如：丢弃 Initial/Handshake 密钥

### 延迟转换

某些转换被延迟直到条件满足：
- 握手完成需要多个条件
- TLS 完成 + 传输参数接收 + ACK 确认
