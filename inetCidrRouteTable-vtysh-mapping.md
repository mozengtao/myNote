# FRR vtysh 路由输出到 inetCidrRouteTable SNMP MIB 的映射关系

## 1. 概述

`inetCidrRouteTable`（IP-FORWARD-MIB, OID: 1.3.6.1.2.1.4.24.7）是一个用于表示 IP 路由表的 SNMP MIB 表。FRR（Free Range Routing）的 `vtysh` 命令 `show ip route` 和 `show ipv6 route` 输出的路由信息可以映射到该表的每一行（`inetCidrRouteEntry`）。

## 2. vtysh 输出格式解析

### 2.1 路由代码（Route Code）

```
K - kernel, C - connected, L - local, S - static,
R - RIP, O - OSPF, I - IS-IS, B - BGP, E - EIGRP, ...
```

### 2.2 标志位（Flags）

| 标志 | 含义 |
|------|------|
| `>` | selected route（被选中的最优路由） |
| `*` | FIB route（已安装到内核转发表） |
| `q` | queued |
| `r` | rejected |2

### 2.3 输出行格式

带网关的路由：
```
CODE>* destination/prefix [admin_distance/metric] via next_hop, interface, weight W, uptime
```

直连路由：
```
CODE>* destination/prefix is directly connected, interface, weight W, uptime
```

## 3. 路由选择核心概念：Administrative Distance、Metric 与 Weight

vtysh 输出中 `[X/Y]` 和 `weight W` 包含三个不同层次的路由选择参数。理解它们是理解 vtysh 输出与 MIB 映射关系的前提。

### 3.1 三层筛选模型总览

```
收到数据包，查找目的地 192.168.1.0/24 的路由：

 ┌──────────────────────────────────────────────────────────────────┐
 │  第1层：Administrative Distance（管理距离）— 跨协议选路             │
 │                                                                  │
 │  Connected [0/0]  ◄── AD=0，最可信                                │
 │  Static    [1/0]      AD=1                                       │
 │  OSPF      [110/20]   AD=110                                     │
 │  BGP       [20/0]     AD=20                                      │
 │  RIP       [120/3]    AD=120                                     │
 │                                                                  │
 │  规则：AD 最小的协议胜出 → Connected 胜出                          │
 └──────────────────────────────┬───────────────────────────────────┘
                                │ 如果同一协议有多条路由
                                ▼
 ┌──────────────────────────────────────────────────────────────────┐
 │  第2层：Metric（度量值）— 同协议内选路                             │
 │                                                                  │
 │  OSPF 路由A  [110/10]  ◄── metric=10，开销更低                    │
 │  OSPF 路由B  [110/50]      metric=50                             │
 │  OSPF 路由C  [110/10]      metric=10，与 A 相同                   │
 │                                                                  │
 │  规则：metric 最小胜出 → A 和 C 并列（进入 ECMP）                   │
 └──────────────────────────────┬───────────────────────────────────┘
                                │ 如果多条路由 metric 相同（ECMP）
                                ▼
 ┌──────────────────────────────────────────────────────────────────┐
 │  第3层：Weight（权重）— 等价路径间分配流量                          │
 │                                                                  │
 │  OSPF 路由A  via 10.0.0.1, weight 3  → 75% 流量                   │
 │  OSPF 路由C  via 10.0.0.2, weight 1  → 25% 流量                   │
 │                                                                  │
 │  规则：不比大小，按 weight 比例分配流量                             │
 └──────────────────────────────────────────────────────────────────┘
```

### 3.2 Administrative Distance（管理距离）

#### 概念

Administrative Distance（AD）衡量**路由来源的可信度**。当多种路由协议同时通告到达同一目的地的路由时，AD 决定采信哪个协议的结果。**数值越小，越可信。**

#### 各协议默认 AD 值

| AD 值 | 路由来源 | vtysh 代码 | 说明 |
|-------|---------|-----------|------|
| 0 | Connected / Local | C / L | 本机接口直连，最可信 |
| 0 | Kernel | K | 内核直接管理的路由 |
| 1 | Static | S | 管理员手动配置 |
| 20 | eBGP | B | 外部 BGP 邻居学到的路由 |
| 110 | OSPF | O | 链路状态协议计算结果 |
| 115 | IS-IS | I | 链路状态协议计算结果 |
| 120 | RIP | R | 距离向量协议通告 |
| 200 | iBGP | B | 内部 BGP 邻居学到的路由 |

