# State Pattern in Linux Kernel

## 1. Pattern Overview

```
+------------------------------------------------------------------+
|                       STATE PATTERN                               |
+------------------------------------------------------------------+
|                                                                   |
|    +------------------+         +------------------+              |
|    |    Context       |         |     State        |              |
|    +------------------+  uses   +------------------+              |
|    | - state*         |-------->| + handle()       |              |
|    | + request()      |         +--------+---------+              |
|    |   {              |                  ^                        |
|    |   state->handle()|                  |                        |
|    |   }              |         +--------+--------+               |
|    | + setState()     |         |        |        |               |
|    +------------------+         v        v        v               |
|                            +-------+ +-------+ +-------+          |
|                            |StateA | |StateB | |StateC |          |
|                            +-------+ +-------+ +-------+          |
|                            |handle | |handle | |handle |          |
|                            +-------+ +-------+ +-------+          |
|                                                                   |
|    Object changes behavior based on internal state                |
|    States are encapsulated in separate structures                 |
|    Transitions change the current state pointer                   |
|                                                                   |
+------------------------------------------------------------------+
```

**中文说明：** 状态模式将对象状态封装为独立结构体，通过切换状态对象改变行为，替代大量if-else。在Linux内核中，设备状态机、网络连接状态、进程状态等都使用这种模式。每个状态实现相同的接口，但有不同的行为，状态转换通过更改状态指针完成。

---

## 2. Linux Kernel Implementation

### 2.1 Kernel Example: Device State Machine

```c
/* From: drivers/s390/cio/device.h */

/**
 * Device states - enumeration of all possible states
 */
enum dev_state {
    DEV_STATE_NOT_OPER,
    DEV_STATE_SENSE_PGID,
    DEV_STATE_SENSE_ID,
    DEV_STATE_OFFLINE,
    DEV_STATE_VERIFY,
    DEV_STATE_ONLINE,
    DEV_STATE_W4SENSE,
    DEV_STATE_DISBAND_PGID,
    DEV_STATE_BOXED,
    DEV_STATE_TIMEOUT_KILL,
    DEV_STATE_QUIESCE,
    DEV_STATE_DISCONNECTED,
    DEV_STATE_DISCONNECTED_SENSE_ID,
    DEV_STATE_CMFCHANGE,
    DEV_STATE_CMFUPDATE,
    DEV_STATE_STEAL_LOCK,
    NR_DEV_STATES  /* Total number of states */
};

/**
 * Device events - triggers for state transitions
 */
enum dev_event {
    DEV_EVENT_NOTOPER,
    DEV_EVENT_INTERRUPT,
    DEV_EVENT_TIMEOUT,
    DEV_EVENT_VERIFY,
    NR_DEV_EVENTS
};

/**
 * State transition function type
 */
typedef void (fsm_func_t)(struct ccw_device *, enum dev_event);

/**
 * State transition table (jump table)
 * [current_state][event] = handler function
 */
extern fsm_func_t *dev_jumptable[NR_DEV_STATES][NR_DEV_EVENTS];

/**
 * dev_fsm_event - Process an event through state machine
 * @cdev: Device
 * @dev_event: Event to process
 *
 * Looks up handler in jump table and calls it.
 */
static inline void dev_fsm_event(struct ccw_device *cdev, enum dev_event dev_event)
{
    int state = cdev->private->state;
    
    /* Call state-specific handler */
    dev_jumptable[state][dev_event](cdev, dev_event);
}

/**
 * dev_fsm_final_state - Check if in final state
 * @cdev: Device
 *
 * Returns true if device is in a stable state.
 */
static inline int dev_fsm_final_state(struct ccw_device *cdev)
{
    return (cdev->private->state == DEV_STATE_NOT_OPER ||
            cdev->private->state == DEV_STATE_OFFLINE ||
            cdev->private->state == DEV_STATE_ONLINE ||
            cdev->private->state == DEV_STATE_BOXED);
}
```

### 2.2 Kernel Example: Network Connection States

