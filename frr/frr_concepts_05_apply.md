# Extracting FRR Architecture - Part 5: APPLY

## Applying the Mapping in Real Projects

This document provides practical guidance for applying FRR-inspired
architecture, including common mistakes to avoid.

---

## 1. Step-by-Step Application Plan

```
+===========================================================================+
|                    PRACTICAL APPLICATION PLAN                             |
+===========================================================================+

                    STEP 1: IDENTIFY EXECUTION SIDE EFFECTS
                    =======================================

  What to look for:
  +------------------------------------------------------------------+
  |  Category        | Examples                                      |
  +------------------------------------------------------------------+
  |  Network I/O     | send(), recv(), connect(), accept()           |
  |  File I/O        | open(), read(), write(), close()              |
  |  Process Control | fork(), exec(), kill(), wait()                |
  |  System Calls    | ioctl(), mmap(), mlock()                      |
  |  Time Effects    | sleep(), nanosleep(), clock_settime()         |
  |  External Comms  | Database calls, RPC, message queue ops        |
  +------------------------------------------------------------------+

  Action: Mark each occurrence with a comment
  +------------------------------------------------------------------+
  |  void process_request(struct request *req) {                     |
  |      if (validate(req)) {                                        |
  |          // SIDE_EFFECT: network I/O                             |
  |          send(sock, req->response, req->len, 0);                 |
  |      }                                                           |
  |  }                                                               |
  +------------------------------------------------------------------+


                    STEP 2: ISOLATE DECISION LOGIC
                    ==============================

  Before:
  +------------------------------------------------------------------+
  |  void handle_packet(struct packet *pkt) {                        |
  |      // Decision: which route to use?                            |
  |      struct route *r = lookup_route(pkt->dst);                   |
  |      if (!r) {                                                   |
  |          r = get_default_route();                                |
  |      }                                                           |
  |                                                                  |
  |      // Decision: should we forward?                             |
  |      if (r->flags & ROUTE_FORWARD) {                             |
  |          // Execution mixed in!                                  |
  |          send(r->interface, pkt->data, pkt->len);                |
  |      } else {                                                    |
  |          // More execution!                                      |
  |          drop_and_log(pkt);                                      |
  |      }                                                           |
  |  }                                                               |
  +------------------------------------------------------------------+

  After:
  +------------------------------------------------------------------+
  |  // Pure decision function - no side effects                     |
  |  struct forward_decision decide_forward(struct packet *pkt,      |
  |                                         struct route_table *rt) {|
  |      struct forward_decision d = {0};                            |
  |                                                                  |
  |      struct route *r = lookup_route(rt, pkt->dst);               |
  |      if (!r) {                                                   |
  |          r = get_default_route(rt);                              |
  |      }                                                           |
  |                                                                  |
  |      if (r && (r->flags & ROUTE_FORWARD)) {                      |
  |          d.action = ACTION_FORWARD;                              |
  |          d.interface = r->interface;                             |
  |      } else {                                                    |
  |          d.action = ACTION_DROP;                                 |
  |          d.reason = DROP_NO_ROUTE;                               |
  |      }                                                           |
  |                                                                  |
  |      return d;  // Return decision, don't execute                |
  |  }                                                               |
  |                                                                  |
  |  // Execution happens elsewhere                                  |
  |  void execute_decision(struct forward_decision *d,               |
  |                        struct packet *pkt) {                     |
  |      switch (d->action) {                                        |
  |      case ACTION_FORWARD:                                        |
  |          send(d->interface, pkt->data, pkt->len);                |
  |          break;                                                  |
  |      case ACTION_DROP:                                           |
  |          log_drop(pkt, d->reason);                               |
  |          break;                                                  |
  |      }                                                           |
  |  }                                                               |
  +------------------------------------------------------------------+


                    STEP 3: INTRODUCE STATE AUTHORITY
                    =================================

  Before: Scattered state
  +------------------------------------------------------------------+
  |  // In routes.c                                                  |
  |  static struct route routes[MAX_ROUTES];                         |
  |  static int num_routes = 0;                                      |
  |                                                                  |
  |  // In config.c                                                  |
  |  static int max_routes = 1000;                                   |
  |                                                                  |
  |  // In stats.c                                                   |
  |  static int route_lookups = 0;                                   |
  |  static int route_hits = 0;                                      |
  +------------------------------------------------------------------+

  After: Centralized
  +------------------------------------------------------------------+
  |  // In state/routing_state.h                                     |
  |  struct routing_state {                                          |
  |      // Configuration (from config)                              |
  |      int max_routes;                                             |
  |                                                                  |
  |      // Authoritative state                                      |
  |      struct route *routes;                                       |
  |      int num_routes;                                             |
  |                                                                  |
  |      // Derived/stats (can be recalculated)                      |
  |      int route_lookups;                                          |
  |      int route_hits;                                             |
  |                                                                  |
  |      // Subscribers                                              |
  |      struct subscriber *subscribers;                             |
  |  };                                                              |
  |                                                                  |
  |  // Access only through functions                                |
  |  struct routing_state *routing_state_get(void);                  |
  |  int routing_add_route(struct routing_state *s,                  |
  |                        const struct route *r);                   |
  |  int routing_del_route(struct routing_state *s,                  |
  |                        const struct route_id *id);               |
  +------------------------------------------------------------------+


                    STEP 4: CONVERT LOGIC TO FSMs
                    =============================

  Identify FSM candidates:
  +------------------------------------------------------------------+
  |  SIGN                          | CONVERT TO FSM?                 |
  +------------------------------------------------------------------+
  |  Multiple bool flags for state | YES - combine into single enum  |
  |  if-else chains with state     | YES - use transition table      |
  |  Timeouts and retries          | YES - explicit timeout states   |
  |  Connection lifecycle          | YES - classic FSM use case      |
  |  Simple one-shot logic         | NO  - overkill                  |
  |  Stateless transformations     | NO  - not applicable            |
  +------------------------------------------------------------------+

  Example conversion:
  +------------------------------------------------------------------+
  |  // Before: implicit state                                       |
  |  struct connection {                                             |
  |      bool connected;                                             |
  |      bool authenticated;                                         |
  |      bool handshake_done;                                        |
  |      int retry_count;                                            |
  |  };                                                              |
  |                                                                  |
  |  // After: explicit FSM                                          |
  |  enum conn_state {                                               |
  |      CONN_INIT,                                                  |
  |      CONN_CONNECTING,                                            |
  |      CONN_CONNECTED,                                             |
  |      CONN_AUTHENTICATING,                                        |
  |      CONN_READY,                                                 |
  |      CONN_FAILED,                                                |
  |      CONN_CLOSED                                                 |
  |  };                                                              |
  |                                                                  |
  |  struct connection {                                             |
  |      enum conn_state state;  // Single state variable            |
  |      int retry_count;        // Kept for retry logic             |
  |  };                                                              |
  +------------------------------------------------------------------+


                    STEP 5: ADD EVENT LOOP
                    ======================

  Minimum viable event loop:
  +------------------------------------------------------------------+
  |  struct event_loop {                                             |
  |      int epoll_fd;                                               |
  |      bool running;                                               |
  |      struct timer_heap *timers;                                  |
  |  };                                                              |
  |                                                                  |
  |  void event_loop_run(struct event_loop *loop) {                  |
  |      while (loop->running) {                                     |
  |          // 1. Calculate next timer                              |
  |          int timeout_ms = get_next_timer_ms(loop->timers);       |
  |                                                                  |
  |          // 2. Wait for events                                   |
  |          struct epoll_event events[MAX_EVENTS];                  |
  |          int n = epoll_wait(loop->epoll_fd, events,              |
  |                             MAX_EVENTS, timeout_ms);             |
  |                                                                  |
  |          // 3. Handle I/O events                                 |
  |          for (int i = 0; i < n; i++) {                           |
  |              struct event_handler *h = events[i].data.ptr;       |
  |              h->callback(h, events[i].events);                   |
  |          }                                                       |
  |                                                                  |
  |          // 4. Handle expired timers                             |
  |          process_expired_timers(loop->timers);                   |
  |      }                                                           |
  |  }                                                               |
  +------------------------------------------------------------------+
```

