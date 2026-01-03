# TCP FSM: Concurrency & Locking

## 1. The Concurrency Challenge

TCP must handle concurrent access from:

```
+------------------+     +------------------+     +------------------+
|   User Process   |     |   Softirq/BH     |     |   Timer IRQ      |
|   (syscalls)     |     |   (packets)      |     |   (timeouts)     |
+------------------+     +------------------+     +------------------+
        |                        |                        |
        |   sendmsg()            |   tcp_v4_rcv()         |   tcp_*_timer()
        |   close()              |                        |
        v                        v                        v
+------------------------------------------------------------------+
|                        SAME SOCKET                                |
|                        sk->sk_state                               |
+------------------------------------------------------------------+
```

**Without synchronization**:
- User calls `close()` → sets state to `FIN_WAIT1`
- Simultaneously, packet arrives → reads state as `ESTABLISHED`
- Race condition: FIN might not be sent, connection hangs

---

## 2. The Socket Lock Architecture

### 2.1 Lock Structure

From `include/net/sock.h`:

```c
typedef struct {
    spinlock_t      slock;      // Hardware spinlock
    int             owned;      // Software ownership flag
    wait_queue_head_t wq;       // Waiters for the lock
#ifdef CONFIG_DEBUG_LOCK_ALLOC
    struct lockdep_map dep_map;
#endif
} socket_lock_t;

struct sock {
    // ...
    socket_lock_t   sk_lock;
    // ...
};
```

### 2.2 Two-Level Locking

```
                    OWNER HELD
                    (owned = 1)
                         |
    +--------------------+--------------------+
    |                                         |
    v                                         v
+------------------+                   +------------------+
| User Context     |                   | Bottom Half      |
| lock_sock(sk)    |                   | bh_lock_sock(sk) |
| release_sock(sk) |                   | bh_unlock_sock() |
+------------------+                   +------------------+
    |                                         |
    | Sets owned=1                            | Checks owned
    | Blocks others                           | If owned: backlog
    |                                         |
    v                                         v
+------------------+                   +------------------+
| Full processing  |                   | Full or Backlog  |
+------------------+                   +------------------+
```

---

## 3. Where Locking Occurs Relative to State Transitions

### 3.1 User Context: `lock_sock()` / `release_sock()`

```c
// From tcp.c tcp_close()
void tcp_close(struct sock *sk, long timeout)
{
    lock_sock(sk);  // <-- ACQUIRE LOCK
    
    sk->sk_shutdown = SHUTDOWN_MASK;

    if (sk->sk_state == TCP_LISTEN) {
        tcp_set_state(sk, TCP_CLOSE);  // STATE TRANSITION
        inet_csk_listen_stop(sk);
        goto adjudge_to_death;
    }

    // ... more state transitions ...
    
    release_sock(sk);  // <-- RELEASE LOCK
}
```

```c
// From tcp.c tcp_sendmsg()
int tcp_sendmsg(struct kiocb *iocb, struct sock *sk, ...)
{
    lock_sock(sk);  // <-- ACQUIRE

    // Check state with lock held
    if ((1 << sk->sk_state) & ~(TCPF_ESTABLISHED | TCPF_CLOSE_WAIT))
        if ((err = sk_stream_wait_connect(sk, &timeo)) != 0)
            goto out_err;

    // ... send data ...

    release_sock(sk);  // <-- RELEASE
    return copied;
}
```

### 3.2 Bottom Half Context: `bh_lock_sock()`

```c
// From tcp_ipv4.c tcp_v4_rcv()
int tcp_v4_rcv(struct sk_buff *skb)
{
    // ... socket lookup ...
    
    bh_lock_sock_nested(sk);  // <-- TRY ACQUIRE
    
    ret = 0;
    if (!sock_owned_by_user(sk)) {
        // Lock acquired: process immediately
        if (!tcp_prequeue(sk, skb))
            ret = tcp_v4_do_rcv(sk, skb);
    } else if (unlikely(sk_add_backlog(sk, skb))) {
        // User holds lock: queue to backlog
        bh_unlock_sock(sk);
        goto discard_and_relse;
    }
    
    bh_unlock_sock(sk);  // <-- RELEASE
    sock_put(sk);
    return ret;
}
```

### 3.3 Timer Context

