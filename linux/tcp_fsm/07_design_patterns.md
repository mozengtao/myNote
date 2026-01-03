# TCP FSM: Design Patterns Used

## 1. Pattern Overview

The Linux TCP FSM employs **multiple design patterns** in combination:

```
+------------------------------------------------------------------+
|                    TCP FSM DESIGN PATTERNS                        |
+------------------------------------------------------------------+
|                                                                   |
|  1. Enum + Switch FSM       - Core state dispatch                |
|  2. Event-Driven FSM        - Reactive processing                |
|  3. Hierarchical FSM        - Implicit state grouping            |
|  4. Flag-Augmented FSM      - Orthogonal state dimensions        |
|  5. Table-Driven FSM        - Transition tables (partial)        |
|                                                                   |
+------------------------------------------------------------------+
```

---

## 2. Pattern 1: Enum + Switch FSM

### 2.1 Structure

```c
// State definition (tcp_states.h)
enum {
    TCP_ESTABLISHED = 1,
    TCP_SYN_SENT,
    TCP_SYN_RECV,
    // ...
    TCP_MAX_STATES
};

// State dispatch (tcp_input.c)
switch (sk->sk_state) {
case TCP_CLOSE:
    goto discard;
    
case TCP_LISTEN:
    if (th->syn) {
        icsk->icsk_af_ops->conn_request(sk, skb);
    }
    break;
    
case TCP_SYN_SENT:
    return tcp_rcv_synsent_state_process(sk, skb, th, len);
    
// ... more cases ...
}
```

### 2.2 Why This Pattern

| Advantage | Explanation |
|-----------|-------------|
| Compiler optimization | Switch becomes jump table (O(1) dispatch) |
| Type safety | Enum prevents invalid state values |
| Readability | Clear mapping of state → behavior |
| Debugging | State visible in debugger |
| Exhaustiveness | Compiler warns on missing cases (with -Wswitch) |

### 2.3 Implementation Variations

```c
// VARIATION 1: Nested switch (by event type)
switch (sk->sk_state) {
case TCP_SYN_RECV:
    if (th->ack) {
        // Handle ACK
    }
    break;
}

// VARIATION 2: Delegation to specialized handlers
case TCP_SYN_SENT:
    return tcp_rcv_synsent_state_process(sk, skb, th, len);

// VARIATION 3: Bitmask for multi-state checks
if ((1 << sk->sk_state) & (TCPF_SYN_SENT | TCPF_SYN_RECV))
    // Handle both states
```

中文说明：
- Enum + Switch是TCP FSM的基础模式
- 编译器将switch优化为跳转表实现O(1)调度
- 枚举提供类型安全和编译时检查

---

## 3. Pattern 2: Event-Driven FSM

### 3.1 Structure

```
            EVENTS                      FSM CORE                    OUTPUTS
    +------------------+          +------------------+          +------------------+
    | Packet Arrival   |--------->|                  |--------->| State Change     |
    | Timer Expiration |--------->|   FSM Dispatch   |--------->| Send Packet      |
    | Syscall          |--------->|   (stateless)    |--------->| Wake Process     |
    | ICMP Error       |--------->|                  |--------->| Update Timers    |
    +------------------+          +------------------+          +------------------+
                                         |
                                         v
                                  +------------------+
                                  |  State Storage   |
                                  |  (sk->sk_state)  |
                                  +------------------+
```

### 3.2 Implementation

```c
// Event: Packet arrival
// Handler: tcp_v4_rcv() -> tcp_v4_do_rcv() -> tcp_rcv_state_process()
int tcp_v4_rcv(struct sk_buff *skb)
{
    // ... find socket ...
    ret = tcp_v4_do_rcv(sk, skb);  // Process event
}

// Event: Timer expiration
// Handler: tcp_retransmit_timer()
void tcp_retransmit_timer(struct sock *sk)
{
    // ... handle timeout event ...
}

// Event: User syscall
// Handler: tcp_close()
void tcp_close(struct sock *sk, long timeout)
{
    // ... handle close event ...
}
```

