# SCALE | Table-Driven FSM Design

## 1. Why Switch-Case FSMs Fail

```
THE SWITCH-CASE FSM ANTI-PATTERN
================================

Typical "beginner" FSM:

int fsm_handle(fsm_t *f, event_t e) {
    switch (f->state) {
        case STATE_IDLE:
            switch (e) {
                case EVENT_CONNECT:
                    f->state = STATE_CONNECTING;
                    start_connect(f);
                    break;
                case EVENT_DATA:
                    /* Ignore in IDLE */      // Hidden transition!
                    break;
                default:
                    /* What goes here? */     // Undefined behavior!
                    break;
            }
            break;
        case STATE_CONNECTING:
            switch (e) {
                case EVENT_CONNECTED:
                    f->state = STATE_CONNECTED;
                    on_connect(f);
                    break;
                case EVENT_ERROR:
                    f->state = STATE_ERROR;
                    on_error(f);
                    break;
                /* Missing EVENT_DATA case */ // Bug: silent ignore!
                /* Missing EVENT_TIMEOUT */   // Bug: hangs forever!
            }
            break;
        // ... continues for every state
    }
}
```

```
PROBLEM 1: SCALABILITY
======================

Lines of code for switch-case FSM:
  
  N states × M events = O(N×M) case statements
  
  Each case:
    - state switch entry
    - event switch entry  
    - transition logic
    - action call
    - break statements

Example growth:
  5 states × 4 events  = ~100 lines
  10 states × 8 events = ~400 lines
  20 states × 15 events = ~1500 lines

+----------------------------------------------------------+
| At 20+ states, the switch-case becomes UNMANAGEABLE      |
| - Hard to find specific transitions                      |
| - Easy to miss cases                                     |
| - Inconsistent handling across states                    |
+----------------------------------------------------------+
```

```
PROBLEM 2: READABILITY
======================

To answer "What happens in STATE_X when EVENT_Y occurs?"

Switch-case: Read 500 lines of code, find the right case

Table-driven: Look up table[STATE_X][EVENT_Y]


To answer "What events are valid in STATE_X?"

Switch-case: Read through STATE_X's entire switch block

Table-driven: Read one row of the table


To answer "What states can transition to STATE_Y?"

Switch-case: Grep entire file for "state = STATE_Y"

Table-driven: Scan column for STATE_Y in next_state
```

```
PROBLEM 3: HIDDEN TRANSITIONS
=============================

Switch-case hides transitions in three ways:

1. DEFAULT CASES
   switch (event) {
       case EVENT_A: ...; break;
       case EVENT_B: ...; break;
       default: /* What happens here? Hidden! */
   }

2. FALL-THROUGH
   switch (event) {
       case EVENT_A:
       case EVENT_B:  // Both handled same way - but intentional?
           do_something();
           break;
   }

3. MISSING CASES
   switch (event) {
       case EVENT_A: ...; break;
       // EVENT_B not listed - compiler warning if lucky
       // Silent ignore at runtime
   }

In table-driven FSM:
  - Every cell is explicit
  - No default behavior
  - Missing entries are compile-time errors (with proper macros)
```

---

## 2. Table-Driven Design

```
THE STATE × EVENT MATRIX
========================

         |  EVENT_1   |  EVENT_2   |  EVENT_3   |  EVENT_4   |
---------+------------+------------+------------+------------+
STATE_A  |  B / act1  |  ILLEGAL   |  A / act2  |  ILLEGAL   |
STATE_B  |  ILLEGAL   |  C / act3  |  ILLEGAL   |  D / act4  |
STATE_C  |  A / act5  |  ILLEGAL   |  C / act6  |  ILLEGAL   |
STATE_D  |  ILLEGAL   |  ILLEGAL   |  ILLEGAL   |  A / act7  |


Each cell contains:
  - Next state (or ILLEGAL)
  - Action to execute (or NULL)

This IS the specification.
This IS the implementation.
This IS the documentation.
```

