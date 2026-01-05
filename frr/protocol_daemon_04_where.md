# Protocol Daemon Architecture - Part 4: WHERE

## Code Reading Plan

```
+============================================================================+
|                    OSPF SOURCE CODE ORGANIZATION                           |
+============================================================================+

Directory Structure (ospfd/):
+------------------------------------------------------------------+
|                                                                  |
|  ospfd/                                                          |
|  |                                                               |
|  +-- Core Files (Start Here)                                     |
|  |   +-- ospf_main.c         Entry point, initialization         |
|  |   +-- ospfd.c             Instance management, core logic     |
|  |   +-- ospfd.h             Main data structures                |
|  |                                                               |
|  +-- State Machines                                              |
|  |   +-- ospf_ism.c          Interface State Machine             |
|  |   +-- ospf_ism.h                                              |
|  |   +-- ospf_nsm.c          Neighbor State Machine              |
|  |   +-- ospf_nsm.h                                              |
|  |                                                               |
|  +-- Protocol Logic                                              |
|  |   +-- ospf_packet.c       Packet encode/decode                |
|  |   +-- ospf_hello.c        Hello protocol handling             |
|  |   +-- ospf_flood.c        LSA flooding                        |
|  |   +-- ospf_spf.c          SPF calculation                     |
|  |   +-- ospf_route.c        Route calculation from SPF          |
|  |                                                               |
|  +-- Data Structures                                             |
|  |   +-- ospf_lsa.c          LSA management                      |
|  |   +-- ospf_lsdb.c         Link-State Database                 |
|  |   +-- ospf_neighbor.c     Neighbor management                 |
|  |   +-- ospf_interface.c    Interface management                |
|  |                                                               |
|  +-- Zebra Integration                                           |
|  |   +-- ospf_zebra.c        Zebra client, route install         |
|  |                                                               |
|  +-- Features                                                    |
|  |   +-- ospf_abr.c          Area Border Router logic            |
|  |   +-- ospf_asbr.c         AS Boundary Router logic            |
|  |   +-- ospf_ase.c          AS-External route handling          |
|  |   +-- ospf_vty.c          CLI commands                        |
|  |   +-- ospf_gr.c           Graceful Restart                    |
|  |   +-- ospf_bfd.c          BFD integration                     |
|  |                                                               |
|  +-- Optional Features                                           |
|      +-- ospf_opaque.c       Opaque LSA support                  |
|      +-- ospf_te.c           Traffic Engineering                 |
|      +-- ospf_sr.c           Segment Routing                     |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**

OSPF 源代码组织。ospfd/ 目录结构包括：

核心文件（从这里开始）：ospf_main.c（入口点、初始化）、ospfd.c（实例管理、核心逻辑）、ospfd.h（主数据结构）。

状态机：ospf_ism.c/h（接口状态机）、ospf_nsm.c/h（邻居状态机）。

协议逻辑：ospf_packet.c（数据包编解码）、ospf_hello.c（Hello 协议处理）、ospf_flood.c（LSA 泛洪）、ospf_spf.c（SPF 计算）、ospf_route.c（从 SPF 计算路由）。

数据结构：ospf_lsa.c（LSA 管理）、ospf_lsdb.c（链路状态数据库）、ospf_neighbor.c（邻居管理）、ospf_interface.c（接口管理）。

Zebra 集成：ospf_zebra.c（Zebra 客户端、路由安装）。

功能：ospf_abr.c（ABR 逻辑）、ospf_asbr.c（ASBR 逻辑）、ospf_ase.c（AS 外部路由处理）、ospf_vty.c（CLI 命令）、ospf_gr.c（优雅重启）、ospf_bfd.c（BFD 集成）。

可选功能：ospf_opaque.c（Opaque LSA）、ospf_te.c（流量工程）、ospf_sr.c（段路由）。

---

## Phase 1: Main Entry Point (ospf_main.c)

```
+===========================================================================+
|                    MAIN ENTRY POINT ANALYSIS                              |
+===========================================================================+

