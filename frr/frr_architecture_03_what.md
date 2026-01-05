# FRR Architecture Guide - Part 3: WHAT | Architecture and Concrete Forms

## ASCII Architecture Overview

```
+==============================================================================+
|                 WHAT FRR LOOKS LIKE IN CODE - Concrete Forms                 |
+==============================================================================+

                        CORE ARCHITECTURAL PATTERNS
+-----------------------------------------------------------------------------+
|                                                                             |
|   Pattern 1: DAEMON-BASED MODULAR ARCHITECTURE                              |
|   =============================================                             |
|                                                                             |
|   +--------+  +--------+  +--------+  +--------+  +--------+                |
|   | bgpd/  |  | ospfd/ |  | isisd/ |  | ripd/  |  |staticd/|                |
|   |        |  |        |  |        |  |        |  |        |                |
|   | main() |  | main() |  | main() |  | main() |  | main() |                |
|   +---+----+  +---+----+  +---+----+  +---+----+  +---+----+                |
|       |           |           |           |           |                     |
|       +-----------+-----------+-----------+-----------+                     |
|                               |                                             |
|                               v                                             |
|                     +-------------------+                                   |
|                     |     lib/          |                                   |
|                     | (shared code)     |                                   |
|                     |  - event.c        |                                   |
|                     |  - memory.c       |                                   |
|                     |  - log.c          |                                   |
|                     |  - zclient.c      |                                   |
|                     +-------------------+                                   |
|                                                                             |
+-----------------------------------------------------------------------------+

   Pattern 2: EVENT LOOP AND CALLBACKS
   ====================================

+-----------------------------------------------------------------------------+
|                                                                             |
|   +------------------------------------------------------------------+      |
|   |                     struct event_loop (master)                   |      |
|   +------------------------------------------------------------------+      |
|   |                                                                  |      |
|   |  +------------+  +------------+  +------------+  +------------+  |      |
|   |  | timer heap |  | read array |  | write array|  | event list |  |      |
|   |  +-----+------+  +-----+------+  +-----+------+  +-----+------+  |      |
|   |        |               |               |               |         |      |
|   |        +---------------+---------------+---------------+         |      |
|   |                        |                                         |      |
|   |                        v                                         |      |
|   |                 +-------------+                                  |      |
|   |                 | ready queue |                                  |      |
|   |                 +------+------+                                  |      |
|   |                        |                                         |      |
|   +------------------------|-----------------------------------------+      |
|                            |                                                |
|                            v                                                |
|   +------------------------------------------------------------------+      |
|   |                    struct event                                  |      |
|   +------------------------------------------------------------------+      |
|   | type: EVENT_READ | EVENT_TIMER | EVENT_EVENT | ...               |      |
|   | func: callback function pointer                                  |      |
|   | arg:  user data                                                  |      |
|   | u:    union { fd, sands (timer), val }                           |      |
|   | hist: CPU usage history                                          |      |
|   +------------------------------------------------------------------+      |
|                                                                             |
+-----------------------------------------------------------------------------+

   Pattern 3: EXPLICIT STATE MACHINES
   ===================================

+-----------------------------------------------------------------------------+
|                                                                             |
|   Example: BGP Peer FSM (Simplified)                                        |
|                                                                             |
|                           +-------+                                         |
|                           | Idle  |<--------------+                         |
|                           +---+---+               |                         |
|                               |                   |                         |
|                               | Start             | Error/Stop              |
|                               v                   |                         |
|                          +--------+               |                         |
|                          | Connect|---------------+                         |
|                          +---+----+               |                         |
|                              |                    |                         |
|                              | TCP Connected      |                         |
|                              v                    |                         |
|                         +---------+               |                         |
|                         | OpenSent|---------------+                         |
|                         +----+----+               |                         |
|                              |                    |                         |
|                              | Open Received      |                         |
|                              v                    |                         |
|                       +-----------+               |                         |
|                       |OpenConfirm|---------------+                         |
|                       +-----+-----+               |                         |
|                             |                     |                         |
|                             | Keepalive Received  |                         |
|                             v                     |                         |
|                      +-------------+              |                         |
|                      | Established |              |                         |
|                      +------+------+              |                         |
|                             |                     |                         |
|                             +---------------------+                         |
|                                                                             |
+-----------------------------------------------------------------------------+

                        KEY DATA STRUCTURES
+-----------------------------------------------------------------------------+
|                                                                             |
|   ROUTE LIFECYCLE DATA STRUCTURES                                           |
|                                                                             |
|   Protocol Layer:                                                           |
|   +-----------------------+                                                 |
|   | bgp_path_info         |  <- BGP-specific route info                     |
|   | ospf_route            |  <- OSPF-specific route info                    |
|   | isis_route_info       |  <- IS-IS-specific route info                   |
|   +-----------+-----------+                                                 |
|               |                                                             |
|               | Abstracted via ZAPI                                         |
|               v                                                             |
|   Zebra Layer:                                                              |
|   +-----------------------+                                                 |
|   | struct route_entry    |  <- Generic RIB entry                           |
|   |   - type (BGP/OSPF/..)                                                  |
|   |   - distance                                                            |
|   |   - metric                                                              |
|   |   - nexthop_group                                                       |
|   |   - flags                                                               |
|   +-----------+-----------+                                                 |
|               |                                                             |
|               | Best path selected                                          |
|               v                                                             |
|   Kernel Layer:                                                             |
|   +-----------------------+                                                 |
|   | FIB Entry             |  <- Installed in kernel                         |
|   |   - prefix/mask                                                         |
|   |   - nexthop(s)                                                          |
|   |   - interface                                                           |
|   +-----------------------+                                                 |
|                                                                             |
+-----------------------------------------------------------------------------+

                        ROUTE TABLE HIERARCHY
+-----------------------------------------------------------------------------+
|                                                                             |
|   +-----------------------------------------------------------------------+ |
|   |                        struct zebra_vrf                               | |
|   |  (VRF container - default VRF or custom VRFs)                         | |
|   +-----------------------------------------------------------------------+ |
|               |                                                             |
|               | Contains multiple address families                          |
|               v                                                             |
|   +-------------------+  +-------------------+  +-------------------+       |
|   | table[AFI_IP]     |  | table[AFI_IP6]    |  | table[AFI_L2VPN]  |       |
|   | (IPv4 routes)     |  | (IPv6 routes)     |  | (EVPN routes)     |       |
|   +--------+----------+  +-------------------+  +-------------------+       |
|            |                                                                |
|            | Multiple sub-tables (SAFI)                                     |
|            v                                                                |
|   +-------------------+  +-------------------+                              |
|   |SAFI_UNICAST table |  |SAFI_MULTICAST tab |                              |
|   +--------+----------+  +-------------------+                              |
|            |                                                                |
|            | Radix tree for prefix lookup                                   |
|            v                                                                |
|   +-------------------+                                                     |
|   | struct route_node |  <- Prefix in radix tree                            |
|   |   - prefix/mask   |                                                     |
|   |   - info (routes) |---> route_entry -> route_entry -> ...               |
|   +-------------------+          ^              ^                           |
|                                  |              |                           |
|                              from BGP      from OSPF                        |
|                                                                             |
+-----------------------------------------------------------------------------+

                        NEXTHOP STRUCTURES
+-----------------------------------------------------------------------------+
|                                                                             |
|   +-----------------------------------------------------------------------+ |
|   |                    struct nexthop_group                               | |
|   |  (Container for multiple nexthops - ECMP support)                     | |
|   +-----------------------------------+-----------------------------------+ |
|                                       |                                     |
|         +-----------------------------+-----------------------------+       |
|         |                             |                             |       |
|         v                             v                             v       |
|   +-----------+                 +-----------+                 +-----------+ |
|   | nexthop 1 |                 | nexthop 2 |                 | nexthop 3 | |
|   +-----------+                 +-----------+                 +-----------+ |
|   | type      |                 | type      |                 | type      | |
|   | gate.ipv4 |                 | gate.ipv4 |                 | gate.ipv6 | |
|   | ifindex   |                 | ifindex   |                 | ifindex   | |
|   | weight    |                 | weight    |                 | weight    | |
|   +-----------+                 +-----------+                 +-----------+ |
|                                                                             |
|   Nexthop Types:                                                            |
|   - NEXTHOP_TYPE_IFINDEX      (直连接口)                                     |
|   - NEXTHOP_TYPE_IPV4         (IPv4 网关)                                   |
|   - NEXTHOP_TYPE_IPV4_IFINDEX (IPv4 网关 + 接口)                             |
|   - NEXTHOP_TYPE_IPV6         (IPv6 网关)                                   |
|   - NEXTHOP_TYPE_BLACKHOLE    (黑洞路由)                                    |
|                                                                             |
+-----------------------------------------------------------------------------+
```

