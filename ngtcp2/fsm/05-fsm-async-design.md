# Section 5: FSM + Async Design

## 5.1 How FSM Integrates with Asynchronous I/O

### ngtcp2's Non-Blocking Design Philosophy

ngtcp2 is designed for integration with any async I/O framework (libevent, libuv, io_uring, etc.):

```
┌─────────────────────────────────────────────────────────────────┐
│                    Application Event Loop                        │
│   (libevent, libuv, epoll, kqueue, io_uring, etc.)              │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│   ┌─────────────┐    ┌─────────────┐    ┌─────────────┐        │
│   │ Socket FD   │    │   Timer     │    │ Other I/O   │        │
│   │  Readable   │    │   Expired   │    │   Events    │        │
│   └──────┬──────┘    └──────┬──────┘    └──────┬──────┘        │
│          │                  │                  │                │
│          ▼                  ▼                  ▼                │
│   ┌─────────────────────────────────────────────────────────┐  │
│   │                     ngtcp2_conn                          │  │
│   │                                                          │  │
│   │  ngtcp2_conn_read_pkt()    ← Socket readable             │  │
│   │  ngtcp2_conn_handle_expiry() ← Timer fired               │  │
│   │  ngtcp2_conn_write_pkt()   ← Application wants to send   │  │
│   │                                                          │  │
│   │  Returns immediately (non-blocking)                      │  │
│   └─────────────────────────────────────────────────────────┘  │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### Key Design Principles

1. **No Internal I/O**: ngtcp2 never performs socket operations
2. **No Internal Timers**: Application manages all timer scheduling
3. **Stateless Entry Points**: Each call is self-contained
4. **Timestamp-Based**: All timing uses application-provided timestamps

---

## 5.2 Timer Integration Pattern

### Getting the Next Timer Expiry

```c
// Application asks: "When should I wake up?"
ngtcp2_tstamp expiry = ngtcp2_conn_get_expiry(conn);

if (expiry == UINT64_MAX) {
  // No timer needed
} else {
  ngtcp2_tstamp now = get_current_timestamp();
  ngtcp2_duration timeout = expiry > now ? expiry - now : 0;
  
  // Schedule timer with event loop
  event_add(timer_event, timeout);
}
```

### Timer Expiry Handling

```c
// Timer callback
void on_timer_expired(void *arg) {
  ngtcp2_conn *conn = arg;
  ngtcp2_tstamp now = get_current_timestamp();
  
  // Handle all expired timers
  int rv = ngtcp2_conn_handle_expiry(conn, now);
  
  if (rv == NGTCP2_ERR_IDLE_CLOSE) {
    // Connection timed out
    close_connection(conn);
    return;
  }
  
  // May need to send packets (e.g., PTO probe)
  send_pending_packets(conn);
  
  // Reschedule timer
  schedule_timer(conn);
}
```

### Multiple Timer Sources

```c
// lib/ngtcp2_conn.c:11204-11213
ngtcp2_tstamp ngtcp2_conn_get_expiry(ngtcp2_conn *conn) {
  ngtcp2_tstamp res;
  
  // Loss detection timer
  res = ngtcp2_conn_loss_detection_expiry(conn);
  
  // ACK delay timer
  res = ngtcp2_min_uint64(res, ngtcp2_conn_ack_delay_expiry(conn));
  
  // Internal timers (path validation, PMTUD, etc.)
  res = ngtcp2_min_uint64(res, ngtcp2_conn_internal_expiry(conn));
  
  // Lost packet cleanup timer
  res = ngtcp2_min_uint64(res, ngtcp2_conn_lost_pkt_expiry(conn));
  
  // Keep-alive timer
  res = ngtcp2_min_uint64(res, conn_keep_alive_expiry(conn));
  
  // Handshake timeout
  res = ngtcp2_min_uint64(res, conn_handshake_expiry(conn));
  
  // Idle timeout
  res = ngtcp2_min_uint64(res, ngtcp2_conn_get_idle_expiry(conn));
  
  // Pacing timer
  res = ngtcp2_min_uint64(res, conn->tx.pacing.next_ts);
  
  return res;
}
```

---

## 5.3 Async I/O Integration Pattern

### Read Path

```c
// Application's socket read callback
void on_socket_readable(void *arg) {
  ngtcp2_conn *conn = arg;
  uint8_t buf[65536];
  ngtcp2_path path;
  ngtcp2_pkt_info pi;
  
  // 1. Read from socket (non-blocking)
  ssize_t nread = recvfrom(sockfd, buf, sizeof(buf), 0,
                           &path.remote.addr, &path.remote.addrlen);
  if (nread < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return;  // No more data
    }
    handle_error();
    return;
  }
  
  // 2. Get timestamp
  ngtcp2_tstamp ts = get_current_timestamp();
  
  // 3. Feed to ngtcp2
  int rv = ngtcp2_conn_read_pkt(conn, &path, &pi, buf, nread, ts);
  
  if (rv == NGTCP2_ERR_DRAINING) {
    // Connection received close, enter draining
    start_draining_timer(conn);
    return;
  }
  
  if (rv < 0) {
    handle_quic_error(conn, rv);
    return;
  }
  
  // 4. May need to send response packets
  send_pending_packets(conn);
  
  // 5. Reschedule timer
  schedule_timer(conn);
}
```

### Write Path

```c
// Application wants to send data
void send_pending_packets(ngtcp2_conn *conn) {
  uint8_t buf[1280];  // Min QUIC packet size
  ngtcp2_path_storage ps;
  ngtcp2_pkt_info pi;
  
  ngtcp2_path_storage_zero(&ps);
  
  while (1) {
    ngtcp2_tstamp ts = get_current_timestamp();
    
    ngtcp2_ssize nwrite = ngtcp2_conn_write_pkt(conn, &ps.path, &pi,
                                                buf, sizeof(buf), ts);
    
    if (nwrite < 0) {
      if (nwrite == NGTCP2_ERR_WRITE_MORE) {
        // Packet partially written, continue
        continue;
      }
      handle_quic_error(conn, nwrite);
      return;
    }
    
    if (nwrite == 0) {
      // Nothing to write
      break;
    }
    
    // Send packet
    sendto(sockfd, buf, nwrite, 0,
           &ps.path.remote.addr, ps.path.remote.addrlen);
  }
}
```

---

## 5.4 How ngtcp2 Avoids Race Conditions

### Single-Threaded Guarantee

ngtcp2 explicitly states that it is **not thread-safe**. All access to a connection must be from the same thread:

```
Thread Safety Model:

        Thread A                    Thread B
           │                           │
           ▼                           │
   ngtcp2_conn_read_pkt()             │  ← NOT ALLOWED
           │                           │
           ▼                           │
   ngtcp2_conn_write_pkt()            │
           │                           │
           ▼                           ▼
   Done                         ngtcp2_conn_write_pkt() ← NOW OK
