# TCP FSM: Engineering Trade-offs

## 1. Performance vs Clarity

### 1.1 The Trade-off

```
+------------------------------------------------------------------+
|                 PERFORMANCE vs CLARITY                            |
+------------------------------------------------------------------+
|                                                                   |
|  HIGH CLARITY (not used):                                         |
|    struct transition {                                           |
|        state_t from, to;                                         |
|        event_t trigger;                                          |
|        action_fn action;                                         |
|    };                                                            |
|    // Easy to read but O(n) lookup                               |
|                                                                   |
|  HIGH PERFORMANCE (used):                                         |
|    switch (sk->sk_state) {                                       |
|    case TCP_ESTABLISHED:                                         |
|        // Inline actions, O(1) dispatch                          |
|        break;                                                    |
|    }                                                             |
|                                                                   |
+------------------------------------------------------------------+
```

### 1.2 Kernel Code Evidence

```c
// FAST PATH - Performance Critical
// From tcp_v4_do_rcv()
if (sk->sk_state == TCP_ESTABLISHED) { /* Fast path */
    sock_rps_save_rxhash(sk, skb);
    if (tcp_rcv_established(sk, skb, tcp_hdr(skb), skb->len)) {
        rsk = sk;
        goto reset;
    }
    return 0;  // Exit immediately for common case
}

// SLOW PATH - Clarity Prioritized
// From tcp_rcv_state_process()
switch (sk->sk_state) {
case TCP_CLOSE:
    goto discard;  // Clear intent

case TCP_LISTEN:
    if (th->ack)
        return 1;  // RFC says send RST
    if (th->rst)
        goto discard;  // RFC says discard
    if (th->syn) {
        // Handle connection request
    }
    goto discard;

// Each case documents the RFC requirement
```

### 1.3 Performance Optimizations

```c
// Optimization 1: Branch prediction hints
if (likely(sk->sk_state == TCP_ESTABLISHED)) {
    // Most packets hit this path
}

// Optimization 2: Inline hot functions
static inline void tcp_fast_path_check(struct sock *sk)
{
    // Inlining eliminates function call overhead
}

// Optimization 3: Avoid unnecessary state checks
// tcp_rcv_established() doesn't check state - caller guarantees it

// Optimization 4: Direct indexing instead of search
static const unsigned char new_state[16] = { ... };
next = new_state[sk->sk_state];  // O(1)
```

中文说明：
- 快速路径优化ESTABLISHED状态（最常见）
- 慢速路径保持代码清晰度
- 使用likely/unlikely提示、内联函数、直接索引

---

## 2. Centralized vs Distributed Transition Logic

### 2.1 The Linux Choice: Distributed

```
                    DISTRIBUTED LOGIC
+------------------------------------------------------------------+
|                                                                   |
|  tcp_close()          tcp_fin()          tcp_reset()             |
|  User close           FIN received       RST received            |
|  +---------+          +---------+        +---------+             |
|  |         |          |         |        |         |             |
|  | Local   |          | Local   |        | Local   |             |
|  | Logic   |          | Logic   |        | Logic   |             |
|  |         |          |         |        |         |             |
|  +---------+          +---------+        +---------+             |
|       |                    |                  |                   |
|       +--------------------+------------------+                   |
|                            |                                      |
|                            v                                      |
|                    +---------------+                              |
|                    | tcp_set_state |                              |
|                    | (centralized  |                              |
|                    |  side-effects)|                              |
|                    +---------------+                              |
|                                                                   |
+------------------------------------------------------------------+
```

### 2.2 Why NOT Centralized Table

A centralized approach would look like:

```c
// HYPOTHETICAL - NOT USED
struct fsm_transition {
    int from_state;
    int event;
    int to_state;
    void (*action)(struct sock *);
};

static struct fsm_transition tcp_transitions[] = {
    {TCP_LISTEN,     EV_SYN,      TCP_SYN_RECV,    action_send_synack},
    {TCP_SYN_SENT,   EV_SYN_ACK,  TCP_ESTABLISHED, action_send_ack},
    {TCP_ESTABLISHED,EV_FIN,      TCP_CLOSE_WAIT,  action_send_ack},
    // 100+ more entries...
};
```

Problems with this approach:
1. **Lookup overhead**: O(n) or requires hash table
2. **Complex actions**: Can't fit in function pointer
3. **Conditional transitions**: Not expressible in flat table
4. **Context loss**: Action functions lack local context

