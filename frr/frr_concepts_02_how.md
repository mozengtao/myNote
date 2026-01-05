# Extracting FRR Architecture - Part 2: HOW

## How to Translate FRR Concepts into Generic Architecture

This document maps FRR-specific concepts to universal architectural patterns
that apply to any complex C system.

---

## 1. Control Plane vs Data Plane → Decision vs Execution

```
+===========================================================================+
|                    DECISION VS EXECUTION SEPARATION                       |
+===========================================================================+

                    FRR MODEL
                    =========

  +------------------+     +------------------+     +------------------+
  |  Protocol Daemon |     |  Protocol Daemon |     |  Protocol Daemon |
  |  (bgpd)          |     |  (ospfd)         |     |  (ripd)          |
  +------------------+     +------------------+     +------------------+
           |                       |                       |
           | Routing decisions     |                       |
           v                       v                       v
  +---------------------------------------------------------------------+
  |                            ZEBRA                                    |
  |                       (Control Plane)                               |
  |   - Validates routes                                                |
  |   - Selects best paths                                              |
  |   - Maintains RIB                                                   |
  +---------------------------------------------------------------------+
                                   |
                                   | Route install requests
                                   v
  +---------------------------------------------------------------------+
  |                          KERNEL                                     |
  |                       (Data Plane)                                  |
  |   - Actually forwards packets                                       |
  |   - Maintains FIB                                                   |
  +---------------------------------------------------------------------+


                    GENERIC MODEL
                    =============

  +------------------+     +------------------+     +------------------+
  |  Decision Engine |     |  Decision Engine |     |  Decision Engine |
  |  (Plugin A)      |     |  (Plugin B)      |     |  (Plugin C)      |
  +------------------+     +------------------+     +------------------+
           |                       |                       |
           | Intent / Requests     |                       |
           v                       v                       v
  +---------------------------------------------------------------------+
  |                        STATE MANAGER                                |
  |                       (Decision Layer)                              |
  |   - Validates requests                                              |
  |   - Resolves conflicts                                              |
  |   - Maintains authoritative state                                   |
  +---------------------------------------------------------------------+
                                   |
                                   | Execution commands
                                   v
  +---------------------------------------------------------------------+
  |                      EXECUTION BACKEND                              |
  |                       (Execution Layer)                             |
  |   - Performs actual operations                                      |
  |   - Reports success/failure                                         |
  +---------------------------------------------------------------------+


+===========================================================================+
|                    THE EXECUTION BOUNDARY                                 |
+===========================================================================+

  WRONG: Decision code performs execution
  =======================================

  void handle_request(Request *req) {
      // Decision logic mixed with execution
      if (should_accept(req)) {
          write(fd, req->data, req->len);    // <-- EXECUTION IN DECISION!
          update_state(req);
      }
  }

  Problems:
  +------------------------------------------------------------------+
  | - Cannot test decision logic without real I/O                    |
  | - Blocking I/O stalls decision processing                        |
  | - Failure handling intermixed with logic                         |
  | - Cannot batch or optimize executions                            |
  +------------------------------------------------------------------+


  RIGHT: Decision produces intent, separate execution
  ====================================================

  void handle_request(Request *req) {
      // Pure decision logic
      if (should_accept(req)) {
          Intent *intent = create_intent(req);
          submit_to_executor(intent);      // <-- Submit, don't execute
          update_pending_state(req);       // <-- State is "pending"
      }
  }

  void executor_thread() {
      while (true) {
          Intent *intent = dequeue_intent();
          Result result = execute(intent);
          notify_decision_layer(intent, result);  // <-- Feedback loop
      }
  }

  Benefits:
  +------------------------------------------------------------------+
  | - Decision logic is testable without I/O                         |
  | - Execution can be batched                                       |
  | - Clear failure boundary                                         |
  | - Async execution doesn't block decisions                        |
  +------------------------------------------------------------------+


+===========================================================================+
|                    SINGLE-WRITER MODEL                                    |
+===========================================================================+

  PROBLEM: Multiple writers to shared state
  =========================================

  Thread A:                    Thread B:
     |                            |
     v                            v
  +--------+                   +--------+
  | Read X |                   | Read X |
  +--------+                   +--------+
     |                            |
     | X = 10                     | X = 10
     v                            v
  +--------+                   +--------+
  | X = 15 |                   | X = 20 |
  +--------+                   +--------+
     |                            |
     | Write X                    | Write X
     v                            v
  +----------------------------------+
  |      X = 15 or 20 ???           |  <-- Race condition!
  +----------------------------------+


  SOLUTION: Single-writer state manager
  =====================================

  Thread A:                    Thread B:
     |                            |
     v                            v
  +-------------+              +-------------+
  | Request:    |              | Request:    |
  | "set X=15"  |              | "set X=20"  |
  +-------------+              +-------------+
         \                        /
          \                      /
           v                    v
  +----------------------------------+
  |        STATE MANAGER             |
  |   (Single thread / serial)       |
  |                                  |
  |   1. Receive "set X=15"          |
  |   2. Validate, apply             |
  |   3. Receive "set X=20"          |
  |   4. Validate, apply             |
  |                                  |
  |   Result: X = 20 (deterministic) |
  +----------------------------------+
```