```c
/* From: include/net/llc_c_st.h */

/**
 * LLC Connection states
 */
#define LLC_CONN_STATE_ADM       1  /* Disconnected, initial */
#define LLC_CONN_STATE_SETUP     2  /* Disconnected */
#define LLC_CONN_STATE_NORMAL    3  /* Connected */
#define LLC_CONN_STATE_BUSY      4  /* Connected, busy */
#define LLC_CONN_STATE_REJ       5  /* Connected, reject */
#define LLC_CONN_STATE_AWAIT     6  /* Connected, waiting */
#define LLC_CONN_STATE_AWAIT_BUSY 7
#define LLC_CONN_STATE_AWAIT_REJ 8
#define LLC_CONN_STATE_D_CONN    9  /* Disconnecting */
#define LLC_CONN_STATE_RESET    10
#define LLC_CONN_STATE_ERROR    11
#define LLC_CONN_STATE_TEMP     12

/**
 * State transition structure
 */
struct llc_conn_state_trans {
    llc_conn_ev_t       ev;           /* Event type */
    u8                  next_state;   /* State after transition */
    llc_conn_ev_qfyr_t  *ev_qualifiers;  /* Event qualifiers */
    llc_conn_action_t   *ev_actions;  /* Actions to perform */
};

/**
 * State structure
 */
struct llc_conn_state {
    u8 current_state;
    struct llc_conn_state_trans **transitions;
};

/* State table */
extern struct llc_conn_state llc_conn_state_table[];
```

### 2.3 Kernel Example: ISDN Call States

```c
/* From: drivers/isdn/hisax/callc.c */

/**
 * Call states for ISDN connections
 */
enum {
    ST_NULL,          /* 0: Inactive */
    ST_OUT_DIAL,      /* 1: Outgoing, awaiting confirm */
    ST_IN_WAIT_LL,    /* 2: Incoming, wait for LL confirm */
    ST_IN_ALERT_SENT, /* 3: Incoming, ALERT sent */
    ST_IN_WAIT_CONN_ACK, /* 4: CONNECT sent, await ACK */
    ST_WAIT_BCONN,    /* 5: Wait for B-channel */
    ST_ACTIVE,        /* 6: Active call */
    ST_WAIT_BRELEASE, /* 7: Clearing, wait B release */
    ST_WAIT_BREL_DISC,/* 8: Clearing, DISCONNECT received */
    ST_WAIT_DCOMMAND, /* 9: Clearing, wait D command */
    ST_WAIT_DRELEASE, /* 10: DISCONNECT sent, wait RELEASE */
    ST_WAIT_D_REL_CNF,/* 11: RELEASE sent, wait confirm */
    ST_IN_PROCEED_SEND, /* 12: Incoming, proceeding */
};

/**
 * Call events
 */
enum {
    EV_DIAL,          /* 0 */
    EV_SETUP_CNF,     /* 1 */
    EV_ACCEPTB,       /* 2 */
    EV_DISCONNECT_IND,/* 3 */
    EV_RELEASE,       /* 4 */
    /* ... more events ... */
};

/* State names for debugging */
static char *strState[] = {
    "ST_NULL", "ST_OUT_DIAL", "ST_IN_WAIT_LL",
    "ST_IN_ALERT_SENT", "ST_IN_WAIT_CONN_ACK",
    /* ... */
};
```

### 2.4 Kernel Example: Process States

```c
/* From: include/linux/sched.h */

/**
 * Task states - basic process states
 */
#define TASK_RUNNING        0
#define TASK_INTERRUPTIBLE  1
#define TASK_UNINTERRUPTIBLE 2
#define __TASK_STOPPED      4
#define __TASK_TRACED       8
#define TASK_DEAD           64
#define TASK_WAKEKILL       128
#define TASK_WAKING         256

/**
 * struct task_struct - Process descriptor
 *
 * Contains current state and state-dependent behavior.
 */
struct task_struct {
    volatile long state;  /* Current state */
    /* ... other fields ... */
};

/**
 * set_task_state - Atomic state change
 * @tsk: Task
 * @state_value: New state
 *
 * Changes task state with proper memory barriers.
 */
#define set_task_state(tsk, state_value) \
    set_mb((tsk)->state, (state_value))
```

### 2.5 Architecture Diagram

