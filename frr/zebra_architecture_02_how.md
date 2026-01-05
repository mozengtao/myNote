# Zebra Architecture Guide - Part 2: HOW

## Zebra's Design Model

```
+============================================================================+
|                        ZEBRA DESIGN MODEL OVERVIEW                         |
+============================================================================+

                         +------------------------+
                         |    Management Plane    |
                         |  (vtysh, NETCONF, etc) |
                         +------------------------+
                                    |
                                    v
+----------------------------------------------------------------------------+
|                            CONTROL PLANE                                   |
|                                                                            |
|  +--------+   +--------+   +--------+   +--------+   +--------+           |
|  | bgpd   |   | ospfd  |   | isisd  |   | ripd   |   | staticd|           |
|  +--------+   +--------+   +--------+   +--------+   +--------+           |
|       |           |           |           |             |                  |
|       +-----------+-----------+-----------+-------------+                  |
|                               |                                            |
|                               | ZAPI (Zebra API)                          |
|                               | Unix Domain Socket                        |
|                               v                                            |
|  +====================================================================+   |
|  |                          ZEBRA DAEMON                              |   |
|  |                                                                    |   |
|  |  +---------------+  +---------------+  +------------------+       |   |
|  |  | Client Mgmt   |  | RIB Manager   |  | Redistribution   |       |   |
|  |  | (zserv.c)     |  | (zebra_rib.c) |  | Engine           |       |   |
|  |  +---------------+  +---------------+  +------------------+       |   |
|  |         |                  |                    |                  |   |
|  |         v                  v                    v                  |   |
|  |  +----------------------------------------------------------+     |   |
|  |  |                    Meta Queue System                     |     |   |
|  |  |  (Priority-based work queue for route processing)       |     |   |
|  |  +----------------------------------------------------------+     |   |
|  |                            |                                       |   |
|  |                            v                                       |   |
|  |  +----------------------------------------------------------+     |   |
|  |  |                   Dataplane Abstraction                  |     |   |
|  |  |                   (zebra_dplane.c)                       |     |   |
|  |  +----------------------------------------------------------+     |   |
|  +====================================================================+   |
|                               |                                            |
+----------------------------------------------------------------------------+
                                |
                                | Netlink / BSD routing socket
                                v
+----------------------------------------------------------------------------+
|                            DATA PLANE                                      |
|                                                                            |
|                       +------------------------+                           |
|                       |     Linux Kernel       |                           |
|                       |   Forwarding Table     |                           |
|                       |       (FIB)            |                           |
|                       +------------------------+                           |
|                                                                            |
+----------------------------------------------------------------------------+
```

**中文说明：**

上图展示了 Zebra 设计模型的整体架构。从上到下分为三层：管理平面（vtysh、NETCONF 等）、控制平面（包含各协议进程和 Zebra 守护进程）、数据平面（Linux 内核转发表）。协议进程通过 ZAPI（Unix 域套接字）与 Zebra 通信。Zebra 内部包含客户端管理（zserv.c）、RIB 管理器（zebra_rib.c）、重分发引擎，以及优先级工作队列（Meta Queue）和数据平面抽象层（zebra_dplane.c）。最终通过 Netlink 与内核同步。

---

## 1. Zebra as RIB Master and FIB Synchronizer

```
+===========================================================================+
|                     RIB vs FIB Relationship                               |
+===========================================================================+

    RIB (Routing Information Base)     FIB (Forwarding Information Base)
    +-----------------------------+    +-----------------------------+
    |  Location: Zebra userspace  |    |  Location: Linux Kernel     |
    +-----------------------------+    +-----------------------------+
    |  Contents:                  |    |  Contents:                  |
    |  - All protocol routes      |    |  - Only installed best route|
    |  - All route attributes     |    |  - Optimized forwarding info|
    |  - Supports multipath ECMP  |    |  - Used for packet forward  |
    +-----------------------------+    +-----------------------------+
    |  Operations:                |    |  Operations:                |
    |  - Route add/delete/update  |    |  - Table lookup forwarding  |
    |  - Best path selection      |    |  - Hardware acceleration    |
    |  - Nexthop resolution       |    |                             |
    +-----------------------------+    +-----------------------------+
                  |                                ^
                  |      zebra_dplane.c            |
                  +--------------------------------+
                         Synchronization

Sync Process:
1. RIB selects best route
2. Send via Dataplane abstraction
3. Netlink message reaches kernel
4. Kernel updates FIB
5. Kernel returns confirmation
6. Zebra updates route status
```

**中文说明：**

