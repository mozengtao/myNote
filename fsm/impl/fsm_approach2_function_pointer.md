# FSM Approach 2: Function Pointer Table

## Topic: Connection State Machine

The same connection manager, implemented using per-state handler functions for maximum flexibility.

---

## WHY | Engineering Motivation

```
+====================================================================================+
|                        WHY FUNCTION POINTER FSM?                                   |
+====================================================================================+

    The Limitation of Simple Table-Driven FSM:
    +------------------------------------------------------------------------+
    |                                                                        |
    |   Transition table works great when:                                   |
    |   - Transitions are uniform (next_state + simple action)               |
    |   - Most cells have similar structure                                  |
    |                                                                        |
    |   But what if:                                                         |
    |   - Different states need DIFFERENT handling logic?                    |
    |   - Some transitions require complex guards/conditions?                |
    |   - Actions vary significantly between transitions?                    |
    |   - You want to unit-test each state independently?                    |
    |                                                                        |
    |   Example:                                                             |
    |   +---------------------------------------------------------------+   |
    |   | STATE_CONNECTING:                                             |   |
    |   |   - On CONNECTED_ACK: validate certificate, setup buffers,    |   |
    |   |                       initialize counters, log metrics        |   |
    |   |   - On TIMEOUT: check retry count, backoff, reschedule        |   |
    |   |   - On ERROR: classify error, report metrics, maybe retry     |   |
    |   |                                                               |   |
    |   | STATE_CONNECTED:                                              |   |
    |   |   - On DISCONNECT: flush buffers, notify peers, save state    |   |
    |   |   - On ERROR: attempt recovery before giving up               |   |
    |   +---------------------------------------------------------------+   |
    |                                                                        |
    |   Each state has UNIQUE logic that doesn't fit a simple table cell.    |
    |                                                                        |
    +------------------------------------------------------------------------+


    The Function Pointer Solution:
    +------------------------------------------------------------------------+
    |                                                                        |
    |   Instead of: table[state][event] = { next_state, simple_action }      |
    |                                                                        |
    |   Use:        handlers[state] = state_handler_function                 |
    |               OR                                                        |
    |               handlers[state][event] = specific_handler_function       |
    |                                                                        |
    |   Each handler function encapsulates ALL logic for that state/event:  |
    |   - Input validation                                                   |
    |   - Guard conditions                                                   |
    |   - Side effects                                                       |
    |   - State transition                                                   |
    |   - Error handling                                                     |
    |                                                                        |
    +------------------------------------------------------------------------+


    When to Use Function Pointer FSM:
    +------------------------------------------------------------------------+
    |                                                                        |
    |   ✓ Complex per-state logic that varies significantly                  |
    |   ✓ Need to unit-test each state handler independently                 |
    |   ✓ Guard conditions depend on complex runtime state                   |
    |   ✓ Want to swap handlers at runtime (plugin architecture)             |
    |   ✓ State handlers need different helper functions                     |
    |                                                                        |
    |   ✗ Simple FSM with uniform transitions                                |
    |   ✗ Very tight memory constraints (function pointers add overhead)     |
    |   ✗ Need to inspect all transitions at once (table is more visible)    |
    |                                                                        |
    +------------------------------------------------------------------------+
```

### 中文说明

**为什么需要函数指针FSM？**

简单表驱动FSM的局限性：
1. 当不同状态需要**截然不同**的处理逻辑时，表格单元无法承载
2. 当转换需要**复杂的条件判断**时，简单的 action 函数不够用
3. 当需要**独立测试**每个状态的处理逻辑时，分散的代码难以测试

函数指针方案将每个状态的处理逻辑封装在独立函数中：
- 每个 handler 函数可以有任意复杂的逻辑
- 可以独立编译和测试
- 可以在运行时替换（插件架构）

---

## HOW | Design Philosophy and Core Ideas

