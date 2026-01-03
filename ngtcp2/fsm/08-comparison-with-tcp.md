# Section 8: Comparison with TCP FSM

## 8.1 TCP FSM Overview

### Classic TCP State Machine

```
                              ┌───────────────────┐
                              │      CLOSED       │
                              └─────────┬─────────┘
                                        │
           ┌────────────────────────────┼────────────────────────────┐
           │ passive open               │ active open                │
           ▼                            ▼                            │
    ┌─────────────┐           ┌─────────────────┐                   │
    │   LISTEN    │           │   SYN_SENT      │                   │
    └──────┬──────┘           └────────┬────────┘                   │
           │ recv SYN                  │ recv SYN+ACK               │
           │ send SYN+ACK              │ send ACK                   │
           ▼                           ▼                            │
    ┌─────────────────┐       ┌─────────────────┐                   │
    │  SYN_RECEIVED   │       │   ESTABLISHED   │◀──────────────────┘
    └────────┬────────┘       └────────┬────────┘
             │ recv ACK                │
             │                         │
             ▼                         │
    ┌─────────────────┐               │
    │   ESTABLISHED   │               │
    └─────────────────┘               │
             │                        │
             │ close                  │ close
             ▼                        ▼
    ┌─────────────────┐       ┌─────────────────┐
    │   FIN_WAIT_1    │       │  CLOSE_WAIT     │
    └────────┬────────┘       └────────┬────────┘
             │                         │
             ▼                         ▼
    ┌─────────────────┐       ┌─────────────────┐
    │   FIN_WAIT_2    │       │   LAST_ACK      │
    └────────┬────────┘       └─────────────────┘
             │
             ▼
    ┌─────────────────┐
    │    TIME_WAIT    │
    └────────┬────────┘
             │ 2MSL timeout
             ▼
    ┌─────────────────┐
    │     CLOSED      │
    └─────────────────┘
```

### TCP Characteristics

- **11 explicit states** (including CLOSED)
- **Kernel-managed**: All state in kernel, timer handling in kernel
- **Single purpose**: Just connection/data/close
- **Sequential establishment**: SYN → SYN+ACK → ACK
- **Simple teardown**: FIN → FIN+ACK or simultaneous close

---

## 8.2 QUIC (ngtcp2) FSM Overview

```
┌────────────────────────────────────────────────────────────────────┐
│                         QUIC FSM LAYERS                             │
├────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  Layer 1: Primary States (7)                                        │
│  ┌────────────────────────────────────────────────────────────┐    │
│  │ CLIENT_INITIAL → CLIENT_WAIT → POST_HANDSHAKE → CLOSING    │    │
│  │ SERVER_INITIAL → SERVER_WAIT ─────────────────→ DRAINING   │    │
│  └────────────────────────────────────────────────────────────┘    │
│                                                                     │
│  Layer 2: Flags (18+)                                               │
│  ┌────────────────────────────────────────────────────────────┐    │
│  │ TLS_HANDSHAKE_COMPLETED | HANDSHAKE_CONFIRMED | RECV_RETRY │    │
│  │ EARLY_DATA_REJECTED | KEY_UPDATE_NOT_CONFIRMED | ...       │    │
│  └────────────────────────────────────────────────────────────┘    │
│                                                                     │
│  Layer 3: Sub-FSMs                                                  │
│  ┌────────────────────────────────────────────────────────────┐    │
│  │ Path Validation | ECN Validation | Key Update | PMTUD      │    │
│  └────────────────────────────────────────────────────────────┘    │
│                                                                     │
│  Layer 4: Per-PktNS State                                           │
│  ┌────────────────────────────────────────────────────────────┐    │
│  │ Initial PktNS | Handshake PktNS | Application PktNS        │    │
│  └────────────────────────────────────────────────────────────┘    │
│                                                                     │
└────────────────────────────────────────────────────────────────────┘
```

---

## 8.3 Explicitness Comparison

### TCP: Implicit State

```c
// TCP state is opaque to application
// Kernel manages everything internally

// Application sees only:
int fd = socket(AF_INET, SOCK_STREAM, 0);
connect(fd, &addr, sizeof(addr));  // Blocking, state changes internally
// State transitions happen inside kernel, invisible to app
```

### QUIC (ngtcp2): Explicit State

```c
// Application has full visibility and control

// Check current state
switch (conn->state) {
case NGTCP2_CS_CLIENT_WAIT_HANDSHAKE:
  // Can inspect flags, timers, everything
  if (conn->flags & NGTCP2_CONN_FLAG_HANDSHAKE_COMPLETED) {
    // Partial handshake state visible
  }
  break;
}

// Timer management is explicit
ngtcp2_tstamp expiry = ngtcp2_conn_get_expiry(conn);
// Application schedules timer, drives state machine
```

### Comparison Table

