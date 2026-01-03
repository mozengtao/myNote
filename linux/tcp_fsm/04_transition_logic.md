# TCP FSM: Transition Logic

## 1. Core Transition Logic Locations

### 1.1 Primary Files

| File | Purpose | Key Functions |
|------|---------|---------------|
| `tcp_input.c` | Incoming packet processing | `tcp_rcv_state_process()`, `tcp_fin()`, `tcp_reset()` |
| `tcp.c` | User-space initiated transitions | `tcp_close()`, `tcp_shutdown()`, `tcp_set_state()` |
| `tcp_output.c` | Connection initiation | `tcp_connect()` |
| `tcp_minisocks.c` | TIME_WAIT handling | `tcp_timewait_state_process()`, `tcp_time_wait()` |
| `tcp_timer.c` | Timer-driven transitions | `tcp_retransmit_timer()`, `tcp_keepalive_timer()` |

### 1.2 The Master Dispatcher: `tcp_rcv_state_process()`

Location: `net/ipv4/tcp_input.c` (lines 5790-6005)

```c
/*
 * This function implements the receiving procedure of RFC 793 for
 * all states except ESTABLISHED and TIME_WAIT.
 * It's called from both tcp_v4_rcv and tcp_v6_rcv and should be
 * address independent.
 */
int tcp_rcv_state_process(struct sock *sk, struct sk_buff *skb,
                          const struct tcphdr *th, unsigned int len)
{
    struct tcp_sock *tp = tcp_sk(sk);
    struct inet_connection_sock *icsk = inet_csk(sk);
    int queued = 0;

    switch (sk->sk_state) {
    case TCP_CLOSE:
        goto discard;

    case TCP_LISTEN:
        if (th->ack)
            return 1;  // Send RST
        if (th->rst)
            goto discard;
        if (th->syn) {
            if (icsk->icsk_af_ops->conn_request(sk, skb) < 0)
                return 1;
            kfree_skb(skb);
            return 0;
        }
        goto discard;

    case TCP_SYN_SENT:
        queued = tcp_rcv_synsent_state_process(sk, skb, th, len);
        if (queued >= 0)
            return queued;
        tcp_urg(sk, skb, th);
        __kfree_skb(skb);
        tcp_data_snd_check(sk);
        return 0;
    }

    // Validate incoming packet
    res = tcp_validate_incoming(sk, skb, th, 0);
    if (res <= 0)
        return -res;

    // Process ACK for remaining states
    if (th->ack) {
        int acceptable = tcp_ack(sk, skb, FLAG_SLOWPATH) > 0;

        switch (sk->sk_state) {
        case TCP_SYN_RECV:
            if (acceptable) {
                tcp_set_state(sk, TCP_ESTABLISHED);
                sk->sk_state_change(sk);
                // ... initialization ...
            }
            break;

        case TCP_FIN_WAIT1:
            if (tp->snd_una == tp->write_seq) {
                tcp_set_state(sk, TCP_FIN_WAIT2);
                // ... or tcp_time_wait() ...
            }
            break;

        case TCP_CLOSING:
            if (tp->snd_una == tp->write_seq) {
                tcp_time_wait(sk, TCP_TIME_WAIT, 0);
                goto discard;
            }
            break;

        case TCP_LAST_ACK:
            if (tp->snd_una == tp->write_seq) {
                tcp_done(sk);  // -> CLOSED
                goto discard;
            }
            break;
        }
    }

    // Process segment text
    switch (sk->sk_state) {
    case TCP_CLOSE_WAIT:
    case TCP_CLOSING:
    case TCP_LAST_ACK:
    case TCP_FIN_WAIT1:
    case TCP_FIN_WAIT2:
    case TCP_ESTABLISHED:
        tcp_data_queue(sk, skb);
        queued = 1;
        break;
    }

    return 0;
}
```

中文说明：
- `tcp_rcv_state_process()`是处理非ESTABLISHED状态包的核心函数
- 使用嵌套的switch语句根据当前状态分发处理
- 实现了RFC 793的接收过程

---

## 2. How switch(enum state) Is Used

### 2.1 Pattern: Two-Level Dispatch