```
+====================================================================================+
|                       HOW FUNCTION POINTER FSM WORKS                               |
+====================================================================================+

    Two Variants of Function Pointer FSM:
    +------------------------------------------------------------------------+
    |                                                                        |
    |   Variant A: Per-State Handler                                         |
    |   +------------------------------+                                     |
    |   | handlers[STATE_CONNECTING]   | = state_connecting_handler()        |
    |   | handlers[STATE_CONNECTED]    | = state_connected_handler()         |
    |   | handlers[STATE_DISCONNECTING]| = state_disconnecting_handler()     |
    |   +------------------------------+                                     |
    |                                                                        |
    |   Each handler receives the event and decides what to do.              |
    |   Handler contains internal switch on event.                           |
    |                                                                        |
    |   Variant B: Per-State-Event Handler (Full Matrix)                     |
    |   +------------------------------+                                     |
    |   | handlers[STATE][EVENT]       | = specific_handler()                |
    |   +------------------------------+                                     |
    |                                                                        |
    |   Each cell has its own dedicated function.                            |
    |   More granular, but more functions to write.                          |
    |                                                                        |
    +------------------------------------------------------------------------+


    Variant A: Per-State Handler (This Example)
    +------------------------------------------------------------------------+
    |                                                                        |
    |   typedef int (*StateHandler)(FSM *fsm, Event event);                  |
    |                                                                        |
    |   StateHandler handlers[STATE_COUNT] = {                               |
    |       [STATE_CLOSED]        = handle_state_closed,                     |
    |       [STATE_CONNECTING]    = handle_state_connecting,                 |
    |       [STATE_CONNECTED]     = handle_state_connected,                  |
    |       [STATE_DISCONNECTING] = handle_state_disconnecting,              |
    |   };                                                                   |
    |                                                                        |
    |   Execution:                                                           |
    |   1. Get current state                                                 |
    |   2. Call handlers[current_state](fsm, event)                          |
    |   3. Handler internally switches on event                              |
    |   4. Handler updates fsm->state if transition occurs                   |
    |                                                                        |
    +------------------------------------------------------------------------+


    Handler Function Structure:
    +------------------------------------------------------------------------+
    |                                                                        |
    |   int handle_state_connecting(FSM *fsm, Event event)                   |
    |   {                                                                    |
    |       switch (event) {                                                 |
    |       case EVENT_CONNECTED_ACK:                                        |
    |           // Complex logic specific to this state+event                |
    |           validate_connection(fsm);                                    |
    |           setup_buffers(fsm);                                          |
    |           fsm->state = STATE_CONNECTED;                                |
    |           return 0;                                                    |
    |                                                                        |
    |       case EVENT_TIMEOUT:                                              |
    |           if (fsm->retry_count < MAX_RETRIES) {                        |
    |               // Guard condition: retry allowed                        |
    |               fsm->retry_count++;                                      |
    |               schedule_retry(fsm);                                     |
    |               return 0;  // Stay in CONNECTING                         |
    |           }                                                            |
    |           // Guard condition: max retries exceeded                     |
    |           fsm->state = STATE_CLOSED;                                   |
    |           return -1;                                                   |
    |                                                                        |
    |       case EVENT_ERROR:                                                |
    |           classify_and_handle_error(fsm);                              |
    |           fsm->state = STATE_CLOSED;                                   |
    |           return -1;                                                   |
    |                                                                        |
    |       default:                                                         |
    |           log_ignored_event(fsm, event);                               |
    |           return 0;                                                    |
    |       }                                                                |
    |   }                                                                    |
    |                                                                        |
    +------------------------------------------------------------------------+


    Comparison with Table-Driven:
    +------------------------------------------------------------------------+
    |                                                                        |
    |   Table-Driven:                    Function Pointer:                   |
    |   +----------------------+         +----------------------+            |
    |   | transition_table[][] |         | handlers[]           |            |
    |   |   -> next_state      |         |   -> handler_func    |            |
    |   |   -> simple_action   |         |      (full control)  |            |
    |   +----------------------+         +----------------------+            |
    |           |                                  |                         |
    |           v                                  v                         |
    |   Data-driven: table IS spec       Code-driven: funcs ARE spec         |
    |   Easy to inspect all at once      Harder to see big picture           |
    |   Limited per-cell logic           Unlimited per-cell logic            |
    |                                                                        |
    +------------------------------------------------------------------------+
```

