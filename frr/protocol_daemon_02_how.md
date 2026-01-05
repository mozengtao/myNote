# Protocol Daemon Architecture - Part 2: HOW

## Protocol Engine Design Philosophy

```
+============================================================================+
|                    PROTOCOL ENGINE ARCHITECTURE                            |
+============================================================================+

High-Level Protocol Daemon Structure:
+------------------------------------------------------------------+
|                                                                  |
|                      Protocol Daemon (e.g., ospfd)               |
|  +------------------------------------------------------------+  |
|  |                                                            |  |
|  |  +------------------+    +------------------+              |  |
|  |  | Event Loop       |    | Configuration    |              |  |
|  |  | (lib/event.c)    |    | (vty/CLI)        |              |  |
|  |  +--------+---------+    +--------+---------+              |  |
|  |           |                       |                        |  |
|  |           v                       v                        |  |
|  |  +------------------------------------------------+       |  |
|  |  |           Protocol State Machine (FSM)         |       |  |
|  |  |  +----------+  +----------+  +----------+      |       |  |
|  |  |  | Neighbor |  | Interface|  | Area     |      |       |  |
|  |  |  | FSM      |  | FSM      |  | State    |      |       |  |
|  |  |  +----------+  +----------+  +----------+      |       |  |
|  |  +------------------------------------------------+       |  |
|  |           |                                                |  |
|  |           v                                                |  |
|  |  +------------------+    +------------------+              |  |
|  |  | Route Objects    |    | Protocol DB      |              |  |
|  |  | (learned routes) |    | (LSDB/RIB-IN)    |              |  |
|  |  +--------+---------+    +------------------+              |  |
|  |           |                                                |  |
|  |           v                                                |  |
|  |  +------------------+                                      |  |
|  |  | Zebra Client     |                                      |  |
|  |  | (zclient)        |                                      |  |
|  |  +--------+---------+                                      |  |
|  |           |                                                |  |
|  +-----------|------------------------------------------------+  |
|              |                                                   |
|              v                                                   |
|       +-------------+                                            |
|       |   Zebra     |                                            |
|       +-------------+                                            |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**

协议引擎架构。协议守护进程（如 ospfd）的高层结构包括：事件循环（lib/event.c）处理所有异步事件；配置模块（vty/CLI）处理用户配置；协议状态机（FSM）管理邻居 FSM、接口 FSM、区域状态等；路由对象存储学到的路由；协议数据库（如 OSPF 的 LSDB、BGP 的 RIB-IN）；Zebra 客户端（zclient）与 Zebra 通信将路由安装到内核。

---

## Explicit Finite State Machines

```
+===========================================================================+
|                    OSPF NEIGHBOR FSM EXAMPLE                              |
+===========================================================================+

OSPF Neighbor State Machine (from RFC 2328):
+------------------------------------------------------------------+
|                                                                  |
|                         +-------+                                |
|                         | Down  |<------ Initial State           |
|                         +---+---+                                |
|                             |                                    |
|                             | HelloReceived                      |
|                             v                                    |
|                         +-------+                                |
|                  +----->| Init  |                                |
|                  |      +---+---+                                |
|                  |          |                                    |
|                  |          | 2-WayReceived                      |
|                  |          v                                    |
|    KillNbr   +---+---+  +-------+                                |
|    SeqMismatch  | 2-Way |<-+       |                                |
|    BadLSReq |      +---+---+  |       |                                |
|                  |    |       | AdjOK?                           |
|                  |    |       v                                  |
|                  |    |   +-------+                              |
|                  |    +-->|ExStart|                              |
|                  |        +---+---+                              |
|                  |            |                                  |
|                  |            | NegotiationDone                  |
|                  |            v                                  |
|                  |        +-------+                              |
|                  |        |Exchange|                             |
|                  |        +---+---+                              |
|                  |            |                                  |
|                  |            | ExchangeDone                     |
|                  |            v                                  |
|                  |        +-------+                              |
|                  |        |Loading|                              |
|                  |        +---+---+                              |
|                  |            |                                  |
|                  |            | LoadingDone                      |
|                  |            v                                  |
|                  |        +-------+                              |
|                  +--------| Full  |<------ Adjacency Formed      |
|                           +-------+                              |
|                                                                  |
+------------------------------------------------------------------+

