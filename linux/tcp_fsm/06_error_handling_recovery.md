# TCP FSM: Error Handling & Recovery

## 1. Protocol Violation Handling

### 1.1 Categories of Protocol Violations

```
+------------------------------------------------------------------+
|                    PROTOCOL VIOLATIONS                            |
+------------------------------------------------------------------+
|                                                                   |
|  TYPE 1: Invalid Packet in State                                 |
|    Example: ACK received in LISTEN state                         |
|    Response: Send RST                                            |
|                                                                   |
|  TYPE 2: Sequence Number Violations                              |
|    Example: Data outside receive window                          |
|    Response: Send ACK with correct window                        |
|                                                                   |
|  TYPE 3: Invalid Flag Combinations                               |
|    Example: SYN in established connection window                 |
|    Response: Send RST, abort connection                          |
|                                                                   |
|  TYPE 4: Resource Exhaustion                                     |
|    Example: Too many orphan sockets                              |
|    Response: Send RST, force close                               |
|                                                                   |
+------------------------------------------------------------------+
```

### 1.2 RST (Reset) Handling

From `tcp_input.c`:

```c
static void tcp_reset(struct sock *sk)
{
    /* Set appropriate error based on current state */
    switch (sk->sk_state) {
    case TCP_SYN_SENT:
        sk->sk_err = ECONNREFUSED;  // Connection refused
        break;
    case TCP_CLOSE_WAIT:
        sk->sk_err = EPIPE;         // Broken pipe
        break;
    case TCP_CLOSE:
        return;                      // Already closed, ignore
    default:
        sk->sk_err = ECONNRESET;    // Connection reset
    }

    /* This barrier is coupled with smp_rmb() in tcp_poll() */
    smp_wmb();

    if (!sock_flag(sk, SOCK_DEAD))
        sk->sk_error_report(sk);    // Notify application

    tcp_done(sk);                   // Clean up connection
}
```

### 1.3 Sending RST on Violations

```c
// From tcp_rcv_state_process()
case TCP_LISTEN:
    if (th->ack)
        return 1;  // Signals caller to send RST

// From tcp_v4_do_rcv()
if (tcp_rcv_state_process(sk, skb, tcp_hdr(skb), skb->len)) {
    rsk = sk;
    goto reset;
}
// ...
reset:
    tcp_v4_send_reset(rsk, skb);  // Send RST packet
```

### 1.4 Validation Chain

```c
// From tcp_validate_incoming()
static int tcp_validate_incoming(struct sock *sk, struct sk_buff *skb,
                                  const struct tcphdr *th, int syn_inerr)
{
    struct tcp_sock *tp = tcp_sk(sk);

    /* Step 1: Check sequence number */
    if (!tcp_sequence(tp, TCP_SKB_CB(skb)->seq, TCP_SKB_CB(skb)->end_seq)) {
        if (!th->rst)
            tcp_send_dupack(sk, skb);  // Send duplicate ACK
        goto discard;
    }

    /* Step 2: Check RST bit */
    if (th->rst) {
        tcp_reset(sk);
        goto discard;
    }

    /* Step 3: Check PAWS (Protection Against Wrapped Sequences) */
    if (tcp_paws_discard(sk, skb))
        goto discard;

    /* Step 4: Check for SYN in window (attack detection) */
    if (th->syn && !before(TCP_SKB_CB(skb)->seq, tp->rcv_nxt)) {
        NET_INC_STATS_BH(sock_net(sk), LINUX_MIB_TCPABORTONSYN);
        tcp_reset(sk);
        return -1;
    }

    return 1;  // Packet is valid
    
discard:
    __kfree_skb(skb);
    return 0;
}
```

中文说明：
- 协议违规分为四类：状态无效、序列号违规、标志组合无效、资源耗尽
- RST处理根据当前状态设置适当的错误码
- `tcp_validate_incoming()`执行多步验证链

---

## 2. Error States: Explicit vs Implicit

### 2.1 Explicit Error Indicators

```c
// Socket error field
sk->sk_err = ECONNRESET;

// Error reporting callback
sk->sk_error_report(sk);

// Shutdown flags
sk->sk_shutdown |= RCV_SHUTDOWN;  // Can't receive more
sk->sk_shutdown |= SEND_SHUTDOWN; // Can't send more

// Socket flag
sock_set_flag(sk, SOCK_DEAD);     // Socket orphaned
sock_set_flag(sk, SOCK_DONE);     // Connection complete
```

