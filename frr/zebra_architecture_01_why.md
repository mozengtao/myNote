# Zebra Architecture Guide - Part 1: WHY

## Why Zebra Exists

```
+================================================================+
|                    THE ROUTING CHAOS PROBLEM                    |
+================================================================+

Without Zebra (Anti-Pattern):
+------------------------------------------+
|               Linux Kernel               |
|            Routing Table (FIB)           |
+------------------------------------------+
     ^         ^         ^         ^
     |         |         |         |
  BGP writes  OSPF writes IS-IS writes  Static writes
     |         |         |         |
+------+  +------+  +------+  +------+
| bgpd |  | ospfd|  | isisd|  |staticd|
+------+  +------+  +------+  +------+

  PROBLEM: Race conditions, conflicting routes,
           no single source of truth!

================================================================

With Zebra (Correct Architecture):
+------------------------------------------+
|               Linux Kernel               |
|            Routing Table (FIB)           |
+------------------------------------------+
                    ^
                    | Single Writer
                    | (Netlink/socket)
                    |
+==========================================+
|                 ZEBRA                    |
|   +----------------------------------+   |
|   |     RIB (Routing Info Base)      |   |
|   |  - Best path selection           |   |
|   |  - Nexthop resolution            |   |
|   |  - VRF management                |   |
|   +----------------------------------+   |
+==========================================+
     ^         ^         ^         ^
     |         |         |         |
  ZAPI     ZAPI      ZAPI      ZAPI
     |         |         |         |
+------+  +------+  +------+  +------+
| bgpd |  | ospfd|  | isisd|  |staticd|
+------+  +------+  +------+  +------+

  SOLUTION: Single authority, consistent state,
            proper arbitration!
```

**中文说明：**

上图展示了两种架构对比。第一种是反模式：多个协议进程（bgpd、ospfd、isisd、staticd）直接向内核路由表写入，导致竞态条件、路由冲突、没有单一真相来源。第二种是正确架构：所有协议通过 ZAPI 与 Zebra 通信，Zebra 作为唯一写者管理 RIB（路由信息库），负责最佳路径选择、下一跳解析和 VRF 管理，然后通过 Netlink 同步到内核 FIB。

---

## 1. Why Routing Protocols Cannot Directly Operate Kernel

### Problem 1: Race Condition

```
Timeline Example:

Time T1: BGP discovers best path 10.0.0.0/24 via 192.168.1.1
Time T2: OSPF discovers better path 10.0.0.0/24 via 192.168.1.2
Time T3: BGP writes to kernel (192.168.1.1)
Time T4: OSPF writes to kernel (192.168.1.2)
Time T5: BGP receives update, writes again (192.168.1.1)

Result: Kernel routing table state is unpredictable!
```

**中文说明：**

竞态条件示例展示了多协议同时操作内核的问题。BGP 和 OSPF 各自独立发现路由，按不同时序写入内核，导致路由表状态不可预测。根本原因是：每个协议进程独立运行、没有全局锁或协调机制、内核不理解路由协议优先级。

---

### Problem 2: Lack of Unified Best Path Selection

```
Different Protocols' Best Path Criteria:

+----------+----------------------------------+
| Protocol | Best Path Selection Criteria     |
+----------+----------------------------------+
| BGP      | AS-PATH length + Local Pref      |
| OSPF     | Link Cost                        |
| IS-IS    | Metric                           |
| RIP      | Hop Count                        |
+----------+----------------------------------+

Problem: When BGP and OSPF both have routes to the same
         destination, who decides which should be installed?
```

**中文说明：**

不同协议使用不同的最佳路径判断标准。BGP 使用 AS-PATH 长度和本地优先级，OSPF 使用链路成本，IS-IS 使用度量值，RIP 使用跳数。问题在于：当多个协议同时有到达同一目的地的路由时，谁来仲裁哪条路由应该被安装到内核？

---

### Problem 3: Cannot Implement Graceful Restart

**中文说明：**

无法实现优雅重启是另一个关键问题：协议进程崩溃时没有机制保护已安装的路由；重启后无法知道之前安装了哪些路由；可能导致路由黑洞或环路。

---

## 2. Why Single RIB Authority is Mandatory

```
+----------------------------------------------------------+
|                    RIB Core Responsibilities              |
+----------------------------------------------------------+
|                                                          |
|  1. Route Arbitration                                    |
|     - Select best route by Administrative Distance (AD)  |
|     - Lower AD = Higher priority                         |
|                                                          |
|     +------------------+-----------+                     |
|     | Route Type       | Default AD|                     |
|     +------------------+-----------+                     |
|     | Connected        | 0         |                     |
|     | Static           | 1         |                     |
|     | eBGP             | 20        |                     |
|     | OSPF             | 110       |                     |
|     | IS-IS            | 115       |                     |
|     | RIP              | 120       |                     |
|     | iBGP             | 200       |                     |
|     +------------------+-----------+                     |
|                                                          |
|  2. Nexthop Resolution                                   |
|     - Recursively resolve nexthop to connected interface |
|     - Track nexthop reachability changes                 |
|                                                          |
|  3. Route Redistribution                                 |
|     - Control which routes can be learned by which proto |
|                                                          |
|  4. Policy Filtering                                     |
|     - Apply route-maps                                   |
|     - Apply access control lists (ACL)                   |
|                                                          |
+----------------------------------------------------------+
```

