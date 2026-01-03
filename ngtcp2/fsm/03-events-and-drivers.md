# Section 3: Events & Drivers

## 3.1 Event Categories in QUIC FSM

QUIC's FSM is driven by three primary event categories:

```
                    ┌─────────────────┐
                    │   ngtcp2_conn   │
                    │      FSM        │
                    └────────┬────────┘
                             │
         ┌───────────────────┼───────────────────┐
         │                   │                   │
         ▼                   ▼                   ▼
┌─────────────────┐ ┌─────────────────┐ ┌─────────────────┐
│ Network Events  │ │  Timer Events   │ │  API Events     │
│                 │ │                 │ │                 │
│ - Packet RX     │ │ - Loss detect   │ │ - write_pkt     │
│ - Path change   │ │ - Idle timeout  │ │ - read_pkt      │
│ - ECN feedback  │ │ - ACK delay     │ │ - open_stream   │
│                 │ │ - Keep-alive    │ │ - close_stream  │
│                 │ │ - Handshake TO  │ │ - shutdown      │
└─────────────────┘ └─────────────────┘ └─────────────────┘
```

---

## 3.2 Network Events

### Packet Reception Event

The primary network event is receiving a packet:

```c
// lib/ngtcp2_conn.c:10208-10307
int ngtcp2_conn_read_pkt_versioned(ngtcp2_conn *conn, const ngtcp2_path *path,
                                   int pkt_info_version,
                                   const ngtcp2_pkt_info *pi,
                                   const uint8_t *pkt, size_t pktlen,
                                   ngtcp2_tstamp ts) {
  // Update internal timestamp
  conn_update_timestamp(conn, ts);
  
  // State-dependent packet processing
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
  
  return conn_recv_cpkt(conn, path, pi, pkt, pktlen, ts);
}
```

### Path Change Event

Detected when packet arrives from a different path:

```c
// lib/ngtcp2_conn.c:8736-8920
static int conn_recv_non_probing_pkt_on_new_path(ngtcp2_conn *conn,
                                                 const ngtcp2_path *path,
                                                 size_t dgramlen,
                                                 int new_cid_used,
                                                 ngtcp2_tstamp ts) {
  // Triggers path validation sub-FSM
  // May trigger connection migration
}
```

### ECN Feedback Event

```c
// ECN state transitions based on ACK feedback
// lib/ngtcp2_conn.c:933-999
static void conn_handle_tx_ecn(ngtcp2_conn *conn, ngtcp2_pkt_info *pi,
                               uint16_t *prtb_entry_flags, ngtcp2_pktns *pktns,
                               const ngtcp2_pkt_hd *hd, ngtcp2_tstamp ts) {
  // Updates ECN validation state based on received feedback
}
```

---

## 3.3 Timer Events

ngtcp2 uses a unified timer model with multiple expiry sources:

### Timer Architecture

```c
// lib/ngtcp2_conn.c:11204-11213
ngtcp2_tstamp ngtcp2_conn_get_expiry(ngtcp2_conn *conn) {
  ngtcp2_tstamp res;
  
  // Combine all timer sources
  res = ngtcp2_min_uint64(ngtcp2_conn_loss_detection_expiry(conn),
                          ngtcp2_conn_ack_delay_expiry(conn));
  res = ngtcp2_min_uint64(res, ngtcp2_conn_internal_expiry(conn));
  res = ngtcp2_min_uint64(res, ngtcp2_conn_lost_pkt_expiry(conn));
  res = ngtcp2_min_uint64(res, conn_keep_alive_expiry(conn));
  res = ngtcp2_min_uint64(res, conn_handshake_expiry(conn));
  res = ngtcp2_min_uint64(res, ngtcp2_conn_get_idle_expiry(conn));
  res = ngtcp2_min_uint64(res, conn->tx.pacing.next_ts);
  
  return res;
}
```

### Timer Types

| Timer | Purpose | Handler |
|-------|---------|---------|
| `loss_detection_timer` | Detect packet loss, trigger retransmission | `ngtcp2_conn_on_loss_detection_timer()` |
| `ack_delay_expiry` | Limit ACK delay | `acktr_cancel_expired_ack_delay_timer()` |
| `idle_expiry` | Close idle connections | Returns `NGTCP2_ERR_IDLE_CLOSE` |
| `handshake_expiry` | Handshake timeout | Returns `NGTCP2_ERR_HANDSHAKE_TIMEOUT` |
| `keep_alive_expiry` | Send keep-alive | Triggers PING frame |
| `pacing.next_ts` | Pace packet transmission | Controls write timing |