---

## 中文解释 (Chinese Explanation)

### 2.1 决策与执行分离

**为什么决策代码不能直接执行？**

```
错误示例：
void process_event(Event *e) {
    if (should_handle(e)) {
        // 决策和执行混在一起
        send_response(socket, data);    // I/O 操作
        log_to_file(logfile, message);  // 文件操作
        update_database(db, record);    // 数据库操作
    }
}

问题：
1. 测试困难：需要 mock 所有 I/O
2. 阻塞：一个慢 I/O 阻塞所有处理
3. 错误处理复杂：每个 I/O 都可能失败
4. 无法优化：无法批量处理
```

**正确做法：引入执行边界**

```
正确示例：
// 决策层：只产生意图
void process_event(Event *e) {
    if (should_handle(e)) {
        Intent *intent = malloc(sizeof(Intent));
        intent->type = ACTION_SEND_RESPONSE;
        intent->data = data;
        
        enqueue(execution_queue, intent);  // 提交，不执行
    }
}

// 执行层：专门执行
void executor_run() {
    while (true) {
        Intent *intent = dequeue(execution_queue);
        
        int result = do_execute(intent);
        
        // 反馈给决策层
        notify_completion(intent->id, result);
    }
}
```

### 2.2 Zebra → 中央状态权威

**Zebra 在 FRR 中的角色：**

```
FRR 架构：
- bgpd 说："我学到了路由 10.0.0.0/8"
- ospfd 说："我也学到了路由 10.0.0.0/8，但度量不同"
- Zebra 决定：哪个路由胜出？安装哪个？

关键点：
- 协议守护进程不直接修改内核路由表
- 所有路由都提交给 Zebra
- Zebra 是唯一能修改 RIB 的组件
```

**通用化为状态管理器：**

```
+------------------------------------------+
|            状态管理器职责                 |
+------------------------------------------+
| 1. 接收请求（不是直接修改）               |
| 2. 验证请求合法性                         |
| 3. 解决冲突（多个请求修改同一状态）       |
| 4. 应用变更（原子性）                     |
| 5. 通知订阅者                             |
| 6. 支持状态查询                           |
+------------------------------------------+
```

**好处：**

1. **防止分裂状态（Split-Brain）**
   - 只有一个写入者
   - 状态总是一致的

2. **支持重放（Replay）**
   - 所有修改都是请求
   - 可以记录和重放

3. **支持回滚（Rollback）**
   - 保存状态历史
   - 可以恢复到之前状态

4. **便于测试**
   - 发送请求序列
   - 验证最终状态

---

## 2. Protocol Daemons → Independent Decision Engines

