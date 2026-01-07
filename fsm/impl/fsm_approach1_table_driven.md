# FSM Approach 1: Table-Driven State Machine

## Topic: Connection State Machine

A connection manager that handles CONNECT, DISCONNECT, TIMEOUT, and ERROR events across multiple states.

---

## WHY | Engineering Motivation

```
+====================================================================================+
|                           WHY TABLE-DRIVEN FSM?                                    |
+====================================================================================+

    The Problem with Switch-Case FSM:
    +------------------------------------------------------------------------+
    |                                                                        |
    |   void handle_event(State *s, Event e) {                               |
    |       switch(s->current) {                                             |
    |           case STATE_A:                                                |
    |               if (e == EVENT_X) { ... }      // Scattered logic        |
    |               else if (e == EVENT_Y) { ... } // Hard to verify         |
    |               break;                         // Missing cases?         |
    |           case STATE_B:                                                |
    |               ...                            // Duplicated patterns    |
    |       }                                                                |
    |   }                                                                    |
    |                                                                        |
    |   Problems:                                                            |
    |   - Transitions scattered across 100s of lines                         |
    |   - Cannot verify all state×event combinations at compile time         |
    |   - Adding new state requires modifying multiple places                |
    |   - No single source of truth for state diagram                        |
    |                                                                        |
    +------------------------------------------------------------------------+


    The Table-Driven Solution:
    +------------------------------------------------------------------------+
    |                                                                        |
    |   Transition Table = Single Source of Truth                            |
    |                                                                        |
    |            |  EVENT_X      |  EVENT_Y      |  EVENT_Z      |           |
    |   ---------+---------------+---------------+---------------+           |
    |   STATE_A  | -> STATE_B    | -> STATE_A    | -> STATE_C    |           |
    |   STATE_B  | -> STATE_C    | -> STATE_A    | (invalid)     |           |
    |   STATE_C  | (invalid)     | -> STATE_A    | -> STATE_B    |           |
    |                                                                        |
    |   Benefits:                                                            |
    |   - All transitions visible in ONE place                               |
    |   - Compiler ensures table completeness (2D array)                     |
    |   - Easy to generate documentation from table                          |
    |   - O(1) lookup: table[state][event]                                   |
    |                                                                        |
    +------------------------------------------------------------------------+


    When to Use Table-Driven FSM:
    +------------------------------------------------------------------------+
    |                                                                        |
    |   ✓ Moderate number of states (3-20)                                   |
    |   ✓ Most state×event combinations have defined behavior                |
    |   ✓ Transitions follow consistent patterns                             |
    |   ✓ Need to verify/document all transitions                            |
    |   ✓ Want compile-time completeness checking                            |
    |                                                                        |
    |   ✗ Sparse transitions (most combinations invalid)                     |
    |   ✗ Complex per-transition logic that varies greatly                   |
    |   ✗ Hierarchical states with inheritance                               |
    |                                                                        |
    +------------------------------------------------------------------------+
```

### 中文说明

**为什么需要表驱动FSM？**

传统的 switch-case FSM 存在以下问题：
1. **逻辑分散**：状态转换逻辑散布在数百行代码中
2. **难以验证**：无法在编译期确保所有状态×事件组合都被处理
3. **维护困难**：新增状态需要修改多处代码
4. **缺乏全局视图**：没有单一的"状态图"可供审查

表驱动方案将所有转换规则集中在一个二维数组中，编译器会强制要求填充所有格子，从而确保完整性。

---

## HOW | Design Philosophy and Core Ideas

