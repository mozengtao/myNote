# FRR lib/ Infrastructure: HOW | Design Philosophy

## Overview Diagram

```
+-----------------------------------------------------------------------------+
|                                                                             |
|                 HOW lib/ IS DESIGNED: CORE PRINCIPLES                       |
|                                                                             |
+-----------------------------------------------------------------------------+
|                                                                             |
|  Principle 1: EVENT-DRIVEN, NOT THREAD-HEAVY                                |
|  ============================================                               |
|                                                                             |
|  Traditional Multi-threaded          FRR Event-Driven                       |
|  ========================           ================                        |
|                                                                             |
|  +-------+  +-------+  +-------+    +---------------------------+           |
|  |Thread1|  |Thread2|  |Thread3|    |     Single Event Loop     |           |
|  |  BGP  |  | OSPF  |  | Timer |    +---------------------------+           |
|  +---+---+  +---+---+  +---+---+    |                           |           |
|      |          |          |        |  while (1) {              |           |
|      |   LOCKS  |   LOCKS  |        |    event = fetch_next();  |           |
|      +----+-----+----+-----+        |    event->func(event);    |           |
|           |          |              |  }                        |           |
|      +----v----------v----+         |                           |           |
|      |   Shared State     |         +---------------------------+           |
|      |   (complex sync)   |                     |                           |
|      +--------------------+                     v                           |
|                                      No locks needed!                       |
|                                      No race conditions!                    |
|                                      Deterministic execution!               |
|                                                                             |
+-----------------------------------------------------------------------------+
|                                                                             |
|  Principle 2: EXPLICIT OWNERSHIP AND LIFETIMES                              |
|  =============================================                              |
|                                                                             |
|  +---------------------------------------------------------------------+    |
|  |                        Memory Type System                           |    |
|  +---------------------------------------------------------------------+    |
|  |                                                                     |    |
|  |   DECLARE_MGROUP(BGP)              // Define memory group           |    |
|  |   DECLARE_MTYPE(BGP, BGP_PEER)     // Define memory type            |    |
|  |                                                                     |    |
|  |   peer = XMALLOC(MTYPE_BGP_PEER, sizeof(*peer));  // Allocate       |    |
|  |           ^                                                         |    |
|  |           |                                                         |    |
|  |           +-- Type tag tracks:                                      |    |
|  |               - Number of allocations                               |    |
|  |               - Total bytes                                         |    |
|  |               - Peak usage                                          |    |
|  |                                                                     |    |
|  |   XFREE(MTYPE_BGP_PEER, peer);     // Free (sets ptr to NULL!)      |    |
|  |                                                                     |    |
|  +---------------------------------------------------------------------+    |
|  |                                                                     |    |
|  |   At Shutdown:                                                      |    |
|  |   +---------------------------------------------------------+       |    |
|  |   | MTYPE_BGP_PEER:     12 allocations leaked!              |       |    |
|  |   | MTYPE_OSPF_LSA:      0 allocations (clean)              |       |    |
|  |   +---------------------------------------------------------+       |    |
|  |                                                                     |    |
|  +---------------------------------------------------------------------+    |
|                                                                             |
+-----------------------------------------------------------------------------+
|                                                                             |
|  Principle 3: FAIL-FAST WITH STRONG ASSERTIONS                              |
|  ==============================================                             |
|                                                                             |
|  +---------------------------------------------------------------------+    |
|  |                                                                     |    |
|  |   void process_route(struct route_entry *re)                        |    |
|  |   {                                                                 |    |
|  |       assert(re != NULL);           // Crash immediately if NULL    |    |
|  |       assert(re->nexthop != NULL);  // Don't propagate bad state    |    |
|  |                                                                     |    |
|  |       // ...process...                                              |    |
|  |   }                                                                 |    |
|  |                                                                     |    |
|  |   Why Fail-Fast?                                                    |    |
|  |   +-----------------------------------------------------------+     |    |
|  |   | - Better to crash than corrupt routing tables              |     |    |
|  |   | - Core dump provides exact failure location                |     |    |
|  |   | - Easier to debug than silent data corruption              |     |    |
|  |   | - Routing protocol bugs can cause network-wide failures    |     |    |
|  |   +-----------------------------------------------------------+     |    |
|  |                                                                     |    |
|  +---------------------------------------------------------------------+    |
|                                                                             |
+-----------------------------------------------------------------------------+
|                                                                             |
|  Principle 4: DEBUGGABILITY OVER MICRO-OPTIMIZATIONS                        |
|  ====================================================                       |
|                                                                             |
|  +---------------------------------------------------------------------+    |
|  |                                                                     |    |
|  |   Event Loop with Tracing:                                          |    |
|  |   +-----------------------------------------------------------+     |    |
|  |   |                                                           |     |    |
|  |   |  struct event {                                           |     |    |
|  |   |      void (*func)(struct event *);   // callback          |     |    |
|  |   |      void *arg;                       // user data        |     |    |
|  |   |      const struct xref_eventsched *xref; // SOURCE INFO!  |     |    |
|  |   |      //                                  ^                |     |    |
|  |   |      //                                  |                |     |    |
|  |   |      // Records file:line where event was scheduled       |     |    |
|  |   |  };                                                       |     |    |
|  |   |                                                           |     |    |
|  |   +-----------------------------------------------------------+     |    |
|  |                                                                     |    |
|  |   Log Output Example:                                               |    |
|  |   +-----------------------------------------------------------+     |    |
|  |   | [bgpd] Timer expired: bgp_keepalive_timer                  |     |    |
|  |   |        scheduled at bgpd/bgp_fsm.c:1234                    |     |    |
|  |   |        ran for 45us (cpu: 32us)                            |     |    |
|  |   +-----------------------------------------------------------+     |    |
|  |                                                                     |    |
|  +---------------------------------------------------------------------+    |
|                                                                             |
+-----------------------------------------------------------------------------+
|                                                                             |
|  HOW THESE PRINCIPLES SHAPE lib/ DESIGN                                     |
|  ======================================                                     |
|                                                                             |
|  +-------------------------+      +-------------------------+               |
|  |     event.c / frrevent.h      |      |    memory.c / memory.h      |               |
|  +-------------------------+      +-------------------------+               |
|  | - Poll-based main loop  |      | - MTYPE macros          |               |
|  | - Timer heap (min-heap) |      | - Atomic counters       |               |
|  | - No threading by default     | - Group hierarchy       |               |
|  | - CPU time tracking     |      | - Leak detection        |               |
|  +-------------------------+      +-------------------------+               |
|            |                                   |                            |
|            +----------------+------------------+                            |
|                             |                                               |
|                             v                                               |
|  +---------------------------------------------------------------------+    |
|  |                       Daemon Main Loop                              |    |
|  +---------------------------------------------------------------------+    |
|  |                                                                     |    |
|  |  int main(int argc, char **argv)                                    |    |
|  |  {                                                                  |    |
|  |      frr_preinit(&daemon_info, argc, argv);                         |    |
|  |      master = frr_init();                 // Create event loop      |    |
|  |                                                                     |    |
|  |      // Register protocol-specific events                          |    |
|  |      event_add_timer(master, protocol_timer, NULL, 10, &t);         |    |
|  |      event_add_read(master, socket_handler, NULL, sockfd, &t);      |    |
|  |                                                                     |    |
|  |      frr_run(master);                     // Enter event loop       |    |
|  |      // Never returns until shutdown                                |    |
|  |  }                                                                  |    |
|  |                                                                     |    |
|  +---------------------------------------------------------------------+    |
|                                                                             |
+-----------------------------------------------------------------------------+
```

