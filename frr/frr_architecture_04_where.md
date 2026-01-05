# FRR Architecture Guide - Part 4: WHERE | Source Code Reading Strategy

## ASCII Architecture Overview

```
+==============================================================================+
|                WHERE TO READ - FRR Source Code Navigation                    |
+==============================================================================+

                        FRR SOURCE TREE STRUCTURE
+-----------------------------------------------------------------------------+
|                                                                             |
|   frr/                                                                      |
|   ├── README.md              <- Start here                                  |
|   ├── configure.ac           <- Build configuration                         |
|   ├── Makefile.am            <- Build rules                                 |
|   │                                                                         |
|   ├── doc/                   <- Documentation                               |
|   │   ├── user/              <- User guide                                  |
|   │   ├── developer/         <- Developer guide                             |
|   │   └── manpages/          <- Man pages                                   |
|   │                                                                         |
|   ├── lib/                   <- CRITICAL: Shared infrastructure             |
|   │   ├── event.c            <- Event loop (READ THIS FIRST)                |
|   │   ├── memory.c           <- Memory management                           |
|   │   ├── log.c              <- Logging                                     |
|   │   ├── command.c          <- CLI framework                               |
|   │   ├── zclient.c          <- Zebra client library                        |
|   │   ├── prefix.c           <- IP prefix operations                        |
|   │   ├── table.c            <- Routing table (radix tree)                  |
|   │   ├── nexthop.c          <- Nexthop handling                            |
|   │   ├── vrf.c              <- VRF support                                 |
|   │   └── stream.c           <- Message serialization                       |
|   │                                                                         |
|   ├── zebra/                 <- CRITICAL: Core routing infrastructure       |
|   │   ├── main.c             <- Zebra entry point                           |
|   │   ├── zserv.c            <- ZAPI server                                 |
|   │   ├── rib.c              <- RIB management                              |
|   │   ├── zebra_rib.c        <- RIB operations                              |
|   │   ├── rt_netlink.c       <- Linux Netlink interface                     |
|   │   ├── zebra_vrf.c        <- VRF management                              |
|   │   └── redistribute.c     <- Route redistribution                        |
|   │                                                                         |
|   ├── bgpd/                  <- BGP protocol daemon                         |
|   │   ├── bgp_main.c         <- BGP entry point                             |
|   │   ├── bgp_fsm.c          <- BGP state machine                           |
|   │   ├── bgp_route.c        <- Route handling                              |
|   │   ├── bgp_attr.c         <- Path attributes                             |
|   │   ├── bgp_open.c         <- OPEN message                                |
|   │   ├── bgp_update.c       <- UPDATE message                              |
|   │   └── bgp_zebra.c        <- Zebra interaction                           |
|   │                                                                         |
|   ├── ospfd/                 <- OSPF v2 daemon                              |
|   │   ├── ospf_main.c        <- Entry point                                 |
|   │   ├── ospf_spf.c         <- SPF calculation                             |
|   │   ├── ospf_lsa.c         <- LSA handling                                |
|   │   └── ospf_zebra.c       <- Zebra interaction                           |
|   │                                                                         |
|   ├── isisd/                 <- IS-IS daemon                                |
|   ├── ripd/                  <- RIP daemon                                  |
|   ├── staticd/               <- Static routes daemon                        |
|   │                                                                         |
|   ├── vtysh/                 <- CLI shell                                   |
|   │   ├── vtysh.c            <- Shell main                                  |
|   │   └── vtysh_cmd.c        <- Command definitions                         |
|   │                                                                         |
|   └── tests/                 <- Unit and integration tests                  |
|                                                                             |
+-----------------------------------------------------------------------------+

                        RECOMMENDED READING ORDER
+-----------------------------------------------------------------------------+
|                                                                             |
|   Phase 1: ORIENTATION                                                      |
|   =====================                                                     |
|                                                                             |
|   +--------+     +--------+     +-------------+     +-----------+           |
|   |README.md|--->|doc/    |--->|configure.ac |--->| Makefile.am|            |
|   +--------+     +--------+     +-------------+     +-----------+           |
|                                                                             |
|   Goal: Understand project structure, build system, documentation           |
|                                                                             |
+-----------------------------------------------------------------------------+
|                                                                             |
|   Phase 2: SHARED INFRASTRUCTURE (Critical!)                                |
|   ==========================================                                |
|                                                                             |
|   lib/                                                                      |
|   +----------+     +----------+     +---------+     +----------+            |
|   | event.c  |--->| memory.c |--->| log.c   |--->| command.c |              |
|   +----------+     +----------+     +---------+     +----------+            |
|        |                                                 |                  |
|        v                                                 v                  |
|   +----------+     +----------+     +---------+     +----------+            |
|   | prefix.c |--->| table.c  |--->| nexthop.c|--->| zclient.c|              |
|   +----------+     +----------+     +---------+     +----------+            |
|                                                                             |
|   Goal: Master the foundation all daemons depend on                         |
|                                                                             |
+-----------------------------------------------------------------------------+
|                                                                             |
|   Phase 3: ZEBRA (Core Routing)                                             |
|   =============================                                             |
|                                                                             |
|   zebra/                                                                    |
|   +--------+     +--------+     +-------------+     +------------+          |
|   | main.c |--->| zserv.c|--->| rib.c       |--->|rt_netlink.c|             |
|   +--------+     +--------+     +-------------+     +------------+          |
|        |                              |                   |                 |
|        v                              v                   v                 |
|   +------------+     +-------------+     +----------------+                 |
|   |zebra_vrf.c |--->|redistribute.c|--->| zebra_nhg.c   |                   |
|   +------------+     +-------------+     +----------------+                 |
|                                                                             |
|   Goal: Understand the heart of FRR's routing architecture                  |
|                                                                             |
+-----------------------------------------------------------------------------+
|                                                                             |
|   Phase 4: ONE PROTOCOL (Deep Dive)                                         |
|   ==================================                                        |
|                                                                             |
|   Recommended: bgpd/ OR ospfd/                                              |
|                                                                             |
|   bgpd/                                                                     |
|   +-----------+     +----------+     +-----------+     +------------+       |
|   | bgp_main.c|--->| bgp_fsm.c|--->|bgp_route.c|--->|bgp_zebra.c |          |
|   +-----------+     +----------+     +-----------+     +------------+       |
|                          |                                                  |
|                          v                                                  |
|   +-----------+     +----------+     +-----------+                          |
|   |bgp_open.c |--->|bgp_update|--->| bgp_attr.c|                            |
|   +-----------+     +----------+     +-----------+                          |
|                                                                             |
|   Goal: Learn one protocol deeply before generalizing                       |
|                                                                             |
+-----------------------------------------------------------------------------+
|                                                                             |
|   Phase 5: MANAGEMENT AND CLI                                               |
|   ===========================                                               |
|                                                                             |
|   vtysh/                                                                    |
|   +---------+     +------------+     +-----------+                          |
|   | vtysh.c |--->| vtysh_cmd.c|--->| vtysh_*.c |                            |
|   +---------+     +------------+     +-----------+                          |
|                                                                             |
|   lib/command.c (revisit)                                                   |
|                                                                             |
|   Goal: Understand configuration flow into runtime state                    |
|                                                                             |
+-----------------------------------------------------------------------------+

                     KEY FILES FOR EACH CONCEPT
+-----------------------------------------------------------------------------+
|                                                                             |
|   CONCEPT: Event-Driven Programming                                         |
|   Files to read:                                                            |
|   +-----------------------------------------------------------------+       |
|   | lib/event.c         | Main event loop implementation            |       |
|   | lib/frrevent.h      | Public API                                |       |
|   | bgpd/bgp_main.c     | Example usage in a daemon                 |       |
|   +-----------------------------------------------------------------+       |
|                                                                             |
|   CONCEPT: Route Installation                                               |
|   Files to read:                                                            |
|   +-----------------------------------------------------------------+       |
|   | zebra/rib.c         | RIB management                            |       |
|   | zebra/zebra_rib.c   | RIB operations                            |       |
|   | zebra/rt_netlink.c  | Kernel interaction                        |       |
|   +-----------------------------------------------------------------+       |
|                                                                             |
|   CONCEPT: Protocol-Zebra Communication                                     |
|   Files to read:                                                            |
|   +-----------------------------------------------------------------+       |
|   | lib/zclient.c       | Client library                            |       |
|   | zebra/zserv.c       | Server side                               |       |
|   | bgpd/bgp_zebra.c    | BGP's use of ZAPI                         |       |
|   +-----------------------------------------------------------------+       |
|                                                                             |
|   CONCEPT: BGP Protocol Implementation                                      |
|   Files to read:                                                            |
|   +-----------------------------------------------------------------+       |
|   | bgpd/bgp_fsm.c      | State machine                             |       |
|   | bgpd/bgp_packet.c   | Message parsing                           |       |
|   | bgpd/bgp_route.c    | Route handling                            |       |
|   | bgpd/bgp_attr.c     | Path attributes                           |       |
|   +-----------------------------------------------------------------+       |
|                                                                             |
+-----------------------------------------------------------------------------+
```