此图说明 RIB 与 FIB 的关系。RIB 位于 Zebra 用户空间进程，包含所有协议提交的路由、所有路由属性，支持多路径（ECMP），负责路由添加/删除/更新、最佳路径选择和下一跳解析。FIB 位于 Linux 内核，仅包含已安装的最佳路由和优化后的转发信息，用于数据包转发，可能有硬件加速。同步过程为：RIB 选出最佳路由 → 通过 Dataplane 抽象层发送 → Netlink 消息到达内核 → 内核更新 FIB → 内核返回确认 → Zebra 更新路由状态。

---

## 2. Client/Server Model

```
+===========================================================================+
|                      ZAPI Client-Server Model                             |
+===========================================================================+

                     +---------------------------+
                     |        Zebra Server       |
                     |                           |
                     |  zsock (Unix Domain Sock) |
                     |          |                |
                     |    accept() listening     |
                     +----------|-----------------+
                                |
         +----------------------+----------------------+
         |                      |                      |
    +----v----+           +----v----+           +----v----+
    | Client  |           | Client  |           | Client  |
    | Thread 1|           | Thread 2|           | Thread 3|
    +----+----+           +----+----+           +----+----+
         |                      |                      |
    +----v----+           +----v----+           +----v----+
    |  bgpd   |           |  ospfd  |           |  isisd  |
    +---------+           +---------+           +---------+


Message Flow:

Client (bgpd)                    Server (Zebra)
     |                                |
     |--- ZEBRA_ROUTE_ADD ----------->|
     |    (route info)                |
     |                                |---> Add to Meta Queue
     |                                |---> Process route
     |                                |---> Update RIB
     |                                |---> Push to Dataplane
     |<-- ZEBRA_ROUTE_NOTIFY ---------|
     |    (install confirmation)      |
     |                                |
```

**中文说明：**

客户端-服务器模型展示了 Zebra 如何与协议进程通信。Zebra 服务器通过 Unix 域套接字监听连接，每个协议进程（bgpd、ospfd、isisd）作为客户端连接到 Zebra。消息流程：客户端发送 ZEBRA_ROUTE_ADD（路由信息）→ 服务器将其加入 Meta Queue → 处理路由 → 更新 RIB → 推送到 Dataplane → 返回 ZEBRA_ROUTE_NOTIFY（安装确认）给客户端。

---

### ZAPI Message Types (Partial)

| Message Type | Direction | Purpose |
|--------------|-----------|---------|
| ZEBRA_ROUTE_ADD | Client→Server | Add route |
| ZEBRA_ROUTE_DELETE | Client→Server | Delete route |
| ZEBRA_REDISTRIBUTE_ADD | Client→Server | Request route redistribution |
| ZEBRA_INTERFACE_ADD | Server→Client | Notify interface added |
| ZEBRA_NEXTHOP_UPDATE | Server→Client | Nexthop state change |

**中文说明：**

ZAPI 消息类型表列出了部分消息类型及其方向和用途。ZEBRA_ROUTE_ADD 和 ZEBRA_ROUTE_DELETE 从客户端发送到服务器，用于添加和删除路由。ZEBRA_REDISTRIBUTE_ADD 请求路由重分发。ZEBRA_INTERFACE_ADD 和 ZEBRA_NEXTHOP_UPDATE 从服务器发送到客户端，分别通知接口添加和下一跳状态变化。

---

## 3. Netlink Abstraction Boundary

```
+===========================================================================+
|                     Kernel Interaction Abstraction Layer                  |
+===========================================================================+

  +---------------------------+
  |     zebra_rib.c           |  <- Route logic, kernel-agnostic
  |  "I want to install route"|
  +-------------+-------------+
                |
                v
  +---------------------------+
  |   zebra_dplane.c          |  <- Dataplane abstraction
  |  "Create context, queue"  |
  +-------------+-------------+
                |
                v
  +---------------------------+
  |   dplane_provider         |  <- Pluggable providers
  |  (kernel / fpm / dpdk)    |
  +-------------+-------------+
                |
      +---------+---------+
      |                   |
      v                   v
  +-------+           +-------+
  | Linux |           | BSD   |
  |Netlink|           |Socket |
  +-------+           +-------+


Dataplane Context (dplane_ctx):
+----------------------------------+
| Op Type: ROUTE_INSTALL/DELETE    |
| Dest Prefix: 10.0.0.0/24         |
| Nexthop Info:                    |
|   - 192.168.1.1 via eth0         |
|   - 192.168.1.2 via eth1 (ECMP)  |
| VRF ID: 0                        |
| Table ID: 254                    |
| Protocol: BGP                    |
| Sequence: 12345                  |
+----------------------------------+
```