### 2.2 Error Detection via State

```c
// Application checks via getsockopt(SO_ERROR)
int tcp_getsockopt(...) {
    if (optname == SO_ERROR) {
        err = sock_error(sk);  // Returns and clears sk->sk_err
    }
}

// Or via read/write failures
ssize_t tcp_recvmsg(...) {
    if (sk->sk_err) {
        copied = sock_error(sk);  // Return error
        break;
    }
}
```

### 2.3 State-Based Error Recovery

```c
// Connection timeout handling
static void tcp_write_err(struct sock *sk)
{
    sk->sk_err = sk->sk_err_soft ? : ETIMEDOUT;
    sk->sk_error_report(sk);

    tcp_done(sk);  // Transition to CLOSED
    NET_INC_STATS_BH(sock_net(sk), LINUX_MIB_TCPABORTONTIMEOUT);
}
```

---

## 3. TIME_WAIT: State Not Just Timer

### 3.1 Why TIME_WAIT is a State

```
                    PURPOSE OF TIME_WAIT
+------------------------------------------------------------------+
|                                                                   |
|  REASON 1: Reliable Connection Termination                       |
|    - If our final ACK is lost, peer will retransmit FIN          |
|    - We must be able to retransmit ACK                           |
|    - Requires state to remember connection parameters            |
|                                                                   |
|  REASON 2: Old Duplicate Segment Prevention                      |
|    - Prevent old segments from being accepted in new connection  |
|    - Must wait 2*MSL (Maximum Segment Lifetime)                  |
|    - Can't just use timer - need to reject packets properly      |
|                                                                   |
|  REASON 3: Port Reuse Prevention                                 |
|    - Prevent immediate reuse of same 4-tuple                     |
|    - State in hash table blocks new connection with same tuple   |
|                                                                   |
+------------------------------------------------------------------+
```

### 3.2 TIME_WAIT Implementation

From `tcp_minisocks.c`:

```c
/*
 * Main purpose of TIME-WAIT state is to close connection gracefully,
 * when one of ends sits in LAST-ACK or CLOSING retransmitting FIN
 * (and, probably, tail of data) and one or more our ACKs are lost.
 *
 * What is TIME-WAIT timeout? It is associated with maximal packet
 * lifetime in the internet, which results in wrong conclusion, that
 * it is set to catch "old duplicate segments" wandering out of their path.
 * It is not quite correct. This timeout is calculated so that it exceeds
 * maximal retransmission timeout enough to allow to lose one (or more)
 * segments sent by peer and our ACKs.
 */

enum tcp_tw_status
tcp_timewait_state_process(struct inet_timewait_sock *tw, struct sk_buff *skb,
                           const struct tcphdr *th)
{
    struct tcp_timewait_sock *tcptw = tcp_twsk((struct sock *)tw);
    int paws_reject = 0;

    // Handle FIN_WAIT2 substate
    if (tw->tw_substate == TCP_FIN_WAIT2) {
        /* Just repeat all the checks of tcp_rcv_state_process() */

        if (paws_reject ||
            !tcp_in_window(TCP_SKB_CB(skb)->seq, TCP_SKB_CB(skb)->end_seq,
                           tcptw->tw_rcv_nxt,
                           tcptw->tw_rcv_nxt + tcptw->tw_rcv_wnd))
            return TCP_TW_ACK;

        if (th->rst)
            goto kill;

        if (th->syn && !before(TCP_SKB_CB(skb)->seq, tcptw->tw_rcv_nxt))
            goto kill_with_rst;

        /* FIN arrived, enter true time-wait state. */
        if (th->fin) {
            tw->tw_substate = TCP_TIME_WAIT;
            tcptw->tw_rcv_nxt = TCP_SKB_CB(skb)->end_seq;
        }
        // ...
    }

    // Full TIME_WAIT handling
    if (th->syn) {
        // Check if this is a valid new connection attempt
        if (!paws_reject &&
            (TCP_SKB_CB(skb)->seq == tcptw->tw_rcv_nxt &&
             (TCP_SKB_CB(skb)->seq == TCP_SKB_CB(skb)->end_seq || th->rst))) {
            // Allow connection reuse
            inet_twsk_deschedule(tw, &tcp_death_row);
            inet_twsk_put(tw);
            return TCP_TW_SYN;  // Signal: accept new SYN
        }
    }

    return TCP_TW_ACK;  // Default: send ACK

kill_with_rst:
    inet_twsk_deschedule(tw, &tcp_death_row);
    inet_twsk_put(tw);
    return TCP_TW_RST;

kill:
    inet_twsk_deschedule(tw, &tcp_death_row);
    inet_twsk_put(tw);
    return TCP_TW_SUCCESS;
}
```