Code Implementation Pattern (ospf_nsm.c):
+------------------------------------------------------------------+
|                                                                  |
|  /* State transition table */                                    |
|  struct {                                                        |
|      int (*func)(struct ospf_neighbor *);                        |
|      int next_state;                                             |
|  } NSM[OSPF_NSM_STATE_MAX][OSPF_NSM_EVENT_MAX] = {               |
|      /* Down state */                                            |
|      {                                                           |
|          { nsm_ignore, NSM_DependUpon },     /* NoEvent    */    |
|          { nsm_hello_received, NSM_Init },   /* HelloRecv  */    |
|          { nsm_ignore, NSM_Down },           /* Start      */    |
|          ...                                                     |
|      },                                                          |
|      /* Init state */                                            |
|      {                                                           |
|          { nsm_ignore, NSM_DependUpon },     /* NoEvent    */    |
|          { nsm_ignore, NSM_Init },           /* HelloRecv  */    |
|          { nsm_twoway_received, NSM_DependUpon }, /* 2Way   */   |
|          ...                                                     |
|      },                                                          |
|      ...                                                         |
|  };                                                              |
|                                                                  |
|  /* Event execution macro */                                     |
|  #define OSPF_NSM_EVENT_EXECUTE(N, E)                            |
|      ospf_nsm_event((N), (E), __FILE__, __LINE__)                |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**

显式有限状态机 - OSPF 邻居 FSM 示例。

OSPF 邻居状态机（来自 RFC 2328）：从 Down 状态开始，收到 Hello 后进入 Init，收到双向确认后进入 2-Way，如果需要建立邻接（AdjOK?）则进入 ExStart，经过协商（NegotiationDone）进入 Exchange 交换数据库描述，交换完成（ExchangeDone）后进入 Loading 加载 LSA，加载完成（LoadingDone）后进入 Full 状态表示邻接完全建立。任何时候发生 KillNbr、SeqMismatch、BadLSReq 等错误事件都会回退到较低状态。

代码实现模式：使用二维数组 NSM[状态][事件] 定义状态转换表，每个元素包含处理函数指针和下一个状态。使用宏 OSPF_NSM_EVENT_EXECUTE 触发事件，便于调试和跟踪。

---

## Event-Driven Message Processing

```
+===========================================================================+
|                    EVENT-DRIVEN ARCHITECTURE                              |
+===========================================================================+

Main Event Loop Flow:
+------------------------------------------------------------------+
|                                                                  |
|  ospfd Main Loop (simplified):                                   |
|                                                                  |
|  while (running) {                                               |
|      +--------------------------------------------------+        |
|      |  Wait for events (poll/select)                   |        |
|      |  - Socket readable (packet arrived)              |        |
|      |  - Timer expired                                 |        |
|      |  - Signal received                               |        |
|      +--------------------------------------------------+        |
|                          |                                       |
|                          v                                       |
|      +--------------------------------------------------+        |
|      |  Dispatch to appropriate handler                 |        |
|      +--------------------------------------------------+        |
|          |           |           |           |                   |
|          v           v           v           v                   |
|      +-------+   +-------+   +-------+   +-------+               |
|      |Packet |   | Timer |   | Zebra |   | VTY   |               |
|      |Handler|   |Handler|   |Message|   |Command|               |
|      +-------+   +-------+   +-------+   +-------+               |
|  }                                                               |
|                                                                  |
+------------------------------------------------------------------+

Event Types in OSPF:
+------------------------------------------------------------------+
|                                                                  |
|  1. Socket Read Events (Packet Reception)                        |
|     +----------------------------------------------------------+ |
|     | ospf_read() - Main packet reception handler              | |
|     |   -> Decode packet type                                  | |
|     |   -> Dispatch to: ospf_hello(), ospf_db_desc(),          | |
|     |                   ospf_ls_req(), ospf_ls_upd(),          | |
|     |                   ospf_ls_ack()                          | |
|     +----------------------------------------------------------+ |
|                                                                  |
|  2. Timer Events                                                 |
|     +----------------------------------------------------------+ |
|     | ospf_hello_timer()     - Send periodic Hello             | |
|     | ospf_wait_timer()      - Interface wait timer            | |
|     | ospf_inactivity_timer()- Neighbor dead timer             | |
|     | ospf_spf_timer()       - SPF calculation delay           | |
|     | ospf_lsa_refresh_timer()- LSA refresh                    | |
|     | ospf_maxage_walker()   - MaxAge LSA cleanup              | |
|     +----------------------------------------------------------+ |
|                                                                  |
|  3. Zebra Events                                                 |
|     +----------------------------------------------------------+ |
|     | Interface add/delete/change                              | |
|     | Address add/delete                                       | |
|     | Route redistribution                                     | |
|     | Router-ID update                                         | |
|     +----------------------------------------------------------+ |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**

事件驱动消息处理。

主事件循环流程：ospfd 主循环不断等待事件（使用 poll/select），事件类型包括 Socket 可读（数据包到达）、定时器到期、信号接收。事件发生后分派到相应处理器：数据包处理器、定时器处理器、Zebra 消息处理、VTY 命令处理。

OSPF 中的事件类型：(1) Socket 读事件（数据包接收）- ospf_read() 是主数据包接收处理器，解码数据包类型后分派到 ospf_hello()、ospf_db_desc()、ospf_ls_req()、ospf_ls_upd()、ospf_ls_ack()；(2) 定时器事件 - ospf_hello_timer() 发送周期性 Hello、ospf_wait_timer() 接口等待定时器、ospf_inactivity_timer() 邻居死亡定时器、ospf_spf_timer() SPF 计算延迟、ospf_lsa_refresh_timer() LSA 刷新、ospf_maxage_walker() MaxAge LSA 清理；(3) Zebra 事件 - 接口增删改、地址增删、路由重分发、Router-ID 更新。

---

## Clear Separation of Concerns

```
+===========================================================================+
|                    SEPARATION OF CONCERNS                                 |
+===========================================================================+

