# Linux Kernel Embedded State Machines (v3.2)

## Overview

This document explains how **state machines are implemented** in the Linux kernel, covering enum+switch FSMs, ops-based state patterns, and hybrid approaches.

---

## TCP FSM Analysis

### TCP State Definition

From `include/net/tcp_states.h`:

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
    TCP_CLOSING,
    TCP_MAX_STATES
};
```

### TCP State Diagram

```
+------------------------------------------------------------------+
|  TCP STATE MACHINE                                               |
+------------------------------------------------------------------+

                              CLOSED
                                |
                +---------------+---------------+
                |                               |
           passive open                    active open
           (listen)                        (connect)
                |                               |
                v                               v
            LISTEN                          SYN_SENT
                |                               |
           rcv SYN                         rcv SYN+ACK
           send SYN+ACK                    send ACK
                |                               |
                v                               v
           SYN_RECV --------+     +-------- ESTABLISHED
                |           |     |              |
           rcv ACK          +-----+         close/rcv FIN
                |                               |
                +-------> ESTABLISHED <---------+
                              |
              +---------------+---------------+
              |                               |
          close                           rcv FIN
          send FIN                        send ACK
              |                               |
              v                               v
          FIN_WAIT1                       CLOSE_WAIT
              |                               |
         +----+----+                      close
         |         |                      send FIN
    rcv ACK    rcv FIN+ACK                    |
         |     send ACK                       v
         v         |                      LAST_ACK
     FIN_WAIT2     |                          |
         |         v                      rcv ACK
    rcv FIN    CLOSING                        |
    send ACK       |                          v
         |     rcv ACK                     CLOSED
         v         |
     TIME_WAIT <---+
         |
    2MSL timeout
         |
         v
      CLOSED
```

**中文解释：**
- TCP 状态机定义了连接的完整生命周期
- 从 CLOSED → LISTEN/SYN_SENT → ESTABLISHED → 各种关闭状态
- 每个转换由接收的报文或应用程序操作触发

### TCP State Transition Code

From `net/ipv4/tcp_input.c`:

```c
int tcp_rcv_state_process(struct sock *sk, struct sk_buff *skb,
                          const struct tcphdr *th, unsigned int len)
{
    struct tcp_sock *tp = tcp_sk(sk);
    int queued = 0;
    
    switch (sk->sk_state) {
    case TCP_CLOSE:
        goto discard;
        
    case TCP_LISTEN:
        if (th->syn) {
            /* Create new socket for incoming connection */
            if (icsk->icsk_af_ops->conn_request(sk, skb) < 0)
                return 1;
            /* ... */
            tcp_set_state(sk, TCP_SYN_RECV);
        }
        goto discard;
        
    case TCP_SYN_SENT:
        queued = tcp_rcv_synsent_state_process(sk, skb, th, len);
        /* ... */
        break;
        
    case TCP_SYN_RECV:
        if (th->ack) {
            tcp_set_state(sk, TCP_ESTABLISHED);
            /* ... */
        }
        break;
    
    /* ... more states ... */
    }
    
    return 0;
}
```

### Centralized State Mutation

From `net/ipv4/tcp.c`:

```c
void tcp_set_state(struct sock *sk, int state)
{
    int oldstate = sk->sk_state;
    
    /* State transition validation */
    switch (state) {
    case TCP_ESTABLISHED:
        if (oldstate != TCP_SYN_RECV && oldstate != TCP_SYN_SENT)
            WARN_ON(1);  /* Invalid transition! */
        break;
    /* ... validation for other states ... */
    }
    
    sk->sk_state = state;
    
    /* Side effects */
    if (state == TCP_CLOSE)
        sk->sk_shutdown = SHUTDOWN_MASK;
    
    /* Debugging/tracing */
    trace_tcp_state_change(sk, oldstate, state);
}
```

**中文解释：**
- TCP 状态转换集中在 `tcp_set_state()` 函数
- 验证状态转换有效性
- 执行副作用（如设置关闭标志）
- 追踪/调试支持

---

## Other Kernel FSM Examples

### Example 1: USB Device States

From `include/linux/usb/ch9.h`:

```c
enum usb_device_state {
    USB_STATE_NOTATTACHED = 0,
    USB_STATE_ATTACHED,
    USB_STATE_POWERED,
    USB_STATE_RECONNECTING,
    USB_STATE_UNAUTHENTICATED,
    USB_STATE_DEFAULT,
    USB_STATE_ADDRESS,
    USB_STATE_CONFIGURED,
    USB_STATE_SUSPENDED,
};
```

```
+------------------------------------------------------------------+
|  USB DEVICE STATE MACHINE                                        |
+------------------------------------------------------------------+

    NOTATTACHED
         |
    attach
         v
    ATTACHED
         |
    power on
         v
    POWERED
         |
    reset
         v
    DEFAULT
         |
    set address
         v
    ADDRESS
         |
    configure
         v
    CONFIGURED <--+
         |        |
    suspend   resume
         |        |
         v        |
    SUSPENDED ----+