### 3.3 TIME_WAIT as Memory-Optimized State

```c
// Full socket: ~600 bytes
struct sock { ... };
struct tcp_sock { ... };

// TIME_WAIT socket: ~240 bytes  
struct inet_timewait_sock {
    struct sock_common  __tw_common;
    int                 tw_timeout;
    volatile unsigned char tw_substate;
    // Only essential fields...
};

struct tcp_timewait_sock {
    struct inet_timewait_sock tw_sk;
    u32                       tw_rcv_nxt;
    u32                       tw_snd_nxt;
    u32                       tw_rcv_wnd;
    u32                       tw_ts_recent;
    long                      tw_ts_recent_stamp;
};
```

中文说明：
- TIME_WAIT是状态而非仅仅定时器，因为需要：
  1. 可靠地重传最后的ACK
  2. 阻止旧段被新连接接受
  3. 防止端口立即重用
- 使用精简的`inet_timewait_sock`结构节省内存
- 仍然响应数据包（发送ACK或允许新SYN）

---

## 4. Orphan Socket Handling

### 4.1 What is an Orphan?

```c
// Orphan = socket without user-space file descriptor
// Created when:
// - Application calls close() but connection not finished
// - FIN sent, waiting for peer's FIN
// - In FIN_WAIT1, FIN_WAIT2, CLOSING, etc.

void tcp_close(struct sock *sk, long timeout)
{
    // ...
    
    sock_orphan(sk);  // Disconnect from file descriptor
    
    // Socket continues to exist until connection fully closed
    // But consumes kernel resources without user control
}
```

### 4.2 Orphan Resource Limits

```c
// From tcp_timer.c
static int tcp_out_of_resources(struct sock *sk, int do_reset)
{
    struct tcp_sock *tp = tcp_sk(sk);
    int shift = 0;

    /* Penalize long-idle connections */
    if ((s32)(tcp_time_stamp - tp->lsndtime) > 2*TCP_RTO_MAX || !do_reset)
        shift++;

    /* Penalize if we received dubious ICMP */
    if (sk->sk_err_soft)
        shift++;

    if (tcp_too_many_orphans(sk, shift)) {
        if (net_ratelimit())
            printk(KERN_INFO "Out of socket memory\n");

        /* Force close with RST */
        if ((s32)(tcp_time_stamp - tp->lsndtime) <= TCP_TIMEWAIT_LEN ||
            (!tp->snd_wnd && !tp->packets_out))
            do_reset = 1;
            
        if (do_reset)
            tcp_send_active_reset(sk, GFP_ATOMIC);
            
        tcp_done(sk);
        NET_INC_STATS_BH(sock_net(sk), LINUX_MIB_TCPABORTONMEMORY);
        return 1;
    }
    return 0;
}
```

### 4.3 FIN_WAIT2 Timeout

```c
// From tcp_timer.c tcp_keepalive_timer()
if (sk->sk_state == TCP_FIN_WAIT2 && sock_flag(sk, SOCK_DEAD)) {
    if (tp->linger2 >= 0) {
        const int tmo = tcp_fin_time(sk) - TCP_TIMEWAIT_LEN;

        if (tmo > 0) {
            tcp_time_wait(sk, TCP_FIN_WAIT2, tmo);
            goto out;
        }
    }
    tcp_send_active_reset(sk, GFP_ATOMIC);
    goto death;
}
```

---

## 5. Connection Recovery Mechanisms

### 5.1 Retransmission on Timeout

