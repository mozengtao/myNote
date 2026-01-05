# Zebra Architecture Guide - Part 6: REUSE

## Real-World Usage and Lessons Learned

```
+============================================================================+
|                    REUSE STRATEGIES                                        |
+============================================================================+

    Option A: Use FRR as-is           Option B: Reuse Zebra Concepts
    +-------------------------+       +-------------------------+
    |  Your Custom Protocol   |       |  Your Routing System    |
    |  (as FRR daemon)        |       |  (standalone)           |
    +-------------------------+       +-------------------------+
              |                                   |
              v                                   v
    +-------------------------+       +-------------------------+
    |  ZAPI Client Library    |       |  Your RIB Manager       |
    |  (lib/zclient.c)        |       |  (inspired by Zebra)    |
    +-------------------------+       +-------------------------+
              |                                   |
              v                                   v
    +-------------------------+       +-------------------------+
    |  Zebra Daemon           |       |  Your Kernel Interface  |
    |  (RIB/FIB Manager)      |       |  (Netlink wrapper)      |
    +-------------------------+       +-------------------------+
              |                                   |
              v                                   v
    +-------------------------+       +-------------------------+
    |  Linux Kernel           |       |  Linux Kernel           |
    +-------------------------+       +-------------------------+
```

**中文说明：**

复用策略图展示了两种选择。选项 A：直接使用 FRR - 将自定义协议实现为 FRR 守护进程，使用 ZAPI 客户端库（lib/zclient.c）与 Zebra 通信，Zebra 负责 RIB/FIB 管理，最终与 Linux 内核交互。选项 B：复用 Zebra 概念 - 构建独立的路由系统，参考 Zebra 设计自己的 RIB 管理器和内核接口（Netlink 封装），直接与 Linux 内核交互。

---

## 1. Building Custom Routing Process that Feeds Zebra

```
+===========================================================================+
|                     Custom Protocol Integration Strategy                  |
+===========================================================================+

Scenario: You have a proprietary routing protocol,
          need to install routes to Linux kernel

Recommended Architecture:
+------------------------------------------------------------------+
|                                                                  |
|  +------------------+                                            |
|  | Your Protocol    |                                            |
|  | my_protocol_d    |                                            |
|  +--------+---------+                                            |
|           |                                                      |
|           | Using FRR's zclient library                          |
|           v                                                      |
|  +------------------+                                            |
|  | lib/zclient.c    |                                            |
|  | (ZAPI Client)    |                                            |
|  +--------+---------+                                            |
|           |                                                      |
|           | Unix Socket                                          |
|           v                                                      |
|  +------------------+                                            |
|  | Zebra Daemon     |                                            |
|  | (RIB Manager)    |                                            |
|  +--------+---------+                                            |
|           |                                                      |
|           | Netlink                                              |
|           v                                                      |
|  +------------------+                                            |
|  | Linux Kernel     |                                            |
|  +------------------+                                            |
|                                                                  |
+------------------------------------------------------------------+

Implementation Steps:

1. Link FRR Library
+------------------------------------------------------------------+
| # CMakeLists.txt or Makefile                                     |
|                                                                  |
| LDFLAGS += -lfrr                                                 |
| # Or compile lib/zclient.c, lib/stream.c etc directly            |
+------------------------------------------------------------------+

2. Initialize zclient
+------------------------------------------------------------------+
| #include "lib/zclient.h"                                         |
|                                                                  |
| struct zclient *zclient;                                         |
|                                                                  |
| void init_zebra_connection(struct event_loop *master) {          |
|     zclient = zclient_new(master, &zclient_options_default,      |
|                           NULL, 0);                              |
|                                                                  |
|     /* Use custom proto type or ZEBRA_ROUTE_STATIC */            |
|     zclient_init(zclient, ZEBRA_ROUTE_STATIC, 0, NULL);          |
|                                                                  |
|     zclient_start(zclient);                                      |
| }                                                                |
+------------------------------------------------------------------+

3. Implement Route Operations
+------------------------------------------------------------------+
| void add_my_route(struct prefix_ipv4 *dest,                      |
|                   struct in_addr *nexthop) {                     |
|     struct zapi_route api = {0};                                 |
|                                                                  |
|     api.type = ZEBRA_ROUTE_STATIC;                               |
|     api.flags = 0;                                               |
|     prefix_copy(&api.prefix, (struct prefix *)dest);             |
|                                                                  |
|     SET_FLAG(api.message, ZAPI_MESSAGE_NEXTHOP);                 |
|     api.nexthop_num = 1;                                         |
|     api.nexthops[0].type = NEXTHOP_TYPE_IPV4;                    |
|     api.nexthops[0].gate.ipv4 = *nexthop;                        |
|                                                                  |
|     zclient_route_send(ZEBRA_ROUTE_ADD, zclient, &api);          |
| }                                                                |
+------------------------------------------------------------------+
```

