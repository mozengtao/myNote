# Section 2: State Modeling

## 2.1 Where QUIC Connection State is Stored

### Primary State Location

The connection state in ngtcp2 is stored in the `ngtcp2_conn` structure:

```c
// lib/ngtcp2_conn.h:318-654
struct ngtcp2_conn {
  ngtcp2_objalloc frc_objalloc;
  ngtcp2_objalloc rtb_entry_objalloc;
  ngtcp2_objalloc strm_objalloc;
  ngtcp2_conn_state state;           // PRIMARY FSM STATE
  ngtcp2_callbacks callbacks;
  
  // Connection IDs
  ngtcp2_cid rcid;                   // Remote CID
  ngtcp2_cid oscid;                  // Original Source CID
  ngtcp2_cid retry_scid;             // Retry Source CID
  
  // Packet number spaces
  ngtcp2_pktns *in_pktns;            // Initial packet number space
  ngtcp2_pktns *hs_pktns;            // Handshake packet number space
  ngtcp2_pktns pktns;                // Application data packet number space
  
  // ... many more fields
  
  uint32_t flags;                    // AUGMENTING FLAGS
  int server;                        // Role flag
};
```

### State Components Breakdown

```
ngtcp2_conn
    │
    ├── state (ngtcp2_conn_state)     ← Primary FSM state
    │
    ├── flags (uint32_t)              ← Augmenting flags (18+)
    │
    ├── Packet Number Spaces
    │   ├── in_pktns (Initial)        ← Each has own state
    │   ├── hs_pktns (Handshake)
    │   └── pktns (Application)
    │
    ├── Sub-FSM Objects
    │   ├── pv (ngtcp2_pv*)           ← Path Validation FSM
    │   ├── pmtud (ngtcp2_pmtud*)     ← PMTUD FSM
    │   └── tx.ecn.state              ← ECN Validation FSM
    │
    ├── Crypto State
    │   ├── crypto.key_update         ← Key rotation state
    │   └── early.ckm                 ← 0-RTT key state
    │
    └── Timer State
        ├── idle_ts
        ├── keep_alive.last_ts
        └── cstat.loss_detection_timer
```

---

## 2.2 How States are Encoded

### Primary State: Enumeration

```c
// lib/ngtcp2_conn.h:58-69
typedef enum {
  NGTCP2_CS_CLIENT_INITIAL,
  NGTCP2_CS_CLIENT_WAIT_HANDSHAKE,
  NGTCP2_CS_SERVER_INITIAL,
  NGTCP2_CS_SERVER_WAIT_HANDSHAKE,
  NGTCP2_CS_POST_HANDSHAKE,
  NGTCP2_CS_CLOSING,
  NGTCP2_CS_DRAINING,
} ngtcp2_conn_state;
```

### Augmenting Flags: Bitmask

```c
// lib/ngtcp2_conn.h:124-195
#define NGTCP2_CONN_FLAG_NONE                          0x00u
#define NGTCP2_CONN_FLAG_TLS_HANDSHAKE_COMPLETED       0x01u
#define NGTCP2_CONN_FLAG_INITIAL_PKT_PROCESSED         0x02u
#define NGTCP2_CONN_FLAG_TRANSPORT_PARAM_RECVED        0x04u
#define NGTCP2_CONN_FLAG_LOCAL_TRANSPORT_PARAMS_COMMITTED 0x08u
#define NGTCP2_CONN_FLAG_RECV_RETRY                    0x10u
#define NGTCP2_CONN_FLAG_EARLY_DATA_REJECTED           0x20u
#define NGTCP2_CONN_FLAG_KEEP_ALIVE_CANCELLED          0x40u
#define NGTCP2_CONN_FLAG_HANDSHAKE_CONFIRMED           0x80u
#define NGTCP2_CONN_FLAG_HANDSHAKE_COMPLETED           0x0100u
#define NGTCP2_CONN_FLAG_HANDSHAKE_EARLY_RETRANSMIT    0x0200u
#define NGTCP2_CONN_FLAG_CLEAR_FIXED_BIT               0x0400u
#define NGTCP2_CONN_FLAG_KEY_UPDATE_NOT_CONFIRMED      0x0800u
#define NGTCP2_CONN_FLAG_PPE_PENDING                   0x1000u
#define NGTCP2_CONN_FLAG_RESTART_IDLE_TIMER_ON_WRITE   0x2000u
#define NGTCP2_CONN_FLAG_SERVER_ADDR_VERIFIED          0x4000u
#define NGTCP2_CONN_FLAG_EARLY_KEY_INSTALLED           0x8000u
#define NGTCP2_CONN_FLAG_KEY_UPDATE_INITIATOR          0x10000u
#define NGTCP2_CONN_FLAG_AGGREGATE_PKTS                0x20000u
#define NGTCP2_CONN_FLAG_CRUMBLE_INITIAL_CRYPTO        0x40000u
```

