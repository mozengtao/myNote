# TCP FSM: Hands-On Reconstruction

## 1. What to Keep from Kernel TCP

```
+------------------------------------------------------------------+
|                    ESSENTIAL ELEMENTS TO KEEP                     |
+------------------------------------------------------------------+
|                                                                   |
|  ✓ State enumeration (11 states)                                 |
|  ✓ Event-driven architecture                                     |
|  ✓ Switch-based dispatch                                         |
|  ✓ Bitmask flags for state groups                               |
|  ✓ Orthogonal flags (shutdown, capabilities)                    |
|  ✓ Centralized state setter                                      |
|  ✓ Entry/exit actions pattern                                    |
|  ✓ Error propagation via error field                            |
|  ✓ Timeout handling with backoff                                 |
|                                                                   |
+------------------------------------------------------------------+
```

## 2. What to Discard

```
+------------------------------------------------------------------+
|                    KERNEL-SPECIFIC TO DISCARD                     |
+------------------------------------------------------------------+
|                                                                   |
|  ✗ spinlock/bh_lock (use pthread_mutex instead)                 |
|  ✗ softirq/backlog queue (use event queue instead)              |
|  ✗ sk_buff structure (use simple buffer struct)                 |
|  ✗ Memory barriers (handled by pthread_mutex)                   |
|  ✗ Prequeue/DMA handling                                         |
|  ✗ Hash tables for socket lookup                                |
|  ✗ Kernel memory allocation (GFP flags)                         |
|  ✗ Per-CPU structures                                            |
|  ✗ Netfilter hooks                                               |
|                                                                   |
+------------------------------------------------------------------+
```

---

## 3. Simplified TCP FSM in User-Space C

### 3.1 Header File: `simple_tcp_fsm.h`

