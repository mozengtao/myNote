# WHY | Rejecting Transitions Is More Important Than Accepting Them

## 1. Illegal Transitions: Where Bugs Live

```
THE 80/20 RULE OF FSM BUGS
==========================

80% of state machine bugs are ILLEGAL TRANSITIONS
20% are wrong actions or wrong next states

Illegal Transition = Event occurs in a state where it's not valid

Examples:
  - SEND_DATA in DISCONNECTED state
  - CONNECT in already CONNECTED state  
  - AUTHENTICATE before CONNECTED
  - PROCESS_RESPONSE when no request was sent

These are NOT edge cases.
These are the CORE of what FSMs prevent.
```

```
ANATOMY OF AN ILLEGAL TRANSITION BUG
====================================

The Bug:
  User clicks "Send" button twice very fast.
  
Without FSM:
  +----------------------------------------------------+
  |  void on_send_clicked() {                          |
  |      send_data(buffer);  // Called twice!          |
  |      clear_buffer();     // Double-free!           |
  |  }                                                 |
  +----------------------------------------------------+
  
  Timeline:
    T1: click -> send_data() starts
    T2: click -> send_data() starts (while T1 in progress!)
    T3: T1 completes, clear_buffer()
    T4: T2 completes, clear_buffer() <- CRASH!

With FSM:
  +----------------------------------------------------+
  |  State: READY                                      |
  |  Event: SEND_CLICKED                               |
  |  Transition: READY -> SENDING                      |
  |                                                    |
  |  State: SENDING                                    |
  |  Event: SEND_CLICKED                               |
  |  Transition: ILLEGAL <- Second click rejected!     |
  +----------------------------------------------------+
  
  Timeline:
    T1: click -> state=SENDING, send starts
    T2: click -> ILLEGAL (state is SENDING, not READY)
    T3: send completes -> state=READY
```

```
WHY ILLEGAL EVENTS ARE NOT "EDGE CASES"
=======================================

Common Misconception:
  "Illegal events are rare, I'll handle them in a catch-all"

Reality:
  Illegal events happen because of:
  
  1. RACE CONDITIONS
     +-------+     +-------+
     | User  |     | Timer |
     +---+---+     +---+---+
         |             |
         | click       | timeout
         v             v
     [Both arrive at same time]
     
  2. ASYNC COMPLETION
     request() -----> [ network ] -----> response()
                            |
                            +-----> error()  (also arrives!)
     
  3. USER BEHAVIOR
     - Double-clicks
     - Clicking during loading
     - Pressing back during submit
     
  4. SYSTEM EVENTS
     - Disconnect during operation
     - Timeout during retry
     - Shutdown during cleanup
```

---

## 2. Why Ignoring Illegal Events Is Dangerous

```
THE SILENT IGNORE ANTI-PATTERN
==============================

WRONG:
  int fsm_handle(fsm_t *f, event_t e) {
      transition_t *t = lookup(f->state, e);
      if (t == NULL) {
          return 0;  // Silently ignore <- DANGEROUS!
      }
      // ...
  }

Why this is dangerous:

1. HIDES BUGS
   +---------------------------------+
   | The event was sent for a reason |
   | Someone expected it to work     |
   | Silent ignore = silent failure  |
   +---------------------------------+

2. CORRUPTS INVARIANTS
   Caller thinks: "I sent DISCONNECT, so we're disconnecting"
   Reality: FSM ignored it, still CONNECTED
   Later: Caller assumes disconnected, crashes

3. MAKES DEBUGGING IMPOSSIBLE
   Log: "Request sent"
   Log: "Waiting for response"
   Log: "Timeout, no response"
   
   What actually happened:
   - FSM was in CLOSING state
   - SEND event was silently ignored
   - No request was ever sent
   - No log of the ignored event

4. ALLOWS ILLEGAL STATES
   If DISCONNECT is ignored during SENDING,
   the connection might be half-closed,
   leading to resource leaks or hangs.
```

```
THE ACCUMULATING ERROR PROBLEM
==============================

Silent ignore leads to "state drift":

Expected:    IDLE -> CONNECTING -> CONNECTED -> SENDING -> IDLE
                                       ^
                                       |
Actual:      IDLE -> CONNECTING -------+  (CONNECTED event ignored!)
                          |
                          v
             Still CONNECTING, but socket is connected!
                          |
                          v
             SEND event arrives
                          |
                          v
             Ignored (not valid in CONNECTING)
                          |
                          v
             User sees: "Stuck in connecting"
             Reality: FSM diverged from system state
```