ospf_main.c - Daemon Startup Sequence:
+------------------------------------------------------------------+
|                                                                  |
|  main()                                                          |
|      |                                                           |
|      +---> frr_preinit()           FRR library pre-init          |
|      |                                                           |
|      +---> frr_opt_add()           Add OSPF-specific options     |
|      |                                                           |
|      +---> frr_getopt()            Parse command line            |
|      |                                                           |
|      +---> master = frr_init()     Initialize FRR framework      |
|      |         |                                                 |
|      |         +---> Creates event loop                          |
|      |         +---> Sets up logging                             |
|      |         +---> Initializes memory management               |
|      |                                                           |
|      +---> ospf_master_init(master) Initialize OSPF globals      |
|      |         |                                                 |
|      |         +---> om->ospf = list_new()                       |
|      |         +---> om->master = master                         |
|      |                                                           |
|      +---> ospf_vrf_init()         VRF infrastructure            |
|      |                                                           |
|      +---> access_list_init()      Access-list module            |
|      +---> prefix_list_init()      Prefix-list module            |
|      +---> route_map_init()        Route-map module              |
|      |                                                           |
|      +---> ospf_if_init()          Interface module              |
|      +---> ospf_zebra_init()       Zebra client init             |
|      |         |                                                 |
|      |         +---> zclient_new()                               |
|      |         +---> Register callbacks                          |
|      |         +---> zclient_start()                             |
|      |                                                           |
|      +---> ospf_vty_init()         CLI commands init             |
|      |                                                           |
|      +---> frr_config_fork()       Read config, daemonize        |
|      |                                                           |
|      +---> frr_run(master)         Enter main event loop         |
|                  |                                               |
|                  +---> Never returns (runs event loop)           |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**

主入口点分析。ospf_main.c 中的 main() 函数启动序列：

1. frr_preinit() - FRR 库预初始化
2. frr_opt_add() - 添加 OSPF 特定选项
3. frr_getopt() - 解析命令行
4. frr_init() - 初始化 FRR 框架（创建事件循环、设置日志、初始化内存管理）
5. ospf_master_init(master) - 初始化 OSPF 全局变量（om->ospf 列表、om->master 事件循环）
6. ospf_vrf_init() - VRF 基础设施
7. 初始化各模块：access_list_init()、prefix_list_init()、route_map_init()
8. ospf_if_init() - 接口模块
9. ospf_zebra_init() - Zebra 客户端初始化（创建 zclient、注册回调、启动 zclient）
10. ospf_vty_init() - CLI 命令初始化
11. frr_config_fork() - 读取配置、守护进程化
12. frr_run(master) - 进入主事件循环（永不返回）

---

## Phase 2: FSM Implementation Reading

