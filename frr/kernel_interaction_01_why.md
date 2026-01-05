# FRR Kernel Interaction Architecture - Part 1: WHY

## Why Kernel Is Not a Database

Understanding why the Linux kernel's routing state cannot be treated as a
reliable database is fundamental to building robust network control systems.

---

## 1. Architectural Overview

```
+===========================================================================+
|                    KERNEL STATE CHARACTERISTICS                           |
+===========================================================================+

+---------------------------------------------------------------------------+
|                          CONTROL PLANE (FRR)                              |
|                                                                           |
|  +------------------+     +------------------+     +------------------+   |
|  |   RIB (Source    |     |  Dataplane       |     |  Reconciliation  |   |
|  |   of Truth)      |---->|  Context Queue   |---->|  Logic           |   |
|  +------------------+     +------------------+     +------------------+   |
|           |                       |                        |              |
|           v                       v                        v              |
|  +------------------------------------------------------------------------+
|  |                    NETLINK SOCKET LAYER                                |
|  |   - Command Socket (sync operations)                                   |
|  |   - Event Socket (async notifications)                                 |
|  +------------------------------------------------------------------------+
+---------------------------------------------------------------------------+
                                    |
                                    | Netlink Messages
                                    v
+---------------------------------------------------------------------------+
|                          KERNEL SPACE                                     |
|                                                                           |
|  +------------------+     +------------------+     +------------------+   |
|  |   FIB (Forward   |     |  Interface       |     |  Neighbor        |   |
|  |   Info Base)     |     |  State           |     |  Cache (ARP/ND)  |   |
|  +------------------+     +------------------+     +------------------+   |
|           |                       |                        |              |
|           +-----------------------------------------------+              |
|                                   |                                       |
|                    +--------------v---------------+                       |
|                    |      MUTABLE STATE           |                       |
|                    |  - Can change without notice |                       |
|                    |  - No transaction support    |                       |
|                    |  - Race conditions possible  |                       |
|                    +------------------------------+                       |
+---------------------------------------------------------------------------+
                                    |
                                    | External Events
                                    v
+---------------------------------------------------------------------------+
|                      EXTERNAL STATE SOURCES                               |
|                                                                           |
|  +------------------+     +------------------+     +------------------+   |
|  |  Other Routing   |     |  Network Admin   |     |  Hardware        |   |
|  |  Daemons         |     |  (ip route cmd)  |     |  Events          |   |
|  +------------------+     +------------------+     +------------------+   |
+---------------------------------------------------------------------------+


+===========================================================================+
|                    STATE CONSISTENCY CHALLENGES                           |
+===========================================================================+

  Time --->
  
  T0: FRR installs route A
  +-----------------------+
  |  FRR RIB: Route A     |  ------>  Kernel FIB: Route A  (consistent)
  +-----------------------+

  T1: Admin runs "ip route del A"
  +-----------------------+
  |  FRR RIB: Route A     |  ----X    Kernel FIB: (empty)  (INCONSISTENT!)
  +-----------------------+

  T2: Link goes down, kernel auto-removes routes
  +-----------------------+
  |  FRR RIB: Route B     |  ----X    Kernel FIB: (empty)  (INCONSISTENT!)
  +-----------------------+

  T3: Another daemon installs conflicting route
  +-----------------------+
  |  FRR RIB: Route C     |  ----X    Kernel FIB: Route D  (CONFLICT!)
  +-----------------------+


+===========================================================================+
|                         WHY RECONCILIATION                                |
+===========================================================================+

                    +------------------------+
                    |   FRR STARTUP          |
                    +------------------------+
                              |
              +---------------+---------------+
              |                               |
              v                               v
   +-------------------+           +-------------------+
   | Read Kernel State |           | Build RIB from    |
   | (Full Dump)       |           | Config/Protocols  |
   +-------------------+           +-------------------+
              |                               |
              +---------------+---------------+
                              |
                              v
                    +------------------------+
                    |    COMPARE             |
                    | (Kernel vs RIB)        |
                    +------------------------+
                              |
              +---------------+---------------+
              |               |               |
              v               v               v
   +-------------+    +-------------+    +-------------+
   | In Kernel   |    | In RIB      |    | In Both     |
   | Not in RIB  |    | Not Kernel  |    | (Match?)    |
   +-------------+    +-------------+    +-------------+
         |                   |                  |
         v                   v                  v
   +-------------+    +-------------+    +-------------+
   | Delete from |    | Install to  |    | Update if   |
   | Kernel      |    | Kernel      |    | Different   |
   +-------------+    +-------------+    +-------------+
```

---

## 中文解释 (Chinese Explanation)

### 1.1 内核状态的本质特征

**为什么内核不是数据库？**

Linux 内核的路由状态具有以下特点，使其无法作为可靠的数据源：

1. **可变性 (Mutability)**
   - 内核状态可以被任何具有权限的进程修改
   - 网络管理员可以使用 `ip route` 命令直接修改路由
   - 其他路由守护进程也可以安装/删除路由
   - 硬件事件（如链路故障）会自动触发状态变更

