# WHAT | Core FSM Concepts (Zero Distractions)

## 1. Fundamental Elements

```
THE FIVE ELEMENTS OF AN FSM
===========================

+-------------------------------------------------------------------+
|                                                                   |
|    +----------+    EVENT     +----------+                         |
|    |  STATE   | -----------> |  STATE   |                         |
|    |    A     |  [GUARD]     |    B     |                         |
|    +----------+   /ACTION    +----------+                         |
|         ^                                                         |
|         |                                                         |
|    EXACTLY ONE                                                    |
|    STATE ACTIVE                                                   |
|                                                                   |
+-------------------------------------------------------------------+

1. STATE    = "Where the system is right now"
2. EVENT    = "What just happened" (external fact)
3. TRANSITION = "Legal path from state A to state B"
4. ACTION   = "What to do when transitioning"
5. GUARD    = "Condition that must be true for transition"
```

```
ELEMENT 1: STATE
================

Definition: A distinct mode of operation

Properties:
+------------------------------------------------+
| - Mutually exclusive (only ONE active)         |
| - Named after a condition, not an action       |
| - Persistent until explicitly changed          |
| - Observable (you can ask "what state?")       |
+------------------------------------------------+

Good State Names:          Bad State Names:
  IDLE                       WAITING_FOR_DATA  (too specific)
  CONNECTING                 DO_CONNECT        (verb = action, not state)
  AUTHENTICATED              CHECK_AUTH        (verb = action)
  ERROR                      HANDLE_ERROR      (verb = action)
  CLOSED                     CLOSING_NOW       (too transient)

In C:
  typedef enum {
      STATE_IDLE,
      STATE_CONNECTING,
      STATE_CONNECTED,
      STATE_ERROR,
      STATE_CLOSED
  } state_t;
```

```
ELEMENT 2: EVENT
================

Definition: An external fact that has occurred

Properties:
+------------------------------------------------+
| - Past tense ("connection established")        |
| - External to FSM (FSM doesn't generate them)  |
| - Instantaneous (happened, not happening)      |
| - Cannot be "undone"                           |
+------------------------------------------------+

Good Event Names:          Bad Event Names:
  CONNECT_REQUESTED          DO_CONNECT        (command, not fact)
  DATA_RECEIVED              READ_DATA         (command)
  TIMEOUT_EXPIRED            WAIT              (command)
  ERROR_OCCURRED             HANDLE_ERROR      (command)
  CLOSE_REQUESTED            PLEASE_CLOSE      (request ≠ fact)

In C:
  typedef enum {
      EVENT_CONNECT_REQUESTED,
      EVENT_CONNECTED,
      EVENT_DATA_RECEIVED,
      EVENT_TIMEOUT,
      EVENT_ERROR,
      EVENT_CLOSE_REQUESTED
  } event_t;
```

```
ELEMENT 3: TRANSITION
=====================

Definition: A legal state change triggered by an event

Visual:
         EVENT_X
  [A] --------------> [B]
       
       "When in state A, if event X occurs,
        move to state B"

Properties:
+------------------------------------------------+
| - Explicit (listed in transition table)        |
| - Deterministic (same input = same output)     |
| - Atomic (no "halfway" transitions)            |
| - Legal transitions are WHITELIST, not blacklist|
+------------------------------------------------+

In C (conceptual):
  struct transition {
      state_t  from_state;
      event_t  event;
      state_t  to_state;
      action_t action;
  };
```

```
ELEMENT 4: ACTION
=================

Definition: Side effect executed during transition

Visual:
         EVENT_X
  [A] --------------> [B]
            |
            v
        action()

Properties:
+------------------------------------------------+
| - Executed AFTER state change decision         |
| - Should not affect transition decision        |
| - Can fail (but state has already changed!)    |
| - Optional (some transitions have no action)   |
+------------------------------------------------+

Action Types:
  - Entry action:  Executed when entering a state
  - Exit action:   Executed when leaving a state
  - Transition action: Executed during specific transition

In C:
  typedef int (*action_fn)(fsm_t *fsm, void *context);
  
  int action_start_connection(fsm_t *fsm, void *ctx) {
      conn_t *c = (conn_t *)ctx;
      return socket_connect(c->host, c->port);
  }
```

```
ELEMENT 5: GUARD
================

Definition: Boolean condition that enables/disables a transition

Visual:
         EVENT_X [retry_count < 3]
  [A] -----------------------------> [B]
  
         EVENT_X [retry_count >= 3]
  [A] -----------------------------> [FAILED]

Properties:
+------------------------------------------------+
| - Evaluated BEFORE transition decision         |
| - Pure function (no side effects)              |
| - Based on FSM context, not external state     |
| - Should be simple and fast                    |
+------------------------------------------------+

In C:
  typedef bool (*guard_fn)(fsm_t *fsm, void *context);
  
  bool guard_can_retry(fsm_t *fsm, void *ctx) {
      conn_t *c = (conn_t *)ctx;
      return c->retry_count < MAX_RETRIES;
  }
```