---

## 中文详细说明

### 1. FRR 使用的核心架构模式

#### 1.1 守护进程模块化架构

每个路由协议运行在独立进程中：

```
目录结构：
frr/
├── bgpd/           # BGP 守护进程
│   ├── bgp_main.c  # 入口点
│   ├── bgp_fsm.c   # 状态机
│   └── bgp_route.c # 路由处理
├── ospfd/          # OSPF 守护进程
├── isisd/          # IS-IS 守护进程
├── zebra/          # 路由管理器
└── lib/            # 共享库
```

**模块化的好处：**
- 故障隔离：bgpd 崩溃不影响 ospfd
- 独立开发：协议团队可以并行工作
- 选择性部署：只启动需要的协议

#### 1.2 事件循环和回调

从 `event.c` 看核心数据结构：

```c
/* 事件类型 */
enum event_types {
    EVENT_READ,     // 文件描述符可读
    EVENT_WRITE,    // 文件描述符可写
    EVENT_TIMER,    // 定时器到期
    EVENT_EVENT,    // 普通事件
    EVENT_READY,    // 已就绪
    EVENT_UNUSED,   // 空闲
    EVENT_EXECUTE,  // 立即执行
};

/* 事件结构 */
struct event {
    uint8_t type;                    // 事件类型
    struct event_loop *master;       // 所属事件循环
    void (*func)(struct event *);    // 回调函数
    void *arg;                       // 用户数据
    union {
        int fd;                      // 文件描述符
        struct timeval sands;        // 定时器时间
        int val;                     // 事件值
    } u;
    struct cpu_event_history *hist;  // CPU 统计
};
```

