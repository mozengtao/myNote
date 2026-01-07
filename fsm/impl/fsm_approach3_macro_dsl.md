# FSM Approach 3: Macro-Based DSL for FSM Definition

## Topic: Connection State Machine

The same connection manager, implemented using C preprocessor macros as a Domain-Specific Language (DSL) for maximum declarativeness and self-documentation.

---

## WHY | Engineering Motivation

```
+====================================================================================+
|                           WHY MACRO-BASED DSL?                                     |
+====================================================================================+

    The Problem with Both Previous Approaches:
    +------------------------------------------------------------------------+
    |                                                                        |
    |   Table-Driven:                                                        |
    |   +----------------------------------------------------------+        |
    |   | static const Transition table[STATE_COUNT][EVENT_COUNT]= |        |
    |   |   [STATE_CLOSED] = {                                     |        |
    |   |     [EVENT_CONNECT] = { STATE_CONNECTING, act_connect, ""},|       |
    |   |     [EVENT_TIMEOUT] = { STATE_CLOSED, act_ignore, "" },  |        |
    |   |     ...                                                   |        |
    |   |   },                                                      |        |
    |   | };                                                        |        |
    |   +----------------------------------------------------------+        |
    |   Problem: Verbose, repetitive, hard to read at a glance              |
    |                                                                        |
    |   Function Pointer:                                                    |
    |   +----------------------------------------------------------+        |
    |   | int handle_closed(FSM *fsm, Event event) {               |        |
    |   |   switch(event) {                                        |        |
    |   |     case EVENT_CONNECT: ... break;                       |        |
    |   |     case EVENT_TIMEOUT: ... break;                       |        |
    |   |   }                                                       |        |
    |   | }                                                         |        |
    |   +----------------------------------------------------------+        |
    |   Problem: Transitions buried in code, not visible as data            |
    |                                                                        |
    +------------------------------------------------------------------------+


    The Ideal Representation (What We Want):
    +------------------------------------------------------------------------+
    |                                                                        |
    |   We want to write FSM specification like a STATE DIAGRAM:             |
    |                                                                        |
    |   STATE(CLOSED)                                                        |
    |       ON(CONNECT)    -> CONNECTING   DO(start_connect)                 |
    |       ON(DISCONNECT) -> CLOSED       DO(ignore)                        |
    |       ON(TIMEOUT)    -> CLOSED       DO(ignore)                        |
    |                                                                        |
    |   STATE(CONNECTING)                                                    |
    |       ON(CONNECTED_ACK) -> CONNECTED    DO(connected)                  |
    |       ON(TIMEOUT)       -> CLOSED       GUARD(max_retries) DO(timeout) |
    |       ON(DISCONNECT)    -> CLOSED       DO(abort)                      |
    |                                                                        |
    |   Benefits:                                                            |
    |   - Reads like a state diagram                                         |
    |   - Self-documenting                                                   |
    |   - Easy to audit all transitions                                      |
    |   - Can generate documentation from same source                        |
    |                                                                        |
    +------------------------------------------------------------------------+


    Macro DSL Makes This Possible:
    +------------------------------------------------------------------------+
    |                                                                        |
    |   C Preprocessor can transform DSL syntax into:                        |
    |   - Transition table entries                                           |
    |   - State/event enums                                                  |
    |   - Documentation strings                                              |
    |   - Validation code                                                    |
    |                                                                        |
    |   #define FSM_TRANSITIONS \                                            |
    |       X(CLOSED,      CONNECT,       CONNECTING,    act_connect)   \    |
    |       X(CLOSED,      DISCONNECT,    CLOSED,        act_ignore)    \    |
    |       X(CONNECTING,  CONNECTED_ACK, CONNECTED,     act_connected) \    |
    |       X(CONNECTING,  TIMEOUT,       CLOSED,        act_timeout)        |
    |                                                                        |
    |   The X-macro pattern lets us reuse this spec for:                     |
    |   - Generating transition table                                        |
    |   - Generating state names                                             |
    |   - Generating documentation                                           |
    |                                                                        |
    +------------------------------------------------------------------------+


    When to Use Macro DSL:
    +------------------------------------------------------------------------+
    |                                                                        |
    |   ✓ Want maximum declarativeness and readability                       |
    |   ✓ Need to generate multiple outputs from same specification          |
    |   ✓ Team is comfortable with macro techniques                          |
    |   ✓ FSM structure is relatively stable                                 |
    |   ✓ Want automatic documentation generation                            |
    |                                                                        |
    |   ✗ Team unfamiliar with X-macro pattern                               |
    |   ✗ Need complex runtime-dynamic transitions                           |
    |   ✗ Debugging macros is difficult in your environment                  |
    |   ✗ Heavy use of guards that don't fit simple pattern                  |
    |                                                                        |
    +------------------------------------------------------------------------+
```