#### vtysh 中的体现

`[X/Y]` 中的 **X** 就是 AD：

```
K>* 0.0.0.0/0        [0/0]     ← AD=0, kernel 路由
S>* 64.150.10.181/32  [1/0]     ← AD=1, static 路由
K>* ::/0              [0/1024]  ← AD=0, kernel 路由
```

直连路由（C/L）的 AD=0，因为完全不需要比较，所以 vtysh 不显示 `[X/Y]`。

#### 适用场景

当**不同协议**同时通告到同一目的网络的路由时触发 AD 比较。例如：
- 管理员配了一条 `S 10.0.0.0/8 via 192.168.1.1`（AD=1）
- 同时 OSPF 也学到了 `O 10.0.0.0/8 via 192.168.2.1`（AD=110）
- FRR 选择 Static（AD=1 < 110），OSPF 路由作为备份保留

### 3.3 Metric（度量值）

#### 概念

Metric 衡量**同一路由协议内，到达目的地的路径优劣**。AD 筛选确定了采信哪个协议后，metric 在该协议的多条候选路由中选出最优路径。**数值越小，路径越优。**

#### 各协议 metric 的含义

不同协议对 metric 的定义完全不同：

| 协议 | Metric 代表 | 计算方式 | 示例 |
|------|-----------|---------|------|
| Static | 管理员指定的固定值 | 手动设定 | 通常为 0 |
| Kernel | 内核设定的优先级 | 系统自动设定 | IPv6 RA 默认路由为 1024 |
| RIP | 跳数（hop count） | 经过路由器的数量 | 3 = 经过 3 台路由器 |
| OSPF | 链路开销（cost） | 通常 10^8 / 带宽(bps) | 1G 链路 = 1, 100M = 10 |
| BGP | 多属性综合 | AS-path、MED、local-pref 等 | 复杂的多步骤比较 |
| IS-IS | 链路开销 | 管理员配置或自动 | 类似 OSPF |

#### vtysh 中的体现

`[X/Y]` 中的 **Y** 就是 metric：

```
K>* 0.0.0.0/0  [0/0]     ← metric=0
K>* ::/0       [0/1024]  ← metric=1024（IPv6 RA 默认路由的内核默认值）
S>* 64.150.10.181/32 [1/0]  ← metric=0
```

#### 适用场景

当**同一协议**有多条到达同一目的地的路由时触发 metric 比较。例如：
- OSPF 通过 eth1（1G 链路）学到 `10.0.0.0/8`，cost=1
- OSPF 通过 eth2（100M 链路）学到 `10.0.0.0/8`，cost=10
- FRR 选择 eth1（metric 1 < 10）

如果 metric 相等，则两条路由都保留，形成 ECMP（等价多路径），进入第 3 层 weight 分配。

### 3.4 Weight（权重）

#### 概念

Weight 用于 **ECMP（Equal-Cost Multi-Path，等价多路径）场景下的流量分配**。当多条到达同一目的地的路由具有相同的 AD 和 metric 时，它们都会被安装到转发表中，weight 决定每条路径承载多少比例的流量。

**Weight 不是选路指标，而是负载分配比例。**

#### vtysh 中的体现

```
S>* 192.168.1.0/24 [1/0] via 10.0.0.1, eth1, weight 1, 01:00:00
  *                      via 10.0.0.2, eth2, weight 3, 01:00:00
```

流量按 weight 比例分配：
- via 10.0.0.1：weight 1 → 1/(1+3) = **25%** 流量
- via 10.0.0.2：weight 3 → 3/(1+3) = **75%** 流量

#### 单路径情况

当只有一个下一跳时，`weight 1` 无实际意义，是 FRR 的默认值：

```
K>* ::/0 [0/1024] via fe80::1a2a:d301:f47f:2823, eth1, weight 1, 22:41:23
                                                        ^^^^^^^^
                                                        单路径，weight 无作用
```

#### 适用场景

- 服务器双上联到两台交换机，两条默认路由 metric 相同
- 数据中心多条等价 BGP/OSPF 路径需要负载均衡
- 需要非均等分流时（如一条链路带宽是另一条的 3 倍）

### 3.5 完整路由选择流程

以一个数据包到达路由器、需要查找到 `10.0.0.0/8` 的路由为例：

