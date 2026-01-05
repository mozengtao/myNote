# Protocol Daemon Architecture - Part 5: API

## Extension Points

```
+============================================================================+
|                    PROTOCOL DAEMON EXTENSION POINTS                        |
+============================================================================+

Extension Architecture Overview:
+------------------------------------------------------------------+
|                                                                  |
|  +----------------------------------------------------------+   |
|  |                    Protocol Daemon                       |   |
|  |                                                          |   |
|  |  +------------------+    +------------------+             |   |
|  |  | Core Protocol    |    | Extension Hooks  |             |   |
|  |  | (Fixed Logic)    |    | (Pluggable)      |             |   |
|  |  +------------------+    +--------+---------+             |   |
|  |                                   |                       |   |
|  |           +----------+------------+----------+            |   |
|  |           |          |            |          |            |   |
|  |           v          v            v          v            |   |
|  |     +---------+ +---------+ +---------+ +---------+       |   |
|  |     | Opaque  | | BFD     | | Segment | | Traffic |       |   |
|  |     | LSA     | | Support | | Routing | | Eng     |       |   |
|  |     +---------+ +---------+ +---------+ +---------+       |   |
|  |                                                          |   |
|  +----------------------------------------------------------+   |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**

扩展点架构概述。协议守护进程包含核心协议（固定逻辑）和扩展钩子（可插拔）。扩展钩子可以连接各种扩展功能：Opaque LSA、BFD 支持、段路由、流量工程等。

---

## New Feature Plugin Points

```
+===========================================================================+
|                    WHERE NEW FEATURES PLUG IN                             |
+===========================================================================+

1. Opaque LSA Extension Framework:
+------------------------------------------------------------------+
|                                                                  |
|  /* ospf_opaque.c - Opaque LSA registration */                   |
|                                                                  |
|  struct ospf_opaque_functab {                                    |
|      uint8_t opaque_type;                                        |
|      /* Callbacks */                                             |
|      int (*new_if_hook)(struct interface *ifp);                  |
|      int (*del_if_hook)(struct interface *ifp);                  |
|      void (*ism_change_hook)(struct ospf_interface *oi,          |
|                              int old_status);                    |
|      void (*nsm_change_hook)(struct ospf_neighbor *nbr,          |
|                              int old_status);                    |
|      void (*show_hook)(struct vty *vty, struct ospf_lsa *lsa);   |
|      int (*lsa_originate_hook)(void *arg);                       |
|      struct ospf_lsa *(*lsa_refresher)(struct ospf_lsa *lsa);    |
|  };                                                              |
|                                                                  |
|  Registration:                                                   |
|  +----------------------------------------------------------+   |
|  | ospf_register_opaque_functab(                            |   |
|  |     OSPF_OPAQUE_AREA_LSA,     /* Type 10 */              |   |
|  |     OPAQUE_TYPE_TE,           /* Sub-type */             |   |
|  |     ospf_te_new_if,                                      |   |
|  |     ospf_te_del_if,                                      |   |
|  |     ospf_te_ism_change,                                  |   |
|  |     ospf_te_nsm_change,                                  |   |
|  |     ospf_te_show_info,                                   |   |
|  |     ospf_te_lsa_originate,                               |   |
|  |     ospf_te_lsa_refresh                                  |   |
|  | );                                                       |   |
|  +----------------------------------------------------------+   |
|                                                                  |
+------------------------------------------------------------------+

