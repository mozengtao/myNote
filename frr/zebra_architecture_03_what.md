# Zebra Architecture Guide - Part 3: WHAT

## Zebra's Responsibilities

```
+============================================================================+
|                    ZEBRA RESPONSIBILITY MATRIX                             |
+============================================================================+

    ZEBRA OWNS                              ZEBRA DOES NOT OWN
    +---------------------------+           +---------------------------+
    | * RIB management          |           | * Protocol state machines |
    | * Best path selection     |           | * Neighbor discovery      |
    | * Nexthop resolution      |           | * Route calculation algo  |
    | * VRF/table management    |           | * Protocol message parsing|
    | * Kernel FIB sync         |           | * Session management      |
    | * Interface state mgmt    |           | * Authentication handling |
    | * Route redistribution    |           | * Protocol-level timers   |
    | * Policy filtering        |           | * Topology database       |
    | * MPLS label management   |           | * SPF calculation         |
    | * Graceful restart coord  |           | * BGP decision process    |
    +---------------------------+           +---------------------------+

Clear Boundary:
+------------------------------------------------------------------------+
|  Protocol Process (bgpd/ospfd/...)  |  Zebra                           |
+-------------------------------------+----------------------------------+
|  "Here is route to 10.0.0.0/24      |  "I will decide if this route   |
|   via 192.168.1.1,                  |   is better than existing,      |
|   please install it"                |   resolve nexthop, then install |
|                                     |   to kernel"                    |
+-------------------------------------+----------------------------------+
```

**中文说明：**

Zebra 职责矩阵清晰划分了 Zebra 的职责边界。Zebra 负责：RIB 管理和维护、最佳路径选择、下一跳解析、VRF/表管理、内核 FIB 同步、接口状态管理、路由重分发、策略过滤、MPLS 标签管理、优雅重启协调。Zebra 不负责：协议状态机、邻居发现、路由计算算法、协议消息解析、会话管理、认证处理、协议级定时器、拓扑数据库、SPF 计算、BGP 决策过程。边界明确：协议进程说"这是到 10.0.0.0/24 via 192.168.1.1 的路由，请帮我安装"，Zebra 说"我来决定这条路由是否比现有的更好，解析下一跳，然后安装到内核"。

---

## 1. RIB Data Structures

```
+===========================================================================+
|                     RIB Core Data Structures                              |
+===========================================================================+

struct route_entry (Route Entry):
+------------------------------------------+
| next/prev       | Linked list pointers   |
| nhe             | Nexthop hash entry ptr |
| nhe_id          | Nexthop group ID       |
| type            | Route type (BGP/OSPF)  |
| vrf_id          | VRF identifier         |
| table           | Routing table ID       |
| metric          | Metric value           |
| distance        | Administrative distance|
| flags           | Route flags            |
| status          | Internal status        |
|   - REMOVED     | Marked for deletion    |
|   - CHANGED     | Modified               |
|   - QUEUED      | Queued for processing  |
|   - INSTALLED   | Installed in kernel    |
|   - FAILED      | Installation failed    |
| uptime          | Route creation time    |
| tag             | Route tag              |
+------------------------------------------+


RIB Table Structure:
+------------------------------------------------------------------+
|                        zebra_vrf                                 |
|  +------------------------------------------------------------+  |
|  |  table[AFI_IP][SAFI_UNICAST]    (IPv4 unicast table)       |  |
|  |  table[AFI_IP][SAFI_MULTICAST]  (IPv4 multicast table)     |  |
|  |  table[AFI_IP6][SAFI_UNICAST]   (IPv6 unicast table)       |  |
|  |  table[AFI_IP6][SAFI_MULTICAST] (IPv6 multicast table)     |  |
|  +------------------------------------------------------------+  |
|                               |                                  |
|                               v                                  |
|  +------------------------------------------------------------+  |
|  |                     route_table                            |  |
|  |  (Patricia/Radix Trie structure)                          |  |
|  |                                                            |  |
|  |       [10.0.0.0/8]                                        |  |
|  |          /        \                                        |  |
|  |   [10.1.0.0/16]  [10.2.0.0/16]                            |  |
|  |        |              |                                    |  |
|  |  [10.1.1.0/24]  [10.2.1.0/24]                             |  |
|  |                                                            |  |
|  +------------------------------------------------------------+  |
|                               |                                  |
|                               v                                  |
|  +------------------------------------------------------------+  |
|  |                     route_node                             |  |
|  |  +------------------------------------------------------+  |  |
|  |  | prefix: 10.1.1.0/24                                  |  |  |
|  |  | info: -> rib_dest_t                                  |  |  |
|  |  +------------------------------------------------------+  |  |
|  +------------------------------------------------------------+  |
|                               |                                  |
|                               v                                  |
|  +------------------------------------------------------------+  |
|  |                     rib_dest_t                             |  |
|  |  +------------------------------------------------------+  |  |
|  |  | routes: linked list (all protocol routes)            |  |  |
|  |  |   -> route_entry (BGP, distance=20)                  |  |  |
|  |  |   -> route_entry (OSPF, distance=110)                |  |  |
|  |  |   -> route_entry (Static, distance=1)                |  |  |
|  |  | selected_fib: currently selected best route          |  |  |
|  |  +------------------------------------------------------+  |  |
|  +------------------------------------------------------------+  |
+------------------------------------------------------------------+
```