```c
// From tcp_timer.c
static void tcp_write_timer(unsigned long data)
{
    struct sock *sk = (struct sock *)data;

    bh_lock_sock(sk);  // <-- ACQUIRE
    
    if (!sock_owned_by_user(sk)) {
        tcp_write_timer_handler(sk);
    } else {
        // Reschedule if user holds lock
        sk_reset_timer(sk, &icsk->icsk_retransmit_timer,
                       jiffies + (HZ / 20));
    }
    
    bh_unlock_sock(sk);  // <-- RELEASE
    sock_put(sk);
}
```

中文说明：
- 用户上下文使用`lock_sock()`/`release_sock()`
- 底半部使用`bh_lock_sock()`
- 定时器上下文也使用`bh_lock_sock()`
- 状态转换只在持有锁时发生

---

## 4. Race Avoidance Mechanisms

### 4.1 The Backlog Queue

When bottom half can't acquire the lock:

```c
// From tcp_v4_rcv()
if (!sock_owned_by_user(sk)) {
    ret = tcp_v4_do_rcv(sk, skb);  // Process now
} else {
    sk_add_backlog(sk, skb);       // Queue for later
}
```

Backlog is processed when user releases lock:

```c
// From sock.c
void release_sock(struct sock *sk)
{
    spin_lock_bh(&sk->sk_lock.slock);
    
    if (sk->sk_backlog.tail) {
        __release_sock(sk);  // Process backlog
    }
    
    sk->sk_lock.owned = 0;
    // Wake up waiters...
    spin_unlock_bh(&sk->sk_lock.slock);
}

static void __release_sock(struct sock *sk)
{
    struct sk_buff *skb = sk->sk_backlog.head;

    do {
        sk->sk_backlog.head = sk->sk_backlog.tail = NULL;
        spin_unlock_bh(&sk->sk_lock.slock);

        do {
            struct sk_buff *next = skb->next;
            skb->next = NULL;
            sk_backlog_rcv(sk, skb);  // Process each packet
            skb = next;
        } while (skb != NULL);

        spin_lock_bh(&sk->sk_lock.slock);
    } while (sk->sk_backlog.tail);
}
```

### 4.2 Memory Barriers for Lock-Free Reads

```c
// From tcp.c tcp_poll() - read without lock
unsigned int tcp_poll(struct file *file, struct socket *sock, poll_table *wait)
{
    struct sock *sk = sock->sk;
    
    if (sk->sk_state == TCP_LISTEN)  // <-- Lock-free read
        return inet_csk_listen_poll(sk);

    // ...

    /* This barrier is coupled with smp_wmb() in tcp_reset() */
    smp_rmb();
    if (sk->sk_err)
        mask |= POLLERR;

    return mask;
}

// From tcp_input.c tcp_reset() - corresponding write barrier
static void tcp_reset(struct sock *sk)
{
    switch (sk->sk_state) {
    case TCP_SYN_SENT:
        sk->sk_err = ECONNREFUSED;
        break;
    // ...
    }
    
    /* This barrier is coupled with smp_rmb() in tcp_poll() */
    smp_wmb();

    if (!sock_flag(sk, SOCK_DEAD))
        sk->sk_error_report(sk);

    tcp_done(sk);
}
```

### 4.3 Volatile State Field

```c
// From sock.h
struct sock_common {
    // ...
    volatile unsigned char  skc_state;  // <-- volatile for lock-free reads
    // ...
};
```

中文说明：
- Backlog队列允许底半部在用户持锁时排队数据包
- 内存屏障确保跨CPU的状态可见性
- volatile确保编译器不优化掉状态读取

---

## 5. Execution Context Assumptions

### 5.1 User Context Properties

```c
// Can sleep: YES
// Preemptible: YES (unless in atomic section)
// Runs on: one CPU at a time per thread

void tcp_close(struct sock *sk, long timeout)
{
    lock_sock(sk);  // May sleep if contended
    
    // Can use blocking waits
    sk_stream_wait_close(sk, timeout);  // May sleep
    
    release_sock(sk);
}
```

### 5.2 Softirq/BH Context Properties

```c
// Can sleep: NO
// Preemptible: NO
// Runs on: any CPU, may run concurrently

int tcp_v4_do_rcv(struct sock *sk, struct sk_buff *skb)
{
    // No sleeping operations allowed
    // No lock_sock() - use bh_lock_sock()
    
    if (sk->sk_state == TCP_ESTABLISHED) {
        if (tcp_rcv_established(sk, skb, tcp_hdr(skb), skb->len)) {
            // ...
        }
    }
}
```

### 5.3 Timer Context Properties