**中文说明：**

自定义协议进程集成策略。场景：你有一个专有路由协议，需要将其路由安装到 Linux 内核。推荐架构：你的协议进程 → 使用 FRR 的 zclient 库 → Unix Socket → Zebra 守护进程（RIB 管理器）→ Netlink → Linux 内核。实现步骤：(1) 链接 FRR 库（LDFLAGS += -lfrr 或直接编译 lib/zclient.c 等）；(2) 初始化 zclient（创建 zclient、设置协议类型、启动连接）；(3) 实现路由操作（构造 zapi_route 结构并调用 zclient_route_send()）。

---

## 2. Why Bypassing Zebra is an Anti-Pattern

```
+===========================================================================+
|                     Dangers of Bypassing Zebra                            |
+===========================================================================+

Anti-pattern: Directly use Netlink to operate kernel routing table

+------------------------------------------------------------------+
|  "I just need to install a few static routes,                    |
|   why bother with Zebra?"                                        |
|                                                                  |
|  WRONG: Direct Netlink operation:                                |
|                                                                  |
|  Your Program -----> Netlink -----> Kernel                       |
|                                                                  |
|  Problems:                                                       |
|                                                                  |
|  1. Conflicts with FRR                                           |
|     - If system runs FRR, your routes may be overwritten         |
|     - No coordination mechanism                                  |
|                                                                  |
|  2. No best path selection                                       |
|     - Your program must handle multi-protocol arbitration        |
|                                                                  |
|  3. No nexthop resolution                                        |
|     - Recursive routes need self-implementation                  |
|                                                                  |
|  4. No failure recovery                                          |
|     - Routes may persist after program crash                     |
|     - Or routes lost                                             |
|                                                                  |
|  5. No VRF awareness                                             |
|     - VRF route management is complex                            |
|                                                                  |
+------------------------------------------------------------------+

Benefits of Correct Approach:
+------------------------------------------------------------------+
|  CORRECT: Via ZAPI:                                              |
|                                                                  |
|  Your Program ---> ZAPI ---> Zebra ---> Netlink ---> Kernel     |
|                                                                  |
|  Benefits:                                                       |
|                                                                  |
|  1. Coordination with other protocols                            |
|     - Zebra handles multi-protocol route arbitration             |
|     - Unified administrative distance selection                  |
|                                                                  |
|  2. Automatic nexthop resolution                                 |
|     - Zebra recursively resolves nexthop                         |
|     - Nexthop state tracking and notification                    |
|                                                                  |
|  3. Graceful restart support                                     |
|     - Routes protected when protocol crashes                     |
|     - Route sync after recovery                                  |
|                                                                  |
|  4. VRF support                                                  |
|     - Zebra manages all VRFs                                     |
|                                                                  |
|  5. Observability                                                |
|     - Unified CLI commands                                       |
|     - Unified logging                                            |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**

为什么绕过 Zebra 是反模式。反模式：直接使用 Netlink 操作内核路由表。问题：(1) 与 FRR 冲突 - 如果系统运行 FRR，你的路由可能被覆盖，没有协调机制；(2) 没有最佳路径选择 - 你的程序必须自己处理多协议路由仲裁；(3) 没有下一跳解析 - 递归路由需要自己实现；(4) 没有故障恢复 - 程序崩溃后路由可能残留或丢失；(5) 没有 VRF 感知 - VRF 路由管理复杂。

正确做法的好处（通过 ZAPI）：(1) 与其他协议协调 - Zebra 处理多协议路由仲裁，统一的管理距离选择；(2) 自动下一跳解析 - Zebra 递归解析下一跳，下一跳状态跟踪和通知；(3) 优雅重启支持 - 协议崩溃时路由受保护，恢复后路由同步；(4) VRF 支持 - Zebra 管理所有 VRF；(5) 可观测性 - 统一的 CLI 命令和日志。

---

## 3. Reusing Zebra Ideas Without Reusing Code

```
+===========================================================================+
|                     Architecture Pattern Extraction                       |
+===========================================================================+