```
+====================================================================================+
|                          HOW TABLE-DRIVEN FSM WORKS                                |
+====================================================================================+

    Core Idea: Separate DATA from CODE
    +------------------------------------------------------------------------+
    |                                                                        |
    |   Traditional FSM:                                                     |
    |   +------------------+                                                 |
    |   | if/switch logic  |  <-- Transitions encoded as CODE               |
    |   +------------------+                                                 |
    |                                                                        |
    |   Table-Driven FSM:                                                    |
    |   +------------------+     +------------------+                        |
    |   | Transition TABLE |     | Generic Engine   |                        |
    |   | (DATA)           | --> | (CODE)           |                        |
    |   +------------------+     +------------------+                        |
    |         ^                         |                                    |
    |         |                         v                                    |
    |   Easy to read,            Simple, reusable,                           |
    |   modify, verify           rarely changes                              |
    |                                                                        |
    +------------------------------------------------------------------------+


    Transition Table Structure:
    +------------------------------------------------------------------------+
    |                                                                        |
    |   typedef struct {                                                     |
    |       State       next_state;   // Target state after transition       |
    |       ActionFunc  action;       // Side-effect function (optional)     |
    |       const char* description;  // Human-readable description          |
    |   } Transition;                                                        |
    |                                                                        |
    |   // 2D array: rows = states, columns = events                         |
    |   Transition table[STATE_COUNT][EVENT_COUNT];                          |
    |                                                                        |
    |                     EVENT_CONNECT  EVENT_DISCONNECT  EVENT_TIMEOUT     |
    |                    +---------------+----------------+---------------+  |
    |   STATE_CLOSED     | CONNECTING    | (ignore)       | (ignore)      |  |
    |                    | do_connect    |                |               |  |
    |                    +---------------+----------------+---------------+  |
    |   STATE_CONNECTING | (ignore)      | CLOSED         | CLOSED        |  |
    |                    |               | do_abort       | do_timeout    |  |
    |                    +---------------+----------------+---------------+  |
    |   STATE_CONNECTED  | (ignore)      | DISCONNECTING  | (ignore)      |  |
    |                    |               | do_disconnect  |               |  |
    |                    +---------------+----------------+---------------+  |
    |                                                                        |
    +------------------------------------------------------------------------+


    Execution Model:
    +------------------------------------------------------------------------+
    |                                                                        |
    |   +-------+                                                            |
    |   | Event | ─────────────────────────────┐                             |
    |   +-------+                              |                             |
    |                                          v                             |
    |   +---------------+    +---------------------------------+             |
    |   | Current State | -> | table[current_state][event]     |             |
    |   +---------------+    +---------------------------------+             |
    |                                          |                             |
    |                                          v                             |
    |                        +----------------------------------+            |
    |                        | Transition {                     |            |
    |                        |   next_state = STATE_X           |            |
    |                        |   action = do_something()        |            |
    |                        | }                                |            |
    |                        +----------------------------------+            |
    |                                    |                                   |
    |                    +---------------+---------------+                   |
    |                    v                               v                   |
    |            +--------------+               +-----------------+          |
    |            | Execute      |               | Update State    |          |
    |            | action()     |               | current = next  |          |
    |            +--------------+               +-----------------+          |
    |                                                                        |
    +------------------------------------------------------------------------+


    Design Decisions:
    +------------------------------------------------------------------------+
    |                                                                        |
    |   1. Invalid Transitions:                                              |
    |      - Use sentinel value (next_state = INVALID or same state)         |
    |      - action = NULL means no side-effect                              |
    |                                                                        |
    |   2. Guard Conditions:                                                 |
    |      - Add guard function pointer to Transition struct                 |
    |      - Or handle in action function (simpler for small FSMs)           |
    |                                                                        |
    |   3. Entry/Exit Actions:                                               |
    |      - Separate arrays: on_enter[STATE_COUNT], on_exit[STATE_COUNT]    |
    |      - Called by engine when state actually changes                    |
    |                                                                        |
    |   4. Extended State (Context):                                         |
    |      - FSM struct contains both state enum AND context data            |
    |      - Actions receive context pointer for side-effects                |
    |                                                                        |
    +------------------------------------------------------------------------+
```

### 中文说明

**表驱动FSM的核心思想：数据与代码分离**

