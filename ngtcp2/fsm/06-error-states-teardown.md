# Section 6: Error States & Teardown

## 6.1 How Connection Failure is Handled

### Error Classification

ngtcp2 categorizes errors into several types:

```c
// Fatal vs Non-Fatal Errors

// Fatal errors - connection must be closed
NGTCP2_ERR_FATAL = -500
NGTCP2_ERR_NOMEM
NGTCP2_ERR_CALLBACK_FAILURE
NGTCP2_ERR_CRYPTO            // TLS error
NGTCP2_ERR_PROTO             // Protocol violation

// Non-fatal errors - connection can continue
NGTCP2_ERR_INVALID_ARGUMENT
NGTCP2_ERR_STREAM_NOT_FOUND

// Terminal states - connection is ending
NGTCP2_ERR_CLOSING           // Sent CONNECTION_CLOSE
NGTCP2_ERR_DRAINING          // Received CONNECTION_CLOSE
NGTCP2_ERR_IDLE_CLOSE        // Idle timeout
NGTCP2_ERR_HANDSHAKE_TIMEOUT // Handshake timed out
```

### Error Handling Flow

```
                     Error Detected
                          │
                          ▼
              ┌───────────────────────┐
              │   Is it recoverable?  │
              └───────────┬───────────┘
                    │           │
                    │ No        │ Yes
                    ▼           ▼
            ┌─────────────┐  ┌─────────────┐
            │   Close     │  │  Continue   │
            │ Connection  │  │ Processing  │
            └──────┬──────┘  └─────────────┘
                   │
                   ▼
         ┌─────────────────────┐
         │ Write CONNECTION_   │
         │ CLOSE packet        │
         └──────────┬──────────┘
                    │
                    ▼
         ┌─────────────────────┐
         │ Transition to       │
         │ CLOSING state       │
         └─────────────────────┘
```

---

## 6.2 Connection Close Error Handling

### Transport Error (QUIC Protocol Error)

```c
// lib/ngtcp2_conn.c:12642-12645
void ngtcp2_ccerr_set_transport_error(ngtcp2_ccerr *ccerr, uint64_t error_code,
                                      const uint8_t *reason, size_t reasonlen) {
  ccerr_init(ccerr, NGTCP2_CCERR_TYPE_TRANSPORT, error_code, reason, reasonlen);
}

// Usage: Protocol violation
ngtcp2_ccerr ccerr;
ngtcp2_ccerr_set_transport_error(&ccerr, NGTCP2_PROTOCOL_VIOLATION, NULL, 0);
ngtcp2_conn_write_connection_close(conn, path, pi, buf, buflen, &ccerr, ts);
```

### Application Error

```c
// lib/ngtcp2_conn.c:12683-12690
void ngtcp2_ccerr_set_application_error(ngtcp2_ccerr *ccerr,
                                        uint64_t error_code,
                                        const uint8_t *reason,
                                        size_t reasonlen) {
  ccerr_init(ccerr, NGTCP2_CCERR_TYPE_APPLICATION, error_code, reason, reasonlen);
}

// Usage: Application-level close (e.g., HTTP/3 error)
ngtcp2_ccerr ccerr;
ngtcp2_ccerr_set_application_error(&ccerr, H3_NO_ERROR, NULL, 0);
ngtcp2_conn_write_connection_close(conn, path, pi, buf, buflen, &ccerr, ts);
```

### TLS Alert

```c
// lib/ngtcp2_conn.c:12677-12682
void ngtcp2_ccerr_set_tls_alert(ngtcp2_ccerr *ccerr, uint8_t tls_alert,
                                const uint8_t *reason, size_t reasonlen) {
  ccerr_init(ccerr, NGTCP2_CCERR_TYPE_TRANSPORT,
             NGTCP2_CRYPTO_ERROR | tls_alert, reason, reasonlen);
}

// Usage: TLS handshake failure
ngtcp2_ccerr ccerr;
ngtcp2_ccerr_set_tls_alert(&ccerr, SSL_AD_CERTIFICATE_EXPIRED, NULL, 0);
```

---

## 6.3 Graceful vs Abrupt Shutdown

### Graceful Shutdown (CLOSING State)

```
Application                    ngtcp2                        Peer
    │                           │                              │
    │ write_connection_close()  │                              │
    │─────────────────────────▶│                              │
    │                           │ CONNECTION_CLOSE             │
    │                           │─────────────────────────────▶│
    │                           │                              │
    │                           │ state = CLOSING              │
    │                           │                              │
    │                           │ (wait for packets)           │
    │                           │◀─────────────────────────────│
    │                           │ Respond with same close      │
    │                           │─────────────────────────────▶│
    │                           │                              │
    │          (timeout)        │                              │
    │◀─────────────────────────│                              │
    │                           │                              │
    │     conn_del()            │                              │
    │─────────────────────────▶│                              │
```

### Code Implementation

