# Protocol Daemon Architecture - Part 6: REUSE

## Applying Protocol Design Elsewhere

```
+============================================================================+
|                    REUSING PROTOCOL DAEMON PATTERNS                        |
+============================================================================+

Transferable Design Patterns:
+------------------------------------------------------------------+
|                                                                  |
|  From OSPF/BGP Implementation:                                   |
|                                                                  |
|  +----------------------------------------------------------+   |
|  | 1. Event-Driven Architecture                             |   |
|  | 2. Explicit Finite State Machines                        |   |
|  | 3. Separation of Protocol/Route/Install                  |   |
|  | 4. Timer-Based State Management                          |   |
|  | 5. Callback-Based Extensions                             |   |
|  +----------------------------------------------------------+   |
|                                                                  |
|  Applicable To:                                                  |
|                                                                  |
|  +------------------+  +------------------+  +------------------+ |
|  | Custom Control   |  | Protocol        |  | Network          | |
|  | Protocols        |  | Simulators      |  | Monitoring       | |
|  +------------------+  +------------------+  +------------------+ |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**

复用协议守护进程模式。

从 OSPF/BGP 实现中可转移的设计模式：事件驱动架构、显式有限状态机、协议/路由/安装分离、基于定时器的状态管理、基于回调的扩展。

适用于：自定义控制协议、协议模拟器、网络监控系统。

---

## Writing a Custom Control Protocol

```
+===========================================================================+
|                    CUSTOM PROTOCOL DESIGN TEMPLATE                        |
+===========================================================================+

Structure Based on FRR Patterns:
+------------------------------------------------------------------+
|                                                                  |
|  my_protocol/                                                    |
|  |                                                               |
|  +-- my_proto_main.c      Entry point                            |
|  |   +-- main()                                                  |
|  |   +-- my_proto_init()                                         |
|  |   +-- Event loop setup                                        |
|  |                                                               |
|  +-- my_proto.c           Core logic                             |
|  |   +-- Instance management                                     |
|  |   +-- Configuration                                           |
|  |                                                               |
|  +-- my_proto_fsm.c       State machines                         |
|  |   +-- Session FSM                                             |
|  |   +-- Peer FSM                                                |
|  |                                                               |
|  +-- my_proto_packet.c    Message handling                       |
|  |   +-- Encode/decode                                           |
|  |   +-- Send/receive                                            |
|  |                                                               |
|  +-- my_proto_db.c        Data structures                        |
|  |   +-- Peer table                                              |
|  |   +-- Route/data storage                                      |
|  |                                                               |
|  +-- my_proto_client.c    External interface                     |
|      +-- Route installation                                      |
|      +-- Event notification                                      |
|                                                                  |
+------------------------------------------------------------------+

Core Components to Implement:
+------------------------------------------------------------------+
|                                                                  |
|  1. Event Loop (Reuse or Implement)                              |
|  +----------------------------------------------------------+   |
|  | struct event_loop {                                      |   |
|  |     struct pollfd *fds;                                  |   |
|  |     int nfds;                                            |   |
|  |     struct timer_queue timers;                           |   |
|  | };                                                       |   |
|  |                                                          |   |
|  | void event_loop_run(struct event_loop *loop) {           |   |
|  |     while (running) {                                    |   |
|  |         timeout = timer_queue_next(&loop->timers);       |   |
|  |         n = poll(loop->fds, loop->nfds, timeout);        |   |
|  |         process_io_events(loop);                         |   |
|  |         process_timer_events(&loop->timers);             |   |
|  |     }                                                    |   |
|  | }                                                        |   |
|  +----------------------------------------------------------+   |
|                                                                  |
|  2. FSM Framework                                                |
|  +----------------------------------------------------------+   |
|  | typedef int (*fsm_action)(void *ctx);                    |   |
|  |                                                          |   |
|  | struct fsm_transition {                                  |   |
|  |     int current_state;                                   |   |
|  |     int event;                                           |   |
|  |     fsm_action action;                                   |   |
|  |     int next_state;                                      |   |
|  | };                                                       |   |
|  |                                                          |   |
|  | int fsm_execute(struct fsm *fsm, void *ctx, int event) { |   |
|  |     struct fsm_transition *t = fsm_lookup(fsm, event);   |   |
|  |     if (t->action)                                       |   |
|  |         t->action(ctx);                                  |   |
|  |     return t->next_state;                                |   |
|  | }                                                        |   |
|  +----------------------------------------------------------+   |
|                                                                  |
|  3. Timer Management                                             |
|  +----------------------------------------------------------+   |
|  | struct timer {                                           |   |
|  |     struct timespec expire;                              |   |
|  |     void (*callback)(void *arg);                         |   |
|  |     void *arg;                                           |   |
|  | };                                                       |   |
|  |                                                          |   |
|  | void timer_add(struct timer_queue *q,                    |   |
|  |                int msec, void (*cb)(void *), void *arg); |   |
|  | void timer_cancel(struct timer *t);                      |   |
|  +----------------------------------------------------------+   |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**