2. BFD Integration Hook:
+------------------------------------------------------------------+
|                                                                  |
|  /* ospf_bfd.c - BFD session management */                       |
|                                                                  |
|  Hook Points:                                                    |
|  +----------------------------------------------------------+   |
|  | ospf_bfd_init()            Initialize BFD integration    |   |
|  | ospf_bfd_info_nbr_create() Create BFD for neighbor       |   |
|  | ospf_bfd_trigger_event()   Handle BFD state change       |   |
|  +----------------------------------------------------------+   |
|                                                                  |
|  Integration with NSM:                                           |
|  +----------------------------------------------------------+   |
|  | When neighbor reaches Full:                              |   |
|  |   ospf_bfd_info_nbr_create(nbr)                          |   |
|  |       -> bfd_sess_install()                              |   |
|  |                                                          |   |
|  | When BFD session down:                                   |   |
|  |   ospf_bfd_trigger_event(nbr, BFD_STATUS_DOWN)           |   |
|  |       -> OSPF_NSM_EVENT_EXECUTE(nbr, NSM_InactivityTimer)|   |
|  +----------------------------------------------------------+   |
|                                                                  |
+------------------------------------------------------------------+

3. Segment Routing Extension:
+------------------------------------------------------------------+
|                                                                  |
|  /* ospf_sr.c - Segment Routing support */                       |
|                                                                  |
|  Extension Points:                                               |
|  +----------------------------------------------------------+   |
|  | ospf_sr_init()            Initialize SR subsystem        |   |
|  | ospf_sr_update_task()     Process SR-related updates     |   |
|  |                                                          |   |
|  | Hooks into:                                              |   |
|  | - Router-LSA generation (add Router Capabilities TLV)    |   |
|  | - Extended Link LSA (SID/Label TLVs)                     |   |
|  | - Extended Prefix LSA (Prefix-SID TLV)                   |   |
|  | - SPF calculation (consider SID in path selection)       |   |
|  +----------------------------------------------------------+   |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**

新功能插件点。

1. Opaque LSA 扩展框架：通过 struct ospf_opaque_functab 定义扩展回调，包括 new_if_hook、del_if_hook、ism_change_hook、nsm_change_hook、show_hook、lsa_originate_hook、lsa_refresher。使用 ospf_register_opaque_functab() 注册扩展。

2. BFD 集成钩子：ospf_bfd_init() 初始化 BFD 集成、ospf_bfd_info_nbr_create() 为邻居创建 BFD 会话、ospf_bfd_trigger_event() 处理 BFD 状态变化。与 NSM 集成：邻居达到 Full 时创建 BFD 会话，BFD 会话 Down 时触发 NSM_InactivityTimer 事件。

3. 段路由扩展：ospf_sr_init() 初始化 SR 子系统、ospf_sr_update_task() 处理 SR 相关更新。钩入 Router-LSA 生成（添加 Router Capabilities TLV）、Extended Link LSA（SID/Label TLV）、Extended Prefix LSA（Prefix-SID TLV）、SPF 计算（在路径选择中考虑 SID）。

---

## Stable Data Structures