---

## 中文解释 (Chinese Explanation)

### 5.1 实施步骤详解

**步骤 1：标识执行副作用**

```
在代码中搜索并标记所有副作用：

// SIDE_EFFECT: network I/O
send(sock, data, len, 0);

// SIDE_EFFECT: file I/O
write(fd, buffer, size);

// SIDE_EFFECT: database
db_insert(conn, record);

目的：
- 了解副作用分布
- 为后续隔离做准备
```

**步骤 2：隔离决策逻辑**

```
原则：决策函数只做决策，不执行

// 输入：当前状态 + 事件
// 输出：决策结果
// 无副作用

struct decision decide(const struct state *s, 
                       const struct event *e) {
    struct decision d = {0};
    
    // 纯逻辑，无 I/O
    if (condition(s, e)) {
        d.action = ACTION_A;
    } else {
        d.action = ACTION_B;
    }
    
    return d;  // 返回决策，不执行
}
```

**步骤 3：引入状态权威**

```
1. 找到所有分散的状态
2. 创建集中的状态结构
3. 提供访问函数
4. 添加验证和通知

关键：只有状态管理器能修改状态
其他模块只能提交请求
```

**步骤 4：转换为 FSM**

```
何时需要 FSM？

需要：
- 有多个 bool 表示阶段
- 有复杂的 if-else 链
- 有超时和重试
- 有生命周期管理

不需要：
- 简单的一次性逻辑
- 无状态转换
```

