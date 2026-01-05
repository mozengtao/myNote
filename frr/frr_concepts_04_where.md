# Extracting FRR Architecture - Part 4: WHERE

## Mapping to Your Own Codebase

This document provides a practical guide for identifying FRR-like roles in
your existing project and restructuring it according to FRR principles.

---

## 1. Identifying Roles in Your Project

```
+===========================================================================+
|                    ROLE IDENTIFICATION CHECKLIST                          |
+===========================================================================+

                    FINDING YOUR "ZEBRA" (State Authority)
                    ======================================

  Ask these questions about your codebase:

  +------------------------------------------------------------------+
  |  QUESTION                          | IF YES, THAT'S YOUR "ZEBRA" |
  +------------------------------------------------------------------+
  |  Which module holds the "main"     |                             |
  |  data that others query?           |  --> State Manager          |
  +------------------------------------------------------------------+
  |  Which module do you debug first   |                             |
  |  when state is wrong?              |  --> State Authority        |
  +------------------------------------------------------------------+
  |  Which module validates data       |                             |
  |  before it's stored?               |  --> State Manager          |
  +------------------------------------------------------------------+
  |  Which module sends notifications  |                             |
  |  when things change?               |  --> Central Coordinator    |
  +------------------------------------------------------------------+


  WARNING: If the answer is "multiple" or "unclear", you have a
  split-brain architecture. This is your first refactoring target.


                    FINDING YOUR "PROTOCOL DAEMONS" (Decision Engines)
                    ===================================================

  +------------------------------------------------------------------+
  |  QUESTION                          | IF YES, THAT'S AN ENGINE    |
  +------------------------------------------------------------------+
  |  Which modules make independent    |                             |
  |  decisions about what to do?       |  --> Decision Engine        |
  +------------------------------------------------------------------+
  |  Which modules could run in        |                             |
  |  isolation (with mock data)?       |  --> Pluggable Engine       |
  +------------------------------------------------------------------+
  |  Which modules implement           |                             |
  |  different "policies" or "modes"?  |  --> Strategy Engine        |
  +------------------------------------------------------------------+
  |  Which modules talk to external    |                             |
  |  systems for input?                |  --> Protocol Engine        |
  +------------------------------------------------------------------+


                    FINDING YOUR "KERNEL" (Execution Backend)
                    =========================================

  +------------------------------------------------------------------+
  |  QUESTION                          | IF YES, THAT'S BACKEND      |
  +------------------------------------------------------------------+
  |  Which modules do actual I/O?      |  --> Execution Backend      |
  |  (files, network, devices)         |                             |
  +------------------------------------------------------------------+
  |  Which modules make system calls?  |  --> System Backend         |
  +------------------------------------------------------------------+
  |  Which modules talk to hardware?   |  --> Hardware Backend       |
  +------------------------------------------------------------------+
  |  Which modules have side effects   |                             |
  |  visible outside the process?      |  --> Execution Backend      |
  +------------------------------------------------------------------+


                    FINDING YOUR "VTYSH" (Management Plane)
                    =======================================

  +------------------------------------------------------------------+
  |  QUESTION                          | IF YES, THAT'S MANAGEMENT   |
  +------------------------------------------------------------------+
  |  How do humans configure this?     |  --> Config Interface       |
  +------------------------------------------------------------------+
  |  How do humans monitor this?       |  --> Monitoring Interface   |
  +------------------------------------------------------------------+
  |  How do scripts/tools interact?    |  --> API Interface          |
  +------------------------------------------------------------------+
  |  Where is the CLI implemented?     |  --> Management Plane       |
  +------------------------------------------------------------------+


+===========================================================================+
|                    EXAMPLE: MAPPING A REAL SYSTEM                         |
+===========================================================================+

  Example: A Job Scheduler System
  ===============================

  Before analysis:
  +------------------------------------------------------------------+
  | scheduler.c    - 3000 lines, does everything                     |
  | jobs.c         - 1500 lines, job definition and execution        |
  | workers.c      - 1000 lines, worker management                   |
  | api.c          - 500 lines, REST API                             |
  | main.c         - 200 lines, startup                              |
  +------------------------------------------------------------------+


  After role identification:
  +------------------------------------------------------------------+
  |                                                                  |
  |  scheduler.c --> Mixed! Contains:                                |
  |    - Job queue management (State)                                |
  |    - Scheduling policy (Engine)                                  |
  |    - Worker dispatch (Backend)                                   |
  |                                                                  |
  |  jobs.c --> Mixed! Contains:                                     |
  |    - Job data structures (State)                                 |
  |    - Job execution (Backend)                                     |
  |                                                                  |
  |  workers.c --> Mixed! Contains:                                  |
  |    - Worker pool state (State)                                   |
  |    - Worker communication (Backend)                              |
  |                                                                  |
  |  api.c --> Management plane (relatively clean)                   |
  |                                                                  |
  +------------------------------------------------------------------+


  Target architecture:
  +------------------------------------------------------------------+
  |                                                                  |
  |  state/                                                          |
  |    job_store.c      - Job queue, job state                       |
  |    worker_pool.c    - Worker state                               |
  |    scheduler_state.c - Overall scheduler state                   |
  |                                                                  |
  |  engines/                                                        |
  |    fifo_scheduler.c   - FIFO scheduling policy                   |
  |    priority_scheduler.c - Priority-based policy                  |
  |    fair_scheduler.c   - Fair share policy                        |
  |                                                                  |
  |  backend/                                                        |
  |    job_executor.c     - Actually run jobs                        |
  |    worker_comm.c      - Communicate with workers                 |
  |                                                                  |
  |  mgmt/                                                           |
  |    api.c              - REST API                                 |
  |    cli.c              - Command line                             |
  |                                                                  |
  |  core/                                                           |
  |    event.c            - Event loop                               |
  |    log.c              - Logging                                  |
  |                                                                  |
  +------------------------------------------------------------------+
```