---

## 中文详细解释

### 设计原则 1: 事件驱动，而非重线程

**传统多线程方法的问题：**

在传统的多线程设计中，每个协议或功能运行在独立线程中：
- BGP 线程处理 BGP 会话
- OSPF 线程处理 OSPF 邻接
- 定时器线程管理超时

这种方法的问题：
1. **需要大量锁**：访问共享路由表需要同步
2. **死锁风险**：复杂的锁顺序容易出错
3. **竞态条件**：难以发现和调试
4. **不确定性**：相同输入可能产生不同输出

**FRR 的事件驱动方法：**

FRR 使用单线程事件循环：

```c
while (1) {
    struct event *e = event_fetch(master);  // 等待下一个事件
    event_call(e);                          // 执行事件回调
}
```

优势：
- **无需锁**：所有代码在同一线程执行
- **无竞态条件**：没有并发访问
- **确定性执行**：便于调试和推理
- **简单的状态机**：协议逻辑更清晰

### 设计原则 2: 显式所有权和生命周期

**内存类型系统的设计：**

FRR 不使用简单的 `malloc/free`，而是包装成类型化的分配器：

```c
// 定义内存组（每个子系统一个）
DEFINE_MGROUP(BGP, "BGP daemon");

// 定义内存类型（每种对象一个）
DEFINE_MTYPE(BGP, BGP_PEER, "BGP peer");
DEFINE_MTYPE(BGP, BGP_PATH_INFO, "BGP path info");

// 使用类型化分配
peer = XMALLOC(MTYPE_BGP_PEER, sizeof(*peer));

// 使用类型化释放（自动将指针置 NULL！）
XFREE(MTYPE_BGP_PEER, peer);  // peer == NULL after this
```