```
+------------------------------------------------------------------+
|               LINUX KERNEL STATE PATTERN                          |
|                  (Device State Machine)                           |
+------------------------------------------------------------------+
|                                                                   |
|    Device Context                                                 |
|    +---------------------------+                                  |
|    | struct ccw_device         |                                  |
|    +---------------------------+                                  |
|    | private->state = OFFLINE  |                                  |
|    +-------------+-------------+                                  |
|                  |                                                |
|    Event: DEV_EVENT_VERIFY                                        |
|                  |                                                |
|                  v                                                |
|    +-------------+-------------+                                  |
|    | dev_fsm_event(cdev, event)|                                  |
|    +-------------+-------------+                                  |
|                  |                                                |
|                  v                                                |
|    +-------------+-------------+                                  |
|    | Lookup in jump table:     |                                  |
|    | dev_jumptable[OFFLINE]    |                                  |
|    |            [VERIFY]       |                                  |
|    +-------------+-------------+                                  |
|                  |                                                |
|                  v                                                |
|    +-------------+-------------+                                  |
|    | Call state handler:       |                                  |
|    | ccw_device_verify_start() |                                  |
|    +-------------+-------------+                                  |
|                  |                                                |
|                  v                                                |
|    +-------------+-------------+                                  |
|    | Handler may change state: |                                  |
|    | cdev->private->state =    |                                  |
|    |     DEV_STATE_VERIFY;     |                                  |
|    +---------------------------+                                  |
|                                                                   |
|    State Transition Diagram:                                      |
|                                                                   |
|    +-------+  verify   +--------+  success  +--------+            |
|    |OFFLINE|---------->| VERIFY |---------->| ONLINE |            |
|    +-------+           +--------+           +--------+            |
|        ^                   |                    |                 |
|        |                   | fail               | error           |
|        |                   v                    v                 |
|        |              +--------+           +--------+             |
|        +--------------| NOT_OP |<----------| BOXED  |             |
|                       +--------+           +--------+             |
|                                                                   |
+------------------------------------------------------------------+
```

**中文说明：** Linux内核的设备状态机是状态模式的典型实现。设备的当前状态存储在cdev->private->state中。当事件发生时，dev_fsm_event()函数使用状态和事件作为索引在跳转表中查找处理函数，并调用该函数。处理函数执行状态特定的操作，并可能改变设备的状态。

---

## 3. Advantages Analysis

| Advantage | Description |
|-----------|-------------|
| **Eliminates Conditionals** | Replace switch/if-else with state objects |
| **Localized State Behavior** | Each state's logic in one place |
| **Easy to Add States** | New states don't affect existing code |
| **Explicit Transitions** | State changes are clear and traceable |
| **State-Specific Data** | States can have their own data |
| **Testability** | Each state can be tested independently |

**中文说明：** 状态模式的优势包括：消除条件判断（用状态对象替代switch/if-else）、状态行为本地化（每个状态的逻辑集中在一处）、易于添加状态（新状态不影响现有代码）、显式转换（状态变化清晰可追踪）、状态特定数据（每个状态可以有自己的数据）、可测试性（每个状态可独立测试）。

---

## 4. User-Space Implementation Example

