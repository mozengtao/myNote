# HOW | States + Events + Transitions (Hands-On Foundation)

## 1. Designing States

```
STATE GRANULARITY: THE GOLDILOCKS PRINCIPLE
===========================================

Too Coarse (Too few states):
+------------------+
|    PROCESSING    |  <- What KIND of processing?
+------------------+     - Connecting? Reading? Writing?
                         - How do we know what to do next?

Too Fine (Too many states):
+-----------------------+
| READING_BYTE_1        |
| READING_BYTE_2        |  <- This is data, not state
| READING_BYTE_3        |     Use a counter instead
| ...                   |
+-----------------------+

Just Right:
+-------------+     +-------------+     +-------------+
| CONNECTING  | --> | READING_HDR | --> | READING_BODY|
+-------------+     +-------------+     +-------------+
                          
Each state represents a DISTINCT MODE OF OPERATION
with DIFFERENT VALID EVENTS
```

```
RULE: States Should Change Valid Events
=======================================

Ask: "What events make sense in this state?"

State: DISCONNECTED
  Valid events: CONNECT_REQUESTED
  Invalid: DATA_RECEIVED, SEND_REQUESTED (no connection!)

State: CONNECTED  
  Valid events: DATA_RECEIVED, SEND_REQUESTED, DISCONNECT_REQUESTED
  Invalid: CONNECT_REQUESTED (already connected!)

State: CLOSING
  Valid events: CLOSE_COMPLETE, TIMEOUT
  Invalid: SEND_REQUESTED (we're closing!)

If two states have IDENTICAL valid events,
they might be the same state with different data.
```

```
AVOIDING PSEUDO-STATES
======================

Pseudo-state: A "state" that exists only for bookkeeping

WRONG:
  +------------------+
  | WAITING_FOR_ACK  |  <- This is not a state,
  +------------------+     it's a pending operation

  +------------------+
  | RETRY_3          |  <- This is not a state,
  +------------------+     it's a counter value

RIGHT:
  State: SENDING
  Context: { awaiting_ack: true, retry_count: 3 }

  The STATE is "SENDING"
  The DATA is "which attempt, expecting what response"


Visual Test for Pseudo-States:
+-----------------------------------------------------+
| Can the system behave DIFFERENTLY based on          |
| which "state" it's in?                              |
|                                                     |
| YES -> It's a real state                            |
| NO  -> It's just data, store it elsewhere           |
+-----------------------------------------------------+
```

```
MAKING STATES MUTUALLY EXCLUSIVE
================================

Test: Can the system be in both states simultaneously?

WRONG:
  states: CONNECTED, AUTHENTICATED, READY
  
  Can be: CONNECTED and AUTHENTICATED? YES (problem!)
  Can be: AUTHENTICATED and READY? YES (problem!)
  
  These should be:
  - CONNECTED_UNAUTHENTICATED
  - CONNECTED_AUTHENTICATED
  - READY (implies connected and authenticated)

RIGHT:
  states: IDLE, CONNECTING, CONNECTED, AUTHENTICATING, READY, ERROR

  Progression:
  IDLE -> CONNECTING -> CONNECTED -> AUTHENTICATING -> READY
                |                          |
                v                          v
              ERROR                      ERROR

Each state is EXCLUSIVE - you cannot be "CONNECTING" AND "AUTHENTICATING"
```

---

## 2. Designing Events

```
EVENTS AS "THINGS THAT HAPPENED"
================================

Grammar Rule: Events should read as past tense or noun phrases

WRONG (Commands):                 RIGHT (Facts):
  CONNECT                          CONNECTION_REQUESTED
  READ_DATA                        DATA_RECEIVED
  TIMEOUT                          TIMEOUT_EXPIRED
  RETRY                            RETRY_TRIGGERED
  CLOSE                            CLOSE_REQUESTED

Why Past Tense?
  - Events describe what HAS happened
  - By the time FSM sees it, it already occurred
  - FSM cannot prevent events, only respond


Event as External Fact:
+----------------------------------------------------------+
|                                                          |
|  Network  -----> [ packet arrived ] -----> EVENT_DATA_RX |
|                                                          |
|  Timer    -----> [ timer fired ]    -----> EVENT_TIMEOUT |
|                                                          |
|  User     -----> [ button pressed ] -----> EVENT_CLOSE   |
|                                                          |
+----------------------------------------------------------+
     External World          Translation         FSM Event
```

```
AVOIDING COMMAND-STYLE EVENTS
=============================

Command-style events conflate "request" with "completion"

WRONG:
  Event: CONNECT
  
  Does this mean:
    a) "Please connect" (request)
    b) "Connection established" (completion)
    c) "Start connecting" (action trigger)
  
  AMBIGUOUS!

RIGHT:
  EVENT_CONNECT_REQUESTED   <- User/system wants to connect
  EVENT_CONNECT_STARTED     <- Connection attempt began
  EVENT_CONNECT_SUCCEEDED   <- Connection established
  EVENT_CONNECT_FAILED      <- Connection attempt failed

Now the FSM can distinguish:
  IDLE + CONNECT_REQUESTED -> CONNECTING (start attempt)
  CONNECTING + CONNECT_SUCCEEDED -> CONNECTED
  CONNECTING + CONNECT_FAILED -> ERROR
```

