# Case 2: TCP Congestion Control Algorithms

## Subsystem Background

```
+=============================================================================+
|                    TCP CONGESTION CONTROL ARCHITECTURE                       |
+=============================================================================+

                          TCP CORE
                          ========

    +------------------------------------------------------------------+
    |                     net/ipv4/tcp_*.c                              |
    |                                                                   |
    |   MECHANISM (Fixed):                                              |
    |   - Segment transmission (tcp_transmit_skb)                       |
    |   - ACK processing (tcp_ack)                                      |
    |   - Retransmission timer (tcp_retransmit_timer)                   |
    |   - Connection state machine                                      |
    |   - Window advertisement                                          |
    |                                                                   |
    +------------------------------------------------------------------+
                                |
                                | delegates POLICY to
                                v
    +------------------------------------------------------------------+
    |                 CONGESTION CONTROL STRATEGIES                     |
    |                   (Strategy Pattern)                              |
    |                                                                   |
    |   +------------------+  +------------------+  +------------------+|
    |   |      CUBIC       |  |      Reno        |  |     Vegas        ||
    |   | (default Linux)  |  | (classic)        |  | (delay-based)    ||
    |   +------------------+  +------------------+  +------------------+|
    |                                                                   |
    |   +------------------+  +------------------+  +------------------+|
    |   |       BIC        |  |     Westwood     |  |    Hybla         ||
    |   | (aggressive)     |  | (bandwidth est)  |  | (satellite)      ||
    |   +------------------+  +------------------+  +------------------+|
    |                                                                   |
    +------------------------------------------------------------------+

    KEY INSIGHT:
    - TCP CORE knows WHEN congestion events happen (packet loss, ACK)
    - Congestion Control knows HOW to adjust sending rate
```

**中文说明：**

TCP拥塞控制架构：核心（`net/ipv4/tcp_*.c`）负责机制——段发送、ACK处理、重传定时器、连接状态机、窗口通告。核心将策略委托给拥塞控制策略：CUBIC（Linux默认）、Reno（经典）、Vegas（基于延迟）、BIC（激进）、Westwood（带宽估计）、Hybla（卫星链路）。关键洞察：TCP核心知道何时发生拥塞事件（丢包、ACK），拥塞控制知道如何调整发送速率。

---

## The Strategy Interface: struct tcp_congestion_ops

### Components

| Component | Role |
|-----------|------|
| **Strategy Interface** | `struct tcp_congestion_ops` |
| **Replaceable Algorithm** | CUBIC, Reno, Vegas, etc. |
| **Selection Mechanism** | Per-socket, sysctl default, `setsockopt()` |

### The Interface

```c
struct tcp_congestion_ops {
    struct list_head list;
    unsigned long flags;

    /* Initialize congestion control state */
    void (*init)(struct sock *sk);
    /* Cleanup when connection closes */
    void (*release)(struct sock *sk);

    /* === THE KEY CONGESTION CONTROL DECISIONS === */
    
    /* Calculate slow start threshold after loss */
    u32 (*ssthresh)(struct sock *sk);
    
    /* Congestion avoidance: adjust cwnd */
    void (*cong_avoid)(struct sock *sk, u32 ack, u32 in_flight);
    
    /* Minimum congestion window */
    u32 (*min_cwnd)(const struct sock *sk);
    
    /* Undo cwnd reduction */
    u32 (*undo_cwnd)(struct sock *sk);

    /* === EVENT HANDLERS === */
    
    /* Called when ACK received */
    void (*pkts_acked)(struct sock *sk, u32 num_acked, s32 rtt_us);
    
    /* Called on congestion event */
    void (*set_state)(struct sock *sk, u8 new_state);
    
    /* Called on cwnd event */
    void (*cwnd_event)(struct sock *sk, enum tcp_ca_event ev);

    /* ... more operations ... */
    
    char name[TCP_CA_NAME_MAX];
    struct module *owner;
};
```

### Control Flow: How Core Uses Strategy