### 3.3 Why Event-Driven

| Property | Benefit |
|----------|---------|
| No polling | Zero CPU usage when idle |
| Low latency | Immediate response to events |
| Scalability | Single event queue for all connections |
| Kernel integration | Uses existing interrupt/softirq infrastructure |
| Memory efficiency | No per-connection event loop |

中文说明：
- 事件驱动模式：FSM对事件被动响应
- 事件源：网络包、定时器、系统调用、ICMP错误
- 优势：零空闲开销、低延迟、高可扩展性

---

## 4. Pattern 3: Hierarchical FSM (Implicit)

### 4.1 State Groupings

```
                    +---------------------------+
                    |       ALL STATES          |
                    +---------------------------+
                               |
            +------------------+------------------+
            |                                     |
    +---------------+                     +---------------+
    | Connected     |                     | Non-Connected |
    | States        |                     | States        |
    +---------------+                     +---------------+
    | ESTABLISHED   |                     | CLOSED        |
    | FIN_WAIT1     |                     | LISTEN        |
    | FIN_WAIT2     |                     | SYN_SENT      |
    | CLOSE_WAIT    |                     | SYN_RECV      |
    | CLOSING       |                     | TIME_WAIT     |
    | LAST_ACK      |                     +---------------+
    +---------------+
```

### 4.2 Implementation via Bitmasks

```c
// Define state groups
#define TCPF_CONNECTED (TCPF_ESTABLISHED | TCPF_FIN_WAIT1 | TCPF_FIN_WAIT2 | \
                        TCPF_CLOSE_WAIT | TCPF_CLOSING | TCPF_LAST_ACK)

// Check if in connected group
if ((1 << sk->sk_state) & ~(TCPF_SYN_SENT | TCPF_SYN_RECV)) {
    // Connected state - can read data
}

// Check if can send data
if ((1 << sk->sk_state) & (TCPF_ESTABLISHED | TCPF_CLOSE_WAIT)) {
    // Can send
}
```

### 4.3 Implicit Hierarchy in Code Structure

```c
// tcp_v4_do_rcv() - first level dispatch
if (sk->sk_state == TCP_ESTABLISHED) {
    // Fast path for most common state
    return tcp_rcv_established(sk, skb, tcp_hdr(skb), skb->len);
}

if (sk->sk_state == TCP_LISTEN) {
    // Handle new connections
    nsk = tcp_v4_hnd_req(sk, skb);
}

// Slow path for other states
return tcp_rcv_state_process(sk, skb, tcp_hdr(skb), skb->len);
```

中文说明：
- 隐式层次结构：状态被分组但不显式编码
- 通过位掩码实现状态组检查
- 代码结构反映状态分组（快速路径 vs 慢速路径）

---

## 5. Pattern 4: Flag-Augmented FSM

### 5.1 The Problem: State Explosion

```
Without flags: 11 states × 5 CA states × 4 shutdown combos = 220 states
With flags:    11 states + 5 CA states + 4 shutdown flags = ~20 values
```

### 5.2 Orthogonal State Dimensions

```
+------------------------------------------------------------------+
|                    ORTHOGONAL DIMENSIONS                          |
+------------------------------------------------------------------+
|                                                                   |
|  DIMENSION 1: Connection State (sk->sk_state)                    |
|    Values: CLOSED, LISTEN, SYN_SENT, ESTABLISHED, ...           |
|    Transitions: RFC 793 state machine                            |
|                                                                   |
|  DIMENSION 2: Congestion Control (icsk->icsk_ca_state)          |
|    Values: Open, Disorder, CWR, Recovery, Loss                  |
|    Transitions: Independent of connection state                  |
|                                                                   |
|  DIMENSION 3: Shutdown (sk->sk_shutdown)                         |
|    Flags: RCV_SHUTDOWN, SEND_SHUTDOWN                           |
|    Orthogonal: Can receive shutdown before send shutdown        |
|                                                                   |
|  DIMENSION 4: Socket Flags (various)                             |
|    SOCK_DONE: Connection complete                               |
|    SOCK_DEAD: Socket orphaned                                   |
|    SOCK_LINGER: Linger on close                                 |
|                                                                   |
+------------------------------------------------------------------+
```