```
步骤 1: 最长前缀匹配（Longest Prefix Match）
─────────────────────────────────────────────
  路由表中有:
    10.0.0.0/8      ← 匹配
    10.0.0.0/16     ← 匹配，且前缀更长
    10.0.0.0/24     ← 匹配，且前缀最长 ◄── 选中
    192.168.0.0/16  ← 不匹配

  规则: 前缀越长越精确，优先使用

步骤 2: Administrative Distance 比较
─────────────────────────────────────────────
  匹配到 10.0.0.0/24 的路由有:
    S  10.0.0.0/24  [1/0]    AD=1   ◄── 胜出
    O  10.0.0.0/24  [110/10] AD=110
    R  10.0.0.0/24  [120/2]  AD=120

  规则: AD 最小的协议胜出

步骤 3: Metric 比较（同一协议内）
─────────────────────────────────────────────
  如果有多条 Static 路由:
    S  10.0.0.0/24  [1/0]  via 10.1.1.1   ◄── metric 相同
    S  10.0.0.0/24  [1/0]  via 10.2.2.2   ◄── metric 相同 → ECMP
    S  10.0.0.0/24  [1/5]  via 10.3.3.3       metric=5，淘汰

  规则: metric 最小胜出；相同则保留所有（ECMP）

步骤 4: Weight 流量分配（ECMP 路径间）
─────────────────────────────────────────────
  两条等价路径:
    via 10.1.1.1, weight 1  → 50% 流量
    via 10.2.2.2, weight 1  → 50% 流量

  规则: 按 weight 比例分配
```

### 3.6 三者对比总结

| 维度 | Administrative Distance | Metric | Weight |
|------|------------------------|--------|--------|
| 作用层次 | 跨协议选路（第 1 层） | 同协议内选路（第 2 层） | 等价路径分流（第 3 层） |
| 比较对象 | 不同协议到同一目的地 | 同一协议到同一目的地 | AD 和 metric 都相同的路径 |
| 选择规则 | 越小越优，唯一胜出 | 越小越优，相等则并列 | 不比大小，按比例分配 |
| vtysh 中的位置 | `[X/Y]` 中的 X | `[X/Y]` 中的 Y | `weight W` |
| MIB 对应字段 | **无**（FRR 内部概念） | inetCidrRouteMetric1 | **无**（FRR 内部概念） |
| 谁设定的 | 协议默认值（可手动调） | 协议自动计算或手动设定 | 管理员设定或默认为 1 |

### 3.7 与 inetCidrRouteTable MIB 的关系

inetCidrRouteTable 展示的是路由选择**之后**的最终结果，而非选择过程本身：

```
FRR 内部路由选择过程（不可见于 MIB）          MIB 展示的最终结果
────────────────────────────────          ─────────────────────
AD 比较   → 淘汰非最优协议                inetCidrRouteProto  (来源协议)
Metric 比较 → 淘汰非最优路径              inetCidrRouteMetric1 (metric 值)
Weight 分配 → 确定各路径流量比例           不可见（每条路径是独立的行）
```

- **AD** 在 MIB 中没有对应字段，但通过 `inetCidrRouteProto` 可以间接推断路由来源
- **Metric** 映射到 `inetCidrRouteMetric1`
- **Weight** 在 MIB 中没有对应字段；ECMP 的多条路径在 MIB 表中体现为多行（因为 NextHop 是 INDEX 的一部分），但无法从 MIB 中获知流量分配比例

## 4. 字段映射详解

### 3.1 inetCidrRouteEntry 字段总览