```
+===========================================================================+
|                    FSM IMPLEMENTATION READING GUIDE                       |
+===========================================================================+

File: ospf_nsm.c (Neighbor State Machine)
+------------------------------------------------------------------+
|                                                                  |
|  Key Components to Study:                                        |
|                                                                  |
|  1. State/Event Definitions (ospf_nsm.h)                         |
|     +--------------------------------------------------------+  |
|     | enum ospf_nsm_state {                                  |  |
|     |     NSM_DependUpon, NSM_Deleted, NSM_Down,             |  |
|     |     NSM_Attempt, NSM_Init, NSM_TwoWay,                 |  |
|     |     NSM_ExStart, NSM_Exchange, NSM_Loading,            |  |
|     |     NSM_Full                                           |  |
|     | };                                                     |  |
|     |                                                        |  |
|     | enum ospf_nsm_event {                                  |  |
|     |     NSM_NoEvent, NSM_HelloReceived, NSM_Start,         |  |
|     |     NSM_TwoWayReceived, NSM_NegotiationDone,           |  |
|     |     NSM_ExchangeDone, NSM_BadLSReq, NSM_LoadingDone,   |  |
|     |     NSM_AdjOK, NSM_SeqNumberMismatch, NSM_OneWay,      |  |
|     |     NSM_KillNbr, NSM_InactivityTimer, NSM_LLDown       |  |
|     | };                                                     |  |
|     +--------------------------------------------------------+  |
|                                                                  |
|  2. Transition Table (ospf_nsm.c)                                |
|     +--------------------------------------------------------+  |
|     | struct {                                               |  |
|     |     int (*func)(struct ospf_neighbor *);               |  |
|     |     int next_state;                                    |  |
|     | } NSM[OSPF_NSM_STATE_MAX][OSPF_NSM_EVENT_MAX];         |  |
|     +--------------------------------------------------------+  |
|                                                                  |
|  3. Event Handler (ospf_nsm.c)                                   |
|     +--------------------------------------------------------+  |
|     | void ospf_nsm_event(struct ospf_neighbor *nbr,         |  |
|     |                     int event)                         |  |
|     | {                                                      |  |
|     |     /* Lookup + Execute + State Change */              |  |
|     | }                                                      |  |
|     +--------------------------------------------------------+  |
|                                                                  |
|  4. Action Functions                                             |
|     +--------------------------------------------------------+  |
|     | nsm_hello_received()  - Process Hello                  |  |
|     | nsm_twoway_received() - Start/skip adjacency           |  |
|     | nsm_negotiation_done()- Complete DD negotiation        |  |
|     | nsm_exchange_done()   - DD exchange complete           |  |
|     | nsm_adj_ok()          - Check if adjacency needed      |  |
|     | nsm_kill_nbr()        - Tear down neighbor             |  |
|     +--------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+

Reading Order for FSM Understanding:
+------------------------------------------------------------------+
|                                                                  |
|  Step 1: Read header file (ospf_nsm.h)                           |
|          - Understand states and events                          |
|                                                                  |
|  Step 2: Find transition table in ospf_nsm.c                     |
|          - Map states to events to actions                       |
|                                                                  |
|  Step 3: Read ospf_nsm_event() function                          |
|          - Understand dispatch mechanism                         |
|                                                                  |
|  Step 4: Trace one complete transition                           |
|          - e.g., Down -> Init -> TwoWay -> Full                  |
|                                                                  |
|  Step 5: Read action functions                                   |
|          - Understand what each transition does                  |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**

FSM 实现阅读指南。

ospf_nsm.c（邻居状态机）关键组件：

1. 状态/事件定义（ospf_nsm.h）- 枚举定义所有状态（NSM_Down 到 NSM_Full）和事件（NSM_HelloReceived、NSM_TwoWayReceived 等）

2. 转换表（ospf_nsm.c）- 二维数组 NSM[状态][事件]，每个元素包含处理函数和下一状态

3. 事件处理器（ospf_nsm.c）- ospf_nsm_event() 函数执行查找、执行、状态变更

4. 动作函数 - nsm_hello_received()（处理 Hello）、nsm_twoway_received()（开始/跳过邻接）、nsm_negotiation_done()（完成 DD 协商）、nsm_exchange_done()（DD 交换完成）、nsm_adj_ok()（检查是否需要邻接）、nsm_kill_nbr()（拆除邻居）

FSM 理解的阅读顺序：
1. 读头文件（ospf_nsm.h）理解状态和事件
2. 找到 ospf_nsm.c 中的转换表
3. 读 ospf_nsm_event() 函数理解分派机制
4. 跟踪一个完整转换（如 Down → Init → TwoWay → Full）
5. 读动作函数理解每个转换做什么

---

## Phase 3: Route Add/Withdraw Paths

```
+===========================================================================+
|                    ROUTE LIFECYCLE TRACKING                               |
+===========================================================================+

Route Learning Path (Intra-Area):
+------------------------------------------------------------------+
|                                                                  |
|  1. LSA Reception                                                |
|     ospf_ls_upd() (ospf_packet.c)                                |
|         |                                                        |
|         v                                                        |
|  2. LSA Processing                                               |
|     ospf_lsa_install() (ospf_lsa.c)                              |
|         |                                                        |
|         +---> LSDB update                                        |
|         +---> Schedule SPF                                       |
|         |                                                        |
|         v                                                        |
|  3. SPF Calculation (triggered by timer)                         |
|     ospf_spf_calculate() (ospf_spf.c)                            |
|         |                                                        |
|         +---> Build SPF tree                                     |
|         +---> Calculate costs                                    |
|         |                                                        |
|         v                                                        |
|  4. Route Calculation                                            |
|     ospf_ia_routing() (ospf_route.c)                             |
|         |                                                        |
|         +---> Convert SPF to routes                              |
|         |                                                        |
|         v                                                        |
|  5. Route Installation                                           |
|     ospf_zebra_add() (ospf_zebra.c)                              |
|         |                                                        |
|         +---> zclient_route_send()                               |
|         |                                                        |
|         v                                                        |
|     Zebra -> Kernel                                              |
|                                                                  |
+------------------------------------------------------------------+

