# TCP FSM: Events & Inputs

## 1. Major Event Sources

The TCP FSM responds to events from **four primary sources**:

```
+------------------+     +------------------+     +------------------+
|  NETWORK LAYER   |     |   KERNEL TIMERS  |     |   USER SPACE     |
|  Incoming Pkts   |     |   (async fire)   |     |   Syscalls       |
+------------------+     +------------------+     +------------------+
        |                        |                        |
        v                        v                        v
+------------------------------------------------------------------+
|                         TCP FSM CORE                              |
|                                                                   |
|   tcp_v4_rcv()      tcp_*_timer()      tcp_sendmsg()             |
|   tcp_rcv_state_    tcp_retrans-       tcp_close()               |
|   process()         mit_timer()        tcp_connect()             |
+------------------------------------------------------------------+
        |                        |                        |
        v                        v                        v
+------------------+     +------------------+     +------------------+
|  ICMP ERRORS     |     |  MEMORY PRESSURE |     |  DEVICE EVENTS   |
|  (e.g., no route)|     |  (OOM handling)  |     |  (MTU changes)   |
+------------------+     +------------------+     +------------------+
```

---

## 2. Event Source #1: Incoming Packets

### 2.1 Entry Point: `tcp_v4_rcv()`

Location: `net/ipv4/tcp_ipv4.c`

```c
int tcp_v4_rcv(struct sk_buff *skb)
{
    const struct iphdr *iph;
    const struct tcphdr *th;
    struct sock *sk;
    
    // Validate packet
    if (skb->pkt_type != PACKET_HOST)
        goto discard_it;
    
    // Find matching socket
    sk = __inet_lookup_skb(&tcp_hashinfo, skb, th->source, th->dest);
    if (!sk)
        goto no_tcp_socket;

    // Handle TIME_WAIT specially
    if (sk->sk_state == TCP_TIME_WAIT)
        goto do_time_wait;

    // Lock socket and process
    bh_lock_sock_nested(sk);
    if (!sock_owned_by_user(sk)) {
        if (!tcp_prequeue(sk, skb))
            ret = tcp_v4_do_rcv(sk, skb);
    } else {
        sk_add_backlog(sk, skb);
    }
    bh_unlock_sock(sk);
    // ...
}
```

### 2.2 Packet Types and FSM Triggers

| Packet Flags | FSM Event | Typical Handler |
|--------------|-----------|-----------------|
| SYN only | Connection request | `tcp_rcv_state_process()` |
| SYN+ACK | Connection accepted | `tcp_rcv_synsent_state_process()` |
| ACK only | Data/window update | `tcp_ack()` |
| FIN | Close initiated | `tcp_fin()` |
| RST | Connection reset | `tcp_reset()` |
| Data (PSH) | Payload received | `tcp_data_queue()` |

### 2.3 Dispatcher: `tcp_v4_do_rcv()`

```c
int tcp_v4_do_rcv(struct sock *sk, struct sk_buff *skb)
{
    if (sk->sk_state == TCP_ESTABLISHED) {
        // FAST PATH: Most packets hit established connections
        sock_rps_save_rxhash(sk, skb);
        if (tcp_rcv_established(sk, skb, tcp_hdr(skb), skb->len))
            goto reset;
        return 0;
    }

    if (sk->sk_state == TCP_LISTEN) {
        // Handle incoming connections
        struct sock *nsk = tcp_v4_hnd_req(sk, skb);
        if (nsk != sk) {
            if (tcp_child_process(sk, nsk, skb))
                goto reset;
            return 0;
        }
    }

    // SLOW PATH: All other states
    if (tcp_rcv_state_process(sk, skb, tcp_hdr(skb), skb->len))
        goto reset;
    return 0;
}
```

中文说明：
- 数据包通过`tcp_v4_rcv()`进入TCP
- 根据套接字状态分流：ESTABLISHED走快速路径，LISTEN处理新连接
- 其他状态通过`tcp_rcv_state_process()`处理

---

## 3. Event Source #2: Timers

### 3.1 Timer Types

From `tcp_timer.c`:

```c
void tcp_init_xmit_timers(struct sock *sk)
{
    inet_csk_init_xmit_timers(sk, 
        &tcp_write_timer,     // Retransmission timer
        &tcp_delack_timer,    // Delayed ACK timer
        &tcp_keepalive_timer  // Keepalive timer
    );
}
```

### 3.2 Retransmission Timer