```c
/*
 * simple_tcp_fsm.h - Simplified TCP-like FSM for educational purposes
 * 
 * This preserves the TCP state machine structure without kernel dependencies.
 */

#ifndef SIMPLE_TCP_FSM_H
#define SIMPLE_TCP_FSM_H

#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>

/*
 * =============================================================================
 * STATE DEFINITIONS (from tcp_states.h)
 * =============================================================================
 */

/* Primary connection states - matches RFC 793 */
typedef enum {
    TCP_CLOSE = 0,        /* No connection */
    TCP_LISTEN,           /* Waiting for connection request */
    TCP_SYN_SENT,         /* SYN sent, waiting for SYN-ACK */
    TCP_SYN_RECV,         /* SYN received, SYN-ACK sent */
    TCP_ESTABLISHED,      /* Connection established */
    TCP_FIN_WAIT_1,       /* FIN sent, waiting for ACK */
    TCP_FIN_WAIT_2,       /* FIN ACKed, waiting for peer FIN */
    TCP_CLOSE_WAIT,       /* Peer FIN received, waiting for close() */
    TCP_CLOSING,          /* Simultaneous close */
    TCP_LAST_ACK,         /* Waiting for final ACK */
    TCP_TIME_WAIT,        /* Waiting for old segments to expire */
    TCP_STATE_MAX
} tcp_state_t;

/* Bitmask versions for efficient multi-state checks */
#define TCPF_CLOSE        (1 << TCP_CLOSE)
#define TCPF_LISTEN       (1 << TCP_LISTEN)
#define TCPF_SYN_SENT     (1 << TCP_SYN_SENT)
#define TCPF_SYN_RECV     (1 << TCP_SYN_RECV)
#define TCPF_ESTABLISHED  (1 << TCP_ESTABLISHED)
#define TCPF_FIN_WAIT_1   (1 << TCP_FIN_WAIT_1)
#define TCPF_FIN_WAIT_2   (1 << TCP_FIN_WAIT_2)
#define TCPF_CLOSE_WAIT   (1 << TCP_CLOSE_WAIT)
#define TCPF_CLOSING      (1 << TCP_CLOSING)
#define TCPF_LAST_ACK     (1 << TCP_LAST_ACK)
#define TCPF_TIME_WAIT    (1 << TCP_TIME_WAIT)

/* State group masks */
#define TCPF_CONNECTED    (TCPF_ESTABLISHED | TCPF_FIN_WAIT_1 | TCPF_FIN_WAIT_2 | \
                           TCPF_CLOSE_WAIT | TCPF_CLOSING | TCPF_LAST_ACK)

/*
 * =============================================================================
 * EVENT DEFINITIONS
 * =============================================================================
 */

typedef enum {
    /* User-initiated events */
    EV_ACTIVE_OPEN,       /* connect() called */
    EV_PASSIVE_OPEN,      /* listen() called */
    EV_CLOSE,             /* close() called */
    EV_SEND,              /* send() called */
    
    /* Packet-initiated events */
    EV_RCV_SYN,           /* Received SYN */
    EV_RCV_SYN_ACK,       /* Received SYN+ACK */
    EV_RCV_ACK,           /* Received ACK */
    EV_RCV_FIN,           /* Received FIN */
    EV_RCV_RST,           /* Received RST */
    EV_RCV_DATA,          /* Received data */
    
    /* Timer events */
    EV_TIMEOUT,           /* Timer expired */
    EV_TIME_WAIT_DONE,    /* TIME_WAIT completed */
    
    EV_MAX
} tcp_event_t;

/*
 * =============================================================================
 * FLAGS (orthogonal to state)
 * =============================================================================
 */

/* Shutdown flags */
#define SHUTDOWN_RD       (1 << 0)  /* Receive shutdown */
#define SHUTDOWN_WR       (1 << 1)  /* Send shutdown */
#define SHUTDOWN_MASK     (SHUTDOWN_RD | SHUTDOWN_WR)

/* Connection flags */
#define FLAG_SOCK_DEAD    (1 << 2)  /* Socket orphaned */
#define FLAG_SOCK_DONE    (1 << 3)  /* Connection complete */

/*
 * =============================================================================
 * CONNECTION STRUCTURE (simplified struct sock / struct tcp_sock)
 * =============================================================================
 */

typedef struct tcp_connection {
    /* Primary state */
    tcp_state_t state;
    
    /* Orthogonal flags */
    unsigned int shutdown;
    unsigned int flags;
    
    /* Error tracking */
    int error;
    
    /* Sequence numbers (simplified) */
    uint32_t snd_nxt;     /* Next sequence to send */
    uint32_t rcv_nxt;     /* Next sequence expected */
    
    /* Timing */
    int retransmit_count;
    int timeout_ms;
    
    /* Synchronization (replaces spinlock) */
    pthread_mutex_t lock;
    
    /* Callbacks */
    void (*on_state_change)(struct tcp_connection *conn, 
                            tcp_state_t old_state, 
                            tcp_state_t new_state);
    void (*on_error)(struct tcp_connection *conn, int error);
    void (*on_data_ready)(struct tcp_connection *conn);
    
    /* Application context */
    void *user_data;
    
} tcp_connection_t;

/*
 * =============================================================================
 * API
 * =============================================================================
 */

/* Lifecycle */
tcp_connection_t *tcp_connection_create(void);
void tcp_connection_destroy(tcp_connection_t *conn);

/* Event handling */
int tcp_handle_event(tcp_connection_t *conn, tcp_event_t event, void *data);

/* State query */
tcp_state_t tcp_get_state(tcp_connection_t *conn);
const char *tcp_state_name(tcp_state_t state);
bool tcp_can_send(tcp_connection_t *conn);
bool tcp_can_recv(tcp_connection_t *conn);

#endif /* SIMPLE_TCP_FSM_H */
```

### 3.2 Implementation File: `simple_tcp_fsm.c`