---

## 3. Enforcement Strategies

```
STRATEGY 1: ASSERTIONS (Fail-Fast)
==================================

Best for: Development, debug builds, critical systems

int fsm_event(fsm_t *f, event_t e) {
    transition_t *t = lookup(f->state, e);
    
    /* Fail immediately on illegal transition */
    assert(t != NULL && "Illegal state transition!");
    
    f->state = t->next_state;
    if (t->action) t->action(f->ctx);
    return 0;
}

Pros:
  + Immediate feedback
  + Cannot be ignored
  + Stack trace at failure point
  
Cons:
  - Crashes production (use ONLY in debug)
  - No graceful recovery
```

```
STRATEGY 2: LOGGING + ERROR RETURN (Recoverable)
================================================

Best for: Production systems, library code

int fsm_event(fsm_t *f, event_t e) {
    transition_t *t = lookup(f->state, e);
    
    if (t == NULL) {
        /* Log with full context */
        log_error("FSM: Illegal transition! "
                  "state=%s event=%s",
                  state_name(f->state),
                  event_name(e));
        
        /* Return error, let caller decide */
        return -EINVAL;
    }
    
    f->state = t->next_state;
    if (t->action) t->action(f->ctx);
    return 0;
}

Pros:
  + Production-safe
  + Caller can handle error
  + Full audit trail
  
Cons:
  - Caller might ignore return value
  - Error can propagate far from cause
```

```
STRATEGY 3: ERROR STATE TRANSITION
==================================

Best for: Systems requiring graceful degradation

int fsm_event(fsm_t *f, event_t e) {
    transition_t *t = lookup(f->state, e);
    
    if (t == NULL) {
        log_error("FSM: Illegal transition, entering ERROR state");
        
        /* Transition to dedicated error state */
        f->state = STATE_ERROR;
        f->error_code = ERR_ILLEGAL_TRANSITION;
        f->last_event = e;
        
        /* Trigger error handling */
        if (f->on_error) f->on_error(f);
        
        return -1;
    }
    
    f->state = t->next_state;
    if (t->action) t->action(f->ctx);
    return 0;
}

Pros:
  + System remains in valid state
  + Error is visible and queryable
  + Recovery possible from ERROR state
  
Cons:
  - More complex state machine
  - Need to design ERROR state transitions
```

```
STRATEGY COMPARISON
===================

+------------------+-------------+-------------+-------------+
|                  | ASSERT      | LOG+RETURN  | ERROR STATE |
+------------------+-------------+-------------+-------------+
| Development      | ★★★★★      | ★★★        | ★★★        |
| Production       | ☆☆☆☆☆      | ★★★★       | ★★★★★     |
| Debuggability    | ★★★★★      | ★★★★       | ★★★        |
| Recovery         | ☆☆☆☆☆      | ★★★        | ★★★★★     |
| Complexity       | ★★★★★      | ★★★★       | ★★          |
+------------------+-------------+-------------+-------------+

Recommendation:
  - Use ASSERT in debug builds (#ifndef NDEBUG)
  - Use LOG+RETURN or ERROR STATE in release builds
  - NEVER silently ignore
```

---

## 4. FSM as a Correctness Boundary

```
FSM AS AN INVARIANT ENFORCER
============================

The FSM maintains system invariants automatically:

Invariant: "Cannot send data without active connection"

Without FSM (Manual enforcement):
  void send_data(conn_t *c, data_t *d) {
      if (!c->connected) { ... }      // Check 1
      if (c->closing) { ... }         // Check 2
      if (c->error) { ... }           // Check 3
      if (!c->authenticated) { ... }  // Check 4
      // Did we miss any?
      // Will future code add more states?
  }

With FSM (Automatic enforcement):
  // SEND_DATA event only valid in READY state
  // Transition table enforces this
  // Cannot forget, cannot bypass
  
  Transition table:
  +-------+------------+--------+
  | State | Event      | Valid? |
  +-------+------------+--------+
  | READY | SEND_DATA  | YES    |
  | *     | SEND_DATA  | NO     |  <- Automatically enforced
  +-------+------------+--------+
```