```

### State Consistency Guarantees

```c
// All state updates happen atomically from caller's perspective

// Example: State check and transition are in same function call
int ngtcp2_conn_read_pkt_versioned(...) {
  // Check state
  switch (conn->state) {
  case NGTCP2_CS_CLOSING:
    return NGTCP2_ERR_CLOSING;  // Immediate return, no partial state
  // ...
  }
  
  // Process...
  
  // Transition (if needed)
  conn->state = NGTCP2_CS_POST_HANDSHAKE;  // Atomic assignment
  
  return result;  // Caller sees consistent state
}
```

### No Callbacks During Critical Sections

```c
// Callbacks are invoked at safe points, never mid-transition
static int conn_recv_stream(ngtcp2_conn *conn, const ngtcp2_stream *fr,
                           ngtcp2_tstamp ts) {
  // ... process frame ...
  
  // State is consistent before callback
  return conn_call_recv_stream_data(conn, strm, flags, offset, data, datalen);
}
```

---

## 5.5 Async Patterns in Sub-FSMs

### Path Validation Async Pattern

```c
// lib/ngtcp2_pv.h:92-117
struct ngtcp2_pv {
  ngtcp2_dcid dcid;
  ngtcp2_dcid fallback_dcid;
  ngtcp2_static_ringbuf_pv_ents ents;  // Pending challenges
  ngtcp2_duration timeout;
  ngtcp2_duration fallback_pto;
  ngtcp2_tstamp started_ts;
  size_t round;
  size_t probe_pkt_left;
  uint8_t flags;
};

// Path validation is async:
// 1. Send PATH_CHALLENGE (during write)
// 2. Wait for PATH_RESPONSE (async, during read)
// 3. Timeout if no response (timer event)
```

### Key Update Async Pattern

```c
// Key update confirmation is async:
// 1. Send packet with new key (sets KEY_UPDATE_NOT_CONFIRMED flag)
// 2. Wait for ACK of that packet
// 3. On ACK, clear flag and confirm key update