```c
/*
 * simple_tcp_fsm.c - Simplified TCP-like FSM implementation
 */

#include "simple_tcp_fsm.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/*
 * =============================================================================
 * STATE NAMES (for debugging)
 * =============================================================================
 */

static const char *state_names[TCP_STATE_MAX] = {
    [TCP_CLOSE]       = "CLOSE",
    [TCP_LISTEN]      = "LISTEN",
    [TCP_SYN_SENT]    = "SYN_SENT",
    [TCP_SYN_RECV]    = "SYN_RECV",
    [TCP_ESTABLISHED] = "ESTABLISHED",
    [TCP_FIN_WAIT_1]  = "FIN_WAIT_1",
    [TCP_FIN_WAIT_2]  = "FIN_WAIT_2",
    [TCP_CLOSE_WAIT]  = "CLOSE_WAIT",
    [TCP_CLOSING]     = "CLOSING",
    [TCP_LAST_ACK]    = "LAST_ACK",
    [TCP_TIME_WAIT]   = "TIME_WAIT",
};

const char *tcp_state_name(tcp_state_t state) {
    if (state >= TCP_STATE_MAX) return "INVALID";
    return state_names[state];
}

/*
 * =============================================================================
 * CLOSE STATE TRANSITION TABLE (from tcp.c new_state[])
 * =============================================================================
 */

#define TCP_ACTION_FIN  (1 << 7)
#define TCP_STATE_MASK  0x0F

static const unsigned char close_state_table[TCP_STATE_MAX] = {
    [TCP_CLOSE]       = TCP_CLOSE,
    [TCP_LISTEN]      = TCP_CLOSE,
    [TCP_SYN_SENT]    = TCP_CLOSE,
    [TCP_SYN_RECV]    = TCP_FIN_WAIT_1 | TCP_ACTION_FIN,
    [TCP_ESTABLISHED] = TCP_FIN_WAIT_1 | TCP_ACTION_FIN,
    [TCP_FIN_WAIT_1]  = TCP_FIN_WAIT_1,
    [TCP_FIN_WAIT_2]  = TCP_FIN_WAIT_2,
    [TCP_CLOSE_WAIT]  = TCP_LAST_ACK | TCP_ACTION_FIN,
    [TCP_CLOSING]     = TCP_CLOSING,
    [TCP_LAST_ACK]    = TCP_LAST_ACK,
    [TCP_TIME_WAIT]   = TCP_CLOSE,
};

/*
 * =============================================================================
 * CENTRALIZED STATE SETTER (from tcp.c tcp_set_state())
 * =============================================================================
 */

static void tcp_set_state(tcp_connection_t *conn, tcp_state_t new_state)
{
    tcp_state_t old_state = conn->state;
    
    /* Exit actions for old state */
    switch (old_state) {
    case TCP_SYN_SENT:
        /* Clear connection timeout */
        conn->timeout_ms = 0;
        break;
    case TCP_ESTABLISHED:
        /* Statistics: connection no longer established */
        break;
    default:
        break;
    }
    
    /* Change state */
    conn->state = new_state;
    
    /* Entry actions for new state */
    switch (new_state) {
    case TCP_ESTABLISHED:
        /* Initialize for data transfer */
        conn->retransmit_count = 0;
        break;
    case TCP_TIME_WAIT:
        /* Start 2MSL timer (simulated) */
        conn->timeout_ms = 2 * 60 * 1000;  /* 2 minutes */
        break;
    case TCP_CLOSE:
        /* Cleanup */
        conn->shutdown = SHUTDOWN_MASK;
        conn->flags |= FLAG_SOCK_DONE;
        break;
    default:
        break;
    }
    
    /* Notify callback */
    if (conn->on_state_change) {
        conn->on_state_change(conn, old_state, new_state);
    }
}

/*
 * =============================================================================
 * FIN HANDLING (from tcp_input.c tcp_fin())
 * =============================================================================
 */

static void tcp_fin(tcp_connection_t *conn)
{
    conn->shutdown |= SHUTDOWN_RD;
    conn->flags |= FLAG_SOCK_DONE;
    
    switch (conn->state) {
    case TCP_SYN_RECV:
    case TCP_ESTABLISHED:
        /* Move to CLOSE_WAIT */
        tcp_set_state(conn, TCP_CLOSE_WAIT);
        break;
        
    case TCP_FIN_WAIT_1:
        /* Simultaneous close -> CLOSING */
        tcp_set_state(conn, TCP_CLOSING);
        break;
        
    case TCP_FIN_WAIT_2:
        /* Normal close -> TIME_WAIT */
        tcp_set_state(conn, TCP_TIME_WAIT);
        break;
        
    case TCP_CLOSE_WAIT:
    case TCP_CLOSING:
    case TCP_LAST_ACK:
        /* Retransmitted FIN, ignore */
        break;
        
    default:
        fprintf(stderr, "tcp_fin: Unexpected state %s\n", 
                tcp_state_name(conn->state));
        break;
    }
}

/*
 * =============================================================================
 * RST HANDLING (from tcp_input.c tcp_reset())
 * =============================================================================
 */

static void tcp_reset(tcp_connection_t *conn)
{
    switch (conn->state) {
    case TCP_SYN_SENT:
        conn->error = -1;  /* ECONNREFUSED */
        break;
    case TCP_CLOSE_WAIT:
        conn->error = -2;  /* EPIPE */
        break;
    case TCP_CLOSE:
        return;  /* Already closed */
    default:
        conn->error = -3;  /* ECONNRESET */
        break;
    }
    
    if (conn->on_error) {
        conn->on_error(conn, conn->error);
    }
    
    tcp_set_state(conn, TCP_CLOSE);
}

/*
 * =============================================================================
 * CLOSE HANDLING (from tcp.c tcp_close())
 * =============================================================================
 */

static int tcp_close_state(tcp_connection_t *conn)
{
    int next = close_state_table[conn->state];
    int new_state = next & TCP_STATE_MASK;
    
    tcp_set_state(conn, new_state);
    
    return next & TCP_ACTION_FIN;  /* Returns whether to send FIN */
}

/*
 * =============================================================================
 * STATE HANDLERS
 * =============================================================================
 */

/* Handle events in CLOSE state */
static int handle_close_state(tcp_connection_t *conn, tcp_event_t event)
{
    switch (event) {
    case EV_ACTIVE_OPEN:
        tcp_set_state(conn, TCP_SYN_SENT);
        conn->timeout_ms = 1000;  /* 1 second initial timeout */
        /* TODO: Send SYN */
        return 0;
        
    case EV_PASSIVE_OPEN:
        tcp_set_state(conn, TCP_LISTEN);
        return 0;
        
    default:
        return 0;  /* Ignore */
    }
}

/* Handle events in LISTEN state */
static int handle_listen_state(tcp_connection_t *conn, tcp_event_t event)
{
    switch (event) {
    case EV_RCV_SYN:
        tcp_set_state(conn, TCP_SYN_RECV);
        /* TODO: Send SYN+ACK */
        return 0;
        
    case EV_CLOSE:
        tcp_set_state(conn, TCP_CLOSE);
        return 0;
        
    default:
        return 0;  /* Ignore */
    }
}

/* Handle events in SYN_SENT state */
static int handle_syn_sent_state(tcp_connection_t *conn, tcp_event_t event)
{
    switch (event) {
    case EV_RCV_SYN_ACK:
        tcp_set_state(conn, TCP_ESTABLISHED);
        /* TODO: Send ACK */
        return 0;
        
    case EV_RCV_SYN:
        /* Simultaneous open */
        tcp_set_state(conn, TCP_SYN_RECV);
        /* TODO: Send SYN+ACK */
        return 0;
        
    case EV_RCV_RST:
        tcp_reset(conn);
        return -1;
        
    case EV_TIMEOUT:
        conn->retransmit_count++;
        if (conn->retransmit_count > 5) {
            conn->error = -4;  /* ETIMEDOUT */
            tcp_set_state(conn, TCP_CLOSE);
            return -1;
        }
        /* TODO: Retransmit SYN */
        conn->timeout_ms *= 2;  /* Exponential backoff */
        return 0;
        
    case EV_CLOSE:
        tcp_set_state(conn, TCP_CLOSE);
        return 0;
        
    default:
        return 0;
    }
}

/* Handle events in SYN_RECV state */
static int handle_syn_recv_state(tcp_connection_t *conn, tcp_event_t event)
{
    switch (event) {
    case EV_RCV_ACK:
        tcp_set_state(conn, TCP_ESTABLISHED);
        return 0;
        
    case EV_RCV_RST:
        tcp_reset(conn);
        return -1;
        
    case EV_CLOSE:
        if (tcp_close_state(conn)) {
            /* TODO: Send FIN */
        }
        return 0;
        
    default:
        return 0;
    }
}

/* Handle events in ESTABLISHED state */
static int handle_established_state(tcp_connection_t *conn, tcp_event_t event)
{
    switch (event) {
    case EV_RCV_FIN:
        tcp_fin(conn);
        /* TODO: Send ACK */
        return 0;
        
    case EV_RCV_RST:
        tcp_reset(conn);
        return -1;
        
    case EV_RCV_DATA:
        if (conn->on_data_ready) {
            conn->on_data_ready(conn);
        }
        return 0;
        
    case EV_CLOSE:
        if (tcp_close_state(conn)) {
            /* TODO: Send FIN */
        }
        return 0;
        
    default:
        return 0;
    }
}

/* Handle events in FIN_WAIT_1 state */
static int handle_fin_wait_1_state(tcp_connection_t *conn, tcp_event_t event)
{
    switch (event) {
    case EV_RCV_ACK:
        /* FIN has been ACKed */
        tcp_set_state(conn, TCP_FIN_WAIT_2);
        return 0;
        
    case EV_RCV_FIN:
        /* Simultaneous close */
        tcp_fin(conn);
        /* TODO: Send ACK */
        return 0;
        
    case EV_RCV_RST:
        tcp_reset(conn);
        return -1;
        
    default:
        return 0;
    }
}

/* Handle events in FIN_WAIT_2 state */
static int handle_fin_wait_2_state(tcp_connection_t *conn, tcp_event_t event)
{
    switch (event) {
    case EV_RCV_FIN:
        tcp_fin(conn);
        /* TODO: Send ACK */
        return 0;
        
    case EV_TIMEOUT:
        /* FIN_WAIT_2 timeout */
        tcp_set_state(conn, TCP_CLOSE);
        return 0;
        
    default:
        return 0;
    }
}

/* Handle events in CLOSE_WAIT state */
static int handle_close_wait_state(tcp_connection_t *conn, tcp_event_t event)
{
    switch (event) {
    case EV_CLOSE:
        if (tcp_close_state(conn)) {
            /* TODO: Send FIN */
        }
        return 0;
        
    default:
        return 0;
    }
}

/* Handle events in CLOSING state */
static int handle_closing_state(tcp_connection_t *conn, tcp_event_t event)
{
    switch (event) {
    case EV_RCV_ACK:
        /* FIN has been ACKed */
        tcp_set_state(conn, TCP_TIME_WAIT);
        return 0;
        
    default:
        return 0;
    }
}

/* Handle events in LAST_ACK state */
static int handle_last_ack_state(tcp_connection_t *conn, tcp_event_t event)
{
    switch (event) {
    case EV_RCV_ACK:
        /* Connection fully closed */
        tcp_set_state(conn, TCP_CLOSE);
        return 0;
        
    default:
        return 0;
    }
}

/* Handle events in TIME_WAIT state */
static int handle_time_wait_state(tcp_connection_t *conn, tcp_event_t event)
{
    switch (event) {
    case EV_TIME_WAIT_DONE:
        tcp_set_state(conn, TCP_CLOSE);
        return 0;
        
    case EV_RCV_FIN:
        /* Retransmitted FIN - send ACK */
        /* TODO: Send ACK, restart timer */
        return 0;
        
    default:
        return 0;
    }
}

/*
 * =============================================================================
 * MAIN DISPATCHER (from tcp_rcv_state_process())
 * =============================================================================
 */

int tcp_handle_event(tcp_connection_t *conn, tcp_event_t event, void *data)
{
    int result;
    
    pthread_mutex_lock(&conn->lock);
    
    switch (conn->state) {
    case TCP_CLOSE:
        result = handle_close_state(conn, event);
        break;
    case TCP_LISTEN:
        result = handle_listen_state(conn, event);
        break;
    case TCP_SYN_SENT:
        result = handle_syn_sent_state(conn, event);
        break;
    case TCP_SYN_RECV:
        result = handle_syn_recv_state(conn, event);
        break;
    case TCP_ESTABLISHED:
        result = handle_established_state(conn, event);
        break;
    case TCP_FIN_WAIT_1:
        result = handle_fin_wait_1_state(conn, event);
        break;
    case TCP_FIN_WAIT_2:
        result = handle_fin_wait_2_state(conn, event);
        break;
    case TCP_CLOSE_WAIT:
        result = handle_close_wait_state(conn, event);
        break;
    case TCP_CLOSING:
        result = handle_closing_state(conn, event);
        break;
    case TCP_LAST_ACK:
        result = handle_last_ack_state(conn, event);
        break;
    case TCP_TIME_WAIT:
        result = handle_time_wait_state(conn, event);
        break;
    default:
        fprintf(stderr, "Unknown state: %d\n", conn->state);
        result = -1;
    }
    
    pthread_mutex_unlock(&conn->lock);
    return result;
}

/*
 * =============================================================================
 * UTILITY FUNCTIONS
 * =============================================================================
 */

tcp_state_t tcp_get_state(tcp_connection_t *conn) {
    return conn->state;
}

bool tcp_can_send(tcp_connection_t *conn) {
    return ((1 << conn->state) & (TCPF_ESTABLISHED | TCPF_CLOSE_WAIT)) &&
           !(conn->shutdown & SHUTDOWN_WR);
}

bool tcp_can_recv(tcp_connection_t *conn) {
    return ((1 << conn->state) & TCPF_CONNECTED) &&
           !(conn->shutdown & SHUTDOWN_RD);
}

/*
 * =============================================================================
 * LIFECYCLE
 * =============================================================================
 */

tcp_connection_t *tcp_connection_create(void) {
    tcp_connection_t *conn = calloc(1, sizeof(tcp_connection_t));
    if (!conn) return NULL;
    
    conn->state = TCP_CLOSE;
    conn->timeout_ms = 1000;
    pthread_mutex_init(&conn->lock, NULL);
    
    return conn;
}

void tcp_connection_destroy(tcp_connection_t *conn) {
    if (!conn) return;
    pthread_mutex_destroy(&conn->lock);
    free(conn);
}
```