Route Withdrawal Path:
+------------------------------------------------------------------+
|                                                                  |
|  Trigger: Neighbor Down / LSA MaxAge / Network Gone              |
|         |                                                        |
|         v                                                        |
|  1. LSA Removal/MaxAge                                           |
|     ospf_lsa_maxage() or ospf_lsa_flush()                        |
|         |                                                        |
|         v                                                        |
|  2. SPF Re-calculation                                           |
|     ospf_spf_calculate()                                         |
|         |                                                        |
|         +---> Route no longer in SPF result                      |
|         |                                                        |
|         v                                                        |
|  3. Route Comparison                                             |
|     ospf_route_cmp() (ospf_route.c)                              |
|         |                                                        |
|         +---> Detect removed routes                              |
|         |                                                        |
|         v                                                        |
|  4. Route Deletion                                               |
|     ospf_zebra_delete() (ospf_zebra.c)                           |
|         |                                                        |
|         +---> zclient_route_send(ZEBRA_ROUTE_DELETE)             |
|         |                                                        |
|         v                                                        |
|     Zebra -> Kernel (route removed)                              |
|                                                                  |
+------------------------------------------------------------------+

Key Functions to Trace:
+------------------------------------------------------------------+
|                                                                  |
|  Route Installation:                                             |
|  +----------------------------------------------------------+   |
|  | ospf_route_install()   - Install route into OSPF table   |   |
|  | ospf_zebra_add()       - Send to Zebra                   |   |
|  | zclient_route_send()   - ZAPI message                    |   |
|  +----------------------------------------------------------+   |
|                                                                  |
|  Route Deletion:                                                 |
|  +----------------------------------------------------------+   |
|  | ospf_route_delete()    - Remove from OSPF table          |   |
|  | ospf_zebra_delete()    - Tell Zebra to remove            |   |
|  +----------------------------------------------------------+   |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**

路由生命周期跟踪。

路由学习路径（区域内）：
1. LSA 接收 - ospf_ls_upd()（ospf_packet.c）
2. LSA 处理 - ospf_lsa_install()（ospf_lsa.c）更新 LSDB、调度 SPF
3. SPF 计算（由定时器触发）- ospf_spf_calculate()（ospf_spf.c）构建 SPF 树、计算成本
4. 路由计算 - ospf_ia_routing()（ospf_route.c）将 SPF 转换为路由
5. 路由安装 - ospf_zebra_add()（ospf_zebra.c）调用 zclient_route_send()，然后 Zebra 安装到内核

路由撤销路径：触发条件：邻居 Down / LSA MaxAge / 网络消失
1. LSA 移除/MaxAge - ospf_lsa_maxage() 或 ospf_lsa_flush()
2. SPF 重新计算 - ospf_spf_calculate()，路由不再在 SPF 结果中
3. 路由比较 - ospf_route_cmp() 检测被移除的路由
4. 路由删除 - ospf_zebra_delete() 调用 zclient_route_send(ZEBRA_ROUTE_DELETE)，Zebra 从内核移除路由

---

## Phase 4: Zebra Update Calls