**中文说明：**

RIB 的核心职责包括四个方面：
1. **路由仲裁** - 根据管理距离（AD）选择最佳路由，AD 值越小优先级越高
2. **下一跳解析** - 递归解析下一跳到直连接口，跟踪下一跳可达性变化
3. **路由重分发** - 控制哪些路由可以被哪些协议学习
4. **策略过滤** - 应用路由映射（Route-map）和访问控制列表（ACL）

---

## 3. Consequences of Multiple Writers

### Consequence 1: Route Flapping

```
Kernel routing table state keeps switching:

T1: 10.0.0.0/24 -> 192.168.1.1 (BGP)
T2: 10.0.0.0/24 -> 192.168.1.2 (OSPF)
T3: 10.0.0.0/24 -> 192.168.1.1 (BGP)
T4: 10.0.0.0/24 -> 192.168.1.2 (OSPF)
...

Impact:
- Packet loss
- TCP connection interruption
- Real-time traffic quality degradation
```

**中文说明：**

路由抖动是多写者问题的第一个后果。内核路由表状态来回切换，导致数据包丢失、TCP 连接中断、实时流量质量下降。

---

### Consequence 2: Route Leakage

```
Scenario: VRF A and VRF B should be isolated

Without Zebra:
+--------+          +--------+
| VRF A  |  LEAK!   | VRF B  |
| Routes | <------> | Routes |
+--------+          +--------+

Problem: Protocol processes may incorrectly install
         routes into the wrong VRF
```

**中文说明：**

路由泄露是第二个后果。VRF A 和 VRF B 应该隔离，但没有 Zebra 时，协议进程可能错误地将路由安装到错误的 VRF。

---

### Consequence 3: Cannot Achieve Consistent Snapshot

```
Problem: When NMS queries routing table

Query at T1:
  10.0.0.0/24 -> 192.168.1.1

Query at T2 (1 second later):
  10.0.0.0/24 -> 192.168.1.2

Reason: Protocol process modified routing table during query
```

**中文说明：**

无法实现一致性快照是第三个后果。网络管理系统查询路由表时，由于协议进程在查询期间修改了路由表，导致两次查询结果不同。

---

## 4. Zebra as the Solution

```
+============================================================+
|                    ZEBRA ARCHITECTURE BENEFITS              |
+============================================================+
|                                                            |
|  +-----------------------+                                 |
|  | Protocol processes    |  -> Eliminates race conditions |
|  | don't touch kernel    |                                 |
|  +-----------------------+                                 |
|                                                            |
|  +-----------------------+                                 |
|  | Single RIB performs   |  -> Solves best path selection |
|  | route arbitration     |                                 |
|  +-----------------------+                                 |
|                                                            |
|  +-----------------------+                                 |
|  | Transactional FIB     |  -> Solves consistency issues  |
|  | updates               |                                 |
|  +-----------------------+                                 |
|                                                            |
|  +-----------------------+                                 |
|  | VRF-aware route       |  -> Solves route leakage       |
|  | management            |                                 |
|  +-----------------------+                                 |
|                                                            |
|  +-----------------------+                                 |
|  | Graceful restart      |  -> Solves process crash issue |
|  | support               |                                 |
|  +-----------------------+                                 |
|                                                            |
+============================================================+
```

**中文说明：**

Zebra 作为解决方案提供五大架构优势：
1. 协议进程不直接操作内核路由表 → 解决竞态条件
2. 单一 RIB 进行路由仲裁 → 解决最佳路径选择问题
3. 事务性 FIB 更新 → 解决一致性问题
4. VRF 感知路由管理 → 解决路由泄露问题
5. 优雅重启支持 → 解决进程崩溃问题

---

## 5. Core Design Principles

| Principle | Description | Benefit |
|-----------|-------------|---------|
| **Single Point Control** | Only Zebra can modify kernel routing table | Eliminates race conditions |
| **Protocol Agnostic** | RIB doesn't care which protocol route comes from | Unified processing logic |
| **Async Updates** | Protocol returns immediately after submitting route | Doesn't block protocol processing |
| **Eventual Consistency** | FIB eventually becomes consistent with RIB | High availability |
| **Failure Isolation** | One protocol crash doesn't affect others | Fault isolation |

**中文说明：**

核心设计原则包括：单点控制（只有 Zebra 可以修改内核路由表，消除竞态条件）、协议无关（RIB 不关心路由来自哪个协议，统一处理逻辑）、异步更新（协议提交路由后立即返回，不阻塞协议处理）、最终一致性（FIB 最终会与 RIB 一致，保证高可用性）、失败隔离（一个协议崩溃不影响其他协议）。

---

## Key Takeaways

1. **Zebra is a necessary abstraction layer** - It isolates protocol logic from kernel interaction
2. **Single writer principle** - Only Zebra can modify the kernel routing table
3. **RIB is the source of truth** - All routing decisions are based on RIB state
4. **Protocol decoupling** - Protocol processes focus on protocol logic, not FIB installation details

**中文说明：**

关键要点：Zebra 是必要的抽象层，隔离了协议逻辑和内核交互；单一写者原则，只有 Zebra 可以修改内核路由表；RIB 是真相之源，所有路由决策基于 RIB 状态；协议解耦，协议进程专注于协议逻辑，不关心 FIB 安装细节。