### 中文说明

**函数指针FSM的两种变体**：

1. **Per-State Handler**（本例采用）：
   - 每个状态一个 handler 函数
   - Handler 内部用 switch 处理不同事件
   - 适合：状态数较多，每个状态的逻辑相对独立

2. **Per-State-Event Handler**：
   - 每个状态×事件组合一个 handler 函数
   - 更细粒度，但函数数量 = STATE_COUNT × EVENT_COUNT
   - 适合：需要极致的单元测试隔离

**Handler 函数的结构**：
- 接收 FSM 指针和事件
- 内部 switch 处理事件
- 执行任意复杂的逻辑（守卫条件、副作用、错误处理）
- 负责更新 `fsm->state`

**与表驱动的对比**：
- 表驱动：数据驱动，表格就是规格说明，易于检查
- 函数指针：代码驱动，函数就是规格说明，逻辑更灵活

---

## WHAT | Architecture and Concrete Forms

```
+====================================================================================+
|                     FUNCTION POINTER FSM ARCHITECTURE                              |
+====================================================================================+

    File Organization:
    +------------------------------------------------------------------------+
    |                                                                        |
    |   fsm_core.h                 fsm_handlers.c                            |
    |   +------------------+       +----------------------------------+      |
    |   | State enum       |       | handle_state_closed()            |      |
    |   | Event enum       |       | handle_state_connecting()        |      |
    |   | FSM struct       |       | handle_state_connected()         |      |
    |   | Handler type     |       | handle_state_disconnecting()     |      |
    |   | Public API       |       | (each can be in separate file)   |      |
    |   +------------------+       +----------------------------------+      |
    |                                                                        |
    |   fsm_engine.c                                                         |
    |   +----------------------------------+                                 |
    |   | handlers[] array                 |                                 |
    |   | fsm_init()                       |                                 |
    |   | fsm_dispatch()                   |                                 |
    |   +----------------------------------+                                 |
    |                                                                        |
    +------------------------------------------------------------------------+


    Control Flow:
    +------------------------------------------------------------------------+
    |                                                                        |
    |   fsm_dispatch(fsm, EVENT_TIMEOUT)                                     |
    |         |                                                              |
    |         v                                                              |
    |   +------------------+                                                 |
    |   | Get current state|  fsm->state = STATE_CONNECTING                  |
    |   +------------------+                                                 |
    |         |                                                              |
    |         v                                                              |
    |   +---------------------------+                                        |
    |   | handlers[STATE_CONNECTING]| = handle_state_connecting              |
    |   +---------------------------+                                        |
    |         |                                                              |
    |         v                                                              |
    |   +-----------------------------------+                                |
    |   | handle_state_connecting(fsm, ev) |                                 |
    |   +-----------------------------------+                                |
    |         |                                                              |
    |         v                                                              |
    |   +-----------------------------------+                                |
    |   | switch (event) {                 |                                 |
    |   |   case EVENT_TIMEOUT:            |                                 |
    |   |     if (retry < MAX) {           |  <-- Guard condition            |
    |   |       retry++;                   |  <-- Side effect                |
    |   |       schedule_retry();          |  <-- Side effect                |
    |   |       return 0; // stay          |  <-- No transition              |
    |   |     }                            |                                 |
    |   |     fsm->state = STATE_CLOSED;   |  <-- Transition                 |
    |   |     return -1;                   |                                 |
    |   | }                                |                                 |
    |   +-----------------------------------+                                |
    |                                                                        |
    +------------------------------------------------------------------------+


    Memory Layout:
    +------------------------------------------------------------------------+
    |                                                                        |
    |   handlers[] (static, const):          FSM Instance (runtime):         |
    |   +---------------------------+        +--------------------+          |
    |   | [0] = handle_closed       |        | state: CONNECTING  |          |
    |   | [1] = handle_connecting   |------->| retry_count: 2     |          |
    |   | [2] = handle_connected    |        | last_error: 0      |          |
    |   | [3] = handle_disconnecting|        | user_data: ptr     |          |
    |   +---------------------------+        +--------------------+          |
    |           ^                                     |                      |
    |           |                                     | (passed to handler)  |
    |           +-------------------------------------+                      |
    |                                                                        |
    +------------------------------------------------------------------------+


    Testing Strategy:
    +------------------------------------------------------------------------+
    |                                                                        |
    |   Unit Test Each Handler Independently:                                |
    |                                                                        |
    |   void test_connecting_timeout_retry(void)                             |
    |   {                                                                    |
    |       FSM fsm = { .state = STATE_CONNECTING, .retry_count = 1 };       |
    |       int result = handle_state_connecting(&fsm, EVENT_TIMEOUT);       |
    |       assert(result == 0);                                             |
    |       assert(fsm.state == STATE_CONNECTING);  // still connecting      |
    |       assert(fsm.retry_count == 2);           // retry incremented     |
    |   }                                                                    |
    |                                                                        |
    |   void test_connecting_timeout_max_retries(void)                       |
    |   {                                                                    |
    |       FSM fsm = { .state = STATE_CONNECTING, .retry_count = MAX };     |
    |       int result = handle_state_connecting(&fsm, EVENT_TIMEOUT);       |
    |       assert(result == -1);                                            |
    |       assert(fsm.state == STATE_CLOSED);      // gave up               |
    |   }                                                                    |
    |                                                                        |
    +------------------------------------------------------------------------+
```