**典型使用模式：**

```c
/* 注册读事件 */
event_add_read(master, bgp_read_callback, peer, peer->fd, &peer->t_read);

/* 注册定时器 */
event_add_timer(master, bgp_keepalive_timer, peer, 
                peer->v_keepalive, &peer->t_keepalive);

/* 回调函数 */
void bgp_read_callback(struct event *event)
{
    struct peer *peer = EVENT_ARG(event);
    
    /* 处理数据 */
    bgp_read(peer);
    
    /* 重新注册（如果需要继续监听） */
    event_add_read(peer->master, bgp_read_callback, peer, 
                   peer->fd, &peer->t_read);
}
```

#### 1.3 显式有限状态机

FRR 大量使用显式状态机管理协议状态：

```c
/* BGP FSM 状态 (概念示例) */
enum bgp_fsm_state {
    BGP_STATE_IDLE,
    BGP_STATE_CONNECT,
    BGP_STATE_ACTIVE,
    BGP_STATE_OPENSENT,
    BGP_STATE_OPENCONFIRM,
    BGP_STATE_ESTABLISHED,
    BGP_STATE_CLEARING,
    BGP_STATE_DELETED,
};

/* 状态转换 */
void bgp_fsm_change_state(struct peer *peer, int new_state)
{
    int old_state = peer->state;
    
    /* 退出旧状态动作 */
    bgp_fsm_state_exit(peer, old_state);
    
    /* 更新状态 */
    peer->state = new_state;
    
    /* 进入新状态动作 */
    bgp_fsm_state_enter(peer, new_state);
    
    /* 日志记录 */
    zlog_info("Peer %s: %s -> %s",
              peer->host,
              bgp_state_str(old_state),
              bgp_state_str(new_state));
}
```

