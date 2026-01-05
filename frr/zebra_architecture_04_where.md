# Zebra Architecture Guide - Part 4: WHERE

## Source Code Navigation

```
+============================================================================+
|                    ZEBRA SOURCE TREE OVERVIEW                              |
+============================================================================+

frr/zebra/
|
+-- Core Files (Must Read)
|   +-- main.c              <- Entry point, init sequence
|   +-- zebra_rib.c         <- RIB core logic (5000+ lines)
|   +-- zebra_vrf.c         <- VRF management
|   +-- zserv.c             <- ZAPI server
|   +-- zebra_dplane.c      <- Dataplane abstraction
|
+-- Kernel Interface
|   +-- rt_netlink.c        <- Linux Netlink implementation
|   +-- kernel_netlink.c    <- Netlink low-level wrapper
|   +-- rt_socket.c         <- BSD routing socket
|   +-- kernel_socket.c     <- BSD low-level wrapper
|
+-- Feature Modules
|   +-- zebra_mpls.c        <- MPLS management
|   +-- zebra_vxlan.c       <- VXLAN/EVPN support
|   +-- zebra_pbr.c         <- Policy-based routing
|   +-- zebra_nhg.c         <- Nexthop groups
|   +-- zebra_rnh.c         <- Nexthop tracking (NHT)
|
+-- Headers
|   +-- rib.h               <- RIB data structure definitions
|   +-- zserv.h             <- ZAPI protocol definitions
|   +-- zebra_dplane.h      <- Dataplane interface
|
+-- dpdk/                   <- DPDK hardware offload (optional)
    +-- zebra_dplane_dpdk.c
```

**中文说明：**

Zebra 源码树概览展示了目录结构。核心文件（必读）：main.c（入口点，初始化顺序）、zebra_rib.c（RIB 核心逻辑，5000+ 行）、zebra_vrf.c（VRF 管理）、zserv.c（ZAPI 服务器）、zebra_dplane.c（Dataplane 抽象层）。内核接口：rt_netlink.c（Linux Netlink 实现）、kernel_netlink.c（Netlink 底层封装）、rt_socket.c（BSD 路由套接字）、kernel_socket.c（BSD 底层封装）。功能模块：zebra_mpls.c（MPLS 管理）、zebra_vxlan.c（VXLAN/EVPN 支持）、zebra_pbr.c（策略路由）、zebra_nhg.c（下一跳组）、zebra_rnh.c（下一跳跟踪）。头文件：rib.h（RIB 数据结构定义）、zserv.h（ZAPI 协议定义）、zebra_dplane.h（Dataplane 接口）。

---

## 1. Reading Order and File Roles

### Phase 1: Entry and Initialization

```
+===========================================================================+
|  File: zebra/main.c                                                       |
|  Role: Zebra process entry point                                          |
+===========================================================================+

main() function execution order:

+------------------------------------------------------------------+
|  1. frr_preinit()         - FRR framework pre-initialization     |
|  2. zebra_debug_init()    - Debug facility initialization        |
|  3. zebra_ns_init()       - Namespace/VRF initialization         |
|  4. zebra_vty_init()      - CLI command registration             |
|  5. zebra_mpls_init()     - MPLS subsystem initialization        |
|  6. zebra_pbr_init()      - Policy routing initialization        |
|  7. frr_config_fork()     - Read configuration file              |
|  8. zebra_dplane_start()  - Start Dataplane thread               |
|  9. zserv_start()         - Start ZAPI server (implicit)         |
| 10. frr_run()             - Enter event loop                     |
+------------------------------------------------------------------+

Key code locations:
- L468: zebra_ns_init() - Initialize kernel routing socket
- L519: zebra_dplane_start() - Critical! Starts kernel sync
- L536: frr_run() - Never-returning event loop
```

**中文说明：**

zebra/main.c 是 Zebra 进程入口点。main() 函数执行顺序：(1) frr_preinit() - FRR 框架预初始化；(2) zebra_debug_init() - 调试设施初始化；(3) zebra_ns_init() - 命名空间/VRF 初始化；(4) zebra_vty_init() - CLI 命令注册；(5) zebra_mpls_init() - MPLS 子系统初始化；(6) zebra_pbr_init() - 策略路由初始化；(7) frr_config_fork() - 读取配置文件；(8) zebra_dplane_start() - 启动 Dataplane 线程；(9) zserv_start() - 启动 ZAPI 服务器（隐含）；(10) frr_run() - 进入事件循环。关键代码位置：L468 zebra_ns_init() 初始化内核路由套接字，L519 zebra_dplane_start() 是关键点，启动内核同步，L536 frr_run() 是永不返回的事件循环。

