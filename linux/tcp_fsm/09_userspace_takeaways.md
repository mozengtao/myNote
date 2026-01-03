# TCP FSM: User-Space Takeaways

## 1. When Enum + Switch is Sufficient

### 1.1 Criteria for Simple FSM

```
USE ENUM + SWITCH WHEN:
+------------------------------------------------------------------+
|                                                                   |
|  ✓ Number of states < 20                                         |
|  ✓ Transitions are deterministic                                 |
|  ✓ Actions are simple or local                                   |
|  ✓ No conditional transitions                                    |
|  ✓ Single event type per handler                                 |
|                                                                   |
+------------------------------------------------------------------+
```

### 1.2 User-Space Example: Simple Protocol

```c
typedef enum {
    STATE_IDLE,
    STATE_CONNECTING,
    STATE_CONNECTED,
    STATE_CLOSING,
    STATE_CLOSED,
    STATE_MAX
} connection_state_t;

typedef struct {
    connection_state_t state;
    int socket_fd;
    // ... other fields
} connection_t;

int handle_event(connection_t *conn, int event) {
    switch (conn->state) {
    case STATE_IDLE:
        if (event == EV_CONNECT_REQUEST) {
            // Start connection
            conn->state = STATE_CONNECTING;
            return start_connect(conn);
        }
        break;
        
    case STATE_CONNECTING:
        if (event == EV_CONNECTED) {
            conn->state = STATE_CONNECTED;
            return 0;
        }
        if (event == EV_ERROR) {
            conn->state = STATE_CLOSED;
            return -1;
        }
        break;
        
    case STATE_CONNECTED:
        if (event == EV_CLOSE_REQUEST) {
            conn->state = STATE_CLOSING;
            return start_close(conn);
        }
        if (event == EV_PEER_CLOSED) {
            conn->state = STATE_CLOSED;
            return 0;
        }
        break;
        
    case STATE_CLOSING:
        if (event == EV_CLOSED) {
            conn->state = STATE_CLOSED;
            return 0;
        }
        break;
        
    case STATE_CLOSED:
        // Ignore all events
        break;
    }
    
    return 0;
}
```

### 1.3 Benefits

```c
// BENEFIT 1: Compiler optimization
// Compiler generates jump table for O(1) dispatch

// BENEFIT 2: Exhaustiveness checking (with -Wswitch)
// warning: enumeration value 'STATE_CONNECTING' not handled

// BENEFIT 3: Clear structure
// Each case shows exactly what happens in that state

// BENEFIT 4: Easy debugging
printf("Current state: %d\n", conn->state);
```

中文说明：
- 状态数<20、转换确定性高时使用Enum+Switch
- 编译器优化为跳转表
- -Wswitch提供穷尽性检查

---

## 2. When Flags Should Complement FSM State

### 2.1 Signs You Need Flags

```
ADD FLAGS WHEN:
+------------------------------------------------------------------+
|                                                                   |
|  ✓ State explosion imminent (>50 potential states)              |
|  ✓ Multiple orthogonal concerns                                  |
|  ✓ Half-states needed (e.g., half-closed connections)           |
|  ✓ Behavior modifiers independent of core state                 |
|  ✓ Same state with different capabilities                       |
|                                                                   |
+------------------------------------------------------------------+
```

### 2.2 User-Space Example: Connection with Flags

```c
// Instead of: STATE_CONNECTED_CAN_SEND_CAN_RECV
//             STATE_CONNECTED_CAN_SEND_CANT_RECV
//             STATE_CONNECTED_CANT_SEND_CAN_RECV
//             STATE_CONNECTED_CANT_SEND_CANT_RECV

typedef enum {
    STATE_IDLE,
    STATE_CONNECTING,
    STATE_CONNECTED,
    STATE_CLOSING,
    STATE_CLOSED,
} connection_state_t;

// Orthogonal flags
#define FLAG_CAN_SEND   (1 << 0)
#define FLAG_CAN_RECV   (1 << 1)
#define FLAG_ENCRYPTED  (1 << 2)
#define FLAG_COMPRESSED (1 << 3)

typedef struct {
    connection_state_t state;
    unsigned int flags;
    int socket_fd;
} connection_t;

// Check state + flags independently
int can_send(connection_t *conn) {
    return conn->state == STATE_CONNECTED &&
           (conn->flags & FLAG_CAN_SEND);
}

int can_recv(connection_t *conn) {
    return conn->state == STATE_CONNECTED &&
           (conn->flags & FLAG_CAN_RECV);
}

// Handle events with flag awareness
void handle_shutdown_read(connection_t *conn) {
    conn->flags &= ~FLAG_CAN_RECV;  // Clear receive flag
    // State remains STATE_CONNECTED
}
```