### 5.3 Implementation

```c
// Connection state
sk->sk_state = TCP_FIN_WAIT1;

// Independent congestion state
inet_csk(sk)->icsk_ca_state = TCP_CA_Recovery;

// Shutdown flags
sk->sk_shutdown |= SEND_SHUTDOWN;

// Socket flags  
sock_set_flag(sk, SOCK_DONE);

// Combined check
if (sk->sk_state == TCP_ESTABLISHED &&
    !(sk->sk_shutdown & RCV_SHUTDOWN)) {
    // Can receive data
}
```

### 5.4 Congestion Control Sub-FSM

```c
enum tcp_ca_state {
    TCP_CA_Open = 0,      // Normal operation
    TCP_CA_Disorder = 1,  // Dubious ACK received
    TCP_CA_CWR = 2,       // ECN congestion window reduced
    TCP_CA_Recovery = 3,  // Fast recovery in progress
    TCP_CA_Loss = 4,      // Retransmission timeout
};

// State transitions
void tcp_set_ca_state(struct sock *sk, const u8 ca_state)
{
    struct inet_connection_sock *icsk = inet_csk(sk);

    if (icsk->icsk_ca_ops->set_state)
        icsk->icsk_ca_ops->set_state(sk, ca_state);
    icsk->icsk_ca_state = ca_state;
}
```

中文说明：
- 标志增强模式避免了状态爆炸
- 正交维度：连接状态、拥塞控制、关闭、套接字标志
- 每个维度独立变化和检查

---

## 6. Pattern 5: Table-Driven FSM (Partial)

### 6.1 Close State Transitions

```c
// From tcp.c
static const unsigned char new_state[16] = {
  /* current state:        new state:      action:     */
  /* (Invalid)      */ TCP_CLOSE,
  /* TCP_ESTABLISHED */ TCP_FIN_WAIT1 | TCP_ACTION_FIN,
  /* TCP_SYN_SENT   */ TCP_CLOSE,
  /* TCP_SYN_RECV   */ TCP_FIN_WAIT1 | TCP_ACTION_FIN,
  /* TCP_FIN_WAIT1  */ TCP_FIN_WAIT1,
  /* TCP_FIN_WAIT2  */ TCP_FIN_WAIT2,
  /* TCP_TIME_WAIT  */ TCP_CLOSE,
  /* TCP_CLOSE      */ TCP_CLOSE,
  /* TCP_CLOSE_WAIT */ TCP_LAST_ACK  | TCP_ACTION_FIN,
  /* TCP_LAST_ACK   */ TCP_LAST_ACK,
  /* TCP_LISTEN     */ TCP_CLOSE,
  /* TCP_CLOSING    */ TCP_CLOSING,
};

static int tcp_close_state(struct sock *sk)
{
    int next = (int)new_state[sk->sk_state];
    int ns = next & TCP_STATE_MASK;

    tcp_set_state(sk, ns);

    return next & TCP_ACTION_FIN;  // Returns action flag
}
```

### 6.2 Why Only Partial Table-Driven

The full transition logic is NOT table-driven because:

```c
// Complex conditions can't fit in a table
case TCP_FIN_WAIT1:
    if (tp->snd_una == tp->write_seq) {
        // Only transition if all data ACKed
        tcp_set_state(sk, TCP_FIN_WAIT2);
        // ... complex setup ...
    }
    break;

// Multiple actions required
case TCP_SYN_RECV:
    if (acceptable) {
        tcp_set_state(sk, TCP_ESTABLISHED);
        sk->sk_state_change(sk);
        tcp_init_metrics(sk);
        tcp_init_congestion_control(sk);
        // ... many more actions ...
    }
    break;
```