**步骤 5：添加事件循环**

```
最小事件循环：

while (running) {
    // 1. 计算下次定时器
    timeout = get_next_timer();
    
    // 2. 等待事件
    events = epoll_wait(epfd, timeout);
    
    // 3. 处理 I/O 事件
    for each event:
        handler(event);
    
    // 4. 处理到期定时器
    process_timers();
}
```

---

## 2. Common Mistakes When Copying FRR

```
+===========================================================================+
|                    ANTI-PATTERNS TO AVOID                                 |
+===========================================================================+

                    MISTAKE 1: OVER-ABSTRACTION
                    ===========================

  BAD: Abstracting everything
  +------------------------------------------------------------------+
  |  // Too many layers for simple operation                         |
  |  struct abstract_factory {                                       |
  |      struct abstract_builder *(*create_builder)(void);           |
  |  };                                                              |
  |                                                                  |
  |  struct abstract_builder {                                       |
  |      struct abstract_product *(*build)(void);                    |
  |  };                                                              |
  |                                                                  |
  |  // Just to create a connection object...                        |
  |  struct connection *c = factory->create_builder()                |
  |                               ->build()                          |
  |                               ->get_connection();                |
  +------------------------------------------------------------------+

  GOOD: Direct and simple
  +------------------------------------------------------------------+
  |  struct connection *connection_create(const char *host, int port)|
  |  {                                                               |
  |      struct connection *c = malloc(sizeof(*c));                  |
  |      if (!c) return NULL;                                        |
  |      // Initialize directly                                      |
  |      c->host = strdup(host);                                     |
  |      c->port = port;                                             |
  |      c->state = CONN_INIT;                                       |
  |      return c;                                                   |
  |  }                                                               |
  +------------------------------------------------------------------+


                    MISTAKE 2: OVER-MODULARIZATION
                    ==============================

  BAD: Too many tiny modules
  +------------------------------------------------------------------+
  |  project/                                                        |
  |    src/                                                          |
  |      core/                                                       |
  |        event/                                                    |
  |          loop/                                                   |
  |            internal/                                             |
  |              impl/                                               |
  |                event_loop.c  (50 lines)                          |
  +------------------------------------------------------------------+

  GOOD: Reasonable grouping
  +------------------------------------------------------------------+
  |  project/                                                        |
  |    src/                                                          |
  |      core/                                                       |
  |        event.c    (200 lines - event loop + timers)              |
  |        log.c      (100 lines)                                    |
  |        memory.c   (150 lines)                                    |
  +------------------------------------------------------------------+


                    MISTAKE 3: COPYING MULTI-PROCESS DESIGN
                    =======================================

  FRR uses multiple processes because:
  - Different protocols have independent lifecycles
  - Protocol failures should be isolated
  - Historical reasons (Zebra/Quagga heritage)

  Your project probably doesn't need this unless:
  - You have truly independent subsystems
  - You need process-level isolation for security
  - Different components have vastly different reliability

  SINGLE PROCESS IS FINE for most applications!
  +------------------------------------------------------------------+
  |  // Instead of multiple processes...                             |
  |  // Use in-process isolation:                                    |
  |                                                                  |
  |  struct engine {                                                 |
  |      const char *name;                                           |
  |      void *private_data;  // Engine's private state              |
  |      // Engine cannot access other engines' data                 |
  |  };                                                              |
  |                                                                  |
  |  // Engines communicate only via messages                        |
  |  engine_send(engine_a, engine_b, &message);                      |
  +------------------------------------------------------------------+


                    MISTAKE 4: BLIND API REUSE
                    ==========================

  BAD: Copying FRR APIs without understanding
  +------------------------------------------------------------------+
  |  // Copied from FRR without understanding why                    |
  |  struct zclient *zclient_new(struct event_loop *master);         |
  |  void zclient_free(struct zclient *zclient);                     |
  |  int zclient_connect(struct zclient *zclient,                    |
  |                      const struct sockaddr *addr);               |
  |                                                                  |
  |  // But your system doesn't have a Zebra!                        |
  |  // This API makes no sense in your context.                     |
  +------------------------------------------------------------------+

  GOOD: Design APIs for your domain
  +------------------------------------------------------------------+
  |  // APIs that make sense for YOUR system                         |
  |  struct job_client *job_client_new(struct event_loop *loop);     |
  |  int job_submit(struct job_client *c, const struct job *job);    |
  |  int job_cancel(struct job_client *c, job_id_t id);              |
  |                                                                  |
  |  // Inspired by FRR's client pattern, but domain-specific        |
  +------------------------------------------------------------------+
```

