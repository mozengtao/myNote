# HOW | FSM Logic ≠ Work Logic

## 1. Separation of Concerns

```
THE FUNDAMENTAL SPLIT
=====================

+------------------------------------------------------------------+
|                                                                  |
|    FSM LOGIC                      WORK LOGIC                     |
|    (Control Plane)                (Data Plane)                   |
|                                                                  |
|    - What state are we in?        - How to connect?              |
|    - Is this event valid?         - How to send data?            |
|    - What state comes next?       - How to parse response?       |
|    - Which action to trigger?     - How to handle errors?        |
|                                                                  |
|    DECIDES                        EXECUTES                       |
|                                                                  |
+------------------------------------------------------------------+

        Events                           Results
           |                                ^
           v                                |
    +-------------+    Action Pointer    +--+--+
    |    FSM      | ------------------> | Work |
    |  (Decides)  |                     | (Does)|
    +-------------+                     +------+
           |                                |
           v                                v
      Next State                       Side Effects
```

```
VISUAL: FSM Decides, Actions Execute
====================================

                    +------------------+
     Event          |                  |
  ---------->       |      FSM         |
                    |                  |
                    |  if (state==A    |
                    |      && event==X)|
                    |    next = B      |
                    |    action = f()  |
                    |                  |
                    +--------+---------+
                             |
                             | "Execute action f"
                             v
                    +------------------+
                    |                  |
                    |   Action f()     |
                    |                  |
                    |  - Open socket   |
                    |  - Send bytes    |
                    |  - Log message   |
                    |                  |
                    +------------------+
                             |
                             v
                       Side effects
                    (network, disk, etc.)


The FSM NEVER:
  - Opens sockets
  - Sends data
  - Reads files
  - Sleeps or waits
  - Allocates large buffers

The FSM ONLY:
  - Checks current state
  - Looks up transitions
  - Sets next state
  - Calls action functions
```

---

## 2. Action Isolation

```
SIDE EFFECTS OUTSIDE TRANSITION LOGIC
=====================================

WRONG (Mixed concerns):
+----------------------------------------------------+
|  int fsm_handle(fsm_t *f, event_t e) {             |
|      if (f->state == CONNECTING && e == CONNECTED) |
|      {                                             |
|          f->socket = socket(AF_INET, ...);  // NO! |
|          connect(f->socket, ...);           // NO! |
|          send(f->socket, "HELLO", ...);     // NO! |
|          f->state = CONNECTED;                     |
|      }                                             |
|  }                                                 |
+----------------------------------------------------+

Problems:
  - FSM function does I/O
  - Hard to test (needs network)
  - Hard to trace (where did connect fail?)
  - Action logic mixed with state logic


RIGHT (Separated concerns):
+----------------------------------------------------+
|  /* FSM only decides */                            |
|  int fsm_handle(fsm_t *f, event_t e) {             |
|      transition_t *t = lookup(f->state, e);        |
|      if (!t) return -1;                            |
|                                                    |
|      state_t old = f->state;                       |
|      f->state = t->next_state;                     |
|                                                    |
|      /* Action is a function pointer */            |
|      if (t->action) {                              |
|          return t->action(f->ctx);                 |
|      }                                             |
|      return 0;                                     |
|  }                                                 |
|                                                    |
|  /* Actions are separate */                        |
|  int action_start_connect(void *ctx) {             |
|      conn_ctx_t *c = ctx;                          |
|      c->socket = socket(AF_INET, ...);             |
|      return connect(c->socket, ...);               |
|  }                                                 |
+----------------------------------------------------+
```

```
WHY TRANSITIONS SHOULD BE PURE DECISIONS
========================================

Pure = No side effects, same input = same output

The Transition Function:
  (state, event) -> (next_state, action)

This is a PURE FUNCTION (mathematically):
  - Depends only on state and event
  - Returns same result for same inputs
  - Has no side effects itself

Actions are NOT pure (and that's OK):
  - They perform I/O
  - They modify external state
  - They can fail

                    PURE                  IMPURE
              +-------------+        +-------------+
   Event ---> | Transition  | -----> |   Action    | ---> Side Effects
              |   Logic     |        |   Logic     |
              +-------------+        +-------------+
                    |
                    v
               Next State
               (determined,
               predictable)
```