---

## 4. Usage Example

```c
/* example.c */
#include "simple_tcp_fsm.h"
#include <stdio.h>

void on_state_change(tcp_connection_t *conn, tcp_state_t old, tcp_state_t new) {
    printf("State: %s -> %s\n", tcp_state_name(old), tcp_state_name(new));
}

void on_error(tcp_connection_t *conn, int error) {
    printf("Error: %d\n", error);
}

int main() {
    tcp_connection_t *conn = tcp_connection_create();
    conn->on_state_change = on_state_change;
    conn->on_error = on_error;
    
    printf("=== Client Connection Sequence ===\n");
    
    /* Simulate client connection */
    tcp_handle_event(conn, EV_ACTIVE_OPEN, NULL);
    tcp_handle_event(conn, EV_RCV_SYN_ACK, NULL);
    tcp_handle_event(conn, EV_RCV_DATA, NULL);
    tcp_handle_event(conn, EV_CLOSE, NULL);
    tcp_handle_event(conn, EV_RCV_ACK, NULL);
    tcp_handle_event(conn, EV_RCV_FIN, NULL);
    tcp_handle_event(conn, EV_TIME_WAIT_DONE, NULL);
    
    tcp_connection_destroy(conn);
    
    return 0;
}

/* Output:
 * === Client Connection Sequence ===
 * State: CLOSE -> SYN_SENT
 * State: SYN_SENT -> ESTABLISHED
 * State: ESTABLISHED -> FIN_WAIT_1
 * State: FIN_WAIT_1 -> FIN_WAIT_2
 * State: FIN_WAIT_2 -> TIME_WAIT
 * State: TIME_WAIT -> CLOSE
 */
```