```
EXTERNAL VS INTERNAL EVENTS
===========================

External Events: Come from outside the FSM
  - User input
  - Network I/O
  - Timer expiration
  - Hardware interrupts
  - Other subsystems

Internal Events: Generated by FSM actions (USE SPARINGLY)
  - Action completion
  - Self-transitions

                    +----------+
  External Events   |          |
  ----------------->|   FSM    |
  (from world)      |          |
                    +----+-----+
                         |
                         | Actions (may generate
                         | internal events, but
                         v prefer external)
                    +---------+
                    |  World  |
                    +---------+


DANGER: Internal Events
  - Can create infinite loops
  - Harder to trace
  - Break the "FSM responds to world" model

PREFER: Let the world generate events
  - Action triggers I/O
  - I/O completion generates external event
  - FSM responds to external event
```

```c
/* Event Design Example: HTTP Client FSM */

typedef enum {
    /* Connection events */
    EVENT_CONNECT_REQUESTED,    /* User wants to connect */
    EVENT_SOCKET_CONNECTED,     /* TCP connection established */
    EVENT_SOCKET_ERROR,         /* TCP error occurred */
    
    /* Request events */
    EVENT_REQUEST_QUEUED,       /* Request added to queue */
    EVENT_REQUEST_SENT,         /* Request written to socket */
    EVENT_HEADERS_RECEIVED,     /* Response headers parsed */
    EVENT_BODY_RECEIVED,        /* Response body complete */
    
    /* Lifecycle events */
    EVENT_TIMEOUT_EXPIRED,      /* Operation timed out */
    EVENT_CLOSE_REQUESTED,      /* User wants to close */
    EVENT_SOCKET_CLOSED,        /* Connection terminated */
    
    EVENT_COUNT
} http_event_t;

/* Note: All past tense or noun phrases
   Note: Distinguish request/completion
   Note: All external facts, not commands */
```

---

## 3. Transition Rules

```
RULE 1: EXPLICIT TRANSITIONS ONLY
=================================

Every legal state+event combination is listed explicitly.
No "catch-all" handlers. No "default" transitions.

WRONG:
  switch (state) {
      case CONNECTED:
          switch (event) {
              case DATA_RX: handle_data(); break;
              default: /* ignore other events */ break;  // DANGER!
          }
          break;
      default: /* unknown state */ break;  // DANGER!
  }

RIGHT:
  Transition table explicitly lists ALL valid combinations:
  
  | State      | Event      | Next State | Action          |
  |------------|------------|------------|-----------------|
  | IDLE       | CONNECT_REQ| CONNECTING | start_connect() |
  | CONNECTING | CONNECTED  | READY      | init_session()  |
  | CONNECTING | ERROR      | IDLE       | log_error()     |
  | READY      | DATA_RX    | READY      | process_data()  |
  | READY      | CLOSE_REQ  | CLOSING    | start_close()   |
  | CLOSING    | CLOSED     | IDLE       | cleanup()       |
  
  Any combination NOT in this table is ILLEGAL.
```

```
RULE 2: NO IMPLICIT FALL-THROUGH
================================

State doesn't "naturally" flow to next state.
Every transition requires an event.

WRONG (Implicit progression):
  void update() {
      if (state == INIT) {
          do_init();
          state = READY;  // Implicit transition!
      }
  }

RIGHT (Event-driven):
  void handle_event(event_t e) {
      if (state == INIT && e == EVENT_INIT_COMPLETE) {
          state = READY;  // Explicit transition
      }
  }


WRONG (Fall-through states):
  STEP1 -> STEP2 -> STEP3 -> STEP4  (automatic)
  
  This is a procedure, not an FSM!

RIGHT (Event-driven progression):
  STEP1 --[step1_done]--> STEP2 --[step2_done]--> STEP3
  
  Each arrow is a REAL EVENT that could fail or be delayed.
```

```
RULE 3: SELF-TRANSITIONS ARE EXPLICIT
=====================================

Staying in the same state is a transition too.

WRONG (Implicit stay):
  if (transition_exists(state, event)) {
      state = transitions[state][event];
  }
  // else: silently stay in current state  // DANGER!

RIGHT (Explicit self-transition):
  | State | Event    | Next State | Action           |
  |-------|----------|------------|------------------|
  | READY | DATA_RX  | READY      | process_data()   |  <- Explicit!
  | READY | HEARTBEAT| READY      | reset_timeout()  |  <- Explicit!

Self-transitions should:
  1. Be in the transition table
  2. Optionally have actions
  3. Be distinguishable from "illegal event"
```