```
THE TRANSACTION ANALOGY
=======================

Think of state transitions like database transactions:

1. BEGIN TRANSACTION
   - Lookup transition (pure, no side effects)
   - Validate event is legal
   
2. UPDATE STATE
   - Change state variable (atomic)
   
3. EXECUTE SIDE EFFECTS
   - Run action (may fail!)
   
4. HANDLE ACTION FAILURE
   - Action failure ≠ transition failure
   - State already changed
   - Need recovery mechanism


CRITICAL QUESTION:
  What happens if action fails AFTER state changed?

Option A: State changes BEFORE action (shown above)
  + State reflects intent
  + Action can check state
  - May need rollback on failure

Option B: State changes AFTER action succeeds
  + No rollback needed
  - State doesn't reflect in-progress work
  - Hard to query "what are we trying to do?"

Most FSMs use Option A with explicit error states:
  CONNECTING --(action fails)--> ERROR
  CONNECTING --(action succeeds)--> wait for CONNECTED event
```

```c
/* Complete Example: Separation of Concerns */

/* === TYPES === */
typedef enum {
    STATE_IDLE,
    STATE_CONNECTING,
    STATE_CONNECTED,
    STATE_ERROR,
    STATE_COUNT
} state_t;

typedef enum {
    EVENT_CONNECT,
    EVENT_CONNECTED,
    EVENT_DISCONNECT,
    EVENT_ERROR,
    EVENT_COUNT
} event_t;

/* Forward declaration */
typedef struct fsm fsm_t;

/* Action function type: takes context, returns status */
typedef int (*action_fn)(void *ctx);

/* Transition definition */
typedef struct {
    state_t   next_state;
    action_fn action;        /* May be NULL */
} transition_t;

/* FSM structure */
struct fsm {
    state_t state;
    void    *ctx;           /* User context for actions */
};


/* === ACTIONS (Impure, do real work) === */

int action_start_connect(void *ctx) {
    connection_t *c = (connection_t *)ctx;
    printf("ACTION: Starting connection to %s:%d\n", c->host, c->port);
    
    c->sock = socket(AF_INET, SOCK_STREAM, 0);
    if (c->sock < 0) return -1;
    
    /* Non-blocking connect */
    int rc = connect(c->sock, ...);
    return (rc == 0 || errno == EINPROGRESS) ? 0 : -1;
}

int action_complete_connect(void *ctx) {
    connection_t *c = (connection_t *)ctx;
    printf("ACTION: Connection established\n");
    return 0;
}

int action_cleanup(void *ctx) {
    connection_t *c = (connection_t *)ctx;
    printf("ACTION: Cleaning up\n");
    if (c->sock >= 0) {
        close(c->sock);
        c->sock = -1;
    }
    return 0;
}

int action_handle_error(void *ctx) {
    connection_t *c = (connection_t *)ctx;
    printf("ACTION: Handling error\n");
    action_cleanup(ctx);
    return 0;
}


/* === TRANSITION TABLE (Pure, declarative) === */

#define ILLEGAL { STATE_COUNT, NULL }

static const transition_t transitions[STATE_COUNT][EVENT_COUNT] = {
    /*                  CONNECT                        CONNECTED                      DISCONNECT                ERROR                  */
    [STATE_IDLE]      = {{STATE_CONNECTING, action_start_connect}, ILLEGAL,           ILLEGAL,                  ILLEGAL               },
    [STATE_CONNECTING]= {ILLEGAL,                                  {STATE_CONNECTED, action_complete_connect}, {STATE_IDLE, action_cleanup}, {STATE_ERROR, action_handle_error}},
    [STATE_CONNECTED] = {ILLEGAL,                                  ILLEGAL,           {STATE_IDLE, action_cleanup}, {STATE_ERROR, action_handle_error}},
    [STATE_ERROR]     = {ILLEGAL,                                  ILLEGAL,           {STATE_IDLE, action_cleanup}, ILLEGAL               },
};


/* === FSM ENGINE (Pure logic, no domain knowledge) === */

int fsm_init(fsm_t *fsm, void *ctx) {
    fsm->state = STATE_IDLE;
    fsm->ctx = ctx;
    return 0;
}

int fsm_handle(fsm_t *fsm, event_t event) {
    /* Bounds check */
    if (fsm->state >= STATE_COUNT || event >= EVENT_COUNT) {
        return -1;
    }
    
    /* Lookup transition (PURE) */
    const transition_t *t = &transitions[fsm->state][event];
    
    /* Check for illegal transition */
    if (t->next_state >= STATE_COUNT) {
        fprintf(stderr, "Illegal transition: state=%d event=%d\n",
                fsm->state, event);
        return -1;
    }
    
    /* Log transition (for debugging) */
    printf("TRANSITION: %d --%d--> %d\n", fsm->state, event, t->next_state);
    
    /* Update state FIRST */
    fsm->state = t->next_state;
    
    /* Execute action (IMPURE) */
    if (t->action != NULL) {
        int rc = t->action(fsm->ctx);
        if (rc != 0) {
            /* Action failed - may need error recovery */
            printf("Action failed with rc=%d\n", rc);
            return rc;
        }
    }
    
    return 0;
}

state_t fsm_get_state(const fsm_t *fsm) {
    return fsm->state;
}
```