```c
/* Table-Driven FSM Implementation */

/* === Type Definitions === */

typedef enum {
    STATE_IDLE,
    STATE_CONNECTING,
    STATE_CONNECTED,
    STATE_SENDING,
    STATE_ERROR,
    STATE_COUNT     /* Sentinel for array sizing */
} state_t;

typedef enum {
    EVENT_CONNECT_REQ,
    EVENT_CONNECTED,
    EVENT_SEND_REQ,
    EVENT_SEND_DONE,
    EVENT_ERROR,
    EVENT_CLOSE,
    EVENT_COUNT     /* Sentinel for array sizing */
} event_t;

typedef int (*action_fn)(void *ctx);

typedef struct {
    state_t   next_state;
    action_fn action;
} transition_t;


/* === Action Declarations === */

static int act_start_connect(void *ctx);
static int act_on_connected(void *ctx);
static int act_start_send(void *ctx);
static int act_on_send_done(void *ctx);
static int act_handle_error(void *ctx);
static int act_cleanup(void *ctx);


/* === The Transition Table === */

/* Marker for illegal transitions */
#define ILLEGAL { STATE_COUNT, NULL }
#define SELF(s) { s, NULL }  /* Stay in same state, no action */

static const transition_t fsm_table[STATE_COUNT][EVENT_COUNT] = {
/*                 CONNECT_REQ               CONNECTED                    SEND_REQ                   SEND_DONE                 ERROR                      CLOSE                */
[STATE_IDLE]      = {{STATE_CONNECTING, act_start_connect}, ILLEGAL,                    ILLEGAL,                   ILLEGAL,                  ILLEGAL,                   ILLEGAL               },
[STATE_CONNECTING]= {ILLEGAL,                               {STATE_CONNECTED, act_on_connected}, ILLEGAL,          ILLEGAL,                  {STATE_ERROR, act_handle_error}, {STATE_IDLE, act_cleanup}},
[STATE_CONNECTED] = {ILLEGAL,                               ILLEGAL,                    {STATE_SENDING, act_start_send}, ILLEGAL,            {STATE_ERROR, act_handle_error}, {STATE_IDLE, act_cleanup}},
[STATE_SENDING]   = {ILLEGAL,                               ILLEGAL,                    ILLEGAL,                   {STATE_CONNECTED, act_on_send_done}, {STATE_ERROR, act_handle_error}, {STATE_IDLE, act_cleanup}},
[STATE_ERROR]     = {ILLEGAL,                               ILLEGAL,                    ILLEGAL,                   ILLEGAL,                  ILLEGAL,                   {STATE_IDLE, act_cleanup}},
};


/* === FSM Engine === */

typedef struct {
    state_t state;
    void    *ctx;
} fsm_t;

int fsm_init(fsm_t *fsm, void *ctx) {
    fsm->state = STATE_IDLE;
    fsm->ctx = ctx;
    return 0;
}

int fsm_dispatch(fsm_t *fsm, event_t event) {
    /* Bounds check */
    if (event >= EVENT_COUNT) {
        return -1;
    }
    
    /* Table lookup */
    const transition_t *t = &fsm_table[fsm->state][event];
    
    /* Check for illegal transition */
    if (t->next_state == STATE_COUNT) {
        /* Log, assert, or return error */
        fprintf(stderr, "ILLEGAL: state=%d event=%d\n", 
                fsm->state, event);
        return -1;
    }
    
    /* Transition */
    state_t old_state = fsm->state;
    fsm->state = t->next_state;
    
    /* Execute action */
    int rc = 0;
    if (t->action != NULL) {
        rc = t->action(fsm->ctx);
    }
    
    return rc;
}

state_t fsm_state(const fsm_t *fsm) {
    return fsm->state;
}
```

```
DEFAULT ILLEGAL TRANSITIONS
===========================

The key insight: ILLEGAL is the default, not the exception.

With ILLEGAL as marker:
  - Unspecified transitions are caught at runtime
  - Adding new states requires explicit thought about all events
  - Adding new events requires explicit thought about all states

Alternative: Compile-time checking with static_assert

#define TRANSITION_COUNT (STATE_COUNT * EVENT_COUNT)

/* Ensure table is fully specified */
static_assert(
    sizeof(fsm_table) == sizeof(transition_t) * TRANSITION_COUNT,
    "FSM table size mismatch - missing transitions?"
);
```

