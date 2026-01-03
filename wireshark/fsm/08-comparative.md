# Section 8: Comparative Analysis

## 8.1 Wireshark FSMs vs TCP FSM in Linux Kernel

### Linux Kernel TCP FSM

```
+------------------------------------------------------------------+
|           LINUX KERNEL TCP STATE MACHINE (RFC 793)                |
+------------------------------------------------------------------+

                              +---------+
                              |  CLOSED |
                              +---------+
                                   |
          passive OPEN    active OPEN, create TCB
          create TCB      send SYN
                |               |
                v               v
          +---------+     +-----------+
          |  LISTEN |     |  SYN_SENT |
          +---------+     +-----------+
               |                |
        rcv SYN |          rcv SYN,ACK
        send SYN,ACK       send ACK
               v                v
          +-----------+   +-----------+
          |  SYN_RCVD |   |           |
          +-----------+   |           |
               |          |           |
        rcv ACK|          |           |
               v          v           |
          +-------------------+       |
          |    ESTABLISHED    |<------+
          +-------------------+
               |           |
        CLOSE  |           | rcv FIN
        send FIN           | send ACK
               v           v
          +-----------+  +-----------+
          | FIN_WAIT_1|  |CLOSE_WAIT |
          +-----------+  +-----------+
```

### Linux Kernel Implementation

```c
// From Linux kernel: include/net/tcp_states.h
enum {
    TCP_ESTABLISHED = 1,
    TCP_SYN_SENT,
    TCP_SYN_RECV,
    TCP_FIN_WAIT1,
    TCP_FIN_WAIT2,
    TCP_TIME_WAIT,
    TCP_CLOSE,
    TCP_CLOSE_WAIT,
    TCP_LAST_ACK,
    TCP_LISTEN,
    TCP_CLOSING,
    TCP_NEW_SYN_RECV,
    TCP_BOUND_INACTIVE,
    TCP_MAX_STATES
};

// State stored explicitly in socket structure
struct sock {
    // ...
    volatile unsigned char sk_state;  // TCP state enum
    // ...
};

// Strict state transitions enforced
static int tcp_rcv_state_process(struct sock *sk, struct sk_buff *skb) {
    switch (sk->sk_state) {
        case TCP_SYN_SENT:
            // Only certain packets valid in this state
            // ...
        case TCP_ESTABLISHED:
            // Different handling in established state
            // ...
    }
}
```

### Wireshark TCP "FSM"

```c
// Wireshark does NOT track RFC 793 TCP states
// Instead tracks what it has OBSERVED

struct tcp_analysis {
    // NO explicit state enum!
    
    // Track observations instead
    uint8_t conversation_completeness;  // Flags of what we've seen
    
    // Per-flow tracking (bidirectional)
    tcp_flow_t flow1;
    tcp_flow_t flow2;
    
    // Analysis results
    struct tcp_acked *ta;  // Per-packet analysis
};

// "State" derived from observations
bool is_established(struct tcp_analysis *tcpd) {
    return (tcpd->conversation_completeness & 
            (TCP_COMPLETENESS_SYNSENT | TCP_COMPLETENESS_SYNACK | 
             TCP_COMPLETENESS_ACK)) == 
           (TCP_COMPLETENESS_SYNSENT | TCP_COMPLETENESS_SYNACK | 
            TCP_COMPLETENESS_ACK);
}
```

### Key Differences

| Aspect | Linux Kernel | Wireshark |
|--------|-------------|-----------|
| **Purpose** | Implement TCP protocol | Analyze TCP traffic |
| **State Variable** | Explicit enum | Implicit in flags |
| **State Transitions** | Strict, per RFC | Observations, flexible |
| **Invalid States** | Rejected | Flagged, analyzed |
| **Partial Connection** | Error | Normal (mid-capture) |
| **Direction** | Single socket | Both directions |

---

## 8.2 Wireshark FSMs vs Textbook FSMs

### Textbook FSM Definition