自定义协议设计模板。

基于 FRR 模式的结构：my_protocol/ 目录包含 my_proto_main.c（入口点、初始化、事件循环设置）、my_proto.c（核心逻辑、实例管理、配置）、my_proto_fsm.c（状态机：会话 FSM、对端 FSM）、my_proto_packet.c（消息处理：编解码、收发）、my_proto_db.c（数据结构：对端表、路由/数据存储）、my_proto_client.c（外部接口：路由安装、事件通知）。

需要实现的核心组件：
1. 事件循环 - 使用 pollfd 监控 I/O、定时器队列管理定时事件
2. FSM 框架 - 定义状态转换结构、实现 fsm_execute() 查找转换并执行动作
3. 定时器管理 - 定义定时器结构、提供 timer_add() 和 timer_cancel() 接口

---

## Simulating Routing Protocols

```
+===========================================================================+
|                    PROTOCOL SIMULATION DESIGN                             |
+===========================================================================+

Simulation Architecture:
+------------------------------------------------------------------+
|                                                                  |
|  +----------------------------------------------------------+   |
|  |                    Simulation Controller                 |   |
|  +---------------------------+------------------------------+   |
|                              |                                   |
|          +-------------------+-------------------+               |
|          |                   |                   |               |
|          v                   v                   v               |
|  +---------------+   +---------------+   +---------------+       |
|  | Virtual       |   | Virtual       |   | Virtual       |       |
|  | Router 1      |   | Router 2      |   | Router N      |       |
|  +-------+-------+   +-------+-------+   +-------+-------+       |
|          |                   |                   |               |
|          | FSM Instance      | FSM Instance      | FSM Instance  |
|          |                   |                   |               |
|          +-------------------+-------------------+               |
|                              |                                   |
|                    +------------------+                          |
|                    | Simulated Network|                          |
|                    | (Message Passing)|                          |
|                    +------------------+                          |
|                                                                  |
+------------------------------------------------------------------+

Key Simulation Components:
+------------------------------------------------------------------+
|                                                                  |
|  1. Virtual Time Management                                      |
|  +----------------------------------------------------------+   |
|  | /* Replace real timers with simulation time */           |   |
|  | struct sim_timer {                                       |   |
|  |     uint64_t virtual_time;  /* When to fire */           |   |
|  |     void (*callback)(void *);                            |   |
|  |     void *arg;                                           |   |
|  | };                                                       |   |
|  |                                                          |   |
|  | void sim_advance_time(uint64_t delta) {                  |   |
|  |     current_time += delta;                               |   |
|  |     while (timer_queue_peek() <= current_time) {         |   |
|  |         struct sim_timer *t = timer_queue_pop();         |   |
|  |         t->callback(t->arg);                             |   |
|  |     }                                                    |   |
|  | }                                                        |   |
|  +----------------------------------------------------------+   |
|                                                                  |
|  2. Message Queue Instead of Sockets                             |
|  +----------------------------------------------------------+   |
|  | struct sim_message {                                     |   |
|  |     int src_router;                                      |   |
|  |     int dst_router;                                      |   |
|  |     void *packet_data;                                   |   |
|  |     size_t packet_len;                                   |   |
|  |     uint64_t delivery_time;  /* Virtual time */          |   |
|  | };                                                       |   |
|  |                                                          |   |
|  | void sim_send_packet(int src, int dst, void *pkt, ...) { |   |
|  |     struct sim_message *msg = create_message(...);       |   |
|  |     msg->delivery_time = current_time + link_delay;      |   |
|  |     message_queue_insert(msg);                           |   |
|  | }                                                        |   |
|  +----------------------------------------------------------+   |
|                                                                  |
|  3. Reuse FSM Logic Directly                                     |
|  +----------------------------------------------------------+   |
|  | /* FSM code from real implementation */                  |   |
|  | /* Replace I/O with simulation hooks */                  |   |
|  |                                                          |   |
|  | struct virtual_router {                                  |   |
|  |     int router_id;                                       |   |
|  |     struct ospf_fsm fsm;      /* Reuse FSM */            |   |
|  |     struct lsdb *lsdb;        /* Reuse LSDB */           |   |
|  |     struct sim_interface *ifs; /* Sim interfaces */      |   |
|  | };                                                       |   |
|  +----------------------------------------------------------+   |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**

协议模拟设计。

模拟架构：模拟控制器管理多个虚拟路由器（每个有独立的 FSM 实例），通过模拟网络（消息传递）进行通信。

关键模拟组件：
1. 虚拟时间管理 - 用模拟时间替换真实定时器，sim_advance_time() 推进时间并触发到期的定时器
2. 消息队列代替 Socket - 使用 sim_message 结构包含源/目的路由器、数据包数据、虚拟交付时间，sim_send_packet() 创建消息并考虑链路延迟
3. 直接复用 FSM 逻辑 - 将真实实现的 FSM 代码用于虚拟路由器，只替换 I/O 为模拟钩子

---

## Avoiding Tight Kernel Coupling

```
+===========================================================================+
|                    KERNEL DECOUPLING PATTERNS                             |
+===========================================================================+