**这种设计的优势：**

1. **运行时统计**：
   ```
   show memory bgp
   BGP:
     BGP_PEER:       256 allocs, 45KB total
     BGP_PATH_INFO:  15420 allocs, 1.2MB total
   ```

2. **关闭时泄漏检测**：
   ```
   memstats: BGP_PEER: 3 allocations leaked!
   ```

3. **防止悬空指针**：
   `XFREE` 宏自动将指针置为 `NULL`，防止使用已释放的内存

### 设计原则 3: 快速失败与强断言

**为什么选择崩溃而不是继续运行：**

在路由软件中，静默的数据损坏比崩溃更危险：
- 损坏的路由表可能导致流量黑洞
- 错误可能传播到网络其他部分
- 调试隐藏的损坏非常困难

**断言的使用模式：**

```c
// 在函数入口验证不变量
void rib_process(struct route_node *rn)
{
    assert(rn);                          // 必须有路由节点
    assert(rn->table);                   // 必须属于某个表
    assert(rn->table->vrf);              // 必须有 VRF 上下文
    
    // 如果断言失败，程序立即崩溃
    // 核心转储包含完整的调用栈
}
```

**这种策略的好处：**

1. Bug 在第一现场被发现，而不是在多步操作后
2. 核心转储提供精确的故障位置
3. 比追踪神秘的数据损坏容易得多

### 设计原则 4: 可调试性优于微优化

**事件系统的设计体现这一原则：**

每个事件都携带调度位置信息：

```c
struct event {
    void (*func)(struct event *);         // 回调函数
    void *arg;                            // 用户数据
    const struct xref_eventsched *xref;   // 调度位置信息！
};

struct xref_eventsched {
    struct xref xref;      // 包含文件名、行号、函数名
    const char *funcname;  // 回调函数名称
    const char *dest;      // 目标事件指针名
    uint32_t event_type;   // 事件类型
};
```

**这允许：**

1. **事件追踪**：
   ```
   [bgpd] Event callback bgp_connect_timer
          from bgpd/bgp_fsm.c:567
          cpu_time=45us wall_time=52us
   ```

2. **长时间运行检测**：
   如果回调执行时间过长，自动告警

3. **事件统计**：
   ```
   show event cpu
   Self    Time         Max     Invoked  Type       Event
   158ms   1.234s       45ms    12345    Timer      bgp_keepalive_timer
   ```

### 这些原则如何塑造 lib/

**事件循环 (event.c)：**
- 使用 `poll()` 而非 `select()`（支持更多文件描述符）
- 定时器使用最小堆（O(log n) 插入和删除）
- 默认不使用多线程
- 记录每个回调的 CPU 时间

**内存管理 (memory.c)：**
- MTYPE 宏在编译时生成类型信息
- 使用原子计数器（支持线程安全统计）
- 内存组形成层次结构
- 关闭时自动检测泄漏

**标准守护进程主循环：**

```c
int main(int argc, char **argv)
{
    struct event_loop *master;
    
    // 阶段1：预初始化
    frr_preinit(&daemon_info, argc, argv);
    
    // 阶段2：初始化（创建事件循环）
    master = frr_init();
    
    // 阶段3：注册协议事件
    event_add_timer(master, my_timer, NULL, 10, &timer_event);
    event_add_read(master, my_socket, NULL, fd, &read_event);
    
    // 阶段4：运行（永不返回直到关闭）
    frr_run(master);
    
    return 0;
}
```

### 总结

lib/ 的设计哲学可以总结为：

| 原则 | 实现方式 | 好处 |
|------|----------|------|
| 事件驱动 | 单线程 poll 循环 | 无锁、确定性、简单 |
| 显式所有权 | MTYPE 类型系统 | 可追踪、可审计、防泄漏 |
| 快速失败 | 大量 assert() | 早发现、易定位、防损坏 |
| 可调试性 | xref 源码追踪 | 可观察、可分析、可优化 |