1. **转换表是数据**：所有转换规则编码在一个二维数组中，易于阅读和修改
2. **引擎是代码**：一个通用的执行引擎，根据当前状态和事件查表，执行转换

**转换表结构**：
- 每个表格单元包含：目标状态、动作函数、描述
- 行 = 当前状态，列 = 输入事件
- `table[current_state][event]` 直接获得转换规则

**执行模型**：
1. 接收事件
2. 用当前状态和事件作为索引查表
3. 获取转换规则（目标状态 + 动作）
4. 执行动作函数
5. 更新当前状态

---

## WHAT | Architecture and Concrete Forms

```
+====================================================================================+
|                      TABLE-DRIVEN FSM ARCHITECTURE                                 |
+====================================================================================+

    File Organization:
    +------------------------------------------------------------------------+
    |                                                                        |
    |   connection_fsm.h          connection_fsm.c                           |
    |   +------------------+      +-------------------------------------+    |
    |   | State enum       |      | Transition table (static const)    |    |
    |   | Event enum       |      | Action implementations             |    |
    |   | FSM struct       |      | Engine: fsm_init(), fsm_dispatch() |    |
    |   | Public API       |      | Entry/Exit handlers (optional)     |    |
    |   +------------------+      +-------------------------------------+    |
    |                                                                        |
    +------------------------------------------------------------------------+


    State Diagram (Connection FSM):
    +------------------------------------------------------------------------+
    |                                                                        |
    |                        CONNECT                                         |
    |              +--------------------+                                    |
    |              |                    v                                    |
    |         +--------+          +------------+                             |
    |         | CLOSED |          | CONNECTING |---+                         |
    |         +--------+          +------------+   |                         |
    |              ^                    |          | TIMEOUT/ERROR           |
    |              |     CONNECTED_ACK  |          |                         |
    |              |                    v          |                         |
    |              |              +-----------+    |                         |
    |              +--DISCONNECT--| CONNECTED |    |                         |
    |              |    _ACK      +-----------+    |                         |
    |              |                    |          |                         |
    |              |          DISCONNECT|          |                         |
    |              |                    v          |                         |
    |         +---------------+        +--+        |                         |
    |         | DISCONNECTING |<-------+  |        |                         |
    |         +---------------+           |        |                         |
    |              |                      +--------+                         |
    |              +------TIMEOUT---------+                                  |
    |                                                                        |
    +------------------------------------------------------------------------+


    Memory Layout:
    +------------------------------------------------------------------------+
    |                                                                        |
    |   FSM Instance (runtime):         Transition Table (static, const):   |
    |   +--------------------+          +--------------------------------+   |
    |   | state: CONNECTED   |          | [CLOSED][CONNECT]    -> ...    |   |
    |   | retry_count: 2     |          | [CLOSED][DISCONNECT] -> ...    |   |
    |   | last_error: 0      |          | [CONNECTING][...]    -> ...    |   |
    |   | user_data: ptr     |          | ...                            |   |
    |   +--------------------+          +--------------------------------+   |
    |           |                                    ^                       |
    |           | fsm_dispatch(event)                |                       |
    |           +------------------------------------+                       |
    |                      table lookup                                      |
    |                                                                        |
    +------------------------------------------------------------------------+
```

### 中文说明

**架构组成**：

1. **头文件**：定义状态枚举、事件枚举、FSM结构体、公共API
2. **源文件**：实现转换表（静态常量）、动作函数、引擎函数

**状态图说明**：
- `CLOSED` → (CONNECT) → `CONNECTING`
- `CONNECTING` → (CONNECTED_ACK) → `CONNECTED`
- `CONNECTING` → (TIMEOUT/ERROR) → `CLOSED`
- `CONNECTED` → (DISCONNECT) → `DISCONNECTING`
- `DISCONNECTING` → (DISCONNECTED_ACK) → `CLOSED`