```
EXPLICIT TRANSITION DECLARATIONS
================================

For maximum clarity, declare transitions individually:

/* Transition declaration macro */
#define T(from, event, to, action) \
    [from][event] = { to, action }

/* Default all to ILLEGAL first */
static transition_t fsm_table[STATE_COUNT][EVENT_COUNT] = {
    /* All ILLEGAL by default (zero-initialized = STATE_COUNT = ILLEGAL) */
};

/* Then explicitly set valid transitions */
static void init_transitions(void) {
    /* Connection flow */
    fsm_table[STATE_IDLE][EVENT_CONNECT_REQ] = 
        (transition_t){ STATE_CONNECTING, act_start_connect };
    
    fsm_table[STATE_CONNECTING][EVENT_CONNECTED] = 
        (transition_t){ STATE_CONNECTED, act_on_connected };
    
    fsm_table[STATE_CONNECTING][EVENT_ERROR] = 
        (transition_t){ STATE_ERROR, act_handle_error };
    
    /* ... etc ... */
}

/* Or use designated initializers (C99+) */
static const transition_t fsm_table[STATE_COUNT][EVENT_COUNT] = {
    T(STATE_IDLE,       EVENT_CONNECT_REQ, STATE_CONNECTING, act_start_connect),
    T(STATE_CONNECTING, EVENT_CONNECTED,   STATE_CONNECTED,  act_on_connected),
    T(STATE_CONNECTING, EVENT_ERROR,       STATE_ERROR,      act_handle_error),
    /* All other cells remain zero (ILLEGAL) */
};
```

---

## 3. Ownership and Lifecycle

```
WHO OWNS THE FSM TABLE?
=======================

Option 1: STATIC TABLE (Most Common)
+----------------------------------------------------------+
| - Defined at compile time                                |
| - Shared by all instances of this FSM type               |
| - Cannot be modified at runtime                          |
| - Lives in .rodata section                               |
+----------------------------------------------------------+

  static const transition_t table[...] = { ... };
  
  fsm_t fsm1;  /* Uses table */
  fsm_t fsm2;  /* Uses same table */


Option 2: INSTANCE TABLE (Per-FSM)
+----------------------------------------------------------+
| - Each FSM instance has its own table                    |
| - Can be customized per instance                         |
| - More memory, more flexibility                          |
| - Useful for plugin systems                              |
+----------------------------------------------------------+

  typedef struct {
      state_t state;
      void *ctx;
      transition_t *table;  /* Per-instance */
  } fsm_t;
  
  fsm_t fsm1 = { .table = table_variant_a };
  fsm_t fsm2 = { .table = table_variant_b };


Option 3: DYNAMIC TABLE (Runtime-Built)
+----------------------------------------------------------+
| - Table built at runtime                                 |
| - Can be modified while FSM runs (DANGEROUS)             |
| - Useful for scripted/configurable systems               |
| - Requires careful synchronization                       |
+----------------------------------------------------------+

  fsm_t *fsm_create(void) {
      fsm_t *f = malloc(sizeof(*f));
      f->table = malloc(sizeof(transition_t) * N * M);
      build_table(f->table, config);
      return f;
  }
```

```
STATIC VS DYNAMIC: WHEN TO USE EACH
===================================

USE STATIC TABLES when:
+----------------------------------------------------------+
| - FSM behavior is fixed at compile time                  |
| - All instances behave identically                       |
| - Maximum performance needed                             |
| - Behavior must be auditable                             |
+----------------------------------------------------------+

USE DYNAMIC TABLES when:
+----------------------------------------------------------+
| - Behavior configured at runtime                         |
| - Different instances need different behavior            |
| - FSM is part of a plugin/scripting system               |
| - Rapid prototyping / testing                            |
+----------------------------------------------------------+


Most C systems use STATIC TABLES:
  - Simpler
  - Faster (no indirection)
  - Safer (cannot corrupt table)
  - Debugger-friendly (symbols visible)
```

```
FSM LIFECYCLE
=============

+------------------+
|     CREATE       |  fsm_init(&fsm, context)
+--------+---------+
         |
         | state = INITIAL_STATE
         v
+------------------+
|      READY       |  FSM can receive events
+--------+---------+
         |
         | Events arrive...
         v
+------------------+
|     RUNNING      |  fsm_dispatch(&fsm, event)
|                  |  state transitions occur
+--------+---------+
         |
         | Terminal state reached
         | or explicit shutdown
         v
+------------------+
|    SHUTDOWN      |  fsm_shutdown(&fsm)
+--------+---------+
         |
         | Cleanup actions
         v
+------------------+
|    DESTROYED     |  fsm_destroy(&fsm)
+------------------+


Key points:
1. FSM context outlives events
2. FSM may have terminal states (can't recover)
3. Shutdown may be a state itself
4. Destroy cleans up resources
```

