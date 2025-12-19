# Finite State Machines in the Linux Kernel

An architectural apprenticeship in FSM design patterns from Linux kernel v3.2.

---

## Table of Contents

- [Phase 1: TCP FSM (Canonical Kernel FSM)](#phase-1--tcp-fsm-canonical-kernel-fsm)
- [Phase 2: enum + switch FSM Pattern](#phase-2--enum--switch-fsm-pattern)
- [Phase 3: Ops-Based State Pattern](#phase-3--ops-based-state-pattern)
- [Phase 4: Hybrid FSMs (Enum + Ops + Events)](#phase-4--hybrid-fsms-enum--ops--events)
- [Phase 5: Design Rules & Failure Modes](#phase-5--design-rules--failure-modes)
- [Final Deliverable: FSM Decision Framework](#final-deliverable-fsm-decision-framework)

---

## Phase 1 — TCP FSM (Canonical Kernel FSM)

The TCP state machine is the most well-known FSM in the kernel. It directly implements RFC 793 and serves as the canonical example of the enum + switch pattern.

### 1.1 Where TCP States Are Defined

```c
/* include/net/tcp_states.h */
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

/* Bitmask versions for fast state testing */
#define TCP_STATE_MASK  0xF

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
    TCPF_CLOSING     = (1 << 11),
};
```

**Why states are centralized:**
1. **Single source of truth** — All TCP code uses the same state values
2. **RFC compliance** — Maps directly to RFC 793 state diagram
3. **Tool support** — Debuggers, netstat, ss can interpret state consistently
4. **Bitmask optimization** — `TCPF_*` flags enable testing multiple states in one operation

**说明:**
- TCP 状态定义集中在一个头文件中，确保全系统一致性
- 每个状态对应 RFC 793 的状态
- 位掩码版本（`TCPF_*`）允许高效测试多个状态

### 1.2 Complete TCP State Diagram

```
                              ┌─────────────────────────────────────────────┐
                              │              TCP STATE MACHINE               │
                              │         (RFC 793 Implementation)             │
                              └─────────────────────────────────────────────┘

                                        ┌──────────┐
                                        │  CLOSED  │ (Initial state)
                                        └────┬─────┘
                                             │
                           ┌─────────────────┴─────────────────┐
                           │                                   │
                      Passive Open                        Active Open
                      (listen)                           (connect)
                           │                                   │
                           ▼                                   ▼
                    ┌──────────┐                        ┌──────────┐
                    │  LISTEN  │                        │ SYN_SENT │
                    └────┬─────┘                        └────┬─────┘
                         │                                   │
                    rcv SYN                             rcv SYN+ACK
                    send SYN+ACK                        send ACK
                         │                                   │
                         ▼                                   │
                  ┌────────────┐                             │
                  │ SYN_RECV   │◄────────────────────────────┘
                  └─────┬──────┘      (simultaneous open)
                        │
                   rcv ACK
                        │
                        ▼
               ╔══════════════════╗
               ║   ESTABLISHED    ║  ◄── Data Transfer State
               ╚════════┬═════════╝
                        │
         ┌──────────────┴──────────────┐
         │                             │
    Close (send FIN)              rcv FIN
         │                        send ACK
         ▼                             │
   ┌────────────┐                      ▼
   │ FIN_WAIT1  │               ┌────────────┐
   └─────┬──────┘               │ CLOSE_WAIT │
         │                      └──────┬─────┘
         │                             │
    ┌────┴────┐                   Close (send FIN)
    │         │                        │
rcv ACK   rcv FIN                      ▼
    │     send ACK              ┌────────────┐
    ▼         │                 │  LAST_ACK  │
┌──────────┐  │                 └──────┬─────┘
│FIN_WAIT2 │  │                        │
└────┬─────┘  │                   rcv ACK
     │        │                        │
rcv FIN       ▼                        ▼
send ACK  ┌────────┐             ┌──────────┐
     │    │CLOSING │             │  CLOSED  │
     │    └───┬────┘             └──────────┘
     │        │
     │   rcv ACK
     │        │
     ▼        ▼
   ┌─────────────┐
   │  TIME_WAIT  │ ────── (2MSL timeout) ────► CLOSED
   └─────────────┘
```

**说明:**
- `ESTABLISHED` 是数据传输状态
- `TIME_WAIT` 确保所有数据包都被处理完毕
- 主动关闭方进入 `FIN_WAIT1/2`
- 被动关闭方进入 `CLOSE_WAIT` → `LAST_ACK`

### 1.3 TCP State Meaning

| State | Phase | Description |
|-------|-------|-------------|
| `TCP_CLOSE` | Initial/Final | Socket not in use |
| `TCP_LISTEN` | Connection Setup | Waiting for connection requests |
| `TCP_SYN_SENT` | Connection Setup | Active open, SYN sent, waiting for SYN+ACK |
| `TCP_SYN_RECV` | Connection Setup | SYN received, SYN+ACK sent, waiting for ACK |
| `TCP_ESTABLISHED` | Data Transfer | Connection open, data can flow |
| `TCP_FIN_WAIT1` | Teardown | FIN sent, waiting for ACK or FIN |
| `TCP_FIN_WAIT2` | Teardown | FIN acknowledged, waiting for peer's FIN |
| `TCP_CLOSE_WAIT` | Teardown | FIN received, waiting for application close |
| `TCP_CLOSING` | Teardown | Simultaneous close, waiting for ACK |
| `TCP_LAST_ACK` | Teardown | FIN sent after receiving FIN, waiting for ACK |
| `TCP_TIME_WAIT` | Teardown | Waiting 2MSL before final close |

### 1.4 Where State Transitions Occur

```
CODE LOCATION MAP FOR TCP STATE TRANSITIONS:

┌──────────────────────────────────────────────────────────────────────────────┐
│                              TCP SOURCE FILES                                 │
├──────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  net/ipv4/tcp_input.c                                                        │
│  ────────────────────                                                        │
│  • tcp_rcv_state_process()   ← MAIN STATE MACHINE DISPATCHER                 │
│  • tcp_rcv_established()     ← Fast path for ESTABLISHED state               │
│  • tcp_rcv_synsent_state_process() ← SYN_SENT handling                       │
│  • tcp_fin()                 ← FIN reception handling                        │
│                                                                              │
│  net/ipv4/tcp_output.c                                                       │
│  ─────────────────────                                                       │
│  • tcp_send_fin()            ← Initiates close sequence                      │
│  • tcp_connect()             ← Initiates connection (→ SYN_SENT)             │
│  • tcp_send_synack()         ← Responds to SYN (→ SYN_RECV)                  │
│                                                                              │
│  net/ipv4/tcp.c                                                              │
│  ──────────────                                                              │
│  • tcp_set_state()           ← CENTRAL STATE MUTATION FUNCTION               │
│  • tcp_close()               ← User-initiated close                          │
│  • tcp_disconnect()          ← Reset connection                              │
│                                                                              │
│  net/ipv4/tcp_timer.c                                                        │
│  ────────────────────                                                        │
│  • tcp_keepalive_timer()     ← Keepalive timeout handling                    │
│  • tcp_retransmit_timer()    ← Retransmission handling                       │
│  • tcp_time_wait_handler()   ← TIME_WAIT expiration                          │
│                                                                              │
└──────────────────────────────────────────────────────────────────────────────┘
```

**说明:**
- `tcp_rcv_state_process()` 是主状态机调度器
- `tcp_set_state()` 是中央状态变更函数，所有状态转换通过此函数
- 输入处理在 `tcp_input.c`，输出处理在 `tcp_output.c`，定时器在 `tcp_timer.c`

### 1.5 Central State Mutation: `tcp_set_state()`

```c
/* net/ipv4/tcp.c:1783-1816 */
void tcp_set_state(struct sock *sk, int state)
{
    int oldstate = sk->sk_state;

    /* [KEY] Handle state-specific side effects */
    switch (state) {
    case TCP_ESTABLISHED:
        if (oldstate != TCP_ESTABLISHED)
            TCP_INC_STATS(sock_net(sk), TCP_MIB_CURRESTAB);
        break;

    case TCP_CLOSE:
        if (oldstate == TCP_CLOSE_WAIT || oldstate == TCP_ESTABLISHED)
            TCP_INC_STATS(sock_net(sk), TCP_MIB_ESTABRESETS);

        /* [KEY] Unhash from connection table when closing */
        sk->sk_prot->unhash(sk);
        if (inet_csk(sk)->icsk_bind_hash &&
            !(sk->sk_userlocks & SOCK_BINDPORT_LOCK))
            inet_put_port(sk);
        /* fall through */

    default:
        if (oldstate == TCP_ESTABLISHED)
            TCP_DEC_STATS(sock_net(sk), TCP_MIB_CURRESTAB);
    }

    /* [KEY] Change state AFTER socket is unhashed
     * to avoid closed socket sitting in hash tables.
     */
    sk->sk_state = state;

#ifdef STATE_TRACE
    SOCK_DEBUG(sk, "TCP sk=%p, State %s -> %s\n",
               sk, statename[oldstate], statename[state]);
#endif
}
EXPORT_SYMBOL_GPL(tcp_set_state);
```

**Why centralized state mutation:**
1. **Statistics tracking** — Increment/decrement connection counters
2. **Side effects** — Unhash from tables, release ports
3. **Ordering** — State changes after cleanup to avoid races
4. **Debugging** — Single point for tracing all transitions
5. **Invariant enforcement** — Can add assertions here

### 1.6 Tracing a Concrete Transition: TCP_SYN_RECV → TCP_ESTABLISHED

```
TRANSITION: SYN_RECV → ESTABLISHED (Three-way handshake completion)

┌─────────────────────────────────────────────────────────────────────────────┐
│                                                                             │
│    Client                                            Server                 │
│    ──────                                            ──────                 │
│                                                                             │
│    CLOSED                                            LISTEN                 │
│       │                                                 │                   │
│       │ ────── SYN (seq=x) ─────────────────────────►   │                   │
│       │                                                 │                   │
│    SYN_SENT                                          SYN_RECV               │
│       │                                                 │                   │
│       │ ◄───── SYN+ACK (seq=y, ack=x+1) ──────────────  │                   │
│       │                                                 │                   │
│       │ ────── ACK (ack=y+1) ───────────────────────►   │                   │
│       │                                                 │                   │
│    ESTABLISHED                               ──► ESTABLISHED ◄──            │
│                                                         │                   │
│                                              This transition we trace       │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

**Code trace:**

```c
/* === STEP 1: Incoming ACK packet received === */
/* net/ipv4/tcp_ipv4.c - tcp_v4_rcv() */
int tcp_v4_rcv(struct sk_buff *skb)
{
    /* ... lookup socket ... */
    
    /* [KEY] Dispatch based on socket state */
    if (sk->sk_state == TCP_TIME_WAIT)
        goto do_time_wait;
    
    /* Normal processing */
    if (!sock_owned_by_user(sk)) {
        /* [KEY] Call state machine */
        if (tcp_rcv_state_process(sk, skb, tcp_hdr(skb), skb->len)) {
            rsk = sk;
            goto reset;
        }
    }
}

/* === STEP 2: State machine dispatcher === */
/* net/ipv4/tcp_input.c:5790-6005 */
int tcp_rcv_state_process(struct sock *sk, struct sk_buff *skb,
                          const struct tcphdr *th, unsigned int len)
{
    struct tcp_sock *tp = tcp_sk(sk);
    struct inet_connection_sock *icsk = inet_csk(sk);
    
    /* [KEY] Main state switch */
    switch (sk->sk_state) {
    case TCP_CLOSE:
        goto discard;

    case TCP_LISTEN:
        /* ... handle SYN ... */
        
    case TCP_SYN_SENT:
        /* ... handle SYN+ACK ... */
    }
    
    /* [KEY] For SYN_RECV and later states, validate incoming packet */
    res = tcp_validate_incoming(sk, skb, th, 0);
    if (res <= 0)
        return -res;

    /* [KEY] step 5: check the ACK field */
    if (th->ack) {
        int acceptable = tcp_ack(sk, skb, FLAG_SLOWPATH) > 0;

        switch (sk->sk_state) {
        case TCP_SYN_RECV:
            if (acceptable) {
                /* === THIS IS THE TRANSITION! === */
                
                /* [1] Update copied sequence */
                tp->copied_seq = tp->rcv_nxt;
                
                /* [2] Memory barrier for visibility */
                smp_mb();
                
                /* [3] CHANGE STATE */
                tcp_set_state(sk, TCP_ESTABLISHED);
                
                /* [4] Notify waiters */
                sk->sk_state_change(sk);

                /* [5] Wake up application if waiting */
                if (sk->sk_socket)
                    sk_wake_async(sk, SOCK_WAKE_IO, POLL_OUT);

                /* [6] Update send window */
                tp->snd_una = TCP_SKB_CB(skb)->ack_seq;
                tp->snd_wnd = ntohs(th->window) << tp->rx_opt.snd_wscale;
                
                /* [7] Rebuild route for correct metrics */
                icsk->icsk_af_ops->rebuild_header(sk);

                /* [8] Initialize congestion control */
                tcp_init_congestion_control(sk);
                
                /* ... more initialization ... */
            }
            break;
            
        /* ... other states ... */
        }
    }
}
```

**Transition breakdown:**

| Step | Code | Purpose |
|------|------|---------|
| 1 | `tp->copied_seq = tp->rcv_nxt` | Mark where application should read from |
| 2 | `smp_mb()` | Ensure writes visible before state change |
| 3 | `tcp_set_state(sk, TCP_ESTABLISHED)` | **Actual state mutation** |
| 4 | `sk->sk_state_change(sk)` | Notify callbacks (e.g., netfilter) |
| 5 | `sk_wake_async()` | Wake application if blocked on connect() |
| 6 | Update snd_una, snd_wnd | Initialize send parameters |
| 7 | `rebuild_header()` | Update cached route |
| 8 | `tcp_init_congestion_control()` | Start congestion algorithm |

**说明:**
- 状态转换不仅仅是设置一个值
- 需要更新多个相关字段（窗口、序列号等）
- 需要唤醒等待的应用程序
- 需要初始化拥塞控制算法
- 需要内存屏障确保可见性

### 1.7 Why TCP FSM is Explicit, Centralized, and Switch-Driven

```
WHY THIS DESIGN?

┌─────────────────────────────────────────────────────────────────────────────┐
│                                                                             │
│  EXPLICIT (sk->sk_state is a simple integer):                               │
│  ─────────────────────────────────────────────                              │
│  • Debuggable: netstat, ss, /proc/net/tcp show state directly               │
│  • Auditable: Can grep for all sk_state accesses                            │
│  • Tool-friendly: BPF, ftrace can filter by state                           │
│  • Standard: Maps to well-known RFC 793 diagram                             │
│                                                                             │
│  CENTRALIZED (tcp_set_state):                                               │
│  ─────────────────────────────                                              │
│  • Statistics: All transitions update counters in one place                 │
│  • Side effects: Unhashing, port release happen consistently                │
│  • Logging: Single point to add tracing                                     │
│  • Assertions: Can check invariants on every transition                     │
│                                                                             │
│  SWITCH-DRIVEN (switch(sk->sk_state)):                                      │
│  ─────────────────────────────────────                                      │
│  • Performance: Compiler optimizes to jump table                            │
│  • Complete: Compiler warns about missing cases (-Wswitch)                  │
│  • Readable: Each state's handling is localized                             │
│  • Predictable: No virtual dispatch overhead                                │
│                                                                             │
│  ALTERNATIVES REJECTED:                                                     │
│  ──────────────────────                                                     │
│  • Function pointer per state: Too much indirection for hot path            │
│  • State objects: Memory overhead unacceptable                              │
│  • Hierarchical states: TCP is flat, no need for hierarchy                  │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

**说明:**
- 显式状态便于调试和工具支持
- 集中式状态变更确保统计和副作用一致
- switch 语句性能最优（编译器优化为跳转表）
- TCP 状态机是扁平的，不需要层次结构

### 1.8 Userspace Simulation: TCP-like FSM

This complete C program simulates the TCP FSM pattern in userspace:

```c
/*
 * tcp_fsm_simulation.c
 * 
 * Userspace simulation of Linux kernel TCP FSM pattern.
 * Demonstrates: enum states, central state mutation, switch-driven dispatch.
 * 
 * Compile: gcc -o tcp_fsm tcp_fsm_simulation.c -pthread
 * Run: ./tcp_fsm
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>

/*
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │                        ARCHITECTURE OVERVIEW                             │
 * ├─────────────────────────────────────────────────────────────────────────┤
 * │                                                                         │
 * │   ┌─────────────┐     ┌─────────────────┐     ┌──────────────────┐      │
 * │   │  Events     │────►│  State Machine  │────►│  State Change    │      │
 * │   │  (packets)  │     │  (switch/case)  │     │  (centralized)   │      │
 * │   └─────────────┘     └─────────────────┘     └──────────────────┘      │
 * │                              │                         │                │
 * │                              ▼                         ▼                │
 * │                       ┌─────────────┐          ┌──────────────┐         │
 * │                       │  Context    │          │  Statistics  │         │
 * │                       │  (socket)   │          │  (counters)  │         │
 * │                       └─────────────┘          └──────────────┘         │
 * │                                                                         │
 * └─────────────────────────────────────────────────────────────────────────┘
 */

/* =========================================================================
 * STEP 1: Define states (like include/net/tcp_states.h)
 * ========================================================================= */

/* [KEY] Centralized state enum - single source of truth */
typedef enum {
    TCP_CLOSE = 0,      /* Initial/final state */
    TCP_LISTEN,         /* Waiting for connections */
    TCP_SYN_SENT,       /* Active open, SYN sent */
    TCP_SYN_RECV,       /* SYN received, SYN+ACK sent */
    TCP_ESTABLISHED,    /* Connection open */
    TCP_FIN_WAIT1,      /* FIN sent, waiting for ACK */
    TCP_FIN_WAIT2,      /* FIN acknowledged */
    TCP_CLOSE_WAIT,     /* Received FIN, waiting for close */
    TCP_CLOSING,        /* Simultaneous close */
    TCP_LAST_ACK,       /* Waiting for final ACK */
    TCP_TIME_WAIT,      /* Waiting 2MSL */
    TCP_MAX_STATES
} tcp_state_t;

/* [KEY] State names for debugging (like kernel's statename[]) */
static const char *state_names[] = {
    "CLOSE", "LISTEN", "SYN_SENT", "SYN_RECV", "ESTABLISHED",
    "FIN_WAIT1", "FIN_WAIT2", "CLOSE_WAIT", "CLOSING", "LAST_ACK", "TIME_WAIT"
};

/* [KEY] Bitmask versions for fast multi-state testing */
#define TCPF_CLOSE       (1 << TCP_CLOSE)
#define TCPF_LISTEN      (1 << TCP_LISTEN)
#define TCPF_ESTABLISHED (1 << TCP_ESTABLISHED)
#define TCPF_TIME_WAIT   (1 << TCP_TIME_WAIT)

/* =========================================================================
 * STEP 2: Define events (incoming packets/user actions)
 * ========================================================================= */

typedef enum {
    EVENT_LISTEN,       /* User calls listen() */
    EVENT_CONNECT,      /* User calls connect() */
    EVENT_CLOSE,        /* User calls close() */
    EVENT_RCV_SYN,      /* Received SYN packet */
    EVENT_RCV_SYNACK,   /* Received SYN+ACK */
    EVENT_RCV_ACK,      /* Received ACK */
    EVENT_RCV_FIN,      /* Received FIN */
    EVENT_TIMEOUT,      /* Timer expired */
} tcp_event_t;

static const char *event_names[] = {
    "LISTEN", "CONNECT", "CLOSE", "RCV_SYN", "RCV_SYNACK",
    "RCV_ACK", "RCV_FIN", "TIMEOUT"
};

/* =========================================================================
 * STEP 3: Context object (like struct sock)
 * ========================================================================= */

/* Statistics (like TCP MIB counters) */
static struct {
    int established_count;
    int resets_count;
    int transitions_total;
} tcp_stats = {0, 0, 0};

static pthread_mutex_t stats_lock = PTHREAD_MUTEX_INITIALIZER;

/* [KEY] Socket context - holds state and connection data */
typedef struct tcp_sock {
    tcp_state_t     state;          /* [KEY] Current state */
    pthread_mutex_t lock;           /* Protects state transitions */
    
    /* Connection parameters */
    unsigned int    local_port;
    unsigned int    remote_port;
    unsigned int    seq_num;
    unsigned int    ack_num;
    
    /* Callbacks (simplified sk->sk_state_change) */
    void (*state_change_cb)(struct tcp_sock *);
} tcp_sock_t;

/* =========================================================================
 * STEP 4: Central state mutation (like tcp_set_state)
 * ========================================================================= */

/*
 * [KEY] ALL state changes go through this function.
 * This is the kernel pattern: centralize side effects.
 */
static void tcp_set_state(tcp_sock_t *sk, tcp_state_t new_state)
{
    tcp_state_t old_state = sk->state;
    
    /* [KEY] Validate transition */
    assert(new_state >= 0 && new_state < TCP_MAX_STATES);
    
    /* [KEY] Handle state-specific side effects */
    pthread_mutex_lock(&stats_lock);
    
    switch (new_state) {
    case TCP_ESTABLISHED:
        if (old_state != TCP_ESTABLISHED) {
            tcp_stats.established_count++;
            printf("  [STATS] Established connections: %d\n", 
                   tcp_stats.established_count);
        }
        break;
        
    case TCP_CLOSE:
        if (old_state == TCP_ESTABLISHED || old_state == TCP_CLOSE_WAIT) {
            tcp_stats.resets_count++;
        }
        if (old_state == TCP_ESTABLISHED) {
            tcp_stats.established_count--;
        }
        break;
        
    default:
        if (old_state == TCP_ESTABLISHED) {
            tcp_stats.established_count--;
        }
        break;
    }
    
    tcp_stats.transitions_total++;
    pthread_mutex_unlock(&stats_lock);
    
    /* [KEY] Change state AFTER side effects (ordering matters!) */
    sk->state = new_state;
    
    /* [KEY] Logging - single point for all transitions */
    printf("  STATE: %s -> %s\n", state_names[old_state], state_names[new_state]);
    
    /* [KEY] Notify callbacks */
    if (sk->state_change_cb) {
        sk->state_change_cb(sk);
    }
}

/* =========================================================================
 * STEP 5: State machine dispatcher (like tcp_rcv_state_process)
 * ========================================================================= */

/*
 * [KEY] Main state machine - switch on state, then on event.
 * Returns: 0 = success, -1 = invalid transition
 */
static int tcp_process_event(tcp_sock_t *sk, tcp_event_t event)
{
    int result = 0;
    
    pthread_mutex_lock(&sk->lock);  /* [KEY] Serialize transitions */
    
    printf("Event: %s in state %s\n", event_names[event], state_names[sk->state]);
    
    /* [KEY] Main state switch - like tcp_rcv_state_process */
    switch (sk->state) {
    
    case TCP_CLOSE:
        switch (event) {
        case EVENT_LISTEN:
            tcp_set_state(sk, TCP_LISTEN);
            break;
        case EVENT_CONNECT:
            /* Send SYN */
            sk->seq_num = rand() % 10000;
            printf("  [ACTION] Sending SYN, seq=%u\n", sk->seq_num);
            tcp_set_state(sk, TCP_SYN_SENT);
            break;
        default:
            printf("  [ERROR] Invalid event in CLOSE state\n");
            result = -1;
        }
        break;
        
    case TCP_LISTEN:
        switch (event) {
        case EVENT_RCV_SYN:
            /* Send SYN+ACK */
            sk->ack_num++;
            sk->seq_num = rand() % 10000;
            printf("  [ACTION] Sending SYN+ACK, seq=%u, ack=%u\n", 
                   sk->seq_num, sk->ack_num);
            tcp_set_state(sk, TCP_SYN_RECV);
            break;
        case EVENT_CLOSE:
            tcp_set_state(sk, TCP_CLOSE);
            break;
        default:
            printf("  [ERROR] Invalid event in LISTEN state\n");
            result = -1;
        }
        break;
        
    case TCP_SYN_SENT:
        switch (event) {
        case EVENT_RCV_SYNACK:
            /* Send ACK, connection established */
            sk->ack_num++;
            printf("  [ACTION] Sending ACK, ack=%u\n", sk->ack_num);
            tcp_set_state(sk, TCP_ESTABLISHED);
            break;
        case EVENT_RCV_SYN:
            /* Simultaneous open */
            printf("  [ACTION] Simultaneous open, sending SYN+ACK\n");
            tcp_set_state(sk, TCP_SYN_RECV);
            break;
        case EVENT_TIMEOUT:
            printf("  [ACTION] Connection timeout, aborting\n");
            tcp_set_state(sk, TCP_CLOSE);
            break;
        default:
            result = -1;
        }
        break;
        
    case TCP_SYN_RECV:
        switch (event) {
        case EVENT_RCV_ACK:
            /* Three-way handshake complete! */
            printf("  [ACTION] Handshake complete!\n");
            tcp_set_state(sk, TCP_ESTABLISHED);
            break;
        default:
            result = -1;
        }
        break;
        
    case TCP_ESTABLISHED:
        switch (event) {
        case EVENT_CLOSE:
            /* Active close - send FIN */
            printf("  [ACTION] Sending FIN\n");
            tcp_set_state(sk, TCP_FIN_WAIT1);
            break;
        case EVENT_RCV_FIN:
            /* Passive close - received FIN */
            printf("  [ACTION] Received FIN, sending ACK\n");
            tcp_set_state(sk, TCP_CLOSE_WAIT);
            break;
        default:
            /* Data transfer events would go here */
            break;
        }
        break;
        
    case TCP_FIN_WAIT1:
        switch (event) {
        case EVENT_RCV_ACK:
            tcp_set_state(sk, TCP_FIN_WAIT2);
            break;
        case EVENT_RCV_FIN:
            /* Simultaneous close */
            printf("  [ACTION] Simultaneous close, sending ACK\n");
            tcp_set_state(sk, TCP_CLOSING);
            break;
        default:
            result = -1;
        }
        break;
        
    case TCP_FIN_WAIT2:
        switch (event) {
        case EVENT_RCV_FIN:
            printf("  [ACTION] Received FIN, sending ACK, entering TIME_WAIT\n");
            tcp_set_state(sk, TCP_TIME_WAIT);
            break;
        default:
            result = -1;
        }
        break;
        
    case TCP_CLOSE_WAIT:
        switch (event) {
        case EVENT_CLOSE:
            printf("  [ACTION] Application closed, sending FIN\n");
            tcp_set_state(sk, TCP_LAST_ACK);
            break;
        default:
            result = -1;
        }
        break;
        
    case TCP_CLOSING:
        switch (event) {
        case EVENT_RCV_ACK:
            tcp_set_state(sk, TCP_TIME_WAIT);
            break;
        default:
            result = -1;
        }
        break;
        
    case TCP_LAST_ACK:
        switch (event) {
        case EVENT_RCV_ACK:
            printf("  [ACTION] Final ACK received, connection closed\n");
            tcp_set_state(sk, TCP_CLOSE);
            break;
        default:
            result = -1;
        }
        break;
        
    case TCP_TIME_WAIT:
        switch (event) {
        case EVENT_TIMEOUT:
            printf("  [ACTION] 2MSL timeout, connection fully closed\n");
            tcp_set_state(sk, TCP_CLOSE);
            break;
        default:
            /* Ignore other events in TIME_WAIT */
            break;
        }
        break;
        
    default:
        printf("  [BUG] Invalid state: %d\n", sk->state);
        assert(0);  /* Like BUG_ON in kernel */
    }
    
    pthread_mutex_unlock(&sk->lock);
    return result;
}

/* =========================================================================
 * STEP 6: Socket lifecycle functions
 * ========================================================================= */

static void state_change_callback(tcp_sock_t *sk)
{
    /* Simulate kernel's sk->sk_state_change callback */
    printf("  [CALLBACK] State changed to %s\n", state_names[sk->state]);
}

static tcp_sock_t *tcp_socket_create(void)
{
    tcp_sock_t *sk = calloc(1, sizeof(tcp_sock_t));
    if (!sk) return NULL;
    
    sk->state = TCP_CLOSE;
    pthread_mutex_init(&sk->lock, NULL);
    sk->state_change_cb = state_change_callback;
    
    return sk;
}

static void tcp_socket_destroy(tcp_sock_t *sk)
{
    pthread_mutex_destroy(&sk->lock);
    free(sk);
}

/* =========================================================================
 * STEP 7: Demonstration scenarios
 * ========================================================================= */

static void demo_three_way_handshake_server(void)
{
    printf("\n=== SERVER: Three-Way Handshake ===\n");
    
    tcp_sock_t *server = tcp_socket_create();
    
    /* Server side of handshake */
    tcp_process_event(server, EVENT_LISTEN);     /* CLOSE -> LISTEN */
    tcp_process_event(server, EVENT_RCV_SYN);    /* LISTEN -> SYN_RECV */
    tcp_process_event(server, EVENT_RCV_ACK);    /* SYN_RECV -> ESTABLISHED */
    
    /* Passive close */
    tcp_process_event(server, EVENT_RCV_FIN);    /* ESTABLISHED -> CLOSE_WAIT */
    tcp_process_event(server, EVENT_CLOSE);      /* CLOSE_WAIT -> LAST_ACK */
    tcp_process_event(server, EVENT_RCV_ACK);    /* LAST_ACK -> CLOSE */
    
    tcp_socket_destroy(server);
}

static void demo_three_way_handshake_client(void)
{
    printf("\n=== CLIENT: Three-Way Handshake ===\n");
    
    tcp_sock_t *client = tcp_socket_create();
    
    /* Client side of handshake */
    tcp_process_event(client, EVENT_CONNECT);    /* CLOSE -> SYN_SENT */
    tcp_process_event(client, EVENT_RCV_SYNACK); /* SYN_SENT -> ESTABLISHED */
    
    /* Active close */
    tcp_process_event(client, EVENT_CLOSE);      /* ESTABLISHED -> FIN_WAIT1 */
    tcp_process_event(client, EVENT_RCV_ACK);    /* FIN_WAIT1 -> FIN_WAIT2 */
    tcp_process_event(client, EVENT_RCV_FIN);    /* FIN_WAIT2 -> TIME_WAIT */
    tcp_process_event(client, EVENT_TIMEOUT);    /* TIME_WAIT -> CLOSE */
    
    tcp_socket_destroy(client);
}

int main(void)
{
    printf("TCP FSM Simulation (Kernel Pattern)\n");
    printf("====================================\n");
    
    demo_three_way_handshake_server();
    demo_three_way_handshake_client();
    
    printf("\n=== Final Statistics ===\n");
    printf("Total transitions: %d\n", tcp_stats.transitions_total);
    printf("Current established: %d\n", tcp_stats.established_count);
    printf("Total resets: %d\n", tcp_stats.resets_count);
    
    return 0;
}
```

**说明:**
- 状态定义集中在一个枚举中（`tcp_state_t`）
- 所有状态变更通过 `tcp_set_state()` 函数
- 使用 switch 语句处理状态和事件
- 使用互斥锁保护并发访问
- 包含统计计数器和回调通知

---

## Phase 2 — enum + switch FSM (General Pattern)

### 2.1 The Abstract Pattern

```c
/* THE GENERIC enum + switch FSM PATTERN */

/* Step 1: Define states */
enum my_state {
    STATE_IDLE,
    STATE_RUNNING,
    STATE_PAUSED,
    STATE_STOPPED,
    STATE_MAX
};

/* Step 2: Define events */
enum my_event {
    EVENT_START,
    EVENT_PAUSE,
    EVENT_RESUME,
    EVENT_STOP,
    EVENT_TIMEOUT,
};

/* Step 3: Context object with state */
struct my_context {
    enum my_state state;
    /* ... other fields ... */
};

/* Step 4: Transition function */
int my_handle_event(struct my_context *ctx, enum my_event event)
{
    switch (ctx->state) {
    case STATE_IDLE:
        switch (event) {
        case EVENT_START:
            ctx->state = STATE_RUNNING;
            do_start_actions(ctx);
            return 0;
        default:
            return -EINVAL;  /* Invalid event in this state */
        }
        
    case STATE_RUNNING:
        switch (event) {
        case EVENT_PAUSE:
            ctx->state = STATE_PAUSED;
            return 0;
        case EVENT_STOP:
            ctx->state = STATE_STOPPED;
            do_cleanup(ctx);
            return 0;
        /* ... */
        }
        
    /* ... other states ... */
    
    default:
        WARN_ON(1);  /* Invalid state */
        return -EINVAL;
    }
}
```

### 2.2 Three Other Kernel Subsystems Using enum + switch FSMs

#### Example 1: USB Device State Machine

```c
/* include/linux/usb/ch9.h:888-903 */
enum usb_device_state {
    USB_STATE_NOTATTACHED = 0,      /* Not physically connected */
    
    /* Chapter 9 device states */
    USB_STATE_ATTACHED,              /* Attached but not powered */
    USB_STATE_POWERED,               /* Powered, waiting for reset */
    USB_STATE_RECONNECTING,          /* Auth: reconnecting */
    USB_STATE_UNAUTHENTICATED,       /* Auth: not authenticated */
    USB_STATE_DEFAULT,               /* Reset, limited function */
    USB_STATE_ADDRESS,               /* Address assigned */
    USB_STATE_CONFIGURED,            /* Fully operational */
    
    USB_STATE_SUSPENDED              /* Power management state */
};

/* include/linux/usb.h - Context object */
struct usb_device {
    int                     devnum;
    enum usb_device_state   state;      /* [KEY] State field */
    enum usb_device_speed   speed;
    struct usb_device       *parent;
    /* ... */
};

/* drivers/usb/core/hub.c - State transition */
void usb_set_device_state(struct usb_device *udev,
                          enum usb_device_state new_state)
{
    unsigned long flags;

    spin_lock_irqsave(&device_state_lock, flags);
    
    /* [KEY] Validate transition */
    if (udev->state == USB_STATE_NOTATTACHED)
        ;   /* Can't change from NOTATTACHED */
    else if (new_state != USB_STATE_NOTATTACHED) {
        /* Normal transition */
        if (udev->parent) {
            /* Child devices have limited valid transitions */
            if (udev->parent->state == USB_STATE_NOTATTACHED ||
                udev->parent->state < USB_STATE_CONFIGURED)
                new_state = USB_STATE_NOTATTACHED;
        }
        udev->state = new_state;
    } else {
        /* Detach: recursively disconnect children */
        recursively_mark_NOTATTACHED(udev);
    }
    
    spin_unlock_irqrestore(&device_state_lock, flags);
}
```

**Invariants enforced:**
- NOTATTACHED is a terminal state
- Children can't be in higher state than parent
- State changes are atomic (spinlock protected)

**说明:**
- USB 设备状态遵循 USB 规范第 9 章
- 状态变更受 spinlock 保护，确保原子性
- 子设备不能比父设备状态更高

#### Example 2: SCTP Association State Machine

```c
/* include/net/sctp/constants.h:190-202 */
typedef enum {
    SCTP_STATE_CLOSED            = 0,
    SCTP_STATE_COOKIE_WAIT       = 1,
    SCTP_STATE_COOKIE_ECHOED     = 2,
    SCTP_STATE_ESTABLISHED       = 3,
    SCTP_STATE_SHUTDOWN_PENDING  = 4,
    SCTP_STATE_SHUTDOWN_SENT     = 5,
    SCTP_STATE_SHUTDOWN_RECEIVED = 6,
    SCTP_STATE_SHUTDOWN_ACK_SENT = 7,
} sctp_state_t;

/* include/net/sctp/structs.h - Association context */
struct sctp_association {
    /* ... */
    sctp_state_t state;         /* [KEY] Current state */
    /* ... */
};

/* net/sctp/sm_statetable.c - 2D dispatch table */
/* [event_type][state] → handler function */
static const sctp_sm_table_entry_t
chunk_event_table[SCTP_NUM_CHUNK_TYPES][SCTP_STATE_NUM_STATES];

/* State machine lookup */
const sctp_sm_table_entry_t *sctp_sm_lookup_event(
    sctp_event_t event_type,
    sctp_state_t state,
    sctp_subtype_t event_subtype)
{
    switch (event_type) {
    case SCTP_EVENT_T_CHUNK:
        return sctp_chunk_event_lookup(event_subtype.chunk, state);
    case SCTP_EVENT_T_TIMEOUT:
        return &timeout_event_table[event_subtype.timeout][(int)state];
    /* ... */
    }
}
```

**Key insight:** SCTP uses a 2D table indexed by [event][state] for dispatch, but still uses enum for state values.

#### Example 3: Block Request State Machine

```c
/* include/linux/blkdev.h */
enum rq_cmd_type_bits {
    REQ_TYPE_FS             = 1,    /* fs request */
    REQ_TYPE_BLOCK_PC       = 2,    /* scsi command */
    REQ_TYPE_SENSE          = 3,    /* sense request */
    REQ_TYPE_PM_SUSPEND     = 4,    /* suspend request */
    REQ_TYPE_PM_RESUME      = 5,    /* resume request */
    REQ_TYPE_PM_SHUTDOWN    = 6,    /* shutdown request */
    REQ_TYPE_SPECIAL        = 7,    /* driver defined */
    REQ_TYPE_ATA_TASKFILE   = 8,    /* ATA taskfile */
    REQ_TYPE_ATA_PC         = 9,    /* ATA packet command */
};

/* Request lifecycle states (implicit in code) */
/*
 * ALLOCATED → QUEUED → RUNNING → COMPLETED
 *                ↓
 *            REQUEUED
 */

/* block/blk-core.c - State transitions */
void blk_start_request(struct request *req)
{
    /* [STATE] QUEUED → RUNNING */
    blk_dequeue_request(req);
    req->resid_len = blk_rq_bytes(req);
    /* ... set timestamps, accounting ... */
}

bool blk_end_request(struct request *rq, int error, unsigned int nr_bytes)
{
    /* [STATE] RUNNING → COMPLETED (or requeued) */
    return blk_end_bidi_request(rq, error, nr_bytes, 0);
}
```

### 2.3 When enum + switch is the RIGHT Choice

| Factor | enum + switch Advantage |
|--------|------------------------|
| **Performance** | Switch compiles to jump table (O(1) dispatch) |
| **Auditability** | grep finds all state accesses easily |
| **Predictability** | No virtual dispatch, inline-able |
| **Debuggability** | State is a simple integer, visible in debuggers |
| **Tool support** | BPF, ftrace can filter by state value |
| **Memory** | Single integer field, no pointers |

### 2.4 Limitations of enum + switch

```
WHEN enum + switch BREAKS DOWN:

┌─────────────────────────────────────────────────────────────────────────────┐
│                                                                             │
│  PROBLEM 1: Combinatorial Explosion                                         │
│  ───────────────────────────────────                                        │
│                                                                             │
│    States: S1, S2, S3, S4, S5 (5 states)                                    │
│    Events: E1, E2, E3, E4, E5, E6, E7, E8 (8 events)                        │
│                                                                             │
│    switch(state) {                                                          │
│        case S1: switch(event) { ... 8 cases ... } break;                    │
│        case S2: switch(event) { ... 8 cases ... } break;                    │
│        case S3: switch(event) { ... 8 cases ... } break;                    │
│        case S4: switch(event) { ... 8 cases ... } break;                    │
│        case S5: switch(event) { ... 8 cases ... } break;                    │
│    }                                                                        │
│    // 5 × 8 = 40 cases to maintain!                                         │
│                                                                             │
│  PROBLEM 2: Poor Extensibility                                              │
│  ─────────────────────────────                                              │
│                                                                             │
│    Adding new state requires modifying EVERY event handler                  │
│    Adding new event requires modifying EVERY state handler                  │
│    Changes scattered across large switch statements                         │
│                                                                             │
│  PROBLEM 3: Code Scattering                                                 │
│  ──────────────────────────                                                 │
│                                                                             │
│    If STATE_FOO has 200 lines of handling code,                             │
│    and there are 10 states,                                                 │
│    the switch statement is 2000+ lines                                      │
│    (tcp_rcv_state_process is ~300 lines)                                    │
│                                                                             │
│  PROBLEM 4: State-Specific Data                                             │
│  ─────────────────────────────                                              │
│                                                                             │
│    If STATE_A needs fields X, Y                                             │
│    And STATE_B needs fields Z, W                                            │
│    All fields must be in context struct                                     │
│    Even if only one state uses them                                         │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

**说明:**
- 状态×事件组合爆炸导致巨大 switch 语句
- 扩展性差：添加状态/事件需要修改多处
- 状态特定数据浪费内存（所有状态共享 context struct）

### 2.5 Userspace Simulation: Generic enum + switch FSM

This example shows a USB-like device state machine with table-driven validation:

```c
/*
 * device_fsm_simulation.c
 * 
 * Userspace simulation of a device state machine (USB-like pattern).
 * Demonstrates: enum states, table-driven transitions, hierarchical constraints.
 * 
 * Compile: gcc -o device_fsm device_fsm_simulation.c
 * Run: ./device_fsm
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

/*
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │                    DEVICE STATE MACHINE DIAGRAM                          │
 * ├─────────────────────────────────────────────────────────────────────────┤
 * │                                                                         │
 * │    ┌────────────┐                                                       │
 * │    │ DETACHED   │◄─────────────────────────────────────────┐            │
 * │    └─────┬──────┘                                          │            │
 * │          │ attach                                     detach            │
 * │          ▼                                                 │            │
 * │    ┌────────────┐     power      ┌────────────┐            │            │
 * │    │ ATTACHED   │ ───────────►   │  POWERED   │────────────┤            │
 * │    └────────────┘                └─────┬──────┘            │            │
 * │                                        │ reset             │            │
 * │                                        ▼                   │            │
 * │                                  ┌────────────┐            │            │
 * │                                  │  DEFAULT   │────────────┤            │
 * │                                  └─────┬──────┘            │            │
 * │                                        │ set_address       │            │
 * │                                        ▼                   │            │
 * │                                  ┌────────────┐            │            │
 * │                                  │  ADDRESS   │────────────┤            │
 * │                                  └─────┬──────┘            │            │
 * │                                        │ configure         │            │
 * │                                        ▼                   │            │
 * │                                  ┌────────────┐            │            │
 * │                                  │ CONFIGURED │────────────┤            │
 * │                                  └─────┬──────┘            │            │
 * │                                        │ suspend           │            │
 * │                                        ▼                   │            │
 * │                                  ┌────────────┐            │            │
 * │                                  │ SUSPENDED  │────────────┘            │
 * │                                  └────────────┘                         │
 * │                                                                         │
 * └─────────────────────────────────────────────────────────────────────────┘
 */

/* =========================================================================
 * STEP 1: Define states and events
 * ========================================================================= */

typedef enum {
    DEV_STATE_DETACHED = 0,    /* Not physically connected */
    DEV_STATE_ATTACHED,         /* Connected but not powered */
    DEV_STATE_POWERED,          /* Powered, waiting for reset */
    DEV_STATE_DEFAULT,          /* Reset complete, default address */
    DEV_STATE_ADDRESS,          /* Address assigned */
    DEV_STATE_CONFIGURED,       /* Fully operational */
    DEV_STATE_SUSPENDED,        /* Power-saving mode */
    DEV_STATE_MAX
} device_state_t;

static const char *state_names[] = {
    "DETACHED", "ATTACHED", "POWERED", "DEFAULT",
    "ADDRESS", "CONFIGURED", "SUSPENDED"
};

typedef enum {
    DEV_EVENT_ATTACH,
    DEV_EVENT_DETACH,
    DEV_EVENT_POWER,
    DEV_EVENT_RESET,
    DEV_EVENT_SET_ADDRESS,
    DEV_EVENT_CONFIGURE,
    DEV_EVENT_SUSPEND,
    DEV_EVENT_RESUME,
    DEV_EVENT_MAX
} device_event_t;

static const char *event_names[] = {
    "ATTACH", "DETACH", "POWER", "RESET",
    "SET_ADDRESS", "CONFIGURE", "SUSPEND", "RESUME"
};

/* =========================================================================
 * STEP 2: Transition table (like kernel state tables)
 * ========================================================================= */

/*
 * [KEY] Table-driven FSM: transition_table[current_state][event] = next_state
 * Value of -1 means invalid transition.
 * This is cleaner than nested switches for complex FSMs.
 */
static const int transition_table[DEV_STATE_MAX][DEV_EVENT_MAX] = {
    /*                 ATTACH  DETACH  POWER  RESET  SET_ADDR  CONFIG  SUSPEND  RESUME */
    /* DETACHED   */ {     1,     -1,    -1,    -1,       -1,     -1,      -1,     -1 },
    /* ATTACHED   */ {    -1,      0,     2,    -1,       -1,     -1,      -1,     -1 },
    /* POWERED    */ {    -1,      0,    -1,     3,       -1,     -1,      -1,     -1 },
    /* DEFAULT    */ {    -1,      0,    -1,     3,        4,     -1,      -1,     -1 },
    /* ADDRESS    */ {    -1,      0,    -1,     3,       -1,      5,      -1,     -1 },
    /* CONFIGURED */ {    -1,      0,    -1,     3,       -1,     -1,       6,     -1 },
    /* SUSPENDED  */ {    -1,      0,    -1,    -1,       -1,     -1,      -1,      5 },
};

/* =========================================================================
 * STEP 3: Device context and hierarchy
 * ========================================================================= */

struct device;  /* Forward declaration */

/* [KEY] Device hierarchy (like USB hub → device) */
typedef struct device {
    device_state_t      state;
    int                 id;
    int                 address;
    struct device       *parent;        /* Parent device */
    struct device       **children;     /* Child devices */
    int                 num_children;
    
    /* State change callback */
    void (*state_change)(struct device *dev, device_state_t old, device_state_t new);
} device_t;

/* =========================================================================
 * STEP 4: Central state mutation with hierarchy enforcement
 * ========================================================================= */

/*
 * [KEY] Recursive detachment - like USB recursively_mark_NOTATTACHED()
 * When parent detaches, all children must detach too.
 */
static void device_detach_recursive(device_t *dev)
{
    /* First detach all children */
    for (int i = 0; i < dev->num_children; i++) {
        if (dev->children[i]) {
            device_detach_recursive(dev->children[i]);
        }
    }
    
    /* Then detach self */
    if (dev->state != DEV_STATE_DETACHED) {
        device_state_t old = dev->state;
        dev->state = DEV_STATE_DETACHED;
        printf("  Device %d: %s -> DETACHED (recursive)\n", 
               dev->id, state_names[old]);
    }
}

/*
 * [KEY] Central state mutation with constraints.
 * Like usb_set_device_state() in the kernel.
 */
static int device_set_state(device_t *dev, device_event_t event)
{
    device_state_t old_state = dev->state;
    
    /* [KEY] CONSTRAINT 1: Can't change from DETACHED except via ATTACH */
    if (old_state == DEV_STATE_DETACHED && event != DEV_EVENT_ATTACH) {
        printf("  Device %d: Cannot %s when DETACHED\n", 
               dev->id, event_names[event]);
        return -1;
    }
    
    /* [KEY] CONSTRAINT 2: Child can't be in higher state than parent */
    if (dev->parent) {
        device_state_t parent_state = dev->parent->state;
        
        /* Parent must be at least CONFIGURED for child to operate */
        if (parent_state < DEV_STATE_CONFIGURED && 
            event != DEV_EVENT_DETACH) {
            printf("  Device %d: Parent not ready (state=%s)\n",
                   dev->id, state_names[parent_state]);
            return -1;
        }
    }
    
    /* [KEY] Look up transition in table */
    int new_state = transition_table[old_state][event];
    
    if (new_state < 0) {
        printf("  Device %d: Invalid event %s in state %s\n",
               dev->id, event_names[event], state_names[old_state]);
        return -1;
    }
    
    /* [KEY] Special handling for DETACH - recursive */
    if (event == DEV_EVENT_DETACH) {
        device_detach_recursive(dev);
        return 0;
    }
    
    /* [KEY] Perform state change */
    dev->state = (device_state_t)new_state;
    
    printf("  Device %d: %s -> %s (event: %s)\n",
           dev->id, state_names[old_state], 
           state_names[new_state], event_names[event]);
    
    /* [KEY] Invoke callback */
    if (dev->state_change) {
        dev->state_change(dev, old_state, dev->state);
    }
    
    return 0;
}

/* =========================================================================
 * STEP 5: Device lifecycle
 * ========================================================================= */

static void default_state_change(device_t *dev, device_state_t old, device_state_t new)
{
    if (new == DEV_STATE_CONFIGURED) {
        printf("  [CALLBACK] Device %d is now fully operational!\n", dev->id);
    }
}

static device_t *device_create(int id, device_t *parent)
{
    device_t *dev = calloc(1, sizeof(device_t));
    if (!dev) return NULL;
    
    dev->id = id;
    dev->state = DEV_STATE_DETACHED;
    dev->parent = parent;
    dev->state_change = default_state_change;
    
    /* Register with parent */
    if (parent) {
        parent->children = realloc(parent->children, 
                                   (parent->num_children + 1) * sizeof(device_t *));
        parent->children[parent->num_children++] = dev;
    }
    
    return dev;
}

static void device_destroy(device_t *dev)
{
    if (dev->children) free(dev->children);
    free(dev);
}

/* =========================================================================
 * STEP 6: Demonstration
 * ========================================================================= */

static void demo_device_lifecycle(void)
{
    printf("\n=== Device Lifecycle Demo ===\n\n");
    
    /* Create hub (parent) and device (child) */
    device_t *hub = device_create(0, NULL);
    device_t *dev = device_create(1, hub);
    
    printf("--- Bringing up hub first ---\n");
    device_set_state(hub, DEV_EVENT_ATTACH);
    device_set_state(hub, DEV_EVENT_POWER);
    device_set_state(hub, DEV_EVENT_RESET);
    device_set_state(hub, DEV_EVENT_SET_ADDRESS);
    device_set_state(hub, DEV_EVENT_CONFIGURE);
    
    printf("\n--- Now child device can come up ---\n");
    device_set_state(dev, DEV_EVENT_ATTACH);
    device_set_state(dev, DEV_EVENT_POWER);
    device_set_state(dev, DEV_EVENT_RESET);
    device_set_state(dev, DEV_EVENT_SET_ADDRESS);
    device_set_state(dev, DEV_EVENT_CONFIGURE);
    
    printf("\n--- Test invalid transition ---\n");
    device_set_state(dev, DEV_EVENT_POWER);  /* Can't power when CONFIGURED */
    
    printf("\n--- Detaching hub (cascades to child) ---\n");
    device_set_state(hub, DEV_EVENT_DETACH);
    
    printf("\n--- Child can't start if parent is detached ---\n");
    device_set_state(dev, DEV_EVENT_ATTACH);
    
    device_destroy(dev);
    device_destroy(hub);
}

static void demo_transition_table(void)
{
    printf("\n=== Transition Table Visualization ===\n\n");
    
    printf("          ");
    for (int e = 0; e < DEV_EVENT_MAX; e++) {
        printf("%8.8s ", event_names[e]);
    }
    printf("\n");
    
    for (int s = 0; s < DEV_STATE_MAX; s++) {
        printf("%-10s", state_names[s]);
        for (int e = 0; e < DEV_EVENT_MAX; e++) {
            int next = transition_table[s][e];
            if (next >= 0) {
                printf("%8.8s ", state_names[next]);
            } else {
                printf("       - ");
            }
        }
        printf("\n");
    }
}

int main(void)
{
    printf("Device FSM Simulation (enum + switch + table pattern)\n");
    printf("======================================================\n");
    
    demo_transition_table();
    demo_device_lifecycle();
    
    return 0;
}
```

**Key patterns demonstrated:**

| Pattern | Kernel Example | Simulation Example |
|---------|---------------|-------------------|
| Table-driven transitions | SCTP state table | `transition_table[state][event]` |
| Hierarchical constraints | USB parent/child | `dev->parent->state` check |
| Recursive state change | `recursively_mark_NOTATTACHED` | `device_detach_recursive()` |
| Central state mutation | `usb_set_device_state()` | `device_set_state()` |

**说明:**
- 使用 2D 表格 `transition_table[状态][事件]` 替代嵌套 switch
- 父设备必须先达到 CONFIGURED 状态，子设备才能操作
- DETACH 事件递归传播到所有子设备
- 表格驱动使状态机更易于理解和验证

---

## Phase 3 — Ops-Based State Pattern

### 3.1 State as Data vs State as Behavior

```
TWO PARADIGMS OF STATE REPRESENTATION:

┌─────────────────────────────────────────────────────────────────────────────┐
│                                                                             │
│  STATE AS DATA (enum + switch):                                             │
│  ──────────────────────────────                                             │
│                                                                             │
│    struct context {                   void handle(ctx, event) {             │
│        enum state state;       →          switch (ctx->state) {             │
│        ...                                    case A: ...                   │
│    };                                         case B: ...                   │
│                                           }                                 │
│    State is a VALUE                   }                                     │
│    Behavior is in SWITCH              Behavior DEPENDS ON value             │
│                                                                             │
│  STATE AS BEHAVIOR (ops pattern):                                           │
│  ────────────────────────────────                                           │
│                                                                             │
│    struct context {                   void handle(ctx, event) {             │
│        const struct ops *ops;  →          ctx->ops->handle(ctx, event);     │
│        ...                            }                                     │
│    };                                                                       │
│                                                                             │
│    struct ops state_a_ops = {         State IS the behavior                 │
│        .handle = state_a_handle,      No switch needed                      │
│    };                                 Change ops = change state             │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

**说明:**
- "状态作为数据"：状态是一个值，行为通过 switch 选择
- "状态作为行为"：状态就是操作函数指针表，改变 ops 就是改变状态

### 3.2 Kernel Subsystems Where State is Encoded via ops

#### Example 1: TTY Line Discipline

```c
/* The context object */
struct tty_struct {
    int                     magic;
    struct kref             kref;
    struct tty_driver       *driver;
    const struct tty_operations *ops;
    
    /* [KEY] Line discipline - THIS IS THE STATE */
    struct tty_ldisc        *ldisc;
    
    void                    *driver_data;
    /* ... */
};

/* The ops table for line discipline */
struct tty_ldisc_ops {
    int     (*open)(struct tty_struct *);
    void    (*close)(struct tty_struct *);
    ssize_t (*read)(struct tty_struct *, struct file *,
                    unsigned char __user *, size_t);
    ssize_t (*write)(struct tty_struct *, struct file *,
                     const unsigned char *, size_t);
    int     (*ioctl)(struct tty_struct *, struct file *,
                     unsigned int, unsigned long);
    void    (*receive_buf)(struct tty_struct *,
                           const unsigned char *, char *, int);
    /* ... */
};

/* "State" objects - different behaviors */
struct tty_ldisc_ops n_tty_ops = {           /* Normal TTY */
    .open       = n_tty_open,
    .close      = n_tty_close,
    .read       = n_tty_read,
    .write      = n_tty_write,
    .receive_buf = n_tty_receive_buf,
};

struct tty_ldisc_ops ppp_ldisc = {           /* PPP protocol */
    .open       = ppp_asynctty_open,
    .close      = ppp_asynctty_close,
    .read       = ppp_asynctty_read,
    .write      = ppp_asynctty_write,
    .receive_buf = ppp_asynctty_receive,
};

struct tty_ldisc_ops slip_ldisc = {          /* SLIP protocol */
    .open       = slip_open,
    .close      = slip_close,
    .ioctl      = slip_ioctl,
    .receive_buf = slip_receive_buf,
};

/* "State transition" = changing ldisc */
int tty_set_ldisc(struct tty_struct *tty, int ldisc)
{
    /* [KEY] This is a STATE TRANSITION */
    /* The tty now behaves completely differently */
    
    old_ldisc = tty->ldisc;
    
    /* Close old "state" */
    if (old_ldisc->ops->close)
        old_ldisc->ops->close(tty);
    
    /* Enter new "state" */
    tty->ldisc = new_ldisc;
    
    if (new_ldisc->ops->open)
        new_ldisc->ops->open(tty);
}
```

**Why no enum state?**
- Each line discipline has completely different behavior
- N_TTY does line editing, PPP does framing, SLIP does different framing
- A switch statement would be massive and unmaintainable
- New line disciplines can be added as modules

**说明:**
- TTY 的行规则（line discipline）决定 TTY 如何处理数据
- 切换行规则就是切换状态
- 每个行规则有完全不同的行为（N_TTY 做行编辑，PPP 做帧处理）
- 使用 ops 模式允许新行规则作为模块添加

#### Example 2: TCP Congestion Control

```c
/* The ops table defining congestion behavior */
struct tcp_congestion_ops {
    struct list_head    list;
    unsigned long       flags;
    
    /* [KEY] These function pointers define the "state" behavior */
    void (*init)(struct sock *sk);
    void (*release)(struct sock *sk);
    u32  (*ssthresh)(struct sock *sk);
    void (*cong_avoid)(struct sock *sk, u32 ack, u32 in_flight);
    void (*set_state)(struct sock *sk, u8 new_state);
    void (*cwnd_event)(struct sock *sk, enum tcp_ca_event ev);
    void (*pkts_acked)(struct sock *sk, u32 num_acked, s32 rtt_us);
    u32  (*min_cwnd)(const struct sock *sk);
    u32  (*undo_cwnd)(struct sock *sk);
    
    char            name[TCP_CA_NAME_MAX];
    struct module   *owner;
};

/* Different "states" (congestion algorithms) */
struct tcp_congestion_ops tcp_reno = {
    .name       = "reno",
    .ssthresh   = tcp_reno_ssthresh,
    .cong_avoid = tcp_reno_cong_avoid,
    .min_cwnd   = tcp_reno_min_cwnd,
};

struct tcp_congestion_ops cubictcp = {
    .name       = "cubic",
    .init       = bictcp_init,
    .ssthresh   = bictcp_recalc_ssthresh,
    .cong_avoid = bictcp_cong_avoid,
    .set_state  = bictcp_state,
    .undo_cwnd  = bictcp_undo_cwnd,
    .pkts_acked = bictcp_acked,
};

struct tcp_congestion_ops tcp_vegas = {
    .name       = "vegas",
    .init       = vegas_init,
    .ssthresh   = tcp_reno_ssthresh,
    .cong_avoid = vegas_cong_avoid,
    .pkts_acked = vegas_acked,
    .set_state  = vegas_state,
    .cwnd_event = vegas_cwnd_event,
};

/* Context object stores ops pointer */
struct inet_connection_sock {
    struct inet_sock            icsk_inet;
    const struct tcp_congestion_ops *icsk_ca_ops;  /* [KEY] "State" */
    u32                         icsk_ca_priv[16];   /* Private data */
    /* ... */
};

/* "State transition" = algorithm change */
void tcp_set_congestion_control(struct sock *sk, const char *name)
{
    struct tcp_congestion_ops *new_ca;
    
    new_ca = tcp_ca_find(name);
    if (!new_ca)
        return;
    
    /* [KEY] This is a STATE TRANSITION */
    if (icsk->icsk_ca_ops->release)
        icsk->icsk_ca_ops->release(sk);
    
    icsk->icsk_ca_ops = new_ca;
    
    if (new_ca->init)
        new_ca->init(sk);
}
```

**说明:**
- 拥塞控制算法决定 TCP 如何调整发送速率
- 切换算法（Reno → CUBIC → Vegas）就是状态转换
- 每个算法有自己的私有数据存储在 `icsk_ca_priv`
- 可以运行时动态切换算法

#### Example 3: Network Protocol Family

```c
/* Protocol operations - behavior changes based on protocol "state" */
struct proto_ops {
    int     family;
    int     (*release)(struct socket *sock);
    int     (*bind)(struct socket *sock, struct sockaddr *myaddr, int sockaddr_len);
    int     (*connect)(struct socket *sock, struct sockaddr *vaddr, int sockaddr_len, int flags);
    int     (*accept)(struct socket *sock, struct socket *newsock, int flags);
    int     (*sendmsg)(struct kiocb *iocb, struct socket *sock, struct msghdr *m, size_t total_len);
    int     (*recvmsg)(struct kiocb *iocb, struct socket *sock, struct msghdr *m, size_t total_len, int flags);
    /* ... */
};

/* Different "states" (protocol families) */
const struct proto_ops inet_stream_ops = {      /* TCP */
    .family     = PF_INET,
    .bind       = inet_bind,
    .connect    = inet_stream_connect,
    .sendmsg    = tcp_sendmsg,
    .recvmsg    = tcp_recvmsg,
};

const struct proto_ops inet_dgram_ops = {       /* UDP */
    .family     = PF_INET,
    .bind       = inet_bind,
    .connect    = inet_dgram_connect,
    .sendmsg    = udp_sendmsg,
    .recvmsg    = udp_recvmsg,
};

const struct proto_ops unix_stream_ops = {      /* Unix socket (stream) */
    .family     = PF_UNIX,
    .bind       = unix_bind,
    .connect    = unix_stream_connect,
    .sendmsg    = unix_stream_sendmsg,
    .recvmsg    = unix_stream_recvmsg,
};

/* The socket "state" is determined at creation */
struct socket {
    socket_state            state;      /* enum state too! */
    short                   type;
    const struct proto_ops  *ops;       /* [KEY] Behavior "state" */
    struct sock             *sk;
};

/* socket() system call sets the ops */
int sock_create(int family, int type, int protocol, struct socket **res)
{
    /* [KEY] ops is set based on family+type+protocol */
    /* This determines the socket's behavioral "state" forever */
    
    sock->ops = pf->create(net, sock, protocol, kern);
}
```

### 3.3 Why switch(state) is Avoided

| Reason | Explanation |
|--------|-------------|
| **Open for extension** | New line disciplines/algorithms can be modules |
| **Behavior encapsulation** | Each "state" is self-contained in its ops |
| **No central modification** | Adding state doesn't require touching dispatcher |
| **Private data** | Each state can have its own data structure |
| **Clean separation** | No 1000-line switch statements |

**说明:**
- ops 模式避免巨大的 switch 语句
- 新状态可以作为模块添加，无需修改核心代码
- 每个状态封装自己的行为和数据

### 3.4 Userspace Simulation: Ops-Based State Pattern

This example simulates line disciplines (like TTY) and congestion control (like TCP):

```c
/*
 * ops_state_simulation.c
 * 
 * Userspace simulation of ops-based state pattern.
 * Demonstrates: state as behavior (function pointers), hot-swappable algorithms.
 * 
 * Compile: gcc -o ops_state ops_state_simulation.c
 * Run: ./ops_state
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/*
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │                  OPS-BASED STATE: "STATE AS BEHAVIOR"                    │
 * ├─────────────────────────────────────────────────────────────────────────┤
 * │                                                                         │
 * │   Traditional enum FSM:              Ops-based FSM:                     │
 * │   ─────────────────────              ─────────────────                  │
 * │                                                                         │
 * │   struct context {                   struct context {                   │
 * │       enum state state;     vs           const struct ops *ops;         │
 * │   };                                 };                                 │
 * │                                                                         │
 * │   switch(ctx->state) {               ctx->ops->handle(ctx, data);       │
 * │       case A: ... break;                                                │
 * │       case B: ... break;             // Behavior IS the state           │
 * │   }                                  // No switch needed                │
 * │                                      // Change ops = change state       │
 * │                                                                         │
 * └─────────────────────────────────────────────────────────────────────────┘
 *
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │              EXAMPLE 1: STREAM PROCESSOR (Like Line Discipline)          │
 * ├─────────────────────────────────────────────────────────────────────────┤
 * │                                                                         │
 * │   ┌──────────────┐                                                      │
 * │   │   RAW DATA   │                                                      │
 * │   └──────┬───────┘                                                      │
 * │          │                                                              │
 * │          ▼                                                              │
 * │   ┌──────────────┐     ops = &raw_ops      No processing               │
 * │   │  PROCESSOR   │ ───────────────────► ──────────────────►            │
 * │   │  (context)   │                                                      │
 * │   │              │     ops = &line_ops     Line editing                │
 * │   │  ops ────────┼───────────────────► ──────────────────►            │
 * │   │              │                                                      │
 * │   │              │     ops = &frame_ops    Packet framing              │
 * │   └──────────────┘ ───────────────────► ──────────────────►            │
 * │                                                                         │
 * │   Change ops pointer = Change processing behavior = STATE TRANSITION    │
 * │                                                                         │
 * └─────────────────────────────────────────────────────────────────────────┘
 */

/* =========================================================================
 * PART 1: STREAM PROCESSOR (Like TTY Line Discipline)
 * ========================================================================= */

/* Forward declaration */
struct stream_processor;

/* [KEY] Ops table defines behavioral "state" */
typedef struct stream_ops {
    const char *name;
    
    /* Lifecycle */
    int  (*open)(struct stream_processor *sp);
    void (*close)(struct stream_processor *sp);
    
    /* Data processing */
    int  (*receive)(struct stream_processor *sp, const char *data, int len);
    int  (*transmit)(struct stream_processor *sp, const char *data, int len);
    
    /* Query */
    int  (*bytes_ready)(struct stream_processor *sp);
} stream_ops_t;

/* [KEY] Context object - ops pointer IS the state */
typedef struct stream_processor {
    const stream_ops_t  *ops;           /* [KEY] Current "state" (behavior) */
    void                *private_data;  /* Processor-specific data */
    
    /* Buffering */
    char    buffer[256];
    int     buf_pos;
    
    /* Output callback */
    void    (*output)(const char *data, int len);
} stream_processor_t;

/* =========================================================================
 * "State" 1: RAW processor (like N_TTY raw mode)
 * No processing, pass through directly
 * ========================================================================= */

static int raw_open(stream_processor_t *sp)
{
    printf("  [RAW] Opened - no processing mode\n");
    return 0;
}

static void raw_close(stream_processor_t *sp)
{
    printf("  [RAW] Closed\n");
}

static int raw_receive(stream_processor_t *sp, const char *data, int len)
{
    /* [KEY] Raw mode: pass through unchanged */
    printf("  [RAW] Received %d bytes, passing through\n", len);
    if (sp->output) {
        sp->output(data, len);
    }
    return len;
}

static int raw_transmit(stream_processor_t *sp, const char *data, int len)
{
    printf("  [RAW] Transmit %d bytes\n", len);
    return len;
}

static int raw_bytes_ready(stream_processor_t *sp)
{
    return 0;  /* Always empty, immediate passthrough */
}

/* [KEY] "State object" - behavior definition */
static const stream_ops_t raw_ops = {
    .name        = "RAW",
    .open        = raw_open,
    .close       = raw_close,
    .receive     = raw_receive,
    .transmit    = raw_transmit,
    .bytes_ready = raw_bytes_ready,
};

/* =========================================================================
 * "State" 2: LINE processor (like N_TTY cooked mode)
 * Buffers until newline, provides line editing
 * ========================================================================= */

/* Private data for line processor */
typedef struct {
    int echo_enabled;
    int line_count;
} line_private_t;

static int line_open(stream_processor_t *sp)
{
    line_private_t *priv = malloc(sizeof(line_private_t));
    priv->echo_enabled = 1;
    priv->line_count = 0;
    sp->private_data = priv;
    sp->buf_pos = 0;
    
    printf("  [LINE] Opened - line buffering mode, echo=%s\n",
           priv->echo_enabled ? "on" : "off");
    return 0;
}

static void line_close(stream_processor_t *sp)
{
    line_private_t *priv = sp->private_data;
    printf("  [LINE] Closed - processed %d lines\n", priv->line_count);
    free(priv);
    sp->private_data = NULL;
}

static int line_receive(stream_processor_t *sp, const char *data, int len)
{
    line_private_t *priv = sp->private_data;
    
    /* [KEY] Line mode: buffer until newline */
    for (int i = 0; i < len; i++) {
        char c = data[i];
        
        /* Handle backspace (simple line editing) */
        if (c == '\b' || c == 127) {
            if (sp->buf_pos > 0) {
                sp->buf_pos--;
                printf("  [LINE] Backspace\n");
            }
            continue;
        }
        
        /* Buffer character */
        if (sp->buf_pos < sizeof(sp->buffer) - 1) {
            sp->buffer[sp->buf_pos++] = c;
        }
        
        /* Complete line on newline */
        if (c == '\n') {
            sp->buffer[sp->buf_pos] = '\0';
            priv->line_count++;
            
            printf("  [LINE] Complete line: \"%.*s\"\n", 
                   sp->buf_pos - 1, sp->buffer);
            
            if (sp->output) {
                sp->output(sp->buffer, sp->buf_pos);
            }
            sp->buf_pos = 0;
        }
    }
    
    return len;
}

static int line_transmit(stream_processor_t *sp, const char *data, int len)
{
    printf("  [LINE] Transmit %d bytes\n", len);
    return len;
}

static int line_bytes_ready(stream_processor_t *sp)
{
    return sp->buf_pos;
}

static const stream_ops_t line_ops = {
    .name        = "LINE",
    .open        = line_open,
    .close       = line_close,
    .receive     = line_receive,
    .transmit    = line_transmit,
    .bytes_ready = line_bytes_ready,
};

/* =========================================================================
 * "State" 3: FRAME processor (like SLIP/PPP)
 * Detects frame boundaries with escape sequences
 * ========================================================================= */

#define FRAME_START 0x7E
#define FRAME_ESC   0x7D

typedef struct {
    int frames_received;
    bool in_frame;
    bool escaped;
} frame_private_t;

static int frame_open(stream_processor_t *sp)
{
    frame_private_t *priv = malloc(sizeof(frame_private_t));
    priv->frames_received = 0;
    priv->in_frame = false;
    priv->escaped = false;
    sp->private_data = priv;
    sp->buf_pos = 0;
    
    printf("  [FRAME] Opened - packet framing mode\n");
    return 0;
}

static void frame_close(stream_processor_t *sp)
{
    frame_private_t *priv = sp->private_data;
    printf("  [FRAME] Closed - received %d frames\n", priv->frames_received);
    free(priv);
    sp->private_data = NULL;
}

static int frame_receive(stream_processor_t *sp, const char *data, int len)
{
    frame_private_t *priv = sp->private_data;
    
    /* [KEY] Frame mode: detect frame boundaries */
    for (int i = 0; i < len; i++) {
        unsigned char c = (unsigned char)data[i];
        
        if (c == FRAME_START) {
            if (priv->in_frame && sp->buf_pos > 0) {
                /* End of frame */
                sp->buffer[sp->buf_pos] = '\0';
                priv->frames_received++;
                
                printf("  [FRAME] Complete frame (%d bytes): ", sp->buf_pos);
                for (int j = 0; j < sp->buf_pos; j++) {
                    printf("%02X ", (unsigned char)sp->buffer[j]);
                }
                printf("\n");
                
                if (sp->output) {
                    sp->output(sp->buffer, sp->buf_pos);
                }
            }
            /* Start new frame */
            priv->in_frame = true;
            priv->escaped = false;
            sp->buf_pos = 0;
            continue;
        }
        
        if (!priv->in_frame) continue;
        
        if (c == FRAME_ESC) {
            priv->escaped = true;
            continue;
        }
        
        if (priv->escaped) {
            c ^= 0x20;  /* Unescape */
            priv->escaped = false;
        }
        
        if (sp->buf_pos < sizeof(sp->buffer) - 1) {
            sp->buffer[sp->buf_pos++] = c;
        }
    }
    
    return len;
}

static int frame_transmit(stream_processor_t *sp, const char *data, int len)
{
    printf("  [FRAME] Transmit frame (%d bytes)\n", len);
    return len;
}

static int frame_bytes_ready(stream_processor_t *sp)
{
    return sp->buf_pos;
}

static const stream_ops_t frame_ops = {
    .name        = "FRAME",
    .open        = frame_open,
    .close       = frame_close,
    .receive     = frame_receive,
    .transmit    = frame_transmit,
    .bytes_ready = frame_bytes_ready,
};

/* =========================================================================
 * Stream processor lifecycle (like tty_set_ldisc)
 * ========================================================================= */

/*
 * [KEY] "State transition" = changing the ops pointer
 * Like tty_set_ldisc() in the kernel
 */
static int stream_set_processor(stream_processor_t *sp, const stream_ops_t *new_ops)
{
    printf("\n=== Switching processor: %s -> %s ===\n",
           sp->ops ? sp->ops->name : "NULL", new_ops->name);
    
    /* [KEY] Close old "state" */
    if (sp->ops && sp->ops->close) {
        sp->ops->close(sp);
    }
    
    /* [KEY] Change "state" (ops pointer) */
    sp->ops = new_ops;
    sp->buf_pos = 0;
    
    /* [KEY] Open new "state" */
    if (sp->ops->open) {
        return sp->ops->open(sp);
    }
    
    return 0;
}

/* Process data through current processor */
static int stream_receive(stream_processor_t *sp, const char *data, int len)
{
    if (!sp->ops || !sp->ops->receive) {
        return -1;
    }
    
    /* [KEY] Dispatch through ops - no switch statement! */
    return sp->ops->receive(sp, data, len);
}

static stream_processor_t *stream_create(void)
{
    stream_processor_t *sp = calloc(1, sizeof(stream_processor_t));
    return sp;
}

static void stream_destroy(stream_processor_t *sp)
{
    if (sp->ops && sp->ops->close) {
        sp->ops->close(sp);
    }
    free(sp);
}

/* =========================================================================
 * PART 2: RATE CONTROLLER (Like TCP Congestion Control)
 * ========================================================================= */

/*
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │           EXAMPLE 2: RATE CONTROLLER (Like Congestion Control)           │
 * ├─────────────────────────────────────────────────────────────────────────┤
 * │                                                                         │
 * │   ┌──────────────────────────────────────────────────────────┐          │
 * │   │                     RATE CONTROLLER                       │          │
 * │   │  ┌─────────────────────────────────────────────────────┐ │          │
 * │   │  │  ca_ops = &reno_ops     │  ca_priv[16]              │ │          │
 * │   │  │      ↓                  │     ↓                     │ │          │
 * │   │  │  "Slow start"           │  Algorithm-specific       │ │          │
 * │   │  │  "Congestion avoid"     │  data storage             │ │          │
 * │   │  └─────────────────────────────────────────────────────┘ │          │
 * │   └──────────────────────────────────────────────────────────┘          │
 * │                                                                         │
 * │   Different algorithms (Reno, CUBIC, Vegas) = Different "states"        │
 * │   Each has different behavior AND different private data layout         │
 * │                                                                         │
 * └─────────────────────────────────────────────────────────────────────────┘
 */

struct rate_controller;

/* [KEY] Congestion control ops (like tcp_congestion_ops) */
typedef struct rate_ops {
    const char *name;
    
    void (*init)(struct rate_controller *rc);
    void (*release)(struct rate_controller *rc);
    
    /* [KEY] Algorithm-specific behavior */
    int  (*get_rate)(struct rate_controller *rc);
    void (*on_ack)(struct rate_controller *rc, int bytes_acked);
    void (*on_loss)(struct rate_controller *rc);
} rate_ops_t;

/* [KEY] Context with inline private storage (like icsk_ca_priv) */
typedef struct rate_controller {
    const rate_ops_t    *ops;           /* [KEY] Algorithm "state" */
    int                 ca_priv[16];    /* [KEY] Algorithm private data */
    
    /* Shared state */
    int                 current_rate;
    int                 max_rate;
    int                 total_acked;
    int                 loss_count;
} rate_controller_t;

/* =========================================================================
 * Algorithm 1: Simple Reno-like
 * ========================================================================= */

/* Private data layout for Reno */
typedef struct {
    int cwnd;           /* Congestion window */
    int ssthresh;       /* Slow start threshold */
    bool slow_start;    /* In slow start phase? */
} reno_priv_t;

#define RENO_PRIV(rc) ((reno_priv_t *)((rc)->ca_priv))

static void reno_init(rate_controller_t *rc)
{
    reno_priv_t *priv = RENO_PRIV(rc);
    priv->cwnd = 1;
    priv->ssthresh = 64;
    priv->slow_start = true;
    
    printf("  [RENO] Init: cwnd=%d, ssthresh=%d\n", priv->cwnd, priv->ssthresh);
}

static void reno_release(rate_controller_t *rc)
{
    reno_priv_t *priv = RENO_PRIV(rc);
    printf("  [RENO] Release: final cwnd=%d\n", priv->cwnd);
}

static int reno_get_rate(rate_controller_t *rc)
{
    reno_priv_t *priv = RENO_PRIV(rc);
    return priv->cwnd * 1000;  /* Rate = cwnd * 1000 bytes/sec */
}

static void reno_on_ack(rate_controller_t *rc, int bytes)
{
    reno_priv_t *priv = RENO_PRIV(rc);
    
    if (priv->slow_start) {
        /* [KEY] Exponential growth in slow start */
        priv->cwnd++;
        if (priv->cwnd >= priv->ssthresh) {
            priv->slow_start = false;
            printf("  [RENO] Exiting slow start, cwnd=%d\n", priv->cwnd);
        }
    } else {
        /* [KEY] Linear growth in congestion avoidance */
        static int ack_count = 0;
        ack_count++;
        if (ack_count >= priv->cwnd) {
            priv->cwnd++;
            ack_count = 0;
        }
    }
}

static void reno_on_loss(rate_controller_t *rc)
{
    reno_priv_t *priv = RENO_PRIV(rc);
    
    /* [KEY] Multiplicative decrease on loss */
    priv->ssthresh = priv->cwnd / 2;
    if (priv->ssthresh < 2) priv->ssthresh = 2;
    priv->cwnd = priv->ssthresh;
    priv->slow_start = false;
    
    printf("  [RENO] Loss detected: cwnd=%d, ssthresh=%d\n", 
           priv->cwnd, priv->ssthresh);
}

static const rate_ops_t reno_ops = {
    .name     = "RENO",
    .init     = reno_init,
    .release  = reno_release,
    .get_rate = reno_get_rate,
    .on_ack   = reno_on_ack,
    .on_loss  = reno_on_loss,
};

/* =========================================================================
 * Algorithm 2: CUBIC-like (simplified)
 * ========================================================================= */

typedef struct {
    int cwnd;
    int cwnd_max;       /* Window size before last loss */
    int epoch_start;    /* Time of last loss */
    int time_now;       /* Simulated time */
} cubic_priv_t;

#define CUBIC_PRIV(rc) ((cubic_priv_t *)((rc)->ca_priv))

static void cubic_init(rate_controller_t *rc)
{
    cubic_priv_t *priv = CUBIC_PRIV(rc);
    priv->cwnd = 1;
    priv->cwnd_max = 64;
    priv->epoch_start = 0;
    priv->time_now = 0;
    
    printf("  [CUBIC] Init: cwnd=%d, cwnd_max=%d\n", priv->cwnd, priv->cwnd_max);
}

static void cubic_release(rate_controller_t *rc)
{
    cubic_priv_t *priv = CUBIC_PRIV(rc);
    printf("  [CUBIC] Release: final cwnd=%d\n", priv->cwnd);
}

static int cubic_get_rate(rate_controller_t *rc)
{
    cubic_priv_t *priv = CUBIC_PRIV(rc);
    return priv->cwnd * 1000;
}

static void cubic_on_ack(rate_controller_t *rc, int bytes)
{
    cubic_priv_t *priv = CUBIC_PRIV(rc);
    priv->time_now++;
    
    /* [KEY] CUBIC function: W = C*(t-K)^3 + Wmax */
    /* Simplified: grow faster as we approach Wmax */
    int t = priv->time_now - priv->epoch_start;
    int target = priv->cwnd_max;
    
    if (priv->cwnd < target) {
        /* Below target: grow aggressively */
        priv->cwnd += 2;
    } else {
        /* Above target: grow slowly */
        priv->cwnd++;
    }
}

static void cubic_on_loss(rate_controller_t *rc)
{
    cubic_priv_t *priv = CUBIC_PRIV(rc);
    
    /* [KEY] Remember maximum before loss */
    priv->cwnd_max = priv->cwnd;
    priv->cwnd = priv->cwnd * 7 / 10;  /* 0.7x reduction */
    priv->epoch_start = priv->time_now;
    
    printf("  [CUBIC] Loss detected: cwnd=%d, cwnd_max=%d\n",
           priv->cwnd, priv->cwnd_max);
}

static const rate_ops_t cubic_ops = {
    .name     = "CUBIC",
    .init     = cubic_init,
    .release  = cubic_release,
    .get_rate = cubic_get_rate,
    .on_ack   = cubic_on_ack,
    .on_loss  = cubic_on_loss,
};

/* =========================================================================
 * Rate controller lifecycle (like tcp_set_congestion_control)
 * ========================================================================= */

static int rate_set_algorithm(rate_controller_t *rc, const rate_ops_t *new_ops)
{
    printf("\n=== Switching algorithm: %s -> %s ===\n",
           rc->ops ? rc->ops->name : "NULL", new_ops->name);
    
    /* [KEY] Release old algorithm */
    if (rc->ops && rc->ops->release) {
        rc->ops->release(rc);
    }
    
    /* [KEY] Clear private data */
    memset(rc->ca_priv, 0, sizeof(rc->ca_priv));
    
    /* [KEY] Change algorithm */
    rc->ops = new_ops;
    
    /* [KEY] Initialize new algorithm */
    if (rc->ops->init) {
        rc->ops->init(rc);
    }
    
    return 0;
}

/* =========================================================================
 * Demonstration
 * ========================================================================= */

static void output_callback(const char *data, int len)
{
    printf("  [OUTPUT] %d bytes\n", len);
}

static void demo_stream_processor(void)
{
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════════╗\n");
    printf("║           STREAM PROCESSOR DEMO (Like Line Discipline)            ║\n");
    printf("╚═══════════════════════════════════════════════════════════════════╝\n");
    
    stream_processor_t *sp = stream_create();
    sp->output = output_callback;
    
    /* Start with RAW mode */
    stream_set_processor(sp, &raw_ops);
    stream_receive(sp, "Hello", 5);
    
    /* Switch to LINE mode */
    stream_set_processor(sp, &line_ops);
    stream_receive(sp, "Hello\n", 6);
    stream_receive(sp, "Wor", 3);
    stream_receive(sp, "ld!\n", 4);
    
    /* Switch to FRAME mode */
    stream_set_processor(sp, &frame_ops);
    char frame[] = { 0x7E, 'A', 'B', 'C', 0x7E, 0x7E, 'X', 'Y', 0x7E };
    stream_receive(sp, frame, sizeof(frame));
    
    stream_destroy(sp);
}

static void demo_rate_controller(void)
{
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════════╗\n");
    printf("║         RATE CONTROLLER DEMO (Like Congestion Control)            ║\n");
    printf("╚═══════════════════════════════════════════════════════════════════╝\n");
    
    rate_controller_t rc = {0};
    rc.max_rate = 100000;
    
    /* Start with Reno */
    rate_set_algorithm(&rc, &reno_ops);
    
    printf("\n--- Simulating ACKs (Reno) ---\n");
    for (int i = 0; i < 10; i++) {
        rc.ops->on_ack(&rc, 1000);
        printf("  Rate: %d bytes/sec\n", rc.ops->get_rate(&rc));
    }
    
    printf("\n--- Simulating loss (Reno) ---\n");
    rc.ops->on_loss(&rc);
    printf("  Rate after loss: %d bytes/sec\n", rc.ops->get_rate(&rc));
    
    /* Switch to CUBIC (live!) */
    rate_set_algorithm(&rc, &cubic_ops);
    
    printf("\n--- Simulating ACKs (CUBIC) ---\n");
    for (int i = 0; i < 10; i++) {
        rc.ops->on_ack(&rc, 1000);
        printf("  Rate: %d bytes/sec\n", rc.ops->get_rate(&rc));
    }
    
    printf("\n--- Simulating loss (CUBIC) ---\n");
    rc.ops->on_loss(&rc);
    printf("  Rate after loss: %d bytes/sec\n", rc.ops->get_rate(&rc));
    
    if (rc.ops->release) {
        rc.ops->release(&rc);
    }
}

int main(void)
{
    printf("Ops-Based State Pattern Simulation\n");
    printf("===================================\n");
    
    demo_stream_processor();
    demo_rate_controller();
    
    printf("\n=== Key Insights ===\n");
    printf("1. State is BEHAVIOR (ops pointer), not a value\n");
    printf("2. Change ops = Change state (no switch needed)\n");
    printf("3. Private data stored inline (like icsk_ca_priv)\n");
    printf("4. New algorithms can be added without core changes\n");
    
    return 0;
}
```

**Pattern summary:**

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    OPS-BASED STATE PATTERN COMPONENTS                        │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  1. OPS TABLE (Behavior Definition):                                        │
│     struct xxx_ops {                                                        │
│         void (*operation1)(ctx, ...);                                       │
│         void (*operation2)(ctx, ...);                                       │
│     };                                                                      │
│                                                                             │
│  2. STATE OBJECTS (Different Behaviors):                                    │
│     static const struct xxx_ops behavior_a = { .op1 = a_op1, ... };         │
│     static const struct xxx_ops behavior_b = { .op1 = b_op1, ... };         │
│                                                                             │
│  3. CONTEXT (Holds Current State):                                          │
│     struct context {                                                        │
│         const struct xxx_ops *ops;    /* Current behavior */                │
│         int priv[16];                 /* Private data */                    │
│     };                                                                      │
│                                                                             │
│  4. STATE TRANSITION (Change Ops):                                          │
│     void set_behavior(ctx, new_ops) {                                       │
│         ctx->ops->close(ctx);         /* Exit old state */                  │
│         ctx->ops = new_ops;           /* Change state */                    │
│         ctx->ops->open(ctx);          /* Enter new state */                 │
│     }                                                                       │
│                                                                             │
│  5. DISPATCH (No Switch!):                                                  │
│     ctx->ops->operation(ctx, data);   /* Direct call */                     │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

**说明:**
- 状态就是行为（ops 指针），不是枚举值
- 切换 ops 就是状态转换
- 私有数据可以内联存储（如 `ca_priv[16]`）
- 新算法可以像模块一样添加，无需修改核心代码
- 没有 switch 语句，直接通过函数指针调用

---

## Phase 4 — Hybrid FSMs (Enum + Ops + Events)

### 4.1 The Hybrid Pattern

```
HYBRID FSM: Best of Both Worlds

┌─────────────────────────────────────────────────────────────────────────────┐
│                                                                             │
│   enum state ────────► Coarse-grained state (visible, debuggable)           │
│       +                                                                     │
│   ops table ─────────► Fine-grained behavior (extensible, encapsulated)     │
│       +                                                                     │
│   event dispatch ────► Transition logic (predictable, auditable)            │
│                                                                             │
│   Example:                                                                  │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │                                                                     │   │
│   │    struct sock {                                                    │   │
│   │        int sk_state;              // enum: LISTEN, ESTABLISHED...   │   │
│   │        struct proto *sk_prot;     // ops: tcp_prot, udp_prot...     │   │
│   │    };                                                               │   │
│   │                                                                     │   │
│   │    TCP socket in ESTABLISHED state using CUBIC congestion:          │   │
│   │    • sk_state = TCP_ESTABLISHED  (enum, visible to netstat)         │   │
│   │    • sk_prot = &tcp_prot         (ops, TCP behavior)                │   │
│   │    • icsk_ca_ops = &cubictcp     (ops, congestion behavior)         │   │
│   │                                                                     │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

**说明:**
- 混合 FSM 结合了两种方法的优点
- enum 跟踪粗粒度状态（可调试、可见）
- ops 定义细粒度行为（可扩展、封装）

### 4.2 Hybrid FSM Example 1: TCP Socket

```c
/* TCP uses BOTH enum state AND multiple ops tables */

struct sock {
    /* [ENUM STATE] Connection state - RFC 793 */
    volatile unsigned char  sk_state;       /* TCP_LISTEN, TCP_ESTABLISHED, ... */
    
    /* [OPS 1] Transport protocol behavior */
    struct proto           *sk_prot;        /* tcp_prot or udp_prot */
};

struct inet_connection_sock {
    struct inet_sock            icsk_inet;
    
    /* [OPS 2] Address family behavior */
    const struct inet_connection_sock_af_ops *icsk_af_ops;  /* IPv4 or IPv6 */
    
    /* [OPS 3] Congestion control behavior */
    const struct tcp_congestion_ops *icsk_ca_ops;           /* reno, cubic, vegas */
    
    /* [ENUM STATE 2] Congestion state */
    __u8                        icsk_ca_state;              /* Open, Disorder, CWR, Recovery, Loss */
};

/*
 * ORTHOGONAL STATE AXES:
 * 
 * 1. Connection state (enum sk_state):
 *    CLOSED → LISTEN → SYN_RECV → ESTABLISHED → FIN_WAIT1 → ...
 *    
 * 2. Protocol (ops sk_prot):
 *    tcp_prot | udp_prot | sctp_prot | ...
 *    
 * 3. Address family (ops icsk_af_ops):
 *    ipv4_specific | ipv6_specific
 *    
 * 4. Congestion algorithm (ops icsk_ca_ops):
 *    tcp_reno | cubictcp | tcp_vegas | ...
 *    
 * 5. Congestion phase (enum icsk_ca_state):
 *    TCP_CA_Open | TCP_CA_Disorder | TCP_CA_CWR | TCP_CA_Recovery | TCP_CA_Loss
 *    
 * Each axis changes independently!
 */
```

### 4.3 Hybrid FSM Example 2: SCTP Protocol

```c
/* SCTP uses table-driven dispatch with state enum and ops */

/* [ENUM STATE] */
typedef enum {
    SCTP_STATE_CLOSED            = 0,
    SCTP_STATE_COOKIE_WAIT       = 1,
    SCTP_STATE_COOKIE_ECHOED     = 2,
    SCTP_STATE_ESTABLISHED       = 3,
    SCTP_STATE_SHUTDOWN_PENDING  = 4,
    SCTP_STATE_SHUTDOWN_SENT     = 5,
    SCTP_STATE_SHUTDOWN_RECEIVED = 6,
    SCTP_STATE_SHUTDOWN_ACK_SENT = 7,
} sctp_state_t;

/* [EVENT TYPES] */
typedef enum {
    SCTP_EVENT_T_CHUNK = 1,      /* Incoming SCTP chunk */
    SCTP_EVENT_T_TIMEOUT,        /* Timer expiration */
    SCTP_EVENT_T_OTHER,          /* Other events */
    SCTP_EVENT_T_PRIMITIVE,      /* API primitives */
} sctp_event_t;

/* [OPS PATTERN] Each [event][state] cell contains a function pointer */
typedef struct {
    sctp_state_fn_t *fn;         /* Handler function */
    const char *name;            /* Debug name */
} sctp_sm_table_entry_t;

/* [2D TABLE] Dispatch table */
static const sctp_sm_table_entry_t
chunk_event_table[SCTP_NUM_CHUNK_TYPES][SCTP_STATE_NUM_STATES];

/* [DISPATCH] */
const sctp_sm_table_entry_t *sctp_sm_lookup_event(
    sctp_event_t event_type,
    sctp_state_t state,
    sctp_subtype_t event_subtype)
{
    switch (event_type) {
    case SCTP_EVENT_T_CHUNK:
        return sctp_chunk_event_lookup(event_subtype.chunk, state);
    case SCTP_EVENT_T_TIMEOUT:
        return &timeout_event_table[event_subtype.timeout][(int)state];
    case SCTP_EVENT_T_OTHER:
        return &other_event_table[event_subtype.other][(int)state];
    case SCTP_EVENT_T_PRIMITIVE:
        return &primitive_event_table[event_subtype.primitive][(int)state];
    }
    return &bug;
}

/* [HANDLER INVOCATION] */
sctp_disposition_t sctp_do_sm(sctp_event_t event_type, sctp_subtype_t subtype,
                               sctp_state_t state, ...)
{
    const sctp_sm_table_entry_t *state_fn;
    
    /* Lookup handler */
    state_fn = sctp_sm_lookup_event(event_type, state, subtype);
    
    /* [KEY] Invoke handler - ops-like dispatch */
    return state_fn->fn(ep, asoc, subtype, arg, commands);
}
```

**说明:**
- SCTP 使用 2D 表格：[事件类型][状态] → 处理函数
- 结合了 enum 状态（可调试）和函数指针（灵活）
- 每个 [事件, 状态] 组合有专门的处理函数

### 4.4 Hybrid FSM Example 3: Block I/O Request

```c
/* Block request has enum phase AND ops for driver-specific behavior */

/* [ENUM STATE] Request phase */
enum rq_cmd_type_bits {
    REQ_TYPE_FS             = 1,    /* Filesystem request */
    REQ_TYPE_BLOCK_PC       = 2,    /* SCSI command passthrough */
    REQ_TYPE_SENSE          = 3,    /* Sense data request */
    REQ_TYPE_PM_SUSPEND     = 4,    /* Power management */
    REQ_TYPE_PM_RESUME      = 5,
    REQ_TYPE_PM_SHUTDOWN    = 6,
    REQ_TYPE_SPECIAL        = 7,
};

/* [OPS] Scheduler behavior */
struct elevator_ops {
    elevator_merge_fn *elevator_merge_fn;
    elevator_merged_fn *elevator_merged_fn;
    elevator_dispatch_fn *elevator_dispatch_fn;
    elevator_add_req_fn *elevator_add_req_fn;
    elevator_former_req_fn *elevator_former_req_fn;
    elevator_latter_req_fn *elevator_latter_req_fn;
    /* ... */
};

/* [OPS] Driver behavior */
struct request_queue {
    struct list_head        queue_head;
    struct elevator_queue   *elevator;     /* [OPS] Scheduler */
    make_request_fn         *make_request_fn;  /* [OPS] Request creation */
    prep_rq_fn              *prep_rq_fn;   /* [OPS] Prepare request */
    unprep_rq_fn            *unprep_rq_fn; /* [OPS] Unprepare */
    /* ... */
};

/* Transitions involve BOTH enum state change AND ops invocation */
void blk_finish_request(struct request *req, int error)
{
    /* [ENUM STATE] Request moves to completed */
    req->cmd_flags |= REQ_QUEUED;  /* State flag */
    
    /* [OPS] Driver-specific completion */
    if (req->q->complete)
        req->q->complete(req, error);
    
    /* [OPS] Scheduler notification */
    elv_completed_request(req->q, req);
}
```

### 4.5 How Illegal Transitions Are Prevented

```c
/* Multiple layers of protection */

/* Layer 1: State validation in tcp_rcv_state_process */
switch (sk->sk_state) {
case TCP_CLOSE:
    goto discard;  /* Can't process packets when closed */
    
case TCP_LISTEN:
    if (th->ack)
        return 1;  /* ACKs invalid in LISTEN */
    if (th->rst)
        goto discard;  /* RST invalid in LISTEN */
    /* Only SYN valid */
}

/* Layer 2: Assertions with WARN_ON */
static void tcp_set_state(struct sock *sk, int state)
{
    WARN_ON((1 << state) & ~TCPF_ALLOWED_STATES);  /* Validate new state */
}

/* Layer 3: Ops NULL checks */
if (sk->sk_prot->connect)
    err = sk->sk_prot->connect(sk, uaddr, addr_len);
else
    err = -EOPNOTSUPP;

/* Layer 4: State-specific ops invocation */
/* tcp_rcv_established() only called when sk_state == TCP_ESTABLISHED */
if (sk->sk_state == TCP_ESTABLISHED) {
    tcp_rcv_established(sk, skb, tcp_hdr(skb), skb->len);
} else {
    tcp_rcv_state_process(sk, skb, tcp_hdr(skb), skb->len);
}
```

### 4.6 Why Hybrid FSMs Scale Better

```
SCALABILITY COMPARISON:

┌─────────────────────────────────────────────────────────────────────────────┐
│                                                                             │
│  PURE ENUM FSM:                                                             │
│  ──────────────                                                             │
│                                                                             │
│    States: N                                                                │
│    Events: M                                                                │
│    Total cases: N × M                                                       │
│                                                                             │
│    Adding 1 state: Add M cases                                              │
│    Adding 1 event: Add N cases                                              │
│                                                                             │
│  HYBRID FSM:                                                                │
│  ───────────                                                                │
│                                                                             │
│    Enum states: N (coarse)                                                  │
│    Ops tables: K (orthogonal behaviors)                                     │
│    Events: M                                                                │
│                                                                             │
│    Total complexity: N × M + K ops tables                                   │
│                                                                             │
│    Adding behavior variant: Add 1 ops table (O(1))                          │
│    Adding coarse state: Add M cases (still N×M)                             │
│    But coarse states are FEWER                                              │
│                                                                             │
│  EXAMPLE: TCP                                                               │
│  ────────────                                                               │
│                                                                             │
│    If all combinations were enum:                                           │
│      11 connection states × 5 congestion states × 2 AF = 110 states         │
│                                                                             │
│    With hybrid:                                                             │
│      11 connection states (enum)                                            │
│      + 1 congestion ops table (swappable)                                   │
│      + 1 AF ops table (swappable)                                           │
│      = Much simpler!                                                        │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

**说明:**
- 纯 enum FSM 复杂度是 状态数 × 事件数
- 混合 FSM 将正交维度分离为独立的 ops 表
- 添加新行为变体只需添加一个 ops 表，无需修改状态机

### 4.7 Userspace Simulation: Hybrid FSM (Enum + Ops + Events)

This example simulates a network socket with multiple orthogonal state axes:

```c
/*
 * hybrid_fsm_simulation.c
 * 
 * Userspace simulation of hybrid FSM pattern (like TCP socket).
 * Demonstrates: enum state + multiple ops tables + event-driven transitions.
 * 
 * Compile: gcc -o hybrid_fsm hybrid_fsm_simulation.c
 * Run: ./hybrid_fsm
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

/*
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │                    HYBRID FSM: MULTIPLE STATE AXES                       │
 * ├─────────────────────────────────────────────────────────────────────────┤
 * │                                                                         │
 * │   ┌────────────────────────────────────────────────────────────────┐    │
 * │   │                    SMART SOCKET                                 │    │
 * │   ├────────────────────────────────────────────────────────────────┤    │
 * │   │                                                                │    │
 * │   │  AXIS 1: Connection State (enum)                               │    │
 * │   │  ─────────────────────────────────                             │    │
 * │   │  CLOSED → CONNECTING → CONNECTED → DISCONNECTING → CLOSED      │    │
 * │   │  [Visible to users, tracked with enum]                         │    │
 * │   │                                                                │    │
 * │   │  AXIS 2: Transport Protocol (ops)                              │    │
 * │   │  ────────────────────────────────                              │    │
 * │   │  tcp_transport_ops  |  udp_transport_ops  |  mock_transport    │    │
 * │   │  [Different send/recv behavior]                                │    │
 * │   │                                                                │    │
 * │   │  AXIS 3: Security Layer (ops)                                  │    │
 * │   │  ──────────────────────────────                                │    │
 * │   │  plain_security_ops  |  tls_security_ops                       │    │
 * │   │  [Different encryption behavior]                               │    │
 * │   │                                                                │    │
 * │   │  AXIS 4: Rate Control (ops)                                    │    │
 * │   │  ────────────────────────────                                  │    │
 * │   │  unlimited_rate_ops  |  token_bucket_ops  |  leaky_bucket_ops  │    │
 * │   │  [Different rate limiting behavior]                            │    │
 * │   │                                                                │    │
 * │   │  Each axis changes INDEPENDENTLY!                              │    │
 * │   │                                                                │    │
 * │   └────────────────────────────────────────────────────────────────┘    │
 * │                                                                         │
 * │   Without hybrid: 4 × 3 × 2 × 3 = 72 enum states!                       │
 * │   With hybrid: 4 enum states + 3 ops tables                            │
 * │                                                                         │
 * └─────────────────────────────────────────────────────────────────────────┘
 */

/* =========================================================================
 * AXIS 1: Connection State (enum) - like TCP sk_state
 * ========================================================================= */

typedef enum {
    CONN_CLOSED = 0,
    CONN_CONNECTING,
    CONN_CONNECTED,
    CONN_DISCONNECTING,
    CONN_MAX_STATES
} conn_state_t;

static const char *conn_state_names[] = {
    "CLOSED", "CONNECTING", "CONNECTED", "DISCONNECTING"
};

/* Connection events */
typedef enum {
    CONN_EVENT_CONNECT,
    CONN_EVENT_CONNECTED,
    CONN_EVENT_DISCONNECT,
    CONN_EVENT_DISCONNECTED,
    CONN_EVENT_ERROR,
} conn_event_t;

static const char *conn_event_names[] = {
    "CONNECT", "CONNECTED", "DISCONNECT", "DISCONNECTED", "ERROR"
};

/* =========================================================================
 * AXIS 2: Transport Protocol (ops) - like sk_prot
 * ========================================================================= */

struct smart_socket;  /* Forward declaration */

typedef struct transport_ops {
    const char *name;
    
    int  (*connect)(struct smart_socket *sock, const char *addr);
    int  (*send)(struct smart_socket *sock, const void *data, int len);
    int  (*recv)(struct smart_socket *sock, void *data, int len);
    void (*close)(struct smart_socket *sock);
} transport_ops_t;

/* TCP-like transport */
static int tcp_connect(struct smart_socket *sock, const char *addr)
{
    printf("    [TCP] Connecting to %s (3-way handshake)\n", addr);
    return 0;
}

static int tcp_send(struct smart_socket *sock, const void *data, int len)
{
    printf("    [TCP] Sending %d bytes (reliable, ordered)\n", len);
    return len;
}

static int tcp_recv(struct smart_socket *sock, void *data, int len)
{
    printf("    [TCP] Receiving up to %d bytes\n", len);
    return len;
}

static void tcp_close(struct smart_socket *sock)
{
    printf("    [TCP] Closing (FIN handshake)\n");
}

static const transport_ops_t tcp_transport_ops = {
    .name = "TCP",
    .connect = tcp_connect,
    .send = tcp_send,
    .recv = tcp_recv,
    .close = tcp_close,
};

/* UDP-like transport */
static int udp_connect(struct smart_socket *sock, const char *addr)
{
    printf("    [UDP] Setting default destination: %s\n", addr);
    return 0;
}

static int udp_send(struct smart_socket *sock, const void *data, int len)
{
    printf("    [UDP] Sending datagram: %d bytes (unreliable)\n", len);
    return len;
}

static int udp_recv(struct smart_socket *sock, void *data, int len)
{
    printf("    [UDP] Receiving datagram up to %d bytes\n", len);
    return len;
}

static void udp_close(struct smart_socket *sock)
{
    printf("    [UDP] Closed (no handshake needed)\n");
}

static const transport_ops_t udp_transport_ops = {
    .name = "UDP",
    .connect = udp_connect,
    .send = udp_send,
    .recv = udp_recv,
    .close = udp_close,
};

/* =========================================================================
 * AXIS 3: Security Layer (ops) - like TLS
 * ========================================================================= */

typedef struct security_ops {
    const char *name;
    
    int  (*init)(struct smart_socket *sock);
    void (*cleanup)(struct smart_socket *sock);
    int  (*encrypt)(struct smart_socket *sock, const void *plain, 
                    void *cipher, int len);
    int  (*decrypt)(struct smart_socket *sock, const void *cipher,
                    void *plain, int len);
} security_ops_t;

/* Plaintext (no security) */
static int plain_init(struct smart_socket *sock) { return 0; }
static void plain_cleanup(struct smart_socket *sock) { }

static int plain_encrypt(struct smart_socket *sock, const void *plain,
                         void *cipher, int len)
{
    memcpy(cipher, plain, len);
    return len;
}

static int plain_decrypt(struct smart_socket *sock, const void *cipher,
                         void *plain, int len)
{
    memcpy(plain, cipher, len);
    return len;
}

static const security_ops_t plain_security_ops = {
    .name = "PLAIN",
    .init = plain_init,
    .cleanup = plain_cleanup,
    .encrypt = plain_encrypt,
    .decrypt = plain_decrypt,
};

/* TLS-like security */
typedef struct {
    unsigned char key[32];
    int key_len;
} tls_private_t;

static int tls_init(struct smart_socket *sock);  /* Forward */
static void tls_cleanup(struct smart_socket *sock);

static int tls_encrypt(struct smart_socket *sock, const void *plain,
                       void *cipher, int len)
{
    printf("    [TLS] Encrypting %d bytes\n", len);
    /* Simulate encryption: XOR with 0x55 */
    const unsigned char *p = plain;
    unsigned char *c = cipher;
    for (int i = 0; i < len; i++) {
        c[i] = p[i] ^ 0x55;
    }
    return len;
}

static int tls_decrypt(struct smart_socket *sock, const void *cipher,
                       void *plain, int len)
{
    printf("    [TLS] Decrypting %d bytes\n", len);
    const unsigned char *c = cipher;
    unsigned char *p = plain;
    for (int i = 0; i < len; i++) {
        p[i] = c[i] ^ 0x55;
    }
    return len;
}

static const security_ops_t tls_security_ops = {
    .name = "TLS",
    .init = tls_init,
    .cleanup = tls_cleanup,
    .encrypt = tls_encrypt,
    .decrypt = tls_decrypt,
};

/* =========================================================================
 * AXIS 4: Rate Control (ops) - like congestion control
 * ========================================================================= */

typedef struct rate_ops {
    const char *name;
    
    void (*init)(struct smart_socket *sock);
    void (*cleanup)(struct smart_socket *sock);
    bool (*acquire)(struct smart_socket *sock, int bytes);
    void (*release)(struct smart_socket *sock, int bytes);
} rate_ops_t;

/* Unlimited rate */
static void unlimited_init(struct smart_socket *sock) { }
static void unlimited_cleanup(struct smart_socket *sock) { }

static bool unlimited_acquire(struct smart_socket *sock, int bytes)
{
    return true;  /* Always allowed */
}

static void unlimited_release(struct smart_socket *sock, int bytes) { }

static const rate_ops_t unlimited_rate_ops = {
    .name = "UNLIMITED",
    .init = unlimited_init,
    .cleanup = unlimited_cleanup,
    .acquire = unlimited_acquire,
    .release = unlimited_release,
};

/* Token bucket rate limiter */
typedef struct {
    int tokens;
    int max_tokens;
    int refill_rate;
} token_bucket_t;

static void token_init(struct smart_socket *sock);
static void token_cleanup(struct smart_socket *sock);

static bool token_acquire(struct smart_socket *sock, int bytes);
static void token_release(struct smart_socket *sock, int bytes);

static const rate_ops_t token_bucket_ops = {
    .name = "TOKEN_BUCKET",
    .init = token_init,
    .cleanup = token_cleanup,
    .acquire = token_acquire,
    .release = token_release,
};

/* =========================================================================
 * SMART SOCKET: The Hybrid FSM Context
 * ========================================================================= */

typedef struct smart_socket {
    /* [AXIS 1] Connection state (enum - visible) */
    conn_state_t            conn_state;
    
    /* [AXIS 2] Transport behavior (ops - swappable) */
    const transport_ops_t   *transport_ops;
    
    /* [AXIS 3] Security behavior (ops - swappable) */
    const security_ops_t    *security_ops;
    void                    *security_priv;     /* Security private data */
    
    /* [AXIS 4] Rate control behavior (ops - swappable) */
    const rate_ops_t        *rate_ops;
    void                    *rate_priv;         /* Rate control private data */
    
    /* Common fields */
    char                    remote_addr[64];
    int                     bytes_sent;
    int                     bytes_recv;
    
    /* State change callback (like sk->sk_state_change) */
    void (*state_change)(struct smart_socket *);
} smart_socket_t;

/* Implement TLS functions that need socket */
static int tls_init(struct smart_socket *sock)
{
    tls_private_t *priv = malloc(sizeof(tls_private_t));
    memset(priv->key, 0x42, sizeof(priv->key));
    priv->key_len = 32;
    sock->security_priv = priv;
    printf("    [TLS] Initialized with %d-byte key\n", priv->key_len);
    return 0;
}

static void tls_cleanup(struct smart_socket *sock)
{
    if (sock->security_priv) {
        free(sock->security_priv);
        sock->security_priv = NULL;
    }
    printf("    [TLS] Cleaned up\n");
}

/* Implement token bucket functions */
static void token_init(struct smart_socket *sock)
{
    token_bucket_t *tb = malloc(sizeof(token_bucket_t));
    tb->tokens = 1000;
    tb->max_tokens = 1000;
    tb->refill_rate = 100;
    sock->rate_priv = tb;
    printf("    [TOKEN] Init: %d tokens, max=%d\n", tb->tokens, tb->max_tokens);
}

static void token_cleanup(struct smart_socket *sock)
{
    if (sock->rate_priv) {
        free(sock->rate_priv);
        sock->rate_priv = NULL;
    }
}

static bool token_acquire(struct smart_socket *sock, int bytes)
{
    token_bucket_t *tb = sock->rate_priv;
    if (tb->tokens >= bytes) {
        tb->tokens -= bytes;
        printf("    [TOKEN] Acquired %d, remaining=%d\n", bytes, tb->tokens);
        return true;
    }
    printf("    [TOKEN] Rate limited! Need %d, have %d\n", bytes, tb->tokens);
    return false;
}

static void token_release(struct smart_socket *sock, int bytes)
{
    token_bucket_t *tb = sock->rate_priv;
    tb->tokens += bytes;
    if (tb->tokens > tb->max_tokens) {
        tb->tokens = tb->max_tokens;
    }
}

/* =========================================================================
 * Connection State Machine (enum-based transitions)
 * ========================================================================= */

/*
 * [KEY] Central state mutation for enum state (like tcp_set_state)
 */
static void socket_set_conn_state(smart_socket_t *sock, conn_state_t new_state)
{
    conn_state_t old = sock->conn_state;
    
    sock->conn_state = new_state;
    
    printf("  [STATE] %s -> %s\n", 
           conn_state_names[old], conn_state_names[new_state]);
    
    if (sock->state_change) {
        sock->state_change(sock);
    }
}

/*
 * [KEY] Event handler uses switch on enum state,
 * but operations dispatch through ops pointers
 */
static int socket_handle_event(smart_socket_t *sock, conn_event_t event)
{
    printf("Event: %s (state=%s, transport=%s, security=%s, rate=%s)\n",
           conn_event_names[event],
           conn_state_names[sock->conn_state],
           sock->transport_ops->name,
           sock->security_ops->name,
           sock->rate_ops->name);
    
    switch (sock->conn_state) {
    case CONN_CLOSED:
        if (event == CONN_EVENT_CONNECT) {
            /* [KEY] Use ops for actual work */
            sock->transport_ops->connect(sock, sock->remote_addr);
            socket_set_conn_state(sock, CONN_CONNECTING);
            return 0;
        }
        break;
        
    case CONN_CONNECTING:
        if (event == CONN_EVENT_CONNECTED) {
            /* Initialize security layer */
            if (sock->security_ops->init) {
                sock->security_ops->init(sock);
            }
            socket_set_conn_state(sock, CONN_CONNECTED);
            return 0;
        }
        if (event == CONN_EVENT_ERROR) {
            socket_set_conn_state(sock, CONN_CLOSED);
            return -1;
        }
        break;
        
    case CONN_CONNECTED:
        if (event == CONN_EVENT_DISCONNECT) {
            socket_set_conn_state(sock, CONN_DISCONNECTING);
            sock->transport_ops->close(sock);
            return 0;
        }
        if (event == CONN_EVENT_ERROR) {
            sock->security_ops->cleanup(sock);
            socket_set_conn_state(sock, CONN_CLOSED);
            return -1;
        }
        break;
        
    case CONN_DISCONNECTING:
        if (event == CONN_EVENT_DISCONNECTED) {
            sock->security_ops->cleanup(sock);
            socket_set_conn_state(sock, CONN_CLOSED);
            return 0;
        }
        break;
        
    default:
        printf("  [BUG] Invalid state!\n");
        return -1;
    }
    
    printf("  [WARN] Ignoring event %s in state %s\n",
           conn_event_names[event], conn_state_names[sock->conn_state]);
    return 0;
}

/* =========================================================================
 * Data operations (use all ops together)
 * ========================================================================= */

static int socket_send(smart_socket_t *sock, const void *data, int len)
{
    if (sock->conn_state != CONN_CONNECTED) {
        printf("  [ERROR] Not connected!\n");
        return -1;
    }
    
    printf("  SEND %d bytes:\n", len);
    
    /* [AXIS 4] Rate control */
    if (!sock->rate_ops->acquire(sock, len)) {
        return -1;  /* Rate limited */
    }
    
    /* [AXIS 3] Security - encrypt */
    char encrypted[256];
    int enc_len = sock->security_ops->encrypt(sock, data, encrypted, len);
    
    /* [AXIS 2] Transport - send */
    int sent = sock->transport_ops->send(sock, encrypted, enc_len);
    
    sock->bytes_sent += sent;
    return sent;
}

static int socket_recv(smart_socket_t *sock, void *data, int len)
{
    if (sock->conn_state != CONN_CONNECTED) {
        printf("  [ERROR] Not connected!\n");
        return -1;
    }
    
    printf("  RECV up to %d bytes:\n", len);
    
    /* [AXIS 2] Transport - receive */
    char encrypted[256];
    int recv_len = sock->transport_ops->recv(sock, encrypted, len);
    
    /* [AXIS 3] Security - decrypt */
    int dec_len = sock->security_ops->decrypt(sock, encrypted, data, recv_len);
    
    sock->bytes_recv += dec_len;
    return dec_len;
}

/* =========================================================================
 * Runtime ops switching (hot-swappable behavior)
 * ========================================================================= */

/*
 * [KEY] Switch security layer at runtime (like upgrading to TLS)
 */
static int socket_set_security(smart_socket_t *sock, const security_ops_t *new_ops)
{
    printf("\n=== Switching security: %s -> %s ===\n",
           sock->security_ops->name, new_ops->name);
    
    /* Cleanup old */
    if (sock->security_ops->cleanup) {
        sock->security_ops->cleanup(sock);
    }
    
    /* Switch */
    sock->security_ops = new_ops;
    
    /* Initialize new */
    if (sock->security_ops->init && sock->conn_state == CONN_CONNECTED) {
        return sock->security_ops->init(sock);
    }
    
    return 0;
}

/*
 * [KEY] Switch rate control at runtime (like changing congestion algorithm)
 */
static int socket_set_rate_control(smart_socket_t *sock, const rate_ops_t *new_ops)
{
    printf("\n=== Switching rate control: %s -> %s ===\n",
           sock->rate_ops->name, new_ops->name);
    
    if (sock->rate_ops->cleanup) {
        sock->rate_ops->cleanup(sock);
    }
    
    sock->rate_ops = new_ops;
    
    if (sock->rate_ops->init) {
        sock->rate_ops->init(sock);
    }
    
    return 0;
}

/* =========================================================================
 * Socket lifecycle
 * ========================================================================= */

static void default_state_change(smart_socket_t *sock)
{
    printf("  [CALLBACK] New state: %s\n", conn_state_names[sock->conn_state]);
}

static smart_socket_t *socket_create(const transport_ops_t *transport)
{
    smart_socket_t *sock = calloc(1, sizeof(smart_socket_t));
    
    sock->conn_state = CONN_CLOSED;
    sock->transport_ops = transport;
    sock->security_ops = &plain_security_ops;  /* Default: no security */
    sock->rate_ops = &unlimited_rate_ops;      /* Default: no rate limit */
    sock->state_change = default_state_change;
    
    return sock;
}

static void socket_destroy(smart_socket_t *sock)
{
    if (sock->security_ops->cleanup) {
        sock->security_ops->cleanup(sock);
    }
    if (sock->rate_ops->cleanup) {
        sock->rate_ops->cleanup(sock);
    }
    free(sock);
}

/* =========================================================================
 * Demonstration
 * ========================================================================= */

static void demo_hybrid_fsm(void)
{
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════════╗\n");
    printf("║                 HYBRID FSM DEMO (Like TCP Socket)                 ║\n");
    printf("╚═══════════════════════════════════════════════════════════════════╝\n\n");
    
    /* Create socket with TCP transport */
    smart_socket_t *sock = socket_create(&tcp_transport_ops);
    strcpy(sock->remote_addr, "192.168.1.100:8080");
    
    printf("--- Initial State ---\n");
    printf("Connection: %s\n", conn_state_names[sock->conn_state]);
    printf("Transport: %s\n", sock->transport_ops->name);
    printf("Security: %s\n", sock->security_ops->name);
    printf("Rate: %s\n\n", sock->rate_ops->name);
    
    /* Connect */
    printf("--- Connecting ---\n");
    socket_handle_event(sock, CONN_EVENT_CONNECT);
    socket_handle_event(sock, CONN_EVENT_CONNECTED);
    
    /* Send data (no encryption, no rate limit) */
    printf("\n--- Sending data (plain, unlimited) ---\n");
    socket_send(sock, "Hello", 5);
    socket_recv(sock, NULL, 100);
    
    /* Upgrade to TLS while connected! */
    socket_set_security(sock, &tls_security_ops);
    
    /* Send encrypted data */
    printf("\n--- Sending data (encrypted) ---\n");
    socket_send(sock, "Secret", 6);
    
    /* Enable rate limiting */
    socket_set_rate_control(sock, &token_bucket_ops);
    
    /* Send with rate limit */
    printf("\n--- Sending data (encrypted + rate limited) ---\n");
    socket_send(sock, "Data1", 5);
    socket_send(sock, "Data2", 5);
    
    /* Disconnect */
    printf("\n--- Disconnecting ---\n");
    socket_handle_event(sock, CONN_EVENT_DISCONNECT);
    socket_handle_event(sock, CONN_EVENT_DISCONNECTED);
    
    printf("\n--- Final Stats ---\n");
    printf("Bytes sent: %d\n", sock->bytes_sent);
    printf("Bytes received: %d\n", sock->bytes_recv);
    
    socket_destroy(sock);
}

static void demo_different_configurations(void)
{
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════════╗\n");
    printf("║              ORTHOGONAL AXES: Different Configurations            ║\n");
    printf("╚═══════════════════════════════════════════════════════════════════╝\n\n");
    
    /* Configuration 1: UDP + Plain + Unlimited */
    printf("Config 1: UDP + PLAIN + UNLIMITED\n");
    smart_socket_t *s1 = socket_create(&udp_transport_ops);
    strcpy(s1->remote_addr, "8.8.8.8:53");
    socket_handle_event(s1, CONN_EVENT_CONNECT);
    socket_handle_event(s1, CONN_EVENT_CONNECTED);
    socket_send(s1, "DNS query", 9);
    socket_destroy(s1);
    
    /* Configuration 2: TCP + TLS + Token Bucket */
    printf("\nConfig 2: TCP + TLS + TOKEN_BUCKET\n");
    smart_socket_t *s2 = socket_create(&tcp_transport_ops);
    s2->security_ops = &tls_security_ops;
    s2->rate_ops = &token_bucket_ops;
    s2->rate_ops->init(s2);
    strcpy(s2->remote_addr, "api.example.com:443");
    socket_handle_event(s2, CONN_EVENT_CONNECT);
    socket_handle_event(s2, CONN_EVENT_CONNECTED);
    socket_send(s2, "HTTPS request", 13);
    socket_destroy(s2);
}

static void print_complexity_comparison(void)
{
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════════╗\n");
    printf("║                    COMPLEXITY COMPARISON                          ║\n");
    printf("╚═══════════════════════════════════════════════════════════════════╝\n\n");
    
    printf("Without Hybrid FSM (pure enum):\n");
    printf("───────────────────────────────\n");
    printf("  Connection states: 4\n");
    printf("  Transports:        2 (TCP, UDP)\n");
    printf("  Security:          2 (Plain, TLS)\n");
    printf("  Rate control:      2 (Unlimited, TokenBucket)\n");
    printf("  ─────────────────────\n");
    printf("  Total states: 4 × 2 × 2 × 2 = 32 enum values!\n");
    printf("  Each with its own switch case...\n\n");
    
    printf("With Hybrid FSM:\n");
    printf("────────────────\n");
    printf("  Enum states: 4 (connection only)\n");
    printf("  Ops tables:  3 (transport, security, rate)\n");
    printf("  ─────────────────────\n");
    printf("  Total complexity: 4 + 2 + 2 + 2 = 10 items\n");
    printf("  Each axis changes independently!\n\n");
    
    printf("Scalability:\n");
    printf("────────────\n");
    printf("  Add new transport (e.g., QUIC):\n");
    printf("    Pure enum: Multiply all states by 1.5\n");
    printf("    Hybrid:    Add 1 ops table\n\n");
    printf("  Add new security (e.g., DTLS):\n");
    printf("    Pure enum: Multiply all states by 1.5\n");
    printf("    Hybrid:    Add 1 ops table\n");
}

int main(void)
{
    printf("Hybrid FSM Simulation (enum + ops pattern)\n");
    printf("==========================================\n");
    
    demo_hybrid_fsm();
    demo_different_configurations();
    print_complexity_comparison();
    
    return 0;
}
```

**Architecture diagram:**

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         HYBRID FSM ARCHITECTURE                              │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │                       smart_socket_t                                 │   │
│   ├─────────────────────────────────────────────────────────────────────┤   │
│   │                                                                     │   │
│   │  ┌───────────────────┐   ┌──────────────────────────────────────┐   │   │
│   │  │  conn_state       │   │  transport_ops ──────────────────┐   │   │   │
│   │  │  (enum)           │   │                                  │   │   │   │
│   │  │                   │   │    ┌─────────────┐   ┌─────────┐ │   │   │   │
│   │  │  CLOSED ──────────┼───┼───►│ tcp_ops     │ OR│ udp_ops │ │   │   │   │
│   │  │  CONNECTING       │   │    └─────────────┘   └─────────┘ │   │   │   │
│   │  │  CONNECTED        │   │                                  │   │   │   │
│   │  │  DISCONNECTING    │   └──────────────────────────────────┘   │   │   │
│   │  │                   │                                          │   │   │
│   │  │  [Visible state]  │   ┌──────────────────────────────────────┐   │   │
│   │  └───────────────────┘   │  security_ops ───────────────────┐   │   │   │
│   │                          │                                  │   │   │   │
│   │                          │    ┌─────────────┐   ┌─────────┐ │   │   │   │
│   │                          │───►│ plain_ops   │ OR│ tls_ops │ │   │   │   │
│   │                          │    └─────────────┘   └─────────┘ │   │   │   │
│   │                          │                                  │   │   │   │
│   │                          └──────────────────────────────────┘   │   │   │
│   │                                                                 │   │   │
│   │                          ┌──────────────────────────────────────┐   │   │
│   │                          │  rate_ops ───────────────────────┐   │   │   │
│   │                          │                                  │   │   │   │
│   │                          │    ┌─────────────┐   ┌─────────┐ │   │   │   │
│   │                          │───►│unlimited_ops│ OR│token_ops│ │   │   │   │
│   │                          │    └─────────────┘   └─────────┘ │   │   │   │
│   │                          │                                  │   │   │   │
│   │                          └──────────────────────────────────┘   │   │   │
│   │                                                                     │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                                                             │
│   DISPATCH PATTERN:                                                         │
│   ─────────────────                                                         │
│                                                                             │
│   switch(sock->conn_state) {    // Enum-based state machine                 │
│       case CONNECTED:                                                       │
│           sock->transport_ops->send(...);  // Ops dispatch                  │
│           sock->security_ops->encrypt(...);                                 │
│           sock->rate_ops->acquire(...);                                     │
│   }                                                                         │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

**说明:**
- 连接状态用 enum（可见、可调试）
- 行为用 ops 指针（可扩展、可热切换）
- 四个轴独立变化：连接状态 × 传输协议 × 安全层 × 速率控制
- 不使用纯 enum 的原因：4×2×2×2 = 32 个状态太复杂
- 混合方案：4 个 enum 值 + 3 个 ops 表 = 清晰且可扩展

---

## Phase 5 — Design Rules & Failure Modes

### 5.1 Design Rules Used by Kernel Developers

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        FSM DESIGN DECISION RULES                             │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  RULE 1: Use enum when state must be VISIBLE                                │
│  ────────────────────────────────────────────                               │
│                                                                             │
│    ✓ TCP connection state (netstat, /proc/net/tcp)                          │
│    ✓ USB device state (lsusb, sysfs)                                        │
│    ✓ Process state (ps, /proc)                                              │
│                                                                             │
│    Why: Tools, debuggers, and users need to see state                       │
│                                                                             │
│  RULE 2: Use enum when transitions are FEW and FIXED                        │
│  ───────────────────────────────────────────────────                        │
│                                                                             │
│    ✓ TCP has 11 states defined by RFC                                       │
│    ✓ USB has 9 states defined by spec                                       │
│    ✗ Filesystems (dozens, growing)                                          │
│    ✗ Network protocols (hundreds)                                           │
│                                                                             │
│  RULE 3: Use ops when BEHAVIOR differs substantially                        │
│  ────────────────────────────────────────────────────                       │
│                                                                             │
│    ✓ ext4 vs NFS vs procfs (completely different)                           │
│    ✓ TCP vs UDP vs SCTP (different algorithms)                              │
│    ✓ Line disciplines (N_TTY vs PPP vs SLIP)                                │
│                                                                             │
│    Why: Switch statement would be enormous                                  │
│                                                                             │
│  RULE 4: Use ops when EXTENSIBLE without core changes                       │
│  ────────────────────────────────────────────────────                       │
│                                                                             │
│    ✓ New filesystem = new module, no VFS changes                            │
│    ✓ New congestion algorithm = new module                                  │
│    ✓ New device driver = new module                                         │
│                                                                             │
│  RULE 5: Use HYBRID when orthogonal concerns exist                          │
│  ─────────────────────────────────────────────────                          │
│                                                                             │
│    ✓ TCP: connection state (enum) + congestion (ops) + AF (ops)             │
│    ✓ Block: request phase (enum) + scheduler (ops) + driver (ops)           │
│                                                                             │
│    Why: Each axis changes independently                                     │
│                                                                             │
│  RULE 6: Centralize state mutation                                          │
│  ─────────────────────────────────                                          │
│                                                                             │
│    ✓ tcp_set_state() for all TCP state changes                              │
│    ✓ usb_set_device_state() for all USB state changes                       │
│                                                                             │
│    Why: Statistics, logging, invariant checks                               │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 5.2 Common FSM Bugs in Kernel History

#### Bug Type 1: Missing Transitions

```c
/* BUG: Forgot to handle FIN in FIN_WAIT2 state */
case TCP_FIN_WAIT2:
    /* Process incoming FIN */
    if (th->fin) {
        tcp_set_state(sk, TCP_TIME_WAIT);  /* CORRECT */
    }
    /* BUG: What if th->fin is false? Stuck in FIN_WAIT2 */
    /* FIX: Add timeout to catch stuck connections */
```

#### Bug Type 2: Invalid State Reached

```c
/* BUG: State corruption leads to impossible state */
switch (sk->sk_state) {
case TCP_ESTABLISHED:
    /* ... */
    break;
case TCP_CLOSE:
    /* ... */
    break;
default:
    /* BUG: We reached an invalid state! */
    printk(KERN_ERR "TCP: Impossible, sk->sk_state=%d\n", sk->sk_state);
    /* This should never happen if FSM is correct */
}
```

#### Bug Type 3: Race Conditions

```c
/* BUG: Check-then-act race */
void bad_state_change(struct sock *sk)
{
    if (sk->sk_state == TCP_ESTABLISHED) {  /* CHECK */
        /* Another CPU changes state here! */
        tcp_send_fin(sk);                     /* ACT - now wrong! */
    }
}

/* FIX: Hold lock across check and act */
void good_state_change(struct sock *sk)
{
    lock_sock(sk);  /* Or bh_lock_sock() */
    if (sk->sk_state == TCP_ESTABLISHED) {
        tcp_send_fin(sk);
    }
    release_sock(sk);
}
```

#### Bug Type 4: State Leaks

```c
/* BUG: Socket stuck in CLOSE_WAIT forever */
void tcp_fin(struct sock *sk)
{
    tcp_set_state(sk, TCP_CLOSE_WAIT);
    /* BUG: Application never calls close()! */
    /* Socket leaks, never reaches LAST_ACK → CLOSED */
}

/* DEFENSE: Orphan socket timeout */
/* TCP has tcp_orphan_retries to eventually kill stuck sockets */
```

### 5.3 Kernel Defenses Against FSM Bugs

#### Defense 1: WARN_ON Assertions

```c
/* Catch invalid states early */
void tcp_set_state(struct sock *sk, int state)
{
    WARN_ON(state < 0 || state >= TCP_MAX_STATES);
    WARN_ON(sk->sk_state == TCP_CLOSE && state != TCP_LISTEN);
    /* ... */
}

/* Output: WARNING: at net/ipv4/tcp.c:1790 */
/* Stack trace helps locate the bug */
```

#### Defense 2: BUG_ON for Critical Invariants

```c
/* Crash immediately if invariant violated */
void critical_function(struct sock *sk)
{
    BUG_ON(sk->sk_state == TCP_CLOSE);  /* Must not be called on closed socket */
    /* If violated, kernel oopses with stack trace */
}
```

#### Defense 3: Locking Discipline

```c
/* TCP uses multiple lock levels */
/*
 * LOCK HIERARCHY (must acquire in this order):
 * 1. bh_lock_sock(sk)     - Bottom half lock (softirq)
 * 2. lock_sock(sk)        - User context lock
 * 3. sk->sk_lock.slock    - Spinlock for specific fields
 */

void tcp_rcv_state_process(struct sock *sk, ...)
{
    /* Called with bh_lock_sock held */
    /* State transitions are serialized */
}
```

#### Defense 4: Reference Counting

```c
/* Prevent use-after-free when state changes */
void some_operation(struct sock *sk)
{
    sock_hold(sk);  /* Increment refcount */
    
    /* ... operation that might trigger state change ... */
    
    sock_put(sk);   /* Decrement refcount, may free */
}

/* tcp_set_state() doesn't free - separate concern */
/* Caller must ensure socket won't be freed during operation */
```

#### Defense 5: Timeouts

```c
/* Recover from stuck states */
struct inet_connection_sock {
    /* Timers for each possible stuck state */
    struct timer_list   icsk_retransmit_timer;  /* Retransmit timeout */
    struct timer_list   icsk_delack_timer;      /* Delayed ACK */
    /* ... */
};

/* TIME_WAIT has explicit timeout */
void tcp_time_wait(struct sock *sk, int state, int timeo)
{
    /* Set timer to clean up after 2*MSL */
    inet_twsk_schedule(tw, &tcp_death_row, timeo, TCP_TIMEWAIT_LEN);
}

/* FIN_WAIT2 orphan timeout */
/* If no data for N seconds, close anyway */
```

### 5.4 Defense Summary Table

| Defense | Purpose | Example |
|---------|---------|---------|
| `WARN_ON` | Detect invalid states (non-fatal) | `WARN_ON(state >= TCP_MAX_STATES)` |
| `BUG_ON` | Crash on critical invariant violation | `BUG_ON(sk == NULL)` |
| Locks | Serialize state transitions | `bh_lock_sock(sk)` |
| Refcounting | Prevent use-after-free | `sock_hold()/sock_put()` |
| Timeouts | Recover from stuck states | TIME_WAIT, FIN_WAIT2 timeouts |
| Default case | Catch unexpected states | `default: WARN_ON(1);` |

---

## Final Deliverable: FSM Decision Framework

### Decision Tree

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        FSM IMPLEMENTATION DECISION TREE                      │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│                        Does state need to be VISIBLE?                       │
│                       (debugging, tools, user display)                      │
│                               /           \                                  │
│                             YES            NO                                │
│                              │              │                                │
│                              ▼              ▼                                │
│                        ┌─────────┐   Are behaviors RADICALLY                │
│                        │  enum   │      different?                          │
│                        │ required│      /        \                          │
│                        └────┬────┘    YES         NO                        │
│                             │          │           │                        │
│                             │          ▼           ▼                        │
│                             │    ┌──────────┐  ┌──────────┐                 │
│                             │    │  ops     │  │  Maybe   │                 │
│                             │    │  pattern │  │  enum    │                 │
│                             │    └──────────┘  └──────────┘                 │
│                             │                                               │
│                             ▼                                               │
│                  Are there ORTHOGONAL behavior axes?                        │
│                         /           \                                        │
│                       YES            NO                                      │
│                        │              │                                      │
│                        ▼              ▼                                      │
│                  ┌───────────┐  ┌───────────┐                               │
│                  │  HYBRID   │  │Pure enum  │                               │
│                  │ enum+ops  │  │  + switch │                               │
│                  └───────────┘  └───────────┘                               │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Decision Rules Summary

| Condition | Implementation |
|-----------|---------------|
| State visible to tools/users | Use enum (required) |
| States fixed by specification | Use enum |
| < 10 states, < 10 events | Pure enum + switch |
| Behaviors radically different | Use ops pattern |
| Multiple implementations loadable | Use ops pattern |
| Orthogonal behavior axes | Hybrid (enum + ops) |
| Need extensibility without core changes | Use ops pattern |

### Application to User-Space C Systems

```c
/* 
 * APPLYING KERNEL PATTERNS TO USER-SPACE
 * 
 * PATTERN 1: Simple enum + switch
 * Use for: Small FSMs with fixed states, debugging visibility needed
 */

enum connection_state {
    CONN_DISCONNECTED,
    CONN_CONNECTING,
    CONN_CONNECTED,
    CONN_DISCONNECTING,
};

struct connection {
    enum connection_state state;
    /* ... */
};

/* Central state mutation (like tcp_set_state) */
static void set_state(struct connection *conn, enum connection_state new_state)
{
    assert(new_state >= 0 && new_state < CONN_MAX_STATES);
    
    log_state_change(conn->state, new_state);  /* Logging */
    conn->state = new_state;
}

/*
 * PATTERN 2: ops-based state
 * Use for: Plugin architectures, radically different behaviors
 */

struct protocol_ops {
    int (*connect)(struct connection *);
    int (*send)(struct connection *, const void *, size_t);
    int (*recv)(struct connection *, void *, size_t);
    void (*close)(struct connection *);
};

static struct protocol_ops tcp_ops = {
    .connect = tcp_connect,
    .send = tcp_send,
    .recv = tcp_recv,
    .close = tcp_close,
};

static struct protocol_ops udp_ops = {
    .connect = udp_connect,  /* May be no-op */
    .send = udp_send,
    .recv = udp_recv,
    .close = udp_close,
};

struct connection {
    const struct protocol_ops *ops;  /* Behavior "state" */
    void *private_data;              /* Protocol-specific data */
};

/*
 * PATTERN 3: Hybrid
 * Use for: Complex systems with orthogonal concerns
 */

struct my_socket {
    /* Enum state (visible) */
    enum connection_state state;
    
    /* Ops-based behavior (pluggable) */
    const struct protocol_ops *proto_ops;      /* TCP/UDP/custom */
    const struct security_ops *security_ops;   /* TLS/plain/custom */
    const struct logging_ops *log_ops;         /* File/syslog/null */
};

/* State transitions use enum */
void handle_connected(struct my_socket *s)
{
    set_state(s, CONN_CONNECTED);
}

/* Behavior uses ops */
void do_send(struct my_socket *s, const void *data, size_t len)
{
    /* Encrypt if needed */
    if (s->security_ops->encrypt)
        data = s->security_ops->encrypt(s, data, len);
    
    /* Send via protocol */
    s->proto_ops->send(s, data, len);
    
    /* Log */
    if (s->log_ops->log_send)
        s->log_ops->log_send(s, len);
}
```

### Key Takeaways

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          KEY LESSONS FROM KERNEL FSMs                        │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  1. EXPLICIT IS BETTER THAN IMPLICIT                                        │
│     • State should be a simple field, easily inspectable                    │
│     • Avoid encoding state in combinations of flags                         │
│                                                                             │
│  2. CENTRALIZE STATE MUTATIONS                                              │
│     • Single function for all state changes                                 │
│     • Add logging, assertions, statistics there                             │
│                                                                             │
│  3. SEPARATE CONCERNS                                                       │
│     • Use hybrid when you have orthogonal axes                              │
│     • TCP: connection state + congestion + AF                               │
│                                                                             │
│  4. DEFEND AGAINST INVALID STATES                                           │
│     • WARN_ON for soft errors                                               │
│     • BUG_ON for invariant violations                                       │
│     • Timeouts for recovery                                                 │
│                                                                             │
│  5. MATCH PATTERN TO SCALE                                                  │
│     • < 10 states: pure enum                                                │
│     • Need plugins: ops pattern                                             │
│     • Complex system: hybrid                                                │
│                                                                             │
│  6. DESIGN FOR DEBUGGING                                                    │
│     • State should be visible in debugger                                   │
│     • State names should be human-readable                                  │
│     • Transitions should be loggable                                        │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

**说明:**
1. 显式优于隐式 — 状态应该是简单字段
2. 集中状态变更 — 单个函数处理所有状态转换
3. 分离关注点 — 正交的维度用不同机制
4. 防御无效状态 — WARN_ON, BUG_ON, 超时
5. 模式匹配规模 — 小系统用 enum，大系统用 ops 或混合
6. 为调试设计 — 状态应该可见、可读、可追踪

---

*This document analyzes FSM patterns from Linux kernel v3.2. The architectural principles apply to modern kernels and user-space systems.*