```
+===========================================================================+
|                    STABLE VS INTERNAL STRUCTURES                          |
+===========================================================================+

Stable Structures (Safe to Use/Extend):
+------------------------------------------------------------------+
|                                                                  |
|  Core Protocol Structures (Well-Defined API):                    |
|  +----------------------------------------------------------+   |
|  | struct ospf         - OSPF instance (ospfd.h)            |   |
|  |   - ospf->areas                                          |   |
|  |   - ospf->oiflist                                        |   |
|  |   - ospf->lsdb                                           |   |
|  |                                                          |   |
|  | struct ospf_area    - Area information                   |   |
|  |   - area->lsdb                                           |   |
|  |   - area->oiflist                                        |   |
|  |                                                          |   |
|  | struct ospf_interface - Interface (ospf_interface.h)     |   |
|  |   - oi->nbrs                                             |   |
|  |   - oi->state                                            |   |
|  |   - oi->params                                           |   |
|  |                                                          |   |
|  | struct ospf_neighbor - Neighbor (ospf_neighbor.h)        |   |
|  |   - nbr->state                                           |   |
|  |   - nbr->router_id                                       |   |
|  +----------------------------------------------------------+   |
|                                                                  |
|  LSA Structures (RFC-Defined):                                   |
|  +----------------------------------------------------------+   |
|  | struct lsa_header   - Standard LSA header (20 bytes)     |   |
|  | struct ospf_lsa     - LSA wrapper with metadata          |   |
|  | struct ospf_lsdb    - Link-State Database                |   |
|  +----------------------------------------------------------+   |
|                                                                  |
|  Zebra Interface (lib/zclient.h):                                |
|  +----------------------------------------------------------+   |
|  | struct zclient      - Zebra client handle                |   |
|  | struct zapi_route   - Route message structure            |   |
|  | zclient_route_send()- Route installation API             |   |
|  +----------------------------------------------------------+   |
|                                                                  |
+------------------------------------------------------------------+

Internal Structures (Do NOT Depend On):
+------------------------------------------------------------------+
|                                                                  |
|  Implementation Details (May Change):                            |
|  +----------------------------------------------------------+   |
|  | FSM transition tables (NSM[][], ISM[][])                 |   |
|  |   - Internal state machine implementation                |   |
|  |   - May be refactored                                    |   |
|  |                                                          |   |
|  | Timer implementation details                             |   |
|  |   - event_add_timer internals                            |   |
|  |   - Timer wheel structures                               |   |
|  |                                                          |   |
|  | Packet buffer management                                 |   |
|  |   - Stream structures (lib/stream.h)                     |   |
|  |   - Internal encode/decode helpers                       |   |
|  |                                                          |   |
|  | Memory pool details                                      |   |
|  |   - MTYPE_* allocation                                   |   |
|  |   - Memory tracking internals                            |   |
|  +----------------------------------------------------------+   |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**

稳定与内部结构。

稳定结构（可安全使用/扩展）：
- 核心协议结构（有良好定义的 API）：struct ospf（OSPF 实例）、struct ospf_area（区域信息）、struct ospf_interface（接口）、struct ospf_neighbor（邻居）
- LSA 结构（RFC 定义）：struct lsa_header（标准 LSA 头）、struct ospf_lsa（带元数据的 LSA 封装）、struct ospf_lsdb（链路状态数据库）
- Zebra 接口（lib/zclient.h）：struct zclient、struct zapi_route、zclient_route_send()

内部结构（不要依赖）：
- FSM 转换表（NSM[][]、ISM[][]）- 内部状态机实现，可能被重构
- 定时器实现细节 - event_add_timer 内部、Timer wheel 结构
- 数据包缓冲区管理 - Stream 结构、内部编解码辅助函数
- 内存池细节 - MTYPE_* 分配、内存跟踪内部

---

## Internal Assumptions

```
+===========================================================================+
|                    INTERNAL ASSUMPTIONS (DO NOT VIOLATE)                  |
+===========================================================================+