If you're building routing system from scratch,
learn from these Zebra designs:

Pattern 1: Single RIB Authority
+------------------------------------------------------------------+
|                                                                  |
|  Concept: All routing decisions through single component         |
|                                                                  |
|  Implementation Suggestions:                                     |
|  - Create dedicated RIB management module                        |
|  - All protocols submit routes via IPC                           |
|  - RIB module has exclusive kernel table write permission        |
|                                                                  |
|  Your Implementation:                                            |
|  +------------------+                                            |
|  | RibManager class |                                            |
|  | - add_route()    |                                            |
|  | - del_route()    |                                            |
|  | - get_best()     |                                            |
|  +------------------+                                            |
|                                                                  |
+------------------------------------------------------------------+

Pattern 2: Dataplane Abstraction
+------------------------------------------------------------------+
|                                                                  |
|  Concept: Isolate kernel interaction to separate layer           |
|                                                                  |
|  Implementation Suggestions:                                     |
|  - Define DataplaneProvider interface                            |
|  - Linux implementation uses Netlink                             |
|  - Can add DPDK/eBPF implementations                             |
|                                                                  |
|  Your Implementation:                                            |
|  +---------------------------+                                   |
|  | interface DataplaneProvider                                   |
|  | - install_route(Route r)                                      |
|  | - remove_route(Route r)                                       |
|  +---------------------------+                                   |
|         ^         ^                                              |
|         |         |                                              |
|  +------+---+ +---+------+                                       |
|  | Netlink  | | DPDK     |                                       |
|  | Provider | | Provider |                                       |
|  +----------+ +----------+                                       |
|                                                                  |
+------------------------------------------------------------------+

Pattern 3: Async Processing Queue
+------------------------------------------------------------------+
|                                                                  |
|  Concept: Use work queue for batch route updates                 |
|                                                                  |
|  Implementation Suggestions:                                     |
|  - Route updates enqueue first                                   |
|  - Background thread batch processes                             |
|  - Sort by priority                                              |
|                                                                  |
|  Benefits:                                                       |
|  - Doesn't block protocol processing                             |
|  - Can merge duplicate updates                                   |
|  - Control kernel interaction rate                               |
|                                                                  |
+------------------------------------------------------------------+

Pattern 4: Nexthop Tracking (NHT)
+------------------------------------------------------------------+
|                                                                  |
|  Concept: Proactively notify nexthop state changes               |
|                                                                  |
|  Implementation Suggestions:                                     |
|  - Allow protocols to register interested nexthops               |
|  - Monitor nexthop route changes                                 |
|  - Notify all registrants on route change                        |
|                                                                  |
|  Your Implementation:                                            |
|  +-----------------------------+                                 |
|  | class NexthopTracker        |                                 |
|  | - register(nexthop, cb)     |                                 |
|  | - unregister(nexthop)       |                                 |
|  | - on_route_change(prefix)   |                                 |
|  +-----------------------------+                                 |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**