```
COMPLETE TRANSITION TABLE TEMPLATE
==================================

For N states and M events, you need to consider N×M cells.

+--------+--------+--------+--------+--------+--------+
| State\ | Event  | Event  | Event  | Event  | Event  |
| Event  |   1    |   2    |   3    |   4    |   5    |
+--------+--------+--------+--------+--------+--------+
| State1 |  S2/a1 |   -    |   -    |   -    |   -    |
| State2 |   -    |  S3/a2 |  S4/a3 |   -    |   -    |
| State3 |   -    |   -    |  S3/a4 |  S5/a5 |   -    |
| State4 |   -    |   -    |   -    |   -    |  S1/a6 |
| State5 |   -    |   -    |   -    |   -    |   -    |
+--------+--------+--------+--------+--------+--------+

Legend:
  S2/a1 = Transition to State2, execute action a1
  -     = ILLEGAL (must be handled, not ignored)

This table IS your specification.
This table IS your test oracle.
This table IS your documentation.
```

```c
/* Transition Rules in C: Complete Example */

typedef enum { 
    S_IDLE, S_CONNECTING, S_CONNECTED, S_ERROR, 
    S_COUNT 
} state_t;

typedef enum { 
    E_CONNECT, E_CONNECTED, E_ERROR, E_RESET, 
    E_COUNT 
} event_t;

typedef struct {
    state_t next_state;
    int (*action)(void *ctx);
} transition_t;

/* Mark illegal transitions explicitly */
#define ILLEGAL { .next_state = S_COUNT, .action = NULL }

static transition_t table[S_COUNT][E_COUNT] = {
    /*           E_CONNECT            E_CONNECTED          E_ERROR              E_RESET         */
    [S_IDLE]       = { {S_CONNECTING, start},  ILLEGAL,             ILLEGAL,             ILLEGAL           },
    [S_CONNECTING] = { ILLEGAL,                {S_CONNECTED, init}, {S_ERROR, log_err},  {S_IDLE, cleanup} },
    [S_CONNECTED]  = { ILLEGAL,                ILLEGAL,             {S_ERROR, log_err},  {S_IDLE, cleanup} },
    [S_ERROR]      = { ILLEGAL,                ILLEGAL,             ILLEGAL,             {S_IDLE, cleanup} },
};

int fsm_event(fsm_t *fsm, event_t e) {
    transition_t *t = &table[fsm->state][e];
    
    /* Check for illegal transition */
    if (t->next_state == S_COUNT) {
        fprintf(stderr, "ILLEGAL: state=%d event=%d\n", fsm->state, e);
        return -1;  /* Or assert(0) for fail-fast */
    }
    
    /* Execute action if present */
    int rc = 0;
    if (t->action != NULL) {
        rc = t->action(fsm->context);
    }
    
    /* Transition to new state */
    fsm->state = t->next_state;
    return rc;
}
```

---

## Summary: Design Checklist

```
STATE DESIGN CHECKLIST
======================
[ ] Each state represents a distinct mode of operation
[ ] States have different valid events
[ ] No pseudo-states (counters, flags)
[ ] States are mutually exclusive
[ ] States are named as conditions (nouns/adjectives)

EVENT DESIGN CHECKLIST
======================
[ ] Events are past tense or noun phrases
[ ] Events describe facts, not commands
[ ] Request and completion are separate events
[ ] Events come from external sources
[ ] Internal events are minimized

TRANSITION DESIGN CHECKLIST
===========================
[ ] All valid transitions are explicitly listed
[ ] No default/catch-all handlers
[ ] Self-transitions are explicit
[ ] Illegal transitions are defined (not just missing)
[ ] Transition table is complete (N states × M events)
```

---

**中文解释（Chinese Explanation）**

**设计状态**

状态粒度遵循"金发女孩原则"：太粗（单一 PROCESSING 状态）无法区分不同操作模式；太细（每个字节一个状态）把数据当成了状态。正确的做法是每个状态代表一种**不同的操作模式**，有**不同的有效事件集**。

避免伪状态：如果某个"状态"仅用于记账（如 WAITING_FOR_ACK、RETRY_3），它应该是上下文数据，不是真正的状态。测试方法：系统行为是否因这个"状态"而不同？不同则是真状态，相同则是数据。

确保状态互斥：系统不能同时处于两个状态。如果 CONNECTED 和 AUTHENTICATED 可以同时存在，说明状态设计有问题，应该合并为 CONNECTED_AUTHENTICATED 或设计为线性进程。

**设计事件**

事件是"已发生的事实"，使用过去时态：
- 错误：CONNECT（命令式）
- 正确：CONNECTION_REQUESTED（请求）、CONNECTION_ESTABLISHED（完成）

区分请求和完成：一个操作通常需要两个事件——请求事件和完成事件。这让 FSM 能正确处理异步操作。

外部 vs 内部事件：优先使用外部事件。内部事件（FSM 动作产生的事件）容易造成无限循环，破坏"FSM 响应世界"的模型。

**转换规则**

1. **显式转换**：每个合法的状态+事件组合都明确列出，没有 catch-all 或 default 处理。

2. **无隐式流转**：状态不会"自然"流向下一个状态，每个转换都需要事件触发。

3. **自转换是显式的**：停留在同一状态也是一种转换，需要在表中明确定义，有别于"非法事件"。

转换表是你的规格说明、测试预言、文档——三者合一。对于 N 个状态和 M 个事件，你需要考虑 N×M 个单元格，每个要么是合法转换，要么明确标记为非法。
