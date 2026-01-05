# Protocol Daemon Architecture - Part 3: WHAT

## Core Data Structures

```
+============================================================================+
|                    OSPF CORE DATA STRUCTURES                               |
+============================================================================+

Hierarchy of OSPF Structures:
+------------------------------------------------------------------+
|                                                                  |
|  +------------------+                                            |
|  | ospf_master (om) |  Global singleton                          |
|  +--------+---------+                                            |
|           |                                                      |
|           | om->ospf (list)                                      |
|           v                                                      |
|  +------------------+  +------------------+                      |
|  | struct ospf      |  | struct ospf      |  ... instances       |
|  | (instance 1)     |  | (instance 2)     |                      |
|  +--------+---------+  +------------------+                      |
|           |                                                      |
|           +---> ospf->areas (list)                               |
|           |     +------------------+                             |
|           |     | struct ospf_area |                             |
|           |     | - area_id        |                             |
|           |     | - lsdb           |                             |
|           |     | - oiflist        |                             |
|           |     +--------+---------+                             |
|           |              |                                       |
|           |              +---> area->oiflist                     |
|           |                   +----------------------+           |
|           |                   | struct ospf_interface|           |
|           |                   | - oi->nbrs (table)   |           |
|           |                   | - oi->state (ISM)    |           |
|           |                   +----------+-----------+           |
|           |                              |                       |
|           |                              +---> nbrs table        |
|           |                                   +------------------+
|           |                                   | struct ospf_nbr  |
|           |                                   | - state (NSM)    |
|           |                                   | - db_sum (list)  |
|           |                                   | - ls_rxmt (list) |
|           |                                   +------------------+
|           |                                                      |
|           +---> ospf->lsdb (AS-external LSAs)                    |
|           +---> ospf->oiflist (all interfaces)                   |
|           +---> ospf->vlinks (virtual links)                     |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**

OSPF 核心数据结构层次。

从顶层开始：ospf_master（om）是全局单例，包含所有 OSPF 实例的列表（om->ospf）。每个 struct ospf 代表一个 OSPF 实例，包含：areas 列表（struct ospf_area）、lsdb（AS 外部 LSA）、oiflist（所有接口）、vlinks（虚拟链路）。

每个 struct ospf_area 代表一个 OSPF 区域，包含：area_id（区域 ID）、lsdb（区域 LSA 数据库）、oiflist（区域内接口列表）。

每个 struct ospf_interface 代表一个 OSPF 接口，包含：nbrs 表（邻居表）、state（接口状态机 ISM 状态）。

每个 struct ospf_nbr（或 ospf_neighbor）代表一个邻居，包含：state（邻居状态机 NSM 状态）、db_sum（数据库摘要列表）、ls_rxmt（LSA 重传列表）。

---

## Neighbor/Peer Structures

```
+===========================================================================+
|                    NEIGHBOR STRUCTURE DETAILS                             |
+===========================================================================+

struct ospf_neighbor (from ospf_neighbor.h):
+------------------------------------------------------------------+
|                                                                  |
|  struct ospf_neighbor {                                          |
|      /* Identity */                                              |
|      struct in_addr router_id;     /* Neighbor's Router ID */    |
|      struct in_addr src;           /* Source IP address */       |
|      struct prefix address;        /* Neighbor's address */      |
|                                                                  |
|      /* State Machine */                                         |
|      uint8_t state;                /* NSM state (Down..Full) */  |
|      uint8_t dd_flags;             /* DB Description flags */    |
|      uint32_t dd_seqnum;           /* DD sequence number */      |
|                                                                  |
|      /* DR Election */                                           |
|      uint8_t priority;             /* Router priority */         |
|      struct in_addr d_router;      /* Designated Router */       |
|      struct in_addr bd_router;     /* Backup DR */               |
|                                                                  |
|      /* Timers */                                                |
|      struct event *t_inactivity;   /* Inactivity timer */        |
|      struct event *t_db_desc;      /* DB-Desc retransmit */      |
|      struct event *t_ls_req;       /* LS-Request retransmit */   |
|      struct event *t_ls_upd;       /* LS-Update retransmit */    |
|                                                                  |
|      /* Database Exchange */                                     |
|      struct ospf_lsdb *db_sum;     /* DB summary list */         |
|      struct ospf_lsdb *ls_rxmt;    /* LS retransmit list */      |
|      struct ospf_lsdb *ls_req;     /* LS request list */         |
|                                                                  |
|      /* Back pointers */                                         |
|      struct ospf_interface *oi;    /* Parent interface */        |
|  };                                                              |
|                                                                  |
+------------------------------------------------------------------+