### 中文说明

**文件组织**：
- `fsm_core.h`：类型定义、公共API
- `fsm_handlers.c`：各状态的 handler 函数（可拆分到多个文件）
- `fsm_engine.c`：handlers 数组、初始化、分发函数

**控制流**：
1. `fsm_dispatch()` 获取当前状态
2. 从 `handlers[]` 数组获取对应的 handler 函数指针
3. 调用 handler 函数，传入 FSM 和事件
4. Handler 内部 switch 处理事件，执行逻辑，更新状态

**测试策略**：
- 每个 handler 函数可以独立进行单元测试
- 构造特定的 FSM 状态，调用 handler，验证结果
- 无需完整的 FSM 引擎即可测试状态逻辑

---

## Complete C Example

```c
/******************************************************************************
 * FILE: connection_fsm_func_ptr.c
 * 
 * DESCRIPTION:
 *   Complete example of Function Pointer FSM for a connection state machine.
 *   Uses per-state handler functions for maximum flexibility.
 *   Compile: gcc -o fsm_funcptr connection_fsm_func_ptr.c
 *   Run: ./fsm_funcptr
 *
 * WHY FUNCTION POINTER:
 *   - Each state handler encapsulates ALL logic for that state
 *   - Complex guard conditions and multi-step actions are natural
 *   - Handlers can be unit-tested independently
 *   - Handlers can be swapped at runtime (plugin architecture)
 *
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/*============================================================================
 * PART 1: TYPE DEFINITIONS
 *============================================================================*/

#define MAX_RETRIES 3

typedef enum {
    STATE_CLOSED = 0,
    STATE_CONNECTING,
    STATE_CONNECTED,
    STATE_DISCONNECTING,
    STATE_COUNT
} ConnState;

typedef enum {
    EVENT_CONNECT = 0,
    EVENT_CONNECTED_ACK,
    EVENT_DISCONNECT,
    EVENT_DISCONNECTED_ACK,
    EVENT_TIMEOUT,
    EVENT_ERROR,
    EVENT_COUNT
} ConnEvent;

/* Forward declaration */
struct ConnFSM;

/* KEY DESIGN: Handler function signature
 * - fsm: FSM instance (for state access and modification)
 * - event: The incoming event to handle
 * - Returns: 0 = success, -1 = error
 * 
 * The handler is responsible for:
 *   1. Interpreting the event
 *   2. Checking guard conditions
 *   3. Executing side effects
 *   4. Updating fsm->state if transition occurs
 */
typedef int (*StateHandler)(struct ConnFSM *fsm, ConnEvent event);

/* FSM instance structure */
typedef struct ConnFSM {
    ConnState   state;
    int         retry_count;
    int         last_error;
    const char* name;
    void*       user_data;
} ConnFSM;

/*============================================================================
 * PART 2: NAME TABLES (for logging)
 *============================================================================*/

static const char* state_names[STATE_COUNT] = {
    [STATE_CLOSED]        = "CLOSED",
    [STATE_CONNECTING]    = "CONNECTING",
    [STATE_CONNECTED]     = "CONNECTED",
    [STATE_DISCONNECTING] = "DISCONNECTING"
};

static const char* event_names[EVENT_COUNT] = {
    [EVENT_CONNECT]         = "CONNECT",
    [EVENT_CONNECTED_ACK]   = "CONNECTED_ACK",
    [EVENT_DISCONNECT]      = "DISCONNECT",
    [EVENT_DISCONNECTED_ACK]= "DISCONNECTED_ACK",
    [EVENT_TIMEOUT]         = "TIMEOUT",
    [EVENT_ERROR]           = "ERROR"
};

/*============================================================================
 * PART 3: HELPER FUNCTIONS (Simulated Actions)
 *============================================================================*/

static void log_transition(ConnFSM *fsm, ConnState old_state, ConnState new_state,
                           const char *reason)
{
    printf("  [TRANSITION] %s: %s -> %s (%s)\n",
           fsm->name, state_names[old_state], state_names[new_state], reason);
}

static void log_ignored(ConnFSM *fsm, ConnEvent event)
{
    printf("  [IGNORED] %s: Event %s ignored in state %s\n",
           fsm->name, event_names[event], state_names[fsm->state]);
}

static void simulate_connect_start(ConnFSM *fsm)
{
    printf("  [ACTION] %s: Opening socket, initiating handshake...\n", fsm->name);
}

static void simulate_connection_established(ConnFSM *fsm)
{
    printf("  [ACTION] %s: Handshake complete, connection ready\n", fsm->name);
}

static void simulate_disconnect_start(ConnFSM *fsm)
{
    printf("  [ACTION] %s: Sending disconnect request, flushing buffers...\n", fsm->name);
}

static void simulate_cleanup(ConnFSM *fsm)
{
    printf("  [ACTION] %s: Releasing resources, resetting state\n", fsm->name);
    fsm->retry_count = 0;
    fsm->last_error = 0;
}

static void simulate_schedule_retry(ConnFSM *fsm)
{
    printf("  [ACTION] %s: Scheduling retry (attempt %d/%d)...\n",
           fsm->name, fsm->retry_count + 1, MAX_RETRIES);
}

/*============================================================================
 * PART 4: STATE HANDLER FUNCTIONS
 *
 * KEY DESIGN POINT:
 *   Each handler function encapsulates ALL logic for its state.
 *   The handler is a self-contained unit that can be:
 *   - Unit tested independently
 *   - Replaced at runtime
 *   - Moved to a separate file
 *
 *   Handler responsibility:
 *   1. Switch on event to determine action
 *   2. Execute guard conditions (e.g., retry_count check)
 *   3. Execute side effects (e.g., start timer)
 *   4. Update fsm->state if transition occurs
 *   5. Return success/error code
 *============================================================================*/

/*
 * Handler for STATE_CLOSED
 * 
 * Valid transitions:
 *   CONNECT -> STATE_CONNECTING (start connection attempt)
 * 
 * All other events are ignored (already closed)
 */
static int handle_state_closed(ConnFSM *fsm, ConnEvent event)
{
    /* KEY: Handler decides what to do based on event */
    switch (event) {
    case EVENT_CONNECT:
        /* Action: start connection */
        simulate_connect_start(fsm);
        fsm->retry_count = 0;
        
        /* Transition: CLOSED -> CONNECTING */
        log_transition(fsm, STATE_CLOSED, STATE_CONNECTING, "user requested connect");
        fsm->state = STATE_CONNECTING;
        return 0;

    case EVENT_CONNECTED_ACK:
    case EVENT_DISCONNECT:
    case EVENT_DISCONNECTED_ACK:
    case EVENT_TIMEOUT:
    case EVENT_ERROR:
        /* All ignored in CLOSED state */
        log_ignored(fsm, event);
        return 0;

    default:
        printf("  [ERROR] %s: Unknown event %d\n", fsm->name, event);
        return -1;
    }
}

/*
 * Handler for STATE_CONNECTING
 * 
 * This handler demonstrates COMPLEX LOGIC:
 *   - CONNECTED_ACK -> STATE_CONNECTED
 *   - DISCONNECT -> STATE_CLOSED (abort)
 *   - TIMEOUT -> retry if count < MAX, else STATE_CLOSED (guard condition!)
 *   - ERROR -> STATE_CLOSED
 */
static int handle_state_connecting(ConnFSM *fsm, ConnEvent event)
{
    switch (event) {
    case EVENT_CONNECTED_ACK:
        /* Connection succeeded */
        simulate_connection_established(fsm);
        
        log_transition(fsm, STATE_CONNECTING, STATE_CONNECTED, "handshake complete");
        fsm->state = STATE_CONNECTED;
        return 0;

    case EVENT_DISCONNECT:
        /* User aborted connection attempt */
        simulate_cleanup(fsm);
        
        log_transition(fsm, STATE_CONNECTING, STATE_CLOSED, "user aborted");
        fsm->state = STATE_CLOSED;
        return 0;

    case EVENT_TIMEOUT:
        /* KEY: Guard condition - check retry count */
        if (fsm->retry_count < MAX_RETRIES) {
            /* Guard passed: retry allowed */
            fsm->retry_count++;
            simulate_schedule_retry(fsm);
            printf("  [GUARD] Retry allowed: %d/%d\n", fsm->retry_count, MAX_RETRIES);
            /* No state change - stay in CONNECTING */
            return 0;
        }
        
        /* Guard failed: max retries exceeded */
        printf("  [GUARD] Max retries exceeded, giving up\n");
        fsm->last_error = -1;
        simulate_cleanup(fsm);
        
        log_transition(fsm, STATE_CONNECTING, STATE_CLOSED, "max retries exceeded");
        fsm->state = STATE_CLOSED;
        return -1;  /* Return error to indicate failure */

    case EVENT_ERROR:
        /* Connection error */
        fsm->last_error = -2;
        simulate_cleanup(fsm);
        
        log_transition(fsm, STATE_CONNECTING, STATE_CLOSED, "connection error");
        fsm->state = STATE_CLOSED;
        return -1;

    case EVENT_CONNECT:
    case EVENT_DISCONNECTED_ACK:
        log_ignored(fsm, event);
        return 0;

    default:
        printf("  [ERROR] %s: Unknown event %d\n", fsm->name, event);
        return -1;
    }
}

/*
 * Handler for STATE_CONNECTED
 * 
 * Valid transitions:
 *   DISCONNECT -> STATE_DISCONNECTING
 *   ERROR -> STATE_CLOSED (connection lost)
 */
static int handle_state_connected(ConnFSM *fsm, ConnEvent event)
{
    switch (event) {
    case EVENT_DISCONNECT:
        /* User requested disconnect */
        simulate_disconnect_start(fsm);
        
        log_transition(fsm, STATE_CONNECTED, STATE_DISCONNECTING, "user requested disconnect");
        fsm->state = STATE_DISCONNECTING;
        return 0;

    case EVENT_ERROR:
        /* Connection lost unexpectedly */
        printf("  [ACTION] %s: Connection lost unexpectedly!\n", fsm->name);
        fsm->last_error = -3;
        simulate_cleanup(fsm);
        
        log_transition(fsm, STATE_CONNECTED, STATE_CLOSED, "connection lost");
        fsm->state = STATE_CLOSED;
        return -1;

    case EVENT_CONNECT:
    case EVENT_CONNECTED_ACK:
    case EVENT_DISCONNECTED_ACK:
    case EVENT_TIMEOUT:
        log_ignored(fsm, event);
        return 0;

    default:
        printf("  [ERROR] %s: Unknown event %d\n", fsm->name, event);
        return -1;
    }
}

/*
 * Handler for STATE_DISCONNECTING
 * 
 * Valid transitions:
 *   DISCONNECTED_ACK -> STATE_CLOSED
 *   TIMEOUT -> STATE_CLOSED (force close)
 *   ERROR -> STATE_CLOSED
 */
static int handle_state_disconnecting(ConnFSM *fsm, ConnEvent event)
{
    switch (event) {
    case EVENT_DISCONNECTED_ACK:
        /* Graceful disconnect completed */
        simulate_cleanup(fsm);
        
        log_transition(fsm, STATE_DISCONNECTING, STATE_CLOSED, "disconnect complete");
        fsm->state = STATE_CLOSED;
        return 0;

    case EVENT_TIMEOUT:
        /* Disconnect timed out, force close */
        printf("  [ACTION] %s: Disconnect timeout, forcing close\n", fsm->name);
        simulate_cleanup(fsm);
        
        log_transition(fsm, STATE_DISCONNECTING, STATE_CLOSED, "timeout, forced close");
        fsm->state = STATE_CLOSED;
        return 0;

    case EVENT_ERROR:
        /* Error during disconnect, still close */
        simulate_cleanup(fsm);
        
        log_transition(fsm, STATE_DISCONNECTING, STATE_CLOSED, "error during disconnect");
        fsm->state = STATE_CLOSED;
        return -1;

    case EVENT_CONNECT:
    case EVENT_CONNECTED_ACK:
    case EVENT_DISCONNECT:
        log_ignored(fsm, event);
        return 0;

    default:
        printf("  [ERROR] %s: Unknown event %d\n", fsm->name, event);
        return -1;
    }
}

/*============================================================================
 * PART 5: HANDLER TABLE AND ENGINE
 *
 * KEY DESIGN POINT:
 *   The handlers array is the "glue" between states and their implementations.
 *   It's a simple 1D array indexed by state.
 *   The engine just looks up and calls the appropriate handler.
 *============================================================================*/

/* KEY: Array of function pointers, indexed by state */
static StateHandler handlers[STATE_COUNT] = {
    [STATE_CLOSED]        = handle_state_closed,
    [STATE_CONNECTING]    = handle_state_connecting,
    [STATE_CONNECTED]     = handle_state_connected,
    [STATE_DISCONNECTING] = handle_state_disconnecting,
};

/* Initialize FSM */
void fsm_init(ConnFSM *fsm, const char *name, void *user_data)
{
    memset(fsm, 0, sizeof(*fsm));
    fsm->state = STATE_CLOSED;
    fsm->name = name;
    fsm->user_data = user_data;
    printf("[FSM] %s: Initialized to state %s\n", name, state_names[fsm->state]);
}

/* Dispatch event to FSM
 *
 * KEY DESIGN: The engine is trivial
 *   1. Validate input
 *   2. Look up handler for current state
 *   3. Call handler
 *   
 * All the logic lives in the handlers, not here.
 */
int fsm_dispatch(ConnFSM *fsm, ConnEvent event)
{
    StateHandler handler;
    int result;

    if (fsm == NULL || event >= EVENT_COUNT) {
        printf("[FSM] ERROR: Invalid fsm or event\n");
        return -1;
    }

    if (fsm->state >= STATE_COUNT) {
        printf("[FSM] ERROR: Invalid state %d\n", fsm->state);
        return -1;
    }

    printf("[FSM] %s: Dispatching event %s in state %s\n",
           fsm->name, event_names[event], state_names[fsm->state]);

    /* KEY: Look up handler for current state */
    handler = handlers[fsm->state];
    
    if (handler == NULL) {
        printf("[FSM] ERROR: No handler for state %s\n", state_names[fsm->state]);
        return -1;
    }

    /* KEY: Call handler - it does all the work */
    result = handler(fsm, event);

    return result;
}

/* Get current state name */
const char* fsm_get_state_name(const ConnFSM *fsm)
{
    return state_names[fsm->state];
}

/*============================================================================
 * PART 6: DEMONSTRATION
 *============================================================================*/

int main(void)
{
    ConnFSM conn;

    printf("\n========== FUNCTION POINTER FSM DEMO ==========\n\n");

    fsm_init(&conn, "Connection1", NULL);

    printf("\n--- Scenario 1: Successful Connection ---\n");
    fsm_dispatch(&conn, EVENT_CONNECT);
    fsm_dispatch(&conn, EVENT_CONNECTED_ACK);

    printf("\n--- Scenario 2: Graceful Disconnect ---\n");
    fsm_dispatch(&conn, EVENT_DISCONNECT);
    fsm_dispatch(&conn, EVENT_DISCONNECTED_ACK);

    printf("\n--- Scenario 3: Retry Logic (Guard Condition Demo) ---\n");
    fsm_dispatch(&conn, EVENT_CONNECT);
    fsm_dispatch(&conn, EVENT_TIMEOUT);  /* Retry 1 */
    fsm_dispatch(&conn, EVENT_TIMEOUT);  /* Retry 2 */
    fsm_dispatch(&conn, EVENT_TIMEOUT);  /* Retry 3 */
    fsm_dispatch(&conn, EVENT_TIMEOUT);  /* Max exceeded -> CLOSED */

    printf("\n--- Scenario 4: Connection Lost While Connected ---\n");
    fsm_dispatch(&conn, EVENT_CONNECT);
    fsm_dispatch(&conn, EVENT_CONNECTED_ACK);
    fsm_dispatch(&conn, EVENT_ERROR);  /* Connection lost */

    printf("\n========== DEMO COMPLETE ==========\n");
    printf("Final state: %s\n\n", fsm_get_state_name(&conn));

    return 0;
}
```