### Expiry Handling

```c
// lib/ngtcp2_conn.c:11215-11279
int ngtcp2_conn_handle_expiry(ngtcp2_conn *conn, ngtcp2_tstamp ts) {
  conn_update_timestamp(conn, ts);
  
  // Check each timer type
  if (ngtcp2_conn_get_idle_expiry(conn) <= ts) {
    return NGTCP2_ERR_IDLE_CLOSE;
  }
  
  ngtcp2_conn_cancel_expired_ack_delay_timer(conn, ts);
  conn_cancel_expired_keep_alive_timer(conn, ts);
  conn_cancel_expired_pkt_tx_timer(conn, ts);
  ngtcp2_conn_remove_lost_pkt(conn, ts);
  
  // Path validation timer
  if (conn->pv) {
    ngtcp2_pv_cancel_expired_timer(conn->pv, ts);
  }
  
  // PMTUD timer
  if (conn->pmtud) {
    ngtcp2_pmtud_handle_expiry(conn->pmtud, ts);
    if (ngtcp2_pmtud_finished(conn->pmtud)) {
      ngtcp2_conn_stop_pmtud(conn);
    }
  }
  
  // Loss detection timer
  if (ngtcp2_conn_loss_detection_expiry(conn) <= ts) {
    rv = ngtcp2_conn_on_loss_detection_timer(conn, ts);
    if (rv != 0) {
      return rv;
    }
  }
  
  // Handshake timeout
  if (!conn_is_tls_handshake_completed(conn) &&
      ngtcp2_tstamp_elapsed(conn->local.settings.initial_ts,
                            conn->local.settings.handshake_timeout, ts)) {
    return NGTCP2_ERR_HANDSHAKE_TIMEOUT;
  }
  
  return 0;
}
```

---

## 3.4 API-Driven Events

### Write Events

```c
// Application initiates packet writing
// lib/ngtcp2_conn.c:11884-11906
ngtcp2_ssize ngtcp2_conn_write_stream_versioned(...) {
  // Triggers state-dependent behavior
}

// lib/ngtcp2_conn.c:12071-12408
ngtcp2_ssize ngtcp2_conn_write_vmsg(ngtcp2_conn *conn, ...) {
  switch (conn->state) {
  case NGTCP2_CS_CLIENT_INITIAL:
  case NGTCP2_CS_CLIENT_WAIT_HANDSHAKE:
    // Handshake path
    break;
  case NGTCP2_CS_SERVER_INITIAL:
  case NGTCP2_CS_SERVER_WAIT_HANDSHAKE:
    // Server handshake path
    break;
  case NGTCP2_CS_POST_HANDSHAKE:
    // Normal data path
    break;
  case NGTCP2_CS_CLOSING:
    return NGTCP2_ERR_CLOSING;
  case NGTCP2_CS_DRAINING:
    return NGTCP2_ERR_DRAINING;
  }
}
```

### Stream Operations

```c
// Open stream
int ngtcp2_conn_open_bidi_stream(ngtcp2_conn *conn, int64_t *pstream_id,
                                 void *stream_user_data);
int ngtcp2_conn_open_uni_stream(ngtcp2_conn *conn, int64_t *pstream_id,
                                void *stream_user_data);

// Shutdown stream
int ngtcp2_conn_shutdown_stream(ngtcp2_conn *conn, uint32_t flags,
                                int64_t stream_id, uint64_t app_error_code);
```

### Connection Close

```c
// Graceful close
ngtcp2_ssize ngtcp2_conn_write_connection_close_pkt(
  ngtcp2_conn *conn, ngtcp2_path *path, ngtcp2_pkt_info *pi, uint8_t *dest,
  size_t destlen, uint64_t error_code, const uint8_t *reason, size_t reasonlen,
  ngtcp2_tstamp ts);

// Triggers: conn->state = NGTCP2_CS_CLOSING;
```

---

## 3.5 Event Serialization

### Single-Threaded Model

ngtcp2 is designed for single-threaded use, avoiding race conditions:

```
Application Thread
        │
        ▼
┌───────────────────────────────────────────────┐
│              ngtcp2_conn                       │
│                                               │
│  1. conn_update_timestamp(ts)                 │
│  2. Process event                             │
│  3. Update state                              │
│  4. Return to application                     │
│                                               │
│  [No concurrent access allowed]               │
└───────────────────────────────────────────────┘
```