---

### Phase 2: RIB Core Logic

```
+===========================================================================+
|  File: zebra/zebra_rib.c                                                  |
|  Role: RIB brain - route add/delete/selection                             |
+===========================================================================+

Core Data Structure:

+------------------------------------------------------------------+
|  route_info[] array (L97-170):                                   |
|  Defines for each route type:                                    |
|    - Default distance                                            |
|    - Meta Queue mapping                                          |
|                                                                  |
|  [ZEBRA_ROUTE_BGP] = {                                           |
|      ZEBRA_ROUTE_BGP,                                            |
|      ZEBRA_EBGP_DISTANCE_DEFAULT,  // 20                         |
|      META_QUEUE_BGP                 // Priority queue            |
|  }                                                               |
+------------------------------------------------------------------+

Core Function Call Chain:

rib_add() - Protocol requests route add
    |
    v
rib_addnode() - Add to route node
    |
    v
rib_queue_add() - Enqueue to Meta Queue
    |
    v
rib_process() - Process route node [CRITICAL!]
    |
    +---> rib_process_update_fib() - Calculate FIB diff
    |         |
    |         v
    |     rib_install_kernel() - Push to Dataplane
    |
    +---> rib_process_result() - Handle Dataplane result


Important function locations:
- L2815: rib_process() - RIB processing core
- L2200: rib_install_kernel() - Kernel install entry
- L1700: nexthop_active_check() - Nexthop resolution
```

**中文说明：**

zebra/zebra_rib.c 是 RIB 的大脑，负责路由添加/删除/选择。核心数据结构 route_info[] 数组（L97-170）定义每种路由类型的默认距离和 Meta Queue 映射，例如 ZEBRA_ROUTE_BGP 的默认距离是 20，映射到 META_QUEUE_BGP。核心函数调用链：rib_add()（协议请求添加路由）→ rib_addnode()（添加到路由节点）→ rib_queue_add()（入队 Meta Queue）→ rib_process()（处理路由节点，关键函数）→ rib_process_update_fib()（计算 FIB 差异）→ rib_install_kernel()（推送到 Dataplane），以及 rib_process_result()（处理 Dataplane 结果）。重要函数位置：L2815 rib_process()（RIB 处理核心）、L2200 rib_install_kernel()（内核安装入口）、L1700 nexthop_active_check()（下一跳解析）。

---

### Phase 3: VRF Management

```
+===========================================================================+
|  File: zebra/zebra_vrf.c                                                  |
|  Role: VRF lifecycle management                                           |
+===========================================================================+

VRF Lifecycle:

+------------------------------------------------------------------+
|  1. Kernel creates VRF device (ip link add vrf1 type vrf)        |
|                        |                                          |
|                        v                                          |
|  2. Netlink notifies Zebra (RTM_NEWLINK)                          |
|                        |                                          |
|                        v                                          |
|  3. zebra_vrf_new()                                               |
|     - Create zebra_vrf struct                                     |
|     - Initialize routing tables                                   |
|     - Initialize RNH tables                                       |
|                        |                                          |
|                        v                                          |
|  4. zebra_vrf_enable()                                            |
|     - VRF associates with namespace                               |
|     - Send VRF_ADD to all clients                                 |
+------------------------------------------------------------------+

Key functions:
- L91: zebra_vrf_new() - VRF creation callback
- L46: zebra_vrf_add_update() - Notify clients of VRF change
- L80: zebra_vrf_update_all() - Send all VRFs to new client
```

**中文说明：**

zebra/zebra_vrf.c 负责 VRF 生命周期管理。VRF 生命周期：(1) 内核创建 VRF 设备（ip link add vrf1 type vrf）；(2) Netlink 通知 Zebra（RTM_NEWLINK）；(3) zebra_vrf_new() - 创建 zebra_vrf 结构、初始化路由表、初始化 RNH 表；(4) zebra_vrf_enable() - VRF 关联到命名空间、发送 VRF_ADD 给所有客户端。关键函数：L91 zebra_vrf_new()（VRF 创建回调）、L46 zebra_vrf_add_update()（通知客户端 VRF 变化）、L80 zebra_vrf_update_all()（向新客户端发送所有 VRF）。

---

### Phase 4: Netlink Kernel Interface