---

## Key Takeaways

```
+====================================================================================+
|                       FUNCTION POINTER FSM SUMMARY                                 |
+====================================================================================+

    Strengths:
    +------------------------------------------------------------------------+
    | ✓ Maximum flexibility: each handler can have arbitrary complex logic   |
    | ✓ Testable: handlers can be unit-tested independently                  |
    | ✓ Encapsulated: all state logic in one place (the handler function)    |
    | ✓ Extensible: handlers can call different helper functions             |
    | ✓ Swappable: handlers can be replaced at runtime                       |
    | ✓ Guard conditions: naturally expressed as if/else in handler          |
    +------------------------------------------------------------------------+

    Weaknesses:
    +------------------------------------------------------------------------+
    | ✗ Less visible: transitions scattered across handler functions         |
    | ✗ More code: each handler has boilerplate switch statement             |
    | ✗ Harder to audit: must read all handlers to see all transitions       |
    | ✗ No compile-time check for missing event handling                     |
    +------------------------------------------------------------------------+

    Best Practices:
    +------------------------------------------------------------------------+
    | 1. Always handle default case in switch (log/assert unknown events)    |
    | 2. Keep handlers focused: complex logic should be in helper functions  |
    | 3. Use consistent structure: validation, action, transition, logging   |
    | 4. Consider splitting handlers into separate files for large FSMs      |
    | 5. Write unit tests for each handler function                          |
    +------------------------------------------------------------------------+

    When to Choose Function Pointer over Table-Driven:
    +------------------------------------------------------------------------+
    | - Complex guard conditions that depend on runtime state                |
    | - Per-transition logic varies significantly                            |
    | - Need to unit-test state handlers in isolation                        |
    | - May need to replace handlers at runtime                              |
    | - State logic is complex enough to warrant separate functions          |
    +------------------------------------------------------------------------+
```

### 中文说明

**函数指针FSM的核心优势**：

1. **最大灵活性**：每个 handler 可以有任意复杂的逻辑
2. **可测试性**：handler 可以独立进行单元测试
3. **封装性**：一个状态的所有逻辑集中在一个函数中
4. **守卫条件**：自然地用 if/else 表达，无需额外机制

**函数指针FSM的局限**：
1. **可见性差**：转换分散在各个 handler 函数中，难以一眼看清全貌
2. **代码量大**：每个 handler 都有 switch 语句的样板代码
3. **无编译期检查**：漏掉某个事件的处理不会触发编译错误

**何时选择函数指针方案**：
- 守卫条件依赖复杂的运行时状态
- 不同转换的处理逻辑差异很大
- 需要对状态逻辑进行独立的单元测试
- 可能需要在运行时替换 handler（插件架构）
