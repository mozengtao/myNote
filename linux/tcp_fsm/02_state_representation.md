# TCP FSM: State Representation

## 1. Locating the TCP State Definitions

### 1.1 The State Enum: `tcp_states.h`

Location: `include/net/tcp_states.h`

```c
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
    TCP_CLOSING,    /* Now a valid state */
    TCP_MAX_STATES  /* Leave at the end! */
};
```

**Key Design Decisions:**
1. **Anonymous enum**: Used directly as integers
2. **Starts at 1**: `TCP_ESTABLISHED = 1`, not 0 (TCP_CLOSE is 7, not 0)
3. **Sentinel value**: `TCP_MAX_STATES` for bounds checking
4. **Comment on CLOSING**: Indicates historical context ("Now a valid state")

### 1.2 Bitmask Flags for Multi-State Checks

```c
#define TCP_STATE_MASK	0xF

#define TCP_ACTION_FIN	(1 << 7)

enum {
    TCPF_ESTABLISHED = (1 << 1),
    TCPF_SYN_SENT    = (1 << 2),
    TCPF_SYN_RECV    = (1 << 3),
    TCPF_FIN_WAIT1   = (1 << 4),
    TCPF_FIN_WAIT2   = (1 << 5),
    TCPF_TIME_WAIT   = (1 << 6),
    TCPF_CLOSE       = (1 << 7),
    TCPF_CLOSE_WAIT  = (1 << 8),
    TCPF_LAST_ACK    = (1 << 9),
    TCPF_LISTEN      = (1 << 10),
    TCPF_CLOSING     = (1 << 11) 
};
```

This enables efficient multi-state checks:
```c
// From tcp.c do_tcp_sendpages():
if ((1 << sk->sk_state) & ~(TCPF_ESTABLISHED | TCPF_CLOSE_WAIT))
    if ((err = sk_stream_wait_connect(sk, &timeo)) != 0)
        goto out_err;
```

中文说明：
- 主状态使用简单枚举值（1-11）
- 位掩码版本（TCPF_*）用于高效的多状态检查
- `(1 << sk->sk_state)` 将状态转换为位标志进行集合操作

---

## 2. Where State is Stored

### 2.1 Primary State Storage: `struct sock_common`

Location: `include/net/sock.h`

```c
struct sock_common {
    __be32          skc_daddr;
    __be32          skc_rcv_saddr;
    union {
        unsigned int    skc_hash;
        __u16           skc_u16hashes[2];
    };
    unsigned short      skc_family;
    volatile unsigned char  skc_state;    /* <-- TCP STATE HERE */
    unsigned char       skc_reuse;
    int                 skc_bound_dev_if;
    // ...
};
```

### 2.2 Accessor Macro

```c
// From sock.h
#define sk_state    __sk_common.skc_state
```

This allows accessing state as `sk->sk_state` from `struct sock`.

### 2.3 Memory Layout Significance

```
struct sock (simplified):
+------------------------------------------+
|  struct sock_common __sk_common;         |
|    +------------------------------------+|
|    | skc_daddr          (4 bytes)       ||
|    | skc_rcv_saddr      (4 bytes)       ||
|    | skc_hash           (4 bytes)       ||
|    | skc_family         (2 bytes)       ||
|    | skc_state          (1 byte)  <---  ||  TCP STATE
|    | skc_reuse          (1 byte)        ||
|    | skc_bound_dev_if   (4 bytes)       ||
|    +------------------------------------+|
|  socket_lock_t sk_lock;                  |
|  struct sk_buff_head sk_receive_queue;   |
|  // ... more fields ...                  |
+------------------------------------------+
```

中文说明：
- 状态存储在`sock_common`结构的`skc_state`字段
- 该字段是`volatile unsigned char`类型（1字节）
- 通过`sk_state`宏从`struct sock`访问
- 状态字段位于套接字结构的开头，便于缓存

---

## 3. Analysis: Why Enum + Integer State

### 3.1 Performance Reasons

```c
// Fast state comparison (single byte compare)
if (sk->sk_state == TCP_ESTABLISHED)

// Fast multi-state check (bitwise operation)  
if ((1 << sk->sk_state) & (TCPF_SYN_SENT | TCPF_SYN_RECV))

// Fast state table lookup (direct indexing)
static const unsigned char new_state[16] = {
  /* current state:        new state:      action: */
  /* (Invalid)      */ TCP_CLOSE,
  /* TCP_ESTABLISHED */ TCP_FIN_WAIT1 | TCP_ACTION_FIN,
  /* TCP_SYN_SENT   */ TCP_CLOSE,
  // ...
};
next = new_state[sk->sk_state];
```