---

## 中文详细说明

### 1. 推荐阅读顺序（自顶向下）

#### 阶段 1：了解项目结构

```
目标：理解 FRR 的构建、打包和组织方式

阅读顺序：
1. README.md          - 项目概述和快速入门
2. doc/developer/     - 开发者指南
3. configure.ac       - Autoconf 配置，了解编译选项
4. Makefile.am        - 构建规则

关注点：
+==================================================================+
|  - 支持的平台                                                     |
|  - 编译依赖                                                       |
|  - 守护进程列表                                                   |
|  - 测试框架                                                       |
+==================================================================+
```

#### 阶段 2：共享基础设施（关键！）

```
这是最重要的阶段！所有守护进程都依赖这些模块。

lib/event.c - 事件循环
=======================
重点理解：
- event_master_create() - 创建事件循环
- event_fetch() - 主循环
- event_add_read/write/timer() - 注册事件
- event_cancel() - 取消事件

lib/memory.c - 内存管理
=======================
重点理解：
- MTYPE 定义系统
- XMALLOC/XFREE 宏
- 内存统计和调试

lib/zclient.c - Zebra 客户端
============================
重点理解：
- zclient 结构
- 消息编解码
- 回调注册

lib/prefix.c - 前缀操作
========================
重点理解：
- struct prefix 结构
- 前缀比较和匹配
- 字符串转换

lib/table.c - 路由表
=====================
重点理解：
- Radix tree 实现
- route_node 结构
- 遍历和查找
```