Three-Layer Separation:
+------------------------------------------------------------------+
|                                                                  |
|  Layer 1: Protocol Logic (Protocol-Specific)                     |
|  +----------------------------------------------------------+   |
|  |                                                          |   |
|  |  +----------------+  +----------------+  +-------------+ |   |
|  |  | FSM Management |  | Packet Encode/ |  | SPF/Route   | |   |
|  |  | (ospf_nsm.c)   |  | Decode         |  | Calculation | |   |
|  |  | (ospf_ism.c)   |  | (ospf_packet.c)|  | (ospf_spf.c)| |   |
|  |  +----------------+  +----------------+  +-------------+ |   |
|  |                                                          |   |
|  |  +----------------+  +----------------+                   |   |
|  |  | LSA Management |  | Flooding Logic |                   |   |
|  |  | (ospf_lsa.c)   |  | (ospf_flood.c) |                   |   |
|  |  +----------------+  +----------------+                   |   |
|  |                                                          |   |
|  +----------------------------------------------------------+   |
|                              |                                   |
|                              v                                   |
|  Layer 2: Route Objects (Protocol-Specific Storage)              |
|  +----------------------------------------------------------+   |
|  |                                                          |   |
|  |  +------------------+  +------------------+               |   |
|  |  | LSDB             |  | OSPF Routes      |               |   |
|  |  | (ospf_lsdb.c)    |  | (ospf_route.c)   |               |   |
|  |  +------------------+  +------------------+               |   |
|  |                                                          |   |
|  |  +------------------+  +------------------+               |   |
|  |  | Neighbor Table   |  | Interface List   |               |   |
|  |  | (oi->nbrs)       |  | (ospf->oiflist)  |               |   |
|  |  +------------------+  +------------------+               |   |
|  |                                                          |   |
|  +----------------------------------------------------------+   |
|                              |                                   |
|                              v                                   |
|  Layer 3: Zebra Interaction (Generic Interface)                  |
|  +----------------------------------------------------------+   |
|  |                                                          |   |
|  |  +------------------+  +------------------+               |   |
|  |  | Route Install    |  | Route Withdraw   |               |   |
|  |  | ospf_zebra_add() |  | ospf_zebra_del() |               |   |
|  |  +------------------+  +------------------+               |   |
|  |                                                          |   |
|  |  +------------------+  +------------------+               |   |
|  |  | Interface Sync   |  | Redistribution   |               |   |
|  |  | (interface CB)   |  | (redist CB)      |               |   |
|  |  +------------------+  +------------------+               |   |
|  |                                                          |   |
|  +----------------------------------------------------------+   |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**