**中文说明：**

RIB 核心数据结构包含多个层次。route_entry（路由条目）包含链表指针、下一跳组哈希条目指针、下一跳组 ID、路由类型、VRF 标识符、路由表 ID、度量值、管理距离、路由标志、内部状态（REMOVED/CHANGED/QUEUED/INSTALLED/FAILED）、路由创建时间和路由标签。zebra_vrf 包含按地址族和子地址族组织的路由表（IPv4/IPv6 单播/组播）。route_table 使用 Patricia/Radix Trie 结构组织。route_node 包含前缀和指向 rib_dest_t 的指针。rib_dest_t 包含所有协议的路由链表（如 BGP distance=20、OSPF distance=110、Static distance=1）和当前选中的最佳路由。

---

## 2. Nexthop Resolution

```
+===========================================================================+
|                     Recursive Nexthop Resolution Process                  |
+===========================================================================+

Scenario: BGP learns route 10.0.0.0/8 via 192.168.100.1

Step 1: Check if 192.168.100.1 is directly connected
+---------------------------+
| 192.168.100.1 connected?  |
| Check interface: not found|
| Result: recursive needed  |
+---------------------------+

Step 2: Recursive lookup for 192.168.100.1 route
+---------------------------+
| Lookup 192.168.100.1/32   |
| Found: 192.168.100.0/24   |
|        via 10.1.1.1       |
|        (learned by OSPF)  |
+---------------------------+

Step 3: Recursive resolve 10.1.1.1
+---------------------------+
| Lookup 10.1.1.1           |
| Found: 10.1.1.0/24        |
|        directly connected |
|        via eth0           |
| Resolution complete!      |
+---------------------------+

Final Result:
+----------------------------------------+
| Original: 10.0.0.0/8 via 192.168.100.1 |
| Resolved: 10.0.0.0/8 via eth0          |
|           (egress interface resolved)  |
+----------------------------------------+


NHT (Nexthop Tracking) Mechanism:
+------------------------------------------------------------------+
|                                                                  |
|  When BGP registers tracking for 192.168.100.1:                  |
|                                                                  |
|  +------------------+                                            |
|  | rnh (nexthop)    |                                            |
|  | - 192.168.100.1  |                                            |
|  | - client_list:   |---> [bgpd] [other clients...]             |
|  | - state: resolved|                                            |
|  +------------------+                                            |
|                                                                  |
|  When OSPF withdraws 192.168.100.0/24:                           |
|  1. Zebra re-resolves 192.168.100.1                              |
|  2. Discovers resolution fails                                    |
|  3. Notifies all registered clients (ZEBRA_NEXTHOP_UPDATE)       |
|  4. BGP knows nexthop unreachable, withdraws related routes      |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**

递归下一跳解析过程示例：BGP 学到路由 10.0.0.0/8 via 192.168.100.1。步骤 1：检查 192.168.100.1 是否直连，查找接口未找到，需要递归解析。步骤 2：递归查找 192.168.100.1 的路由，找到 192.168.100.0/24 via 10.1.1.1（OSPF 学到）。步骤 3：再次递归解析 10.1.1.1，找到 10.1.1.0/24 直连 eth0，解析完成。最终结果：原始路由 10.0.0.0/8 via 192.168.100.1 解析为 10.0.0.0/8 via eth0（出接口已确定）。

NHT（下一跳跟踪）机制：当 BGP 注册跟踪 192.168.100.1 时，创建 rnh 结构包含下一跳地址、客户端列表和状态。当 OSPF 撤销 192.168.100.0/24 时：Zebra 重新解析 192.168.100.1 → 发现无法解析 → 通知所有注册的客户端（ZEBRA_NEXTHOP_UPDATE）→ BGP 知道下一跳不可达，撤销相关路由。

---

## 3. VRF-Aware Routing

```
+===========================================================================+
|                     VRF (Virtual Routing and Forwarding)                  |
+===========================================================================+