```c
// LEVEL 1: Initial state check
switch (sk->sk_state) {
case TCP_CLOSE:
    goto discard;

case TCP_LISTEN:
    // Handle listen-specific logic...
    break;

case TCP_SYN_SENT:
    // Delegate to specialized function
    return tcp_rcv_synsent_state_process(sk, skb, th, len);
}

// LEVEL 2: Post-validation dispatch
if (th->ack) {
    switch (sk->sk_state) {
    case TCP_SYN_RECV:
        // Transition to ESTABLISHED
        break;
    case TCP_FIN_WAIT1:
        // Transition to FIN_WAIT2
        break;
    }
}
```

### 2.2 Pattern: Event-Specific Handler

From `tcp_fin()` in `tcp_input.c`:

```c
static void tcp_fin(struct sock *sk)
{
    inet_csk_schedule_ack(sk);
    sk->sk_shutdown |= RCV_SHUTDOWN;
    sock_set_flag(sk, SOCK_DONE);

    switch (sk->sk_state) {
    case TCP_SYN_RECV:
    case TCP_ESTABLISHED:
        /* Move to CLOSE_WAIT */
        tcp_set_state(sk, TCP_CLOSE_WAIT);
        inet_csk(sk)->icsk_ack.pingpong = 1;
        break;

    case TCP_CLOSE_WAIT:
    case TCP_CLOSING:
        /* Received a retransmission of the FIN, do nothing. */
        break;

    case TCP_LAST_ACK:
        /* RFC793: Remain in the LAST-ACK state. */
        break;

    case TCP_FIN_WAIT1:
        /* Simultaneous close: enter CLOSING */
        tcp_send_ack(sk);
        tcp_set_state(sk, TCP_CLOSING);
        break;

    case TCP_FIN_WAIT2:
        /* Received a FIN -- enter TIME_WAIT */
        tcp_send_ack(sk);
        tcp_time_wait(sk, TCP_TIME_WAIT, 0);
        break;

    default:
        /* Only TCP_LISTEN and TCP_CLOSE left - impossible */
        printk(KERN_ERR "%s: Impossible, sk->sk_state=%d\n",
               __func__, sk->sk_state);
        break;
    }
}
```

中文说明：
- 使用switch语句实现事件特定的状态转换
- 每个case处理该状态下接收FIN的行为
- default分支用于捕获不可能的状态

---

## 3. Why Transitions Are NOT Centralized

### 3.1 The Alternative: Single Transition Table

```c
// HYPOTHETICAL - NOT USED IN LINUX
struct transition {
    int current_state;
    int event;
    int next_state;
    void (*action)(struct sock *);
};

static struct transition table[] = {
    {TCP_LISTEN,     EV_SYN,      TCP_SYN_RECV,   send_synack},
    {TCP_SYN_SENT,   EV_SYN_ACK,  TCP_ESTABLISHED, send_ack},
    {TCP_ESTABLISHED,EV_FIN,      TCP_CLOSE_WAIT, send_ack},
    // ... hundreds more entries ...
};

void process_event(struct sock *sk, int event) {
    for (int i = 0; i < ARRAY_SIZE(table); i++) {
        if (table[i].current_state == sk->sk_state &&
            table[i].event == event) {
            table[i].action(sk);
            sk->sk_state = table[i].next_state;
            return;
        }
    }
}
```

### 3.2 Why This Was Rejected

**Problem 1: Complex Actions**
```c
// Real FIN handling requires:
// - Schedule ACK
// - Set shutdown flags
// - Set SOCK_DONE flag
// - State-specific additional work
// - Wake up waiters
// - Possibly send immediate ACK
// A simple action pointer can't capture this
```

**Problem 2: Conditional Transitions**
```c
// FIN_WAIT1 -> FIN_WAIT2 requires:
if (tp->snd_una == tp->write_seq) {
    // Only transition if all data acknowledged
    tcp_set_state(sk, TCP_FIN_WAIT2);
}
// Can't express in simple table
```

**Problem 3: Performance**
```c
// Table lookup: O(n) or O(log n) with sorting
// Switch statement: O(1) via jump table

// With 11 states × ~20 events = 220 transitions
// Table approach adds measurable overhead
```

### 3.3 The Actual Design: Distributed Logic