关注点分离 - 三层分离。

第 1 层：协议逻辑（协议特定）- 包括 FSM 管理（ospf_nsm.c、ospf_ism.c）、数据包编解码（ospf_packet.c）、SPF/路由计算（ospf_spf.c）、LSA 管理（ospf_lsa.c）、泛洪逻辑（ospf_flood.c）。

第 2 层：路由对象（协议特定存储）- 包括 LSDB（ospf_lsdb.c）、OSPF 路由（ospf_route.c）、邻居表（oi->nbrs）、接口列表（ospf->oiflist）。

第 3 层：Zebra 交互（通用接口）- 包括路由安装（ospf_zebra_add()）、路由撤销（ospf_zebra_del()）、接口同步（interface CB）、重分发（redist CB）。

---

## Protocol Instance Management

```
+===========================================================================+
|                    OSPF INSTANCE MANAGEMENT                               |
+===========================================================================+

From ospfd.c - Instance Creation Flow:
+------------------------------------------------------------------+
|                                                                  |
|  ospf_new_alloc(instance, name)                                  |
|      |                                                           |
|      +---> Allocate struct ospf                                  |
|      |     - router_id = 0                                       |
|      |     - vrf_id lookup                                       |
|      |                                                           |
|      +---> Initialize lists                                      |
|      |     - oiflist (interfaces)                                |
|      |     - vlinks (virtual links)                              |
|      |     - areas                                               |
|      |                                                           |
|      +---> Initialize route tables                               |
|      |     - networks                                            |
|      |     - nbr_nbma                                            |
|      |     - new_external_route                                  |
|      |     - old_external_route                                  |
|      |                                                           |
|      +---> Create LSDB                                           |
|      |     ospf_lsdb_new()                                       |
|      |                                                           |
|      +---> Initialize timers                                     |
|      |     - t_maxage_walker                                     |
|      |     - t_lsa_refresher                                     |
|      |                                                           |
|      +---> Register with Zebra                                   |
|      |     ospf_zebra_vrf_register()                             |
|      |                                                           |
|      +---> Initialize helpers                                    |
|            - ospf_gr_helper_instance_init()                      |
|            - ospf_asbr_external_aggregator_init()                |
|            - ospf_opaque_type11_lsa_init()                       |
|                                                                  |
+------------------------------------------------------------------+

Multi-Instance Support:
+------------------------------------------------------------------+
|                                                                  |
|  Global: ospf_master (om)                                        |
|  +--------------------------------------------+                  |
|  | om->ospf = list of all OSPF instances      |                  |
|  | om->master = event loop                    |                  |
|  +--------------------------------------------+                  |
|              |                                                   |
|              v                                                   |
|  +------------+  +------------+  +------------+                  |
|  | ospf inst1 |  | ospf inst2 |  | ospf inst3 |                  |
|  | VRF: red   |  | VRF: blue  |  | VRF: default|                 |
|  +------------+  +------------+  +------------+                  |
|                                                                  |
|  Lookup Functions:                                               |
|  - ospf_lookup_instance(instance)                                |
|  - ospf_lookup_by_vrf_id(vrf_id)                                 |
|  - ospf_lookup_by_inst_name(instance, name)                      |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**

协议实例管理。

OSPF 实例创建流程（来自 ospfd.c）：ospf_new_alloc(instance, name) 函数执行以下步骤：(1) 分配 struct ospf，初始化 router_id 为 0，查找 vrf_id；(2) 初始化列表 - oiflist（接口）、vlinks（虚拟链路）、areas（区域）；(3) 初始化路由表 - networks、nbr_nbma、new_external_route、old_external_route；(4) 创建 LSDB（ospf_lsdb_new()）；(5) 初始化定时器 - t_maxage_walker、t_lsa_refresher；(6) 向 Zebra 注册（ospf_zebra_vrf_register()）；(7) 初始化辅助功能 - 优雅重启帮助器、ASBR 外部聚合器、Opaque LSA。

多实例支持：全局 ospf_master（om）包含所有 OSPF 实例列表和事件循环。可以有多个实例，每个关联不同的 VRF（如 inst1 关联 VRF red、inst2 关联 VRF blue、inst3 关联默认 VRF）。提供多种查找函数按实例号、VRF ID 或名称查找。