```
+===========================================================================+
|  File: zebra/rt_netlink.c                                                 |
|  Role: Linux-specific kernel routing table operations                     |
+===========================================================================+

Netlink Message Types:

+------------------------------------------------------------------+
|  Send to Kernel:                                                 |
|  - RTM_NEWROUTE  : Add route                                     |
|  - RTM_DELROUTE  : Delete route                                  |
|  - RTM_NEWNEIGH  : Add neighbor (ARP/ND)                         |
|  - RTM_NEWNEXTHOP: Add nexthop group (kernel >= 5.3)             |
+------------------------------------------------------------------+
|  Receive from Kernel:                                            |
|  - RTM_NEWLINK   : Interface add/modify                          |
|  - RTM_DELLINK   : Interface delete                              |
|  - RTM_NEWADDR   : Address add                                   |
|  - RTM_DELADDR   : Address delete                                |
+------------------------------------------------------------------+

Core Functions:
+------------------------------------------------------------------+
|  netlink_route_multipath()  - Build Netlink route message        |
|      |                                                           |
|      +---> _netlink_route_build_multipath() - Build multipath    |
|      |                                                           |
|      +---> nl_batch_add_msg() - Add to batch queue               |
|      |                                                           |
|      +---> kernel_netlink_send() - Send to kernel                |
+------------------------------------------------------------------+

Code structure:
- L100-150: Netlink helper macros and structs
- L2264+: netlink_route_multipath() - Route install main func
- L2800+: netlink_nexthop() - Kernel nexthop group operations
```

**中文说明：**

zebra/rt_netlink.c 负责 Linux 特定的内核路由表操作。Netlink 消息类型分为发送到内核（RTM_NEWROUTE 添加路由、RTM_DELROUTE 删除路由、RTM_NEWNEIGH 添加邻居、RTM_NEWNEXTHOP 添加下一跳组）和从内核接收（RTM_NEWLINK 接口添加/修改、RTM_DELLINK 接口删除、RTM_NEWADDR 地址添加、RTM_DELADDR 地址删除）。核心函数：netlink_route_multipath() 构造 Netlink 路由消息，调用 _netlink_route_build_multipath() 构造多路径属性，nl_batch_add_msg() 添加到批处理队列，kernel_netlink_send() 发送到内核。代码结构：L100-150 Netlink 辅助宏和结构，L2264+ netlink_route_multipath() 路由安装主函数，L2800+ netlink_nexthop() 内核下一跳组操作。

---

### Phase 5: ZAPI Server

```
+===========================================================================+
|  File: zebra/zserv.c                                                      |
|  Role: Protocol process communication server                              |
+===========================================================================+

Thread Model:

+------------------------------------------------------------------+
|                                                                  |
|   Main Thread          Client Thread 1      Client Thread N      |
|   +-----------+        +-------------+      +-------------+      |
|   | zserv_    |        | zserv_read()|      | zserv_read()|      |
|   | accept()  |------->| Read msg    |      | Read msg    |      |
|   +-----------+        +------+------+      +------+------+      |
|         |                     |                    |              |
|         |                     v                    v              |
|         |              +-------------+      +-------------+      |
|         |              | ibuf_fifo   |      | ibuf_fifo   |      |
|         |              | (input Q)   |      | (input Q)   |      |
|         |              +------+------+      +------+------+      |
|         |                     |                    |              |
|         +---------------------+--------------------+              |
|                               |                                   |
|                               v                                   |
|                    +--------------------+                         |
|                    | zserv_process_     |                         |
|                    | messages()         |                         |
|                    | (main thread proc) |                         |
|                    +--------------------+                         |
|                                                                  |
+------------------------------------------------------------------+

Key functions:
- L771: zserv_client_create() - Create new client
- L320: zserv_read() - Client thread reads
- L527: zserv_process_messages() - Main thread processes messages
- L564: zserv_send_message() - Send message to client

Message Processing Flow:
+------------------------------------------------------------------+
|  zserv_read() [client thread]                                    |
|      |                                                           |
|      v                                                           |
|  stream_fifo_push(ibuf_fifo)                                     |
|      |                                                           |
|      v                                                           |
|  zserv_event(ZSERV_PROCESS_MESSAGES)                             |
|      |                                                           |
|      v [switch to main thread]                                   |
|  zserv_process_messages()                                        |
|      |                                                           |
|      v                                                           |
|  zserv_handle_commands() [in zapi_msg.c]                         |
|      |                                                           |
|      v                                                           |
|  Specific handler (e.g., zread_route_add)                        |
+------------------------------------------------------------------+
```