---

## 中文解释 (Chinese Explanation)

### 4.1 识别你项目中的角色

**找到你的 "Zebra"（状态权威）：**

```
问自己这些问题：

1. 哪个模块持有"主要数据"？
   - 如果其他模块需要查询数据，它们去哪里？
   - 那个地方就是你的状态权威

2. 状态出问题时，你先调试哪里？
   - 你的直觉告诉你问题在哪
   - 那个地方应该是状态权威

3. 哪个模块验证数据？
   - 数据在哪里被检查是否合法？
   - 验证者应该是状态权威

警告信号：
- 如果答案是"好几个地方"→ 分裂状态，需要重构
- 如果答案是"不确定"→ 架构不清晰，需要梳理
```

**找到你的 "协议守护进程"（决策引擎）：**

```
特征：
- 做决策，但不直接执行
- 可以独立测试（给假数据）
- 实现某种策略或算法
- 从外部获取输入

例子：
- 调度算法模块
- 路由策略模块
- 规则引擎模块
- 事件处理器模块
```

**找到你的 "内核"（执行后端）：**

```
特征：
- 执行实际 I/O（文件、网络、设备）
- 调用系统 API
- 有外部可见的副作用
- 可能失败或阻塞

例子：
- 数据库写入模块
- 网络通信模块
- 硬件控制模块
- 文件操作模块
```

---

## 2. Refactoring Strategy