```c
void tcp_retransmit_timer(struct sock *sk)
{
    struct tcp_sock *tp = tcp_sk(sk);
    struct inet_connection_sock *icsk = inet_csk(sk);

    if (!tp->packets_out)
        goto out;

    // State-dependent behavior
    if (!tp->snd_wnd && !sock_flag(sk, SOCK_DEAD) &&
        !((1 << sk->sk_state) & (TCPF_SYN_SENT | TCPF_SYN_RECV))) {
        // Zero window probe handling...
    }

    // Timeout handling may change state
    if (tcp_write_timeout(sk))
        goto out;  // tcp_write_timeout() may call tcp_done()
    
    // Retransmit...
    tcp_retransmit_skb(sk, tcp_write_queue_head(sk));
}
```

### 3.3 Keepalive Timer

```c
static void tcp_keepalive_timer(unsigned long data)
{
    struct sock *sk = (struct sock *)data;
    
    // State check
    if (!sock_flag(sk, SOCK_KEEPOPEN) || 
        sk->sk_state == TCP_CLOSE)
        goto out;

    // FIN_WAIT_2 timeout handling
    if (sk->sk_state == TCP_FIN_WAIT2 && sock_flag(sk, SOCK_DEAD)) {
        if (tp->linger2 >= 0) {
            // May transition to TIME_WAIT
            tcp_time_wait(sk, TCP_FIN_WAIT2, tmo);
            goto out;
        }
        // Or send RST and close
        tcp_send_active_reset(sk, GFP_ATOMIC);
        goto death;
    }
    
    // Send keepalive probe
    if (icsk->icsk_probes_out >= keepalive_probes(tp)) {
        tcp_send_active_reset(sk, GFP_ATOMIC);
        tcp_write_err(sk);  // Terminates connection
        goto out;
    }
    
    tcp_write_wakeup(sk);
}
```

### 3.4 TIME_WAIT Timer

From `tcp_minisocks.c`:

```c
// TIME_WAIT is handled by inet_timewait_sock infrastructure
struct inet_timewait_death_row tcp_death_row = {
    .period     = TCP_TIMEWAIT_LEN / INET_TWDR_TWKILL_SLOTS,
    .death_lock = __SPIN_LOCK_UNLOCKED(tcp_death_row.death_lock),
    .hashinfo   = &tcp_hashinfo,
    .tw_timer   = TIMER_INITIALIZER(inet_twdr_hangman, 0,
                                    (unsigned long)&tcp_death_row),
    // ...
};
```

中文说明：
- TCP使用三类主要定时器：重传、延迟ACK、保活
- TIME_WAIT状态有专用定时器管理器
- 定时器处理函数检查当前状态并可能触发状态转换

---

## 4. Event Source #3: User-Space Syscalls

### 4.1 Connection Initiation: `connect()`

```c
// From af_inet.c
int inet_stream_connect(struct socket *sock, struct sockaddr *uaddr,
                        int addr_len, int flags)
{
    struct sock *sk = sock->sk;
    
    lock_sock(sk);

    switch (sock->state) {
    case SS_UNCONNECTED:
        if (sk->sk_state != TCP_CLOSE)
            goto out;

        // This triggers: CLOSED -> SYN_SENT
        err = sk->sk_prot->connect(sk, uaddr, addr_len);
        sock->state = SS_CONNECTING;
        break;
    }
    
    // Wait for connection (blocking)
    if ((1 << sk->sk_state) & (TCPF_SYN_SENT | TCPF_SYN_RECV)) {
        if (!timeo || !inet_wait_for_connect(sk, timeo))
            goto out;
    }
    
    release_sock(sk);
}

// From tcp_ipv4.c
int tcp_v4_connect(struct sock *sk, struct sockaddr *uaddr, int addr_len)
{
    // ...
    tcp_set_state(sk, TCP_SYN_SENT);  // STATE TRANSITION
    err = tcp_connect(sk);  // Send SYN packet
    // ...
}
```

### 4.2 Connection Close: `close()`

```c
// From tcp.c
void tcp_close(struct sock *sk, long timeout)
{
    lock_sock(sk);
    sk->sk_shutdown = SHUTDOWN_MASK;

    if (sk->sk_state == TCP_LISTEN) {
        tcp_set_state(sk, TCP_CLOSE);
        inet_csk_listen_stop(sk);
        goto adjudge_to_death;
    }

    // Flush receive queue
    while ((skb = __skb_dequeue(&sk->sk_receive_queue)) != NULL) {
        data_was_unread += len;
        __kfree_skb(skb);
    }

    if (data_was_unread) {
        // Unread data: send RST
        tcp_set_state(sk, TCP_CLOSE);
        tcp_send_active_reset(sk, sk->sk_allocation);
    } else if (sock_flag(sk, SOCK_LINGER) && !sk->sk_lingertime) {
        // Zero linger: abort
        sk->sk_prot->disconnect(sk, 0);
    } else if (tcp_close_state(sk)) {
        // Normal close: send FIN
        tcp_send_fin(sk);
    }

    release_sock(sk);
}
```