```
+===========================================================================+
|                    ZEBRA INTEGRATION ANALYSIS                             |
+===========================================================================+

File: ospf_zebra.c - Zebra Client Implementation
+------------------------------------------------------------------+
|                                                                  |
|  Initialization (ospf_zebra_init):                               |
|  +----------------------------------------------------------+   |
|  | zclient = zclient_new(master, &zclient_options_default,  |   |
|  |                       zclient_callbacks, ...);           |   |
|  |                                                          |   |
|  | /* Register callbacks */                                 |   |
|  | zclient->router_id_update = ospf_router_id_update_zebra; |   |
|  | zclient->interface_address_add = ospf_interface_addr_add;|   |
|  | zclient->redistribute_route_add = ospf_zebra_read_route; |   |
|  | ...                                                      |   |
|  |                                                          |   |
|  | zclient_start(zclient);                                  |   |
|  +----------------------------------------------------------+   |
|                                                                  |
+------------------------------------------------------------------+

Outbound Messages (OSPF -> Zebra):
+------------------------------------------------------------------+
|                                                                  |
|  Route Operations:                                               |
|  +----------------------------------------------------------+   |
|  | ospf_zebra_add(ospf, p, or)                              |   |
|  |     -> Build zapi_route structure                        |   |
|  |     -> Set prefix, nexthops, metric                      |   |
|  |     -> zclient_route_send(ZEBRA_ROUTE_ADD, zclient, &api)|   |
|  |                                                          |   |
|  | ospf_zebra_delete(ospf, p, or)                           |   |
|  |     -> zclient_route_send(ZEBRA_ROUTE_DELETE, ...)       |   |
|  +----------------------------------------------------------+   |
|                                                                  |
|  Redistribution Requests:                                        |
|  +----------------------------------------------------------+   |
|  | ospf_zebra_redistribute(type, vrf_id)                    |   |
|  |     -> zclient_redistribute(ZEBRA_REDISTRIBUTE_ADD, ...)  |   |
|  +----------------------------------------------------------+   |
|                                                                  |
+------------------------------------------------------------------+

Inbound Messages (Zebra -> OSPF):
+------------------------------------------------------------------+
|                                                                  |
|  Interface Events:                                               |
|  +----------------------------------------------------------+   |
|  | ospf_interface_add()        - New interface              |   |
|  | ospf_interface_delete()     - Interface removed          |   |
|  | ospf_interface_state_up()   - Link up                    |   |
|  | ospf_interface_state_down() - Link down                  |   |
|  | ospf_interface_address_add()- Address added              |   |
|  | ospf_interface_address_delete() - Address removed        |   |
|  +----------------------------------------------------------+   |
|                                                                  |
|  Route Events:                                                   |
|  +----------------------------------------------------------+   |
|  | ospf_zebra_read_route()     - Redistributed route        |   |
|  |     -> Process external route for redistribution         |   |
|  |     -> May originate AS-External LSA                     |   |
|  +----------------------------------------------------------+   |
|                                                                  |
|  Router ID:                                                      |
|  +----------------------------------------------------------+   |
|  | ospf_router_id_update_zebra()                            |   |
|  |     -> Update OSPF router-id from Zebra                  |   |
|  |     -> Trigger LSA regeneration if changed               |   |
|  +----------------------------------------------------------+   |
|                                                                  |
+------------------------------------------------------------------+

Control Flow for Route Installation:
+------------------------------------------------------------------+
|                                                                  |
|  SPF Complete                                                    |
|      |                                                           |
|      v                                                           |
|  ospf_route_install()                                            |
|      |                                                           |
|      +---> Compare with old_table                                |
|      |                                                           |
|      +---> For each new/changed route:                           |
|      |         ospf_zebra_add(ospf, prefix, route)               |
|      |             |                                             |
|      |             +---> Build zapi_route                        |
|      |             |     - api.type = ZEBRA_ROUTE_OSPF           |
|      |             |     - api.prefix = prefix                   |
|      |             |     - api.nexthops = [...]                  |
|      |             |     - api.metric = cost                     |
|      |             |                                             |
|      |             +---> zclient_route_send()                    |
|      |                       |                                   |
|      |                       v                                   |
|      |                   [ZAPI Message to Zebra]                 |
|      |                                                           |
|      +---> For each removed route:                               |
|                ospf_zebra_delete(ospf, prefix, route)            |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**

Zebra 集成分析。

ospf_zebra.c - Zebra 客户端实现。初始化（ospf_zebra_init）：创建 zclient、注册回调函数（router_id_update、interface_address_add、redistribute_route_add 等）、启动 zclient。

出站消息（OSPF → Zebra）：
- 路由操作：ospf_zebra_add() 构建 zapi_route 结构、设置前缀/下一跳/度量、调用 zclient_route_send()；ospf_zebra_delete() 发送删除请求
- 重分发请求：ospf_zebra_redistribute() 请求从 Zebra 接收其他协议的路由

入站消息（Zebra → OSPF）：
- 接口事件：ospf_interface_add/delete()、ospf_interface_state_up/down()、ospf_interface_address_add/delete()
- 路由事件：ospf_zebra_read_route() 处理重分发的外部路由，可能产生 AS-External LSA
- Router ID：ospf_router_id_update_zebra() 从 Zebra 更新 OSPF router-id

路由安装控制流：SPF 完成后调用 ospf_route_install()，与 old_table 比较，对每个新增/变更路由调用 ospf_zebra_add() 构建 zapi_route 并发送 ZAPI 消息给 Zebra；对每个移除路由调用 ospf_zebra_delete()。