### 中文说明

**为什么需要宏DSL？**

前两种方案的问题：
1. **表驱动**：虽然集中，但语法冗长，大量重复的结构体初始化器
2. **函数指针**：逻辑清晰，但转换规则埋在代码中，无法一目了然

**理想的表示方式**：像状态图一样书写规格说明：
```
STATE(CLOSED)
    ON(CONNECT) -> CONNECTING  DO(start_connect)
```

**X-Macro技术**使这成为可能：
- 定义一次转换规则
- 复用规则生成：转换表、状态名、文档等
- 单一事实来源，自动保持一致

---

## HOW | Design Philosophy and Core Ideas

```
+====================================================================================+
|                        HOW MACRO DSL WORKS                                         |
+====================================================================================+

    The X-Macro Pattern:
    +------------------------------------------------------------------------+
    |                                                                        |
    |   Step 1: Define data as a macro that takes X as parameter             |
    |   +----------------------------------------------------------+        |
    |   | #define STATES(X) \                                      |        |
    |   |     X(CLOSED)      \                                     |        |
    |   |     X(CONNECTING)  \                                     |        |
    |   |     X(CONNECTED)   \                                     |        |
    |   |     X(DISCONNECTING)                                     |        |
    |   +----------------------------------------------------------+        |
    |                                                                        |
    |   Step 2: Define X to generate what you need                           |
    |   +----------------------------------------------------------+        |
    |   | // Generate enum:                                        |        |
    |   | #define X(name) STATE_##name,                            |        |
    |   | typedef enum { STATES(X) STATE_COUNT } State;            |        |
    |   | #undef X                                                 |        |
    |   |                                                          |        |
    |   | // Generate name strings:                                |        |
    |   | #define X(name) #name,                                   |        |
    |   | const char* state_names[] = { STATES(X) };               |        |
    |   | #undef X                                                 |        |
    |   +----------------------------------------------------------+        |
    |                                                                        |
    |   Result: enum and names are ALWAYS in sync!                           |
    |                                                                        |
    +------------------------------------------------------------------------+


    Extended X-Macro for Transitions:
    +------------------------------------------------------------------------+
    |                                                                        |
    |   #define TRANSITIONS(X) \                                             |
    |   /*  FROM_STATE    EVENT          TO_STATE       ACTION */  \         |
    |       X(CLOSED,      CONNECT,       CONNECTING,    act_connect)   \    |
    |       X(CLOSED,      DISCONNECT,    CLOSED,        act_ignore)    \    |
    |       X(CLOSED,      TIMEOUT,       CLOSED,        act_ignore)    \    |
    |       X(CONNECTING,  CONNECTED_ACK, CONNECTED,     act_connected) \    |
    |       X(CONNECTING,  TIMEOUT,       CLOSED,        act_timeout)   \    |
    |       X(CONNECTING,  DISCONNECT,    CLOSED,        act_abort)     \    |
    |       ...                                                              |
    |                                                                        |
    |   This is THE specification. Everything else is generated.             |
    |                                                                        |
    +------------------------------------------------------------------------+


    Generating Transition Table:
    +------------------------------------------------------------------------+
    |                                                                        |
    |   #define X(from, event, to, action) \                                 |
    |       [STATE_##from][EVENT_##event] = { STATE_##to, action, #action }, |
    |                                                                        |
    |   static const Transition table[STATE_COUNT][EVENT_COUNT] = {          |
    |       TRANSITIONS(X)                                                   |
    |   };                                                                   |
    |   #undef X                                                             |
    |                                                                        |
    |   This generates the entire 2D table from the TRANSITIONS macro!       |
    |                                                                        |
    +------------------------------------------------------------------------+


    Multiple Outputs from Same Source:
    +------------------------------------------------------------------------+
    |                                                                        |
    |                    +----------------------+                            |
    |                    | TRANSITIONS(X) macro |                            |
    |                    | (Single Source)      |                            |
    |                    +----------------------+                            |
    |                              |                                         |
    |              +---------------+---------------+                         |
    |              |               |               |                         |
    |              v               v               v                         |
    |   +----------------+ +---------------+ +------------------+            |
    |   | Transition     | | Documentation | | Validation       |            |
    |   | Table (code)   | | (text/dot)    | | (assertions)     |            |
    |   +----------------+ +---------------+ +------------------+            |
    |                                                                        |
    |   // Generate DOT graph for visualization                              |
    |   #define X(from, ev, to, act) \                                       |
    |       printf("  %s -> %s [label=\"%s\"];\n", #from, #to, #ev);         |
    |   void print_dot_graph(void) { TRANSITIONS(X) }                        |
    |   #undef X                                                             |
    |                                                                        |
    +------------------------------------------------------------------------+


    Handling Default/Fallback Transitions:
    +------------------------------------------------------------------------+
    |                                                                        |
    |   Problem: X-macro only defines explicit transitions.                  |
    |            What about undefined state×event combinations?              |
    |                                                                        |
    |   Solution 1: Initialize entire table first, then apply X-macro       |
    |   +----------------------------------------------------------+        |
    |   | // First, initialize all cells to "ignore"               |        |
    |   | static Transition table[STATE_COUNT][EVENT_COUNT];       |        |
    |   | void init_table(void) {                                  |        |
    |   |     for (int s = 0; s < STATE_COUNT; s++)                |        |
    |   |         for (int e = 0; e < EVENT_COUNT; e++)            |        |
    |   |             table[s][e] = (Transition){s, act_ignore, ""};|        |
    |   |     // Then apply X-macro to override specific cells     |        |
    |   |     #define X(from, ev, to, act) \                       |        |
    |   |         table[STATE_##from][EVENT_##ev] = \              |        |
    |   |             (Transition){STATE_##to, act, #act};         |        |
    |   |     TRANSITIONS(X)                                       |        |
    |   |     #undef X                                             |        |
    |   | }                                                         |        |
    |   +----------------------------------------------------------+        |
    |                                                                        |
    |   Solution 2: Define DEFAULT_TRANSITIONS macro                        |
    |   (More explicit, but more verbose)                                    |
    |                                                                        |
    +------------------------------------------------------------------------+
```