```
FSM AS DOCUMENTATION THAT CANNOT LIE
====================================

Code comments lie:
  /* Connection must be established before calling this */
  void send_request();  // Can be called without connection!

README lies:
  "Call authenticate() after connect()"  // Not enforced!

FSM transition table CANNOT lie:
  +---------------+---------------+---------------+
  | DISCONNECTED  | AUTH_REQUEST  | ILLEGAL       |
  | CONNECTED     | AUTH_REQUEST  | AUTHENTICATING|
  +---------------+---------------+---------------+
  
  This IS the specification.
  This IS enforced at runtime.
  Code and documentation are ONE.


FSM Visualization = System Specification:

    +------+    CONNECT_REQ    +------------+
    | IDLE | ----------------> | CONNECTING |
    +------+                   +------------+
       ^                            |
       |                            | CONNECTED
       |   DISCONNECT               v
       +--------------------  +------------+    AUTH_REQ    +----------------+
                              | CONNECTED  | -------------> | AUTHENTICATING |
                              +------------+                +----------------+
                                    ^                              |
                                    |        AUTH_SUCCESS          |
                                    +------------------------------+

This diagram IS the code.
Change the diagram, change the behavior.
```

```
THE ILLEGAL TRANSITION AS A BUG REPORT
======================================

When an illegal transition is detected:

1. You know EXACTLY what went wrong:
   "State=CONNECTING, Event=SEND_DATA"

2. You know the system is in a KNOWN state:
   Still CONNECTING (not corrupted)

3. You can answer:
   - Who sent the event? (caller)
   - Why did they expect it to work? (their bug)
   - What should have happened first? (missing transition)

Compare to:
  "Segmentation fault at 0x7fff..."
  "Connection reset by peer"
  "Unknown error -1"

The FSM converts MYSTERY CRASHES into SPECIFIC BUG REPORTS.
```

---

## Summary: The Rejection Mindset

```
+----------------------------------------------------------+
|                                                          |
|  Traditional thinking:                                   |
|    "Handle all events, have a default case"              |
|                                                          |
|  FSM thinking:                                           |
|    "Define legal transitions, REJECT everything else"    |
|                                                          |
+----------------------------------------------------------+

The FSM is a WHITELIST, not a BLACKLIST.

  BLACKLIST:  "Don't allow X, Y, Z"
              (What about W? Forgot to blacklist!)
              
  WHITELIST:  "Only allow A, B, C"
              (Everything else is automatically illegal)


MINDSET SHIFT:
  From: "What can go wrong?" (infinite possibilities)
  To:   "What is allowed?" (finite, enumerable)
```

---

**中文解释（Chinese Explanation）**

**非法转换是 Bug 的温床**

FSM bug 的 80% 是非法转换——事件在不应该发生的状态下发生。例如：在 DISCONNECTED 状态收到 SEND_DATA，在已连接时又收到 CONNECT。这不是边缘情况，这是 FSM 存在的核心意义。

非法事件发生的原因：
- 竞态条件（用户点击和定时器同时触发）
- 异步完成（响应和错误同时到达）
- 用户行为（双击、加载时点击）
- 系统事件（操作中断开、重试中超时）

**为什么静默忽略非法事件是危险的**

"静默忽略"反模式的危害：
1. **隐藏 Bug**：事件被发送是有原因的，静默忽略 = 静默失败
2. **破坏不变量**：调用者认为操作已执行，实际被忽略
3. **无法调试**：日志没有记录被忽略的事件
4. **允许非法状态**：系统状态与 FSM 状态发生漂移

**执行策略**

1. **断言（Fail-Fast）**：开发阶段使用，立即崩溃，提供堆栈跟踪
2. **日志 + 错误返回**：生产环境使用，记录完整上下文，让调用者决定
3. **错误状态转换**：需要优雅降级的系统，转换到专门的 ERROR 状态

**绝对不要静默忽略**。

**FSM 作为正确性边界**

FSM 自动执行系统不变量："没有连接就不能发送数据"——这由转换表强制执行，不可能遗漏，不可能绕过。

FSM 转换表是**不会说谎的文档**：
- 代码注释可以过时
- README 可以不准确
- 但转换表同时是规格说明和运行时执行

当检测到非法转换时，你得到的是**精确的错误报告**，而不是神秘的崩溃。

**拒绝思维**

传统思维："处理所有事件，设置默认情况"
FSM 思维："定义合法转换，**拒绝一切其他**"

FSM 是白名单，不是黑名单：
- 黑名单："不允许 X、Y、Z"（W 呢？忘了禁止！）
- 白名单："只允许 A、B、C"（其他一切自动非法）

思维转变：从"什么可能出错？"（无限可能）到"什么是允许的？"（有限、可枚举）。