Anti-Pattern: Direct Kernel Manipulation
+------------------------------------------------------------------+
|                                                                  |
|  WRONG:                                                          |
|  +----------------------------------------------------------+   |
|  | void my_protocol_install_route(struct route *r) {        |   |
|  |     int sock = socket(AF_NETLINK, SOCK_RAW, ...);        |   |
|  |     struct rtmsg msg = build_rtmsg(r);                   |   |
|  |     send(sock, &msg, ...);  /* Direct to kernel */       |   |
|  |     close(sock);                                         |   |
|  | }                                                        |   |
|  +----------------------------------------------------------+   |
|                                                                  |
|  Problems:                                                       |
|  - No coordination with other routing daemons                    |
|  - No route arbitration                                          |
|  - No graceful restart support                                   |
|  - Kernel state may drift from protocol state                    |
|                                                                  |
+------------------------------------------------------------------+

Correct Pattern: Abstract Route Installation
+------------------------------------------------------------------+
|                                                                  |
|  CORRECT:                                                        |
|  +----------------------------------------------------------+   |
|  | /* Abstract interface */                                 |   |
|  | struct route_installer {                                 |   |
|  |     int (*install)(struct route *r);                     |   |
|  |     int (*withdraw)(struct route *r);                    |   |
|  |     int (*lookup)(struct prefix *p, struct route *out);  |   |
|  | };                                                       |   |
|  |                                                          |   |
|  | /* Implementation options */                             |   |
|  | struct route_installer zebra_installer = {               |   |
|  |     .install = zebra_route_add,                          |   |
|  |     .withdraw = zebra_route_del,                         |   |
|  |     .lookup = zebra_route_lookup,                        |   |
|  | };                                                       |   |
|  |                                                          |   |
|  | struct route_installer mock_installer = {                |   |
|  |     .install = mock_route_add,    /* For testing */      |   |
|  |     .withdraw = mock_route_del,                          |   |
|  |     .lookup = mock_route_lookup,                         |   |
|  | };                                                       |   |
|  |                                                          |   |
|  | /* Usage */                                              |   |
|  | void my_protocol_install_route(struct route *r) {        |   |
|  |     installer->install(r);  /* Abstracted */             |   |
|  | }                                                        |   |
|  +----------------------------------------------------------+   |
|                                                                  |
+------------------------------------------------------------------+

Benefits of Abstraction:
+------------------------------------------------------------------+
|                                                                  |
|  +----------------------------------------------------------+   |
|  | 1. Testability                                           |   |
|  |    - Use mock installer in unit tests                    |   |
|  |    - No kernel interaction during testing                |   |
|  |                                                          |   |
|  | 2. Portability                                           |   |
|  |    - Different installers for different platforms        |   |
|  |    - Linux (Netlink), BSD (routing socket), etc.         |   |
|  |                                                          |   |
|  | 3. Coordination                                          |   |
|  |    - Zebra installer handles multi-protocol arbitration  |   |
|  |    - Graceful restart support                            |   |
|  |                                                          |   |
|  | 4. Simulation                                            |   |
|  |    - Use simulation installer for protocol testing       |   |
|  |    - Model network without real kernel                   |   |
|  +----------------------------------------------------------+   |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**

内核解耦模式。

反模式：直接内核操作 - 直接使用 Netlink socket 向内核安装路由。问题：无法与其他路由守护进程协调、无路由仲裁、无优雅重启支持、内核状态可能与协议状态不同步。