| # | MIB 字段 | 类型 | INDEX | vtysh 中的来源 |
|---|---------|------|-------|---------------|
| 1 | inetCidrRouteDestType | InetAddressType | YES | `show ip route` → ipv4(1)；`show ipv6 route` → ipv6(2) |
| 2 | inetCidrRouteDest | InetAddress | YES | 目的网络地址（`/` 前面的部分） |
| 3 | inetCidrRoutePfxLen | InetAddressPrefixLength | YES | 前缀长度（`/` 后面的数字） |
| 4 | inetCidrRoutePolicy | OBJECT IDENTIFIER | YES | 默认 `{0 0}`，vtysh 不显示 |
| 5 | inetCidrRouteNextHopType | InetAddressType | YES | 与 DestType 相同；直连路由可能为 unknown(0) |
| 6 | inetCidrRouteNextHop | InetAddress | YES | `via` 后面的地址；直连路由为零长度字符串 |
| 7 | inetCidrRouteIfIndex | InterfaceIndexOrZero | - | 接口名（eth1, dummy0）对应的系统 ifIndex |
| 8 | inetCidrRouteType | INTEGER | - | 根据路由类型判断：local(3) / remote(4) |
| 9 | inetCidrRouteProto | IANAipRouteProtocol | - | 路由代码字母（K/C/L/S/B/O/R/I...） |
| 10 | inetCidrRouteAge | Gauge32 | - | 路由存活时间（uptime），转换为秒数 |
| 11 | inetCidrRouteNextHopAS | InetAutonomousSystemNumber | - | BGP 路由的下一跳 AS 号，其它为 0 |
| 12 | inetCidrRouteMetric1 | Integer32 | - | `[admin_distance/metric]` 中的 metric 值 |
| 13 | inetCidrRouteMetric2 | Integer32 | - | 未使用，值为 -1 |
| 14 | inetCidrRouteMetric3 | Integer32 | - | 未使用，值为 -1 |
| 15 | inetCidrRouteMetric4 | Integer32 | - | 未使用，值为 -1 |
| 16 | inetCidrRouteMetric5 | Integer32 | - | 未使用，值为 -1 |
| 17 | inetCidrRouteStatus | RowStatus | - | 活跃路由为 active(1) |

### 3.2 各字段详细映射说明

#### inetCidrRouteDestType (INDEX 1)

地址族类型，由路由的地址类型决定：

| vtysh 命令 | 值 |
|-----------|-----|
| `show ip route` (IPv4) | ipv4(1) |
| `show ipv6 route` (IPv6) | ipv6(2) |

#### inetCidrRouteDest (INDEX 2)

目的网络地址，即 CIDR 记法中 `/` 前面的部分：

| vtysh 输出示例 | inetCidrRouteDest |
|---------------|-------------------|
| `0.0.0.0/0` | `0.0.0.0` |
| `64.110.0.0/16` | `64.110.0.0` |
| `64.150.10.181/32` | `64.150.10.181` |
| `2001:64:110:100::/64` | `2001:64:110:100::` |
| `::/0` | `::` |

#### inetCidrRoutePfxLen (INDEX 3)

前缀长度，即 CIDR 记法中 `/` 后面的数字：

| vtysh 输出示例 | inetCidrRoutePfxLen |
|---------------|---------------------|
| `0.0.0.0/0` | 0 |
| `64.110.0.0/16` | 16 |
| `64.150.10.181/32` | 32 |
| `2001:64:110:100::/64` | 64 |
| `fd00:198:18::6/128` | 128 |

#### inetCidrRoutePolicy (INDEX 4)

路由策略标识符。vtysh 基本输出中不显示此信息，默认值为 `{0 0}`。用于区分到同一目的地的多条策略路由。

#### inetCidrRouteNextHopType (INDEX 5) 和 inetCidrRouteNextHop (INDEX 6)

| vtysh 路由类型 | NextHopType | NextHop |
|--------------|-------------|---------|
| `via 64.220.0.1` (IPv4 网关) | ipv4(1) | `64.220.0.1` |
| `via 64.110.101.111` (IPv4 网关) | ipv4(1) | `64.110.101.111` |
| `via fe80::1a2a:...` (IPv6 链路本地网关) | ipv6(2) | `fe80::1a2a:d301:f47f:2823` |
| `via 2001:64:110:...` (IPv6 全局网关) | ipv6(2) | `2001:64:110:100:101::111` |
| `is directly connected` (直连) | ipv4(1) 或 ipv6(2) | `0.0.0.0` 或 `::` (零长度) |

#### inetCidrRouteIfIndex

接口名到系统 ifIndex 的映射。vtysh 显示的是接口名（如 `eth1`、`dummy0`），需要通过系统的 `ifIndex` 表（IF-MIB::ifIndex）转换为数值索引。

| vtysh 接口名 | ifIndex（示例） |
|-------------|----------------|
| `eth1` | 2（取决于系统） |
| `dummy0` | 3（取决于系统） |

> 实际 ifIndex 值可通过 `ip link show` 或 `snmpwalk IF-MIB::ifDescr` 获取。