```
+===========================================================================+
|                    INCREMENTAL REFACTORING PLAN                           |
+===========================================================================+

                    PHASE 1: EXTRACT STATE
                    ======================

  Goal: Create a single state authority

  Step 1.1: Identify all state locations
  +------------------------------------------------------------------+
  | grep -r "static.*=" src/                                         |
  | grep -r "global" src/                                            |
  | Look for: global variables, static variables, singleton patterns |
  +------------------------------------------------------------------+

  Step 1.2: Categorize state
  +------------------------------------------------------------------+
  | Type               | Example                | Action              |
  +------------------------------------------------------------------+
  | Configuration      | max_connections        | Move to config      |
  | Authoritative      | active_connections     | Move to state mgr   |
  | Cache/Derived      | connection_count_cache | Keep, mark as cache |
  | Transient          | temp_buffer            | Keep local          |
  +------------------------------------------------------------------+

  Step 1.3: Create state manager
  +------------------------------------------------------------------+
  |  // Before: scattered state                                      |
  |  static int max_conn;  // in server.c                            |
  |  static struct conn *conns;  // in connection.c                  |
  |  static int active;  // in stats.c                               |
  |                                                                  |
  |  // After: centralized                                           |
  |  struct state_manager {                                          |
  |      int max_connections;                                        |
  |      struct conn *connections;                                   |
  |      int active_count;                                           |
  |  };                                                              |
  |  static struct state_manager *g_state;  // One global            |
  +------------------------------------------------------------------+


                    PHASE 2: INTRODUCE REQUEST LAYER
                    ================================

  Goal: Replace direct mutations with requests

  Step 2.1: Identify mutations
  +------------------------------------------------------------------+
  | Look for:                                                        |
  | - Direct assignments to state: state->x = y;                     |
  | - Increment/decrement: state->count++;                           |
  | - List modifications: list_add(&state->list, item);              |
  +------------------------------------------------------------------+

  Step 2.2: Wrap mutations in functions
  +------------------------------------------------------------------+
  |  // Before                                                       |
  |  void handle_connect(struct conn *c) {                           |
  |      g_state->connections[id] = c;  // Direct mutation           |
  |      g_state->active_count++;       // Direct mutation           |
  |  }                                                               |
  |                                                                  |
  |  // After                                                        |
  |  void handle_connect(struct conn *c) {                           |
  |      state_add_connection(g_state, c);  // Through function      |
  |  }                                                               |
  |                                                                  |
  |  int state_add_connection(struct state_manager *s,               |
  |                           struct conn *c) {                      |
  |      // Validation                                               |
  |      if (s->active_count >= s->max_connections)                  |
  |          return -ENOSPC;                                         |
  |      // Mutation                                                 |
  |      s->connections[c->id] = c;                                  |
  |      s->active_count++;                                          |
  |      // Notification                                             |
  |      notify_subscribers(s, EVENT_CONN_ADDED, c);                 |
  |      return 0;                                                   |
  |  }                                                               |
  +------------------------------------------------------------------+

  Step 2.3: Convert functions to request handlers
  +------------------------------------------------------------------+
  |  // Even more decoupled                                          |
  |  struct request {                                                |
  |      enum request_type type;                                     |
  |      union {                                                     |
  |          struct { struct conn *conn; } add_conn;                 |
  |          struct { int id; } del_conn;                            |
  |      };                                                          |
  |  };                                                              |
  |                                                                  |
  |  void handle_connect(struct conn *c) {                           |
  |      struct request req = {                                      |
  |          .type = REQ_ADD_CONN,                                   |
  |          .add_conn.conn = c,                                     |
  |      };                                                          |
  |      state_submit_request(g_state, &req);                        |
  |  }                                                               |
  +------------------------------------------------------------------+


                    PHASE 3: SEPARATE EXECUTION
                    ===========================

  Goal: Decision code no longer performs I/O

  Step 3.1: Identify I/O in decision code
  +------------------------------------------------------------------+
  | Look for in your "engine" code:                                  |
  | - write(), send(), recv()                                        |
  | - File operations                                                |
  | - Network calls                                                  |
  | - System calls with side effects                                 |
  +------------------------------------------------------------------+

  Step 3.2: Create execution intents
  +------------------------------------------------------------------+
  |  // Before: decision + execution mixed                           |
  |  void on_job_ready(struct job *job) {                            |
  |      // Decision                                                 |
  |      struct worker *w = select_worker(job);                      |
  |      // Execution (I/O!)                                         |
  |      send(w->socket, job->data, job->len, 0);                   |
  |  }                                                               |
  |                                                                  |
  |  // After: decision produces intent                              |
  |  void on_job_ready(struct job *job) {                            |
  |      // Decision only                                            |
  |      struct worker *w = select_worker(job);                      |
  |      // Create intent, don't execute                             |
  |      struct exec_intent intent = {                               |
  |          .type = INTENT_SEND_JOB,                                |
  |          .worker_id = w->id,                                     |
  |          .job_id = job->id,                                      |
  |      };                                                          |
  |      backend_submit(&intent);                                    |
  |  }                                                               |
  +------------------------------------------------------------------+
```

---

## 中文解释 (Chinese Explanation)

### 4.2 增量重构策略

**阶段 1：提取状态**

```
目标：创建单一状态权威

步骤：
1. 找到所有状态位置
   - grep 搜索 static 变量
   - 找 global 变量
   - 找单例模式

2. 分类状态
   - 配置 → 移到配置模块
   - 权威状态 → 移到状态管理器
   - 缓存/派生 → 保留，标记为缓存
   - 临时变量 → 保持局部

3. 创建状态管理器结构
   - 把分散的状态集中
   - 提供访问函数
   - 添加验证逻辑
```

**阶段 2：引入请求层**

```
目标：用请求替代直接修改

步骤：
1. 找到所有状态修改
   - 直接赋值
   - 增减操作
   - 列表操作

2. 包装成函数
   - state->x = y; → set_x(state, y);
   - 在函数中添加验证
   - 在函数中添加通知

3. 转换为请求处理
   - 创建请求结构
   - 提交请求而不是直接调用
   - 异步处理请求
```

**阶段 3：分离执行**

```
目标：决策代码不执行 I/O

步骤：
1. 找决策代码中的 I/O
   - 网络操作
   - 文件操作
   - 系统调用

2. 创建执行意图
   - 不直接执行
   - 描述"想做什么"
   - 提交给执行后端

3. 后端处理意图
   - 专门的执行线程
   - 处理错误和重试
   - 报告结果
```