```

### Example 2: SCTP Association States

From `include/net/sctp/constants.h`:

```c
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
```

### Example 3: Block Request States

```c
enum rq_cmd_type_bits {
    REQ_TYPE_FS             = 1,    /* fs request */
    REQ_TYPE_BLOCK_PC       = 2,    /* scsi command */
    REQ_TYPE_SENSE          = 3,    /* sense request */
    REQ_TYPE_PM_SUSPEND     = 4,    /* suspend request */
    REQ_TYPE_PM_RESUME      = 5,    /* resume request */
    /* ... */
};
```

**中文解释：**
- USB、SCTP、Block I/O 都使用 enum+switch FSM
- 每个都有明确的状态定义
- 转换由事件触发

---

## State Transition Invariants

```
+------------------------------------------------------------------+
|  FSM INVARIANTS                                                  |
+------------------------------------------------------------------+

    INVARIANT 1: Valid Transitions Only
    +----------------------------------------------------------+
    | Not all state combinations are valid transitions         |
    | TCP_LISTEN → TCP_ESTABLISHED is INVALID                  |
    | Must go through SYN_RECV                                 |
    +----------------------------------------------------------+
    
    INVARIANT 2: Centralized State Change
    +----------------------------------------------------------+
    | All state changes through a single function              |
    | tcp_set_state(), not direct sk->sk_state = X            |
    | Enables validation, logging, side effects                |
    +----------------------------------------------------------+
    
    INVARIANT 3: Lock Protection
    +----------------------------------------------------------+
    | State and related data protected by same lock            |
    | Cannot have inconsistent state+data                      |
    +----------------------------------------------------------+
    
    INVARIANT 4: Completion Guarantees
    +----------------------------------------------------------+
    | Some states guarantee resources are available            |
    | TCP_ESTABLISHED = connection resources allocated         |
    | TCP_CLOSE = resources being freed                        |
    +----------------------------------------------------------+
```

### Transition Validation

```c
/* Pattern: Validate before transition */
int state_transition(struct context *ctx, int new_state)
{
    /* Validate transition is legal */
    if (!is_valid_transition(ctx->state, new_state)) {
        WARN_ON(1);
        return -EINVAL;
    }
    
    /* Validate preconditions for new state */
    switch (new_state) {
    case STATE_ACTIVE:
        if (!ctx->resources_allocated)
            return -EINVAL;
        break;
    }
    
    /* Perform transition */
    ctx->state = new_state;
    
    /* Execute side effects */
    on_state_change(ctx);
    
    return 0;
}
```

**中文解释：**
- 不变量1：只允许有效转换
- 不变量2：集中状态变更
- 不变量3：锁保护状态一致性
- 不变量4：状态保证资源可用性

---

## Why FSMs are Split Across Files

```
+------------------------------------------------------------------+
|  TCP FSM FILE ORGANIZATION                                       |
+------------------------------------------------------------------+

    net/ipv4/
    ├── tcp.c           # State machine core, tcp_set_state()
    ├── tcp_input.c     # RX path transitions (rcv_state_process)
    ├── tcp_output.c    # TX path triggers
    ├── tcp_timer.c     # Timeout-driven transitions
    └── tcp_minisocks.c # TIME_WAIT and SYN_RECV handling
    
    WHY SPLIT:
    +----------------------------------------------------------+
    | 1. FUNCTIONAL SEPARATION                                  |
    |    - Input processing in tcp_input.c                     |
    |    - Output processing in tcp_output.c                   |
    |    - Timer handling in tcp_timer.c                       |
    |                                                          |
    | 2. SIZE MANAGEMENT                                        |
    |    - tcp_input.c alone is 5000+ lines                    |
    |    - Single file would be unmaintainable                 |
    |                                                          |
    | 3. COMPILATION UNITS                                      |
    |    - Separate object files                               |
    |    - Parallel compilation                                |
    |    - Smaller incremental builds                          |
    |                                                          |
    | 4. DIFFERENT AUTHORS/MAINTAINERS                          |
    |    - Timer code by timer expert                          |
    |    - Input processing by protocol expert                 |
    +----------------------------------------------------------+