---

## 5. Summary: Reconstruction Principles

```
+------------------------------------------------------------------+
|                    RECONSTRUCTION GUIDE                           |
+------------------------------------------------------------------+
|                                                                   |
|  PRESERVED FROM KERNEL:                                           |
|    ✓ 11-state enumeration matching RFC 793                       |
|    ✓ Bitmask flags for state groups                              |
|    ✓ Switch-based dispatch in main handler                       |
|    ✓ Per-state handler functions                                 |
|    ✓ Centralized tcp_set_state() with entry/exit actions        |
|    ✓ close_state_table with action flags                        |
|    ✓ tcp_fin() and tcp_reset() patterns                         |
|    ✓ Shutdown flags orthogonal to state                          |
|    ✓ Error propagation via error field + callback               |
|                                                                   |
|  ADAPTED FOR USER-SPACE:                                          |
|    - pthread_mutex instead of spinlock/bh_lock                   |
|    - Simple callbacks instead of sk_* function pointers          |
|    - No backlog queue (event handling is synchronous)            |
|    - No sk_buff (just event type + data pointer)                |
|    - No hash tables (single connection per instance)            |
|                                                                   |
|  OMITTED (kernel-specific):                                       |
|    - Memory barriers (handled by pthread_mutex)                  |
|    - Softirq context handling                                    |
|    - Network device integration                                  |
|    - Actual packet sending/receiving                             |
|                                                                   |
+------------------------------------------------------------------+
```

中文总结：
1. **保留核心结构**：11状态枚举、位掩码、switch调度、集中状态设置器
2. **适应用户空间**：pthread_mutex代替spinlock，简单回调代替内核函数指针
3. **省略内核特定**：内存屏障、软中断上下文、网络设备集成
4. **可扩展**：添加实际网络I/O、定时器集成、事件队列即可成为完整实现

This simplified FSM captures the **essence** of TCP state machine design while remaining
comprehensible and usable in user-space applications.