### 2.3 Actual Distributed Benefits

```c
// BENEFIT 1: Full context available
static void tcp_fin(struct sock *sk)
{
    struct tcp_sock *tp = tcp_sk(sk);
    
    // Can access all socket state
    inet_csk_schedule_ack(sk);
    sk->sk_shutdown |= RCV_SHUTDOWN;
    sock_set_flag(sk, SOCK_DONE);

    switch (sk->sk_state) {
    case TCP_FIN_WAIT1:
        // Context-specific action
        tcp_send_ack(sk);
        tcp_set_state(sk, TCP_CLOSING);
        break;
    }
}

// BENEFIT 2: Conditional transitions
case TCP_FIN_WAIT1:
    if (tp->snd_una == tp->write_seq) {  // <-- Condition
        tcp_set_state(sk, TCP_FIN_WAIT2);
    }
    break;

// BENEFIT 3: Complex multi-action sequences
case TCP_SYN_RECV:
    if (acceptable) {
        tcp_set_state(sk, TCP_ESTABLISHED);
        sk->sk_state_change(sk);
        tcp_init_metrics(sk);
        tcp_init_congestion_control(sk);
        tcp_init_buffer_space(sk);
        // 10+ more initializations...
    }
    break;
```

中文说明：
- Linux选择分布式逻辑，事件处理器包含完整上下文
- 集中式表无法表达复杂动作和条件转换
- `tcp_set_state()`集中处理状态变更的副作用

---

## 3. FSM Correctness vs Flexibility

### 3.1 Strict RFC Compliance

```c
// RFC 793 specifies exact state machine
// Linux follows it closely

/*
 * This function implements the receiving procedure of RFC 793 for
 * all states except ESTABLISHED and TIME_WAIT.
 */
int tcp_rcv_state_process(struct sock *sk, struct sk_buff *skb,
                          const struct tcphdr *th, unsigned int len)
{
    // Comments reference RFC sections
    /* step 5: check the ACK field */
    if (th->ack) {
        // ...
    }
    
    /* step 6: check the URG bit */
    tcp_urg(sk, skb, th);
    
    /* step 7: process the segment text */
    // ...
}
```

### 3.2 Practical Deviations

```c
// DEVIATION 1: BSD-compatible TIME_WAIT handling
/*
 * Nope, it was not mistake. It is really desired behaviour
 * f.e. on http servers, when such sockets are useless, but
 * consume significant resources. Let's do it with special
 * linger2 option.                    --ANK
 */

// DEVIATION 2: Orphan socket limits (not in RFC)
if (tcp_too_many_orphans(sk, shift)) {
    // Force close to protect system resources
    tcp_send_active_reset(sk, GFP_ATOMIC);
    tcp_done(sk);
}

// DEVIATION 3: SYN cookies (not in RFC)
// Allows accepting connections without state during SYN flood
```

### 3.3 Extension Points

```c
// Congestion control is pluggable
struct tcp_congestion_ops {
    void (*init)(struct sock *sk);
    void (*release)(struct sock *sk);
    u32  (*ssthresh)(struct sock *sk);
    void (*cong_avoid)(struct sock *sk, u32 ack, u32 in_flight);
    void (*set_state)(struct sock *sk, u8 new_state);
    // ...
};

// Register new algorithm
tcp_register_congestion_control(&tcp_cubic);
```

中文说明：
- 核心状态机严格遵循RFC 793
- 实际偏离用于系统保护（孤儿限制）和性能（SYN cookies）
- 拥塞控制通过插件机制提供灵活性

---

## 4. Concrete Code Trade-off Examples

### 4.1 Trade-off: Inlining vs Code Size

```c
// INLINED for performance
static inline void tcp_fast_path_check(struct sock *sk)
{
    struct tcp_sock *tp = tcp_sk(sk);

    if (tp->rcv_wnd &&
        !tp->urg_data &&
        !tp->rx_opt.num_sacks)
        tcp_fast_path_on(tp);
    else
        tcp_fast_path_off(tp);
}

// NOT INLINED - code size matters
void tcp_set_state(struct sock *sk, int state)
{
    // ~30 lines of code
    // Called from many places
    // Inlining would bloat code
}
```

### 4.2 Trade-off: Lock Granularity