```c
// lib/ngtcp2_conn.c:12691-12712
ngtcp2_ssize ngtcp2_conn_write_connection_close_versioned(
  ngtcp2_conn *conn, ngtcp2_path *path, int pkt_info_version,
  ngtcp2_pkt_info *pi, uint8_t *dest, size_t destlen, const ngtcp2_ccerr *ccerr,
  ngtcp2_tstamp ts) {
  
  // Write the CONNECTION_CLOSE packet
  nwrite = conn_write_connection_close(conn, pi, dest, destlen, pkt_type,
                                       ccerr->error_code, ccerr->reason,
                                       ccerr->reasonlen, ts);
  
  // Transition to CLOSING state
  conn->state = NGTCP2_CS_CLOSING;  // lib/ngtcp2_conn.c:12538
  
  return nwrite;
}
```

### Abrupt Shutdown (DRAINING State)

```
Peer                           ngtcp2                     Application
  │                              │                             │
  │ CONNECTION_CLOSE             │                             │
  │─────────────────────────────▶│                             │
  │                              │ state = DRAINING            │
  │                              │                             │
  │                              │ NGTCP2_ERR_DRAINING         │
  │                              │────────────────────────────▶│
  │                              │                             │
  │    (no transmission)         │                             │
  │                              │                             │
  │          (timeout)           │                             │
  │                              │────────────────────────────▶│
  │                              │                             │
  │                              │           conn_del()        │
  │                              │◀────────────────────────────│
```

### Code Implementation

```c
// lib/ngtcp2_conn.c:5952-5982
static int conn_recv_connection_close(ngtcp2_conn *conn,
                                      ngtcp2_connection_close *fr) {
  ngtcp2_ccerr *ccerr = &conn->rx.ccerr;
  
  // Immediate transition to DRAINING
  conn->state = NGTCP2_CS_DRAINING;
  
  // Store error information for application
  if (fr->type == NGTCP2_FRAME_CONNECTION_CLOSE) {
    ccerr->type = NGTCP2_CCERR_TYPE_TRANSPORT;
  } else {
    ccerr->type = NGTCP2_CCERR_TYPE_APPLICATION;
  }
  ccerr->error_code = fr->error_code;
  ccerr->frame_type = fr->frame_type;
  
  return 0;
}
```

---

## 6.4 Why QUIC Has Explicit Draining/Closing States

### CLOSING State Purpose

1. **Retransmit Close**: If peer sends more data, we resend CONNECTION_CLOSE
2. **Graceful Termination**: Give peer time to receive close
3. **Prevent State Confusion**: New connections on same 5-tuple won't be confused

```c
// CLOSING behavior: Still respond to incoming packets
case NGTCP2_CS_CLOSING:
  return NGTCP2_ERR_CLOSING;  // Tell application we're closing
  // Application should retransmit CONNECTION_CLOSE on each incoming packet
```

### DRAINING State Purpose

1. **No Transmission**: We must not send any packets
2. **Wait Period**: Must wait before reusing connection resources
3. **Prevent Packet Confusion**: Late-arriving packets won't cause issues

```c
// DRAINING behavior: Completely passive
case NGTCP2_CS_DRAINING:
  return NGTCP2_ERR_DRAINING;  // Tell application we received close
  // Application must not send anything, just wait
```

### Timing Requirements

```
RFC 9000 Section 10.2:
- CLOSING: Wait at least 3 × PTO before cleanup
- DRAINING: Wait at least 3 × PTO before cleanup

                    CONNECTION_CLOSE sent
                           │
                           ▼
            ┌──────────────────────────────┐
            │          CLOSING             │
            │                              │
            │  Duration: 3 × PTO           │
            │  Action: Respond to packets  │
            │          with same close     │
            └──────────────┬───────────────┘
                           │ timeout
                           ▼
            ┌──────────────────────────────┐
            │          CLEANUP             │
            │                              │
            │  ngtcp2_conn_del()           │
            └──────────────────────────────┘

                    CONNECTION_CLOSE received
                           │
                           ▼
            ┌──────────────────────────────┐
            │          DRAINING            │
            │                              │
            │  Duration: 3 × PTO           │
            │  Action: None (passive)      │
            └──────────────┬───────────────┘
                           │ timeout
                           ▼
            ┌──────────────────────────────┐
            │          CLEANUP             │
            │                              │
            │  ngtcp2_conn_del()           │
            └──────────────────────────────┘
```

---

## 6.5 Connection Cleanup

### Cleanup Function