### 2.3 TCP-Inspired Sub-FSM

```c
// Main connection state
typedef enum {
    CONN_IDLE,
    CONN_ACTIVE,
    CONN_CLOSING,
    CONN_CLOSED,
} conn_state_t;

// Orthogonal congestion/flow control state
typedef enum {
    FLOW_NORMAL,
    FLOW_SLOW_START,
    FLOW_CONGESTION,
    FLOW_RECOVERY,
} flow_state_t;

typedef struct {
    conn_state_t conn_state;      // Primary FSM
    flow_state_t flow_state;      // Secondary FSM
    unsigned int flags;           // Boolean modifiers
} connection_t;

// Each dimension can transition independently
void on_packet_loss(connection_t *conn) {
    // Only affects flow state, not connection state
    conn->flow_state = FLOW_CONGESTION;
}
```

中文说明：
- 正交关注点（如发送/接收能力）应使用标志
- 避免状态爆炸：4个状态+4个标志 = 8个值，而非64个显式状态
- 可以有多个独立的子FSM

---

## 3. When to Split FSM Logic Across Modules

### 3.1 Signs You Should Split

```
SPLIT FSM LOGIC WHEN:
+------------------------------------------------------------------+
|                                                                   |
|  ✓ Handler code > 500 lines                                      |
|  ✓ State-specific logic is complex                               |
|  ✓ Different states have different dependencies                  |
|  ✓ Testing individual states separately is valuable             |
|  ✓ Multiple developers work on different states                  |
|                                                                   |
+------------------------------------------------------------------+
```

### 3.2 User-Space Example: Modular FSM

```c
// fsm_core.h - Core definitions
typedef enum {
    STATE_IDLE,
    STATE_HANDSHAKE,
    STATE_ACTIVE,
    STATE_CLOSING,
} state_t;

typedef struct connection connection_t;

// fsm_core.c - Central dispatch
int fsm_handle_event(connection_t *conn, int event) {
    switch (conn->state) {
    case STATE_IDLE:
        return fsm_idle_handle(conn, event);
    case STATE_HANDSHAKE:
        return fsm_handshake_handle(conn, event);
    case STATE_ACTIVE:
        return fsm_active_handle(conn, event);
    case STATE_CLOSING:
        return fsm_closing_handle(conn, event);
    }
    return -1;
}

// fsm_idle.c - Idle state handler
int fsm_idle_handle(connection_t *conn, int event) {
    if (event == EV_CONNECT) {
        // Complex handshake initiation logic
        // ...
        fsm_set_state(conn, STATE_HANDSHAKE);
        return 0;
    }
    return -1;
}

// fsm_active.c - Active state handler
int fsm_active_handle(connection_t *conn, int event) {
    switch (event) {
    case EV_DATA:
        // Complex data handling
        return handle_data(conn);
    case EV_CLOSE:
        fsm_set_state(conn, STATE_CLOSING);
        return start_graceful_close(conn);
    }
    return -1;
}

// fsm_set_state.c - Centralized state transitions
void fsm_set_state(connection_t *conn, state_t new_state) {
    state_t old_state = conn->state;
    
    // Entry actions
    switch (new_state) {
    case STATE_ACTIVE:
        init_active_state_resources(conn);
        break;
    case STATE_CLOSING:
        start_close_timer(conn);
        break;
    }
    
    conn->state = new_state;
    
    // Exit actions / cleanup
    switch (old_state) {
    case STATE_HANDSHAKE:
        cleanup_handshake_state(conn);
        break;
    }
    
    // Notify observers
    notify_state_change(conn, old_state, new_state);
}
```

### 3.3 Module Structure

```
project/
├── fsm/
│   ├── fsm_core.h        # State enum, connection struct
│   ├── fsm_core.c        # Central dispatch
│   ├── fsm_set_state.c   # State transition logic
│   ├── fsm_idle.c        # Idle state handling
│   ├── fsm_handshake.c   # Handshake state handling
│   ├── fsm_active.c      # Active state handling
│   └── fsm_closing.c     # Closing state handling
├── tests/
│   ├── test_fsm_idle.c
│   ├── test_fsm_handshake.c
│   └── test_fsm_active.c
└── Makefile
```

中文说明：
- 每个状态处理器>500行时考虑拆分
- 中央调度器只做分发，具体逻辑在独立模块
- `fsm_set_state()`集中处理入口/出口动作

---

## 4. Event-Driven Architecture Pattern

### 4.1 Structure