```
    tcp_ack() - ACK Processing (Simplified)
    =======================================

    +----------------------------------+
    |  ACK received                    |
    +----------------------------------+
                   |
                   v
    +----------------------------------+
    |  Update RTT estimates            |  MECHANISM
    |  Update SACK scoreboard          |  (Core)
    +----------------------------------+
                   |
                   v
    +==========================================+
    ||  ca_ops->pkts_acked()                  ||  STRATEGY
    ||  (Notify congestion control of ACKs)   ||
    +==========================================+
                   |
                   v
    +----------------------------------+
    |  Check for congestion signals    |  MECHANISM
    |  (duplicate ACKs, ECN, etc.)     |  (Core)
    +----------------------------------+
                   |
             +-----+-----+
             |           |
             v           v
    +----------------+  +------------------+
    |  No congestion |  | Congestion event |
    +----------------+  +------------------+
             |                   |
             v                   v
    +===================+  +===================+
    || ca_ops->        ||  || ca_ops->        ||  STRATEGY
    || cong_avoid()    ||  || ssthresh()      ||  (decides
    || (grow cwnd)     ||  || (reduce cwnd)   ||   how much)
    +===================+  +===================+
```

**中文说明：**

`tcp_ack()`的控制流：核心处理ACK——更新RTT估计、更新SACK记分板（机制），然后调用策略的`pkts_acked()`通知收到ACK，核心检查拥塞信号，如果无拥塞则调用策略的`cong_avoid()`增长窗口，如果有拥塞则调用策略的`ssthresh()`减小窗口。策略决定增长/减小多少。

---

## Why Strategy is Required Here

### 1. Network Conditions Vary Dramatically

```
    NETWORK TYPE           BEST CONGESTION ALGORITHM
    ============           =========================

    Datacenter (low RTT)   DCTCP / High-speed variants
    +-------------------+  - Sub-millisecond RTT
    | Fast, low latency |  - Need aggressive growth
    | Rare loss         |  - ECN-aware
    +-------------------+

    Internet (mixed)       CUBIC / BBR
    +-------------------+  - Variable RTT
    | Bufferbloat       |  - Mix of loss/delay
    | Competing flows   |  - Fair with Reno flows
    +-------------------+

    Satellite (high RTT)   Hybla
    +-------------------+  - 600ms+ RTT
    | Very high latency |  - Need special handling
    | Rare loss         |  - Can't wait for ACKs
    +-------------------+

    Wireless (lossy)       Westwood+ / Veno
    +-------------------+  - Loss != congestion
    | Random loss       |  - Need to distinguish
    | Variable capacity |  - Don't over-reduce
    +-------------------+

    NO SINGLE ALGORITHM WORKS FOR ALL NETWORKS
```

### 2. Runtime Selection is Essential

```
    SELECTION MECHANISMS:

    SYSTEM DEFAULT:
    +-------------------------------------------------------+
    | /proc/sys/net/ipv4/tcp_congestion_control             |
    | $ cat /proc/sys/net/ipv4/tcp_congestion_control       |
    | cubic                                                 |
    |                                                       |
    | $ echo reno > /proc/sys/net/ipv4/tcp_congestion_control |
    +-------------------------------------------------------+

    PER-SOCKET SELECTION:
    +-------------------------------------------------------+
    | char cong_name[] = "reno";                            |
    | setsockopt(fd, IPPROTO_TCP, TCP_CONGESTION,           |
    |            cong_name, strlen(cong_name));             |
    |                                                       |
    | Different sockets can use different algorithms!       |
    +-------------------------------------------------------+

    APPLICATION CAN CHOOSE:
    - HTTP server: use CUBIC for general clients
    - Video streaming: use BBR for smooth delivery
    - Database: use datacenter-optimized algorithm
```

**中文说明：**

为什么需要策略：(1) 网络条件差异巨大——数据中心需要激进增长、互联网需要与Reno公平竞争、卫星链路需要特殊处理高延迟、无线网络需要区分丢包和拥塞。单一算法无法适用所有网络。(2) 运行时选择是必要的——可以通过sysctl设置系统默认、可以通过`setsockopt`为每个socket选择不同算法，应用程序可以根据用途选择最佳算法。

---

## Minimal C Code Simulation