Neighbor State Values:
+------------------------------------------------------------------+
|                                                                  |
|  enum {                                                          |
|      NSM_DependUpon  = 0,    /* State depends on event */        |
|      NSM_Deleted     = 1,    /* Neighbor deleted */              |
|      NSM_Down        = 2,    /* Initial state */                 |
|      NSM_Attempt     = 3,    /* NBMA: attempting contact */      |
|      NSM_Init        = 4,    /* Hello received */                |
|      NSM_TwoWay      = 5,    /* Bidirectional comm */            |
|      NSM_ExStart     = 6,    /* Negotiating master/slave */      |
|      NSM_Exchange    = 7,    /* Exchanging DBDs */               |
|      NSM_Loading     = 8,    /* Loading LSAs */                  |
|      NSM_Full        = 9,    /* Adjacency complete */            |
|  };                                                              |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**

邻居结构详解。

struct ospf_neighbor 包含：
- 身份信息：router_id（邻居的 Router ID）、src（源 IP 地址）、address（邻居地址）
- 状态机：state（NSM 状态从 Down 到 Full）、dd_flags（数据库描述标志）、dd_seqnum（DD 序列号）
- DR 选举：priority（路由器优先级）、d_router（指定路由器）、bd_router（备份 DR）
- 定时器：t_inactivity（非活动定时器）、t_db_desc（DB-Desc 重传）、t_ls_req（LS-Request 重传）、t_ls_upd（LS-Update 重传）
- 数据库交换：db_sum（DB 摘要列表）、ls_rxmt（LS 重传列表）、ls_req（LS 请求列表）
- 反向指针：oi（父接口）

邻居状态值：NSM_Down（初始状态）、NSM_Init（收到 Hello）、NSM_TwoWay（双向通信）、NSM_ExStart（协商主从）、NSM_Exchange（交换 DBD）、NSM_Loading（加载 LSA）、NSM_Full（邻接完成）。

---

## Route Attributes and LSA Structures