#### 阶段 3：Zebra（核心路由基础设施）

```
Zebra 是 FRR 的心脏，必须深入理解。

zebra/main.c - 入口点
=====================
- 初始化流程
- 事件循环启动

zebra/zserv.c - ZAPI 服务器
============================
- 客户端连接处理
- 消息分发
- 会话管理

zebra/rib.c - RIB 管理
======================
- 路由条目管理
- 最佳路径选择
- 路由更新处理

zebra/rt_netlink.c - Linux 内核交互
===================================
- Netlink 消息构建
- RTM_NEWROUTE/DELROUTE
- 接口管理
```

#### 阶段 4：一个路由协议（深入研究）

```
推荐从 BGP 或 OSPF 开始，不要同时学习多个协议。

bgpd/ 阅读顺序：
================

1. bgp_main.c
   - 守护进程初始化
   - 事件循环设置

2. bgp_fsm.c
   - BGP 状态机
   - 状态转换逻辑
   - 事件处理

3. bgp_packet.c
   - 消息解析
   - OPEN/UPDATE/NOTIFICATION/KEEPALIVE

4. bgp_route.c
   - 路由学习
   - 最佳路径算法
   - 路由表管理

5. bgp_zebra.c
   - 与 Zebra 的交互
   - 路由安装/撤销
```