**中文说明：**

Netlink 抽象边界展示了内核交互的分层设计。zebra_rib.c 负责路由逻辑，与内核无关。zebra_dplane.c 是数据平面抽象层，创建上下文并排队。dplane_provider 是可插拔的提供者（内核/FPM/DPDK）。最底层是平台特定实现：Linux 使用 Netlink，BSD 使用路由套接字。Dataplane Context 包含操作类型、目的前缀、下一跳信息、VRF ID、表 ID、协议类型和序列号。

---

### Why This Abstraction is Needed

1. **Platform Independence**: Linux uses Netlink, BSD uses routing socket
2. **Async Processing**: Doesn't block RIB processing
3. **Extensibility**: Can add DPDK, FPM and other providers
4. **Batch Processing**: Can merge multiple updates

**中文说明：**

为什么需要这层抽象：（1）平台独立性 - Linux 用 Netlink，BSD 用路由套接字；（2）异步处理 - 不阻塞 RIB 处理；（3）可扩展性 - 可以添加 DPDK、FPM 等提供者；（4）批处理 - 可以合并多个更新。

---

## 4. Failure Recovery and Resync Logic

```
+===========================================================================+
|                     Graceful Restart (GR)                                 |
+===========================================================================+

Scenario: ospfd process crashes and restarts

Timeline:
+------+------------------------------------------------------------+
| T0   | ospfd running normally, 100 OSPF routes in RIB             |
+------+------------------------------------------------------------+
| T1   | ospfd crashes                                               |
|      | Zebra detects connection lost                               |
|      | Mark all OSPF routes as "stale"                             |
+------+------------------------------------------------------------+
| T2   | Start stale route timer (default 300 seconds)              |
|      | OSPF routes in FIB are PRESERVED during this time!         |
+------+------------------------------------------------------------+
| T3   | ospfd restarts and connects to Zebra                        |
|      | ospfd sends GR capability                                   |
+------+------------------------------------------------------------+
| T4   | ospfd re-learns OSPF routes                                 |
|      | Sends ZEBRA_ROUTE_ADD to Zebra                              |
|      | Zebra clears "stale" mark for matching routes               |
+------+------------------------------------------------------------+
| T5   | ospfd sends GR_END (sync complete)                          |
|      | Zebra deletes all routes still marked as "stale"            |
+------+------------------------------------------------------------+


Route State Machine:
                    +--------+
                    | ACTIVE |<------------------+
                    +--------+                   |
                        |                        |
          ospfd disconnects               ospfd re-adds
                        |                        |
                        v                        |
                    +--------+                   |
                    | STALE  |-------------------+
                    +--------+
                        |
               Timer expires or
             Still stale after GR_END
                        |
                        v
                    +--------+
                    | REMOVED|
                    +--------+
```

**中文说明：**

优雅重启（GR）流程展示了 ospfd 进程崩溃后重启的处理过程。时间线：T0 - ospfd 正常运行，RIB 中有 100 条 OSPF 路由；T1 - ospfd 崩溃，Zebra 检测到连接断开，标记所有 OSPF 路由为 "stale"（过期）；T2 - 启动过期路由定时器（默认 300 秒），此时 FIB 中的 OSPF 路由仍然保留；T3 - ospfd 重启并连接 Zebra，发送 GR 能力；T4 - ospfd 重新学习路由，发送 ZEBRA_ROUTE_ADD，Zebra 清除匹配路由的 "stale" 标记；T5 - ospfd 发送 GR_END（同步完成），Zebra 删除所有仍标记为 "stale" 的路由。路由状态机：ACTIVE → (ospfd 断开) → STALE → (ospfd 重新添加) → ACTIVE 或 (定时器超时/GR_END 后仍为 stale) → REMOVED。

---

## 5. Meta Queue System

