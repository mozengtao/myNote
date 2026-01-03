# Section 1: FSM Role in QUIC

## 1.1 Why QUIC Requires a Complex FSM

### The Challenge of User-Space Protocol Implementation

QUIC, unlike TCP, runs entirely in user-space. This fundamental architectural choice creates several requirements that demand a sophisticated FSM:

1. **No Kernel State Management**: TCP benefits from kernel-level connection tracking. QUIC must explicitly manage all state transitions.

2. **Multiple Encryption Levels**: QUIC has three encryption levels (Initial, Handshake, Application) that must be managed simultaneously during the handshake.

3. **Integrated TLS**: The TLS 1.3 handshake is tightly woven into QUIC, requiring coordination between cryptographic state and transport state.

4. **Connection Migration**: QUIC supports connection migration to new network paths, requiring sophisticated path validation state machines.

5. **0-RTT Data**: Early data transmission requires careful state management to handle rejection scenarios.

### ngtcp2's FSM Complexity Sources

From the ngtcp2 codebase, we observe:

```c
// lib/ngtcp2_conn.h:58-69
typedef enum {
  /* Client specific handshake states */
  NGTCP2_CS_CLIENT_INITIAL,
  NGTCP2_CS_CLIENT_WAIT_HANDSHAKE,
  /* Server specific handshake states */
  NGTCP2_CS_SERVER_INITIAL,
  NGTCP2_CS_SERVER_WAIT_HANDSHAKE,
  /* Shared by both client and server */
  NGTCP2_CS_POST_HANDSHAKE,
  NGTCP2_CS_CLOSING,
  NGTCP2_CS_DRAINING,
} ngtcp2_conn_state;
```

This appears simple, but the real complexity emerges from:
- **18+ connection flags** (`NGTCP2_CONN_FLAG_*`) that augment the base state
- **3 packet number spaces** with independent state
- **Sub-FSMs** for path validation, ECN validation, PMTUD, etc.

---

## 1.2 How QUIC FSM Differs from TCP FSM

### TCP FSM: Kernel-Managed Simplicity

```
TCP States (11 total):
CLOSED → LISTEN → SYN_RECEIVED → ESTABLISHED → FIN_WAIT_1 → ...

Characteristics:
- Single linear progression
- Kernel handles timeouts
- No encryption state interleaving
- Connection = single path
```

### QUIC FSM: User-Space Complexity

```
QUIC Layered State Model:

Layer 1: Connection State (ngtcp2_conn_state)
    CLIENT_INITIAL → CLIENT_WAIT_HANDSHAKE → POST_HANDSHAKE → CLOSING/DRAINING

Layer 2: Encryption State (per packet number space)
    Each of: Initial, Handshake, Application has own crypto state

Layer 3: Sub-FSMs
    - Path Validation (ngtcp2_pv)
    - ECN Validation (ngtcp2_ecn_state: TESTING → UNKNOWN/CAPABLE/FAILED)
    - PMTUD (Path MTU Discovery)
    - Key Update cycle

Layer 4: Stream-Level FSMs
    Each stream has its own send/receive state machine
```

### Key Differences Table

| Aspect | TCP | QUIC (ngtcp2) |
|--------|-----|---------------|
| **Implementation** | Kernel | User-space |
| **State Variables** | 1 enum (11 states) | 1 enum + 18 flags + sub-FSMs |
| **Encryption** | External (TLS layer) | Integrated into state machine |
| **Path Management** | Single path | Multi-path with validation FSM |
| **Retransmission** | Per-connection timer | Per-packet-space timers |
| **Graceful Close** | FIN/FIN-ACK sequence | CLOSING + DRAINING phases |

---

## 1.3 Major Lifecycle States

### Primary Connection States