```

```
+------------------------------------------------------------------+
|  TRANSITION TRIGGERS BY FILE                                     |
+------------------------------------------------------------------+

    tcp_input.c:
    - SYN received → LISTEN to SYN_RECV
    - ACK received → SYN_RECV to ESTABLISHED
    - FIN received → various close states
    
    tcp_output.c:
    - Application close → send FIN
    - Retransmit timeout → may affect state
    
    tcp_timer.c:
    - Keepalive timeout → may close
    - TIME_WAIT timeout → to CLOSED
    - Retransmit timeout → error handling
```

**中文解释：**
- TCP FSM 分布在多个文件：
  - `tcp.c`：状态机核心
  - `tcp_input.c`：接收路径转换
  - `tcp_output.c`：发送路径触发
  - `tcp_timer.c`：超时驱动转换
- 原因：功能分离、大小管理、编译优化、维护者分工

---

## User-Space FSM Patterns

### Pattern 1: enum + switch FSM

```c
/* user_space_fsm_switch.c */

#include <stdio.h>
#include <stdbool.h>

/*---------------------------------------------------------
 * State and Event definitions
 *---------------------------------------------------------*/
typedef enum {
    STATE_IDLE,
    STATE_CONNECTING,
    STATE_CONNECTED,
    STATE_DISCONNECTING,
    STATE_ERROR,
    STATE_MAX
} state_t;

typedef enum {
    EVENT_CONNECT,
    EVENT_CONNECTED,
    EVENT_DISCONNECT,
    EVENT_DISCONNECTED,
    EVENT_ERROR,
    EVENT_TIMEOUT,
    EVENT_MAX
} event_t;

const char *state_names[] = {
    "IDLE", "CONNECTING", "CONNECTED", 
    "DISCONNECTING", "ERROR"
};

const char *event_names[] = {
    "CONNECT", "CONNECTED", "DISCONNECT",
    "DISCONNECTED", "ERROR", "TIMEOUT"
};

/*---------------------------------------------------------
 * FSM Context
 *---------------------------------------------------------*/
struct connection {
    state_t state;
    int retry_count;
    void *private_data;
};

/*---------------------------------------------------------
 * Centralized state transition (like tcp_set_state)
 *---------------------------------------------------------*/
static bool set_state(struct connection *conn, state_t new_state)
{
    state_t old = conn->state;
    
    /* Validate transition */
    static const bool valid[STATE_MAX][STATE_MAX] = {
        /*                IDLE  CONN'ING CONN'ED DISC'ING ERROR */
        /* IDLE */      { 0,    1,       0,      0,       1 },
        /* CONNECTING */{ 0,    0,       1,      0,       1 },
        /* CONNECTED */ { 0,    0,       0,      1,       1 },
        /* DISCONNECT */{ 1,    0,       0,      0,       1 },
        /* ERROR */     { 1,    1,       0,      0,       0 },
    };
    
    if (!valid[old][new_state]) {
        printf("INVALID TRANSITION: %s -> %s\n",
               state_names[old], state_names[new_state]);
        return false;
    }
    
    printf("STATE: %s -> %s\n", state_names[old], state_names[new_state]);
    conn->state = new_state;
    return true;
}

/*---------------------------------------------------------
 * State machine processing (like tcp_rcv_state_process)
 *---------------------------------------------------------*/
