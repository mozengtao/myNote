# TCP FSM: Purpose & Problem Statement

## 1. Engineering Problems the TCP FSM Solves

### 1.1 Reliable Data Delivery Over Unreliable Networks

TCP must provide reliable, ordered, error-checked delivery of data between applications running on hosts communicating over an IP network. The FSM tracks:

- **Connection establishment** (three-way handshake)
- **Data transfer** (sequencing, acknowledgment, retransmission)
- **Connection termination** (graceful close, half-close, abortive close)

### 1.2 Concurrent Connection Management

A single host may handle thousands of simultaneous connections, each in different phases:

```
                              +---------+ ---------\      active OPEN
                              |  CLOSED |            \    -----------
                              +---------+<---------\   \   create TCB
                                |     ^              \   \  snd SYN
                   passive OPEN |     |   CLOSE        \   \
                   ------------ |     | ----------       \   \
                    create TCB  |     | delete TCB         \   \
                                V     |                      \   \
                              +---------+            CLOSE    |    \
                              |  LISTEN |          ---------- |     |
                              +---------+          delete TCB |     |
                   rcv SYN      |     |     SEND              |     |
                  -----------   |     |    -------            |     V
 +---------+      snd SYN,ACK  /       \   snd SYN          +---------+
 |         |<-----------------           ------------------>|         |
 |   SYN   |                    rcv SYN                     |   SYN   |
 |   RCVD  |<-----------------------------------------------|   SENT  |
 |         |                    snd ACK                     |         |
 |         |------------------           -------------------|         |
 +---------+   rcv ACK of SYN  \       /  rcv SYN,ACK       +---------+
   |           --------------   |     |   -----------
   |                  x         |     |     snd ACK
   |                            V     V
   |  CLOSE                   +---------+
   | -------                  |  ESTAB  |
   | snd FIN                  +---------+
   |                   CLOSE    |     |    rcv FIN
   V                  -------   |     |    -------
 +---------+          snd FIN  /       \   snd ACK          +---------+
 |  FIN    |<-----------------           ------------------>|  CLOSE  |
 | WAIT-1  |------------------                              |   WAIT  |
 +---------+          rcv FIN  \                            +---------+
   | rcv ACK of FIN   -------   |                            CLOSE  |
   | --------------   snd ACK   |                           ------- |
   V        x                   V                           snd FIN V
 +---------+                  +---------+                   +---------+
 |FINWAIT-2|                  | CLOSING |                   | LAST-ACK|
 +---------+                  +---------+                   +---------+
   |                rcv ACK of FIN |                 rcv ACK of FIN |
   |  rcv FIN       -------------- |    Timeout=2MSL -------------- |
   |  -------              x       V    ------------        x       V
    \ snd ACK                 +---------+delete TCB         +---------+
     ------------------------>|TIME WAIT|------------------>| CLOSED  |
                              +---------+                   +---------+

RFC 793 - TCP State Diagram (ASCII Art)
```

中文说明：
- TCP状态机解决了在不可靠网络上提供可靠传输的核心问题
- 每个连接独立维护状态，支持数千个并发连接
- 状态机明确定义了连接生命周期的每个阶段

### 1.3 Protocol Correctness

The FSM ensures:
- **No data loss**: Retransmission based on state
- **No duplication**: Sequence number tracking  
- **Proper ordering**: State-dependent data queuing
- **Graceful termination**: Both ends agree connection is closed

---

## 2. Why TCP MUST Be Modeled as a State Machine

### 2.1 Asynchronous Event Handling

TCP must respond correctly to events arriving in **any order**:

```
+------------------+     +-----------------+     +------------------+
|  Incoming        |     |   Timer         |     |   User-space     |
|  Packets         |     |   Expiration    |     |   Syscalls       |
+------------------+     +-----------------+     +------------------+
         |                       |                        |
         v                       v                        v
    +--------------------------------------------------------+
    |                    TCP FSM DISPATCHER                  |
    |                                                        |
    |   Current State + Event → Action + New State           |
    +--------------------------------------------------------+
                              |
                              v
                    +------------------+
                    |   State Update   |
                    |   Output Actions |
                    +------------------+
```

中文说明：
- TCP必须处理来自多个源的异步事件
- 状态机提供了一种形式化方法来定义每种状态下对每种事件的正确响应
- 没有状态机，就无法保证协议正确性

### 2.2 Formal Verification Requirement

RFC 793 **specifies TCP as an FSM**. The state machine is not an implementation choice—it's the protocol definition:

From `tcp_states.h`:
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

### 2.3 Deterministic Behavior Under All Conditions

The FSM guarantees **predictable responses**:

| Current State | Event | Next State | Action |
|---------------|-------|------------|--------|
| LISTEN | SYN received | SYN_RECV | Send SYN+ACK |
| SYN_SENT | SYN+ACK received | ESTABLISHED | Send ACK |
| ESTABLISHED | FIN received | CLOSE_WAIT | Send ACK |
| FIN_WAIT_1 | FIN received | CLOSING | Send ACK |
| FIN_WAIT_2 | FIN received | TIME_WAIT | Send ACK |

---

## 3. What Would Break Without Explicit State

### 3.1 Race Condition Vulnerabilities

Without explicit state:
```c
// BROKEN: Implicit state via flags
if (received_syn && !sent_synack) {
    send_synack();    // Race: another thread could check same condition
    sent_synack = 1;
}
```

With explicit state:
```c
// CORRECT: Atomic state check from tcp_rcv_state_process()
switch (sk->sk_state) {
case TCP_LISTEN:
    if (th->syn) {
        icsk->icsk_af_ops->conn_request(sk, skb);
        // State transition is atomic with action
    }
    break;
}
```

### 3.2 Resource Leak Prevention

The FSM ensures proper cleanup:

```c
// From tcp.c tcp_close():
if (sk->sk_state == TCP_FIN_WAIT2) {
    if (tp->linger2 < 0) {
        tcp_set_state(sk, TCP_CLOSE);
        tcp_send_active_reset(sk, GFP_ATOMIC);
    } else {
        tcp_time_wait(sk, TCP_FIN_WAIT2, tmo);
    }
}
```

中文说明：
- 没有显式状态，会导致竞态条件和资源泄漏
- 状态机确保每个状态都有明确的清理路径
- 显式状态使得调试和验证成为可能

### 3.3 Security Implications

Implicit state would enable attacks:
- **SYN floods**: Can't track half-open connections
- **FIN attacks**: Can't detect invalid close sequences
- **State confusion**: Attacker could trick stack into wrong behavior

---

## 4. RFC States vs Implementation States

### 4.1 Core RFC 793 States (11 states)

```
CLOSED, LISTEN, SYN-SENT, SYN-RECEIVED, 
ESTABLISHED, FIN-WAIT-1, FIN-WAIT-2, 
CLOSE-WAIT, CLOSING, LAST-ACK, TIME-WAIT
```

### 4.2 Linux Implementation (12 states)

From `tcp_states.h`:
```c
TCP_ESTABLISHED = 1,  // Data can flow
TCP_SYN_SENT,         // Active open sent SYN
TCP_SYN_RECV,         // Passive open received SYN, sent SYN+ACK
TCP_FIN_WAIT1,        // App closed, sent FIN
TCP_FIN_WAIT2,        // FIN acked, waiting for peer FIN
TCP_TIME_WAIT,        // Waiting for stale packets to die
TCP_CLOSE,            // Socket closed
TCP_CLOSE_WAIT,       // Received FIN, waiting for app close
TCP_LAST_ACK,         // Sent FIN after receiving FIN
TCP_LISTEN,           // Server waiting for connections
TCP_CLOSING,          // Simultaneous close
TCP_MAX_STATES        // Sentinel (not a real state)
```

### 4.3 Sub-states and Flags

Some behavior is encoded via **flags** rather than primary states:

```c
// Congestion control sub-states (from linux/tcp.h)
enum tcp_ca_state {
    TCP_CA_Open = 0,
    TCP_CA_Disorder = 1,
    TCP_CA_CWR = 2,
    TCP_CA_Recovery = 3,
    TCP_CA_Loss = 4,
};

// Shutdown flags
#define RCV_SHUTDOWN    1
#define SEND_SHUTDOWN   2
#define SHUTDOWN_MASK   3
```

中文说明：
- Linux实现与RFC定义高度一致，共11个核心状态
- 拥塞控制使用独立的子状态机
- 关闭标志（RCV_SHUTDOWN, SEND_SHUTDOWN）补充主状态机
- 这种设计避免了状态爆炸问题

---

## 5. Summary: FSM as Engineering Necessity

```
+----------------------------------------------------------------+
|                    TCP STATE MACHINE VALUE                      |
+----------------------------------------------------------------+
| 1. CORRECTNESS    | Formal mapping to RFC 793                  |
| 2. RELIABILITY    | Every state has defined transitions        |
| 3. SECURITY       | Explicit state prevents confusion attacks  |
| 4. CONCURRENCY    | Atomic state + action under lock           |
| 5. DEBUGGING      | State visible via /proc/net/tcp            |
| 6. MAINTAINABILITY| Clear separation of concerns               |
+----------------------------------------------------------------+
```

中文总结：
1. **正确性**：状态机直接映射RFC 793规范
2. **可靠性**：每个状态都有明确定义的转换
3. **安全性**：显式状态防止混淆攻击
4. **并发性**：状态和动作在锁保护下原子执行
5. **可调试性**：状态可通过/proc/net/tcp查看
6. **可维护性**：关注点清晰分离

The TCP FSM is not just good engineering—it's the **only way** to implement a correct, reliable, and secure TCP stack.