```
+===========================================================================+
|                    LSA STRUCTURE (OSPF Route Attributes)                  |
+===========================================================================+

struct ospf_lsa (from ospf_lsa.h):
+------------------------------------------------------------------+
|                                                                  |
|  struct ospf_lsa {                                               |
|      /* LSA Header (20 bytes, network order) */                  |
|      struct lsa_header *data;                                    |
|      /*                                                          |
|       * struct lsa_header {                                      |
|       *     uint16_t ls_age;        Age in seconds               |
|       *     uint8_t options;        Options field                |
|       *     uint8_t type;           LSA type (1-7)               |
|       *     struct in_addr id;      Link State ID                |
|       *     struct in_addr adv_router; Advertising Router        |
|       *     uint32_t ls_seqnum;     Sequence number              |
|       *     uint16_t checksum;      Fletcher checksum            |
|       *     uint16_t length;        Total length                 |
|       * };                                                       |
|       */                                                         |
|                                                                  |
|      /* Management Fields */                                     |
|      uint32_t flags;               /* OSPF_LSA_* flags */        |
|      int lock;                     /* Reference count */         |
|      time_t tv_recv;               /* Time received */           |
|      time_t tv_orig;               /* Time originated */         |
|                                                                  |
|      /* Refresh Management */                                    |
|      int refresh_list;             /* Refresh queue index */     |
|      struct event *t_refresh;      /* Refresh timer */           |
|                                                                  |
|      /* Back Pointers */                                         |
|      struct ospf_area *area;       /* Containing area */         |
|      struct ospf *ospf;            /* OSPF instance */           |
|  };                                                              |
|                                                                  |
+------------------------------------------------------------------+

LSA Types:
+------------------------------------------------------------------+
|                                                                  |
|  Type 1: Router-LSA        - Router links (intra-area)           |
|  Type 2: Network-LSA       - Transit network (DR generates)      |
|  Type 3: Summary-LSA       - Inter-area routes (ABR)             |
|  Type 4: ASBR-Summary-LSA  - ASBR location (ABR)                 |
|  Type 5: AS-External-LSA   - External routes (ASBR)              |
|  Type 7: NSSA-External-LSA - NSSA external routes                |
|  Type 9-11: Opaque LSAs    - Extension mechanism                 |
|                                                                  |
+------------------------------------------------------------------+

LSDB Organization:
+------------------------------------------------------------------+
|                                                                  |
|  struct ospf_lsdb {                                              |
|      /* Per-type route tables */                                 |
|      struct route_table *type[OSPF_MAX_LSA];                     |
|      /*                                                          |
|       * type[OSPF_ROUTER_LSA]    - Router LSAs                   |
|       * type[OSPF_NETWORK_LSA]   - Network LSAs                  |
|       * type[OSPF_SUMMARY_LSA]   - Summary LSAs                  |
|       * type[OSPF_ASBR_SUMMARY_LSA] - ASBR Summary               |
|       * type[OSPF_AS_EXTERNAL_LSA]  - External LSAs              |
|       * type[OSPF_AS_NSSA_LSA]      - NSSA LSAs                  |
|       * type[OSPF_OPAQUE_*_LSA]     - Opaque LSAs                |
|       */                                                         |
|                                                                  |
|      /* Counts */                                                |
|      unsigned long count;          /* Total LSA count */         |
|      unsigned long total;          /* Total checksum */          |
|  };                                                              |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**

LSA 结构（OSPF 路由属性）。

struct ospf_lsa 包含：
- LSA 头（20 字节，网络字节序）：ls_age（年龄秒数）、options（选项字段）、type（LSA 类型 1-7）、id（链路状态 ID）、adv_router（通告路由器）、ls_seqnum（序列号）、checksum（Fletcher 校验和）、length（总长度）
- 管理字段：flags（OSPF_LSA_* 标志）、lock（引用计数）、tv_recv（接收时间）、tv_orig（产生时间）
- 刷新管理：refresh_list（刷新队列索引）、t_refresh（刷新定时器）
- 反向指针：area（所属区域）、ospf（OSPF 实例）

LSA 类型：类型 1 Router-LSA（路由器链路，区域内）、类型 2 Network-LSA（传输网络，DR 生成）、类型 3 Summary-LSA（区域间路由，ABR）、类型 4 ASBR-Summary-LSA（ASBR 位置，ABR）、类型 5 AS-External-LSA（外部路由，ASBR）、类型 7 NSSA-External-LSA（NSSA 外部路由）、类型 9-11 Opaque LSA（扩展机制）。

LSDB 组织：struct ospf_lsdb 包含按类型分类的路由表数组 type[]，以及 LSA 计数和校验和总计。

---

## FSM Representations

```
+===========================================================================+
|                    FSM IMPLEMENTATION PATTERNS                            |
+===========================================================================+