```c
/*
 * State Pattern - User Space Implementation
 * Mimics Linux Kernel's device state machine
 * 
 * Compile: gcc -o state state.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* ============================================================
 * State Definitions
 * ============================================================ */

/* Forward declarations */
struct connection;
struct state;

/* State handler function type */
typedef void (*state_handler_t)(struct connection *conn, int event);

/* Event types */
enum event_type {
    EVENT_CONNECT = 0,
    EVENT_DISCONNECT,
    EVENT_SEND,
    EVENT_RECEIVE,
    EVENT_TIMEOUT,
    EVENT_ERROR,
    EVENT_RESET,
    NUM_EVENTS
};

/* State types */
enum state_type {
    STATE_DISCONNECTED = 0,
    STATE_CONNECTING,
    STATE_CONNECTED,
    STATE_SENDING,
    STATE_RECEIVING,
    STATE_ERROR,
    NUM_STATES
};

/* Event names for debugging */
const char *event_names[] = {
    "CONNECT", "DISCONNECT", "SEND", "RECEIVE", "TIMEOUT", "ERROR", "RESET"
};

/* State names for debugging */
const char *state_names[] = {
    "DISCONNECTED", "CONNECTING", "CONNECTED", 
    "SENDING", "RECEIVING", "ERROR"
};

/* ============================================================
 * State Structure
 * ============================================================ */

struct state {
    enum state_type type;
    const char *name;
    
    /* State-specific handlers for each event */
    state_handler_t handlers[NUM_EVENTS];
    
    /* Entry/exit actions */
    void (*on_enter)(struct connection *conn);
    void (*on_exit)(struct connection *conn);
};

/* ============================================================
 * Context Structure (Connection)
 * ============================================================ */

struct connection {
    const char *name;
    
    /* Current state */
    struct state *current_state;
    enum state_type state_type;
    
    /* Connection data */
    char remote_host[256];
    int port;
    int bytes_sent;
    int bytes_received;
    int error_count;
    
    /* State machine statistics */
    int transitions;
};

/* ============================================================
 * State Transition Function
 * ============================================================ */

void connection_set_state(struct connection *conn, enum state_type new_state);

/* ============================================================
 * State Handler Implementations
 * ============================================================ */

/* --- DISCONNECTED State Handlers --- */

void disconnected_on_enter(struct connection *conn)
{
    printf("  [State] Entering DISCONNECTED\n");
    conn->bytes_sent = 0;
    conn->bytes_received = 0;
}

void disconnected_connect(struct connection *conn, int event)
{
    printf("  [Handler] Initiating connection to %s:%d\n", 
           conn->remote_host, conn->port);
    connection_set_state(conn, STATE_CONNECTING);
}

void disconnected_send(struct connection *conn, int event)
{
    printf("  [Handler] Cannot send: not connected!\n");
}

void disconnected_receive(struct connection *conn, int event)
{
    printf("  [Handler] Cannot receive: not connected!\n");
}

/* --- CONNECTING State Handlers --- */

void connecting_on_enter(struct connection *conn)
{
    printf("  [State] Entering CONNECTING\n");
}

void connecting_timeout(struct connection *conn, int event)
{
    printf("  [Handler] Connection timeout!\n");
    conn->error_count++;
    connection_set_state(conn, STATE_ERROR);
}

void connecting_receive(struct connection *conn, int event)
{
    /* Simulate receiving connection ACK */
    printf("  [Handler] Received connection ACK\n");
    connection_set_state(conn, STATE_CONNECTED);
}

void connecting_error(struct connection *conn, int event)
{
    printf("  [Handler] Connection failed!\n");
    conn->error_count++;
    connection_set_state(conn, STATE_DISCONNECTED);
}

/* --- CONNECTED State Handlers --- */

void connected_on_enter(struct connection *conn)
{
    printf("  [State] Entering CONNECTED\n");
    printf("  [State] Ready to send/receive data\n");
}

void connected_disconnect(struct connection *conn, int event)
{
    printf("  [Handler] Disconnecting...\n");
    connection_set_state(conn, STATE_DISCONNECTED);
}

void connected_send(struct connection *conn, int event)
{
    printf("  [Handler] Preparing to send data\n");
    connection_set_state(conn, STATE_SENDING);
}

void connected_receive(struct connection *conn, int event)
{
    printf("  [Handler] Preparing to receive data\n");
    connection_set_state(conn, STATE_RECEIVING);
}

void connected_error(struct connection *conn, int event)
{
    printf("  [Handler] Error in connected state\n");
    conn->error_count++;
    connection_set_state(conn, STATE_ERROR);
}

/* --- SENDING State Handlers --- */

void sending_on_enter(struct connection *conn)
{
    printf("  [State] Entering SENDING\n");
}

void sending_send(struct connection *conn, int event)
{
    /* Simulate sending data */
    int bytes = 1024;
    conn->bytes_sent += bytes;
    printf("  [Handler] Sent %d bytes (total: %d)\n", bytes, conn->bytes_sent);
    connection_set_state(conn, STATE_CONNECTED);
}

void sending_timeout(struct connection *conn, int event)
{
    printf("  [Handler] Send timeout!\n");
    conn->error_count++;
    connection_set_state(conn, STATE_ERROR);
}

void sending_error(struct connection *conn, int event)
{
    printf("  [Handler] Send error!\n");
    conn->error_count++;
    connection_set_state(conn, STATE_ERROR);
}

/* --- RECEIVING State Handlers --- */

void receiving_on_enter(struct connection *conn)
{
    printf("  [State] Entering RECEIVING\n");
}

void receiving_receive(struct connection *conn, int event)
{
    /* Simulate receiving data */
    int bytes = 2048;
    conn->bytes_received += bytes;
    printf("  [Handler] Received %d bytes (total: %d)\n", 
           bytes, conn->bytes_received);
    connection_set_state(conn, STATE_CONNECTED);
}

void receiving_timeout(struct connection *conn, int event)
{
    printf("  [Handler] Receive timeout!\n");
    connection_set_state(conn, STATE_CONNECTED);  /* Return to connected */
}

void receiving_error(struct connection *conn, int event)
{
    printf("  [Handler] Receive error!\n");
    conn->error_count++;
    connection_set_state(conn, STATE_ERROR);
}

/* --- ERROR State Handlers --- */

void error_on_enter(struct connection *conn)
{
    printf("  [State] Entering ERROR (errors: %d)\n", conn->error_count);
}

void error_reset(struct connection *conn, int event)
{
    printf("  [Handler] Resetting connection\n");
    conn->error_count = 0;
    connection_set_state(conn, STATE_DISCONNECTED);
}

void error_disconnect(struct connection *conn, int event)
{
    printf("  [Handler] Force disconnect from error state\n");
    connection_set_state(conn, STATE_DISCONNECTED);
}

/* Default handler for unhandled events */
void default_handler(struct connection *conn, int event)
{
    printf("  [Handler] Event %s ignored in state %s\n",
           event_names[event], state_names[conn->state_type]);
}

/* ============================================================
 * State Table (Jump Table)
 * ============================================================ */

struct state state_table[NUM_STATES] = {
    /* STATE_DISCONNECTED */
    {
        .type = STATE_DISCONNECTED,
        .name = "DISCONNECTED",
        .handlers = {
            [EVENT_CONNECT] = disconnected_connect,
            [EVENT_DISCONNECT] = default_handler,
            [EVENT_SEND] = disconnected_send,
            [EVENT_RECEIVE] = disconnected_receive,
            [EVENT_TIMEOUT] = default_handler,
            [EVENT_ERROR] = default_handler,
            [EVENT_RESET] = default_handler,
        },
        .on_enter = disconnected_on_enter,
        .on_exit = NULL,
    },
    /* STATE_CONNECTING */
    {
        .type = STATE_CONNECTING,
        .name = "CONNECTING",
        .handlers = {
            [EVENT_CONNECT] = default_handler,
            [EVENT_DISCONNECT] = default_handler,
            [EVENT_SEND] = default_handler,
            [EVENT_RECEIVE] = connecting_receive,
            [EVENT_TIMEOUT] = connecting_timeout,
            [EVENT_ERROR] = connecting_error,
            [EVENT_RESET] = default_handler,
        },
        .on_enter = connecting_on_enter,
        .on_exit = NULL,
    },
    /* STATE_CONNECTED */
    {
        .type = STATE_CONNECTED,
        .name = "CONNECTED",
        .handlers = {
            [EVENT_CONNECT] = default_handler,
            [EVENT_DISCONNECT] = connected_disconnect,
            [EVENT_SEND] = connected_send,
            [EVENT_RECEIVE] = connected_receive,
            [EVENT_TIMEOUT] = default_handler,
            [EVENT_ERROR] = connected_error,
            [EVENT_RESET] = default_handler,
        },
        .on_enter = connected_on_enter,
        .on_exit = NULL,
    },
    /* STATE_SENDING */
    {
        .type = STATE_SENDING,
        .name = "SENDING",
        .handlers = {
            [EVENT_CONNECT] = default_handler,
            [EVENT_DISCONNECT] = default_handler,
            [EVENT_SEND] = sending_send,
            [EVENT_RECEIVE] = default_handler,
            [EVENT_TIMEOUT] = sending_timeout,
            [EVENT_ERROR] = sending_error,
            [EVENT_RESET] = default_handler,
        },
        .on_enter = sending_on_enter,
        .on_exit = NULL,
    },
    /* STATE_RECEIVING */
    {
        .type = STATE_RECEIVING,
        .name = "RECEIVING",
        .handlers = {
            [EVENT_CONNECT] = default_handler,
            [EVENT_DISCONNECT] = default_handler,
            [EVENT_SEND] = default_handler,
            [EVENT_RECEIVE] = receiving_receive,
            [EVENT_TIMEOUT] = receiving_timeout,
            [EVENT_ERROR] = receiving_error,
            [EVENT_RESET] = default_handler,
        },
        .on_enter = receiving_on_enter,
        .on_exit = NULL,
    },
    /* STATE_ERROR */
    {
        .type = STATE_ERROR,
        .name = "ERROR",
        .handlers = {
            [EVENT_CONNECT] = default_handler,
            [EVENT_DISCONNECT] = error_disconnect,
            [EVENT_SEND] = default_handler,
            [EVENT_RECEIVE] = default_handler,
            [EVENT_TIMEOUT] = default_handler,
            [EVENT_ERROR] = default_handler,
            [EVENT_RESET] = error_reset,
        },
        .on_enter = error_on_enter,
        .on_exit = NULL,
    },
};

/* ============================================================
 * Connection Operations
 * ============================================================ */

void connection_set_state(struct connection *conn, enum state_type new_state)
{
    struct state *old_state = conn->current_state;
    struct state *new_state_ptr = &state_table[new_state];
    
    if (old_state && old_state->on_exit) {
        old_state->on_exit(conn);
    }
    
    printf("[Transition] %s -> %s\n", 
           old_state ? old_state->name : "NONE",
           new_state_ptr->name);
    
    conn->current_state = new_state_ptr;
    conn->state_type = new_state;
    conn->transitions++;
    
    if (new_state_ptr->on_enter) {
        new_state_ptr->on_enter(conn);
    }
}

struct connection *connection_create(const char *name, 
                                     const char *host, int port)
{
    struct connection *conn = malloc(sizeof(struct connection));
    if (!conn) return NULL;
    
    conn->name = name;
    strncpy(conn->remote_host, host, sizeof(conn->remote_host) - 1);
    conn->port = port;
    conn->bytes_sent = 0;
    conn->bytes_received = 0;
    conn->error_count = 0;
    conn->transitions = 0;
    conn->current_state = NULL;
    
    /* Set initial state */
    connection_set_state(conn, STATE_DISCONNECTED);
    
    printf("[Connection] Created '%s' targeting %s:%d\n", 
           name, host, port);
    return conn;
}

void connection_destroy(struct connection *conn)
{
    printf("[Connection] Destroyed '%s'\n", conn->name);
    free(conn);
}

/**
 * connection_handle_event - Process event through state machine
 * @conn: Connection
 * @event: Event to process
 *
 * Dispatches event to current state's handler.
 */
void connection_handle_event(struct connection *conn, enum event_type event)
{
    printf("\n[Event] %s received in state %s\n",
           event_names[event], conn->current_state->name);
    
    /* Dispatch to state-specific handler */
    state_handler_t handler = conn->current_state->handlers[event];
    if (handler) {
        handler(conn, event);
    }
}

void connection_print_stats(struct connection *conn)
{
    printf("\n=== Connection Statistics ===\n");
    printf("Name: %s\n", conn->name);
    printf("Current State: %s\n", conn->current_state->name);
    printf("Bytes Sent: %d\n", conn->bytes_sent);
    printf("Bytes Received: %d\n", conn->bytes_received);
    printf("Error Count: %d\n", conn->error_count);
    printf("State Transitions: %d\n", conn->transitions);
    printf("=============================\n");
}

/* ============================================================
 * Main - Demonstrate State Pattern
 * ============================================================ */

int main(void)
{
    struct connection *conn;

    printf("=== State Pattern Demo (Connection State Machine) ===\n\n");

    /* Create connection */
    conn = connection_create("MyConnection", "server.example.com", 8080);

    /* Simulate connection lifecycle */
    printf("\n--- Attempting to connect ---\n");
    connection_handle_event(conn, EVENT_CONNECT);
    
    /* Simulate receiving connection ACK */
    printf("\n--- Simulating connection established ---\n");
    connection_handle_event(conn, EVENT_RECEIVE);
    
    /* Send some data */
    printf("\n--- Sending data ---\n");
    connection_handle_event(conn, EVENT_SEND);
    connection_handle_event(conn, EVENT_SEND);  /* Actually sends */
    
    /* Receive some data */
    printf("\n--- Receiving data ---\n");
    connection_handle_event(conn, EVENT_RECEIVE);
    connection_handle_event(conn, EVENT_RECEIVE);  /* Actually receives */
    
    /* More sending */
    printf("\n--- More communication ---\n");
    connection_handle_event(conn, EVENT_SEND);
    connection_handle_event(conn, EVENT_SEND);
    
    /* Simulate an error */
    printf("\n--- Simulating error ---\n");
    connection_handle_event(conn, EVENT_ERROR);
    
    /* Try to send in error state (should be ignored) */
    printf("\n--- Try to send in error state ---\n");
    connection_handle_event(conn, EVENT_SEND);
    
    /* Reset the connection */
    printf("\n--- Resetting connection ---\n");
    connection_handle_event(conn, EVENT_RESET);
    
    /* Reconnect */
    printf("\n--- Reconnecting ---\n");
    connection_handle_event(conn, EVENT_CONNECT);
    connection_handle_event(conn, EVENT_RECEIVE);
    
    /* Normal disconnect */
    printf("\n--- Normal disconnect ---\n");
    connection_handle_event(conn, EVENT_DISCONNECT);

    /* Print statistics */
    connection_print_stats(conn);

    /* Cleanup */
    connection_destroy(conn);

    printf("\n=== Demo Complete ===\n");
    return 0;
}
```