---

## 2. Minimal FSM Model

```
THE SIMPLEST POSSIBLE FSM
=========================

Requirements:
  - One FSM
  - One source file
  - No I/O
  - No timers
  - No concurrency
  - No dynamic allocation

Example: Door Lock FSM

States:     LOCKED, UNLOCKED
Events:     KEY_TURN, HANDLE_PULL
Transitions:
  LOCKED   + KEY_TURN    -> UNLOCKED
  UNLOCKED + KEY_TURN    -> LOCKED
  UNLOCKED + HANDLE_PULL -> UNLOCKED (door opens, but stays unlocked)
  LOCKED   + HANDLE_PULL -> LOCKED   (nothing happens)
```

```c
/* door_fsm.c - Minimal FSM Example */

#include <stdio.h>
#include <assert.h>

/* States */
typedef enum {
    STATE_LOCKED,
    STATE_UNLOCKED,
    STATE_COUNT  /* Not a real state, just for array sizing */
} state_t;

/* Events */
typedef enum {
    EVENT_KEY_TURN,
    EVENT_HANDLE_PULL,
    EVENT_COUNT  /* Not a real event, just for array sizing */
} event_t;

/* The FSM context */
typedef struct {
    state_t current_state;
} door_fsm_t;

/* State names for debugging */
static const char *state_names[] = {
    [STATE_LOCKED]   = "LOCKED",
    [STATE_UNLOCKED] = "UNLOCKED"
};

/* Event names for debugging */
static const char *event_names[] = {
    [EVENT_KEY_TURN]    = "KEY_TURN",
    [EVENT_HANDLE_PULL] = "HANDLE_PULL"
};

/* Transition table: next_state[current_state][event] */
static const state_t transitions[STATE_COUNT][EVENT_COUNT] = {
    /*                    KEY_TURN        HANDLE_PULL     */
    [STATE_LOCKED]   = { STATE_UNLOCKED, STATE_LOCKED   },
    [STATE_UNLOCKED] = { STATE_LOCKED,   STATE_UNLOCKED }
};

/* Initialize FSM */
void door_fsm_init(door_fsm_t *fsm) {
    fsm->current_state = STATE_LOCKED;
}

/* Process an event */
void door_fsm_event(door_fsm_t *fsm, event_t event) {
    assert(fsm->current_state < STATE_COUNT);
    assert(event < EVENT_COUNT);
    
    state_t old_state = fsm->current_state;
    state_t new_state = transitions[old_state][event];
    
    printf("Event: %s | State: %s -> %s\n",
           event_names[event],
           state_names[old_state],
           state_names[new_state]);
    
    fsm->current_state = new_state;
}

/* Query current state */
state_t door_fsm_state(const door_fsm_t *fsm) {
    return fsm->current_state;
}

/* Example usage */
int main(void) {
    door_fsm_t door;
    door_fsm_init(&door);
    
    door_fsm_event(&door, EVENT_HANDLE_PULL);  /* LOCKED -> LOCKED */
    door_fsm_event(&door, EVENT_KEY_TURN);     /* LOCKED -> UNLOCKED */
    door_fsm_event(&door, EVENT_HANDLE_PULL);  /* UNLOCKED -> UNLOCKED */
    door_fsm_event(&door, EVENT_KEY_TURN);     /* UNLOCKED -> LOCKED */
    
    return 0;
}
```

```
OUTPUT:
Event: HANDLE_PULL | State: LOCKED -> LOCKED
Event: KEY_TURN | State: LOCKED -> UNLOCKED
Event: HANDLE_PULL | State: UNLOCKED -> UNLOCKED
Event: KEY_TURN | State: UNLOCKED -> LOCKED
```

---

## 3. Mental Invariants