正确模式：抽象路由安装 - 定义 route_installer 接口（install、withdraw、lookup），提供不同实现（zebra_installer 用于生产、mock_installer 用于测试）。协议代码调用 installer->install(r) 实现解耦。

抽象的好处：
1. 可测试性 - 单元测试使用模拟安装器，无需内核交互
2. 可移植性 - 不同平台使用不同安装器（Linux 用 Netlink、BSD 用 routing socket）
3. 协调性 - Zebra 安装器处理多协议仲裁和优雅重启
4. 模拟性 - 使用模拟安装器进行协议测试，无需真实内核

---

## FSM Design Lessons

```
+===========================================================================+
|                    FSM DESIGN BEST PRACTICES                              |
+===========================================================================+

Lesson 1: Make States Explicit and Named
+------------------------------------------------------------------+
|                                                                  |
|  GOOD:                                                           |
|  enum session_state {                                            |
|      STATE_IDLE,                                                 |
|      STATE_CONNECTING,                                           |
|      STATE_NEGOTIATING,                                          |
|      STATE_ESTABLISHED,                                          |
|      STATE_CLOSING,                                              |
|  };                                                              |
|                                                                  |
|  BAD:                                                            |
|  int state = 0;  /* What does 0 mean? */                         |
|  if (state == 3) { ... }  /* Magic numbers */                    |
|                                                                  |
+------------------------------------------------------------------+

Lesson 2: Events Should Be Actions, Not Conditions
+------------------------------------------------------------------+
|                                                                  |
|  GOOD:                                                           |
|  enum session_event {                                            |
|      EVENT_START,           /* Command to start */               |
|      EVENT_CONNECTED,       /* Connection established */         |
|      EVENT_MESSAGE_RECEIVED,/* Protocol message arrived */       |
|      EVENT_TIMEOUT,         /* Timer expired */                  |
|      EVENT_ERROR,           /* Something went wrong */           |
|      EVENT_STOP,            /* Command to stop */                |
|  };                                                              |
|                                                                  |
|  BAD:                                                            |
|  if (socket_readable && state == CONNECTING) { ... }             |
|  /* Mixing conditions with event handling */                     |
|                                                                  |
+------------------------------------------------------------------+

Lesson 3: Transition Table Should Be Complete
+------------------------------------------------------------------+
|                                                                  |
|  Every (state, event) pair should have a defined action:         |
|                                                                  |
|  +---------------+--------+--------+--------+--------+--------+  |
|  |     State     | START  |CONNECTED|MSG_RCV| TIMEOUT| ERROR  |  |
|  +---------------+--------+--------+--------+--------+--------+  |
|  | IDLE          | connect| ignore | ignore | ignore | ignore |  |
|  | CONNECTING    | ignore | negot  | ignore | retry  | close  |  |
|  | NEGOTIATING   | ignore | ignore | process| timeout| close  |  |
|  | ESTABLISHED   | ignore | ignore | process| keepalv| close  |  |
|  | CLOSING       | ignore | ignore | ignore | close  | close  |  |
|  +---------------+--------+--------+--------+--------+--------+  |
|                                                                  |
|  No cell should be "undefined"                                   |
|                                                                  |
+------------------------------------------------------------------+

Lesson 4: Actions Should Be Pure Functions
+------------------------------------------------------------------+
|                                                                  |
|  GOOD:                                                           |
|  int action_send_hello(struct session *s) {                      |
|      /* Only does one thing: send hello */                       |
|      return send_packet(s, build_hello());                       |
|  }                                                               |
|                                                                  |
|  BAD:                                                            |
|  int action_handle_event(struct session *s) {                    |
|      /* Does too many things */                                  |
|      send_hello(s);                                              |
|      update_timer(s);                                            |
|      check_neighbors(s);                                         |
|      if (something) state = NEW_STATE;  /* Hidden transition! */ |
|  }                                                               |
|                                                                  |
+------------------------------------------------------------------+

Lesson 5: State Changes Should Be Logged
+------------------------------------------------------------------+
|                                                                  |
|  void fsm_change_state(struct fsm *fsm, int new_state) {         |
|      LOG_INFO("FSM %s: %s -> %s (event: %s)",                    |
|               fsm->name,                                         |
|               state_name(fsm->current_state),                    |
|               state_name(new_state),                             |
|               event_name(fsm->last_event));                      |
|                                                                  |
|      fsm->current_state = new_state;                             |
|      fsm->state_change_time = time(NULL);                        |
|      fsm->state_change_count++;                                  |
|  }                                                               |
|                                                                  |
|  Benefits:                                                       |
|  - Debugging becomes much easier                                 |
|  - Can trace full history of state changes                       |
|  - Performance metrics (time in each state)                      |
|                                                                  |
+------------------------------------------------------------------+

Lesson 6: Separate FSM Logic from I/O
+------------------------------------------------------------------+
|                                                                  |
|  +----------------------------------------------------------+   |
|  |  Protocol Layer (FSM)        |  I/O Layer                |   |
|  |------------------------------|---------------------------|   |
|  |  - State transitions         |  - Socket management      |   |
|  |  - Protocol decisions        |  - Packet serialization   |   |
|  |  - Timer management          |  - Buffer management      |   |
|  |  - Event generation          |  - Network operations     |   |
|  +------------------------------|---------------------------|   |
|                                                                  |
|  FSM says WHAT to do, I/O layer does HOW                         |
|                                                                  |
|  Example:                                                        |
|  FSM: "In state ESTABLISHED, on event TIMEOUT, action=SEND_HELLO"|
|  I/O: build_hello_packet() -> socket_send()                      |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**

FSM 设计最佳实践。

教训 1：使状态显式且命名 - 使用枚举定义命名状态，避免魔术数字。

教训 2：事件应是动作而非条件 - 事件应是明确的动作（如 EVENT_START、EVENT_CONNECTED），避免在事件处理中混合条件检查。

教训 3：转换表应完整 - 每个（状态，事件）对都应有定义的动作，表中不应有"未定义"的单元格。

教训 4：动作应是纯函数 - 每个动作函数只做一件事，避免隐藏的状态转换。

教训 5：状态变化应记录日志 - 记录旧状态、新状态、触发事件、时间戳、变化计数，便于调试和追踪历史。

教训 6：分离 FSM 逻辑和 I/O - 协议层（FSM）负责状态转换、协议决策、定时器管理、事件生成；I/O 层负责 Socket 管理、数据包序列化、缓冲区管理、网络操作。FSM 说明"做什么"，I/O 层实现"怎么做"。

---

## Summary: Key Takeaways

```
+===========================================================================+
|                    PROTOCOL DAEMON DESIGN SUMMARY                         |
+===========================================================================+