---

## 5. State Transition Diagram

```
+------------------------------------------------------------------+
|                   STATE TRANSITION DIAGRAM                        |
+------------------------------------------------------------------+
|                                                                   |
|                    +---------------+                              |
|                    | DISCONNECTED  |                              |
|                    +-------+-------+                              |
|                            |                                      |
|                            | EVENT_CONNECT                        |
|                            v                                      |
|                    +-------+-------+                              |
|          +---------|  CONNECTING   |---------+                    |
|          |         +-------+-------+         |                    |
|          |                 |                 |                    |
|          | TIMEOUT/ERROR   | RECEIVE_ACK     |                    |
|          |                 v                 |                    |
|          |         +-------+-------+         |                    |
|          |  +------|   CONNECTED   |<----+   |                    |
|          |  |      +---+-------+---+     |   |                    |
|          |  |          |       |         |   |                    |
|          |  | SEND     |       | RECV    |   |                    |
|          |  v          |       v         |   |                    |
|          | ++---+      |      +---++     |   |                    |
|          | |SEND|------+------|RECV|-----+   |                    |
|          | |ING |      |      |ING |         |                    |
|          | +--+-+      |      +-+--+         |                    |
|          |    |        |        |            |                    |
|          |    +---+----+----+---+            |                    |
|          |        |         |                |                    |
|          |        | ERROR   |                |                    |
|          |        v         |                |                    |
|          |    +---+---+     |                |                    |
|          +--->| ERROR |<----+                |                    |
|               +---+---+                      |                    |
|                   |                          |                    |
|                   | RESET                    |                    |
|                   |                          |                    |
|                   +-----> DISCONNECTED <-----+                    |
|                            (DISCONNECT)                           |
|                                                                   |
+------------------------------------------------------------------+
```

**中文说明：** 状态转换图展示了连接对象的生命周期。从DISCONNECTED开始，收到CONNECT事件进入CONNECTING状态。连接建立后进入CONNECTED状态，可以发送（SENDING）或接收（RECEIVING）数据。任何状态遇到错误可进入ERROR状态，通过RESET返回DISCONNECTED。DISCONNECT事件使连接返回初始状态。

---

## 6. Key Implementation Points

1. **State Table**: 2D array mapping [state][event] to handler
2. **State Structure**: Contains handlers and entry/exit actions
3. **Event Dispatch**: Look up and call handler from current state
4. **State Transitions**: Change state pointer, call exit/enter hooks
5. **Default Handler**: Handle unexpected events gracefully
6. **State-Specific Data**: Each state can maintain its own context

**中文说明：** 实现状态模式的关键点：状态表（二维数组映射[状态][事件]到处理函数）、状态结构（包含处理函数和入口/出口动作）、事件分发（从当前状态查找并调用处理函数）、状态转换（更改状态指针并调用exit/enter钩子）、默认处理（优雅处理意外事件）、状态特定数据（每个状态可维护自己的上下文）。