---

## 中文解释 (Chinese Explanation)

### 5.2 常见错误

**错误 1：过度抽象**

```
症状：
- 创建一个对象需要经过多层工厂
- 每个类只有几行代码
- 难以追踪实际逻辑

解决：
- 从简单开始
- 只在真正需要时抽象
- 保持代码可读
```

**错误 2：过度模块化**

```
症状：
- 目录嵌套很深
- 每个文件只有几十行
- 找代码需要穿越多层目录

解决：
- 相关代码放一起
- 文件大小适中（100-500 行）
- 目录层级不超过 2-3 层
```

**错误 3：盲目复制多进程设计**

```
FRR 使用多进程的原因：
1. 不同协议独立生命周期
2. 故障隔离
3. 历史原因

你的项目可能不需要！

单进程 + 模块隔离通常足够：
- 每个模块有私有数据
- 模块间只通过消息通信
- 更简单，更容易调试
```

**错误 4：盲目复用 FRR API**

```
错误做法：
- 复制 zclient API
- 但你的系统没有 Zebra
- API 在你的上下文中没有意义

正确做法：
- 理解 FRR 的设计模式
- 设计适合你领域的 API
- 受 FRR 启发，但不是复制
```

---

## 3. When NOT to Use FRR-Inspired Architecture

```
+===========================================================================+
|                    WHEN SIMPLER IS BETTER                                 |
+===========================================================================+

  +------------------------------------------------------------------+
  |  SITUATION                    | RECOMMENDATION                   |
  +------------------------------------------------------------------+
  |  Short-lived process          | Direct synchronous code          |
  |  (< 1 minute runtime)         |                                  |
  +------------------------------------------------------------------+
  |  Simple request-response      | Basic loop, no event system      |
  |  (one client at a time)       |                                  |
  +------------------------------------------------------------------+
  |  Stateless transformation     | Functional approach              |
  |  (input → process → output)   |                                  |
  +------------------------------------------------------------------+
  |  Prototype / POC              | Whatever is fastest              |
  |                               |                                  |
  +------------------------------------------------------------------+
  |  Team unfamiliar with         | Train first, or use simpler      |
  |  event-driven design          | approach                         |
  +------------------------------------------------------------------+


  COMPLEXITY COST vs BENEFIT:
  ===========================

              ^
    Benefit   |                              FRR-style
              |                             architecture
              |                           ****
              |                       ****
              |                    ***
              |                 ***
              |              ***
              |           *** <-- Crossover point
              |        ***
              |     ***       Simple
              |  ***          synchronous
              |**             code
              +---------------------------------->
                        System Complexity

  Below the crossover point: simpler is better
  Above the crossover point: FRR-style pays off


  DECISION FLOWCHART:
  ===================

  +-- Does it run for hours/days? --+
  |                                 |
  NO                               YES
  |                                 |
  v                                 v
  Simple code                 +-- Multiple input sources? --+
                              |                              |
                             NO                             YES
                              |                              |
                              v                              v
                         Simple code               +-- Complex state? --+
                                                   |                    |
                                                  NO                   YES
                                                   |                    |
                                                   v                    v
                                              Event loop           FRR-style
                                              (but simpler)        architecture
```

---

## 中文解释 (Chinese Explanation)

### 5.3 何时不使用 FRR 架构

**简单更好的情况：**

```
1. 短期运行的程序
   - 运行时间 < 1 分钟
   - 用同步代码即可

2. 简单的请求-响应
   - 一次处理一个请求
   - 不需要事件循环

3. 无状态转换
   - 输入 → 处理 → 输出
   - 没有需要管理的状态

4. 原型/概念验证
   - 先让它工作
   - 后续再重构

5. 团队不熟悉事件驱动
   - 先培训
   - 或使用更简单的方法
```

**决策流程图：**

```
是否运行数小时/数天？
├── 否 → 简单代码
└── 是 → 是否有多个输入源？
         ├── 否 → 简单代码
         └── 是 → 是否有复杂状态？
                  ├── 否 → 简单事件循环
                  └── 是 → FRR 式架构
```

---

## 4. Quick Reference

### When to apply each pattern:

| Pattern | Apply When | Skip When |
|---------|------------|-----------|
| State Authority | Multiple writers, consistency needed | Single writer, simple state |
| Decision/Execution Split | I/O in decision code | No I/O in decisions |
| Event Loop | Multiple async inputs | Single sync input |
| FSMs | Complex state transitions | Simple conditions |
| Message Passing | Need process separation | All in one process |
| RCU | Read-heavy, concurrent access | Single-threaded |

---

## Next: Part 6 - VERIFY (Architecture Validation)