### 3.2 Memory Efficiency

- **1 byte** for 12 states (vs. potentially larger structs)
- Fits in CPU cache line with other hot fields
- No dynamic allocation needed

### 3.3 Switch Statement Optimization

```c
// From tcp_rcv_state_process() in tcp_input.c
switch (sk->sk_state) {
case TCP_CLOSE:
    goto discard;

case TCP_LISTEN:
    if (th->ack)
        return 1;
    if (th->rst)
        goto discard;
    if (th->syn) {
        // Handle SYN...
    }
    goto discard;

case TCP_SYN_SENT:
    queued = tcp_rcv_synsent_state_process(sk, skb, th, len);
    // ...
}
```

Compilers optimize this to a **jump table** for O(1) dispatch.

---

## 4. How Invalid States Are Prevented

### 4.1 Controlled State Transitions via `tcp_set_state()`

```c
// From tcp.c
void tcp_set_state(struct sock *sk, int state)
{
    int oldstate = sk->sk_state;

    switch (state) {
    case TCP_ESTABLISHED:
        if (oldstate != TCP_ESTABLISHED)
            TCP_INC_STATS(sock_net(sk), TCP_MIB_CURRESTAB);
        break;

    case TCP_CLOSE:
        if (oldstate == TCP_CLOSE_WAIT || oldstate == TCP_ESTABLISHED)
            TCP_INC_STATS(sock_net(sk), TCP_MIB_ESTABRESETS);

        sk->sk_prot->unhash(sk);
        if (inet_csk(sk)->icsk_bind_hash &&
            !(sk->sk_userlocks & SOCK_BINDPORT_LOCK))
            inet_put_port(sk);
        /* fall through */
    default:
        if (oldstate == TCP_ESTABLISHED)
            TCP_DEC_STATS(sock_net(sk), TCP_MIB_CURRESTAB);
    }

    /* Change state AFTER socket is unhashed to avoid closed
     * socket sitting in hash tables.
     */
    sk->sk_state = state;
}
```

### 4.2 State Transition Table

```c
// From tcp.c - controls close() transitions
static const unsigned char new_state[16] = {
  /* current state:        new state:      action: */
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

    return next & TCP_ACTION_FIN;
}
```

中文说明：
- 状态转换通过`tcp_set_state()`函数集中控制
- `new_state[]`表定义了合法的关闭状态转换
- 高位（TCP_ACTION_FIN）用于编码附加动作
- 直接写入sk_state的代码极少，大多通过辅助函数

### 4.3 Assertions and Warnings

```c
// From tcp_fin() in tcp_input.c
default:
    /* Only TCP_LISTEN and TCP_CLOSE are left, in these
     * cases we should never reach this piece of code.
     */
    printk(KERN_ERR "%s: Impossible, sk->sk_state=%d\n",
           __func__, sk->sk_state);
    break;
```

---

## 5. State Visibility Across Subsystems

### 5.1 Public Interface

```
+-------------------+     +-------------------+     +-------------------+
|   Application     |     |   Netfilter/      |     |   /proc/net/tcp   |
|   (getsockopt)    |     |   Conntrack       |     |   (debugging)     |
+-------------------+     +-------------------+     +-------------------+
         |                         |                         |
         v                         v                         v
+------------------------------------------------------------------+
|                        sk->sk_state                              |
|                    (public, read anywhere)                       |
+------------------------------------------------------------------+
         |                         |                         |
         v                         v                         v
+-------------------+     +-------------------+     +-------------------+
|  TCP Input Path   |     |  TCP Output Path  |     |  Timer Handlers  |
|  (locked writes)  |     |  (locked writes)  |     |  (locked writes) |
+-------------------+     +-------------------+     +-------------------+
```

### 5.2 Read Access (Lock-free)

State can be **read without locks** for quick checks:
```c
// From tcp_poll() - no lock needed for read
if (sk->sk_state == TCP_LISTEN)
    return inet_csk_listen_poll(sk);
```

### 5.3 Write Access (Must Hold Lock)

State changes require socket lock:
```c
// From tcp_v4_do_rcv() - lock already held
if (sk->sk_state == TCP_ESTABLISHED) {
    if (tcp_rcv_established(sk, skb, tcp_hdr(skb), skb->len)) {
        // Handle established state...
    }
}
```

---

## 6. Avoiding State Explosion

### 6.1 The Problem