#### inetCidrRouteType

根据路由是否有网关（下一跳）来判断：

| 条件 | inetCidrRouteType | 说明 |
|------|-------------------|------|
| `is directly connected`（C/L 路由） | local(3) | 下一跳就是最终目的地 |
| `via X.X.X.X`（有网关的 K/S/B/O 路由） | remote(4) | 下一跳不是最终目的地 |
| 被拒绝的路由 | reject(2) | 丢弃并返回 ICMP 通知 |
| 黑洞路由 | blackhole(5) | 静默丢弃 |

#### inetCidrRouteProto

路由代码字母到 IANAipRouteProtocol 枚举值的映射：

| vtysh 代码 | 含义 | IANAipRouteProtocol | 值 |
|-----------|------|---------------------|----|
| K | kernel | netmgmt(3) | 3 |
| C | connected | local(2) | 2 |
| L | local | local(2) | 2 |
| S | static | netmgmt(3) | 3 |
| R | RIP | rip(8) | 8 |
| O | OSPF | ospf(13) | 13 |
| I | IS-IS | isIs(9) | 9 |
| B | BGP | bgp(14) | 14 |
| E | EIGRP | ciscoEigrp(16) | 16 |

#### inetCidrRouteAge

路由存活时间，vtysh 以 `HH:MM:SS` 或 `Xd HH:MM` 格式显示，需转换为秒数：

| vtysh uptime | inetCidrRouteAge (秒) |
|-------------|----------------------|
| `22:41:04` | 22×3600 + 41×60 + 4 = **81,664** |
| `06:54:53` | 6×3600 + 54×60 + 53 = **24,893** |
| `04:30:41` | 4×3600 + 30×60 + 41 = **16,241** |

#### inetCidrRouteNextHopAS

下一跳自治系统号。仅对 BGP 路由有意义，其它路由类型为 0。vtysh 基本路由显示不包含此字段，需通过 `show ip bgp` 获取。

#### inetCidrRouteMetric1

`[admin_distance/metric]` 中的第二个数字（metric）映射为 inetCidrRouteMetric1：

| vtysh 显示 | admin_distance | metric (→ Metric1) |
|-----------|----------------|---------------------|
| `[0/0]` | 0 | **0** |
| `[1/0]` | 1 | **0** |
| `[0/1024]` | 0 | **1024** |
| 直连（无显示） | - | **0** |

> **注意**：`admin_distance`（管理距离）是 FRR 内部用于路由选择的优先级概念，**不直接对应** MIB 中的任何字段。Metric1 仅对应 metric 部分。

#### inetCidrRouteMetric2 ~ inetCidrRouteMetric5

FRR 基本路由表中通常只使用一个 metric，所以 Metric2-5 均为 **-1**（表示未使用）。

#### inetCidrRouteStatus

活跃路由（标志中带 `>` 和 `*`）的 RowStatus 为 **active(1)**。

## 5. 完整映射示例

### 示例 1：IPv4 Kernel 默认路由

vtysh 输出：
```
K>* 0.0.0.0/0 [0/0] via 64.220.0.1, eth1, weight 1, 22:41:04
```

| MIB 字段 | 值 |
|---------|-----|
| inetCidrRouteDestType | ipv4(1) |
| inetCidrRouteDest | 0.0.0.0 |
| inetCidrRoutePfxLen | 0 |
| inetCidrRoutePolicy | {0 0} |
| inetCidrRouteNextHopType | ipv4(1) |
| inetCidrRouteNextHop | 64.220.0.1 |
| inetCidrRouteIfIndex | ifIndex(eth1) |
| inetCidrRouteType | remote(4) |
| inetCidrRouteProto | netmgmt(3) |
| inetCidrRouteAge | 81664 |
| inetCidrRouteNextHopAS | 0 |
| inetCidrRouteMetric1 | 0 |
| inetCidrRouteMetric2 | -1 |
| inetCidrRouteMetric3 | -1 |
| inetCidrRouteMetric4 | -1 |
| inetCidrRouteMetric5 | -1 |
| inetCidrRouteStatus | active(1) |

### 示例 2：IPv4 Connected 路由

vtysh 输出：
```
C>* 64.110.0.0/16 is directly connected, dummy0, weight 1, 22:40:30
```