---

## 3. Directory Structure Template

```
+===========================================================================+
|                    RECOMMENDED DIRECTORY STRUCTURE                        |
+===========================================================================+

  project/
  +-- core/                    # Runtime infrastructure
  |   +-- event.c              # Event loop
  |   +-- event.h
  |   +-- log.c                # Logging
  |   +-- log.h
  |   +-- memory.c             # Memory management
  |   +-- memory.h
  |   +-- timer.c              # Timer management
  |   +-- timer.h
  |   +-- rcu.c                # RCU implementation (optional)
  |   +-- rcu.h
  |
  +-- state/                   # State authority
  |   +-- state_manager.c      # Central state manager
  |   +-- state_manager.h
  |   +-- request.c            # Request handling
  |   +-- request.h
  |   +-- notify.c             # Change notifications
  |   +-- notify.h
  |   +-- types/               # State object definitions
  |       +-- resource.c
  |       +-- resource.h
  |
  +-- engines/                 # Decision engines
  |   +-- engine.c             # Engine framework
  |   +-- engine.h
  |   +-- policy_a/            # Example engine A
  |   |   +-- policy_a.c
  |   |   +-- policy_a.h
  |   +-- policy_b/            # Example engine B
  |       +-- policy_b.c
  |       +-- policy_b.h
  |
  +-- backend/                 # Execution backend
  |   +-- backend.c            # Backend framework
  |   +-- backend.h
  |   +-- executor.c           # Command execution
  |   +-- executor.h
  |   +-- drivers/             # Backend-specific drivers
  |       +-- netlink.c        # Example: Linux netlink
  |       +-- socket.c         # Example: BSD socket
  |
  +-- mgmt/                    # Management plane
  |   +-- config.c             # Configuration handling
  |   +-- config.h
  |   +-- cli.c                # Command line interface
  |   +-- cli.h
  |   +-- api.c                # External API (REST, gRPC)
  |   +-- api.h
  |
  +-- include/                 # Public headers
  |   +-- project.h            # Main header
  |   +-- types.h              # Common types
  |
  +-- main.c                   # Entry point
  +-- Makefile


  RESPONSIBILITY BOUNDARIES:
  ==========================

  +------------------------------------------------------------------+
  |  Directory   | Allowed to...          | NOT allowed to...        |
  +------------------------------------------------------------------+
  |  core/       | Define infrastructure  | Know about state types   |
  |              | Provide utilities      | Make business decisions  |
  +------------------------------------------------------------------+
  |  state/      | Own authoritative data | Perform I/O              |
  |              | Validate requests      | Run business logic       |
  |              | Notify changes         |                          |
  +------------------------------------------------------------------+
  |  engines/    | Make decisions         | Modify state directly    |
  |              | Submit requests        | Perform I/O              |
  |              | React to notifications |                          |
  +------------------------------------------------------------------+
  |  backend/    | Perform I/O            | Make decisions           |
  |              | Execute commands       | Modify state directly    |
  |              | Report results         |                          |
  +------------------------------------------------------------------+
  |  mgmt/       | Parse user input       | Make business decisions  |
  |              | Format output          | Modify state directly    |
  |              | Translate to requests  |                          |
  +------------------------------------------------------------------+
```

---

## 中文解释 (Chinese Explanation)

### 4.3 目录结构模板

**各目录职责：**

```
core/（核心运行时）
允许：
- 定义基础设施（事件循环、日志、内存）
- 提供工具函数

禁止：
- 知道具体业务类型
- 做业务决策

---

state/（状态权威）
允许：
- 持有权威数据
- 验证请求
- 发送变更通知

禁止：
- 执行 I/O
- 运行业务逻辑

---

engines/（决策引擎）
允许：
- 做决策
- 提交请求
- 响应通知

禁止：
- 直接修改状态
- 执行 I/O

---

backend/（执行后端）
允许：
- 执行 I/O
- 执行命令
- 报告结果

禁止：
- 做决策
- 直接修改状态

---

mgmt/（管理平面）
允许：
- 解析用户输入
- 格式化输出
- 翻译成请求

禁止：
- 做业务决策
- 直接修改状态
```

---

## 4. Migration Checklist

Before migrating, answer these questions:

| Question | Your Answer |
|----------|-------------|
| What is your "Zebra"? | |
| What are your "protocol daemons"? | |
| What is your execution backend? | |
| What is your management interface? | |
| Which state is scattered? | |
| Where does decision mix with execution? | |

---

## Next: Part 5 - APPLY (Applying in Real Projects)