```c
// Event queue
typedef struct {
    int type;
    void *data;
    size_t data_len;
} event_t;

typedef struct {
    event_t *events;
    int head, tail;
    int capacity;
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
} event_queue_t;

// FSM with event queue
typedef struct {
    state_t state;
    event_queue_t events;
    pthread_t worker;
} fsm_t;

// Worker thread
void *fsm_worker(void *arg) {
    fsm_t *fsm = (fsm_t *)arg;
    
    while (1) {
        event_t event = event_queue_pop(&fsm->events);  // Blocking
        
        // Lock state during transition
        pthread_mutex_lock(&fsm->state_lock);
        handle_event(fsm, &event);
        pthread_mutex_unlock(&fsm->state_lock);
    }
    return NULL;
}

// Post event from any thread
void fsm_post_event(fsm_t *fsm, int type, void *data, size_t len) {
    event_t event = {type, data, len};
    event_queue_push(&fsm->events, event);  // Thread-safe
}
```

### 4.2 Benefits

```
+------------------------------------------------------------------+
|                    EVENT-DRIVEN BENEFITS                          |
+------------------------------------------------------------------+
|                                                                   |
|  ✓ Decouples event sources from processing                      |
|  ✓ Natural concurrency model                                     |
|  ✓ Ordered processing of events                                  |
|  ✓ Testable (inject events directly)                            |
|  ✓ Scalable (multiple FSMs, one event loop)                     |
|                                                                   |
+------------------------------------------------------------------+
```

---

## 5. Error Handling Patterns

### 5.1 TCP-Inspired Error State

```c
typedef struct {
    state_t state;
    int error_code;        // Last error (like sk->sk_err)
    error_callback_t on_error;  // Error notification
} connection_t;

void set_error(connection_t *conn, int error) {
    conn->error_code = error;
    if (conn->on_error) {
        conn->on_error(conn, error);  // Like sk->sk_error_report()
    }
}

int get_and_clear_error(connection_t *conn) {
    int err = conn->error_code;
    conn->error_code = 0;
    return err;  // Like sock_error()
}

// In state handlers
int fsm_active_handle(connection_t *conn, int event) {
    if (event == EV_PROTOCOL_ERROR) {
        set_error(conn, ERR_PROTOCOL);
        fsm_set_state(conn, STATE_CLOSING);
        return -1;
    }
    // ...
}
```

### 5.2 Timeout Handling

```c
// TCP-inspired timeout handling
typedef struct {
    state_t state;
    struct timeval last_activity;
    int timeout_ms;
    int timeout_count;
} connection_t;

void on_timeout(connection_t *conn) {
    conn->timeout_count++;
    
    if (conn->timeout_count >= MAX_RETRIES) {
        set_error(conn, ERR_TIMEOUT);
        fsm_set_state(conn, STATE_CLOSED);
        return;
    }
    
    // Retry action based on state
    switch (conn->state) {
    case STATE_CONNECTING:
        retry_connect(conn);
        break;
    case STATE_CLOSING:
        retry_close(conn);
        break;
    }
    
    // Exponential backoff
    conn->timeout_ms *= 2;
    if (conn->timeout_ms > MAX_TIMEOUT_MS)
        conn->timeout_ms = MAX_TIMEOUT_MS;
        
    reset_timer(conn, conn->timeout_ms);
}
```

---

## 6. Complete User-Space FSM Template

```c
/* fsm.h */
#ifndef FSM_H
#define FSM_H

#include <pthread.h>

/* State enumeration */
typedef enum {
    FSM_STATE_INIT,
    FSM_STATE_ACTIVE,
    FSM_STATE_CLOSING,
    FSM_STATE_CLOSED,
    FSM_STATE_MAX
} fsm_state_t;

/* Event types */
typedef enum {
    FSM_EV_START,
    FSM_EV_DATA,
    FSM_EV_CLOSE,
    FSM_EV_ERROR,
    FSM_EV_TIMEOUT,
    FSM_EV_MAX
} fsm_event_t;

/* Flags (orthogonal to state) */
#define FSM_FLAG_CAN_SEND    (1 << 0)
#define FSM_FLAG_CAN_RECV    (1 << 1)
#define FSM_FLAG_ERROR       (1 << 2)

/* FSM context */
typedef struct {
    /* State */
    fsm_state_t state;
    unsigned int flags;
    int error_code;
    
    /* Synchronization */
    pthread_mutex_t lock;
    
    /* Callbacks */
    void (*on_state_change)(void *ctx, fsm_state_t old, fsm_state_t new);
    void (*on_error)(void *ctx, int error);
    void *callback_ctx;
    
    /* Application data */
    void *app_data;
} fsm_t;

/* API */
fsm_t *fsm_create(void *app_data);
void fsm_destroy(fsm_t *fsm);
int fsm_handle_event(fsm_t *fsm, fsm_event_t event, void *data);
fsm_state_t fsm_get_state(fsm_t *fsm);

#endif /* FSM_H */
```