| MIB 字段 | 值 |
|---------|-----|
| inetCidrRouteDestType | ipv4(1) |
| inetCidrRouteDest | 64.110.0.0 |
| inetCidrRoutePfxLen | 16 |
| inetCidrRoutePolicy | {0 0} |
| inetCidrRouteNextHopType | ipv4(1) |
| inetCidrRouteNextHop | 0.0.0.0 (零长度) |
| inetCidrRouteIfIndex | ifIndex(dummy0) |
| inetCidrRouteType | local(3) |
| inetCidrRouteProto | local(2) |
| inetCidrRouteAge | 81630 |
| inetCidrRouteNextHopAS | 0 |
| inetCidrRouteMetric1 | 0 |
| inetCidrRouteMetric2 ~ 5 | -1 |
| inetCidrRouteStatus | active(1) |

### 示例 3：IPv4 Local 路由

vtysh 输出：
```
L>* 64.110.101.111/32 is directly connected, dummy0, weight 1, 22:40:30
```

| MIB 字段 | 值 |
|---------|-----|
| inetCidrRouteDestType | ipv4(1) |
| inetCidrRouteDest | 64.110.101.111 |
| inetCidrRoutePfxLen | 32 |
| inetCidrRoutePolicy | {0 0} |
| inetCidrRouteNextHopType | ipv4(1) |
| inetCidrRouteNextHop | 0.0.0.0 (零长度) |
| inetCidrRouteIfIndex | ifIndex(dummy0) |
| inetCidrRouteType | local(3) |
| inetCidrRouteProto | local(2) |
| inetCidrRouteAge | 81630 |
| inetCidrRouteNextHopAS | 0 |
| inetCidrRouteMetric1 | 0 |
| inetCidrRouteMetric2 ~ 5 | -1 |
| inetCidrRouteStatus | active(1) |

### 示例 4：IPv4 Static 路由

vtysh 输出：
```
S>* 64.150.10.181/32 [1/0] via 64.110.101.111, dummy0, weight 1, 06:54:53
```

| MIB 字段 | 值 |
|---------|-----|
| inetCidrRouteDestType | ipv4(1) |
| inetCidrRouteDest | 64.150.10.181 |
| inetCidrRoutePfxLen | 32 |
| inetCidrRoutePolicy | {0 0} |
| inetCidrRouteNextHopType | ipv4(1) |
| inetCidrRouteNextHop | 64.110.101.111 |
| inetCidrRouteIfIndex | ifIndex(dummy0) |
| inetCidrRouteType | remote(4) |
| inetCidrRouteProto | netmgmt(3) |
| inetCidrRouteAge | 24893 |
| inetCidrRouteNextHopAS | 0 |
| inetCidrRouteMetric1 | 0 |
| inetCidrRouteMetric2 ~ 5 | -1 |
| inetCidrRouteStatus | active(1) |

### 示例 5：IPv6 Kernel 默认路由（链路本地下一跳）

vtysh 输出：
```
K>* ::/0 [0/1024] via fe80::1a2a:d301:f47f:2823, eth1, weight 1, 22:41:23
```

| MIB 字段 | 值 |
|---------|-----|
| inetCidrRouteDestType | ipv6(2) |
| inetCidrRouteDest | :: |
| inetCidrRoutePfxLen | 0 |
| inetCidrRoutePolicy | {0 0} |
| inetCidrRouteNextHopType | ipv6(2) |
| inetCidrRouteNextHop | fe80::1a2a:d301:f47f:2823 |
| inetCidrRouteIfIndex | ifIndex(eth1) |
| inetCidrRouteType | remote(4) |
| inetCidrRouteProto | netmgmt(3) |
| inetCidrRouteAge | 81683 |
| inetCidrRouteNextHopAS | 0 |
| inetCidrRouteMetric1 | 1024 |
| inetCidrRouteMetric2 ~ 5 | -1 |
| inetCidrRouteStatus | active(1) |

### 示例 6：IPv6 Static 路由

vtysh 输出：
```
S>* 2001:64:150:100::fe:791c/128 [1/0] via 2001:64:110:100:101::111, dummy0, weight 1, 04:30:41
```