### 4.3 Data Send: `sendmsg()`

```c
int tcp_sendmsg(struct kiocb *iocb, struct sock *sk, struct msghdr *msg,
                size_t size)
{
    lock_sock(sk);

    // Wait for connection if needed
    if ((1 << sk->sk_state) & ~(TCPF_ESTABLISHED | TCPF_CLOSE_WAIT))
        if ((err = sk_stream_wait_connect(sk, &timeo)) != 0)
            goto out_err;

    // Can only send in ESTABLISHED or CLOSE_WAIT
    // (CLOSE_WAIT: received FIN but can still send)
    
    while (--iovlen >= 0) {
        // ... copy data to sk_buff ...
    }

    release_sock(sk);
    return copied;
}
```

中文说明：
- `connect()`：触发CLOSED→SYN_SENT转换
- `close()`：根据条件触发多种关闭路径（正常FIN/异常RST）
- `sendmsg()`：仅在ESTABLISHED或CLOSE_WAIT状态允许
- 所有系统调用都先获取套接字锁

---

## 5. Event Source #4: Errors and Resets

### 5.1 ICMP Error Handling

```c
// From tcp_ipv4.c
void tcp_v4_err(struct sk_buff *icmp_skb, u32 info)
{
    const struct iphdr *iph = (const struct iphdr *)icmp_skb->data;
    struct tcphdr *th = (struct tcphdr *)(icmp_skb->data + (iph->ihl << 2));
    struct sock *sk;

    sk = inet_lookup(net, &tcp_hashinfo, iph->daddr, th->dest,
                     iph->saddr, th->source, inet_iif(icmp_skb));
    if (!sk)
        return;

    switch (type) {
    case ICMP_DEST_UNREACH:
        if (code == ICMP_NET_UNREACH || code == ICMP_HOST_UNREACH) {
            if (sk->sk_state == TCP_SYN_SENT || 
                sk->sk_state == TCP_SYN_RECV)
                // Fatal during connection setup
                tcp_done(sk);
        }
        break;
    }
}
```

### 5.2 RST Packet Handling

```c
// From tcp_input.c
static void tcp_reset(struct sock *sk)
{
    switch (sk->sk_state) {
    case TCP_SYN_SENT:
        sk->sk_err = ECONNREFUSED;
        break;
    case TCP_CLOSE_WAIT:
        sk->sk_err = EPIPE;
        break;
    case TCP_CLOSE:
        return;
    default:
        sk->sk_err = ECONNRESET;
    }
    
    /* This barrier is coupled with smp_rmb() in tcp_poll() */
    smp_wmb();

    if (!sock_flag(sk, SOCK_DEAD))
        sk->sk_error_report(sk);

    tcp_done(sk);  // Transitions to CLOSED
}
```

---

## 6. How Events Enter the FSM

### 6.1 Event Flow Diagram

```
                    +-------------------+
                    |  Network Device   |
                    |  Interrupt        |
                    +-------------------+
                            |
                            v
                    +-------------------+
                    |  NAPI / softirq   |
                    |  ip_rcv()         |
                    +-------------------+
                            |
                            v
                    +-------------------+
                    |  tcp_v4_rcv()     |
                    |  (socket lookup)  |
                    +-------------------+
                            |
            +---------------+---------------+
            |               |               |
            v               v               v
    +-------------+ +-------------+ +-------------+
    | ESTABLISHED | |   LISTEN    | | Other States|
    | Fast Path   | | Connection  | | Slow Path   |
    +-------------+ +-------------+ +-------------+
            |               |               |
            v               v               v
    +-------------+ +-------------+ +-------------+
    |tcp_rcv_     | |tcp_v4_hnd_  | |tcp_rcv_     |
    |established()| |req()        | |state_       |
    +-------------+ +-------------+ |process()    |
                                    +-------------+
```

### 6.2 FSM Dispatcher Functions

| Function | Handles States | Location |
|----------|---------------|----------|
| `tcp_rcv_established()` | ESTABLISHED only | tcp_input.c |
| `tcp_rcv_state_process()` | All except ESTABLISHED, TIME_WAIT | tcp_input.c |
| `tcp_timewait_state_process()` | TIME_WAIT, FIN_WAIT2 (mini) | tcp_minisocks.c |
| `tcp_rcv_synsent_state_process()` | SYN_SENT only | tcp_input.c |

中文说明：
- 事件通过软中断从网络设备进入
- `tcp_v4_rcv()`查找套接字并根据状态分流
- 快速路径处理ESTABLISHED状态（最常见）
- 慢速路径通过`tcp_rcv_state_process()`处理其他状态

---

## 7. Why TCP is Event-Driven (Not Loop-Driven)