2. **无事务支持 (No Transaction Support)**
   - 内核不保证操作的原子性
   - 批量更新可能部分成功、部分失败
   - 没有回滚机制

3. **状态丢失 (State Loss)**
   - 内核路由可能因接口 down 而自动删除
   - 内核重启会丢失所有动态路由
   - 没有持久化保证

### 1.2 为什么需要状态调和 (Reconciliation)

**问题场景：**

```
场景1：外部修改
- FRR 安装了路由 10.0.0.0/8 → 192.168.1.1
- 管理员执行: ip route del 10.0.0.0/8
- 此时 FRR 的 RIB 和内核 FIB 不一致

场景2：链路故障
- FRR 安装了路由，下一跳是 eth0
- eth0 链路故障，内核自动删除相关路由
- FRR 需要知道这个变化

场景3：重启后恢复
- FRR 重启，读取配置和协议状态
- 内核可能还有旧的路由条目
- 需要清理旧条目，安装新条目
```

**调和策略：**

1. **启动时全量同步**
   - 获取内核完整路由表
   - 与 RIB 期望状态比较
   - 增量更新差异

2. **运行时增量监听**
   - 通过 Netlink 监听内核事件
   - 检测外部修改
   - 根据策略决定是否重新安装

### 1.3 为什么 Netlink 错误不是致命的

**错误分类：**

```c
// FRR kernel_netlink.c 中的错误处理逻辑

// 可忽略的错误：
// ENODEV  - 设备不存在（可能正在删除）
// ESRCH   - 路由不存在（可能已被删除）
// EEXIST  - 路由已存在（可能是重复安装）
// ENETDOWN - 网络不可达（临时状态）

// 这些错误表示"状态已经是期望的"或"瞬态竞争条件"
// 不应该导致 FRR 崩溃或停止工作
```

**设计哲学：**

1. **幂等性 (Idempotency)**
   - 重复安装同一路由应该是安全的
   - 删除不存在的路由应该返回成功

2. **最终一致性 (Eventual Consistency)**
   - 短暂的不一致是可接受的
   - 系统会通过定期调和恢复一致

3. **故障隔离 (Fault Isolation)**
   - 单个路由安装失败不应影响其他路由
   - 记录错误日志但继续处理

### 1.4 多写入者问题

```
+---------------------------------------------------------------+
|                    KERNEL FIB                                  |
|                                                                |
|   Writer 1: FRR Zebra                                          |
|   Writer 2: Network Admin (iproute2)                           |
|   Writer 3: NetworkManager                                     |
|   Writer 4: Container Runtime (Docker/K8s)                     |
|   Writer 5: Other routing daemons (BIRD, etc.)                 |
+---------------------------------------------------------------+

问题：谁是路由表的 "权威源"？

FRR 的答案：
- FRR 管理 FRR 自己安装的路由（通过 RTPROT 标识）
- FRR 监听但不干预其他来源的路由
- FRR 维护自己的 RIB 作为 "真相来源"
```

### 1.5 关键设计原则

1. **RIB 是唯一真相来源**
   - 内核状态只是缓存
   - 任何时候都可以从 RIB 重建内核状态

2. **防御性编程**
   - 假设内核状态随时可能变化
   - 验证操作结果而不是假设成功

3. **优雅降级**
   - 内核功能可能不可用（权限、内核版本）
   - 设计应能在功能受限时继续工作

4. **可观测性**
   - 记录所有内核交互
   - 支持调试和问题诊断

---

## 2. Core Engineering Problems

### 2.1 The Multi-Writer Problem

Multiple processes can modify kernel routing state:

| Writer | Example | Impact |
|--------|---------|--------|
| FRR | Zebra daemon | Primary routing source |
| Admin | `ip route add` | Manual overrides |
| Other daemons | NetworkManager, systemd-networkd | Container networking |
| Kernel | Auto-removal on link down | Implicit state changes |

### 2.2 The Consistency Window

```
     FRR                     Kernel
      |                        |
   Install Route A             |
      |------ RTM_NEWROUTE --->|
      |                        | Route A installed
      |                        |
      |                        | <-- External delete
      |                        | Route A removed
      |                        |
   RIB still has A             | FIB has no A
      |                        |
   [INCONSISTENT STATE]        |
```

### 2.3 Why This Matters

- **Packet loss**: Traffic follows kernel FIB, not FRR RIB
- **Routing loops**: Inconsistent state across devices
- **Silent failures**: No immediate indication of state mismatch

---

## 3. FRR's Solution: Source of Truth Architecture

FRR treats the RIB as the authoritative source:

1. **RIB is the master**: Kernel state is derived from RIB
2. **Detect divergence**: Monitor kernel events via Netlink
3. **Reconcile periodically**: Full sync on startup, incremental during runtime
4. **Fail gracefully**: Individual route failures don't crash the system

---

## Next: Part 2 - HOW (Netlink Strategy)