Linux VRF Implementation:
+------------------------------------------------------------------+
|                       Physical Topology                          |
|                                                                  |
|   eth0 ----+---- VRF "red"    ----+---- 192.168.1.0/24          |
|            |                       |                              |
|   eth1 ----+---- VRF "blue"   ----+---- 192.168.1.0/24          |
|            |                       |    (can overlap!)           |
|   eth2 ----+---- VRF "default"----+---- 10.0.0.0/8              |
|                                                                  |
+------------------------------------------------------------------+


Zebra VRF Management:
+------------------------------------------------------------------+
|                                                                  |
|  struct zebra_vrf {                                              |
|      vrf_id_t vrf_id;           // Kernel VRF ID                 |
|      char *name;                 // VRF name                     |
|                                                                  |
|      // Separate routing table per VRF                           |
|      struct route_table *table[AFI_MAX][SAFI_MAX];              |
|                                                                  |
|      // Separate RNH table per VRF                               |
|      struct route_table *rnh_table[AFI_MAX];                    |
|                                                                  |
|      // Router ID (can differ per VRF)                           |
|      struct in_addr rid_user_assigned;                          |
|  }                                                               |
|                                                                  |
+------------------------------------------------------------------+


VRF-Aware Route Installation:
+------------------------------------------------------------------+
|                                                                  |
|  Protocol Process Request:                                       |
|  +----------------------------------------------------------+   |
|  | ZEBRA_ROUTE_ADD                                          |   |
|  | vrf_id = 10  (VRF "red")                                 |   |
|  | prefix = 10.0.0.0/24                                     |   |
|  | nexthop = 192.168.1.1                                    |   |
|  +----------------------------------------------------------+   |
|                              |                                   |
|                              v                                   |
|  Zebra Processing:                                               |
|  +----------------------------------------------------------+   |
|  | 1. Find zebra_vrf for VRF ID=10                          |   |
|  | 2. Lookup/add 10.0.0.0/24 in that VRF's route table      |   |
|  | 3. Resolve nexthop (within same VRF!)                    |   |
|  | 4. Install to kernel with VRF info                       |   |
|  +----------------------------------------------------------+   |
|                              |                                   |
|                              v                                   |
|  Netlink Message:                                                |
|  +----------------------------------------------------------+   |
|  | RTM_NEWROUTE                                             |   |
|  | table = 10  (kernel table ID for VRF)                    |   |
|  | dst = 10.0.0.0/24                                        |   |
|  | gateway = 192.168.1.1                                    |   |
|  +----------------------------------------------------------+   |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**

VRF（虚拟路由和转发）实现展示了 Linux VRF 架构。物理拓扑中，eth0 属于 VRF "red"（192.168.1.0/24），eth1 属于 VRF "blue"（192.168.1.0/24，可以与 red 重叠），eth2 属于 VRF "default"（10.0.0.0/8）。

Zebra VRF 管理：zebra_vrf 结构包含内核 VRF ID、VRF 名称、每个 VRF 独立的路由表、每个 VRF 独立的 RNH 表、路由器 ID（可以每个 VRF 不同）。

VRF 感知的路由安装：协议进程请求（ZEBRA_ROUTE_ADD，vrf_id=10，prefix=10.0.0.0/24，nexthop=192.168.1.1）→ Zebra 处理（查找 VRF ID=10 对应的 zebra_vrf，在该 VRF 的路由表中查找/添加，在同一 VRF 内解析下一跳，安装到内核时带上 VRF 信息）→ Netlink 消息（RTM_NEWROUTE，table=10，dst=10.0.0.0/24，gateway=192.168.1.1）。

---

## 4. Kernel Synchronization Mechanism