Interface State Machine (ISM) - from ospf_ism.c:
+------------------------------------------------------------------+
|                                                                  |
|  Interface States:                                               |
|  +-------+     +--------+     +-------+     +--------+           |
|  | Down  |---->| Loopback|    | Point |---->| DR/BDR |           |
|  +-------+     +--------+     | ToPoint|    | Other  |           |
|       |                       +-------+     +--------+           |
|       |        +-------+          |              |               |
|       +------->|Waiting|----------+              |               |
|                +---+---+                         |               |
|                    |                             |               |
|                    v                             v               |
|                +-------+                    +--------+           |
|                |DR/BDR |                    | Backup |           |
|                +-------+                    +--------+           |
|                                                                  |
+------------------------------------------------------------------+

ISM State Transition Table:
+------------------------------------------------------------------+
|                                                                  |
|  /* ospf_ism.c simplified */                                     |
|  struct {                                                        |
|      int (*func)(struct ospf_interface *);                       |
|      int next_state;                                             |
|  } ISM[ISM_STATE_MAX][ISM_EVENT_MAX] = {                         |
|      /* ISM_Down */                                              |
|      {                                                           |
|          { ism_ignore, ISM_DependUpon },    /* NoEvent */        |
|          { ism_interface_up, ISM_DependUpon }, /* InterfaceUp */ |
|          { ism_ignore, ISM_Down },          /* WaitTimer */      |
|          ...                                                     |
|      },                                                          |
|      /* ISM_Waiting */                                           |
|      {                                                           |
|          { ism_ignore, ISM_DependUpon },                         |
|          { ism_ignore, ISM_Waiting },                            |
|          { ism_wait_timer, ISM_DependUpon }, /* WaitTimer */     |
|          ...                                                     |
|      },                                                          |
|      ...                                                         |
|  };                                                              |
|                                                                  |
+------------------------------------------------------------------+

FSM Execution Pattern:
+------------------------------------------------------------------+
|                                                                  |
|  void ospf_ism_event(struct ospf_interface *oi, int event)       |
|  {                                                               |
|      int old_state = oi->state;                                  |
|      int new_state;                                              |
|      int (*func)(struct ospf_interface *);                       |
|                                                                  |
|      /* Look up transition */                                    |
|      func = ISM[oi->state][event].func;                          |
|      new_state = ISM[oi->state][event].next_state;               |
|                                                                  |
|      /* Execute action function */                               |
|      if (func != NULL)                                           |
|          new_state = (*func)(oi);                                |
|                                                                  |
|      /* State transition */                                      |
|      if (new_state != ISM_DependUpon)                            |
|          oi->state = new_state;                                  |
|                                                                  |
|      /* Post-transition actions */                               |
|      if (old_state != oi->state)                                 |
|          ism_change_state(oi, old_state);                        |
|  }                                                               |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**

FSM 实现模式。

接口状态机（ISM）状态：Down → Loopback（环回）或 Waiting → Point-to-Point 或 DR/BDR/DROther/Backup。

ISM 状态转换表：使用二维数组 ISM[状态][事件] 定义，每个元素包含处理函数指针和下一个状态。例如 ISM_Down 状态收到 InterfaceUp 事件会调用 ism_interface_up 函数；ISM_Waiting 状态的 WaitTimer 到期会调用 ism_wait_timer 函数。

FSM 执行模式：ospf_ism_event() 函数执行步骤：(1) 保存旧状态；(2) 查找转换表获取处理函数和下一状态；(3) 执行动作函数；(4) 如果下一状态不是 DependUpon 则更新状态；(5) 如果状态改变则执行后转换动作 ism_change_state()。

---

## Timers and Events