```
+------------------------------------------------------------------+
|                    TEXTBOOK FSM (Formal)                          |
+------------------------------------------------------------------+
|                                                                   |
|  M = (Q, Σ, δ, q₀, F)                                            |
|                                                                   |
|  Q  = Finite set of states                                        |
|  Σ  = Finite input alphabet                                       |
|  δ  = Transition function: Q × Σ → Q                             |
|  q₀ = Initial state                                               |
|  F  = Set of accepting states                                     |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  Properties:                                                      |
|  - Exactly one state at any time                                  |
|  - Deterministic transitions                                      |
|  - Well-defined initial and final states                          |
|  - Complete transition function                                   |
|                                                                   |
+------------------------------------------------------------------+
```

### Textbook Implementation

```c
// Textbook-style FSM
typedef enum {
    STATE_IDLE,
    STATE_HEADER,
    STATE_BODY,
    STATE_DONE,
    STATE_ERROR
} fsm_state_t;

typedef struct {
    fsm_state_t state;
} fsm_t;

// Transition table
static const fsm_state_t transitions[5][3] = {
    //         EVENT_START  EVENT_DATA  EVENT_END
    /* IDLE */   {STATE_HEADER, STATE_ERROR, STATE_ERROR},
    /* HEADER */ {STATE_ERROR,  STATE_BODY,  STATE_ERROR},
    /* BODY */   {STATE_ERROR,  STATE_BODY,  STATE_DONE},
    /* DONE */   {STATE_ERROR,  STATE_ERROR, STATE_ERROR},
    /* ERROR */  {STATE_ERROR,  STATE_ERROR, STATE_ERROR},
};

void fsm_handle_event(fsm_t *fsm, int event) {
    fsm->state = transitions[fsm->state][event];
}
```

### Wireshark Reality

```c
// Wireshark-style "FSM" - much more complex
typedef struct {
    // Multiple state dimensions
    uint32_t flags;           // Observed events
    uint32_t capabilities;    // What we can do
    
    // Per-direction state
    struct flow_state flow[2];
    
    // Accumulated data
    StringInfo accumulated_data;
    
    // Error tracking
    uint32_t error_flags;
    
} protocol_state_t;

// "Transition" is complex logic
void handle_packet(protocol_state_t *state, packet_info *pinfo, tvbuff_t *tvb) {
    // Record observation
    state->flags |= determine_event(tvb);
    
    // Multiple orthogonal updates
    update_flow_state(&state->flow[direction], tvb);
    accumulate_data(&state->accumulated_data, tvb);
    
    // Check for derived conditions
    if (can_now_decrypt(state)) {
        state->capabilities |= CAP_DECRYPT;
    }
    
    // Handle errors without crashing
    if (is_malformed(tvb)) {
        state->error_flags |= ERROR_MALFORMED;
        // Continue anyway
    }
}
```

### Comparison Table

| Characteristic | Textbook FSM | Wireshark FSM |
|---------------|--------------|---------------|
| **State Type** | Single enum | Composite struct |
| **States** | Finite, enumerated | Effectively infinite (flag combinations) |
| **Transitions** | Deterministic function | Complex conditional logic |
| **Initial State** | Well-defined q₀ | May be unknown (mid-capture) |
| **Error Handling** | Error state | Flags + continue |
| **Completeness** | All transitions defined | Best-effort |
| **Direction** | Usually unidirectional | Bidirectional flows |
| **Purpose** | Accept/reject strings | Extract maximum information |

---

## 8.3 Why Wireshark Deviates

### Fundamental Differences in Requirements