```
+===========================================================================+
|                    PLUGGABLE DECISION ENGINES                             |
+===========================================================================+

                    FRR MODEL
                    =========

  +----------+  +----------+  +----------+  +----------+
  |   bgpd   |  |  ospfd   |  |   ripd   |  |  isisd   |
  +----------+  +----------+  +----------+  +----------+
       |             |             |             |
       | Each is a separate process              |
       | Can crash independently                 |
       | Communicates via IPC                    |
       +-------------+-------------+-------------+
                     |
                     v
              +-------------+
              |   Zebra     |
              +-------------+


                    GENERIC MODEL
                    =============

  +----------+  +----------+  +----------+  +----------+
  | Engine A |  | Engine B |  | Engine C |  | Engine D |
  +----------+  +----------+  +----------+  +----------+
       |             |             |             |
       | Isolated execution contexts             |
       | Well-defined interfaces                 |
       | Independent failure domains             |
       +-------------+-------------+-------------+
                     |
                     v
              +-------------+
              | State Mgr   |
              +-------------+


  Engine Isolation Options:
  =========================

  +------------------------------------------------------------------+
  | Level        | Isolation    | Overhead  | Use When               |
  +------------------------------------------------------------------+
  | Process      | Full         | High      | Untrusted, crash-prone |
  | Thread       | Memory only  | Medium    | Trusted, CPU-bound     |
  | Coroutine    | Cooperative  | Low       | Trusted, I/O-bound     |
  | Module       | None (same)  | Minimal   | Tightly coupled        |
  +------------------------------------------------------------------+


  Engine Interface Design:
  ========================

  // Clean interface - engine knows nothing about internals
  struct engine_ops {
      int (*init)(struct engine_ctx *ctx);
      int (*shutdown)(struct engine_ctx *ctx);
      
      // Receive events from state manager
      int (*on_state_change)(struct engine_ctx *ctx, 
                             const struct state_delta *delta);
      
      // Engine produces intents
      int (*process)(struct engine_ctx *ctx);
  };

  // Registration
  void register_engine(const char *name, const struct engine_ops *ops);
```

---

## 中文解释 (Chinese Explanation)

### 2.3 协议守护进程 → 可插拔决策引擎

**为什么引擎需要隔离？**

```
问题场景：
- 引擎 A 有 bug，发生崩溃
- 如果不隔离：整个系统崩溃
- 如果隔离：只有引擎 A 失败，其他继续工作

FRR 的做法：
- 每个协议是独立进程
- bgpd 崩溃不影响 ospfd
- 可以单独重启 bgpd
```

**通用引擎接口设计：**

```c
// 引擎操作接口
struct engine_ops {
    // 生命周期
    int (*init)(void *config);
    int (*shutdown)(void);
    
    // 接收状态变化通知
    int (*on_state_change)(const struct state_change *change);
    
    // 主处理循环
    int (*process)(void);
    
    // 健康检查
    int (*health_check)(void);
};

// 使用示例
static struct engine_ops my_engine = {
    .init = my_init,
    .shutdown = my_shutdown,
    .on_state_change = my_handle_change,
    .process = my_process,
    .health_check = my_check,
};

register_engine("my_engine", &my_engine);
```

---

## 3. Event-Driven Core → Deterministic Execution Model

```
+===========================================================================+
|                    EVENT-DRIVEN EXECUTION                                 |
+===========================================================================+

  WRONG: Ad-hoc threading
  =======================

  main() {
      pthread_create(&t1, handle_network);   // Thread for network
      pthread_create(&t2, handle_timers);    // Thread for timers
      pthread_create(&t3, handle_signals);   // Thread for signals
      pthread_create(&t4, handle_files);     // Thread for file events
      
      // Chaos: 4 threads accessing shared state
      // Locks everywhere, still has races
  }


  RIGHT: Event loop with explicit dispatch
  ========================================

  +---------------------------------------------------------------------+
  |                         EVENT LOOP                                  |
  +---------------------------------------------------------------------+
  |                                                                     |
  |   +----------------+                                                |
  |   |  poll/epoll    |  <-- Wait for ANY event                        |
  |   +----------------+                                                |
  |          |                                                          |
  |          v                                                          |
  |   +----------------+                                                |
  |   |  Classify      |                                                |
  |   |  Event Type    |                                                |
  |   +----------------+                                                |
  |          |                                                          |
  |   +------+------+------+------+                                     |
  |   |      |      |      |      |                                     |
  |   v      v      v      v      v                                     |
  | Timer  I/O   Signal  IPC   Idle                                     |
  | handler handler handler handler handler                             |
  |   |      |      |      |      |                                     |
  |   +------+------+------+------+                                     |
  |          |                                                          |
  |          v                                                          |
  |   +----------------+                                                |
  |   |  Process ONE   |  <-- Sequential, deterministic                 |
  |   |  event at time |                                                |
  |   +----------------+                                                |
  |          |                                                          |
  |          +---> Loop back to poll                                    |
  |                                                                     |
  +---------------------------------------------------------------------+

  FRR Event System (from lib/event.c):
  ====================================

  struct event {
      int type;              // Timer, I/O, etc.
      int fd;                // File descriptor (if I/O)
      void (*func)(struct event *);  // Handler
      void *arg;             // User data
      struct timeval when;   // When to fire (if timer)
  };

  // Single-threaded processing
  void event_loop(struct event_loop *loop) {
      while (!loop->terminate) {
          // 1. Calculate next timeout
          struct timeval *timeout = get_next_timer(loop);
          
          // 2. Wait for events
          int n = poll(loop->fds, loop->nfds, timeout_ms);
          
          // 3. Process ready events (ONE AT A TIME)
          for each ready fd:
              call_handler(fd);
          
          // 4. Process expired timers (ONE AT A TIME)
          for each expired timer:
              call_timer_handler(timer);
      }
  }
```