```
+-------------------+     +-------------------+     +-------------------+
| tcp_close()       |     | tcp_fin()         |     | tcp_reset()       |
| User close event  |     | FIN received      |     | RST received      |
+-------------------+     +-------------------+     +-------------------+
         |                         |                         |
         v                         v                         v
+------------------------------------------------------------------+
|                   tcp_set_state(sk, new_state)                    |
|                   (centralized state change)                      |
+------------------------------------------------------------------+
         |                         |                         |
         v                         v                         v
+-------------------+     +-------------------+     +-------------------+
| Statistics update |     | Statistics update |     | Statistics update |
| Hash table update |     | Hash table update |     | Hash table update |
| Callback notify   |     | Callback notify   |     | Callback notify   |
+-------------------+     +-------------------+     +-------------------+
```

中文说明：
- 单一转换表无法表达复杂动作和条件转换
- 分布式逻辑允许每个事件处理器包含完整上下文
- `tcp_set_state()`集中处理状态变更的副作用

---

## 4. Entry Actions vs Transition Actions

### 4.1 Entry Actions (State-Specific Setup)

```c
// From tcp_rcv_state_process() - entering ESTABLISHED
case TCP_SYN_RECV:
    if (acceptable) {
        tp->copied_seq = tp->rcv_nxt;
        smp_mb();
        
        // === ENTRY ACTION: TCP_ESTABLISHED ===
        tcp_set_state(sk, TCP_ESTABLISHED);
        sk->sk_state_change(sk);

        if (sk->sk_socket)
            sk_wake_async(sk, SOCK_WAKE_IO, POLL_OUT);

        tp->snd_una = TCP_SKB_CB(skb)->ack_seq;
        tp->snd_wnd = ntohs(th->window) << tp->rx_opt.snd_wscale;
        tcp_init_wl(tp, TCP_SKB_CB(skb)->seq);

        icsk->icsk_af_ops->rebuild_header(sk);
        tcp_init_metrics(sk);
        tcp_init_congestion_control(sk);
        tp->lsndtime = tcp_time_stamp;
        tcp_mtup_init(sk);
        tcp_initialize_rcv_mss(sk);
        tcp_init_buffer_space(sk);
        tcp_fast_path_on(tp);
        // === END ENTRY ACTION ===
    }
    break;
```

### 4.2 Transition Actions (On-the-Wire)

```c
// From tcp.c tcp_close() - exiting current state
if (tcp_close_state(sk)) {
    // === TRANSITION ACTION ===
    tcp_send_fin(sk);  // Send FIN packet
    // === END TRANSITION ACTION ===
}

// tcp_close_state() handles the state change:
static int tcp_close_state(struct sock *sk)
{
    int next = (int)new_state[sk->sk_state];
    int ns = next & TCP_STATE_MASK;

    tcp_set_state(sk, ns);

    return next & TCP_ACTION_FIN;  // Returns whether to send FIN
}
```

### 4.3 The Transition Table with Action Flags

```c
static const unsigned char new_state[16] = {
  /* current state:        new state:      action: */
  /* (Invalid)      */ TCP_CLOSE,
  /* TCP_ESTABLISHED */ TCP_FIN_WAIT1 | TCP_ACTION_FIN,  // <-- Action encoded
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

#define TCP_STATE_MASK  0xF
#define TCP_ACTION_FIN  (1 << 7)
```

中文说明：
- 入口动作：进入新状态时需要的初始化
- 转换动作：状态变化时需要发送的协议消息
- `new_state[]`表用高位编码是否需要发送FIN

---

## 5. Handling Illegal/Unexpected Transitions

### 5.1 Default Case Handling

```c
// From tcp_fin()
default:
    /* Only TCP_LISTEN and TCP_CLOSE are left, in these
     * cases we should never reach this piece of code.
     */
    printk(KERN_ERR "%s: Impossible, sk->sk_state=%d\n",
           __func__, sk->sk_state);
    break;
```

### 5.2 Protocol Violation Response

```c
// From tcp_rcv_state_process()
case TCP_LISTEN:
    if (th->ack)
        return 1;  // Caller sends RST

// From tcp_v4_do_rcv()
if (tcp_rcv_state_process(sk, skb, tcp_hdr(skb), skb->len)) {
    rsk = sk;
    goto reset;  // Send RST
}
// ...
reset:
    tcp_v4_send_reset(rsk, skb);
```

### 5.3 Invalid State Protection via Assertions