#### 阶段 5：管理和 CLI

```
理解配置如何转化为运行时状态。

vtysh/vtysh.c
=============
- CLI shell 实现
- 命令路由到守护进程
- 配置文件处理

lib/command.c
=============
- DEFUN/DEFPY 宏
- 命令树结构
- 参数解析
```

---

### 2. 架构关键数据结构

#### 长期存活的路由状态

```c
/* lib/prefix.h - 前缀表示 */
struct prefix {
    uint8_t family;      // AF_INET or AF_INET6
    uint8_t prefixlen;   // 0-32 for IPv4, 0-128 for IPv6
    union {
        struct in_addr prefix4;
        struct in6_addr prefix6;
    } u;
};

/* lib/table.h - 路由表节点 */
struct route_node {
    struct route_node *parent;
    struct route_node *link[2];
    struct prefix p;
    void *info;           // 指向 route_entry 链表
    unsigned int lock;    // 引用计数
};

/* zebra/zebra_rib.c - RIB 条目 */
struct route_entry {
    uint8_t type;         // ZEBRA_ROUTE_BGP, etc.
    uint8_t instance;
    uint8_t distance;
    uint32_t metric;
    struct nexthop_group *nhe;
    time_t uptime;
    uint32_t flags;
};
```

#### 协议 FSM 编码

```c
/* bgpd/bgp_fsm.h - BGP 对等体状态 */
struct peer {
    int state;            // BGP_STATE_IDLE, etc.
    int ostatus;          // 旧状态
    
    /* 事件定时器 */
    struct event *t_start;
    struct event *t_connect;
    struct event *t_holdtime;
    struct event *t_keepalive;
    
    /* 统计 */
    unsigned int established;
    unsigned int dropped;
};
```

#### 守护进程间的胶合结构

```c
/* lib/zclient.h - Zebra 客户端 */
struct zclient {
    int sock;                         // 与 Zebra 的连接
    struct event_loop *master;        // 事件循环
    
    /* 回调函数 */
    void (*zebra_connected)(struct zclient *);
    int (*router_id_update)(ZAPI_CALLBACK_ARGS);
    int (*interface_address_add)(ZAPI_CALLBACK_ARGS);
    int (*redistribute_route_add)(ZAPI_CALLBACK_ARGS);
};
```

---

### 3. 热点路径（逻辑关键，非性能关键）

#### 路由选择逻辑

```
BGP 最佳路径选择 (bgpd/bgp_route.c: bgp_path_info_cmp)
+==================================================================+
|  比较顺序：                                                       |
|  1. 权重 (Weight) - Cisco 私有属性                                |
|  2. 本地优先级 (Local Preference)                                 |
|  3. 本地发起 (Locally Originated)                                 |
|  4. AS 路径长度                                                   |
|  5. Origin 类型 (IGP < EGP < Incomplete)                         |
|  6. MED (Multi-Exit Discriminator)                               |
|  7. eBGP 优于 iBGP                                               |
|  8. IGP 度量                                                     |
|  9. 最早学习                                                     |
|  10. Router ID                                                   |
+==================================================================+
```

#### 协议更新处理

```
BGP UPDATE 处理流程：
+==================================================================+

bgp_read()                    // I/O 事件回调
    |
    v
bgp_process_packet()          // 消息类型分发
    |
    v
bgp_update_receive()          // UPDATE 处理
    |
    +---> bgp_nlri_parse()    // 解析 NLRI
    |         |
    |         v
    |     bgp_update()        // 更新路由表
    |         |
    |         v
    |     bgp_path_info_add() // 添加路径信息
    |
    +---> bgp_attr_parse()    // 解析路径属性
              |
              v
          bgp_process()       // 触发最佳路径选择

+==================================================================+
```