复用 Zebra 思想而不复用代码。如果你要从零构建路由系统，可以借鉴以下 Zebra 设计：

模式 1：单一 RIB 权威 - 所有路由决策通过单一组件。实现建议：创建专用的 RIB 管理模块、所有协议通过 IPC 提交路由、RIB 模块独占内核路由表写权限。

模式 2：Dataplane 抽象 - 将内核交互隔离到独立层。实现建议：定义 DataplaneProvider 接口、Linux 实现使用 Netlink、可以添加 DPDK/eBPF 等实现。

模式 3：异步处理队列 - 使用工作队列批量处理路由更新。好处：不阻塞协议处理、可以合并重复更新、控制内核交互速率。

模式 4：下一跳跟踪（NHT）- 主动通知下一跳状态变化。实现建议：允许协议注册关心的下一跳、监控下一跳路由变化、路由变化时通知所有注册者。

---

## 4. Kernel Interaction Pitfalls

```
+===========================================================================+
|                     Common Kernel Interaction Issues                      |
+===========================================================================+

Pitfall 1: Netlink Sequence Number Handling
+------------------------------------------------------------------+
|                                                                  |
|  Problem: Netlink responses may arrive out of order              |
|                                                                  |
|  Wrong Approach:                                                 |
|  send(route1);  // seq=1                                         |
|  send(route2);  // seq=2                                         |
|  recv();        // Expect seq=1 response                         |
|                                                                  |
|  Correct Approach:                                               |
|  - Use sequence number to match request and response             |
|  - Or wait for all responses before processing                   |
|  - FRR uses nl_batch for batch processing                        |
|                                                                  |
+------------------------------------------------------------------+

Pitfall 2: ENOBUFS Error
+------------------------------------------------------------------+
|                                                                  |
|  Problem: Large route updates may fill kernel buffer             |
|                                                                  |
|  Symptom: send() returns ENOBUFS                                 |
|                                                                  |
|  Solutions:                                                      |
|  - Implement retry logic                                         |
|  - Use rate limiting                                             |
|  - Increase kernel buffer (sysctl)                               |
|                                                                  |
|  FRR Handling:                                                   |
|  - zebra_dplane.c has retry logic                                |
|  - Failed routes re-enqueue                                      |
|                                                                  |
+------------------------------------------------------------------+

Pitfall 3: VRF Table ID Mapping
+------------------------------------------------------------------+
|                                                                  |
|  Problem: VRF ID != Kernel Table ID                              |
|                                                                  |
|  Linux VRF Implementation:                                       |
|  - Each VRF device associates with a routing table               |
|  - VRF ID is FRR internal concept                                |
|  - Kernel table ID is system-level concept                       |
|                                                                  |
|  Correct Approach:                                               |
|  - Maintain VRF ID to table ID mapping                           |
|  - Use correct table ID when operating kernel                    |
|                                                                  |
+------------------------------------------------------------------+

Pitfall 4: Route Protocol Field
+------------------------------------------------------------------+
|                                                                  |
|  Problem: Kernel route's protocol field                          |
|                                                                  |
|  Common Values:                                                  |
|  - RTPROT_KERNEL (2): Kernel auto-generated                      |
|  - RTPROT_BOOT (3): Static config at boot                        |
|  - RTPROT_STATIC (4): Admin configured                           |
|  - RTPROT_ZEBRA (11): Installed by Zebra                         |
|  - RTPROT_BGP (186): Installed by BGP                            |
|  - RTPROT_OSPF (188): Installed by OSPF                          |
|                                                                  |
|  Note:                                                           |
|  - Must use same protocol when deleting route                    |
|  - Otherwise delete may fail or delete wrong route               |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**

内核交互常见问题。

陷阱 1：Netlink 序列号处理 - Netlink 响应可能乱序到达。正确做法：使用序列号匹配请求和响应，或者等待所有响应后再处理，FRR 使用 nl_batch 批处理。

陷阱 2：ENOBUFS 错误 - 大量路由更新可能导致内核缓冲区满。解决方案：实现重试逻辑、使用速率限制、增大内核缓冲区（sysctl）。FRR 处理：zebra_dplane.c 中有重试逻辑，失败路由重新入队。

陷阱 3：VRF 表 ID 映射 - VRF ID 不等于内核表 ID。Linux VRF 实现：每个 VRF 设备关联一个路由表，VRF ID 是 FRR 内部概念，内核表 ID 是系统级概念。正确做法：维护 VRF ID 到表 ID 的映射，使用正确的表 ID 操作内核。

陷阱 4：路由协议字段 - 内核路由的 protocol 字段。常见值：RTPROT_KERNEL(2) 内核自动生成、RTPROT_BOOT(3) 启动时静态配置、RTPROT_STATIC(4) 管理员配置、RTPROT_ZEBRA(11) Zebra 安装、RTPROT_BGP(186) BGP 安装、RTPROT_OSPF(188) OSPF 安装。注意：删除路由时必须使用相同的 protocol，否则可能删除失败或删错路由。

---

## 5. Debugging Zebra-Related Failures

```
+===========================================================================+
|                     Debugging Tips                                        |
+===========================================================================+