```
+===========================================================================+
|                    TIMER MANAGEMENT                                       |
+===========================================================================+

OSPF Timer Categories:
+------------------------------------------------------------------+
|                                                                  |
|  1. Protocol Timers (RFC-defined)                                |
|  +----------------------------------------------------------+   |
|  | HelloInterval    | Time between Hello packets (10s def)  |   |
|  | RouterDeadInterval| Time to declare neighbor dead (40s)  |   |
|  | RxmtInterval     | Retransmit interval (5s default)      |   |
|  | InfTransDelay    | Link transmission delay (1s)          |   |
|  +----------------------------------------------------------+   |
|                                                                  |
|  2. FSM Timers                                                   |
|  +----------------------------------------------------------+   |
|  | t_hello          | Periodic Hello transmission           |   |
|  | t_wait           | Wait timer (DR election)              |   |
|  | t_inactivity     | Neighbor inactivity (dead interval)   |   |
|  +----------------------------------------------------------+   |
|                                                                  |
|  3. LSA Timers                                                   |
|  +----------------------------------------------------------+   |
|  | t_lsa_refresher  | Periodic LSA refresh (30 min)         |   |
|  | t_maxage_walker  | MaxAge LSA cleanup                    |   |
|  | MinLSInterval    | Min time between LSA origins (5s)     |   |
|  | MinLSArrival     | Min time between LSA accepts (1s)     |   |
|  +----------------------------------------------------------+   |
|                                                                  |
|  4. Calculation Timers                                           |
|  +----------------------------------------------------------+   |
|  | t_spf_calc       | SPF calculation delay                 |   |
|  | t_ase_calc       | External route calculation delay      |   |
|  | t_abr_task       | ABR task timer                        |   |
|  +----------------------------------------------------------+   |
|                                                                  |
+------------------------------------------------------------------+

Timer Registration Pattern:
+------------------------------------------------------------------+
|                                                                  |
|  /* From ospfd.c - Timer setup during instance creation */       |
|                                                                  |
|  /* MaxAge walker - runs periodically */                         |
|  event_add_timer(master, ospf_lsa_maxage_walker, new,            |
|                  OSPF_LSA_MAXAGE_CHECK_INTERVAL,                  |
|                  &new->t_maxage_walker);                          |
|                                                                  |
|  /* LSA refresh walker */                                        |
|  event_add_timer(master, ospf_lsa_refresh_walker, new,           |
|                  new->lsa_refresh_interval,                       |
|                  &new->t_lsa_refresher);                          |
|                                                                  |
|  /* Hello timer on interface */                                  |
|  OSPF_HELLO_TIMER_ON(oi);                                        |
|  /* Expands to: */                                               |
|  event_add_timer(master, ospf_hello_timer, oi,                   |
|                  OSPF_IF_PARAM(oi, v_hello),                      |
|                  &oi->t_hello);                                   |
|                                                                  |
|  /* One-shot timer example (neighbor inactivity) */              |
|  OSPF_NSM_TIMER_ON(nbr->t_inactivity, ospf_inactivity_timer,     |
|                    nbr->v_inactivity);                            |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**

定时器管理。

OSPF 定时器分类：
1. 协议定时器（RFC 定义）：HelloInterval（Hello 包间隔，默认 10 秒）、RouterDeadInterval（声明邻居死亡时间，40 秒）、RxmtInterval（重传间隔，5 秒）、InfTransDelay（链路传输延迟，1 秒）
2. FSM 定时器：t_hello（周期性 Hello 发送）、t_wait（等待定时器用于 DR 选举）、t_inactivity（邻居非活动定时器）
3. LSA 定时器：t_lsa_refresher（LSA 刷新，30 分钟）、t_maxage_walker（MaxAge LSA 清理）、MinLSInterval（LSA 产生最小间隔，5 秒）、MinLSArrival（LSA 接受最小间隔，1 秒）
4. 计算定时器：t_spf_calc（SPF 计算延迟）、t_ase_calc（外部路由计算延迟）、t_abr_task（ABR 任务定时器）

定时器注册模式：使用 event_add_timer() 注册定时器，参数包括事件循环、回调函数、参数、间隔、定时器指针。使用宏如 OSPF_HELLO_TIMER_ON、OSPF_NSM_TIMER_ON 简化注册。

---

## Policy Hooks

```
+===========================================================================+
|                    POLICY AND FILTERING HOOKS                             |
+===========================================================================+