**内存布局**：
- FSM实例（运行时）：当前状态 + 扩展状态（重试次数、错误码等）
- 转换表（静态常量）：编译时确定，所有FSM实例共享

---

## Complete C Example

```c
/******************************************************************************
 * FILE: connection_fsm_table_driven.c
 * 
 * DESCRIPTION:
 *   Complete example of Table-Driven FSM for a connection state machine.
 *   Compile: gcc -o fsm_table connection_fsm_table_driven.c
 *   Run: ./fsm_table
 *
 * WHY TABLE-DRIVEN:
 *   - All transitions visible in ONE 2D array
 *   - Compiler ensures all state×event cells are defined
 *   - Easy to add/remove states without touching engine code
 *   - Self-documenting: table IS the state diagram
 *
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/*============================================================================
 * PART 1: TYPE DEFINITIONS
 *   Define states, events, and the FSM structure
 *============================================================================*/

/* KEY DESIGN: Use enums with explicit COUNT for array sizing */
typedef enum {
    STATE_CLOSED = 0,      /* Initial state, no connection */
    STATE_CONNECTING,      /* Connection attempt in progress */
    STATE_CONNECTED,       /* Connection established */
    STATE_DISCONNECTING,   /* Graceful disconnect in progress */
    STATE_COUNT            /* MUST be last - used for array sizing */
} ConnState;

typedef enum {
    EVENT_CONNECT = 0,     /* User requests connection */
    EVENT_CONNECTED_ACK,   /* Connection succeeded */
    EVENT_DISCONNECT,      /* User requests disconnect */
    EVENT_DISCONNECTED_ACK,/* Disconnect completed */
    EVENT_TIMEOUT,         /* Operation timed out */
    EVENT_ERROR,           /* Error occurred */
    EVENT_COUNT            /* MUST be last - used for array sizing */
} ConnEvent;

/* Forward declaration for action function signature */
struct ConnFSM;

/* KEY DESIGN: Action function pointer type
 * - Receives FSM pointer for context access
 * - Receives event for conditional logic
 * - Returns 0 on success, -1 on failure */
typedef int (*ActionFunc)(struct ConnFSM *fsm, ConnEvent event);

/* KEY DESIGN: Transition structure - one cell in the table
 * Each cell defines: where to go + what to do + description */
typedef struct {
    ConnState   next_state;   /* Target state after transition */
    ActionFunc  action;       /* Side-effect function (NULL = no action) */
    const char* description;  /* Human-readable description for logging */
} Transition;

/* KEY DESIGN: FSM instance structure
 * Separates FSM state from transition logic
 * Multiple instances can share the same transition table */
typedef struct ConnFSM {
    ConnState   state;        /* Current state */
    int         retry_count;  /* Extended state: retry counter */
    int         last_error;   /* Extended state: last error code */
    const char* name;         /* Instance identifier for logging */
    void*       user_data;    /* User-provided context */
} ConnFSM;

/*============================================================================
 * PART 2: STATE AND EVENT NAME TABLES (for logging)
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
 * PART 3: ACTION FUNCTION IMPLEMENTATIONS
 *   These are the side-effects triggered by transitions
 *============================================================================*/

/* Action: Start connection attempt */
static int action_start_connect(ConnFSM *fsm, ConnEvent event)
{
    (void)event;  /* Unused in this action */
    fsm->retry_count = 0;
    printf("  [ACTION] %s: Starting connection attempt...\n", fsm->name);
    return 0;
}

/* Action: Connection established successfully */
static int action_connected(ConnFSM *fsm, ConnEvent event)
{
    (void)event;
    printf("  [ACTION] %s: Connection established! Ready for data.\n", fsm->name);
    return 0;
}

/* Action: Start graceful disconnect */
static int action_start_disconnect(ConnFSM *fsm, ConnEvent event)
{
    (void)event;
    printf("  [ACTION] %s: Initiating graceful disconnect...\n", fsm->name);
    return 0;
}

/* Action: Disconnect completed, cleanup */
static int action_disconnected(ConnFSM *fsm, ConnEvent event)
{
    (void)event;
    fsm->retry_count = 0;
    fsm->last_error = 0;
    printf("  [ACTION] %s: Disconnected, resources cleaned up.\n", fsm->name);
    return 0;
}

/* Action: Handle timeout during connect */
static int action_connect_timeout(ConnFSM *fsm, ConnEvent event)
{
    (void)event;
    fsm->retry_count++;
    fsm->last_error = -1;
    printf("  [ACTION] %s: Connection timeout (retry_count=%d)\n", 
           fsm->name, fsm->retry_count);
    return 0;
}

/* Action: Handle error during connect */
static int action_connect_error(ConnFSM *fsm, ConnEvent event)
{
    (void)event;
    fsm->last_error = -2;
    printf("  [ACTION] %s: Connection error occurred\n", fsm->name);
    return 0;
}

/* Action: Handle timeout during disconnect */
static int action_disconnect_timeout(ConnFSM *fsm, ConnEvent event)
{
    (void)event;
    printf("  [ACTION] %s: Disconnect timeout, forcing close\n", fsm->name);
    return 0;
}

/* Action: Log ignored event (for invalid transitions) */
static int action_ignore(ConnFSM *fsm, ConnEvent event)
{
    printf("  [ACTION] %s: Ignoring event %s in state %s\n",
           fsm->name, event_names[event], state_names[fsm->state]);
    return 0;
}

/*============================================================================
 * PART 4: THE TRANSITION TABLE (Core of Table-Driven FSM)
 *
 * KEY DESIGN POINT:
 *   This 2D array IS the state machine specification.
 *   - Rows: current states (STATE_COUNT rows)
 *   - Columns: input events (EVENT_COUNT columns)
 *   - Each cell: {next_state, action, description}
 *
 *   The compiler ensures EVERY cell is initialized (no missing transitions).
 *   To verify completeness: STATE_COUNT × EVENT_COUNT = total cells
 *============================================================================*/

static const Transition transition_table[STATE_COUNT][EVENT_COUNT] = {
    /* ======================== STATE_CLOSED ======================== */
    [STATE_CLOSED] = {
        /*         CONNECT              CONNECTED_ACK         DISCONNECT           */
        [EVENT_CONNECT]         = { STATE_CONNECTING,    action_start_connect,  "start connection"   },
        [EVENT_CONNECTED_ACK]   = { STATE_CLOSED,        action_ignore,         "ignore (not connecting)" },
        [EVENT_DISCONNECT]      = { STATE_CLOSED,        action_ignore,         "ignore (already closed)" },
        [EVENT_DISCONNECTED_ACK]= { STATE_CLOSED,        action_ignore,         "ignore (already closed)" },
        [EVENT_TIMEOUT]         = { STATE_CLOSED,        action_ignore,         "ignore (no pending op)"  },
        [EVENT_ERROR]           = { STATE_CLOSED,        action_ignore,         "ignore (no pending op)"  },
    },

    /* ======================== STATE_CONNECTING ======================== */
    [STATE_CONNECTING] = {
        [EVENT_CONNECT]         = { STATE_CONNECTING,    action_ignore,         "ignore (already connecting)" },
        [EVENT_CONNECTED_ACK]   = { STATE_CONNECTED,     action_connected,      "connection succeeded" },
        [EVENT_DISCONNECT]      = { STATE_CLOSED,        action_disconnected,   "abort connection" },
        [EVENT_DISCONNECTED_ACK]= { STATE_CONNECTING,    action_ignore,         "ignore (not disconnecting)" },
        [EVENT_TIMEOUT]         = { STATE_CLOSED,        action_connect_timeout,"connection timeout" },
        [EVENT_ERROR]           = { STATE_CLOSED,        action_connect_error,  "connection failed" },
    },

    /* ======================== STATE_CONNECTED ======================== */
    [STATE_CONNECTED] = {
        [EVENT_CONNECT]         = { STATE_CONNECTED,     action_ignore,         "ignore (already connected)" },
        [EVENT_CONNECTED_ACK]   = { STATE_CONNECTED,     action_ignore,         "ignore (already connected)" },
        [EVENT_DISCONNECT]      = { STATE_DISCONNECTING, action_start_disconnect,"start disconnect" },
        [EVENT_DISCONNECTED_ACK]= { STATE_CONNECTED,     action_ignore,         "ignore (not disconnecting)" },
        [EVENT_TIMEOUT]         = { STATE_CONNECTED,     action_ignore,         "ignore (no pending op)" },
        [EVENT_ERROR]           = { STATE_CLOSED,        action_connect_error,  "connection lost" },
    },

    /* ======================== STATE_DISCONNECTING ======================== */
    [STATE_DISCONNECTING] = {
        [EVENT_CONNECT]         = { STATE_DISCONNECTING, action_ignore,         "ignore (disconnecting)" },
        [EVENT_CONNECTED_ACK]   = { STATE_DISCONNECTING, action_ignore,         "ignore (disconnecting)" },
        [EVENT_DISCONNECT]      = { STATE_DISCONNECTING, action_ignore,         "ignore (already disconnecting)" },
        [EVENT_DISCONNECTED_ACK]= { STATE_CLOSED,        action_disconnected,   "disconnect completed" },
        [EVENT_TIMEOUT]         = { STATE_CLOSED,        action_disconnect_timeout,"force close" },
        [EVENT_ERROR]           = { STATE_CLOSED,        action_disconnected,   "error during disconnect" },
    },
};

/*============================================================================
 * PART 5: FSM ENGINE (Generic, Reusable)
 *
 * KEY DESIGN POINT:
 *   The engine is simple and rarely needs modification.
 *   All state machine logic lives in the transition table.
 *============================================================================*/

/* Initialize FSM to starting state */
void fsm_init(ConnFSM *fsm, const char *name, void *user_data)
{
    memset(fsm, 0, sizeof(*fsm));
    fsm->state = STATE_CLOSED;  /* Initial state */
    fsm->name = name;
    fsm->user_data = user_data;
    printf("[FSM] %s: Initialized to state %s\n", name, state_names[fsm->state]);
}

/* Dispatch event to FSM - THE CORE ENGINE FUNCTION
 *
 * KEY DESIGN: O(1) table lookup
 *   1. Use current state and event as array indices
 *   2. Get transition struct from table
 *   3. Execute action (if any)
 *   4. Update state
 */
int fsm_dispatch(ConnFSM *fsm, ConnEvent event)
{
    const Transition *trans;
    ConnState old_state;
    int result = 0;

    /* Validate inputs */
    if (fsm == NULL || event >= EVENT_COUNT) {
        printf("[FSM] ERROR: Invalid fsm or event\n");
        return -1;
    }

    /* KEY: Direct table lookup - O(1) */
    trans = &transition_table[fsm->state][event];
    old_state = fsm->state;

    printf("[FSM] %s: Event %s in state %s -> %s (%s)\n",
           fsm->name, event_names[event], state_names[old_state],
           state_names[trans->next_state], trans->description);

    /* Execute action if defined */
    if (trans->action != NULL) {
        result = trans->action(fsm, event);
    }

    /* Update state (even if action fails, for simplicity) */
    fsm->state = trans->next_state;

    /* Optional: detect state change for entry/exit actions */
    if (old_state != fsm->state) {
        printf("[FSM] %s: State changed: %s -> %s\n",
               fsm->name, state_names[old_state], state_names[fsm->state]);
    }

    return result;
}

/* Get current state (for external queries) */
ConnState fsm_get_state(const ConnFSM *fsm)
{
    return fsm->state;
}

/* Get state name (for debugging) */
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

    printf("\n========== TABLE-DRIVEN FSM DEMO ==========\n\n");

    /* Initialize FSM */
    fsm_init(&conn, "Connection1", NULL);

    printf("\n--- Scenario 1: Successful Connection ---\n");
    fsm_dispatch(&conn, EVENT_CONNECT);       /* CLOSED -> CONNECTING */
    fsm_dispatch(&conn, EVENT_CONNECTED_ACK); /* CONNECTING -> CONNECTED */

    printf("\n--- Scenario 2: Graceful Disconnect ---\n");
    fsm_dispatch(&conn, EVENT_DISCONNECT);       /* CONNECTED -> DISCONNECTING */
    fsm_dispatch(&conn, EVENT_DISCONNECTED_ACK); /* DISCONNECTING -> CLOSED */

    printf("\n--- Scenario 3: Connection Timeout ---\n");
    fsm_dispatch(&conn, EVENT_CONNECT);  /* CLOSED -> CONNECTING */
    fsm_dispatch(&conn, EVENT_TIMEOUT);  /* CONNECTING -> CLOSED (timeout) */

    printf("\n--- Scenario 4: Invalid Events (Ignored) ---\n");
    fsm_dispatch(&conn, EVENT_DISCONNECT);    /* CLOSED: ignore */
    fsm_dispatch(&conn, EVENT_CONNECTED_ACK); /* CLOSED: ignore */

    printf("\n--- Scenario 5: Connection Error ---\n");
    fsm_dispatch(&conn, EVENT_CONNECT);  /* CLOSED -> CONNECTING */
    fsm_dispatch(&conn, EVENT_ERROR);    /* CONNECTING -> CLOSED (error) */

    printf("\n========== DEMO COMPLETE ==========\n");
    printf("Final state: %s\n\n", fsm_get_state_name(&conn));

    return 0;
}
```