```
+===========================================================================+
|                     Dataplane Sync Flow                                   |
+===========================================================================+

Route Update Flow:

Step 1: RIB Decision
+------------------+
| rib_process()    |
| Select best route|
+--------+---------+
         |
         v
Step 2: Create Dataplane Context
+------------------+
| dplane_route_add |
| Encapsulate info |
+--------+---------+
         |
         v
Step 3: Enqueue to Dataplane
+------------------+
| dplane_update_   |
| enqueue()        |
+--------+---------+
         |
         v
Step 4: Dataplane Thread Processes
+------------------+
| kernel_route_    |
| update()         |
+--------+---------+
         |
         v
Step 5: Netlink Send
+------------------+
| netlink_route()  |
| RTM_NEWROUTE     |
+--------+---------+
         |
         v
Step 6: Wait for Kernel ACK
+------------------+
| netlink_talk()   |
| Receive ACK/NACK |
+--------+---------+
         |
         v
Step 7: Callback to Main Thread
+------------------+
| rib_process_     |
| result()         |
| Update status    |
+------------------+


Failure Handling:
+------------------------------------------------------------------+
|                                                                  |
|  Kernel returns error (e.g., ENOBUFS):                           |
|                                                                  |
|  +----------------------------------------------------------+   |
|  | dplane_ctx status = ZEBRA_DPLANE_REQUEST_FAILURE         |   |
|  +----------------------------------------------------------+   |
|                              |                                   |
|                              v                                   |
|  +----------------------------------------------------------+   |
|  | route_entry status |= ROUTE_ENTRY_FAILED                 |   |
|  +----------------------------------------------------------+   |
|                              |                                   |
|                              v                                   |
|  +----------------------------------------------------------+   |
|  | Log error                                                |   |
|  | May trigger retry logic                                  |   |
|  +----------------------------------------------------------+   |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**

Dataplane 同步流程展示了路由更新的完整流程。步骤 1：RIB 决策 - rib_process() 选择最佳路由。步骤 2：创建 Dataplane Context - dplane_route_add 封装路由信息。步骤 3：入队 Dataplane - dplane_update_enqueue()。步骤 4：Dataplane 线程处理 - kernel_route_update()。步骤 5：Netlink 发送 - netlink_route() 发送 RTM_NEWROUTE。步骤 6：等待内核确认 - netlink_talk() 接收 ACK/NACK。步骤 7：回调主线程 - rib_process_result() 更新路由状态。

失败处理：内核返回错误（如 ENOBUFS）时，dplane_ctx 状态设为 ZEBRA_DPLANE_REQUEST_FAILURE，route_entry 状态设为 ROUTE_ENTRY_FAILED，记录错误日志，可能触发重试逻辑。

---

## 5. Client Registration and Permissions

```
+===========================================================================+
|                     ZAPI Client Management                                |
+===========================================================================+

Client Connection Flow:
+------------------------------------------------------------------+
|                                                                  |
|  bgpd starts                                                     |
|     |                                                            |
|     v                                                            |
|  zclient_start()                                                 |
|     |                                                            |
|     +---> connect to /var/run/frr/zserv.api                     |
|     |                                                            |
|     v                                                            |
|  Send ZEBRA_HELLO                                                |
|  +----------------------------------------------------------+   |
|  | proto = ZEBRA_ROUTE_BGP                                  |   |
|  | instance = 0                                             |   |
|  | session_id = 12345                                       |   |
|  | capabilities = GR_CAPABLE | ROUTE_NOTIFY                 |   |
|  +----------------------------------------------------------+   |
|                              |                                   |
|                              v                                   |
|  Zebra handles (zserv_client_create):                            |
|  +----------------------------------------------------------+   |
|  | 1. Create zserv struct                                   |   |
|  | 2. Initialize client thread                              |   |
|  | 3. Set read/write events                                 |   |
|  | 4. Add to client_list                                    |   |
|  | 5. Send VRF/interface/route initial sync                 |   |
|  +----------------------------------------------------------+   |
|                                                                  |
+------------------------------------------------------------------+


zserv Client Structure:
+------------------------------------------------------------------+
|  struct zserv {                                                  |
|      int sock;                     // Socket FD                  |
|      uint8_t proto;                // Protocol type (BGP/OSPF)   |
|      uint16_t instance;            // Protocol instance          |
|      uint32_t session_id;          // Session ID                 |
|                                                                  |
|      // Buffers                                                  |
|      struct stream_fifo *ibuf_fifo;  // Input queue              |
|      struct stream_fifo *obuf_fifo;  // Output queue             |
|                                                                  |
|      // Redistribution bitmap                                    |
|      vrf_bitmap_t redist[AFI_MAX][ZEBRA_ROUTE_MAX];             |
|                                                                  |
|      // Graceful restart info                                    |
|      struct client_gr_info gr_info_queue;                       |
|                                                                  |
|      // Statistics counters                                      |
|      uint32_t v4_route_add_cnt;                                  |
|      uint32_t v4_route_del_cnt;                                  |
|      ...                                                         |
|  }                                                               |
+------------------------------------------------------------------+