Critical Invariants:
+------------------------------------------------------------------+
|                                                                  |
|  1. Single-Threaded Event Loop                                   |
|  +----------------------------------------------------------+   |
|  | ASSUMPTION: All protocol logic runs in single thread     |   |
|  |                                                          |   |
|  | DO NOT:                                                  |   |
|  | - Create additional threads that modify protocol state   |   |
|  | - Use pthread mutexes for protocol data                  |   |
|  | - Make blocking calls in event handlers                  |   |
|  |                                                          |   |
|  | REASON: Event-driven design assumes no concurrent access |   |
|  +----------------------------------------------------------+   |
|                                                                  |
|  2. FSM State Integrity                                          |
|  +----------------------------------------------------------+   |
|  | ASSUMPTION: State changes only through FSM events        |   |
|  |                                                          |   |
|  | DO NOT:                                                  |   |
|  | - Directly modify nbr->state or oi->state                |   |
|  | - Skip event handlers for state transitions              |   |
|  | - Assume state without checking                          |   |
|  |                                                          |   |
|  | CORRECT: OSPF_NSM_EVENT_EXECUTE(nbr, NSM_*)              |   |
|  +----------------------------------------------------------+   |
|                                                                  |
|  3. Reference Counting for LSAs                                  |
|  +----------------------------------------------------------+   |
|  | ASSUMPTION: LSAs are reference-counted                   |   |
|  |                                                          |   |
|  | DO NOT:                                                  |   |
|  | - Free LSA directly                                      |   |
|  | - Hold LSA pointer without locking                       |   |
|  | - Ignore lock/unlock calls                               |   |
|  |                                                          |   |
|  | CORRECT:                                                 |   |
|  |   ospf_lsa_lock(lsa);   /* Increment refcount */         |   |
|  |   ospf_lsa_unlock(&lsa); /* Decrement, may free */       |   |
|  +----------------------------------------------------------+   |
|                                                                  |
|  4. Zebra as Single Route Authority                              |
|  +----------------------------------------------------------+   |
|  | ASSUMPTION: Only Zebra installs routes to kernel         |   |
|  |                                                          |   |
|  | DO NOT:                                                  |   |
|  | - Directly manipulate kernel routing table               |   |
|  | - Bypass Zebra for route installation                    |   |
|  | - Assume kernel state matches protocol state             |   |
|  |                                                          |   |
|  | CORRECT: Always use zclient_route_send()                 |   |
|  +----------------------------------------------------------+   |
|                                                                  |
|  5. Timer Ownership                                              |
|  +----------------------------------------------------------+   |
|  | ASSUMPTION: Timer pointers must be managed correctly     |   |
|  |                                                          |   |
|  | DO NOT:                                                  |   |
|  | - Leave dangling timer pointers                          |   |
|  | - Cancel timer without nullifying pointer                |   |
|  | - Add timer without saving handle                        |   |
|  |                                                          |   |
|  | CORRECT:                                                 |   |
|  |   event_add_timer(master, func, arg, time, &timer_ptr);  |   |
|  |   EVENT_OFF(timer_ptr);  /* Cancel and nullify */        |   |
|  +----------------------------------------------------------+   |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**

内部假设（不要违反）。

关键不变量：

1. 单线程事件循环 - 假设所有协议逻辑在单线程中运行。不要：创建修改协议状态的额外线程、为协议数据使用 pthread 互斥锁、在事件处理器中进行阻塞调用。原因：事件驱动设计假设没有并发访问。

2. FSM 状态完整性 - 假设状态变更只通过 FSM 事件进行。不要：直接修改 nbr->state 或 oi->state、跳过状态转换的事件处理器、不检查就假设状态。正确做法：使用 OSPF_NSM_EVENT_EXECUTE(nbr, NSM_*)。

3. LSA 引用计数 - 假设 LSA 是引用计数的。不要：直接释放 LSA、持有 LSA 指针而不加锁、忽略 lock/unlock 调用。正确做法：ospf_lsa_lock(lsa) 增加引用计数、ospf_lsa_unlock(&lsa) 减少引用计数（可能释放）。

4. Zebra 作为唯一路由权威 - 假设只有 Zebra 向内核安装路由。不要：直接操作内核路由表、绕过 Zebra 安装路由、假设内核状态与协议状态匹配。正确做法：始终使用 zclient_route_send()。

5. 定时器所有权 - 假设定时器指针必须正确管理。不要：留下悬空定时器指针、取消定时器而不置空指针、添加定时器而不保存句柄。正确做法：使用 event_add_timer() 保存句柄，使用 EVENT_OFF() 取消并置空。

---

## API Usage Patterns