| Aspect | TCP | QUIC (ngtcp2) |
|--------|-----|---------------|
| State visibility | Kernel-internal | Fully exposed |
| Timer management | Kernel-handled | Application-driven |
| Error details | Limited (errno) | Rich error types |
| Transition control | Automatic | Application-triggered |
| Debugging | Hard (need kernel traces) | Easy (library state) |

---

## 8.4 Extensibility Comparison

### TCP: Difficult to Extend

```
Problems with extending TCP:
1. Kernel implementation required
2. Middlebox ossification
3. Option space limited (40 bytes)
4. Backward compatibility constraints
5. Years to deploy changes

Example: TCP Fast Open
- Proposed: 2011
- RFC: 2014
- Wide deployment: 2016+
- Still not universal in 2024
```

### QUIC (ngtcp2): Designed for Extension

```
QUIC extension mechanisms:
1. User-space, easy to update
2. Encrypted, middlebox-proof
3. Transport parameters (unbounded)
4. Version negotiation built-in
5. Frame type extensibility

Example: Adding new feature
// Just add new flag
#define NGTCP2_CONN_FLAG_NEW_FEATURE 0x80000u

// Add new sub-FSM if needed
struct ngtcp2_new_feature {
  // ...
};

// Integrate at appropriate layer
if (conn->flags & NGTCP2_CONN_FLAG_NEW_FEATURE) {
  handle_new_feature(conn);
}
```

### Comparison Table

| Aspect | TCP | QUIC (ngtcp2) |
|--------|-----|---------------|
| Adding state | Kernel change | Add flag/struct |
| New timers | Kernel change | Add to expiry check |
| New frames | N/A (fixed) | Add frame handler |
| Deployment | OS upgrade | App update |
| Testing | Complex | Unit testable |

---

## 8.5 Maintainability Comparison

### TCP: Monolithic Kernel Code

```c
// Typical TCP implementation structure (simplified)
// One giant state machine handling everything

void tcp_rcv_state_process(struct sock *sk, struct sk_buff *skb) {
  switch (sk->sk_state) {
  case TCP_CLOSE:
    // Handle CLOSE state
    break;
  case TCP_LISTEN:
    // Handle LISTEN state (hundreds of lines)
    break;
  case TCP_SYN_SENT:
    // Handle SYN_SENT (hundreds of lines)
    break;
  // ... 8 more states, each with extensive handling
  }
}
// Result: Functions with 1000+ lines
```

### QUIC (ngtcp2): Modular Design

```c
// Layered, modular structure

// Entry point - just routing
int ngtcp2_conn_read_pkt_versioned(...) {
  switch (conn->state) {
  case NGTCP2_CS_CLIENT_INITIAL:
  case NGTCP2_CS_CLIENT_WAIT_HANDSHAKE:
    return conn_read_handshake();  // Delegate
  // ...
  }
}

// Specialized handlers
static ngtcp2_ssize conn_read_handshake(...) {
  // Focused on handshake
}

static ngtcp2_ssize conn_recv_pkt(...) {
  // Focused on 1-RTT packets
}

// Sub-FSMs are separate modules
// ngtcp2_pv.c - Path validation
// ngtcp2_pmtud.c - PMTUD
// etc.
```

### Comparison Table

| Aspect | TCP | QUIC (ngtcp2) |
|--------|-----|---------------|
| Code organization | Monolithic | Modular |
| Function size | 1000+ lines | 100-300 lines |
| Testability | System tests | Unit tests |
| Debugging | Kernel debugging | Application debugging |
| Documentation | RFCs + kernel docs | RFCs + inline docs |

---

## 8.6 Error Handling Comparison

### TCP: Limited Error Information

```c
// TCP error handling is opaque
int ret = connect(fd, &addr, sizeof(addr));
if (ret < 0) {
  // Only get errno
  switch (errno) {
  case ECONNREFUSED:
    // Peer sent RST
    break;
  case ETIMEDOUT:
    // Connection timed out
    break;
  case ENETUNREACH:
    // Network unreachable
    break;
  // Limited to standard errnos
  }
}
// No access to:
// - Specific state that failed
// - Number of retries
// - RTT measurements
// - Detailed reason
```

### QUIC (ngtcp2): Rich Error Information

```c
// QUIC provides detailed error information
int rv = ngtcp2_conn_read_pkt(conn, path, pi, buf, len, ts);
if (rv < 0) {
  switch (rv) {
  case NGTCP2_ERR_DRAINING:
    // Peer closed connection
    // Can inspect ccerr for details
    printf("Error code: %lu\n", conn->rx.ccerr.error_code);
    printf("Reason: %.*s\n", (int)conn->rx.ccerr.reasonlen,
           conn->rx.ccerr.reason);
    break;
    
  case NGTCP2_ERR_CRYPTO:
    // TLS error
    printf("TLS alert: %d\n", ngtcp2_conn_get_tls_alert(conn));
    break;
    
  case NGTCP2_ERR_HANDSHAKE_TIMEOUT:
    // Can check how far handshake got
    if (conn->flags & NGTCP2_CONN_FLAG_TLS_HANDSHAKE_COMPLETED) {
      printf("TLS done but QUIC handshake didn't complete\n");
    }
    break;
  }
}
```