void process_event(struct connection *conn, event_t event)
{
    printf("EVENT: %s in state %s\n", 
           event_names[event], state_names[conn->state]);
    
    switch (conn->state) {
    case STATE_IDLE:
        if (event == EVENT_CONNECT) {
            /* Start connection */
            conn->retry_count = 0;
            set_state(conn, STATE_CONNECTING);
        }
        break;
        
    case STATE_CONNECTING:
        switch (event) {
        case EVENT_CONNECTED:
            set_state(conn, STATE_CONNECTED);
            break;
        case EVENT_TIMEOUT:
            if (++conn->retry_count < 3) {
                printf("Retry %d\n", conn->retry_count);
            } else {
                set_state(conn, STATE_ERROR);
            }
            break;
        case EVENT_ERROR:
            set_state(conn, STATE_ERROR);
            break;
        default:
            break;
        }
        break;
        
    case STATE_CONNECTED:
        switch (event) {
        case EVENT_DISCONNECT:
            set_state(conn, STATE_DISCONNECTING);
            break;
        case EVENT_ERROR:
            set_state(conn, STATE_ERROR);
            break;
        default:
            break;
        }
        break;
        
    case STATE_DISCONNECTING:
        if (event == EVENT_DISCONNECTED) {
            set_state(conn, STATE_IDLE);
        }
        break;
        
    case STATE_ERROR:
        if (event == EVENT_CONNECT) {
            conn->retry_count = 0;
            set_state(conn, STATE_CONNECTING);
        }
        break;
        
    default:
        break;
    }
}

/*---------------------------------------------------------
 * Demo
 *---------------------------------------------------------*/
int main(void)
{
    struct connection conn = { .state = STATE_IDLE };
    
    process_event(&conn, EVENT_CONNECT);
    process_event(&conn, EVENT_TIMEOUT);
    process_event(&conn, EVENT_TIMEOUT);
    process_event(&conn, EVENT_CONNECTED);
    process_event(&conn, EVENT_DISCONNECT);
    process_event(&conn, EVENT_DISCONNECTED);
    
    return 0;
}
```

### Pattern 2: Table-Driven FSM

```c
/* user_space_fsm_table.c */

#include <stdio.h>

typedef enum { S_IDLE, S_ACTIVE, S_DONE, S_MAX } state_t;
typedef enum { E_START, E_WORK, E_STOP, E_MAX } event_t;

typedef void (*action_fn)(void *ctx);
typedef struct {
    state_t next_state;
    action_fn action;
} transition_t;

/* Action functions */
void action_start(void *ctx) { printf("Starting...\n"); }
void action_work(void *ctx)  { printf("Working...\n"); }
void action_stop(void *ctx)  { printf("Stopping...\n"); }
void action_none(void *ctx)  { }

/* Transition table */
transition_t fsm_table[S_MAX][E_MAX] = {
    /* S_IDLE */   { {S_ACTIVE, action_start}, {S_IDLE, action_none}, {S_IDLE, action_none} },
    /* S_ACTIVE */ { {S_ACTIVE, action_none}, {S_ACTIVE, action_work}, {S_DONE, action_stop} },
    /* S_DONE */   { {S_ACTIVE, action_start}, {S_DONE, action_none}, {S_DONE, action_none} },
};

state_t current_state = S_IDLE;

void dispatch(event_t event)
{
    transition_t *t = &fsm_table[current_state][event];
    if (t->action)
        t->action(NULL);
    current_state = t->next_state;
}