---

### 2. 关键 FRR 子系统及其角色

#### 2.1 Zebra（RIB/FIB 管理器）

```
为什么 Zebra 存在？
+==================================================================+
|  问题：多个协议可能为同一目的地提供路由                             |
|                                                                  |
|  BGP:  10.0.0.0/24 via 192.168.1.1  (distance=20)                |
|  OSPF: 10.0.0.0/24 via 192.168.2.1  (distance=110)               |
|                                                                  |
|  Zebra 的职责：                                                   |
|  1. 接收所有协议的路由                                             |
|  2. 按 admin distance 选择最佳                                    |
|  3. 只将最佳路由安装到内核                                         |
|  4. 通知协议其路由的安装状态                                       |
+==================================================================+
```

#### 2.2 协议守护进程角色

| 守护进程 | 协议 | 主要职责 |
|----------|------|----------|
| `bgpd` | BGP-4 | AS 间路由、策略控制 |
| `ospfd` | OSPFv2 | IPv4 域内路由 |
| `ospf6d` | OSPFv3 | IPv6 域内路由 |
| `isisd` | IS-IS | 多协议域内路由 |
| `ripd` | RIPv2 | 简单距离矢量 |
| `staticd` | 静态 | 静态路由管理 |
| `mgmtd` | 管理 | 配置管理（新架构） |

#### 2.3 lib/ 共享基础设施

```
lib/ 目录内容：
+==================================================================+
|  文件              |  功能                                        |
+==================================================================+
|  event.c           |  事件循环和回调管理                          |
|  memory.c          |  内存分配和跟踪                              |
|  log.c             |  日志系统                                    |
|  command.c         |  CLI 命令框架                                |
|  zclient.c         |  协议到 Zebra 的客户端库                     |
|  prefix.c          |  IP 前缀操作                                 |
|  table.c           |  路由表（radix tree）                        |
|  nexthop.c         |  下一跳管理                                  |
|  vrf.c             |  VRF 支持                                    |
|  stream.c          |  消息序列化                                  |
|  hash.c            |  哈希表实现                                  |
+==================================================================+
```

---

### 3. 核心数据结构（概念层面）

#### 3.1 路由表示

```c
/* 前缀结构 */
struct prefix {
    uint8_t family;      // AF_INET 或 AF_INET6
    uint8_t prefixlen;   // 前缀长度
    union {
        struct in_addr  prefix4;
        struct in6_addr prefix6;
    } u;
};

/* RIB 条目 */
struct route_entry {
    uint8_t type;              // 路由类型 (ZEBRA_ROUTE_BGP, etc.)
    uint8_t distance;          // 管理距离
    uint32_t metric;           // 度量值
    struct nexthop_group *nhe; // 下一跳组
    uint32_t flags;            // 标志位
    time_t uptime;             // 安装时间
};
```

#### 3.2 VRF 和路由表

```c
/* VRF 结构 */
struct zebra_vrf {
    vrf_id_t vrf_id;
    char *name;
    
    /* 每个地址族的路由表 */
    struct route_table *table[AFI_MAX][SAFI_MAX];
    
    /* 其他 VRF 相关资源 */
};
```

---

### 4. 路由生命周期（端到端）

