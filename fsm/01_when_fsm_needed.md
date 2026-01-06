# WHEN | Recognizing When an FSM Is Needed

## 1. Symptoms That Indicate FSM Necessity

```
SYMPTOM 1: Boolean Flag Explosion
=================================

Before (Flag Hell):
+------------------------------------------+
|  bool is_connected;                      |
|  bool is_authenticated;                  |
|  bool is_ready;                          |
|  bool has_error;                         |
|  bool is_closing;                        |
|  bool is_retrying;                       |
+------------------------------------------+
        |
        v
  How many valid combinations?
  2^6 = 64 possible states
  But only ~8 are actually legal!

        |
        v
+------------------------------------------+
|  if (is_connected && !is_authenticated   |
|      && is_ready && !has_error           |
|      && !is_closing && !is_retrying) {   |
|    // What state is this even?           |
|  }                                       |
+------------------------------------------+


After (FSM):
+------------------------------------------+
|  enum state {                            |
|    DISCONNECTED,                         |
|    CONNECTING,                           |
|    AUTHENTICATING,                       |
|    READY,                                |
|    ERROR,                                |
|    CLOSING                               |
|  };                                      |
|                                          |
|  state current_state;  // Exactly ONE    |
+------------------------------------------+
```

```
SYMPTOM 2: Temporal Logic Bugs
==============================

"This must happen before that"

Timeline of Bug:
+--------+--------+--------+--------+--------+
|  T1    |  T2    |  T3    |  T4    |  T5    |
+--------+--------+--------+--------+--------+
| connect| auth   | ready  | send   | close  |
+--------+--------+--------+--------+--------+
              ^
              |
         Bug: auth() called twice
         Bug: send() before auth()
         Bug: close() during auth()

FSM Prevents This:
+-------------+     +---------------+     +-------+
| CONNECTING  | --> | AUTHENTICATING| --> | READY |
+-------------+     +---------------+     +-------+
      |                    |                  |
      | (only legal        | (only legal      | (only legal
      |  transition)       |  transition)     |  transition)
      v                    v                  v
   auth()              complete()          send()
```

```
SYMPTOM 3: State-Dependent Behavior Scattered Across Code
=========================================================

Bad: State checks everywhere

file1.c:  if (conn->state == READY && !conn->closing) { ... }
file2.c:  if (conn->authenticated && conn->socket > 0) { ... }
file3.c:  if (!conn->error && conn->ready) { ... }

     ^              ^              ^
     |              |              |
     +-------+------+------+-------+
             |
   Different files, different interpretations
   of "what state are we in?"


Good: Centralized state machine

+------------------+
|   fsm_handle()   |  <-- Single point of state decisions
+------------------+
         |
    +----+----+----+
    |    |    |    |
    v    v    v    v
 file1 file2 file3 file4
   |     |     |     |
   +-----+-----+-----+
         |
   All query current_state
   from ONE authoritative source
```

```
SYMPTOM 4: Implicit Assumptions About Execution Order
=====================================================

void process_request(request_t *req) {
    validate(req);      // Assumes: req exists
    parse(req);         // Assumes: req is valid
    execute(req);       // Assumes: req is parsed
    respond(req);       // Assumes: execution done
    cleanup(req);       // Assumes: response sent
}

What if parse() fails?
What if execute() times out?
What if respond() is called twice?

        Implicit order assumptions
               = Hidden FSM
               = Bugs waiting to happen
```

---

## 2. Anti-Examples: When an FSM Is NOT Appropriate

```
WHEN FSM IS OVERKILL
====================

Example 1: Simple Validation
----------------------------
Bad (Over-engineered):

  enum validation_state { START, CHECKING, VALID, INVALID };
  
  // This is just a function return value!

Good (Just use return codes):

  int validate(input_t *in) {
      if (!in) return -1;
      if (in->len == 0) return -2;
      return 0;  // valid
  }


Example 2: One-Shot Operations
------------------------------
Bad (FSM for single operation):

  enum copy_state { INIT, READING, WRITING, DONE };
  
  // No external events, no need to pause/resume

Good (Just a function):

  int copy_file(const char *src, const char *dst) {
      // Sequential code, no state needed
  }


Example 3: Pure Computation
---------------------------
Bad:
  enum sort_state { COMPARING, SWAPPING, DONE };

Good:
  void qsort(...);  // No state between calls
```

```
FSM vs SIMPLE CONDITIONAL LOGIC
===============================

Use Conditionals When:
+--------------------------------------------------+
| - Decisions are independent of history           |
| - No "memory" of previous operations needed      |
| - All information available at decision point    |
| - Single-shot, synchronous operations            |
+--------------------------------------------------+

Use FSM When:
+--------------------------------------------------+
| - Behavior depends on what happened before       |
| - System must "remember" its progress            |
| - Operations span multiple events/time           |
| - Invalid sequences must be prevented            |
| - External events arrive asynchronously          |
+--------------------------------------------------+


Decision Tree:

                  Does behavior depend
                  on previous events?
                         |
              +----------+----------+
              |                     |
             NO                    YES
              |                     |
              v                     v
      Simple conditionals      Consider FSM
                                    |
                          Are there more than
                          2-3 distinct modes?
                                    |
                         +----------+----------+
                         |                     |
                        NO                    YES
                         |                     |
                         v                     v
                    Maybe just           Definitely FSM
                    an enum flag
```