int main(void)
{
    dispatch(E_START);  /* IDLE -> ACTIVE */
    dispatch(E_WORK);   /* Stay ACTIVE */
    dispatch(E_WORK);   /* Stay ACTIVE */
    dispatch(E_STOP);   /* ACTIVE -> DONE */
    return 0;
}
```

**中文解释：**
- **Pattern 1**：enum + switch，与 TCP FSM 类似
- **Pattern 2**：表驱动，转换表预定义，代码更简洁

---

## Failure Analysis

### Common FSM Bugs

```
+------------------------------------------------------------------+
|  FSM FAILURE MODES                                               |
+------------------------------------------------------------------+

    BUG 1: Missing Transition Handler
    +----------------------------------------------------------+
    | switch (state) {                                         |
    | case STATE_A: ...                                        |
    | case STATE_B: ...                                        |
    | /* STATE_C not handled! */                               |
    | }                                                        |
    | Result: Undefined behavior, state gets "stuck"           |
    +----------------------------------------------------------+
    
    BUG 2: Invalid Transition Not Detected
    +----------------------------------------------------------+
    | obj->state = new_state;  /* Direct assignment */         |
    | No validation that transition is legal                   |
    | Result: Inconsistent state, hard to debug                |
    +----------------------------------------------------------+
    
    BUG 3: Race Condition on State
    +----------------------------------------------------------+
    | Thread A: if (state == X) { ... state = Y; }             |
    | Thread B: if (state == X) { ... state = Z; }             |
    | Both see state X, both try to transition                 |
    | Result: Corrupted state                                  |
    +----------------------------------------------------------+
    
    BUG 4: State/Data Inconsistency
    +----------------------------------------------------------+
    | state = STATE_CONNECTED;                                 |
    | /* Interrupted here! */                                  |
    | conn->socket = socket;                                   |
    | Result: State says connected but socket is NULL          |
    +----------------------------------------------------------+
```

### Prevention Patterns

```c
/* Prevention: Lock protects state + data */
void safe_transition(struct context *ctx, state_t new_state, 
                     void *new_data)
{
    spin_lock(&ctx->lock);
    
    /* Atomic: state and data change together */
    ctx->state = new_state;
    ctx->data = new_data;
    
    spin_unlock(&ctx->lock);
}

/* Prevention: Default case in switch */
switch (state) {
case STATE_A:
    /* ... */
    break;
case STATE_B:
    /* ... */
    break;
default:
    WARN_ON(1);  /* Catch missing states */
    break;
}

/* Prevention: Validation function */
static bool is_valid_transition(state_t from, state_t to)
{
    static const bool valid[][STATE_MAX] = {
        /* ... transition matrix ... */
    };
    return valid[from][to];
}
```

**中文解释：**
- **Bug 1**：缺少转换处理 → 使用 default case
- **Bug 2**：无效转换未检测 → 集中验证
- **Bug 3**：状态竞争 → 锁保护
- **Bug 4**：状态/数据不一致 → 原子更新

---

## Summary

```
+------------------------------------------------------------------+
|  FSM PATTERN SUMMARY                                             |
+------------------------------------------------------------------+

    ENUM + SWITCH PATTERN:
    +----------------------------------------------------------+
    | Use when:                                                |
    | - States are well-defined and finite                     |
    | - Transitions are complex with many conditions           |
    | - Need visibility into state machine logic               |
    | - Performance critical (switch is fast)                  |
    +----------------------------------------------------------+
    
    TABLE-DRIVEN PATTERN:
    +----------------------------------------------------------+
    | Use when:                                                |
    | - Transitions are regular and predictable                |
    | - Want to separate logic from data                       |
    | - May need to modify transitions at runtime              |
    | - Want compact representation                            |
    +----------------------------------------------------------+
    
    OPS-BASED STATE PATTERN:
    +----------------------------------------------------------+
    | Use when:                                                |
    | - Behavior differs significantly per state               |
    | - States are "modes" of operation                        |
    | - Want to avoid large switch statements                  |
    | - Examples: line disciplines, congestion algorithms      |
    +----------------------------------------------------------+
    
    BEST PRACTICES:
    +----------------------------------------------------------+
    | 1. Define states explicitly with enum                    |
    | 2. Centralize state transitions in one function          |
    | 3. Validate transitions before applying                  |
    | 4. Protect state with locks                              |
    | 5. Update state and related data atomically              |
    | 6. Add default/fallback handling                         |
    | 7. Log/trace state transitions for debugging             |
    +----------------------------------------------------------+
```

**中文总结：**
状态机实现模式：
1. **enum + switch**：状态明确、转换复杂、需要可见性
2. **表驱动**：转换规则、逻辑与数据分离
3. **ops-based**：状态即行为模式
4. **最佳实践**：显式定义状态、集中转换、验证有效性、锁保护