**中文说明：**

zebra/zserv.c 是协议进程通信服务器。线程模型：主线程通过 zserv_accept() 监听连接，每个客户端有独立的客户端线程运行 zserv_read() 读取消息，消息放入 ibuf_fifo（输入队列），然后由主线程的 zserv_process_messages() 处理。关键函数：L771 zserv_client_create() 创建新客户端，L320 zserv_read() 客户端线程读取，L527 zserv_process_messages() 主线程处理消息，L564 zserv_send_message() 发送消息给客户端。消息处理流程：zserv_read()（客户端线程）→ stream_fifo_push(ibuf_fifo) → zserv_event(ZSERV_PROCESS_MESSAGES) → 切换到主线程 → zserv_process_messages() → zserv_handle_commands()（在 zapi_msg.c）→ 具体消息处理函数（如 zread_route_add）。

---

## 2. Key Data Structure Locations

```
+===========================================================================+
|                     Data Structure Quick Reference                        |
+===========================================================================+

+------------------+------------------+--------------------------------+
| Struct           | File             | Purpose                        |
+------------------+------------------+--------------------------------+
| route_entry      | rib.h:79         | Single route record            |
| rib_dest_t       | rib.h:171        | Route node dest (route list)   |
| nhg_hash_entry   | zebra_nhg.h:87   | Nexthop group hash entry       |
| zebra_vrf        | zebra_vrf.h:55   | VRF routing context            |
| zserv            | zserv.h:62       | ZAPI client session            |
| dplane_ctx       | zebra_dplane.h   | Dataplane operation context    |
| rnh              | rib.h:37         | Nexthop tracking registration  |
+------------------+------------------+--------------------------------+
```

**中文说明：**

数据结构速查表：route_entry（rib.h:79，单条路由记录）、rib_dest_t（rib.h:171，路由节点目的地，包含路由列表）、nhg_hash_entry（zebra_nhg.h:87，下一跳组哈希条目）、zebra_vrf（zebra_vrf.h:55，VRF 路由上下文）、zserv（zserv.h:62，ZAPI 客户端会话）、dplane_ctx（zebra_dplane.h，Dataplane 操作上下文）、rnh（rib.h:37，下一跳跟踪注册）。

---

## 3. Hot Paths (Performance Critical)

```
+===========================================================================+
|                     Performance Critical Paths                            |
+===========================================================================+

Route Add Hot Path:
+------------------------------------------------------------------+
|                                                                  |
|  ZAPI: ZEBRA_ROUTE_ADD                                           |
|      |                                                           |
|      v [zapi_msg.c]                                              |
|  zread_route_add()                                               |
|      |                                                           |
|      v [zebra_rib.c]                                             |
|  rib_add_multipath()                                             |
|      |                                                           |
|      v                                                           |
|  rib_queue_add()  <-- Enqueue point, doesn't block caller        |
|                                                                  |
+------------------------------------------------------------------+

Route Processing Hot Path:
+------------------------------------------------------------------+
|                                                                  |
|  Meta Queue worker thread:                                       |
|      |                                                           |
|      v                                                           |
|  rib_process() - Most important function!                        |
|      |                                                           |
|      +---> Best path selection                                   |
|      |                                                           |
|      +---> Nexthop resolution                                    |
|      |                                                           |
|      +---> FIB diff calculation                                  |
|      |                                                           |
|      v                                                           |
|  dplane_route_add/delete()                                       |
|      |                                                           |
|      v [zebra_dplane.c]                                          |
|  dplane_update_enqueue()  <-- Enqueue to Dataplane               |
|                                                                  |
+------------------------------------------------------------------+

Kernel Sync Hot Path:
+------------------------------------------------------------------+
|                                                                  |
|  Dataplane thread:                                               |
|      |                                                           |
|      v                                                           |
|  dplane_thread_loop()                                            |
|      |                                                           |
|      v                                                           |
|  kernel_dplane_process_func()                                    |
|      |                                                           |
|      v [rt_netlink.c]                                            |
|  kernel_route_rib()                                              |
|      |                                                           |
|      v                                                           |
|  netlink_route_multipath()  <-- Netlink message construction     |
|      |                                                           |
|      v                                                           |
|  nl_batch_send()  <-- Send to kernel                             |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**

性能关键路径分为三条。路由添加热路径：ZAPI ZEBRA_ROUTE_ADD → zread_route_add()（zapi_msg.c）→ rib_add_multipath()（zebra_rib.c）→ rib_queue_add()（入队点，不阻塞调用者）。路由处理热路径：Meta Queue 工作线程 → rib_process()（最重要的函数）→ 最佳路径选择 → 下一跳解析 → FIB 差异计算 → dplane_route_add/delete() → dplane_update_enqueue()（入队 Dataplane）。内核同步热路径：Dataplane 线程 → dplane_thread_loop() → kernel_dplane_process_func() → kernel_route_rib()（rt_netlink.c）→ netlink_route_multipath()（Netlink 消息构造）→ nl_batch_send()（发送到内核）。

---

## 4. Debug Entry Points

```
+===========================================================================+
|                     Debug Key Locations                                   |
+===========================================================================+