---

## 中文解释 (Chinese Explanation)

### 2.4 事件驱动核心 → 确定性执行模型

**为什么避免 ad-hoc 多线程？**

```
问题：
1. 锁的复杂性
   - 需要锁保护每个共享数据
   - 容易死锁
   - 容易忘记加锁

2. 调试困难
   - 竞态条件难以复现
   - 日志顺序不确定
   - 状态快照不一致

3. 推理困难
   - 代码执行顺序不确定
   - 难以验证正确性
```

**事件驱动模型的优势：**

```
单线程事件循环：

while (true) {
    // 1. 等待事件（阻塞点）
    events = poll(fds, timeout);
    
    // 2. 按顺序处理（确定性）
    for each event in events:
        handler = find_handler(event);
        handler(event);  // 同步执行
    
    // 3. 处理到期定时器
    for each expired timer:
        timer->callback(timer);
}

特点：
- 同一时刻只执行一个处理器
- 处理器执行完才处理下一个
- 状态变更是原子的
- 日志顺序与执行顺序一致
```

---

## 4. FSM-Centric Design → Explicit State Machines

```
+===========================================================================+
|                    EXPLICIT STATE MACHINES                                |
+===========================================================================+

  IMPLICIT STATE (BAD):
  =====================

  // State scattered across multiple variables
  bool connected;
  bool authenticated;
  bool ready;
  int retry_count;
  time_t last_attempt;

  void handle_event(Event e) {
      if (connected && authenticated && ready) {
          // ...
      } else if (connected && !authenticated) {
          // ...
      } else if (!connected && retry_count < MAX) {
          // ...
      }
      // Combinatorial explosion!
  }


  EXPLICIT FSM (GOOD):
  ====================

  +-----------------------------------------------------------------------+
  |                        CONNECTION FSM                                 |
  +-----------------------------------------------------------------------+
  |                                                                       |
  |     +----------+     connect_ok      +-------------+                  |
  |     |          |-------------------->|             |                  |
  |     |  INIT    |                     | CONNECTED   |                  |
  |     |          |<--------------------|             |                  |
  |     +----------+     disconnect      +-------------+                  |
  |          |                                 |                          |
  |          | start                           | auth_ok                  |
  |          v                                 v                          |
  |     +----------+                     +-------------+                  |
  |     |          |                     |             |                  |
  |     | CONNECTING|                    | AUTHENTICATED|                 |
  |     |          |                     |             |                  |
  |     +----------+                     +-------------+                  |
  |          |                                 |                          |
  |          | timeout                         | ready_ok                 |
  |          v                                 v                          |
  |     +----------+                     +-------------+                  |
  |     | RETRY    |                     | OPERATIONAL |                  |
  |     +----------+                     +-------------+                  |
  |                                                                       |
  +-----------------------------------------------------------------------+


  FSM Implementation Pattern:
  ===========================

  typedef enum {
      STATE_INIT,
      STATE_CONNECTING,
      STATE_CONNECTED,
      STATE_AUTHENTICATED,
      STATE_OPERATIONAL,
      STATE_RETRY,
      STATE_MAX
  } fsm_state_t;

  typedef enum {
      EVENT_START,
      EVENT_CONNECT_OK,
      EVENT_AUTH_OK,
      EVENT_READY_OK,
      EVENT_DISCONNECT,
      EVENT_TIMEOUT,
      EVENT_MAX
  } fsm_event_t;

  // Transition table - THE source of truth
  struct fsm_transition {
      fsm_state_t from;
      fsm_event_t event;
      fsm_state_t to;
      void (*action)(struct fsm *fsm);
  };

  static const struct fsm_transition transitions[] = {
      { STATE_INIT,       EVENT_START,      STATE_CONNECTING, do_connect },
      { STATE_CONNECTING, EVENT_CONNECT_OK, STATE_CONNECTED,  do_auth },
      { STATE_CONNECTING, EVENT_TIMEOUT,    STATE_RETRY,      do_wait },
      { STATE_CONNECTED,  EVENT_AUTH_OK,    STATE_AUTHENTICATED, do_ready },
      // ... etc
      { STATE_MAX, EVENT_MAX, STATE_MAX, NULL }  // End marker
  };

  void fsm_handle_event(struct fsm *fsm, fsm_event_t event) {
      for (const struct fsm_transition *t = transitions; 
           t->from != STATE_MAX; t++) {
          if (t->from == fsm->state && t->event == event) {
              fsm_state_t old = fsm->state;
              fsm->state = t->to;
              if (t->action)
                  t->action(fsm);
              log("FSM: %s + %s -> %s", 
                  state_name(old), event_name(event), state_name(t->to));
              return;
          }
      }
      log("FSM: Invalid transition %s + %s", 
          state_name(fsm->state), event_name(event));
  }
```