### 中文说明

**X-Macro 模式的核心思想**：

1. **定义数据**：用一个宏（如 `TRANSITIONS(X)`）定义所有转换规则，但不指定 X 是什么
2. **生成代码**：根据需要，将 X 定义为不同的代码生成器，然后展开宏

**单一事实来源**：
- 所有转换规则只定义一次
- 枚举、字符串名、转换表、文档都从同一个宏生成
- 修改规则时，所有输出自动保持同步

**处理默认转换**：
- X-Macro 只定义显式的转换
- 可以先用默认值初始化整个表，再用 X-Macro 覆盖特定单元

---

## WHAT | Architecture and Concrete Forms

```
+====================================================================================+
|                       MACRO DSL FSM ARCHITECTURE                                   |
+====================================================================================+

    File Organization:
    +------------------------------------------------------------------------+
    |                                                                        |
    |   fsm_spec.h (THE specification - human-readable)                      |
    |   +----------------------------------------------------------+        |
    |   | #define STATES(X) \                                      |        |
    |   |     X(CLOSED) X(CONNECTING) X(CONNECTED) X(DISCONNECTING)|        |
    |   |                                                          |        |
    |   | #define EVENTS(X) \                                      |        |
    |   |     X(CONNECT) X(CONNECTED_ACK) X(DISCONNECT) ...        |        |
    |   |                                                          |        |
    |   | #define TRANSITIONS(X) \                                 |        |
    |   |     X(CLOSED, CONNECT, CONNECTING, act_connect) \        |        |
    |   |     X(CONNECTING, CONNECTED_ACK, CONNECTED, act_ok) \    |        |
    |   |     ...                                                   |        |
    |   +----------------------------------------------------------+        |
    |                                                                        |
    |   fsm_generated.c (Generated code - don't edit manually)               |
    |   +----------------------------------------------------------+        |
    |   | #include "fsm_spec.h"                                    |        |
    |   |                                                          |        |
    |   | // Generate enums                                        |        |
    |   | #define X(name) STATE_##name,                            |        |
    |   | typedef enum { STATES(X) STATE_COUNT } State;            |        |
    |   | #undef X                                                 |        |
    |   |                                                          |        |
    |   | // Generate transition table                             |        |
    |   | #define X(from, ev, to, act) ...                         |        |
    |   | static const Transition table[...] = { TRANSITIONS(X) }; |        |
    |   | #undef X                                                 |        |
    |   +----------------------------------------------------------+        |
    |                                                                        |
    |   fsm_engine.c (Unchanged from Table-Driven approach)                  |
    |   +----------------------------------------------------------+        |
    |   | void fsm_init(...) { ... }                               |        |
    |   | int fsm_dispatch(...) { table lookup ... }               |        |
    |   +----------------------------------------------------------+        |
    |                                                                        |
    +------------------------------------------------------------------------+


    Macro Expansion Visualization:
    +------------------------------------------------------------------------+
    |                                                                        |
    |   Source:                                                              |
    |   +----------------------------------------------------------+        |
    |   | #define TRANSITIONS(X) \                                 |        |
    |   |     X(CLOSED, CONNECT, CONNECTING, act_connect)          |        |
    |   |                                                          |        |
    |   | #define X(from, ev, to, act) \                           |        |
    |   |     [STATE_##from][EVENT_##ev] = {STATE_##to, act, #act},|        |
    |   |                                                          |        |
    |   | Transition table[...] = { TRANSITIONS(X) };              |        |
    |   +----------------------------------------------------------+        |
    |                                                                        |
    |   After Preprocessing (gcc -E):                                        |
    |   +----------------------------------------------------------+        |
    |   | Transition table[...] = {                                |        |
    |   |     [STATE_CLOSED][EVENT_CONNECT] = {                    |        |
    |   |         STATE_CONNECTING, act_connect, "act_connect"     |        |
    |   |     },                                                    |        |
    |   | };                                                        |        |
    |   +----------------------------------------------------------+        |
    |                                                                        |
    +------------------------------------------------------------------------+


    Generating Documentation (Graphviz DOT):
    +------------------------------------------------------------------------+
    |                                                                        |
    |   #define X(from, ev, to, act) \                                       |
    |       printf("  %s -> %s [label=\"%s\\n%s\"];\n", \                    |
    |              #from, #to, #ev, #act);                                   |
    |                                                                        |
    |   void generate_dot(void) {                                            |
    |       printf("digraph FSM {\n");                                       |
    |       TRANSITIONS(X)                                                   |
    |       printf("}\n");                                                   |
    |   }                                                                    |
    |   #undef X                                                             |
    |                                                                        |
    |   Output (paste into Graphviz):                                        |
    |   +----------------------------------------------------------+        |
    |   | digraph FSM {                                            |        |
    |   |   CLOSED -> CONNECTING [label="CONNECT\nact_connect"];   |        |
    |   |   CONNECTING -> CONNECTED [label="CONNECTED_ACK\nact_ok"];|       |
    |   |   CONNECTING -> CLOSED [label="TIMEOUT\nact_timeout"];   |        |
    |   |   ...                                                     |        |
    |   | }                                                         |        |
    |   +----------------------------------------------------------+        |
    |                                                                        |
    +------------------------------------------------------------------------+
```