中文说明：
- 表驱动仅用于简单的关闭路径转换
- 完整逻辑无法用表表达（条件复杂、多动作）
- 表中高位编码附加动作（TCP_ACTION_FIN）

---

## 7. Pattern Comparison

```
+------------------------------------------------------------------+
|                    PATTERN COMPARISON                             |
+------------------------------------------------------------------+
| Pattern          | Used For                | Limitation           |
+------------------------------------------------------------------+
| Enum + Switch    | Core dispatch           | Verbose for many     |
|                  |                         | states               |
+------------------------------------------------------------------+
| Event-Driven     | Reactive processing     | Harder to trace      |
|                  |                         | execution flow       |
+------------------------------------------------------------------+
| Hierarchical     | State grouping          | Implicit, not        |
| (Implicit)       |                         | enforced by type     |
+------------------------------------------------------------------+
| Flag-Augmented   | Avoiding explosion      | Can make reasoning   |
|                  |                         | harder               |
+------------------------------------------------------------------+
| Table-Driven     | Simple transitions      | Can't express        |
| (Partial)        |                         | complex logic        |
+------------------------------------------------------------------+
```

---

## 8. Why Each Pattern Was Chosen

### 8.1 Enum + Switch: Primary Dispatch

```
CHOSEN BECAUSE:
- Compiler optimizes to O(1) jump table
- Clear, readable code
- Matches RFC 793 specification structure
- Easy to debug (state visible)
```

### 8.2 Event-Driven: Architecture Fit

```
CHOSEN BECAUSE:
- Kernel is inherently event-driven (interrupts)
- Zero idle overhead
- Handles asynchronous events naturally
- Scales to millions of connections
```

### 8.3 Hierarchical (Implicit): Performance

```
CHOSEN BECAUSE:
- Fast path optimization (ESTABLISHED most common)
- Bitmask checks are single instruction
- Avoids explicit hierarchy overhead
- Matches natural code organization
```

### 8.4 Flag-Augmented: Complexity Management

```
CHOSEN BECAUSE:
- Avoids 200+ explicit states
- Each dimension independently testable
- Matches TCP's orthogonal concerns
- Simpler reasoning about each dimension
```

### 8.5 Table-Driven (Partial): Close Path Simplicity

```
CHOSEN BECAUSE:
- Close transitions are simple and regular
- Table fit in single cache line
- Action encoded in same byte as state
- Reduces code duplication
```

---

## 9. Summary

```
+------------------------------------------------------------------+
|                 TCP FSM PATTERN SUMMARY                           |
+------------------------------------------------------------------+
|                                                                   |
|  CORE PATTERNS:                                                   |
|                                                                   |
|  1. Enum + Switch                                                |
|     └── Primary dispatch mechanism                               |
|     └── O(1) via jump table                                      |
|                                                                   |
|  2. Event-Driven                                                  |
|     └── Packets, timers, syscalls as events                     |
|     └── Reactive, not proactive                                  |
|                                                                   |
|  3. Hierarchical (Implicit)                                       |
|     └── Fast path for ESTABLISHED                                |
|     └── Bitmask state groups                                     |
|                                                                   |
|  4. Flag-Augmented                                                |
|     └── sk_shutdown, ca_state, socket flags                     |
|     └── Orthogonal dimensions                                    |
|                                                                   |
|  5. Table-Driven (Partial)                                        |
|     └── new_state[] for close transitions                       |
|     └── Action flags encoded in table                            |
|                                                                   |
|  DESIGN PRINCIPLE:                                                |
|     Use the right pattern for each sub-problem                   |
|     Combine patterns for comprehensive solution                   |
|                                                                   |
+------------------------------------------------------------------+
```

中文总结：
1. **Enum + Switch**：主调度机制，编译器优化为跳转表
2. **事件驱动**：适应内核中断驱动架构
3. **隐式层次**：快速路径优化+位掩码分组
4. **标志增强**：避免状态爆炸，正交关注点
5. **部分表驱动**：简化关闭路径转换

设计原则：为每个子问题选择正确的模式，组合模式以获得完整解决方案。