Client Capabilities:
+------------------------------------------------------------------+
|  Capability           | Description                              |
+------------------------------------------------------------------+
|  GR_CAPABLE           | Supports graceful restart                |
|  GRACEFUL_RESTART     | Currently in GR mode                     |
|  ROUTE_NOTIFY         | Wants route installation confirmation    |
|  LABEL_MANAGER        | Can manage MPLS labels                   |
|  OPAQUE_DATA          | Can send/receive opaque data             |
+------------------------------------------------------------------+
```

**中文说明：**

ZAPI 客户端管理展示了客户端连接流程。bgpd 启动 → zclient_start() → 连接到 /var/run/frr/zserv.api → 发送 ZEBRA_HELLO（包含 proto=ZEBRA_ROUTE_BGP、instance=0、session_id=12345、capabilities=GR_CAPABLE|ROUTE_NOTIFY）→ Zebra 处理（创建 zserv 结构体、初始化客户端线程、设置读/写事件、加入 client_list、发送 VRF/接口/路由初始同步）。

zserv 客户端结构包含：套接字 FD、协议类型、协议实例、会话 ID、输入/输出队列、重分发位图、优雅重启信息、统计计数器。

客户端能力包括：GR_CAPABLE（支持优雅重启）、GRACEFUL_RESTART（当前处于 GR 模式）、ROUTE_NOTIFY（希望收到路由安装确认）、LABEL_MANAGER（可以管理 MPLS 标签）、OPAQUE_DATA（可以发送/接收不透明数据）。

---

## 6. Zebra Responsibility Boundary Summary

```
+===========================================================================+
|                     Responsibility Boundary                               |
+===========================================================================+

What Zebra Does:
+------------------------------------------------------------------+
| * Receive routes submitted by protocol processes                 |
| * Select best route by administrative distance                   |
| * Recursively resolve nexthop to connected interface             |
| * Manage separate routing tables for multiple VRFs               |
| * Sync FIB with kernel via Netlink                               |
| * Notify clients of nexthop state changes                        |
| * Control route redistribution (OSPF -> BGP, etc.)               |
| * Apply routing policy (route-map)                               |
| * Manage MPLS labels and LSPs                                    |
| * Coordinate graceful restart process                            |
+------------------------------------------------------------------+

What Zebra Does NOT Do:
+------------------------------------------------------------------+
| * Run any routing protocol                                       |
| * Manage protocol neighbor relationships                         |
| * Calculate shortest path (SPF)                                  |
| * Perform BGP path selection                                     |
| * Parse protocol packets                                         |
| * Maintain protocol-specific databases (LSDB, Adj-RIB-In, etc.) |
| * Handle protocol-level timers                                   |
| * Directly forward packets                                       |
+------------------------------------------------------------------+

Key Design Decisions:
+------------------------------------------------------------------+
| 1. Zebra is PASSIVE - responds to protocol requests, doesn't    |
|    actively generate routes                                      |
|                                                                  |
| 2. Zebra is STATELESS (protocol-wise) - doesn't understand      |
|    BGP/OSPF protocol details                                     |
|                                                                  |
| 3. Zebra is RELIABLE - guarantees RIB and FIB eventual          |
|    consistency                                                   |
|                                                                  |
| 4. Zebra is FAIR - treats all protocols by unified rules        |
+------------------------------------------------------------------+
```

**中文说明：**

职责边界总结。Zebra 做什么：接收协议进程提交的路由、根据管理距离选择最佳路由、递归解析下一跳到直连接口、管理多个 VRF 的独立路由表、通过 Netlink 与内核同步 FIB、通知客户端下一跳状态变化、控制路由重分发、应用路由策略、管理 MPLS 标签和 LSP、协调优雅重启过程。

Zebra 不做什么：运行任何路由协议、管理协议邻居关系、计算最短路径（SPF）、进行 BGP 路径选择、解析协议数据包、维护协议特定的数据库、处理协议级别的定时器、直接进行数据包转发。

关键设计决策：(1) Zebra 是被动的 - 响应协议请求，不主动生成路由；(2) Zebra 是无状态的（协议层面）- 不理解 BGP/OSPF 协议细节；(3) Zebra 是可靠的 - 保证 RIB 和 FIB 最终一致；(4) Zebra 是公平的 - 按统一规则对待所有协议。