```c
// lib/ngtcp2_conn.c:1641-1750
void ngtcp2_conn_del(ngtcp2_conn *conn) {
  if (conn == NULL) {
    return;
  }
  
  // Free all streams
  ngtcp2_map_each_free(&conn->strms, delete_strms_each, 
                       (void *)conn->mem);
  
  // Free packet number spaces
  pktns_del(conn->in_pktns, conn->mem);
  pktns_del(conn->hs_pktns, conn->mem);
  pktns_free(&conn->pktns, conn->mem);
  
  // Free crypto contexts
  conn_vneg_crypto_free(conn);
  
  // Free path validation context
  ngtcp2_pv_del(conn->pv);
  
  // Free PMTUD context
  ngtcp2_pmtud_del(conn->pmtud);
  
  // Free connection IDs
  delete_scid(&conn->scid.set, conn->mem);
  
  // Free transport params
  ngtcp2_mem_free(conn->mem, conn->remote.transport_params);
  ngtcp2_mem_free(conn->mem, conn->remote.pending_transport_params);
  
  // Free buffers
  ngtcp2_mem_free(conn->mem, conn->crypto.decrypt_hp_buf.base);
  ngtcp2_mem_free(conn->mem, conn->crypto.decrypt_buf.base);
  
  // Finally, free connection structure
  ngtcp2_mem_free(conn->mem, conn);
}
```

---

## 6.6 Error State Diagram

```
                    ┌─────────────────────────────────┐
                    │         ACTIVE STATES           │
                    │ (CLIENT_*, SERVER_*, POST_*)    │
                    └─────────────────┬───────────────┘
                                      │
           ┌──────────────────────────┼──────────────────────────┐
           │                          │                          │
           │ Application close        │ Receive CONNECTION_CLOSE │
           │ (write_connection_close) │ (from peer)              │
           ▼                          │                          ▼
┌────────────────────┐                │         ┌────────────────────┐
│      CLOSING       │                │         │      DRAINING      │
│                    │                │         │                    │
│ - Send close       │                │         │ - No transmission  │
│ - Resend on recv   │                │         │ - Wait passively   │
│ - Wait 3×PTO       │                │         │ - Wait 3×PTO       │
│                    │                │         │                    │
│ Return:            │                │         │ Return:            │
│ NGTCP2_ERR_CLOSING │                │         │ NGTCP2_ERR_DRAINING│
└────────┬───────────┘                │         └──────────┬─────────┘
         │                            │                    │
         │ Timeout                    │                    │ Timeout
         │                            │                    │
         ▼                            │                    ▼
┌──────────────────────────────────────────────────────────────────┐
│                         DELETED                                   │
│                                                                   │
│  ngtcp2_conn_del() - All resources freed                         │
└──────────────────────────────────────────────────────────────────┘

                Additional Error Paths:

┌─────────────────────────────────────────────────────────────────┐
│ Idle Timeout (ngtcp2_conn_get_idle_expiry <= ts)                 │
│   → Returns NGTCP2_ERR_IDLE_CLOSE                                │
│   → Application should delete connection                         │
├─────────────────────────────────────────────────────────────────┤
│ Handshake Timeout (conn_handshake_expiry <= ts)                  │
│   → Returns NGTCP2_ERR_HANDSHAKE_TIMEOUT                         │
│   → Application may send close or just delete                    │
├─────────────────────────────────────────────────────────────────┤
│ Fatal Error (NGTCP2_ERR_CRYPTO, NGTCP2_ERR_PROTO, etc.)          │
│   → Application should send close with error code                │
│   → Then transition to CLOSING                                   │
└─────────────────────────────────────────────────────────────────┘
```

---

## 6.7 Terminal State Checking

```c
// Check if connection is in terminal state
int ngtcp2_conn_in_closing_period(ngtcp2_conn *conn) {
  return conn->state == NGTCP2_CS_CLOSING;
}

int ngtcp2_conn_in_draining_period(ngtcp2_conn *conn) {
  return conn->state == NGTCP2_CS_DRAINING;
}

// Usage pattern
if (ngtcp2_conn_in_closing_period(conn) || 
    ngtcp2_conn_in_draining_period(conn)) {
  // Connection is terminating, schedule cleanup
  schedule_cleanup(conn, 3 * get_pto(conn));
}
```

---

## 中文解释 (Chinese Explanation)

### 错误分类

ngtcp2 将错误分为几类：
- **致命错误**: 连接必须关闭 (NOMEM, CALLBACK_FAILURE, CRYPTO, PROTO)
- **非致命错误**: 连接可以继续 (INVALID_ARGUMENT, STREAM_NOT_FOUND)
- **终态错误**: 连接正在结束 (CLOSING, DRAINING, IDLE_CLOSE)

### 优雅关闭 vs 突然关闭

**CLOSING 状态（发起方）**：
- 发送 CONNECTION_CLOSE
- 如果收到更多数据包，重发关闭帧
- 等待 3×PTO 后清理

**DRAINING 状态（接收方）**：
- 收到 CONNECTION_CLOSE
- 不发送任何数据包
- 等待 3×PTO 后清理

### 为什么需要显式的 DRAINING/CLOSING 状态

1. **重传关闭帧**: 确保对端收到关闭通知
2. **优雅终止**: 给对端时间处理关闭
3. **防止状态混淆**: 同一5元组的新连接不会被混淆
4. **等待期**: 延迟到达的数据包不会造成问题

### 清理流程

```
CLOSING/DRAINING → 等待 3×PTO → ngtcp2_conn_del() → 资源释放
```