```c
/*
 * MINIMAL TCP CONGESTION CONTROL STRATEGY SIMULATION
 * 
 * Demonstrates how congestion control algorithms work as strategies.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward declarations */
struct sock;
struct tcp_congestion_ops;

/* ==========================================================
 * SOCKET / TCP STATE (Simplified)
 * ========================================================== */
struct sock {
    int fd;
    
    /* Congestion control state */
    unsigned int snd_cwnd;         /* Congestion window */
    unsigned int snd_ssthresh;     /* Slow start threshold */
    unsigned int srtt;             /* Smoothed RTT (us) */
    
    /* Congestion control strategy */
    const struct tcp_congestion_ops *ca_ops;
    
    /* Strategy-specific state */
    union {
        /* CUBIC-specific */
        struct {
            unsigned int cnt;
            unsigned int last_max_cwnd;
            unsigned int epoch_start;
        } cubic;
        
        /* Reno-specific */
        struct {
            unsigned int ack_cnt;
        } reno;
    } ca_state;
};

/* ==========================================================
 * TCP CONGESTION OPS: Strategy Interface
 * ========================================================== */
struct tcp_congestion_ops {
    const char *name;
    
    /* Initialize congestion state */
    void (*init)(struct sock *sk);
    
    /* Calculate new ssthresh after loss */
    unsigned int (*ssthresh)(struct sock *sk);
    
    /* Congestion avoidance: increase cwnd */
    void (*cong_avoid)(struct sock *sk, unsigned int acked);
    
    /* Handle packet loss event */
    void (*on_loss)(struct sock *sk);
    
    /* Get info (for debugging) */
    void (*get_info)(struct sock *sk);
};

/* ==========================================================
 * CUBIC STRATEGY IMPLEMENTATION
 * Linux default - aggressive for high BDP networks
 * ========================================================== */

static void cubic_init(struct sock *sk)
{
    sk->ca_state.cubic.cnt = 0;
    sk->ca_state.cubic.last_max_cwnd = 0;
    sk->ca_state.cubic.epoch_start = 0;
    printf("  [CUBIC] Initialized: aggressive growth for high-speed networks\n");
}

static unsigned int cubic_ssthresh(struct sock *sk)
{
    /* CUBIC: multiplicative decrease by factor of 0.7 */
    unsigned int new_ssthresh = sk->snd_cwnd * 7 / 10;
    
    /* Remember last max for cubic function */
    sk->ca_state.cubic.last_max_cwnd = sk->snd_cwnd;
    
    printf("  [CUBIC] ssthresh: cwnd %u -> ssthresh %u (0.7x)\n",
           sk->snd_cwnd, new_ssthresh);
    
    return new_ssthresh > 2 ? new_ssthresh : 2;
}

static void cubic_cong_avoid(struct sock *sk, unsigned int acked)
{
    /* CUBIC: uses cubic function for window growth */
    /* Simplified: just show the concept */
    
    if (sk->snd_cwnd < sk->snd_ssthresh) {
        /* Slow start: exponential growth */
        sk->snd_cwnd += acked;
        printf("  [CUBIC] Slow start: cwnd = %u (exponential)\n", sk->snd_cwnd);
    } else {
        /* Congestion avoidance: cubic growth */
        /* Real CUBIC: W(t) = C(t-K)^3 + Wmax */
        /* Simplified: faster than Reno, slower than BIC */
        sk->ca_state.cubic.cnt += acked;
        if (sk->ca_state.cubic.cnt >= sk->snd_cwnd) {
            sk->snd_cwnd++;
            sk->ca_state.cubic.cnt = 0;
        }
        printf("  [CUBIC] Cong avoid: cwnd = %u (cubic growth)\n", sk->snd_cwnd);
    }
}

static void cubic_on_loss(struct sock *sk)
{
    printf("  [CUBIC] Loss detected: reducing cwnd\n");
    sk->snd_ssthresh = cubic_ssthresh(sk);
    sk->snd_cwnd = sk->snd_ssthresh;
}

static void cubic_get_info(struct sock *sk)
{
    printf("  [CUBIC] State: cwnd=%u ssthresh=%u last_max=%u\n",
           sk->snd_cwnd, sk->snd_ssthresh, 
           sk->ca_state.cubic.last_max_cwnd);
}

static const struct tcp_congestion_ops cubic_ops = {
    .name = "cubic",
    .init = cubic_init,
    .ssthresh = cubic_ssthresh,
    .cong_avoid = cubic_cong_avoid,
    .on_loss = cubic_on_loss,
    .get_info = cubic_get_info,
};

/* ==========================================================
 * RENO STRATEGY IMPLEMENTATION
 * Classic - conservative, widely deployed
 * ========================================================== */

static void reno_init(struct sock *sk)
{
    sk->ca_state.reno.ack_cnt = 0;
    printf("  [RENO] Initialized: classic AIMD (additive increase, "
           "multiplicative decrease)\n");
}

static unsigned int reno_ssthresh(struct sock *sk)
{
    /* Reno: multiplicative decrease by factor of 0.5 */
    unsigned int new_ssthresh = sk->snd_cwnd / 2;
    
    printf("  [RENO] ssthresh: cwnd %u -> ssthresh %u (0.5x)\n",
           sk->snd_cwnd, new_ssthresh);
    
    return new_ssthresh > 2 ? new_ssthresh : 2;
}

static void reno_cong_avoid(struct sock *sk, unsigned int acked)
{
    if (sk->snd_cwnd < sk->snd_ssthresh) {
        /* Slow start: exponential growth */
        sk->snd_cwnd += acked;
        printf("  [RENO] Slow start: cwnd = %u (exponential)\n", sk->snd_cwnd);
    } else {
        /* Congestion avoidance: linear growth (AIMD) */
        /* Increase cwnd by 1/cwnd per ACK = 1 MSS per RTT */
        sk->ca_state.reno.ack_cnt += acked;
        if (sk->ca_state.reno.ack_cnt >= sk->snd_cwnd) {
            sk->snd_cwnd++;
            sk->ca_state.reno.ack_cnt = 0;
        }
        printf("  [RENO] Cong avoid: cwnd = %u (linear growth)\n", sk->snd_cwnd);
    }
}

static void reno_on_loss(struct sock *sk)
{
    printf("  [RENO] Loss detected: halving cwnd\n");
    sk->snd_ssthresh = reno_ssthresh(sk);
    sk->snd_cwnd = sk->snd_ssthresh;
}

static void reno_get_info(struct sock *sk)
{
    printf("  [RENO] State: cwnd=%u ssthresh=%u ack_cnt=%u\n",
           sk->snd_cwnd, sk->snd_ssthresh, sk->ca_state.reno.ack_cnt);
}

static const struct tcp_congestion_ops reno_ops = {
    .name = "reno",
    .init = reno_init,
    .ssthresh = reno_ssthresh,
    .cong_avoid = reno_cong_avoid,
    .on_loss = reno_on_loss,
    .get_info = reno_get_info,
};

/* ==========================================================
 * TCP CORE (MECHANISM)
 * ========================================================== */

/* Registry of available algorithms */
static const struct tcp_congestion_ops *available_algorithms[] = {
    &cubic_ops,
    &reno_ops,
    NULL,
};

/* Select congestion control algorithm by name */
static const struct tcp_congestion_ops *tcp_ca_find(const char *name)
{
    for (int i = 0; available_algorithms[i]; i++) {
        if (strcmp(available_algorithms[i]->name, name) == 0)
            return available_algorithms[i];
    }
    return NULL;
}

/* Set congestion control for socket (like setsockopt) */
static int tcp_set_congestion_control(struct sock *sk, const char *name)
{
    const struct tcp_congestion_ops *ca = tcp_ca_find(name);
    if (!ca) {
        printf("[TCP CORE] Unknown algorithm: %s\n", name);
        return -1;
    }
    
    sk->ca_ops = ca;
    printf("[TCP CORE] Set congestion control to: %s\n", ca->name);
    
    /* Initialize the new algorithm */
    if (ca->init)
        ca->init(sk);
    
    return 0;
}

/* Core: Process ACK - calls strategy for cwnd update */
static void tcp_ack(struct sock *sk, unsigned int acked)
{
    printf("[TCP CORE] tcp_ack: received ACK for %u segments\n", acked);
    
    /* MECHANISM: ACK processing */
    printf("[TCP CORE] Updating RTT estimates, SACK scoreboard...\n");
    
    /* STRATEGY: Let congestion control decide how to grow window */
    if (sk->ca_ops && sk->ca_ops->cong_avoid) {
        sk->ca_ops->cong_avoid(sk, acked);
    }
}

/* Core: Handle loss event - calls strategy for ssthresh */
static void tcp_enter_loss(struct sock *sk)
{
    printf("[TCP CORE] tcp_enter_loss: packet loss detected\n");
    
    /* MECHANISM: Enter loss recovery state */
    printf("[TCP CORE] Entering loss recovery state...\n");
    
    /* STRATEGY: Let congestion control decide how to reduce */
    if (sk->ca_ops && sk->ca_ops->on_loss) {
        sk->ca_ops->on_loss(sk);
    }
}

/* Core: Get congestion control info */
static void tcp_get_info(struct sock *sk)
{
    printf("[TCP CORE] Connection state:\n");
    printf("[TCP CORE] Algorithm: %s\n", sk->ca_ops ? sk->ca_ops->name : "none");
    
    if (sk->ca_ops && sk->ca_ops->get_info)
        sk->ca_ops->get_info(sk);
}

/* ==========================================================
 * DEMONSTRATION
 * ========================================================== */
int main(void)
{
    printf("================================================\n");
    printf("TCP CONGESTION CONTROL STRATEGY DEMONSTRATION\n");
    printf("================================================\n");

    /* Create two sockets with different congestion control */
    struct sock sock1 = {
        .fd = 1,
        .snd_cwnd = 10,
        .snd_ssthresh = 65535,
        .srtt = 50000,  /* 50ms */
    };

    struct sock sock2 = {
        .fd = 2,
        .snd_cwnd = 10,
        .snd_ssthresh = 65535,
        .srtt = 50000,
    };

    /* === Configure different algorithms === */
    printf("\n=== CONFIGURING SOCKETS ===\n");
    tcp_set_congestion_control(&sock1, "cubic");
    tcp_set_congestion_control(&sock2, "reno");

    /* === Simulate normal operation: receive ACKs === */
    printf("\n=== RECEIVING ACKS (Normal Growth) ===\n");
    
    printf("\n--- Socket 1 (CUBIC) ---\n");
    for (int i = 0; i < 5; i++) {
        tcp_ack(&sock1, 2);
    }

    printf("\n--- Socket 2 (RENO) ---\n");
    for (int i = 0; i < 5; i++) {
        tcp_ack(&sock2, 2);
    }

    /* === Simulate congestion: packet loss === */
    printf("\n=== PACKET LOSS EVENT ===\n");
    
    printf("\n--- Socket 1 (CUBIC) loss response ---\n");
    tcp_enter_loss(&sock1);
    
    printf("\n--- Socket 2 (RENO) loss response ---\n");
    tcp_enter_loss(&sock2);

    /* === Recovery: grow again === */
    printf("\n=== RECOVERY (Different Growth Patterns) ===\n");
    
    printf("\n--- Socket 1 (CUBIC) recovery ---\n");
    for (int i = 0; i < 3; i++) {
        tcp_ack(&sock1, 2);
    }

    printf("\n--- Socket 2 (RENO) recovery ---\n");
    for (int i = 0; i < 3; i++) {
        tcp_ack(&sock2, 2);
    }

    /* === Final state === */
    printf("\n=== FINAL STATE ===\n");
    printf("\n--- Socket 1 ---\n");
    tcp_get_info(&sock1);
    printf("\n--- Socket 2 ---\n");
    tcp_get_info(&sock2);

    printf("\n================================================\n");
    printf("KEY OBSERVATIONS:\n");
    printf("1. Same TCP core, different congestion behaviors\n");
    printf("2. CUBIC reduces to 70%%, RENO reduces to 50%%\n");
    printf("3. Each algorithm maintains its own state\n");
    printf("4. Algorithm selection per-socket at runtime\n");
    printf("================================================\n");

    return 0;
}
```