### Error Type Comparison

| TCP errno | QUIC (ngtcp2) equivalent | Additional info in QUIC |
|-----------|-------------------------|-------------------------|
| ECONNREFUSED | NGTCP2_ERR_DRAINING | Error code, reason phrase |
| ETIMEDOUT | NGTCP2_ERR_IDLE_CLOSE | Timeout duration, state |
| ECONNRESET | NGTCP2_ERR_DRAINING | Error code, frame type |
| N/A | NGTCP2_ERR_CRYPTO | TLS alert, error detail |
| N/A | NGTCP2_ERR_PROTO | Frame type, reason |

---

## 8.7 Why QUIC FSM Evolved Differently

### Historical Context

```
1980s: TCP designed
├── Network: Simple, trusted
├── Middleboxes: Non-existent
├── Encryption: Optional (separate layer)
├── Implementation: Kernel only
└── Evolution: Slow (RFC process + kernel updates)

2010s: QUIC designed
├── Network: Complex, untrusted middleboxes
├── Middlebox ossification: Major problem
├── Encryption: Mandatory (integrated)
├── Implementation: User-space first
└── Evolution: Fast (library updates)
```

### Design Philosophy Differences

```
TCP Philosophy:
"The kernel knows best"
- Hide complexity from applications
- Automatic recovery
- One-size-fits-all behavior

QUIC Philosophy:
"Give applications control"
- Expose state for application decisions
- Application-driven timers
- Flexible per-use-case tuning
```

### Resulting Differences

```
┌──────────────────┬────────────────────┬────────────────────────┐
│ Design Choice    │ TCP                │ QUIC (ngtcp2)          │
├──────────────────┼────────────────────┼────────────────────────┤
│ Implementation   │ Kernel             │ User-space library     │
│ State exposure   │ Hidden             │ Explicit               │
│ Timer handling   │ Internal           │ External (app-driven)  │
│ Encryption       │ Separate (TLS)     │ Integrated             │
│ Extensibility    │ Ossified           │ Designed for extension │
│ Error detail     │ Minimal (errno)    │ Rich (codes + reasons) │
│ Testing          │ System tests       │ Unit tests             │
│ Evolution        │ Years              │ Weeks                  │
└──────────────────┴────────────────────┴────────────────────────┘
```

---

## 8.8 Summary Comparison

```
┌─────────────────────────────────────────────────────────────────┐
│                    TCP vs QUIC FSM SUMMARY                       │
├───────────────────────┬─────────────────┬───────────────────────┤
│ Characteristic        │ TCP             │ QUIC (ngtcp2)         │
├───────────────────────┼─────────────────┼───────────────────────┤
│ Number of states      │ 11 (single)     │ 7 + 18 flags + subs   │
│ State location        │ Kernel          │ User-space struct     │
│ State visibility      │ Opaque          │ Transparent           │
│ Timer management      │ Kernel          │ Application           │
│ Encryption state      │ Separate        │ Integrated            │
│ Error reporting       │ errno           │ Rich error types      │
│ Extensibility         │ Low             │ High                  │
│ Maintainability       │ Hard            │ Modular               │
│ Testability           │ System tests    │ Unit tests            │
│ Deployment cycle      │ Months/Years    │ Days/Weeks            │
│ Middlebox resistance  │ Vulnerable      │ Encrypted             │
│ Multi-path support    │ Limited         │ Native                │
└───────────────────────┴─────────────────┴───────────────────────┘
```

---

## 中文解释 (Chinese Explanation)

### TCP 与 QUIC FSM 的主要区别

1. **显式性**
   - TCP: 状态对应用程序不可见，由内核管理
   - QUIC: 完全暴露状态，应用程序可检查所有细节

2. **可扩展性**
   - TCP: 扩展困难（需要内核更改，中间盒骨化）
   - QUIC: 设计上支持扩展（用户空间，加密防中间盒）

3. **可维护性**
   - TCP: 单片内核代码（函数可达1000+行）
   - QUIC: 模块化设计（函数100-300行）

4. **错误处理**
   - TCP: 有限信息（仅 errno）
   - QUIC: 丰富错误信息（错误码、原因短语、状态详情）

### 为什么 QUIC FSM 不同

历史背景：
- TCP 设计于1980年代，网络简单可信
- QUIC 设计于2010年代，面对复杂、不可信的网络

设计哲学：
- TCP: "内核最懂"—隐藏复杂性
- QUIC: "给应用程序控制权"—暴露状态

结果：
- QUIC 更易测试、扩展、维护
- 部署周期从数月/年降至数天/周
