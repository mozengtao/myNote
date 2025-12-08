# State Machine Pattern (状态机模式)

## 1. Core Concept and Use Cases

### Core Concept
Allow an object to **alter its behavior when its internal state changes**. The object will appear to change its class. State transitions are triggered by events.

### Typical Use Cases
- Protocol state management (TCP, DOCSIS)
- Device lifecycle management
- Game character states
- Workflow/business process control
- Connection state management

---

## 2. Architecture Diagram

```
+--------------------------------------------------------------------------------------------------+
|                                    State Machine Pattern                                          |
+--------------------------------------------------------------------------------------------------+

                              +---------------------------+
                              |       Context             |
                              | (State Machine Owner)     |
                              +---------------------------+
                              | - current_state           |
                              | - state_handlers[]        |
                              +---------------------------+
                              | + handle_event(event)     |
                              | + transition_to(state)    |
                              +-------------+-------------+
                                            |
                      +---------------------+---------------------+
                      |                     |                     |
                      v                     v                     v
               +-------------+       +-------------+       +-------------+
               |   State A   |       |   State B   |       |   State C   |
               |   (INIT)    |       | (CONNECTED) |       |  (ERROR)    |
               +-------------+       +-------------+       +-------------+
               | on_enter()  |       | on_enter()  |       | on_enter()  |
               | on_exit()   |       | on_exit()   |       | on_exit()   |
               | handle()    |       | handle()    |       | handle()    |
               +------+------+       +------+------+       +------+------+
                      |                     |                     |
                      |    EVENT_CONNECT    |    EVENT_ERROR     |
                      +-------------------->+-------------------->+
                      |                     |                     |
                      |<--------------------+<--------------------+
                      |   EVENT_DISCONNECT  |    EVENT_RESET     |


    State Transition Table:
    +---------------+------------------+------------------+
    | Current State | Event            | Next State       |
    +---------------+------------------+------------------+
    | INIT          | EVENT_CONNECT    | CONNECTING       |
    | CONNECTING    | EVENT_SUCCESS    | CONNECTED        |
    | CONNECTING    | EVENT_TIMEOUT    | ERROR            |
    | CONNECTED     | EVENT_DISCONNECT | INIT             |
    | CONNECTED     | EVENT_ERROR      | ERROR            |
    | ERROR         | EVENT_RESET      | INIT             |
    +---------------+------------------+------------------+
```

**中文说明：**

状态机模式的核心流程：

1. **上下文（Context）**：
   - 持有当前状态 `current_state`
   - 维护状态处理器数组 `state_handlers[]`
   - 处理事件并执行状态转换

2. **状态（State）**：
   - 每个状态有独立的行为实现
   - `on_enter()`：进入状态时执行
   - `on_exit()`：离开状态时执行
   - `handle()`：处理该状态下的事件

3. **状态转换**：
   - 由事件触发
   - 根据当前状态和事件决定下一状态
   - 执行退出旧状态、进入新状态的回调

---

## 3. Code Skeleton

```c
/* State enumeration */
typedef enum {
    STATE_INIT,
    STATE_CONNECTING,
    STATE_CONNECTED,
    STATE_ERROR,
    STATE_MAX
} State;

/* Event enumeration */
typedef enum {
    EVENT_CONNECT,
    EVENT_SUCCESS,
    EVENT_DISCONNECT,
    EVENT_ERROR,
    EVENT_RESET,
    EVENT_MAX
} Event;

/* State handler function type */
typedef State (*state_handler_fn)(void* ctx, Event event);

/* State machine context */
typedef struct {
    State current_state;
    state_handler_fn handlers[STATE_MAX];
    void* user_data;
} StateMachine;

/* State machine operations */
void sm_init(StateMachine* sm);
void sm_handle_event(StateMachine* sm, Event event);
void sm_transition_to(StateMachine* sm, State new_state);
```

**中文说明：**

代码骨架包含：
- `State`：状态枚举
- `Event`：事件枚举
- `state_handler_fn`：状态处理函数类型
- `StateMachine`：状态机上下文
- 核心操作：`init`、`handle_event`、`transition_to`

---

## 4. Complete Example Code