```c
// From tcp.c tcp_poll()
if (sk->sk_state == TCP_LISTEN)
    return inet_csk_listen_poll(sk);

// From tcp_sendmsg()
if ((1 << sk->sk_state) & ~(TCPF_ESTABLISHED | TCPF_CLOSE_WAIT))
    if ((err = sk_stream_wait_connect(sk, &timeo)) != 0)
        goto out_err;

// Prevents sending data in wrong states
```

### 5.4 RST on Protocol Error

```c
// From tcp_validate_incoming()
/* step 4: Check for a SYN in window. */
if (th->syn && !before(TCP_SKB_CB(skb)->seq, tp->rcv_nxt)) {
    if (syn_inerr)
        TCP_INC_STATS_BH(sock_net(sk), TCP_MIB_INERRS);
    NET_INC_STATS_BH(sock_net(sk), LINUX_MIB_TCPABORTONSYN);
    tcp_reset(sk);  // Force connection abort
    return -1;
}
```

中文说明：
- 不可能的状态组合通过printk记录错误
- 协议违规通过发送RST响应
- 断言检查确保操作在有效状态执行
- 统计计数器跟踪异常情况

---

## 6. Complete State Transition Map

```
                                +---------+
              active OPEN       |  CLOSED |     passive OPEN
              -----------       +---------+     ------------
              snd SYN             |     \        create TCB
                                  |      \_________________________
                                  |                               |
                                  v                               v
                             +---------+                      +---------+
                             |SYN-SENT |                      | LISTEN  |
                             +---------+                      +---------+
                   rcv SYN      |     |      rcv SYN              |
                   -------      |     |      ------               |
                   snd SYN,ACK  |     |      snd SYN,ACK          |
                                |     |                           |
              +--------+        |     |        +--------+         |
              |        |<-------+     +------->|        |<--------+
              |SYN-RCVD|                       |SYN-RCVD|
              |        |                       |        |
              +--------+                       +--------+
                   |                                |
         rcv ACK   |                                | rcv ACK
         -------   |                                | -------
            x      |                                |    x
                   v                                v
              +-----------------------------------------+
              |              ESTABLISHED                |
              +-----------------------------------------+
                   |                           |
                   |  CLOSE                    |  rcv FIN
                   |  -----                    |  -------
                   |  snd FIN                  |  snd ACK
                   |                           |
                   v                           v
              +---------+                 +---------+
              |FIN-WAIT1|                 |CLOSE-   |
              +---------+                 |WAIT     |
       rcv ACK   |    | rcv FIN           +---------+
       -------   |    | -------                |
          x      |    | snd ACK      CLOSE     |
                 |    |              -----     |
                 v    v              snd FIN   |
              +---------+                      v
              |FIN-WAIT2|                 +---------+
              +---------+                 |LAST-ACK |
                   |                      +---------+
         rcv FIN   |                           |
         -------   |                           | rcv ACK
         snd ACK   |                           | -------
                   |                           |    x
                   v                           v
              +---------+                 +---------+
              |TIME-WAIT|   Timeout       | CLOSED  |
              +---------+---------------->+---------+
```

## 7. Summary

```
+------------------------------------------------------------------+
|                  TRANSITION LOGIC DESIGN                          |
+------------------------------------------------------------------+
|                                                                   |
|  CENTRALIZED:                                                     |
|    - tcp_set_state()     State change + side effects             |
|    - new_state[]         Close path transitions                  |
|    - Statistics/hash     Common cleanup                          |
|                                                                   |
|  DISTRIBUTED:                                                     |
|    - tcp_rcv_state_process()  Packet handling                    |
|    - tcp_fin()                FIN event handling                 |
|    - tcp_reset()              RST event handling                 |
|    - tcp_close()              User close handling                |
|                                                                   |
|  ERROR HANDLING:                                                  |
|    - Default cases        Log impossible states                  |
|    - Return codes         Trigger RST transmission               |
|    - Assertions           Prevent invalid operations             |
|                                                                   |
+------------------------------------------------------------------+
```

中文总结：
1. **核心调度器**：`tcp_rcv_state_process()`处理大多数状态的包处理
2. **两级分发**：初始状态检查 + 事件特定处理
3. **分布式逻辑**：复杂动作和条件转换需要上下文
4. **集中控制点**：`tcp_set_state()`处理状态变更副作用
5. **入口vs转换动作**：分别处理初始化和协议消息
6. **错误处理**：日志、RST响应、断言检查的组合