```
+------------------------------------------------------------------+
|          PROTOCOL IMPLEMENTATION vs PROTOCOL ANALYSIS             |
+------------------------------------------------------------------+
|                                                                   |
|  PROTOCOL IMPLEMENTATION (Linux, etc):                            |
|  =====================================                            |
|  - Must follow RFC exactly                                        |
|  - Reject invalid packets                                         |
|  - Handle one side of connection                                  |
|  - Start from connection beginning                                |
|  - Security: must be strict                                       |
|                                                                   |
|  PROTOCOL ANALYSIS (Wireshark):                                   |
|  ==============================                                   |
|  - Must handle ANY traffic                                        |
|  - Never crash on invalid packets                                 |
|  - See both sides of connection                                   |
|  - Often starts mid-connection                                    |
|  - Security: extract info, don't execute                          |
|                                                                   |
+------------------------------------------------------------------+
```

### Specific Reasons for Deviation

```
+------------------------------------------------------------------+
|                    REASONS WIRESHARK DEVIATES                     |
+------------------------------------------------------------------+
|                                                                   |
|  1. PASSIVE OBSERVER                                              |
|  ====================                                             |
|  - Sees both directions simultaneously                            |
|  - Cannot influence protocol                                      |
|  - Must correlate bidirectional traffic                           |
|                                                                   |
|  2. CAPTURE MAY START ANYTIME                                     |
|  ============================                                     |
|  - Connection may already be established                          |
|  - TLS may already be encrypted                                   |
|  - Must infer state from observations                             |
|                                                                   |
|  3. MALFORMED TRAFFIC IS INTERESTING                              |
|  ===================================                              |
|  - Attacks involve malformed packets                              |
|  - Bugs produce unusual sequences                                 |
|  - Cannot simply reject and forget                                |
|                                                                   |
|  4. MULTIPLE PROTOCOLS SIMULTANEOUSLY                             |
|  ====================================                             |
|  - TCP state + TLS state + HTTP state + ...                       |
|  - Orthogonal state machines interact                             |
|  - Single enum insufficient                                       |
|                                                                   |
|  5. PERFORMANCE AT SCALE                                          |
|  ========================                                         |
|  - Millions of packets                                            |
|  - Cannot afford complex state machines                           |
|  - Incremental processing required                                |
|                                                                   |
+------------------------------------------------------------------+
```

---

## Summary

```
+------------------------------------------------------------------+
|                    COMPARATIVE SUMMARY                            |
+------------------------------------------------------------------+
|                                                                   |
|  Linux TCP FSM:                                                   |
|  - Explicit enum state                                            |
|  - Strict RFC compliance                                          |
|  - Rejects invalid                                                |
|  - Single direction                                               |
|                                                                   |
|  Textbook FSM:                                                    |
|  - Formal (Q, Σ, δ, q₀, F)                                       |
|  - Deterministic                                                  |
|  - Complete transition function                                   |
|  - Accept/reject paradigm                                         |
|                                                                   |
|  Wireshark FSM:                                                   |
|  - Implicit/composite state                                       |
|  - Observation-based                                              |
|  - Best-effort parsing                                            |
|  - Both directions                                                |
|  - Never crashes                                                  |
|  - Extract maximum info                                           |
|                                                                   |
+------------------------------------------------------------------+
```

---

## 中文解释

### 比较分析

**1. Wireshark vs Linux内核TCP FSM**
- Linux内核：
  - 显式枚举状态（TCP_ESTABLISHED等）
  - 严格遵循RFC793
  - 拒绝无效数据包
  - 单向连接
  
- Wireshark：
  - 隐式状态（观察标志）
  - 追踪观察到的事件
  - 标记但不拒绝无效
  - 双向分析

**2. Wireshark vs 教科书FSM**
- 教科书FSM：
  - M = (Q, Σ, δ, q₀, F)
  - 确定性转换函数
  - 明确的接受/拒绝

- Wireshark FSM：
  - 复合结构状态
  - 复杂条件逻辑
  - 最大化信息提取

**3. 为什么Wireshark偏离传统FSM**
- 被动观察者：同时看到双向流量
- 捕获可能中途开始：无法假设从连接开始
- 畸形流量有价值：攻击和bug产生异常
- 多协议同时运行：TCP+TLS+HTTP叠加
- 大规模性能：需要高效处理