---

## 3. Refactoring Mindset: Detecting Hidden States

```
DETECTING HIDDEN STATES IN EXISTING CODE
========================================

Pattern 1: Multiple Booleans That Are Mutually Exclusive
--------------------------------------------------------

Suspicious code:
  bool started;
  bool running;
  bool paused;
  bool stopped;

  // Later...
  if (started && !running && !paused && !stopped) { ... }

Hidden FSM:
  +----------+     +----------+     +----------+     +----------+
  | CREATED  | --> | STARTED  | --> | RUNNING  | --> | STOPPED  |
  +----------+     +----------+     +----------+     +----------+
                        |               ^
                        v               |
                   +----------+         |
                   | PAUSED   |---------+
                   +----------+


Pattern 2: Status Variable With Special Values
----------------------------------------------

Suspicious code:
  int status;
  #define STATUS_OK      0
  #define STATUS_PENDING 1
  #define STATUS_ERROR  -1
  #define STATUS_RETRY  -2
  #define STATUS_TIMEOUT -3

  // This IS a state machine, just poorly expressed!


Pattern 3: Sequence Counters
----------------------------

Suspicious code:
  int step;
  
  void process() {
      if (step == 0) { init(); step = 1; }
      else if (step == 1) { connect(); step = 2; }
      else if (step == 2) { authenticate(); step = 3; }
      // ...
  }

This is a linear FSM hiding in plain sight!
```

```
FSMs ELIMINATE ILLEGAL STATES
=============================

Without FSM (Error Handling After The Fact):
+-------------------------------------------+
|  void send_data(conn_t *c, data_t *d) {   |
|      if (!c->connected) {                 |
|          log_error("not connected!");     |  <-- Error AFTER
|          return;                          |      illegal state
|      }                                    |      occurred
|      if (c->closing) {                    |
|          log_error("closing!");           |
|          return;                          |
|      }                                    |
|      // ... actually send                 |
|  }                                        |
+-------------------------------------------+

With FSM (Illegal States Cannot Exist):
+-------------------------------------------+
|  int fsm_event(fsm_t *f, event_t e) {     |
|      transition_t *t = lookup(f, e);      |
|      if (t == NULL) {                     |
|          // Illegal transition!           |  <-- Caught at
|          assert(0);                       |      boundary
|      }                                    |
|      f->state = t->next_state;            |
|      return t->action(f);                 |
|  }                                        |
|                                           |
|  // SEND event only valid in READY state  |
|  // FSM table enforces this at compile    |
|  // time (static table) or definitively   |
|  // at runtime (dynamic table)            |
+-------------------------------------------+

The difference:
- Without FSM: Check for errors everywhere, hope you didn't miss one
- With FSM: Define legal transitions once, illegal = impossible
```

---

## Summary: The FSM Recognition Checklist

```
+----------------------------------------------------------+
|                 DO I NEED AN FSM?                         |
+----------------------------------------------------------+
|                                                          |
|  [ ] I have 3+ boolean flags that interact               |
|  [ ] I've written "if (state1 && !state2 && state3)"     |
|  [ ] Bugs involve "X happened before Y was ready"        |
|  [ ] I'm checking the same conditions in multiple places |
|  [ ] There's a concept of "current mode" or "phase"      |
|  [ ] External events can arrive at any time              |
|  [ ] I need to handle partial completion / resume        |
|  [ ] Error recovery depends on "how far we got"          |
|                                                          |
|  If 3+ boxes checked: You have a hidden FSM.             |
|  Make it explicit.                                       |
|                                                          |
+----------------------------------------------------------+
```

---

**中文解释（Chinese Explanation）**

**什么时候需要状态机？**

1. **布尔标志爆炸**：当你发现代码中有多个相互关联的布尔变量（如 `is_connected`、`is_ready`、`has_error`），并且它们的组合逻辑变得复杂难以理解时，这是一个明确的信号——你需要状态机。6个布尔变量理论上有64种组合，但实际合法的可能只有8种。

2. **时序逻辑错误**："这个必须在那个之前发生"——如果你的代码依赖于操作的顺序，但这种顺序没有被显式建模，bug就会潜伏其中。状态机通过明确定义合法转换来防止这类错误。

3. **状态相关行为分散**：如果你在多个文件中看到类似的状态检查代码，每处都有略微不同的条件判断，说明状态逻辑应该被集中管理。

4. **隐式执行顺序假设**：当函数依赖于"前一步已经完成"这种隐含假设时，你实际上有一个隐藏的状态机。

**什么时候不需要状态机？**

- 简单的验证逻辑（直接返回错误码即可）
- 一次性操作（没有暂停/恢复的需求）
- 纯计算函数（不需要记住历史）
- 决策不依赖于历史状态

**重构心态**

识别隐藏状态的方法：
- 多个互斥的布尔变量 → 应该是枚举状态
- 带特殊值的状态变量 → 已经是状态机，只是表达不好
- 步骤计数器 → 线性状态机的伪装

状态机的核心价值：**消除非法状态，而不是在错误发生后处理它们**。没有状态机时，你需要在每个操作前检查条件；有状态机后，你在状态转换表中定义一次合法转换，非法操作在源头就被阻止。