---

## Key Takeaways

```
+====================================================================================+
|                          TABLE-DRIVEN FSM SUMMARY                                  |
+====================================================================================+

    Strengths:
    +------------------------------------------------------------------------+
    | ✓ Single source of truth: transition_table IS the state diagram       |
    | ✓ Compile-time completeness: missing cells cause compiler warning     |
    | ✓ O(1) dispatch: direct array indexing, no searching                  |
    | ✓ Easy to extend: add row for new state, column for new event         |
    | ✓ Self-documenting: description field serves as inline documentation  |
    | ✓ Easy testing: can iterate over table to verify all transitions      |
    +------------------------------------------------------------------------+

    Weaknesses:
    +------------------------------------------------------------------------+
    | ✗ Memory overhead: STATE_COUNT × EVENT_COUNT cells (even if sparse)   |
    | ✗ Complex guards: need extra logic for conditional transitions         |
    | ✗ No hierarchy: flat table doesn't support state inheritance          |
    +------------------------------------------------------------------------+

    Best Practices:
    +------------------------------------------------------------------------+
    | 1. Always use designated initializers [STATE_X][EVENT_Y] = {...}       |
    | 2. Define STATE_COUNT/EVENT_COUNT as last enum value                   |
    | 3. Use action_ignore for invalid transitions (not NULL)                |
    | 4. Keep actions small; complex logic belongs elsewhere                 |
    | 5. Add description field for logging and documentation                 |
    +------------------------------------------------------------------------+
```

### 中文说明

**表驱动FSM的核心优势**：

1. **单一事实来源**：转换表就是状态图，不需要额外文档
2. **编译期完整性**：漏填的格子会触发编译警告
3. **O(1) 查找**：直接数组索引，无需遍历
4. **易于扩展**：新增状态只需添加一行，新增事件只需添加一列

**最佳实践**：
- 使用指定初始化器 `[STATE_X][EVENT_Y] = {...}` 提高可读性
- 用 `action_ignore` 处理无效转换，而不是 NULL
- 保持动作函数简单，复杂逻辑放在其他模块
- 添加 description 字段用于日志和文档生成