Route Filtering Points:
+------------------------------------------------------------------+
|                                                                  |
|  1. Distribute List (outbound/inbound filtering)                 |
|     +--------------------------------------------------------+  |
|     | Applied at route redistribution                        |  |
|     | - Filter routes from other protocols into OSPF         |  |
|     | - Filter routes from OSPF to other protocols           |  |
|     +--------------------------------------------------------+  |
|                                                                  |
|  2. Area Range (summarization)                                   |
|     +--------------------------------------------------------+  |
|     | ospf->areas[n]->ranges table                           |  |
|     | - Summarize intra-area routes at ABR                   |  |
|     | - Suppress component routes                            |  |
|     +--------------------------------------------------------+  |
|                                                                  |
|  3. Route-Map Application                                        |
|     +--------------------------------------------------------+  |
|     | ospf_redistribute_check()                              |  |
|     | - Modify route attributes                              |  |
|     | - Set metric, metric-type, tag                         |  |
|     | - Permit/deny routes                                   |  |
|     +--------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+

Policy Application Flow:
+------------------------------------------------------------------+
|                                                                  |
|  External Route Origination:                                     |
|                                                                  |
|  +-------------+                                                 |
|  | Zebra Route |                                                 |
|  | Received    |                                                 |
|  +------+------+                                                 |
|         |                                                        |
|         v                                                        |
|  +-------------+                                                 |
|  | Distribute  |---> Denied? ---> Drop                           |
|  | List Check  |                                                 |
|  +------+------+                                                 |
|         | Permitted                                              |
|         v                                                        |
|  +-------------+                                                 |
|  | Route-Map   |---> Modify attributes                           |
|  | Application |    (metric, type, tag)                          |
|  +------+------+                                                 |
|         |                                                        |
|         v                                                        |
|  +-------------+                                                 |
|  | Originate   |                                                 |
|  | AS-External |                                                 |
|  | LSA         |                                                 |
|  +-------------+                                                 |
|                                                                  |
+------------------------------------------------------------------+

Code Pattern for Policy Check:
+------------------------------------------------------------------+
|                                                                  |
|  /* From ospf_asbr.c - Redistribution check */                   |
|  int ospf_redistribute_check(struct ospf *ospf,                  |
|                              struct external_info *ei,           |
|                              int *changed)                       |
|  {                                                               |
|      struct ospf_redist *red;                                    |
|      struct prefix_ipv4 *p = &ei->p;                             |
|                                                                  |
|      /* Get redistribution config */                             |
|      red = ospf_redist_lookup(ospf, ei->type, ei->instance);     |
|                                                                  |
|      /* Apply route-map if configured */                         |
|      if (ROUTEMAP_NAME(red)) {                                   |
|          route_map_result_t ret;                                 |
|          ret = route_map_apply(ROUTEMAP(red), p, ei);            |
|          if (ret == RMAP_DENYMATCH)                              |
|              return 0;  /* Filtered */                           |
|      }                                                           |
|                                                                  |
|      return 1;  /* Permitted */                                  |
|  }                                                               |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**

策略和过滤钩子。

路由过滤点：
1. 分发列表（出站/入站过滤）- 应用于路由重分发，过滤从其他协议进入 OSPF 的路由，或从 OSPF 到其他协议的路由
2. 区域范围（汇总）- ospf->areas[n]->ranges 表，在 ABR 汇总区域内路由，抑制组件路由
3. 路由映射应用 - ospf_redistribute_check() 函数，修改路由属性（设置 metric、metric-type、tag），允许/拒绝路由

策略应用流程：外部路由产生时，首先从 Zebra 收到路由，然后进行分发列表检查（被拒绝则丢弃），通过后应用路由映射修改属性，最后产生 AS-External LSA。

策略检查代码模式：ospf_redistribute_check() 函数获取重分发配置，如果配置了路由映射则应用它，返回 RMAP_DENYMATCH 表示被过滤，返回 1 表示允许。