```c
/* Complete FSM Lifecycle Example */

typedef struct {
    state_t state;
    void    *ctx;
    bool    running;
} fsm_t;

/* Create and initialize */
int fsm_create(fsm_t *fsm, void *ctx) {
    memset(fsm, 0, sizeof(*fsm));
    fsm->state = STATE_IDLE;
    fsm->ctx = ctx;
    fsm->running = true;
    return 0;
}

/* Check if FSM can accept events */
bool fsm_is_running(const fsm_t *fsm) {
    return fsm->running;
}

/* Dispatch event to FSM */
int fsm_dispatch(fsm_t *fsm, event_t event) {
    if (!fsm->running) {
        return -1;  /* FSM is stopped */
    }
    
    const transition_t *t = &fsm_table[fsm->state][event];
    
    if (t->next_state == STATE_COUNT) {
        return -1;  /* Illegal transition */
    }
    
    fsm->state = t->next_state;
    
    /* Check for terminal state */
    if (fsm->state == STATE_TERMINATED) {
        fsm->running = false;
    }
    
    if (t->action) {
        return t->action(fsm->ctx);
    }
    
    return 0;
}

/* Graceful shutdown */
int fsm_shutdown(fsm_t *fsm) {
    if (fsm->running) {
        /* Trigger shutdown event */
        fsm_dispatch(fsm, EVENT_SHUTDOWN);
    }
    return 0;
}

/* Cleanup resources */
void fsm_destroy(fsm_t *fsm) {
    if (fsm->running) {
        fsm_shutdown(fsm);
    }
    /* ctx cleanup is caller's responsibility */
    memset(fsm, 0, sizeof(*fsm));
}
```

---

## Summary: Table-Driven Design

```
+----------------------------------------------------------+
|                SWITCH-CASE vs TABLE-DRIVEN                |
+----------------------------------------------------------+
|  Criterion      | Switch-Case | Table-Driven            |
+----------------------------------------------------------+
| Scalability     | O(N×M) code | O(1) code + data        |
| Readability     | Poor        | Excellent               |
| Completeness    | Easy to miss| Explicit gaps visible   |
| Maintainability | Hard        | Easy                    |
| Performance     | Similar     | Slightly better*        |
| Testability     | Hard        | Easy (test table)       |
+----------------------------------------------------------+
* Table lookup is O(1), branch prediction may vary

RECOMMENDATION:
  - < 3 states, < 3 events: switch-case OK
  - >= 3 states OR >= 3 events: USE TABLE-DRIVEN
```

---

**中文解释（Chinese Explanation）**

**为什么 switch-case FSM 失败**

1. **可扩展性差**：N 个状态 × M 个事件 = N×M 个 case 语句。20 个状态 × 15 个事件就需要约 1500 行代码，难以管理。

2. **可读性差**：要回答"状态 X 遇到事件 Y 会发生什么"，需要阅读大量代码。表驱动只需查 table[X][Y]。

3. **隐藏转换**：switch-case 通过 default 分支、fall-through、遗漏 case 隐藏转换逻辑，容易产生 bug。

**表驱动设计**

核心是 **状态 × 事件矩阵**：每个单元格包含下一状态和要执行的动作。这同时是规格说明、实现和文档。

关键设计：
- ILLEGAL 是默认值，不是例外
- 每个合法转换显式声明
- 使用编译时检查确保表完整

**所有权和生命周期**

1. **静态表**：编译时定义，所有实例共享，最常用
2. **实例表**：每个 FSM 实例有自己的表，适合插件系统
3. **动态表**：运行时构建，适合脚本化系统，需要小心同步

大多数 C 系统使用静态表：更简单、更快、更安全、调试友好。

FSM 生命周期：创建 → 就绪 → 运行 → 关闭 → 销毁。注意：
- FSM 上下文的生命周期长于单个事件
- 可能有终止状态（无法恢复）
- 关闭本身可能是一个状态
- 销毁时清理资源

**何时使用表驱动**：3 个以上状态或 3 个以上事件时，应该使用表驱动。