Problem 1: Route not installed to kernel
+------------------------------------------------------------------+
|                                                                  |
|  Troubleshooting Steps:                                          |
|                                                                  |
|  1. Check RIB status                                             |
|     vtysh -c "show ip route"                                     |
|     - Is route in RIB?                                           |
|     - Is it selected as best route?                              |
|                                                                  |
|  2. Check route status flags                                     |
|     vtysh -c "show ip route 10.0.0.0/24"                         |
|     - INSTALLED: Installed                                       |
|     - QUEUED: Waiting for installation                           |
|     - FAILED: Installation failed                                |
|                                                                  |
|  3. Check Dataplane status                                       |
|     vtysh -c "show zebra dplane info"                            |
|     - Check queue length                                         |
|     - Check error counts                                         |
|                                                                  |
|  4. Enable debug                                                 |
|     vtysh -c "debug zebra dplane"                                |
|     vtysh -c "debug zebra kernel"                                |
|                                                                  |
+------------------------------------------------------------------+

Problem 2: ZAPI client connection failure
+------------------------------------------------------------------+
|                                                                  |
|  Troubleshooting Steps:                                          |
|                                                                  |
|  1. Check if Zebra is running                                    |
|     ps aux | grep zebra                                          |
|                                                                  |
|  2. Check socket file                                            |
|     ls -la /var/run/frr/zserv.api                                |
|                                                                  |
|  3. Check permissions                                            |
|     - Is client process user in frr group?                       |
|                                                                  |
|  4. Check Zebra client list                                      |
|     vtysh -c "show zebra client"                                 |
|                                                                  |
+------------------------------------------------------------------+

Problem 3: Route flapping
+------------------------------------------------------------------+
|                                                                  |
|  Possible Causes:                                                |
|                                                                  |
|  1. Unstable nexthop                                             |
|     - Check interface status                                     |
|     - Check IGP route convergence                                |
|                                                                  |
|  2. Administrative distance conflict                             |
|     - Multiple protocols have same destination                   |
|     - Same AD values causing flip-flop                           |
|                                                                  |
|  3. Policy conflict                                              |
|     - route-map causing route match flip-flop                    |
|                                                                  |
|  Debug:                                                          |
|     vtysh -c "debug zebra rib"                                   |
|     - Observe route selection process                            |
|                                                                  |
+------------------------------------------------------------------+