```c
/*
 * State Machine Pattern - Connection Manager Example
 * 
 * This example demonstrates a connection state machine
 * managing TCP-like connection states.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ============================================
 * State and Event Definitions
 * ============================================ */
typedef enum {
    STATE_IDLE,             /* Initial state, not connected */
    STATE_CONNECTING,       /* Connection in progress */
    STATE_CONNECTED,        /* Successfully connected */
    STATE_DISCONNECTING,    /* Disconnection in progress */
    STATE_ERROR,            /* Error state */
    STATE_MAX
} ConnectionState;

typedef enum {
    EVENT_CONNECT,          /* Request to connect */
    EVENT_CONNECT_SUCCESS,  /* Connection succeeded */
    EVENT_CONNECT_FAILED,   /* Connection failed */
    EVENT_DISCONNECT,       /* Request to disconnect */
    EVENT_DISCONNECT_DONE,  /* Disconnection complete */
    EVENT_DATA_RECEIVED,    /* Data received */
    EVENT_ERROR,            /* Error occurred */
    EVENT_RESET,            /* Reset to initial state */
    EVENT_TIMEOUT,          /* Operation timeout */
    EVENT_MAX
} ConnectionEvent;

/* State and event name strings for logging */
static const char* state_names[] = {
    "IDLE", "CONNECTING", "CONNECTED", "DISCONNECTING", "ERROR"
};

static const char* event_names[] = {
    "CONNECT", "CONNECT_SUCCESS", "CONNECT_FAILED", "DISCONNECT",
    "DISCONNECT_DONE", "DATA_RECEIVED", "ERROR", "RESET", "TIMEOUT"
};

/* ============================================
 * State Machine Context
 * ============================================ */
typedef struct StateMachine StateMachine;

/* State handler function type */
typedef ConnectionState (*state_handler_fn)(StateMachine* sm, ConnectionEvent event);

/* State callbacks */
typedef void (*state_callback_fn)(StateMachine* sm);

/* Connection data */
typedef struct {
    char remote_host[64];
    int  remote_port;
    int  retry_count;
    int  max_retries;
    int  bytes_sent;
    int  bytes_received;
} ConnectionData;

struct StateMachine {
    ConnectionState current_state;              /* Current state */
    ConnectionState previous_state;             /* Previous state for logging */
    
    state_handler_fn handlers[STATE_MAX];       /* State handlers array */
    state_callback_fn on_enter[STATE_MAX];      /* Entry callbacks */
    state_callback_fn on_exit[STATE_MAX];       /* Exit callbacks */
    
    ConnectionData data;                        /* Connection-specific data */
    int transition_count;                       /* Statistics */
};

/* ============================================
 * State Entry/Exit Callbacks
 * ============================================ */
static void on_enter_idle(StateMachine* sm)
{
    printf("    [ENTER] IDLE: Resetting connection data\n");
    sm->data.retry_count = 0;
    sm->data.bytes_sent = 0;
    sm->data.bytes_received = 0;
}

static void on_exit_idle(StateMachine* sm)
{
    printf("    [EXIT] IDLE: Preparing for connection\n");
}

static void on_enter_connecting(StateMachine* sm)
{
    sm->data.retry_count++;
    printf("    [ENTER] CONNECTING: Attempt #%d to %s:%d\n",
           sm->data.retry_count, sm->data.remote_host, sm->data.remote_port);
}

static void on_exit_connecting(StateMachine* sm)
{
    printf("    [EXIT] CONNECTING: Connection attempt finished\n");
}

static void on_enter_connected(StateMachine* sm)
{
    printf("    [ENTER] CONNECTED: Session established!\n");
    sm->data.retry_count = 0;  /* Reset retry count on success */
}

static void on_exit_connected(StateMachine* sm)
{
    printf("    [EXIT] CONNECTED: Total sent=%d, received=%d bytes\n",
           sm->data.bytes_sent, sm->data.bytes_received);
}

static void on_enter_disconnecting(StateMachine* sm)
{
    printf("    [ENTER] DISCONNECTING: Graceful shutdown in progress\n");
}

static void on_exit_disconnecting(StateMachine* sm)
{
    printf("    [EXIT] DISCONNECTING: Cleanup complete\n");
}

static void on_enter_error(StateMachine* sm)
{
    printf("    [ENTER] ERROR: Connection error occurred!\n");
}

static void on_exit_error(StateMachine* sm)
{
    printf("    [EXIT] ERROR: Recovering from error\n");
}

/* ============================================
 * State Handlers - Core State Machine Logic
 * ============================================ */

/* IDLE state handler */
static ConnectionState handle_idle(StateMachine* sm, ConnectionEvent event)
{
    switch (event) {
        case EVENT_CONNECT:
            printf("    [IDLE] Initiating connection...\n");
            return STATE_CONNECTING;            /* Transition to CONNECTING */
            
        default:
            printf("    [IDLE] Ignoring event: %s\n", event_names[event]);
            return STATE_IDLE;                  /* Stay in IDLE */
    }
}

/* CONNECTING state handler */
static ConnectionState handle_connecting(StateMachine* sm, ConnectionEvent event)
{
    switch (event) {
        case EVENT_CONNECT_SUCCESS:
            printf("    [CONNECTING] Connection successful!\n");
            return STATE_CONNECTED;             /* Transition to CONNECTED */
            
        case EVENT_CONNECT_FAILED:
        case EVENT_TIMEOUT:
            if (sm->data.retry_count < sm->data.max_retries) {
                printf("    [CONNECTING] Failed, will retry (%d/%d)\n",
                       sm->data.retry_count, sm->data.max_retries);
                return STATE_CONNECTING;        /* Stay and retry */
            } else {
                printf("    [CONNECTING] Max retries exceeded\n");
                return STATE_ERROR;             /* Transition to ERROR */
            }
            
        case EVENT_DISCONNECT:
            printf("    [CONNECTING] Connection cancelled\n");
            return STATE_IDLE;                  /* Back to IDLE */
            
        default:
            printf("    [CONNECTING] Ignoring event: %s\n", event_names[event]);
            return STATE_CONNECTING;
    }
}

/* CONNECTED state handler */
static ConnectionState handle_connected(StateMachine* sm, ConnectionEvent event)
{
    switch (event) {
        case EVENT_DATA_RECEIVED:
            sm->data.bytes_received += 100;     /* Simulate receiving data */
            printf("    [CONNECTED] Data received (total: %d bytes)\n",
                   sm->data.bytes_received);
            return STATE_CONNECTED;             /* Stay connected */
            
        case EVENT_DISCONNECT:
            printf("    [CONNECTED] Disconnect requested\n");
            return STATE_DISCONNECTING;         /* Transition to DISCONNECTING */
            
        case EVENT_ERROR:
            printf("    [CONNECTED] Error occurred!\n");
            return STATE_ERROR;                 /* Transition to ERROR */
            
        default:
            printf("    [CONNECTED] Ignoring event: %s\n", event_names[event]);
            return STATE_CONNECTED;
    }
}

/* DISCONNECTING state handler */
static ConnectionState handle_disconnecting(StateMachine* sm, ConnectionEvent event)
{
    switch (event) {
        case EVENT_DISCONNECT_DONE:
            printf("    [DISCONNECTING] Disconnection complete\n");
            return STATE_IDLE;                  /* Back to IDLE */
            
        case EVENT_TIMEOUT:
            printf("    [DISCONNECTING] Timeout, forcing close\n");
            return STATE_IDLE;                  /* Force back to IDLE */
            
        default:
            printf("    [DISCONNECTING] Ignoring event: %s\n", event_names[event]);
            return STATE_DISCONNECTING;
    }
}

/* ERROR state handler */
static ConnectionState handle_error(StateMachine* sm, ConnectionEvent event)
{
    switch (event) {
        case EVENT_RESET:
            printf("    [ERROR] Resetting state machine\n");
            return STATE_IDLE;                  /* Reset to IDLE */
            
        case EVENT_CONNECT:
            printf("    [ERROR] Attempting reconnection\n");
            return STATE_CONNECTING;            /* Try to reconnect */
            
        default:
            printf("    [ERROR] Ignoring event: %s\n", event_names[event]);
            return STATE_ERROR;
    }
}

/* ============================================
 * State Machine Core Functions
 * ============================================ */

/* Initialize state machine */
void sm_init(StateMachine* sm, const char* host, int port)
{
    memset(sm, 0, sizeof(StateMachine));
    
    /* Set initial state */
    sm->current_state = STATE_IDLE;
    sm->previous_state = STATE_IDLE;
    
    /* Register state handlers */
    sm->handlers[STATE_IDLE] = handle_idle;
    sm->handlers[STATE_CONNECTING] = handle_connecting;
    sm->handlers[STATE_CONNECTED] = handle_connected;
    sm->handlers[STATE_DISCONNECTING] = handle_disconnecting;
    sm->handlers[STATE_ERROR] = handle_error;
    
    /* Register entry callbacks */
    sm->on_enter[STATE_IDLE] = on_enter_idle;
    sm->on_enter[STATE_CONNECTING] = on_enter_connecting;
    sm->on_enter[STATE_CONNECTED] = on_enter_connected;
    sm->on_enter[STATE_DISCONNECTING] = on_enter_disconnecting;
    sm->on_enter[STATE_ERROR] = on_enter_error;
    
    /* Register exit callbacks */
    sm->on_exit[STATE_IDLE] = on_exit_idle;
    sm->on_exit[STATE_CONNECTING] = on_exit_connecting;
    sm->on_exit[STATE_CONNECTED] = on_exit_connected;
    sm->on_exit[STATE_DISCONNECTING] = on_exit_disconnecting;
    sm->on_exit[STATE_ERROR] = on_exit_error;
    
    /* Initialize connection data */
    strncpy(sm->data.remote_host, host, sizeof(sm->data.remote_host) - 1);
    sm->data.remote_port = port;
    sm->data.max_retries = 3;
    
    printf("[SM] Initialized for %s:%d\n", host, port);
}

/* Perform state transition */
static void sm_transition_to(StateMachine* sm, ConnectionState new_state)
{
    if (new_state == sm->current_state) {
        return;  /* No transition needed */
    }
    
    sm->transition_count++;
    sm->previous_state = sm->current_state;
    
    printf("[SM] Transition #%d: %s -> %s\n",
           sm->transition_count,
           state_names[sm->current_state],
           state_names[new_state]);
    
    /* Call exit callback for current state */
    if (sm->on_exit[sm->current_state] != NULL) {
        sm->on_exit[sm->current_state](sm);
    }
    
    /* Update state */
    sm->current_state = new_state;
    
    /* Call entry callback for new state */
    if (sm->on_enter[new_state] != NULL) {
        sm->on_enter[new_state](sm);
    }
}

/* Handle event - core of state machine */
void sm_handle_event(StateMachine* sm, ConnectionEvent event)
{
    ConnectionState next_state;
    
    printf("\n[SM] Event: %s (current state: %s)\n",
           event_names[event], state_names[sm->current_state]);
    
    /* Get handler for current state */
    state_handler_fn handler = sm->handlers[sm->current_state];
    
    if (handler == NULL) {
        printf("[SM] Error: No handler for state %s\n",
               state_names[sm->current_state]);
        return;
    }
    
    /* Call handler and get next state */
    next_state = handler(sm, event);
    
    /* Perform transition if state changed */
    sm_transition_to(sm, next_state);
}

/* Get current state */
ConnectionState sm_get_state(StateMachine* sm)
{
    return sm->current_state;
}

/* Print state machine status */
void sm_print_status(StateMachine* sm)
{
    printf("\n=== State Machine Status ===\n");
    printf("Current State: %s\n", state_names[sm->current_state]);
    printf("Previous State: %s\n", state_names[sm->previous_state]);
    printf("Transitions: %d\n", sm->transition_count);
    printf("Remote: %s:%d\n", sm->data.remote_host, sm->data.remote_port);
    printf("Retries: %d/%d\n", sm->data.retry_count, sm->data.max_retries);
    printf("Data: sent=%d, received=%d\n",
           sm->data.bytes_sent, sm->data.bytes_received);
    printf("============================\n\n");
}

/* ============================================
 * Main Function - Demonstration
 * ============================================ */
int main(void)
{
    StateMachine sm;
    
    printf("=== State Machine Pattern Demo ===\n\n");
    
    /* Initialize state machine */
    sm_init(&sm, "192.168.1.100", 8080);
    sm_print_status(&sm);
    
    /* Scenario 1: Successful connection */
    printf("--- Scenario 1: Successful Connection ---\n");
    sm_handle_event(&sm, EVENT_CONNECT);
    sm_handle_event(&sm, EVENT_CONNECT_SUCCESS);
    sm_handle_event(&sm, EVENT_DATA_RECEIVED);
    sm_handle_event(&sm, EVENT_DATA_RECEIVED);
    sm_handle_event(&sm, EVENT_DISCONNECT);
    sm_handle_event(&sm, EVENT_DISCONNECT_DONE);
    sm_print_status(&sm);
    
    /* Scenario 2: Connection failure with retry */
    printf("--- Scenario 2: Connection Failure with Retry ---\n");
    sm_handle_event(&sm, EVENT_CONNECT);
    sm_handle_event(&sm, EVENT_TIMEOUT);         /* Retry 1 */
    sm_handle_event(&sm, EVENT_CONNECT_FAILED);  /* Retry 2 */
    sm_handle_event(&sm, EVENT_TIMEOUT);         /* Retry 3 */
    sm_handle_event(&sm, EVENT_CONNECT_SUCCESS); /* Finally success */
    sm_print_status(&sm);
    
    /* Scenario 3: Error and recovery */
    printf("--- Scenario 3: Error and Recovery ---\n");
    sm_handle_event(&sm, EVENT_ERROR);
    sm_handle_event(&sm, EVENT_RESET);
    sm_print_status(&sm);
    
    printf("=== Demo Complete ===\n");
    return 0;
}
```

**中文说明：**

完整示例代码实现了一个连接状态机：

1. **状态定义**：
   - `IDLE`：初始空闲状态
   - `CONNECTING`：连接中
   - `CONNECTED`：已连接
   - `DISCONNECTING`：断开中
   - `ERROR`：错误状态

2. **事件定义**：
   - `EVENT_CONNECT`：连接请求
   - `EVENT_CONNECT_SUCCESS/FAILED`：连接结果
   - `EVENT_DISCONNECT`：断开请求
   - `EVENT_DATA_RECEIVED`：数据接收
   - `EVENT_ERROR/RESET/TIMEOUT`：异常事件

3. **核心机制**：
   - 每个状态有独立的处理器函数
   - 处理器根据事件返回下一状态
   - 状态转换时执行 `on_exit` 和 `on_enter` 回调

4. **演示场景**：
   - 成功连接流程
   - 连接失败重试流程
   - 错误恢复流程