```c
// Can sleep: NO
// Preemptible: NO (hardirq or softirq)
// May be deferred if lock held

static void tcp_keepalive_timer(unsigned long data)
{
    struct sock *sk = (struct sock *)data;

    bh_lock_sock(sk);
    
    if (sock_owned_by_user(sk)) {
        // Can't process now - reschedule
        inet_csk_reset_keepalive_timer(sk, HZ / 20);
        goto out;
    }

    // Process keepalive...
    
out:
    bh_unlock_sock(sk);
    sock_put(sk);
}
```

---

## 6. Comparison: Kernel vs User-Space FSM

### 6.1 Kernel TCP FSM Constraints

| Aspect | Kernel TCP | Reason |
|--------|------------|--------|
| Sleeping in handlers | NO (BH context) | Softirq can't block |
| Lock type | spinlock + owner flag | Low latency required |
| Memory allocation | GFP_ATOMIC | Can't sleep |
| Preemption | Disabled in BH | Atomicity |
| SMP awareness | Full | Multi-CPU systems |

### 6.2 User-Space FSM Simplifications

```c
// User-space can be simpler:
typedef struct {
    pthread_mutex_t lock;  // Sleeping mutex OK
    int state;
    // ...
} connection_t;

void handle_event(connection_t *conn, int event) {
    pthread_mutex_lock(&conn->lock);  // Can sleep
    
    switch (conn->state) {
    case STATE_ESTABLISHED:
        if (event == EV_FIN) {
            conn->state = STATE_CLOSE_WAIT;
            // Blocking I/O OK
            send_ack(conn);
        }
        break;
    }
    
    pthread_mutex_unlock(&conn->lock);
}
```

---

## 7. Lock Ordering and Deadlock Prevention

### 7.1 Lock Hierarchy

```
                    +-------------------+
                    | Global hash locks |  (tcp_hashinfo)
                    +-------------------+
                            |
                            v
                    +-------------------+
                    |   Socket lock     |  (sk->sk_lock)
                    +-------------------+
                            |
                            v
                    +-------------------+
                    |  Timer locks      |  (per-timer)
                    +-------------------+
```

### 7.2 Safe Locking Patterns

```c
// CORRECT: Acquire socket lock before hash operations
lock_sock(sk);
inet_hash_connect(&tcp_death_row, sk);  // Acquires hash lock internally
release_sock(sk);

// CORRECT: Timer handler checks ownership
bh_lock_sock(sk);
if (!sock_owned_by_user(sk)) {
    // Safe to proceed
}
bh_unlock_sock(sk);
```

### 7.3 Avoiding Deadlock

```c
// PATTERN: Never hold socket lock while waiting for I/O
void tcp_sendmsg(...)
{
    lock_sock(sk);
    
    while (data_to_send) {
        if (!sk_stream_memory_free(sk)) {
            release_sock(sk);        // <-- Release before wait
            err = sk_stream_wait_memory(sk, &timeo);
            lock_sock(sk);           // <-- Re-acquire after
            // Re-check conditions...
        }
        // Send data...
    }
    
    release_sock(sk);
}
```

---

## 8. Summary: Concurrency Architecture

```
+------------------------------------------------------------------+
|                    TCP CONCURRENCY MODEL                          |
+------------------------------------------------------------------+
|                                                                   |
|  LOCK TYPES:                                                      |
|    socket_lock_t = spinlock + ownership flag + wait queue         |
|                                                                   |
|  CONTEXT HANDLING:                                                |
|    User:    lock_sock()     - may sleep                          |
|    BH:      bh_lock_sock()  - non-blocking                       |
|    Timer:   bh_lock_sock()  - reschedule if busy                 |
|                                                                   |
|  RACE PREVENTION:                                                 |
|    Backlog queue:   Deferred processing when user holds lock     |
|    Memory barriers: smp_wmb/rmb for lock-free reads              |
|    Volatile state:  Compiler can't cache state value             |
|                                                                   |
|  GUARANTEES:                                                      |
|    All state transitions under socket lock                       |
|    Packet ordering via backlog + sequence numbers                |
|    No deadlock via strict lock hierarchy                         |
|                                                                   |
+------------------------------------------------------------------+
```

中文总结：
1. **双层锁机制**：spinlock + 所有权标志
2. **上下文区分**：用户可睡眠，BH/Timer不可睡眠
3. **Backlog队列**：用户持锁时的数据包缓冲
4. **内存屏障**：确保跨CPU状态可见性
5. **锁层次**：全局哈希锁 → 套接字锁 → 定时器锁
6. **状态转换原子性**：所有转换在持锁时完成