### 中文说明

**文件组织**：

1. **fsm_spec.h**（规格说明）：
   - 人类可读的 FSM 定义
   - 包含 `STATES(X)`、`EVENTS(X)`、`TRANSITIONS(X)` 宏
   - 这是唯一需要修改的地方

2. **fsm_generated.c**（生成代码）：
   - 包含 `fsm_spec.h`
   - 用不同的 X 定义生成枚举、表格等
   - 不应手动编辑

3. **fsm_engine.c**（引擎）：
   - 与表驱动方案相同的查表引擎
   - 与 FSM 规格无关

**宏展开过程**：
- 预处理器将 `TRANSITIONS(X)` 展开为具体的表格初始化器
- 可以用 `gcc -E` 查看展开后的代码

**生成文档**：
- 同一个 `TRANSITIONS(X)` 宏可以用来生成 Graphviz DOT 格式
- 保证文档与代码永远同步

---

## Complete C Example

```c
/******************************************************************************
 * FILE: connection_fsm_macro_dsl.c
 * 
 * DESCRIPTION:
 *   Complete example of Macro-Based DSL FSM for a connection state machine.
 *   Uses X-macro pattern for maximum declarativeness.
 *   Compile: gcc -o fsm_macro connection_fsm_macro_dsl.c
 *   Run: ./fsm_macro
 *
 * WHY MACRO DSL:
 *   - FSM specification reads like a state diagram
 *   - Single source of truth: enums, tables, docs generated from one place
 *   - Easy to audit: all transitions visible in TRANSITIONS macro
 *   - Can generate Graphviz diagrams for visualization
 *
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/*============================================================================
 * PART 1: X-MACRO SPECIFICATIONS (THE DSL)
 *
 * KEY DESIGN POINT:
 *   This section IS the state machine specification.
 *   Everything else is generated from these macros.
 *   When you modify the FSM, only change this section.
 *============================================================================*/

/*
 * STATES: List all states in the FSM
 * X(name) will be expanded differently depending on use case
 */
#define STATES(X)       \
    X(CLOSED)           \
    X(CONNECTING)       \
    X(CONNECTED)        \
    X(DISCONNECTING)

/*
 * EVENTS: List all events that can be dispatched to the FSM
 */
#define EVENTS(X)           \
    X(CONNECT)              \
    X(CONNECTED_ACK)        \
    X(DISCONNECT)           \
    X(DISCONNECTED_ACK)     \
    X(TIMEOUT)              \
    X(ERROR)

/*
 * TRANSITIONS: THE CORE SPECIFICATION
 * 
 * KEY DESIGN: This is a human-readable state diagram in code form.
 *
 * Format: X(FROM_STATE, EVENT, TO_STATE, ACTION)
 * 
 * Read as: "When in FROM_STATE and EVENT occurs, 
 *           transition to TO_STATE and execute ACTION"
 *
 * NOTE: Undefined combinations will use default (stay in current state, ignore)
 */
#define TRANSITIONS(X)                                                  \
    /* STATE_CLOSED: Starting state, waiting for connection request */  \
    X(CLOSED,       CONNECT,        CONNECTING,     act_start_connect)  \
    X(CLOSED,       DISCONNECT,     CLOSED,         act_ignore)         \
    X(CLOSED,       TIMEOUT,        CLOSED,         act_ignore)         \
    X(CLOSED,       ERROR,          CLOSED,         act_ignore)         \
                                                                        \
    /* STATE_CONNECTING: Connection attempt in progress */              \
    X(CONNECTING,   CONNECTED_ACK,  CONNECTED,      act_connected)      \
    X(CONNECTING,   DISCONNECT,     CLOSED,         act_abort_connect)  \
    X(CONNECTING,   TIMEOUT,        CLOSED,         act_connect_timeout)\
    X(CONNECTING,   ERROR,          CLOSED,         act_connect_error)  \
    X(CONNECTING,   CONNECT,        CONNECTING,     act_ignore)         \
                                                                        \
    /* STATE_CONNECTED: Connection established */                       \
    X(CONNECTED,    DISCONNECT,     DISCONNECTING,  act_start_disconnect) \
    X(CONNECTED,    ERROR,          CLOSED,         act_connection_lost)\
    X(CONNECTED,    CONNECT,        CONNECTED,      act_ignore)         \
    X(CONNECTED,    TIMEOUT,        CONNECTED,      act_ignore)         \
                                                                        \
    /* STATE_DISCONNECTING: Graceful disconnect in progress */          \
    X(DISCONNECTING, DISCONNECTED_ACK, CLOSED,      act_disconnected)   \
    X(DISCONNECTING, TIMEOUT,       CLOSED,         act_force_close)    \
    X(DISCONNECTING, ERROR,         CLOSED,         act_force_close)    \
    X(DISCONNECTING, CONNECT,       DISCONNECTING,  act_ignore)         \
    X(DISCONNECTING, DISCONNECT,    DISCONNECTING,  act_ignore)

/*============================================================================
 * PART 2: GENERATE ENUMS FROM X-MACROS
 *
 * KEY DESIGN POINT:
 *   Enums are ALWAYS in sync with the STATES/EVENTS macros.
 *   Add a new state to STATES(X), enum is automatically updated.
 *============================================================================*/

/* Generate State enum */
#define X(name) STATE_##name,
typedef enum {
    STATES(X)
    STATE_COUNT  /* Must be last */
} ConnState;
#undef X

/* Generate Event enum */
#define X(name) EVENT_##name,
typedef enum {
    EVENTS(X)
    EVENT_COUNT  /* Must be last */
} ConnEvent;
#undef X

/*============================================================================
 * PART 3: GENERATE NAME STRINGS FROM X-MACROS
 *
 * KEY DESIGN POINT:
 *   Name arrays are ALWAYS in sync with enums.
 *   No risk of mismatch between enum value and its name.
 *============================================================================*/

/* Generate state name strings */
#define X(name) [STATE_##name] = #name,
static const char* state_names[STATE_COUNT] = {
    STATES(X)
};
#undef X

/* Generate event name strings */
#define X(name) [EVENT_##name] = #name,
static const char* event_names[EVENT_COUNT] = {
    EVENTS(X)
};
#undef X

/*============================================================================
 * PART 4: TYPE DEFINITIONS
 *============================================================================*/

struct ConnFSM;

/* Action function type */
typedef int (*ActionFunc)(struct ConnFSM *fsm, ConnEvent event);

/* Transition entry */
typedef struct {
    ConnState   next_state;
    ActionFunc  action;
    const char* action_name;  /* Auto-generated from macro stringification */
} Transition;

/* FSM instance */
typedef struct ConnFSM {
    ConnState   state;
    int         retry_count;
    int         last_error;
    const char* name;
    void*       user_data;
} ConnFSM;

/*============================================================================
 * PART 5: ACTION FUNCTION IMPLEMENTATIONS
 *============================================================================*/

/* Forward declarations of all action functions */
static int act_start_connect(ConnFSM *fsm, ConnEvent event);
static int act_connected(ConnFSM *fsm, ConnEvent event);
static int act_start_disconnect(ConnFSM *fsm, ConnEvent event);
static int act_disconnected(ConnFSM *fsm, ConnEvent event);
static int act_abort_connect(ConnFSM *fsm, ConnEvent event);
static int act_connect_timeout(ConnFSM *fsm, ConnEvent event);
static int act_connect_error(ConnFSM *fsm, ConnEvent event);
static int act_connection_lost(ConnFSM *fsm, ConnEvent event);
static int act_force_close(ConnFSM *fsm, ConnEvent event);
static int act_ignore(ConnFSM *fsm, ConnEvent event);

/* Implementation */
static int act_start_connect(ConnFSM *fsm, ConnEvent event)
{
    (void)event;
    fsm->retry_count = 0;
    printf("  [ACTION] %s: Starting connection...\n", fsm->name);
    return 0;
}

static int act_connected(ConnFSM *fsm, ConnEvent event)
{
    (void)event;
    printf("  [ACTION] %s: Connection established!\n", fsm->name);
    return 0;
}

static int act_start_disconnect(ConnFSM *fsm, ConnEvent event)
{
    (void)event;
    printf("  [ACTION] %s: Starting graceful disconnect...\n", fsm->name);
    return 0;
}

static int act_disconnected(ConnFSM *fsm, ConnEvent event)
{
    (void)event;
    fsm->retry_count = 0;
    fsm->last_error = 0;
    printf("  [ACTION] %s: Disconnected, cleanup complete\n", fsm->name);
    return 0;
}

static int act_abort_connect(ConnFSM *fsm, ConnEvent event)
{
    (void)event;
    printf("  [ACTION] %s: Connection attempt aborted\n", fsm->name);
    return 0;
}

static int act_connect_timeout(ConnFSM *fsm, ConnEvent event)
{
    (void)event;
    fsm->retry_count++;
    fsm->last_error = -1;
    printf("  [ACTION] %s: Connection timeout (retry=%d)\n", 
           fsm->name, fsm->retry_count);
    return -1;
}

static int act_connect_error(ConnFSM *fsm, ConnEvent event)
{
    (void)event;
    fsm->last_error = -2;
    printf("  [ACTION] %s: Connection error\n", fsm->name);
    return -1;
}

static int act_connection_lost(ConnFSM *fsm, ConnEvent event)
{
    (void)event;
    fsm->last_error = -3;
    printf("  [ACTION] %s: Connection lost unexpectedly!\n", fsm->name);
    return -1;
}

static int act_force_close(ConnFSM *fsm, ConnEvent event)
{
    (void)event;
    printf("  [ACTION] %s: Forcing connection close\n", fsm->name);
    return 0;
}

static int act_ignore(ConnFSM *fsm, ConnEvent event)
{
    printf("  [ACTION] %s: Ignoring event %s\n", fsm->name, event_names[event]);
    return 0;
}

/*============================================================================
 * PART 6: GENERATE TRANSITION TABLE FROM TRANSITIONS MACRO
 *
 * KEY DESIGN POINT:
 *   This is where the magic happens.
 *   The X macro is defined to generate table initializers.
 *   TRANSITIONS(X) expands to the entire table contents.
 *
 *   #action stringifies the action name for logging/debugging.
 *============================================================================*/

/* Default transition for unspecified combinations */
static const Transition default_transition = { 
    .next_state = STATE_COUNT,  /* Invalid, means "stay in current state" */
    .action = act_ignore, 
    .action_name = "act_ignore (default)" 
};

/* 
 * KEY: Define X to generate one table entry
 * 
 * Syntax: [STATE_from][EVENT_event] = { STATE_to, action, "action" }
 * 
 * The ## operator concatenates tokens (STATE_ + CLOSED = STATE_CLOSED)
 * The # operator stringifies the action name
 */
#define X(from, event, to, action) \
    [STATE_##from][EVENT_##event] = { STATE_##to, action, #action },

/* 
 * Generate the entire transition table!
 * 
 * This single line expands to all the table initializers
 * defined in the TRANSITIONS macro above.
 */
static const Transition transition_table[STATE_COUNT][EVENT_COUNT] = {
    TRANSITIONS(X)
};
#undef X

/*============================================================================
 * PART 7: FSM ENGINE
 *============================================================================*/

void fsm_init(ConnFSM *fsm, const char *name, void *user_data)
{
    memset(fsm, 0, sizeof(*fsm));
    fsm->state = STATE_CLOSED;
    fsm->name = name;
    fsm->user_data = user_data;
    printf("[FSM] %s: Initialized to state %s\n", name, state_names[fsm->state]);
}

int fsm_dispatch(ConnFSM *fsm, ConnEvent event)
{
    const Transition *trans;
    ConnState old_state;
    int result = 0;

    if (fsm == NULL || event >= EVENT_COUNT) {
        printf("[FSM] ERROR: Invalid fsm or event\n");
        return -1;
    }

    trans = &transition_table[fsm->state][event];
    old_state = fsm->state;

    /* Check if this is a valid transition (has action function) */
    if (trans->action == NULL) {
        /* Use default transition */
        trans = &default_transition;
    }

    printf("[FSM] %s: %s + %s -> %s (%s)\n",
           fsm->name, 
           state_names[old_state], 
           event_names[event],
           (trans->next_state < STATE_COUNT) ? state_names[trans->next_state] : "(stay)",
           trans->action_name);

    /* Execute action */
    if (trans->action != NULL) {
        result = trans->action(fsm, event);
    }

    /* Update state (if valid next_state specified) */
    if (trans->next_state < STATE_COUNT) {
        fsm->state = trans->next_state;
    }
    /* else: stay in current state */

    if (old_state != fsm->state) {
        printf("[FSM] %s: State changed: %s -> %s\n",
               fsm->name, state_names[old_state], state_names[fsm->state]);
    }

    return result;
}

const char* fsm_get_state_name(const ConnFSM *fsm)
{
    return state_names[fsm->state];
}

/*============================================================================
 * PART 8: DOCUMENTATION GENERATOR (Bonus!)
 *
 * KEY DESIGN POINT:
 *   The same TRANSITIONS macro can generate Graphviz DOT output.
 *   Paste the output into https://dreampuf.github.io/GraphvizOnline/
 *   to visualize your state machine!
 *============================================================================*/

void generate_graphviz_dot(void)
{
    printf("\n=== GRAPHVIZ DOT OUTPUT ===\n");
    printf("(Paste into https://dreampuf.github.io/GraphvizOnline/)\n\n");
    printf("digraph ConnectionFSM {\n");
    printf("  rankdir=LR;\n");
    printf("  node [shape=box];\n");
    printf("  CLOSED [shape=ellipse, style=bold];\n");
    printf("\n");

    /* KEY: Redefine X to generate DOT edges */
    #define X(from, event, to, action) \
        printf("  %s -> %s [label=\"%s\\n(%s)\"];\n", #from, #to, #event, #action);
    TRANSITIONS(X)
    #undef X

    printf("}\n");
    printf("\n=== END DOT OUTPUT ===\n\n");
}

/*============================================================================
 * PART 9: TRANSITION LIST PRINTER (Bonus!)
 *
 * KEY DESIGN POINT:
 *   Another use of the same TRANSITIONS macro - print a text table.
 *============================================================================*/

void print_transition_list(void)
{
    printf("\n=== TRANSITION LIST ===\n");
    printf("%-15s %-15s %-15s %s\n", "FROM", "EVENT", "TO", "ACTION");
    printf("%-15s %-15s %-15s %s\n", "----", "-----", "--", "------");

    #define X(from, event, to, action) \
        printf("%-15s %-15s %-15s %s\n", #from, #event, #to, #action);
    TRANSITIONS(X)
    #undef X

    printf("=== END LIST ===\n\n");
}

/*============================================================================
 * PART 10: DEMONSTRATION
 *============================================================================*/

int main(void)
{
    ConnFSM conn;

    printf("\n========== MACRO DSL FSM DEMO ==========\n\n");

    /* Print auto-generated documentation */
    print_transition_list();

    /* Initialize FSM */
    fsm_init(&conn, "Connection1", NULL);

    printf("\n--- Scenario 1: Successful Connection ---\n");
    fsm_dispatch(&conn, EVENT_CONNECT);
    fsm_dispatch(&conn, EVENT_CONNECTED_ACK);

    printf("\n--- Scenario 2: Graceful Disconnect ---\n");
    fsm_dispatch(&conn, EVENT_DISCONNECT);
    fsm_dispatch(&conn, EVENT_DISCONNECTED_ACK);

    printf("\n--- Scenario 3: Connection Timeout ---\n");
    fsm_dispatch(&conn, EVENT_CONNECT);
    fsm_dispatch(&conn, EVENT_TIMEOUT);

    printf("\n--- Scenario 4: Connection Error ---\n");
    fsm_dispatch(&conn, EVENT_CONNECT);
    fsm_dispatch(&conn, EVENT_ERROR);

    printf("\n========== DEMO COMPLETE ==========\n");
    printf("Final state: %s\n", fsm_get_state_name(&conn));

    /* Generate Graphviz diagram */
    generate_graphviz_dot();

    return 0;
}
```