```
+===========================================================================+
|                     Priority Work Queue                                   |
+===========================================================================+

Meta Queue Design Goals:
- Process different route types by priority
- Prevent low-priority work from starving high-priority work
- Batch processing for efficiency

Queue Priority (high to low):
+-----+----------------------+--------------------------------+
| Idx | Queue Name           | Processing Content             |
+-----+----------------------+--------------------------------+
| 0   | META_QUEUE_NHG       | Nexthop Groups                 |
| 1   | META_QUEUE_EVPN      | EVPN/VxLAN objects             |
| 2   | META_QUEUE_EARLY_ROUTE| Early route processing        |
| 3   | META_QUEUE_EARLY_LABEL| Early label handling          |
| 4   | META_QUEUE_CONNECTED | Connected routes               |
| 5   | META_QUEUE_KERNEL    | Kernel routes                  |
| 6   | META_QUEUE_STATIC    | Static routes                  |
| 7   | META_QUEUE_NOTBGP    | RIP/OSPF/ISIS/EIGRP routes     |
| 8   | META_QUEUE_BGP       | BGP routes                     |
| 9   | META_QUEUE_OTHER     | Other routes                   |
| 10  | META_QUEUE_GR_RUN    | Graceful restart processing    |
+-----+----------------------+--------------------------------+


Processing Flow:
+------------------+
| route_add request|
+--------+---------+
         |
         v
+------------------+
| Determine queue  |
| priority (by     |
| route type)      |
+--------+---------+
         |
         v
+------------------+
| Enqueue to       |
| corresponding MQ |
+--------+---------+
         |
         v
+------------------+
| Worker thread    |
| processes queues |
| in priority order|
+------------------+
```

**中文说明：**

Meta Queue 系统是优先级工作队列，设计目的是按优先级处理不同类型的路由更新、防止低优先级工作饿死高优先级工作、批量处理提高效率。队列优先级从高到低：NHG（下一跳组）→ EVPN（EVPN/VxLAN 对象）→ 早期路由处理 → 早期标签处理 → 直连路由 → 内核路由 → 静态路由 → RIP/OSPF/ISIS/EIGRP 路由 → BGP 路由 → 其他路由 → 优雅重启处理。处理流程：route_add 请求 → 确定队列优先级（根据路由类型）→ 入队对应 MQ → 工作线程按优先级顺序处理各队列。

---

## 6. Thread Model

```
+===========================================================================+
|                     Zebra Thread Architecture                             |
+===========================================================================+

+-----------------------------------------------------------------------+
|                           Zebra Process                               |
|                                                                       |
|  +-------------------+   +-------------------+   +------------------+ |
|  |    Main Thread    |   |  Dataplane Thread |   | Client Threads   | |
|  +-------------------+   +-------------------+   +------------------+ |
|  |                   |   |                   |   | (per client)     | |
|  | - CLI handling    |   | - Kernel msg send |   | - Read client msg| |
|  | - Config mgmt     |   | - Netlink receive |   | - Send replies   | |
|  | - RIB processing  |   | - FPM comms       |   |                  | |
|  | - Route decision  |   | - Result callback |   |                  | |
|  |                   |   |                   |   |                  | |
|  +-------------------+   +-------------------+   +------------------+ |
|           |                       |                       |           |
|           |      +----------------+                       |           |
|           |      |                                        |           |
|           v      v                                        v           |
|  +-------------------------------------------------------------------+|
|  |                    Event Loop (libevent/frrevent)                 ||
|  +-------------------------------------------------------------------+|
|                                                                       |
+-----------------------------------------------------------------------+


Inter-thread Communication:
+-------------+                              +------------------+
| Main Thread |                              | Dataplane Thread |
+------+------+                              +---------+--------+
       |                                               |
       |  dplane_ctx enqueue                          |
       +--------------------------------------------->|
       |                                               |
       |                                               | Kernel ops
       |                                               |
       |  result callback                              |
       |<----------------------------------------------+
       |                                               |
```

**中文说明：**

Zebra 线程架构包含三类线程：主线程（负责 CLI 处理、配置管理、RIB 处理、路由决策）、Dataplane 线程（负责内核消息发送、Netlink 接收、FPM 通信、结果回调）、客户端线程（每个客户端一个，负责读取客户端消息、发送回复）。所有线程通过事件循环（libevent/frrevent）协调。线程间通信：主线程通过 dplane_ctx enqueue 将任务发送给 Dataplane 线程，Dataplane 线程执行内核操作后通过 result callback 返回结果给主线程。

---

## Key Design Patterns

| Pattern | Implementation | Purpose |
|---------|----------------|---------|
| **Client-Server** | zserv.c | Decouple protocol processes from route management |
| **Work Queue** | Meta Queue | Priority processing and batch operations |
| **Provider Pattern** | Dataplane providers | Support multiple kernel interfaces |
| **Event-Driven** | frrevent | Efficient I/O handling |
| **State Machine** | Route states | Track route lifecycle |

**中文说明：**

关键设计模式包括：客户端-服务器模式（zserv.c 实现，解耦协议进程和路由管理）、工作队列模式（Meta Queue 实现，优先级处理和批量操作）、提供者模式（Dataplane providers 实现，支持多种内核接口）、事件驱动模式（frrevent 实现，高效 I/O 处理）、状态机模式（Route states 实现，跟踪路由生命周期）。