```
完整路由生命周期：
+==================================================================+

Step 1: 协议学习路由
------------------------
  BGP 从邻居收到 UPDATE 消息
  → 解析 NLRI (Network Layer Reachability Information)
  → 创建 bgp_path_info 结构
  → 应用入站策略 (route-map in)

Step 2: 最佳路径选择
------------------------
  bgp_best_selection()
  → 比较 AS 路径长度
  → 比较 origin 类型
  → 比较 MED
  → 比较 IGP metric
  → 选择最佳路径

Step 3: 通告到 Zebra
------------------------
  bgp_zebra_announce()
  → 构建 ZAPI 消息
  → 发送 ZAPI_ROUTE_ADD
  → 等待确认

Step 4: Zebra RIB 处理
------------------------
  rib_add_multipath()
  → 插入到 RIB
  → 与其他协议比较 (admin distance)
  → 选择最佳 RIB 条目

Step 5: 内核安装
------------------------
  rib_install_kernel()
  → 构建 Netlink RTM_NEWROUTE 消息
  → 发送到内核
  → 处理内核响应

Step 6: 路由撤销
------------------------
  当邻居发送 WITHDRAW:
  → bgp_path_info 标记删除
  → ZAPI_ROUTE_DELETE
  → Zebra 从 RIB 移除
  → 内核 RTM_DELROUTE

+==================================================================+
```

---

### 5. FRR 公共库和 API

#### 5.1 lib/ 中的功能

```
可复用的功能：
+==================================================================+
|  类别          |  API                  |  用途                   |
+==================================================================+
|  事件循环      |  event_add_*()        |  异步编程框架           |
|               |  event_cancel()       |                         |
+------------------------------------------------------------------+
|  内存管理      |  XMALLOC/XFREE       |  带类型跟踪的分配       |
|               |  MTYPE_*             |  内存类型定义           |
+------------------------------------------------------------------+
|  日志          |  zlog_*()            |  分级日志               |
|               |  flog_*()            |  格式化日志             |
+------------------------------------------------------------------+
|  CLI           |  DEFUN()             |  命令定义               |
|               |  install_element()   |  命令注册               |
+------------------------------------------------------------------+
|  前缀操作      |  prefix_match()      |  前缀匹配               |
|               |  prefix_copy()       |  前缀复制               |
+------------------------------------------------------------------+
|  Zebra 客户端  |  zclient_new()       |  创建 Zebra 连接        |
|               |  zapi_route_*()      |  路由操作               |
+==================================================================+
```

#### 5.2 稳定 vs 内部 API

```
API 稳定性：
+==================================================================+
|  稳定（可用于外部项目）          |  内部（可能变化）            |
+==================================================================+
|  - lib/event.h 核心 API          |  - daemon/*_internal.h      |
|  - lib/prefix.h                  |  - 带 _internal 后缀的函数  |
|  - lib/zclient.h 基本接口        |  - 未导出的静态函数         |
|  - lib/memory.h                  |  - 内部数据结构字段         |
+==================================================================+
```

---

### 6. 架构边界和权衡

#### 成本分析

```
性能 vs 正确性权衡：
+==================================================================+
|  选择                    |  代价                 |  收益           |
+==================================================================+
|  单线程事件循环          |  无法利用多核         |  无并发 bug     |
|  IPC 而非共享内存        |  消息传递开销         |  进程隔离       |
|  显式状态机              |  更多代码             |  可预测行为     |
|  所有路由经过 Zebra      |  额外跳转             |  统一 RIB 管理  |
+==================================================================+
```

#### FRR 避免的抽象

```
FRR 故意保持简单的地方：
+==================================================================+
|  1. 不使用 C++ 类层次结构                                         |
|     → 直接使用 C 结构体和函数指针                                 |
|                                                                   |
|  2. 不使用通用消息总线                                            |
|     → 使用专门的 ZAPI 协议                                        |
|                                                                   |
|  3. 不抽象内核接口                                                |
|     → 直接使用 Netlink（Linux）或 routing socket（BSD）          |
|                                                                   |
|  4. 不使用 ORM 或数据库                                           |
|     → 内存中的数据结构                                            |
+==================================================================+
```