---

## Key Takeaways

```
+====================================================================================+
|                          MACRO DSL FSM SUMMARY                                     |
+====================================================================================+

    Strengths:
    +------------------------------------------------------------------------+
    | ✓ Maximum declarativeness: TRANSITIONS macro reads like a state diagram|
    | ✓ Single source of truth: enums, tables, docs generated from one place |
    | ✓ Auto-sync: impossible for names/enums/table to get out of sync       |
    | ✓ Documentation: can generate Graphviz, markdown, etc. from same source|
    | ✓ Auditable: all transitions visible in one TRANSITIONS macro          |
    +------------------------------------------------------------------------+

    Weaknesses:
    +------------------------------------------------------------------------+
    | ✗ Macro complexity: team must understand X-macro pattern               |
    | ✗ Debugging: preprocessor errors can be cryptic                        |
    | ✗ Limited flexibility: complex guards don't fit the pattern well       |
    | ✗ IDE support: code navigation may not work well with macros           |
    +------------------------------------------------------------------------+

    Best Practices:
    +------------------------------------------------------------------------+
    | 1. Keep action functions simple - complex logic goes elsewhere         |
    | 2. Use consistent naming: STATE_X, EVENT_X, act_xxx                    |
    | 3. Document the TRANSITIONS macro format clearly                       |
    | 4. Use gcc -E to debug macro expansion issues                          |
    | 5. Consider generating documentation as part of build process          |
    +------------------------------------------------------------------------+

    When to Choose Macro DSL:
    +------------------------------------------------------------------------+
    | - FSM structure is relatively stable                                   |
    | - Team is comfortable with C macros                                    |
    | - Need to generate documentation from code                             |
    | - Want maximum visibility of all transitions                           |
    | - Value declarative style over flexibility                             |
    +------------------------------------------------------------------------+

    X-Macro Pattern Summary:
    +------------------------------------------------------------------------+
    |                                                                        |
    |   1. Define data: #define ITEMS(X)  X(a) X(b) X(c)                     |
    |   2. Use data:    #define X(name) ... generate something ...           |
    |                   ITEMS(X)                                             |
    |                   #undef X                                             |
    |   3. Reuse data:  #define X(name) ... generate something else ...      |
    |                   ITEMS(X)                                             |
    |                   #undef X                                             |
    |                                                                        |
    +------------------------------------------------------------------------+
```

### 中文说明

**宏DSL FSM的核心优势**：

1. **最大声明性**：`TRANSITIONS` 宏读起来就像状态图
2. **单一事实来源**：枚举、表格、文档都从同一个宏生成
3. **自动同步**：名称、枚举值、转换表不可能不同步
4. **可审计性**：所有转换在一个宏中清晰可见

**宏DSL的局限**：
1. **宏复杂性**：团队必须理解 X-Macro 模式
2. **调试困难**：预处理器错误信息可能难以理解
3. **灵活性有限**：复杂的守卫条件不太适合这种模式
4. **IDE支持差**：代码导航可能无法穿透宏

**X-Macro 模式总结**：
1. 定义数据：`#define ITEMS(X) X(a) X(b) X(c)`
2. 使用数据：定义 X 为某种代码生成器，展开 `ITEMS(X)`
3. 复用数据：用不同的 X 定义，再次展开 `ITEMS(X)`

**适用场景**：
- FSM 结构相对稳定
- 团队熟悉 C 宏技术
- 需要从代码生成文档
- 追求转换规则的最大可见性