```c
void tcp_retransmit_timer(struct sock *sk)
{
    struct tcp_sock *tp = tcp_sk(sk);

    if (!tp->packets_out)
        goto out;

    // Check if we should give up
    if (tcp_write_timeout(sk))
        goto out;

    // Perform retransmission
    if (tcp_retransmit_skb(sk, tcp_write_queue_head(sk)) > 0) {
        // Retransmit failed - try again later
        inet_csk_reset_xmit_timer(sk, ICSK_TIME_RETRANS,
                                  min(icsk->icsk_rto, TCP_RESOURCE_PROBE_INTERVAL),
                                  TCP_RTO_MAX);
        goto out;
    }

    // Exponential backoff
    icsk->icsk_backoff++;
    icsk->icsk_retransmits++;

    // Schedule next retry
    inet_csk_reset_xmit_timer(sk, ICSK_TIME_RETRANS,
                              icsk->icsk_rto, TCP_RTO_MAX);
}
```

### 5.2 Connection Abort Conditions

```c
// Maximum retransmissions exceeded
if (retransmits_timed_out(sk, retry_until, syn_set ? 0 : icsk->icsk_user_timeout, syn_set)) {
    tcp_write_err(sk);  // Sets ETIMEDOUT and closes
    return 1;
}

// Too many orphans
if (tcp_too_many_orphans(sk, 0)) {
    tcp_set_state(sk, TCP_CLOSE);
    tcp_send_active_reset(sk, GFP_ATOMIC);
    NET_INC_STATS_BH(sock_net(sk), LINUX_MIB_TCPABORTONMEMORY);
}

// Memory pressure
if (!sk_stream_memory_free(sk)) {
    // May need to abort if can't recover
}
```

---

## 6. Error Statistics and Monitoring

### 6.1 MIB Counters

```c
// Various abort reasons tracked
NET_INC_STATS_BH(sock_net(sk), LINUX_MIB_TCPABORTONTIMEOUT);
NET_INC_STATS_BH(sock_net(sk), LINUX_MIB_TCPABORTONMEMORY);
NET_INC_STATS_BH(sock_net(sk), LINUX_MIB_TCPABORTONCLOSE);
NET_INC_STATS_BH(sock_net(sk), LINUX_MIB_TCPABORTONDATA);
NET_INC_STATS_BH(sock_net(sk), LINUX_MIB_TCPABORTONLINGER);
NET_INC_STATS_BH(sock_net(sk), LINUX_MIB_TCPABORTONSYN);
```

### 6.2 Accessible via /proc

```bash
$ cat /proc/net/netstat | grep -i abort
TcpExt: ... TCPAbortOnTimeout 5 TCPAbortOnData 12 ...

$ cat /proc/net/tcp
sl  local_address rem_address   st tx_queue rx_queue ...
0:  0100007F:0050 00000000:0000 0A ...   # 0A = TCP_LISTEN
```

---

## 7. Summary: Error Handling Architecture

```
+------------------------------------------------------------------+
|                   ERROR HANDLING LAYERS                           |
+------------------------------------------------------------------+
|                                                                   |
|  LAYER 1: Packet Validation                                       |
|    - tcp_validate_incoming()                                     |
|    - Sequence number checks                                      |
|    - Flag validation                                             |
|    - PAWS checking                                               |
|                                                                   |
|  LAYER 2: Protocol Responses                                      |
|    - tcp_reset()  - Handle RST                                   |
|    - tcp_send_dupack() - Signal sequence issues                  |
|    - tcp_v4_send_reset() - Send RST on violations                |
|                                                                   |
|  LAYER 3: State Cleanup                                           |
|    - tcp_done() - Transition to CLOSED                           |
|    - tcp_time_wait() - Enter TIME_WAIT                           |
|    - inet_csk_destroy_sock() - Free resources                    |
|                                                                   |
|  LAYER 4: Resource Management                                     |
|    - Orphan socket limits                                        |
|    - Memory pressure handling                                    |
|    - FIN_WAIT2 timeouts                                          |
|                                                                   |
|  LAYER 5: Monitoring                                              |
|    - MIB counters                                                |
|    - /proc/net/tcp                                               |
|    - sk->sk_err propagation                                      |
|                                                                   |
+------------------------------------------------------------------+
```

中文总结：
1. **协议违规处理**：四类违规有不同的响应策略（RST/ACK/忽略）
2. **TIME_WAIT是状态**：不仅是定时器，需要响应数据包和阻止端口重用
3. **孤儿套接字管理**：限制资源消耗，必要时强制RST关闭
4. **错误传播**：sk_err字段 + 回调通知应用程序
5. **恢复机制**：重传、指数退避、最终超时关闭
6. **监控统计**：MIB计数器通过/proc可见