---

## 3. Testing Implications

```
TESTING FSM LOGIC WITHOUT EXECUTING WORK
========================================

Because FSM logic is pure, it can be tested without:
  - Network connections
  - File I/O
  - Timers
  - External dependencies

Test 1: Transition Correctness
+--------------------------------------------------+
|  void test_transitions() {                       |
|      fsm_t fsm;                                  |
|      fsm.state = STATE_IDLE;                     |
|      fsm.ctx = NULL;  /* No real context! */     |
|                                                  |
|      /* Test: IDLE + CONNECT = CONNECTING */     |
|      state_t next = get_next_state(STATE_IDLE,   |
|                                    EVENT_CONNECT);|
|      assert(next == STATE_CONNECTING);           |
|                                                  |
|      /* Test: IDLE + DISCONNECT = ILLEGAL */     |
|      next = get_next_state(STATE_IDLE,           |
|                            EVENT_DISCONNECT);    |
|      assert(next == STATE_COUNT); /* illegal */  |
|  }                                               |
+--------------------------------------------------+

Test 2: Full Transition Sequences
+--------------------------------------------------+
|  void test_happy_path() {                        |
|      /* Test state sequence without actions */   |
|      state_t states[] = {                        |
|          STATE_IDLE,                             |
|          STATE_CONNECTING,                       |
|          STATE_CONNECTED,                        |
|          STATE_IDLE                              |
|      };                                          |
|      event_t events[] = {                        |
|          EVENT_CONNECT,                          |
|          EVENT_CONNECTED,                        |
|          EVENT_DISCONNECT                        |
|      };                                          |
|                                                  |
|      state_t s = STATE_IDLE;                     |
|      for (int i = 0; i < 3; i++) {               |
|          assert(s == states[i]);                 |
|          s = get_next_state(s, events[i]);       |
|      }                                           |
|      assert(s == states[3]);                     |
|  }                                               |
+--------------------------------------------------+

Test 3: Actions Tested Separately
+--------------------------------------------------+
|  /* Mock/stub for testing */                     |
|  int mock_socket_created = 0;                    |
|                                                  |
|  int mock_action_connect(void *ctx) {            |
|      mock_socket_created = 1;                    |
|      return 0;                                   |
|  }                                               |
|                                                  |
|  void test_connect_action() {                    |
|      connection_t c = { .host = "test" };        |
|      mock_action_connect(&c);                    |
|      assert(mock_socket_created == 1);           |
|  }                                               |
+--------------------------------------------------+
```