### Timestamp Synchronization

All events are stamped with the same timestamp:

```c
// lib/ngtcp2_conn.c:77-87
static void conn_update_timestamp(ngtcp2_conn *conn, ngtcp2_tstamp ts) {
  // Ensure monotonic time
  // Used to synchronize all state decisions
}
```

---

## 3.6 How FSM Transitions are Triggered Safely

### Pattern 1: State Check Before Action

```c
// Always check current state before transition
switch (conn->state) {
case NGTCP2_CS_CLOSING:
  return NGTCP2_ERR_CLOSING;  // No action in terminal state
case NGTCP2_CS_DRAINING:
  return NGTCP2_ERR_DRAINING; // No action in terminal state
default:
  // Proceed with operation
}
```

### Pattern 2: Atomic State Updates

```c
// State changes are simple assignments
// No partial state updates
conn->state = NGTCP2_CS_POST_HANDSHAKE;
```

### Pattern 3: Flag Manipulation

```c
// Setting flags
conn->flags |= NGTCP2_CONN_FLAG_HANDSHAKE_COMPLETED;

// Clearing flags
conn->flags &= (uint32_t)~NGTCP2_CONN_FLAG_KEY_UPDATE_NOT_CONFIRMED;
```

### Pattern 4: Callback-Based Notifications

```c
// Notify application of state changes via callbacks
static int conn_call_handshake_completed(ngtcp2_conn *conn) {
  int rv;
  rv = conn->callbacks.handshake_completed(&conn->local.settings.qlog, conn,
                                            conn->user_data);
  if (rv != 0) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }
  return 0;
}
```

---

## 3.7 Event Flow Diagram

```
                          Application
                              │
        ┌─────────────────────┼─────────────────────┐
        │                     │                     │
        ▼                     ▼                     ▼
   read_pkt()           write_pkt()          handle_expiry()
        │                     │                     │
        ▼                     ▼                     ▼
┌──────────────────────────────────────────────────────────────┐
│                     conn_update_timestamp()                   │
└──────────────────────────────────────────────────────────────┘
        │                     │                     │
        ▼                     ▼                     ▼
┌──────────────────────────────────────────────────────────────┐
│                      State Check (switch)                     │
│   CLIENT_INITIAL | CLIENT_WAIT | SERVER_INITIAL | ...        │
└──────────────────────────────────────────────────────────────┘
        │                     │                     │
        ▼                     ▼                     ▼
┌──────────────────────────────────────────────────────────────┐
│                     Event Processing                          │
│  - Decrypt/Encrypt                                            │
│  - Frame handling                                             │
│  - Timer management                                           │
└──────────────────────────────────────────────────────────────┘
        │                     │                     │
        ▼                     ▼                     ▼
┌──────────────────────────────────────────────────────────────┐
│                     State Transition                          │
│  conn->state = NEW_STATE;                                     │
│  conn->flags |= NEW_FLAGS;                                    │
└──────────────────────────────────────────────────────────────┘
        │                     │                     │
        ▼                     ▼                     ▼
┌──────────────────────────────────────────────────────────────┐
│                     Callbacks (optional)                      │
│  handshake_completed(), recv_stream_data(), etc.             │
└──────────────────────────────────────────────────────────────┘
        │
        ▼
   Return to Application
```

---

## 中文解释 (Chinese Explanation)

### 事件类型

QUIC FSM 由三类事件驱动：

1. **网络事件**
   - 数据包接收：触发状态相关的处理逻辑
   - 路径变化：可能触发连接迁移
   - ECN 反馈：更新 ECN 验证状态

2. **定时器事件**
   - 丢包检测定时器：触发重传
   - 空闲超时：关闭空闲连接
   - 握手超时：终止握手
   - 保活定时器：发送 PING 帧
   - 发送节奏：控制发包速率

3. **API 事件**
   - 写操作：发送数据包
   - 流操作：打开/关闭流
   - 连接关闭：优雅关闭

### 事件序列化

ngtcp2 采用单线程模型：
- 所有事件在同一线程中处理
- 使用时间戳同步所有状态决策
- 避免并发访问，无需锁机制

### 安全状态转换

1. **先检查后执行**: 在执行操作前检查当前状态
2. **原子状态更新**: 状态变化是简单的赋值操作
3. **标志位操作**: 使用位操作设置/清除标志
4. **回调通知**: 通过回调通知应用程序状态变化