```c
/* fsm.c */
#include "fsm.h"
#include <stdlib.h>

/* State names for debugging */
static const char *state_names[] = {
    "INIT", "ACTIVE", "CLOSING", "CLOSED"
};

/* Internal state setter */
static void fsm_set_state(fsm_t *fsm, fsm_state_t new_state) {
    fsm_state_t old_state = fsm->state;
    
    /* Exit actions */
    switch (old_state) {
    case FSM_STATE_ACTIVE:
        /* Cleanup active state resources */
        break;
    default:
        break;
    }
    
    fsm->state = new_state;
    
    /* Entry actions */
    switch (new_state) {
    case FSM_STATE_ACTIVE:
        fsm->flags |= (FSM_FLAG_CAN_SEND | FSM_FLAG_CAN_RECV);
        break;
    case FSM_STATE_CLOSING:
        fsm->flags &= ~FSM_FLAG_CAN_SEND;
        break;
    case FSM_STATE_CLOSED:
        fsm->flags = 0;
        break;
    default:
        break;
    }
    
    /* Notify */
    if (fsm->on_state_change) {
        fsm->on_state_change(fsm->callback_ctx, old_state, new_state);
    }
}

/* Event handler dispatch */
int fsm_handle_event(fsm_t *fsm, fsm_event_t event, void *data) {
    int result = 0;
    
    pthread_mutex_lock(&fsm->lock);
    
    switch (fsm->state) {
    case FSM_STATE_INIT:
        if (event == FSM_EV_START) {
            fsm_set_state(fsm, FSM_STATE_ACTIVE);
        }
        break;
        
    case FSM_STATE_ACTIVE:
        switch (event) {
        case FSM_EV_DATA:
            /* Handle data */
            break;
        case FSM_EV_CLOSE:
            fsm_set_state(fsm, FSM_STATE_CLOSING);
            break;
        case FSM_EV_ERROR:
            fsm->error_code = *(int *)data;
            fsm->flags |= FSM_FLAG_ERROR;
            if (fsm->on_error)
                fsm->on_error(fsm->callback_ctx, fsm->error_code);
            fsm_set_state(fsm, FSM_STATE_CLOSED);
            result = -1;
            break;
        default:
            break;
        }
        break;
        
    case FSM_STATE_CLOSING:
        if (event == FSM_EV_CLOSE || event == FSM_EV_TIMEOUT) {
            fsm_set_state(fsm, FSM_STATE_CLOSED);
        }
        break;
        
    case FSM_STATE_CLOSED:
        /* Ignore all events */
        break;
        
    default:
        break;
    }
    
    pthread_mutex_unlock(&fsm->lock);
    return result;
}

/* Constructor */
fsm_t *fsm_create(void *app_data) {
    fsm_t *fsm = calloc(1, sizeof(fsm_t));
    if (!fsm) return NULL;
    
    fsm->state = FSM_STATE_INIT;
    fsm->app_data = app_data;
    pthread_mutex_init(&fsm->lock, NULL);
    
    return fsm;
}

/* Destructor */
void fsm_destroy(fsm_t *fsm) {
    if (!fsm) return;
    pthread_mutex_destroy(&fsm->lock);
    free(fsm);
}
```

---

## 7. Summary: User-Space Rules

```
+------------------------------------------------------------------+
|                    USER-SPACE FSM RULES                           |
+------------------------------------------------------------------+
|                                                                   |
|  RULE 1: Start with enum + switch                                |
|    - Simple, readable, compiler-optimized                        |
|    - Works for most protocols (< 20 states)                     |
|                                                                   |
|  RULE 2: Add flags for orthogonal concerns                       |
|    - Avoid state explosion                                       |
|    - Keep state enum focused on primary flow                     |
|                                                                   |
|  RULE 3: Split when handlers get complex                         |
|    - One file per state if > 500 lines each                     |
|    - Central dispatch + distributed logic                        |
|    - Centralized state setter for side effects                   |
|                                                                   |
|  RULE 4: Use event-driven for async I/O                          |
|    - Event queue + worker thread                                 |
|    - Lock during state transitions                               |
|    - Order preservation via queue                                |
|                                                                   |
|  RULE 5: TCP-inspired error handling                             |
|    - Error code field + callback                                 |
|    - Exponential backoff for retries                             |
|    - Maximum retry limits                                        |
|                                                                   |
+------------------------------------------------------------------+
```

中文总结：
1. **从Enum+Switch开始**：简单、可读、编译器优化
2. **正交关注点用标志**：避免状态爆炸
3. **复杂时拆分模块**：每状态一个文件
4. **异步I/O用事件驱动**：事件队列+工作线程
5. **TCP式错误处理**：错误码+回调+指数退避