// lib/ngtcp2_rtb.c:815-819
if (conn && (conn->flags & NGTCP2_CONN_FLAG_KEY_UPDATE_NOT_CONFIRMED) &&
    (conn->flags & NGTCP2_CONN_FLAG_KEY_UPDATE_INITIATOR) &&
    ent->hd.pkt_num >= conn->pktns.crypto.tx.ckm->pkt_num) {
  conn->flags &= (uint32_t)~(NGTCP2_CONN_FLAG_KEY_UPDATE_NOT_CONFIRMED |
                             NGTCP2_CONN_FLAG_KEY_UPDATE_INITIATOR);
}
```

---

## 5.6 Event Loop Integration Example

### Complete Integration Pattern

```c
// Pseudo-code for event loop integration

struct connection {
  ngtcp2_conn *quic;
  int sockfd;
  struct event *read_event;
  struct event *timer_event;
};

void on_readable(evutil_socket_t fd, short events, void *arg) {
  struct connection *c = arg;
  uint8_t buf[65536];
  
  // Read all available data
  while (1) {
    ssize_t n = recv(fd, buf, sizeof(buf), 0);
    if (n < 0) break;
    
    ngtcp2_conn_read_pkt(c->quic, &path, &pi, buf, n, timestamp());
  }
  
  // Process writes
  process_writes(c);
  
  // Reschedule timer
  reschedule_timer(c);
}

void on_timer(evutil_socket_t fd, short events, void *arg) {
  struct connection *c = arg;
  
  int rv = ngtcp2_conn_handle_expiry(c->quic, timestamp());
  
  if (rv != 0) {
    handle_error(c, rv);
    return;
  }
  
  // Timer expiry may require sending packets
  process_writes(c);
  
  // Reschedule
  reschedule_timer(c);
}

void reschedule_timer(struct connection *c) {
  ngtcp2_tstamp expiry = ngtcp2_conn_get_expiry(c->quic);
  
  if (expiry == UINT64_MAX) {
    event_del(c->timer_event);
    return;
  }
  
  ngtcp2_tstamp now = timestamp();
  struct timeval tv = {0};
  
  if (expiry > now) {
    ngtcp2_duration d = expiry - now;
    tv.tv_sec = d / NGTCP2_SECONDS;
    tv.tv_usec = (d % NGTCP2_SECONDS) / NGTCP2_MICROSECONDS;
  }
  
  event_add(c->timer_event, &tv);
}
```

---

## 5.7 Async Design Summary

```
┌────────────────────────────────────────────────────────────────┐
│                     ASYNC DESIGN PRINCIPLES                     │
├────────────────────────────────────────────────────────────────┤
│                                                                 │
│  1. NO BLOCKING                                                 │
│     - All functions return immediately                          │
│     - No internal sleeps or waits                               │
│                                                                 │
│  2. NO INTERNAL I/O                                             │
│     - No socket operations                                      │
│     - No file operations                                        │
│     - Application handles all I/O                               │
│                                                                 │
│  3. TIMESTAMP-DRIVEN                                            │
│     - Application provides current time                         │
│     - Library returns next required wake-up time                │
│                                                                 │
│  4. SINGLE-THREADED                                             │
│     - No locks needed                                           │
│     - No race conditions possible                               │
│     - Simple state management                                   │
│                                                                 │
│  5. CALLBACK-BASED NOTIFICATIONS                                │
│     - Application receives events via callbacks                 │
│     - Callbacks invoked at safe points only                     │
│                                                                 │
└────────────────────────────────────────────────────────────────┘
```

---

## 中文解释 (Chinese Explanation)

### 异步 I/O 集成

ngtcp2 设计用于与任何异步 I/O 框架集成：

1. **不执行内部 I/O**: ngtcp2 从不执行 socket 操作
2. **不管理定时器**: 应用程序管理所有定时器调度
3. **无状态入口点**: 每次调用是自包含的
4. **基于时间戳**: 所有时间使用应用程序提供的时间戳

### 定时器集成模式

```c
// 获取下一个定时器到期时间
ngtcp2_tstamp expiry = ngtcp2_conn_get_expiry(conn);

// 设置事件循环定时器
event_add(timer_event, calculate_timeout(expiry));
```

### 避免竞态条件

ngtcp2 明确声明**非线程安全**：
- 对连接的所有访问必须来自同一线程
- 状态更新从调用者角度是原子的
- 回调在安全点调用，不会在转换中途调用

### 异步设计原则

1. **无阻塞**: 所有函数立即返回
2. **无内部 I/O**: 应用程序处理所有 I/O
3. **时间戳驱动**: 应用程序提供当前时间
4. **单线程**: 无需锁，无竞态条件
5. **回调通知**: 通过回调接收事件