| MIB 字段 | 值 |
|---------|-----|
| inetCidrRouteDestType | ipv6(2) |
| inetCidrRouteDest | 2001:64:150:100::fe:791c |
| inetCidrRoutePfxLen | 128 |
| inetCidrRoutePolicy | {0 0} |
| inetCidrRouteNextHopType | ipv6(2) |
| inetCidrRouteNextHop | 2001:64:110:100:101::111 |
| inetCidrRouteIfIndex | ifIndex(dummy0) |
| inetCidrRouteType | remote(4) |
| inetCidrRouteProto | netmgmt(3) |
| inetCidrRouteAge | 16241 |
| inetCidrRouteNextHopAS | 0 |
| inetCidrRouteMetric1 | 0 |
| inetCidrRouteMetric2 ~ 5 | -1 |
| inetCidrRouteStatus | active(1) |

### 示例 7：IPv6 Connected 路由

vtysh 输出：
```
C>* 2001:64:110:100::/64 is directly connected, dummy0, weight 1, 22:40:33
```

| MIB 字段 | 值 |
|---------|-----|
| inetCidrRouteDestType | ipv6(2) |
| inetCidrRouteDest | 2001:64:110:100:: |
| inetCidrRoutePfxLen | 64 |
| inetCidrRoutePolicy | {0 0} |
| inetCidrRouteNextHopType | ipv6(2) |
| inetCidrRouteNextHop | :: (零长度) |
| inetCidrRouteIfIndex | ifIndex(dummy0) |
| inetCidrRouteType | local(3) |
| inetCidrRouteProto | local(2) |
| inetCidrRouteAge | 81633 |
| inetCidrRouteNextHopAS | 0 |
| inetCidrRouteMetric1 | 0 |
| inetCidrRouteMetric2 ~ 5 | -1 |
| inetCidrRouteStatus | active(1) |

## 6. SNMP INDEX 构成与 OID 形式

`inetCidrRouteEntry` 的 INDEX 由 6 个字段组成，因此每一行的 OID 会非常长。以示例 1 为例：

```
inetCidrRouteTable.1          -- inetCidrRouteEntry
  .1                           -- inetCidrRouteDestType = ipv4(1)
  .4.0.0.0.0                   -- inetCidrRouteDest = 0.0.0.0 (长度4 + 4字节)
  .0                           -- inetCidrRoutePfxLen = 0
  .0.0                         -- inetCidrRoutePolicy = {0 0}
  .1                           -- inetCidrRouteNextHopType = ipv4(1)
  .4.64.220.0.1                -- inetCidrRouteNextHop = 64.220.0.1 (长度4 + 4字节)
```

完整 OID 示例（查询 inetCidrRouteIfIndex, column 7）：
```
1.3.6.1.2.1.4.24.7.1.7.1.4.0.0.0.0.0.0.0.1.4.64.220.0.1
```

## 7. vtysh 中不可见但 MIB 中存在的信息

| MIB 字段 | 说明 |
|---------|------|
| inetCidrRoutePolicy | 路由策略 OID，vtysh 不显示，默认 {0 0} |
| inetCidrRouteNextHopAS | 下一跳 AS 号，需 `show ip bgp` 才能看到 |
| inetCidrRouteMetric2-5 | FRR 通常只使用一个 metric |
| inetCidrRouteStatus | RowStatus 管理状态，vtysh 不直接显示 |
| ifIndex 数值 | vtysh 显示接口名，MIB 使用数字索引 |

## 8. 关键注意事项

1. **admin_distance vs metric**：vtysh 中 `[X/Y]` 的 X 是管理距离（FRR 内部概念），Y 是 metric。MIB 中只有 metric 映射到 inetCidrRouteMetric1，管理距离无对应字段。

2. **weight 字段**：vtysh 显示的 `weight` 是 FRR 内部的 ECMP（等价多路径）权重，不直接映射到 inetCidrRouteTable 的任何字段。

3. **fe80:: 链路本地地址**：IPv6 的链路本地下一跳地址（如 `fe80::1a2a:d301:f47f:2823`）在 MIB 中的 inetCidrRouteNextHopType 仍然是 ipv6(2)，而非 ipv6z。

4. **多路径路由**：如果一条路由有多个等价下一跳（ECMP），每个下一跳在 MIB 表中是独立的一行，因为 NextHopType + NextHop 是 INDEX 的一部分。

5. **非 selected 路由**：vtysh 中没有 `>` 标志的路由（如 `C * fe80::/64`）可能仍出现在 MIB 表中，但其 inetCidrRouteStatus 可能为 notInService(2) 而非 active(1)。