---

## What the Kernel Core Does NOT Control

```
+=============================================================================+
|              WHAT CORE DOES NOT CONTROL (Strategy Owns)                      |
+=============================================================================+

    THE CORE DOES NOT DECIDE:

    1. HOW MUCH TO REDUCE ON LOSS
       +-------------------------------------------------------+
       | Core detects loss, calls ca_ops->ssthresh()           |
       | Strategy decides reduction amount:                    |
       |   CUBIC: 70% (less aggressive)                        |
       |   Reno:  50% (standard AIMD)                          |
       |   Vegas: based on delay, not loss                     |
       +-------------------------------------------------------+

    2. HOW FAST TO GROW WINDOW
       +-------------------------------------------------------+
       | Core calls ca_ops->cong_avoid() on each ACK           |
       | Strategy decides growth rate:                         |
       |   CUBIC: cubic function (aggressive in high BDP)      |
       |   Reno:  1 MSS per RTT (linear)                       |
       |   BIC:   binary search                                |
       +-------------------------------------------------------+

    3. WHAT STATE TO MAINTAIN
       +-------------------------------------------------------+
       | CUBIC: epoch_start, last_max_cwnd, K, origin_point    |
       | Vegas: base_rtt, cntRTT, minRTT                       |
       | Reno:  just needs cwnd and ssthresh (minimal state)   |
       +-------------------------------------------------------+

    4. HOW TO INTERPRET SIGNALS
       +-------------------------------------------------------+
       | Some algorithms use different signals:                |
       |   Vegas: delay increase = congestion                  |
       |   Reno:  packet loss = congestion                     |
       |   DCTCP: ECN marks = congestion                       |
       +-------------------------------------------------------+

    THE CORE ONLY PROVIDES:
    - Segment transmission mechanics
    - ACK processing and timing
    - Loss detection (duplicate ACKs, timeout)
    - RTT measurement infrastructure
```