Full TCP state could be described as:
- 11 connection states × 
- 5 congestion states × 
- 4 shutdown combinations × 
- 2 urgent data states × 
- N timer states × ...

This would yield **thousands of explicit states**.

### 6.2 The Solution: Orthogonal Decomposition

```
+------------------+     +------------------+     +------------------+
|  Connection FSM  |     | Congestion FSM   |     |   Flags/Timers   |
|  (sk_state)      |     | (ca_state)       |     |   (various)      |
+------------------+     +------------------+     +------------------+
       11 states              5 states             ~20 flags

Total combinations: 11 × 5 × 2^20 (theoretically)
Actual encoded: 11 + 5 + 20 = ~36 values
```

### 6.3 Implementation

```c
// Connection state (primary FSM)
sk->sk_state = TCP_ESTABLISHED;

// Congestion control state (secondary FSM)
inet_csk(sk)->icsk_ca_state = TCP_CA_Open;

// Shutdown flags (orthogonal to state)
sk->sk_shutdown |= RCV_SHUTDOWN;

// Urgent data flags
tp->urg_data = TCP_URG_VALID;

// Nagle algorithm flags
tp->nonagle |= TCP_NAGLE_CORK;
```

中文说明：
- 通过正交分解避免状态爆炸
- 主连接状态机只有11个状态
- 拥塞控制是独立的5状态子机
- 关闭、紧急数据、Nagle等使用独立标志
- 这使得代码可管理，同时保持完整的语义

---

## 7. When Flags Complement Enum States

### 7.1 Half-Close Tracking

```c
// sk->sk_shutdown can be:
// 0            - fully open
// RCV_SHUTDOWN - received FIN
// SEND_SHUTDOWN- sent FIN  
// SHUTDOWN_MASK- both directions closed

// Used in tcp_poll():
if (sk->sk_shutdown & RCV_SHUTDOWN)
    mask |= POLLIN | POLLRDNORM | POLLRDHUP;
```

### 7.2 Congestion Window Sub-States

```c
enum tcp_ca_state {
    TCP_CA_Open = 0,      // Normal operation
    TCP_CA_Disorder = 1,  // Received dubious ACK
    TCP_CA_CWR = 2,       // Congestion Window Reduced
    TCP_CA_Recovery = 3,  // Fast Recovery active
    TCP_CA_Loss = 4,      // RTO timeout occurred
};
```

This is a **separate FSM** that runs concurrently with the connection FSM.

### 7.3 Urgent Data State

```c
#define TCP_URG_VALID   0x0100
#define TCP_URG_NOTYET  0x0200
#define TCP_URG_READ    0x0400

// tp->urg_data tracks:
// - Whether urgent data exists
// - Whether it's been read
// - Whether more is expected
```

---

## 8. Summary: State Representation Design

```
+------------------------------------------------------------------+
|                   TCP STATE ARCHITECTURE                          |
+------------------------------------------------------------------+
|                                                                   |
|  Primary State:     sk->sk_state (enum, 1 byte)                  |
|                     [CLOSED, LISTEN, SYN_SENT, ...]              |
|                                                                   |
|  Congestion State:  icsk->icsk_ca_state (enum)                   |
|                     [Open, Disorder, CWR, Recovery, Loss]        |
|                                                                   |
|  Shutdown Flags:    sk->sk_shutdown (bitmask)                    |
|                     [RCV_SHUTDOWN | SEND_SHUTDOWN]               |
|                                                                   |
|  URG State:         tp->urg_data (bitmask)                       |
|                     [URG_VALID | URG_NOTYET | URG_READ]          |
|                                                                   |
|  Nagle State:       tp->nonagle (bitmask)                        |
|                     [NAGLE_OFF | NAGLE_CORK | NAGLE_PUSH]        |
|                                                                   |
+------------------------------------------------------------------+
```

中文总结：
1. **主状态**：使用枚举值存储在`sk_state`，仅11个值
2. **位掩码版本**：`TCPF_*`用于高效的多状态检查
3. **状态转换**：通过`tcp_set_state()`集中控制
4. **无效状态防护**：转换表+断言检查
5. **正交分解**：拥塞控制、关闭、紧急数据使用独立状态/标志
6. **性能优化**：单字节存储，jump table调度，缓存友好布局

This design achieves:
- **Minimal memory footprint** (1 byte primary state)
- **O(1) state dispatch** (compiler jump tables)
- **No state explosion** (orthogonal decomposition)
- **Type safety** (enum prevents invalid values at compile time)
- **Runtime safety** (transition table restricts changes)