Useful Debug Commands:
+------------------------------------------------------------------+
| Command                           | Purpose                      |
+------------------------------------------------------------------+
| show ip route                     | View IPv4 routing table      |
| show ip route summary             | Route statistics             |
| show ip route <prefix>            | Detailed route info          |
| show zebra client                 | View connected clients       |
| show zebra client summary         | Client statistics            |
| show zebra dplane info            | Dataplane status             |
| debug zebra rib                   | RIB processing debug         |
| debug zebra dplane                | Dataplane debug              |
| debug zebra kernel                | Kernel interaction debug     |
| debug zebra nht                   | Nexthop tracking debug       |
+------------------------------------------------------------------+
```

**中文说明：**

调试 Zebra 相关故障。

问题 1：路由未安装到内核 - 排查步骤：(1) 检查 RIB 状态（vtysh -c "show ip route"，路由是否在 RIB 中？是否被选为最佳路由？）；(2) 检查路由状态标志（INSTALLED 已安装、QUEUED 等待安装、FAILED 安装失败）；(3) 检查 Dataplane 状态（vtysh -c "show zebra dplane info"，查看队列长度和错误计数）；(4) 开启调试（debug zebra dplane/kernel）。

问题 2：ZAPI 客户端连接失败 - 排查步骤：(1) 检查 Zebra 是否运行（ps aux | grep zebra）；(2) 检查 socket 文件（ls -la /var/run/frr/zserv.api）；(3) 检查权限（客户端进程用户是否在 frr 组）；(4) 检查 Zebra 客户端列表（vtysh -c "show zebra client"）。

问题 3：路由抖动 - 可能原因：(1) 下一跳不稳定（检查接口状态、IGP 路由收敛）；(2) 管理距离冲突（多个协议有相同目的地，AD 值相同导致来回切换）；(3) 策略冲突（route-map 导致路由来回匹配）。调试：vtysh -c "debug zebra rib" 观察路由选择过程。

有用的调试命令表列出了常用命令及其用途。

---

## 6. Summary: Learning Takeaways

```
+===========================================================================+
|                     Key Takeaways Review                                  |
+===========================================================================+

1. Architecture Principles
   * Single RIB authority - Avoid multi-writer problem
   * Control/data plane separation - Clear responsibility boundary
   * Async processing - Don't block protocol logic
   * Pluggable kernel interface - Support multiple platforms

2. Key Components
   * Zebra = RIB manager + FIB synchronizer
   * ZAPI = Standard interface between protocol and Zebra
   * Dataplane = Kernel operation abstraction layer
   * Meta Queue = Priority work queue

3. Best Practices
   * Use ZAPI instead of direct Netlink
   * Implement nexthop tracking callback
   * Handle route installation confirmation
   * Support graceful restart

4. Avoid Pitfalls
   * Don't bypass Zebra to directly operate kernel
   * Don't ignore Netlink error handling
   * Don't confuse VRF ID and table ID
   * Don't ignore route protocol field

5. Reusable Patterns
   -> Single-point route arbitration
   -> Dataplane provider pattern
   -> Async batch processing queue
   -> Nexthop tracking mechanism
```

**中文说明：**

核心要点回顾。

(1) 架构原则：单一 RIB 权威（避免多写者问题）、控制/数据平面分离（清晰的职责边界）、异步处理（不阻塞协议逻辑）、可插拔内核接口（支持多种平台）。

(2) 关键组件：Zebra = RIB 管理器 + FIB 同步器、ZAPI = 协议进程与 Zebra 的标准接口、Dataplane = 内核操作抽象层、Meta Queue = 优先级工作队列。

(3) 最佳实践：使用 ZAPI 而非直接 Netlink、实现下一跳跟踪回调、处理路由安装确认、支持优雅重启。

(4) 避免陷阱：不要绕过 Zebra 直接操作内核、不要忽略 Netlink 错误处理、不要混淆 VRF ID 和表 ID、不要忽略路由协议字段。

(5) 可复用模式：单点路由仲裁、Dataplane 提供者模式、异步批处理队列、下一跳跟踪机制。