```
    +-------------------+
    |   CLIENT_INITIAL  |  (Client sends first Initial packet)
    +-------------------+
             |
             v
    +-----------------------+
    | CLIENT_WAIT_HANDSHAKE |  (Waiting for server handshake)
    +-----------------------+
             |
             v
    +-------------------+
    |  POST_HANDSHAKE   |  (Normal operation, 1-RTT data)
    +-------------------+
             |
      +------+------+
      |             |
      v             v
+-----------+  +------------+
|  CLOSING  |  |  DRAINING  |
+-----------+  +------------+
      |             |
      +------+------+
             v
         [DELETED]
```

### Server-Side Variations

```
    +-------------------+
    |  SERVER_INITIAL   |  (Received first client Initial)
    +-------------------+
             |
             v
    +-----------------------+
    | SERVER_WAIT_HANDSHAKE |  (Processing handshake)
    +-----------------------+
             |
             v
    +-------------------+
    |  POST_HANDSHAKE   |
    +-------------------+
```

### State Descriptions

| State | Description | Key Activities |
|-------|-------------|----------------|
| `CLIENT_INITIAL` | Client created, no packets sent | Generate Initial packet with CRYPTO |
| `CLIENT_WAIT_HANDSHAKE` | Initial sent, waiting for response | Process server handshake, send 0-RTT |
| `SERVER_INITIAL` | Server received Initial, processing | Validate token, prepare response |
| `SERVER_WAIT_HANDSHAKE` | Server sent response, waiting | Wait for client Finished |
| `POST_HANDSHAKE` | Handshake complete, keys installed | Application data exchange |
| `CLOSING` | Sent CONNECTION_CLOSE, waiting | Retransmit close on incoming packets |
| `DRAINING` | Received CONNECTION_CLOSE | Only wait, no transmission |

---

## 1.4 Understanding Through Code

### State Transition in ngtcp2

```c
// Client transitions to WAIT_HANDSHAKE after first write
// lib/ngtcp2_conn.c:10447
conn->state = NGTCP2_CS_CLIENT_WAIT_HANDSHAKE;

// Client transitions to POST_HANDSHAKE when handshake completes
// lib/ngtcp2_conn.c:10530  
conn->state = NGTCP2_CS_POST_HANDSHAKE;

// Receiving CONNECTION_CLOSE triggers DRAINING
// lib/ngtcp2_conn.c:5956
conn->state = NGTCP2_CS_DRAINING;

// Sending CONNECTION_CLOSE triggers CLOSING
// lib/ngtcp2_conn.c:12538
conn->state = NGTCP2_CS_CLOSING;
```

---

## 中文解释 (Chinese Explanation)

### 为什么 QUIC 需要复杂的状态机？

1. **用户态实现**：QUIC 运行在用户空间，不像 TCP 那样由内核管理状态。所有状态转换必须在应用程序中显式处理。

2. **多层加密**：QUIC 在握手过程中同时管理三个加密级别（Initial、Handshake、Application），每个级别都有独立的密钥和状态。

3. **TLS 集成**：TLS 1.3 握手与 QUIC 紧密耦合，加密状态和传输状态必须协同管理。

4. **连接迁移**：QUIC 支持连接迁移到新的网络路径，需要路径验证子状态机。

5. **0-RTT 数据**：早期数据传输需要谨慎的状态管理来处理拒绝场景。

### QUIC 与 TCP 状态机的主要区别

| 方面 | TCP | QUIC |
|------|-----|------|
| 实现位置 | 内核 | 用户空间 |
| 状态变量 | 单一枚举（11个状态） | 枚举 + 18个标志 + 子状态机 |
| 加密处理 | 外部（TLS层） | 集成到状态机中 |
| 路径管理 | 单路径 | 多路径，带验证状态机 |

### 主要生命周期状态

- **CLIENT_INITIAL / SERVER_INITIAL**：初始状态，准备握手
- **CLIENT_WAIT_HANDSHAKE / SERVER_WAIT_HANDSHAKE**：握手进行中
- **POST_HANDSHAKE**：握手完成，正常数据传输
- **CLOSING**：发送了 CONNECTION_CLOSE，等待确认
- **DRAINING**：收到 CONNECTION_CLOSE，只等待不发送