### Sub-FSM: ECN State

```c
// lib/ngtcp2_conn.h:291-296
typedef enum ngtcp2_ecn_state {
  NGTCP2_ECN_STATE_TESTING,    // Initial probing
  NGTCP2_ECN_STATE_UNKNOWN,    // Insufficient data
  NGTCP2_ECN_STATE_FAILED,     // ECN bleached or broken
  NGTCP2_ECN_STATE_CAPABLE,    // ECN works
} ngtcp2_ecn_state;
```

### Sub-FSM: Packet Number Space

```c
// lib/ngtcp2_pktns_id.h:39-60
typedef enum ngtcp2_pktns_id {
  NGTCP2_PKTNS_ID_INITIAL,     // Initial packet space
  NGTCP2_PKTNS_ID_HANDSHAKE,   // Handshake packet space  
  NGTCP2_PKTNS_ID_APPLICATION, // 1-RTT packet space
  NGTCP2_PKTNS_ID_MAX
} ngtcp2_pktns_id;
```

---

## 2.3 Why QUIC Uses Multiple Layered State Variables

### Problem: State Explosion

If we tried to encode all state combinations in a single enum:

```
7 primary states × 18 flags × 3 pktns states × 4 ECN states × ...
= Millions of potential states!
```

### Solution: Layered State Model

ngtcp2 uses a hierarchical approach:

```
Layer 1: Coarse-grained lifecycle (ngtcp2_conn_state)
    - 7 states
    - Mutually exclusive
    - Drives high-level behavior

Layer 2: Fine-grained modifiers (flags)
    - 18+ independent flags
    - Can be combined freely
    - Modify behavior within a state

Layer 3: Independent sub-systems (sub-FSMs)
    - Path Validation: Own lifecycle
    - ECN: Own lifecycle
    - Key Update: Own lifecycle
    - Each can evolve independently
```

### Benefits of Layered Approach

1. **Reduced Complexity**: Each layer has limited states
2. **Independent Evolution**: Sub-FSMs can change without affecting others
3. **Testability**: Each layer can be tested in isolation
4. **Maintainability**: Changes are localized

---

## 2.4 How FSM Complexity is Controlled

### Pattern 1: Guard Conditions

```c
// Check multiple conditions before transition
static int conn_is_tls_handshake_completed(ngtcp2_conn *conn) {
  return (conn->flags & NGTCP2_CONN_FLAG_TLS_HANDSHAKE_COMPLETED) &&
         (conn->flags & NGTCP2_CONN_FLAG_HANDSHAKE_COMPLETED);
}
```

### Pattern 2: State + Flag Combinations

```c
// State check with flag modifier
if (conn->state == NGTCP2_CS_POST_HANDSHAKE &&
    (conn->flags & NGTCP2_CONN_FLAG_HANDSHAKE_CONFIRMED)) {
  // Only perform action when both conditions met
}
```

### Pattern 3: Hierarchical State Machines

```c
// Main FSM delegates to sub-FSM
switch (conn->state) {
case NGTCP2_CS_POST_HANDSHAKE:
  if (conn->pv) {
    // Path validation sub-FSM handles this
    rv = handle_path_validation(conn);
  }
  break;
}
```

### Pattern 4: Lazy State Cleanup