Core Design Principles:
+------------------------------------------------------------------+
|                                                                  |
|  1. Separation of Concerns                                       |
|     - Protocol logic separate from I/O                           |
|     - FSM separate from data structures                          |
|     - Route calculation separate from installation               |
|                                                                  |
|  2. Event-Driven Architecture                                    |
|     - Single-threaded event loop                                 |
|     - Non-blocking operations                                    |
|     - Timer-based state management                               |
|                                                                  |
|  3. Explicit State Machines                                      |
|     - Named states and events                                    |
|     - Complete transition tables                                 |
|     - Logged state changes                                       |
|                                                                  |
|  4. Modular Extensions                                           |
|     - Callback-based hooks                                       |
|     - Optional feature isolation                                 |
|     - Clean API boundaries                                       |
|                                                                  |
|  5. Kernel Abstraction                                           |
|     - Never manipulate kernel directly                           |
|     - Use abstract route installer interface                     |
|     - Support testing and simulation                             |
|                                                                  |
+------------------------------------------------------------------+

What to Reuse:
+------------------------------------------------------------------+
|                                                                  |
|  REUSE:                          AVOID:                          |
|  - FSM design patterns           - Protocol-specific LSA formats |
|  - Event loop concepts           - OSPF-specific algorithms      |
|  - Timer management              - FRR internal APIs             |
|  - Abstraction layers            - Memory allocation details     |
|  - Testing strategies            - CLI implementation details    |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**

协议守护进程设计总结。

核心设计原则：
1. 关注点分离 - 协议逻辑与 I/O 分离、FSM 与数据结构分离、路由计算与安装分离
2. 事件驱动架构 - 单线程事件循环、非阻塞操作、基于定时器的状态管理
3. 显式状态机 - 命名状态和事件、完整转换表、记录状态变化
4. 模块化扩展 - 基于回调的钩子、可选功能隔离、清晰的 API 边界
5. 内核抽象 - 从不直接操作内核、使用抽象路由安装器接口、支持测试和模拟

应复用的：FSM 设计模式、事件循环概念、定时器管理、抽象层、测试策略。
应避免的：协议特定 LSA 格式、OSPF 特定算法、FRR 内部 API、内存分配细节、CLI 实现细节。