```
DETERMINISTIC BEHAVIOR
======================

Pure FSM logic is DETERMINISTIC:

  Same (state, event) -> Same (next_state, action)

This means:
  1. FSM can be fully specified by transition table
  2. FSM can be tested exhaustively
  3. FSM behavior can be verified formally
  4. Bug reproduction is reliable

Non-determinism comes ONLY from:
  - Action execution (I/O results)
  - Event timing (which event arrives first)
  - External state (what the world does)

The FSM itself? 100% predictable.


TEST MATRIX FOR N×M FSM:
+--------------------------------------------------+
| For each (state, event) pair:                    |
|   1. Assert correct next_state                   |
|   2. Assert correct action (or NULL)             |
|   3. Assert illegal transitions detected         |
|                                                  |
| Total test cases: N × M                          |
| (Tractable even for large FSMs)                  |
+--------------------------------------------------+
```

---

## Summary: The Split

```
+----------------------------------------------------------+
|                                                          |
|  +-------------------+      +------------------------+   |
|  |   FSM ENGINE      |      |   ACTION HANDLERS      |   |
|  +-------------------+      +------------------------+   |
|  | - State variable  |      | - I/O operations       |   |
|  | - Transition table|      | - Network calls        |   |
|  | - Event dispatch  |      | - File operations      |   |
|  | - Illegal checks  |      | - Timers               |   |
|  +-------------------+      +------------------------+   |
|         |                           ^                    |
|         | Function pointer          | Results/Errors     |
|         +-------------------------->+                    |
|                                                          |
|  PURE                        IMPURE                      |
|  TESTABLE                    MOCKABLE                    |
|  DETERMINISTIC               ASYNCHRONOUS                |
|                                                          |
+----------------------------------------------------------+

Benefits:
  1. FSM logic testable without dependencies
  2. Actions testable in isolation
  3. Clear responsibility boundaries
  4. Easy to mock for integration tests
  5. Deterministic state behavior
```

---

**中文解释（Chinese Explanation）**

**关注点分离**

FSM 逻辑（控制平面）和工作逻辑（数据平面）必须分离：

- **FSM 逻辑决定**：当前状态是什么？事件是否合法？下一状态是什么？触发哪个动作？
- **工作逻辑执行**：如何连接？如何发送数据？如何解析响应？

FSM 绝不直接执行 I/O、网络操作、文件读写、睡眠等待——它只查找转换、设置状态、调用动作函数。

**动作隔离**

错误示例：在 FSM 处理函数中直接创建 socket、调用 connect。这混合了关注点，难以测试，难以追踪错误。

正确做法：FSM 只做纯粹的决策，动作是独立的函数，通过函数指针调用。

**为什么转换应该是纯决策**

转换函数是数学意义上的纯函数：
- 只依赖状态和事件
- 相同输入返回相同结果
- 本身没有副作用

动作可以是非纯的（进行 I/O、修改外部状态、可能失败），但转换决策必须是纯粹的。

**关键问题：动作失败后状态怎么办？**

两种策略：
- 状态先变，动作后执行（推荐）：状态反映意图，失败时转换到 ERROR 状态
- 动作成功后状态才变：无需回滚，但状态不反映进行中的工作

**测试意义**

因为 FSM 逻辑是纯粹的，可以在不需要网络、文件、定时器的情况下测试：
- 测试转换正确性：验证 (state, event) → next_state
- 测试完整序列：验证状态序列
- 独立测试动作：使用 mock/stub

FSM 是确定性的：相同输入永远产生相同输出。非确定性只来自动作执行和外部世界。对于 N 个状态和 M 个事件，只需 N×M 个测试用例即可完全覆盖。