---

## 中文解释 (Chinese Explanation)

### 2.5 显式状态机设计

**隐式状态的问题：**

```c
// 坏的例子：状态分散在多个变量中
struct connection {
    bool connected;
    bool authenticated;
    bool ready;
    int retry_count;
    time_t last_attempt;
};

// 处理事件时需要检查多个变量组合
if (conn->connected && conn->authenticated && conn->ready) {
    // 情况 1
} else if (conn->connected && !conn->authenticated) {
    // 情况 2
} else if (!conn->connected && conn->retry_count < MAX) {
    // 情况 3
}
// 组合爆炸！
```

**显式 FSM 的优势：**

```c
// 好的例子：状态是枚举值，只有一个
enum state {
    STATE_INIT,
    STATE_CONNECTING,
    STATE_CONNECTED,
    STATE_AUTHENTICATED,
    STATE_OPERATIONAL,
    STATE_RETRY
};

struct connection {
    enum state state;  // 只有一个状态变量
};

// 状态转换清晰定义
switch (conn->state) {
    case STATE_INIT:
        // 只处理这个状态的事件
        break;
    case STATE_CONNECTING:
        // ...
        break;
}
```

**何时引入 FSM？**

```
需要 FSM 的信号：
1. 有多个 bool 变量表示"阶段"
2. 代码中有 if-else 链检查多个条件
3. 存在"只在某些条件下有效"的操作
4. 需要处理超时、重试等生命周期事件
5. 调试时需要知道"当前处于什么状态"

FSM 设计原则：
1. 状态是互斥的（同一时刻只能在一个状态）
2. 转换是显式的（列出所有合法转换）
3. 事件驱动（状态变化由事件触发）
4. 动作在转换时执行
```

---

## 5. Summary: Translation Table

| FRR Concept | Generic Principle | Key Benefit |
|-------------|-------------------|-------------|
| Control/Data Plane | Decision/Execution Split | Testability |
| Zebra | State Authority | Consistency |
| Protocol Daemons | Pluggable Engines | Isolation |
| Event Loop | Deterministic Core | Debuggability |
| FSMs | Explicit State | Observability |
| RCU (frrcu.h) | Safe Deferred Cleanup | Concurrency Safety |
| ZAPI | Request-based API | Decoupling |
| VTY/mgmtd | Management Plane | Operability |

---

## Next: Part 3 - WHAT (Concrete Building Blocks)