**中文说明：**

核心不控制的内容：(1) 丢包时减少多少——核心检测丢包并调用`ssthresh()`，策略决定减少量（CUBIC 70%，Reno 50%）；(2) 窗口增长多快——核心在每个ACK时调用`cong_avoid()`，策略决定增长率；(3) 维护什么状态——CUBIC有epoch_start等，Vegas有base_rtt等，Reno只需最小状态；(4) 如何解释信号——Vegas用延迟增加、Reno用丢包、DCTCP用ECN标记。核心只提供：段发送机制、ACK处理和计时、丢包检测、RTT测量。

---

## Real Kernel Code Reference (v3.2)

### struct tcp_congestion_ops in include/net/tcp.h

```c
struct tcp_congestion_ops {
    struct list_head	list;
    unsigned long flags;

    void (*init)(struct sock *sk);
    void (*release)(struct sock *sk);

    u32 (*ssthresh)(struct sock *sk);
    u32 (*min_cwnd)(const struct sock *sk);
    void (*cong_avoid)(struct sock *sk, u32 ack, u32 in_flight);
    void (*set_state)(struct sock *sk, u8 new_state);
    void (*cwnd_event)(struct sock *sk, enum tcp_ca_event ev);
    void (*pkts_acked)(struct sock *sk, u32 num_acked, s32 rtt_us);
    u32 (*undo_cwnd)(struct sock *sk);

    char name[TCP_CA_NAME_MAX];
    struct module *owner;
};
```

### Selection mechanism in net/ipv4/tcp.c

```c
/* Setsockopt TCP_CONGESTION */
static int tcp_set_congestion_control(struct sock *sk, char *name)
{
    struct inet_connection_sock *icsk = inet_csk(sk);
    struct tcp_congestion_ops *ca;

    ca = tcp_ca_find(name);
    if (!ca)
        return -ENOENT;

    icsk->icsk_ca_ops = ca;
    if (ca->init)
        ca->init(sk);
    
    return 0;
}
```

---

## Key Takeaways

1. **Complete algorithm encapsulation**: Each `tcp_congestion_ops` is a complete congestion control policy
2. **Per-socket selection**: Different connections can use different algorithms
3. **Runtime switchable**: Algorithm can be changed via `setsockopt()`
4. **Self-contained state**: Each algorithm maintains its own state (cubic params, reno counters)
5. **No core involvement in decisions**: Core only triggers events, strategy decides response