```
+===========================================================================+
|                    COMMON API USAGE PATTERNS                              |
+===========================================================================+

Pattern 1: Adding New CLI Command
+------------------------------------------------------------------+
|                                                                  |
|  /* In ospf_vty.c */                                             |
|                                                                  |
|  DEFUN(show_my_feature,                                          |
|        show_my_feature_cmd,                                      |
|        "show ip ospf my-feature",                                |
|        SHOW_STR                                                  |
|        IP_STR                                                    |
|        "OSPF information\n"                                      |
|        "My feature information\n")                               |
|  {                                                               |
|      struct ospf *ospf = ospf_lookup_by_vrf_id(VRF_DEFAULT);     |
|      if (!ospf)                                                  |
|          return CMD_SUCCESS;                                     |
|                                                                  |
|      /* Display feature information */                           |
|      vty_out(vty, "My Feature Data: ...\n");                     |
|      return CMD_SUCCESS;                                         |
|  }                                                               |
|                                                                  |
|  /* Registration in ospf_vty_init() */                           |
|  install_element(VIEW_NODE, &show_my_feature_cmd);               |
|  install_element(ENABLE_NODE, &show_my_feature_cmd);             |
|                                                                  |
+------------------------------------------------------------------+

Pattern 2: Adding Protocol Extension
+------------------------------------------------------------------+
|                                                                  |
|  /* my_extension.c */                                            |
|                                                                  |
|  /* Initialize extension */                                      |
|  void my_extension_init(struct ospf *ospf)                       |
|  {                                                               |
|      /* Allocate extension-specific data */                      |
|      ospf->my_ext_data = XCALLOC(MTYPE_OSPF_MY_EXT, ...);        |
|                                                                  |
|      /* Register hooks */                                        |
|      ospf_register_opaque_functab(                               |
|          OSPF_OPAQUE_AREA_LSA,                                   |
|          MY_OPAQUE_TYPE,                                         |
|          my_ext_if_new,                                          |
|          my_ext_if_del,                                          |
|          my_ext_ism_change,                                      |
|          my_ext_nsm_change,                                      |
|          my_ext_show,                                            |
|          my_ext_lsa_originate,                                   |
|          my_ext_lsa_refresh                                      |
|      );                                                          |
|  }                                                               |
|                                                                  |
|  /* Hook implementation */                                       |
|  static void my_ext_nsm_change(struct ospf_neighbor *nbr,        |
|                                int old_state)                    |
|  {                                                               |
|      if (nbr->state == NSM_Full && old_state < NSM_Full) {       |
|          /* New adjacency - do something */                      |
|          my_ext_handle_new_adj(nbr);                             |
|      }                                                           |
|  }                                                               |
|                                                                  |
+------------------------------------------------------------------+

Pattern 3: External Route Injection
+------------------------------------------------------------------+
|                                                                  |
|  /* Inject external route into OSPF */                           |
|                                                                  |
|  void inject_external_route(struct ospf *ospf,                   |
|                             struct prefix_ipv4 *p,               |
|                             uint32_t metric)                     |
|  {                                                               |
|      struct external_info *ei;                                   |
|                                                                  |
|      /* Create external info */                                  |
|      ei = ospf_external_info_add(ospf, ZEBRA_ROUTE_STATIC,       |
|                                  0, *p, 0, nexthop, 0);          |
|      if (!ei)                                                    |
|          return;                                                 |
|                                                                  |
|      ei->metric = metric;                                        |
|      ei->type = OSPF_AS_EXTERNAL_METRIC_TYPE_1;                  |
|                                                                  |
|      /* Originate AS-External LSA */                             |
|      ospf_external_lsa_originate(ospf, ei);                      |
|  }                                                               |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**

常见 API 使用模式。

模式 1：添加新 CLI 命令 - 使用 DEFUN 宏定义命令处理函数，在 ospf_vty_init() 中使用 install_element() 注册到相应节点（VIEW_NODE、ENABLE_NODE）。

模式 2：添加协议扩展 - 创建初始化函数分配扩展特定数据并注册钩子（使用 ospf_register_opaque_functab()）。实现钩子函数处理接口变化、状态机变化等事件。

模式 3：外部路由注入 - 使用 ospf_external_info_add() 创建外部信息结构，设置度量和类型，调用 ospf_external_lsa_originate() 产生 AS-External LSA。