#### Zebra 路由安装路径

```
Zebra 路由安装流程：
+==================================================================+

rib_add_multipath()           // RIB 添加路由
    |
    v
rib_meta_queue_add()          // 入队处理
    |
    v
rib_process()                 // RIB 处理主函数
    |
    v
rib_process_result()          // 处理结果
    |
    +---> rib_uninstall_kernel() // 旧路由删除
    |
    +---> rib_install_kernel()   // 新路由安装
              |
              v
          netlink_route_multipath()
              |
              v
          nl_rtm_build()      // 构建 Netlink 消息
              |
              v
          netlink_talk()      // 发送到内核

+==================================================================+
```

#### 策略评估

```
路由策略评估 (route-map)：
+==================================================================+

route_map_apply()             // 策略应用入口
    |
    v
route_map_apply_match()       // 匹配条件检查
    |                         // (prefix-list, community, etc.)
    +---> RMAP_MATCH
    |
    +---> RMAP_NOMATCH
    |
    +---> RMAP_NOOP
              |
              v
route_map_apply_set()         // 设置动作
                              // (set metric, set community, etc.)

+==================================================================+
```

---

### 4. 在代码中验证 WHY / HOW / WHAT

#### 架构分离的强制执行

```c
/* 示例：bgpd 不直接操作内核 */

/* bgpd/bgp_zebra.c */
void bgp_zebra_announce(struct bgp_node *rn, 
                        struct bgp_path_info *pi,
                        struct bgp *bgp,
                        afi_t afi, safi_t safi)
{
    struct zapi_route api;
    
    /* 构建 ZAPI 消息 */
    memset(&api, 0, sizeof(api));
    api.vrf_id = bgp->vrf_id;
    api.type = ZEBRA_ROUTE_BGP;
    api.prefix = rn->p;
    
    /* 发送到 Zebra - 不直接操作内核！ */
    zclient_route_send(ZEBRA_ROUTE_ADD, zclient, &api);
}
```

#### 无法避免的复杂性

```c
/* 示例：BGP 路径选择的复杂性是协议固有的 */

/* bgpd/bgp_route.c: bgp_path_info_cmp() */
static int bgp_path_info_cmp(...)
{
    /* 必须按 RFC 4271 规定的顺序比较 */
    
    /* 1. Weight (本地概念) */
    if (newattr->weight != existattr->weight)
        return ...
        
    /* 2. Local Preference */
    if (new_pref != exist_pref)
        return ...
        
    /* 3-10. 更多比较... */
    
    /* 这个复杂性是 BGP 协议要求的，无法简化 */
}
```

#### 清晰性优于抽象

```c
/* FRR 风格：直接明确，避免过度抽象 */

/* 好的做法 - FRR 实际代码 */
struct prefix p;
p.family = AF_INET;
p.prefixlen = 24;
p.u.prefix4.s_addr = htonl(0x0a000000);  // 10.0.0.0

/* 不是 FRR 风格 */
Prefix* p = PrefixFactory::create(AddressFamily::IPv4);
p->setLength(24);
p->setAddress(IPAddress("10.0.0.0"));
```

---

### 5. 阅读代码的实用技巧

```
高效阅读 FRR 代码的技巧：
+==================================================================+

1. 使用 ctags/cscope 建立索引
   $ ctags -R .
   $ cscope -bR

2. 从 main() 开始跟踪
   - 找到初始化顺序
   - 理解事件循环设置

3. 跟踪一个消息的完整生命周期
   - 从网络接收
   - 解析处理
   - 状态更新
   - 响应发送

4. 关注 DEFUN/DEFPY 宏
   - 这些定义了 CLI 命令
   - 从命令跟踪到内部处理

5. 利用日志
   - 开启 debug 级别日志
   - 跟踪消息流

6. 阅读测试代码
   - tests/ 目录下的测试展示了 API 用法

+==================================================================+
```