Debug Commands to Code Mapping:

+------------------------------------------------------------------+
| CLI Command                        | Handler Function/File        |
+------------------------------------------------------------------+
| show ip route                      | zebra_vty.c / do_show_ip_route|
| show ip route summary              | zebra_vty.c                  |
| show zebra client                  | zserv.c:1327                 |
| show zebra fib                     | zebra_vty.c                  |
| debug zebra rib                    | debug.c / IS_ZEBRA_DEBUG_RIB |
| debug zebra dplane                 | debug.c / IS_ZEBRA_DEBUG_DPLANE|
| debug zebra kernel                 | debug.c / IS_ZEBRA_DEBUG_KERNEL|
+------------------------------------------------------------------+

Log Key Points:
- zebra_rib.c: rnode_debug() macro - Route node level debug
- zserv.c: IS_ZEBRA_DEBUG_EVENT - Client events
- rt_netlink.c: IS_ZEBRA_DEBUG_KERNEL - Kernel interaction
```

**中文说明：**

调试关键位置。调试命令对应代码：show ip route（zebra_vty.c / do_show_ip_route）、show ip route summary（zebra_vty.c）、show zebra client（zserv.c:1327）、show zebra fib（zebra_vty.c）、debug zebra rib（debug.c / IS_ZEBRA_DEBUG_RIB）、debug zebra dplane（debug.c / IS_ZEBRA_DEBUG_DPLANE）、debug zebra kernel（debug.c / IS_ZEBRA_DEBUG_KERNEL）。日志关键点：zebra_rib.c 的 rnode_debug() 宏用于路由节点级调试，zserv.c 的 IS_ZEBRA_DEBUG_EVENT 用于客户端事件，rt_netlink.c 的 IS_ZEBRA_DEBUG_KERNEL 用于内核交互。

---

## 5. File Reading Order Summary

```
+===========================================================================+
|                     Recommended Reading Order                             |
+===========================================================================+

Level 1 - Overall Understanding (2-4 hours):
+------------------------------------------------------------------+
| 1. rib.h           - Understand core data structures             |
| 2. main.c          - Understand initialization flow              |
| 3. zserv.h         - Understand ZAPI protocol                    |
+------------------------------------------------------------------+

Level 2 - Core Logic (1-2 days):
+------------------------------------------------------------------+
| 4. zebra_rib.c     - RIB processing logic (focus: rib_process)  |
| 5. zebra_vrf.c     - VRF management                              |
| 6. zserv.c         - Client management                           |
+------------------------------------------------------------------+

Level 3 - Kernel Interface (1 day):
+------------------------------------------------------------------+
| 7. zebra_dplane.c  - Dataplane abstraction                       |
| 8. rt_netlink.c    - Netlink implementation (Linux)              |
+------------------------------------------------------------------+

Level 4 - Advanced Features (as needed):
+------------------------------------------------------------------+
| 9. zebra_nhg.c     - Nexthop groups                              |
| 10. zebra_rnh.c    - Nexthop tracking                            |
| 11. zebra_mpls.c   - MPLS support                                |
+------------------------------------------------------------------+
```

**中文说明：**

推荐阅读顺序。Level 1 整体理解（2-4 小时）：rib.h 理解核心数据结构、main.c 理解初始化流程、zserv.h 理解 ZAPI 协议。Level 2 核心逻辑（1-2 天）：zebra_rib.c RIB 处理逻辑（重点 rib_process）、zebra_vrf.c VRF 管理、zserv.c 客户端管理。Level 3 内核接口（1 天）：zebra_dplane.c Dataplane 抽象、rt_netlink.c Netlink 实现（Linux）。Level 4 高级功能（按需）：zebra_nhg.c 下一跳组、zebra_rnh.c 下一跳跟踪、zebra_mpls.c MPLS 支持。