```c
// COARSE LOCKING (chosen)
// Single socket lock for all operations
lock_sock(sk);
// ... all processing ...
release_sock(sk);

// FINE LOCKING (not chosen)
// Would require:
spin_lock(&sk->state_lock);
// state change
spin_unlock(&sk->state_lock);

spin_lock(&sk->queue_lock);
// queue operation
spin_unlock(&sk->queue_lock);

// Too much complexity, too many potential deadlocks
```

### 4.3 Trade-off: Memory vs Speed

```c
// SPEED CHOSEN: Precomputed bitmasks
enum {
    TCPF_ESTABLISHED = (1 << 1),
    TCPF_SYN_SENT    = (1 << 2),
    // ...
};

// Fast multi-state check
if ((1 << sk->sk_state) & (TCPF_FIN_WAIT1 | TCPF_FIN_WAIT2))

// MEMORY CHOSEN: Minimal TIME_WAIT structure
struct tcp_timewait_sock {
    // Only 240 bytes vs 600+ for full socket
    // Trades functionality for memory
};
```

---

## 5. Trade-off Summary Matrix

```
+------------------------------------------------------------------+
|                    TRADE-OFF MATRIX                               |
+------------------------------------------------------------------+
| Decision              | Chose           | Rejected        |      |
+------------------------------------------------------------------+
| Dispatch mechanism    | switch          | table lookup    | Perf |
| Transition logic      | distributed     | centralized     | Flex |
| Fast path handling    | special case    | unified         | Perf |
| Lock granularity      | coarse (socket) | fine-grained    | Simp |
| State storage         | enum (1 byte)   | struct          | Mem  |
| Flag representation   | bitmasks        | nested states   | Flex |
| TIME_WAIT structure   | minimal struct  | full socket     | Mem  |
| Hot functions         | inline          | out-of-line     | Perf |
| RFC compliance        | strict + exts   | pure            | Prac |
+------------------------------------------------------------------+
| Key: Perf=Performance, Flex=Flexibility, Simp=Simplicity,        |
|      Mem=Memory, Prac=Practicality                               |
+------------------------------------------------------------------+
```

---

## 6. Lessons for System Design

### 6.1 Fast Path Optimization

```
PRINCIPLE: Identify and optimize the common case

TCP applies this:
- ESTABLISHED is most common state
- tcp_rcv_established() is separate fast path
- Skip validation already done by caller
```

### 6.2 Context-Aware Distribution

```
PRINCIPLE: Put logic where context is available

TCP applies this:
- tcp_fin() handles FIN with full socket access
- tcp_close() handles close with user context info
- Avoid passing everything through central dispatcher
```

### 6.3 Layered Correctness

```
PRINCIPLE: Correctness at boundaries, flexibility inside

TCP applies this:
- Strict RFC state machine for interop
- Flexible congestion control plugins
- Practical deviations for real-world needs
```

---

## 7. Summary

```
+------------------------------------------------------------------+
|                 ENGINEERING TRADE-OFFS                            |
+------------------------------------------------------------------+
|                                                                   |
|  PERFORMANCE OPTIMIZATIONS:                                       |
|    - ESTABLISHED fast path                                       |
|    - switch → jump table                                         |
|    - Inline hot functions                                        |
|    - Precomputed bitmasks                                        |
|                                                                   |
|  COMPLEXITY MANAGEMENT:                                           |
|    - Distributed logic (context available)                       |
|    - Coarse locking (avoid deadlocks)                           |
|    - Centralized side-effects (tcp_set_state)                   |
|                                                                   |
|  MEMORY EFFICIENCY:                                               |
|    - 1-byte state enum                                           |
|    - Minimal TIME_WAIT structure                                 |
|    - Orthogonal flags vs nested states                          |
|                                                                   |
|  CORRECTNESS BALANCE:                                             |
|    - Strict RFC compliance for interop                          |
|    - Practical deviations for protection                        |
|    - Extension points for evolution                              |
|                                                                   |
+------------------------------------------------------------------+
```

中文总结：
1. **性能vs清晰度**：快速路径优化性能，慢速路径保持清晰
2. **集中vs分布式**：分布式逻辑提供上下文，集中副作用处理
3. **正确性vs灵活性**：严格RFC合规+实际偏离+扩展点
4. **具体权衡**：
   - switch调度 vs 表查找 → 性能
   - 粗粒度锁 vs 细粒度锁 → 简单性
   - 最小TIME_WAIT结构 → 内存效率
   - 内联热函数 → 性能