```c
// Packet number spaces are discarded lazily
void ngtcp2_conn_discard_initial_state(ngtcp2_conn *conn, ngtcp2_tstamp ts) {
  if (!conn->in_pktns) {
    return;  // Already discarded
  }
  conn_discard_pktns(conn, &conn->in_pktns, ts);
}
```

---

## 2.5 State Encoding Visualization

```
┌─────────────────────────────────────────────────────────────────┐
│                        ngtcp2_conn                               │
├─────────────────────────────────────────────────────────────────┤
│  ┌───────────────────────────────────────────────────────────┐  │
│  │ Primary State: ngtcp2_conn_state                          │  │
│  │ ┌─────────────┬─────────────┬─────────────┬────────────┐  │  │
│  │ │ CLIENT_INIT │ CLIENT_WAIT │ POST_HANDSH │ CLOSING    │  │  │
│  │ └─────────────┴─────────────┴─────────────┴────────────┘  │  │
│  └───────────────────────────────────────────────────────────┘  │
│                                                                  │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │ Flags: uint32_t (bitmask)                                 │  │
│  │ ┌────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬───┐  │  │
│  │ │TLS │INIT│TPAR│LPAR│RTRY│EREJ│KEEP│HCON│HCOM│ERTX│...│  │  │
│  │ │0x01│0x02│0x04│0x08│0x10│0x20│0x40│0x80│0100│0200│...│  │  │
│  │ └────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴───┘  │  │
│  └───────────────────────────────────────────────────────────┘  │
│                                                                  │
│  ┌─────────────────────────────────────────────────────────────┐│
│  │ Sub-FSMs                                                     ││
│  │ ┌─────────────┐ ┌─────────────┐ ┌─────────────────────────┐ ││
│  │ │ ECN State   │ │ PV State    │ │ Key Update State        │ ││
│  │ │ ┌─────────┐ │ │ ┌─────────┐ │ │ ┌─────────────────────┐ │ ││
│  │ │ │TESTING  │ │ │ │VALIDAT. │ │ │ │NOT_CONFIRMED | INIT │ │ ││
│  │ │ │UNKNOWN  │ │ │ │FALLBACK │ │ │ └─────────────────────┘ │ ││
│  │ │ │CAPABLE  │ │ │ │TIMEOUT  │ │ │                         │ ││
│  │ │ │FAILED   │ │ │ └─────────┘ │ │                         │ ││
│  │ │ └─────────┘ │ └─────────────┘ └─────────────────────────┘ ││
│  │ └─────────────┘                                              ││
│  └─────────────────────────────────────────────────────────────┘│
│                                                                  │
│  ┌─────────────────────────────────────────────────────────────┐│
│  │ Packet Number Spaces (each with own state)                   ││
│  │ ┌─────────────┐ ┌─────────────┐ ┌─────────────────────────┐ ││
│  │ │ in_pktns    │ │ hs_pktns    │ │ pktns (Application)     │ ││
│  │ │ (Initial)   │ │ (Handshake) │ │                         │ ││
│  │ └─────────────┘ └─────────────┘ └─────────────────────────┘ ││
│  └─────────────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────────────┘
```

---

## 中文解释 (Chinese Explanation)

### 状态存储位置

ngtcp2 中的连接状态主要存储在 `ngtcp2_conn` 结构体中，包括：

1. **主状态** (`state`): 使用枚举类型，7个状态
2. **辅助标志** (`flags`): 使用位掩码，18+个独立标志
3. **子状态机**: 如路径验证、ECN验证等

### 为什么使用分层状态模型？

如果把所有状态组合编码到一个枚举中：
- 7个主状态 × 18个标志 × 3个包空间状态 × 4个ECN状态 = 数百万种可能状态！

分层方案的好处：
- **降低复杂度**: 每层状态数量有限
- **独立演进**: 子状态机可以独立变化
- **可测试性**: 每层可以独立测试
- **可维护性**: 变更局部化

### 复杂度控制策略

1. **守卫条件**: 多条件组合检查
2. **状态+标志组合**: 主状态与标志配合使用
3. **层次化状态机**: 主FSM委托给子FSM处理
4. **延迟状态清理**: 资源在需要时才清理