```
INVARIANT 1: Exactly One State Is Active
========================================

At ANY point in time:
  - current_state has exactly ONE valid value
  - No "between states"
  - No "multiple states"
  - No "undefined state"

                 WRONG                          RIGHT
          +------------------+            +------------------+
          | state = A        |            | state = A        |
          | also_state = B   |            |                  |
          | in_transition = 1|            | /* ONE value */  |
          +------------------+            +------------------+


INVARIANT 2: Events Are External Facts, Not Commands
====================================================

Events describe what HAS happened, not what SHOULD happen.

    WRONG (Imperative):              RIGHT (Declarative):
    
    EVENT_CONNECT                    EVENT_CONNECT_REQUESTED
    EVENT_SEND_DATA                  EVENT_CONNECTION_ESTABLISHED
    EVENT_CLOSE                      EVENT_DATA_ARRIVED
                                     EVENT_CLOSE_REQUESTED

Why? Because:
  - The FSM doesn't "do" things, it "responds" to things
  - Events come from outside the FSM
  - The FSM decides what happens, not the event


INVARIANT 3: Transitions Define Legality
========================================

If a transition is NOT in the table, it is ILLEGAL.

    Transition Table (Whitelist):
    +--------+----------+------------+
    | State  | Event    | Next State |
    +--------+----------+------------+
    | IDLE   | START    | RUNNING    |
    | RUNNING| STOP     | IDLE       |
    | RUNNING| PAUSE    | PAUSED     |
    | PAUSED | RESUME   | RUNNING    |
    +--------+----------+------------+

    Illegal (not in table):
      IDLE + STOP      -> ???  (Cannot stop what isn't running)
      IDLE + PAUSE     -> ???  (Cannot pause what isn't running)
      PAUSED + START   -> ???  (Already started)

    These are NOT "handled specially" - they CANNOT HAPPEN
    in a correctly functioning system.
```

```
THE FSM CONTRACT
================

+----------------------------------------------------------------+
|                                                                |
|  Given: state S and event E                                    |
|                                                                |
|  If (S, E) is in transition table:                             |
|      -> Transition to next_state                               |
|      -> Execute associated action (if any)                     |
|                                                                |
|  If (S, E) is NOT in transition table:                         |
|      -> THIS IS A BUG (assert, log, or reject)                 |
|      -> The system is in an unexpected situation               |
|      -> Do NOT silently ignore                                 |
|                                                                |
+----------------------------------------------------------------+
```

---

## Visual Summary: What an FSM IS and IS NOT

```
+---------------------------+---------------------------+
|      FSM IS               |      FSM IS NOT           |
+---------------------------+---------------------------+
| A decision maker          | A task executor           |
| A state tracker           | A scheduler               |
| A legality enforcer       | An event source           |
| A deterministic system    | An async processor        |
| A single point of truth   | A distributed system      |
+---------------------------+---------------------------+

FSM Scope:
+-----------------------------------------------------------+
|                                                           |
|  Events (input)                                           |
|       |                                                   |
|       v                                                   |
|  +------------------+                                     |
|  |       FSM        |  <- Knows: current state            |
|  |                  |  <- Knows: legal transitions        |
|  |  [State]         |  <- Decides: next state             |
|  |  [Transitions]   |  <- Triggers: action                |
|  +------------------+                                     |
|       |                                                   |
|       v                                                   |
|  Actions (output)                                         |
|                                                           |
+-----------------------------------------------------------+
|  FSM does NOT know:                                       |
|  - Where events come from                                 |
|  - How actions are implemented                            |
|  - What time it is                                        |
|  - What other systems are doing                           |
+-----------------------------------------------------------+
```

---

**中文解释（Chinese Explanation）**

**状态机的五个基本元素**

1. **状态（State）**：系统当前所处的模式。关键特性：互斥（任意时刻只有一个状态激活）、用名词命名（如 IDLE、CONNECTED）而非动词。

2. **事件（Event）**：已经发生的外部事实。使用过去时态命名（如 DATA_RECEIVED）而非命令式（如 READ_DATA）。事件是外部的，FSM不产生事件，只响应事件。

3. **转换（Transition）**：从状态A到状态B的合法路径。转换是显式定义的白名单——如果不在转换表中，就是非法的。

4. **动作（Action）**：转换过程中执行的副作用。动作在状态转换决策之后执行，不应影响转换决策本身。

5. **守卫（Guard）**：启用或禁用转换的布尔条件。在转换决策之前评估，必须是纯函数（无副作用）。

**最小FSM模型**

示例代码展示了一个门锁FSM：
- 两个状态：LOCKED、UNLOCKED
- 两个事件：KEY_TURN、HANDLE_PULL
- 使用二维数组作为转换表
- 无I/O、无定时器、无并发

**心智不变量**

1. **只有一个状态激活**：`current_state` 任何时刻都有且仅有一个有效值，没有"中间状态"或"多状态"。

2. **事件是事实，不是命令**：事件描述"已经发生什么"，而非"应该做什么"。FSM不"执行"，它"响应"。

3. **转换定义合法性**：转换表是白名单。不在表中的转换是非法的，代表系统错误，不应被静默忽略。

**FSM的本质**

FSM是：决策者、状态追踪器、合法性执行者、确定性系统、单一事实源。

FSM不是：任务执行器、调度器、事件源、异步处理器、分布式系统。

FSM的职责边界很清晰：它只知道当前状态和合法转换，不关心事件从哪来、动作如何实现、现在几点、其他系统在做什么。