### 7.1 The Alternative: Polling/Loop Model

```c
// HYPOTHETICAL BAD DESIGN - DO NOT USE
void tcp_main_loop(struct sock *sk) {
    while (connection_active) {
        poll_for_packets();    // Waste CPU cycles
        check_timers();        // Duplicate timer infrastructure
        check_user_requests(); // How to integrate with syscalls?
        sleep(1ms);           // Latency vs CPU tradeoff
    }
}
```

Problems:
- **CPU waste**: Constant polling even when idle
- **Latency**: Polling interval adds delay
- **Integration**: Doesn't fit kernel event model
- **Scalability**: 10000 connections = 10000 threads/loops

### 7.2 Event-Driven Advantages

```c
// ACTUAL DESIGN
// Events arrive asynchronously:
// - Packets: interrupt → softirq → tcp_v4_rcv()
// - Timers: timer wheel → tcp_*_timer()
// - Syscalls: user context → tcp_*()

// Each event handler:
// 1. Acquires lock
// 2. Checks current state
// 3. Performs action
// 4. Updates state
// 5. Releases lock
```

Benefits:
- **Zero idle overhead**: No work when nothing happening
- **Minimal latency**: Process immediately when event arrives
- **Kernel integration**: Uses existing event infrastructure
- **Scalability**: Single event queue, not per-connection threads

---

## 8. Event Ordering Preservation

### 8.1 Per-Socket Serialization

```c
// From tcp_v4_rcv()
bh_lock_sock_nested(sk);
if (!sock_owned_by_user(sk)) {
    // Process immediately in softirq
    ret = tcp_v4_do_rcv(sk, skb);
} else {
    // User holds lock - queue to backlog
    sk_add_backlog(sk, skb);
}
bh_unlock_sock(sk);
```

### 8.2 Backlog Processing

```c
// When user releases lock:
void release_sock(struct sock *sk)
{
    if (sk->sk_backlog.tail) {
        // Process queued packets in order
        __release_sock(sk);
    }
    // ...
}

static void __release_sock(struct sock *sk)
{
    struct sk_buff *skb = sk->sk_backlog.head;
    while (skb) {
        struct sk_buff *next = skb->next;
        sk_backlog_rcv(sk, skb);  // Process each packet
        skb = next;
    }
}
```

### 8.3 Sequence Number Ordering

```c
// TCP ensures data ordering via sequence numbers, not packet arrival order
if (TCP_SKB_CB(skb)->seq == tp->rcv_nxt) {
    // In sequence - process immediately
    tcp_data_queue(sk, skb);
} else if (after(TCP_SKB_CB(skb)->seq, tp->rcv_nxt)) {
    // Out of order - queue for later
    tcp_ofo_queue(sk);
}
```

中文说明：
- 每个套接字使用锁进行串行化
- 当用户持有锁时，包被放入backlog队列
- 释放锁时按顺序处理backlog
- TCP序列号确保数据顺序，而非包到达顺序

---

## 9. Summary: Event Architecture

```
+------------------------------------------------------------------+
|                     TCP EVENT ARCHITECTURE                        |
+------------------------------------------------------------------+
|                                                                   |
|  EVENT SOURCES:                                                   |
|  +---------------+  +---------------+  +---------------+          |
|  | Network Pkts  |  | Kernel Timers |  | User Syscalls |          |
|  | (interrupt)   |  | (timer wheel) |  | (process ctx) |          |
|  +---------------+  +---------------+  +---------------+          |
|         |                  |                  |                   |
|         v                  v                  v                   |
|  +----------------------------------------------------------+    |
|  |               SOCKET LOCK (serialization)                |    |
|  +----------------------------------------------------------+    |
|         |                  |                  |                   |
|         v                  v                  v                   |
|  +----------------------------------------------------------+    |
|  |                   FSM DISPATCHER                         |    |
|  |  - tcp_rcv_state_process() (main)                       |    |
|  |  - tcp_rcv_established()   (fast path)                  |    |
|  |  - tcp_*_timer()           (timer events)               |    |
|  +----------------------------------------------------------+    |
|                            |                                      |
|                            v                                      |
|  +----------------------------------------------------------+    |
|  |              STATE + ACTION OUTPUT                       |    |
|  +----------------------------------------------------------+    |
|                                                                   |
+------------------------------------------------------------------+
```

中文总结：
1. **四类事件源**：网络包、定时器、系统调用、错误通知
2. **事件驱动模型**：无空闲开销，最小延迟，高可扩展性
3. **调度器分流**：快速路径处理ESTABLISHED，慢速路径处理其他状态
4. **顺序保证**：套接字锁串行化，backlog队列保序
5. **状态感知处理**：每个处理函数根据当前状态决定行为
