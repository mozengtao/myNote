# Nomad Client–Server Architecture（系统化整理）

本文整理了 **HashiCorp Nomad 架构、核心数据模型、调度器 Pipeline、生产级系统架构**，并使用 ASCII Diagram 帮助建立完整的 mental model。

---

# 1 Nomad 简介

Nomad 是一个 **集群调度系统 (Cluster Scheduler)**，用于在多台机器上运行：

- container
- binary
- VM
- batch job
- service

核心思想：

> **Server = 调度 + 控制**  
> **Client = 执行任务**  

---

# 2 Client–Server 架构

整体架构：

```
                +----------------------+
                |      Nomad CLI       |
                | (job submit / query) |
                +----------+-----------+
                           |
                           | HTTP API
                           v
             +------------------------------+
             |        Nomad Servers         |
             |------------------------------|
             |  Leader (Raft)               |	← 核心决策节点，由Raft算法选举
             |  Scheduler                   |	← 仅在Leader运行
             |  State Store                 |	← 由Leader同步到所有Server
             |  Job Placement Decisions     |	← 仅由Leader做出
             +--------------+---------------+
                            |
                            | RPC
                            |
    ---------------------------------------------------
    |                       |                         |
    v                       v                         v
+---------------+   +---------------+   +---------------+
| Nomad Client  |   | Nomad Client  |   | Nomad Client  |
| Node A        |   | Node B        |   | Node C        |
|---------------|   |---------------|   |---------------|
| Task Driver   |   | Task Driver   |   | Task Driver   |
| (docker,exec) |   | (docker,exec) |   | (docker,exec) |
| Resource Mgr  |   | Resource Mgr  |   | Resource Mgr  |
| Task Runner   |   | Task Runner   |   | Task Runner   |
+-------+-------+   +-------+-------+   +-------+-------+
        |                    |                    |
        v                    v                    v
   Container            Batch Job              Service

1. Raft 是 Nomad Server 集群的 “投票规则”，保证数据一致和故障容错；
2. Leader 是 Raft 选举出的核心节点，唯一处理写请求、执行调度决策；
3. Nomad Server 部署 3/5 节点，是为了满足 Raft 法定人数，保证 Leader 选举和集群可用。
```

核心逻辑：

> **Nomad Server = 控制平面**
> **Nomad Client = 数据平面**

---

# 3 Nomad Server 内部结构

Server 负责调度与状态管理。

```
        +------------------------------------+
        |            Nomad Server            |
        |------------------------------------|
        |                                    |
        |  +------------------------------+  |
        |  |           Raft Layer         |  |
        |  |------------------------------|  |
        |  | Leader Election              |  |
        |  | Replicated State             |  |
        |  +--------------+---------------+  |
        |                 |                  |
        |                 v                  |
        |        +------------------+        |
        |        |    Scheduler     |        |
        |        |------------------|        |
        |        | Bin Packing      |        |
        |        | Placement        |        |
        |        | Constraint Eval  |        |
        |        +---------+--------+        |
        |                  |                 |
        |                  v                 |
        |        +------------------+        |
        |        |   Cluster State  |        |
        |        |------------------|        |
        |        | Nodes            |        |
        |        | Allocations      |        |
        |        | Jobs             |        |
        |        +------------------+        |
        |                                    |
        +------------------------------------+
```

Server 通常部署：

> **3 or 5 servers**

保证 Raft quorum。

---

# 4 Nomad Client 内部结构

Client 负责执行任务。

```
      +------------------------------------+
      |            Nomad Client            |
      |------------------------------------|
      |                                    |
      |  +------------------------------+  |
      |  |        Node Manager          |  |
      |  |------------------------------|  |
      |  | CPU / Memory Detection       |  |
      |  | Resource Tracking            |  |
      |  +--------------+---------------+  |
      |                 |                  |
      |                 v                  |
      |       +---------------------+      |
      |       |      Task Driver    |      |
      |       |---------------------|      |
      |       | docker              |      |
      |       | exec                |      |
      |       | qemu                |      |
      |       | java                |      |
      |       +----------+----------+      |
      |                  |                 |
      |                  v                 |
      |       +---------------------+      |
      |       |     Task Runner     |      |
      |       |---------------------|      |
      |       | start/stop task     |      |
      |       | log collection      |      |
      |       +---------------------+      |
      |                                    |
      +------------------------------------+
```

---

# 5 Nomad 核心数据模型

Nomad 最重要的数据结构：

> **Job → TaskGroup → Task → Allocation**

---

## Job 结构

```
Job
│
├── Datacenters
├── Type (service / batch / system)
│
└── Task Groups
    │
    ├── Network
    ├── Volume
    ├── Count
    │
    └── Tasks
        │
        ├── Driver (docker / exec)
        ├── Config
        ├── Resources
        └── Env
```

---

## 示例

```
Job: web-app
│
└── TaskGroup: web-group
    │
    ├── Task: nginx
    │   driver = docker
    │
    └── Task: api
        driver = docker
```

关键特点：

> **TaskGroup 内所有 Task 在同一机器运行**

原因：

- 共享 network
- 共享 volume
- 共享 lifecycle

---

# 6 Allocation（运行实例）

Allocation 是 TaskGroup 的运行实例。

```
Job
│
└── TaskGroup (count = 3)
    │
    ├── Allocation #1 -> Node A
    ├── Allocation #2 -> Node B
    └── Allocation #3 -> Node C
```

展开：

```
Node A                  Node B                  Node C
└─ Allocation           └─ Allocation           └─ Allocation
   ├─ nginx                ├─ nginx                ├─ nginx
   └─ api                  └─ api                  └─ api
```

总结：

> **TaskGroup = 模板**
> **Allocation = 实例**

---

# 7 Scheduler Pipeline

Nomad 调度器是一个事件驱动 pipeline。

> **Job → Evaluation → Scheduler → Plan → Allocation**

完整流程：

```
User submits Job
       │
       ▼
+----------------------+
|        Job           |
+----------+-----------+
           │
           ▼
+----------------------+
|     Evaluation       |
| (Scheduling trigger) |
+----------+-----------+
           │
           ▼
+----------------------+
|     Scheduler        |
| (placement decision) |
+----------+-----------+
           │
           ▼
+----------------------+
|        Plan          |
| (allocation changes) |
+----------+-----------+
           │
           ▼
+----------------------+
|     Allocation       |
| (runtime instance)   |
+----------+-----------+
           │
           ▼
+----------------------+
|    Nomad Client      |
|     run tasks        |
+----------------------+
```

---

# 8 Evaluation（调度事件）

Evaluation 是调度触发事件。

触发条件：

- Job 提交
- Job 更新
- Node 加入
- Node 失败
- Allocation 失败
- Scaling

结构：

```
Evaluation
│
├── JobID
├── Trigger
│   ├── JobRegister
│   ├── NodeUpdate
│   └── AllocationFailed
│
└── Priority
```

---

# 9 Scheduler（节点选择）

Scheduler 进行资源匹配。

流程：

```
TaskGroup requirements
       │
       ▼
  Node filtering
       │
       ▼
  Node scoring
       │
       ▼
  Select best node
```

示例：

```
TaskGroup
  CPU = 500
  MEM = 256

         Nodes
  +------+------+------+
  |NodeA |NodeB |NodeC |
  +------+------+------+
    200    800    300
    fail    ✓    fail
```

---

# 10 Plan（调度计划）

Scheduler 生成 Plan。

```
Plan
│
├── Create Allocation
├── Stop Allocation
└── Reschedule Allocation
```

---

# 11 Raft Commit

Nomad Server 使用 Raft 共识。

```
Scheduler
    │
    ▼
Create Plan
    │
    ▼
Raft Log
    │
    ▼
Replicate to servers
    │
    ▼
  Commit
```

只有 commit 后：

> **Allocation 才真正生效**

---

# 12 Allocation 生命周期

```
pending
   │
   ▼
starting
   │
   ▼
running
   │
   ▼
complete / failed
```

如果失败：

> **重新创建 Evaluation**

---

# 13 生产级 Nomad 系统架构

Nomad 常与两个系统结合：

> **Nomad + Consul + Vault**

职责：

| 组件   | 职责       |
|--------|------------|
| Nomad  | 调度       |
| Consul | 服务发现   |
| Vault  | Secrets    |

---

# 14 Consul 架构

```
            +-------------------+
            |   Consul Server   |
            |  Service Catalog  |
            +---------+---------+
                      |
      ----------------+----------------
      |               |               |
      v               v               v
+-------------+ +-------------+ +-------------+
| Consul Agent| | Consul Agent| | Consul Agent|
| Node A      | | Node B      | | Node C      |
+------+------+ +------+------+ +------+------+
       |               |               |
       v               v               v
  Nomad Task       Nomad Task      Nomad Task
   register         register        register
```

---

# 15 Vault Secrets 管理

```
Task start
    |
    v
Nomad Client
    |
    v
Request secret
    |
    v
  Vault
    |
    v
Generate dynamic secret
    |
    v
Return to task
```

优势：

- 动态密码
- 自动过期
- 不写入代码

---

# 16 Nomad Client 执行结构

```
           Nomad Client
                |
                v
       +------------------+
       | Allocation Runner|
       +---------+--------+
                 |
                 v
           Task Runners
                 |
    -------------+-------------
    |            |            |
    v            v            v
docker drv   exec drv    java drv
    |            |            |
    v            v            v
container    process        JVM
```

---

# 17 完整生产架构图

```
                      Users
                        |
                        v
                 +--------------+
                 |  Nomad CLI   |
                 +------+-------+
                        |
                        v
            +---------------------------+
            |       Nomad Servers       |
            |---------------------------|
            | Scheduler                 |
            | Raft Consensus            |
            | Cluster State             |
            +-----------+---------------+
                        |
                        |
                        v
    -------------------------------------------------
    |                   |                           |
    v                   v                           v
+--------------+  +--------------+         +--------------+
| Nomad Client |  | Nomad Client |         | Nomad Client |
| Node A       |  | Node B       |         | Node C       |
|--------------|  |--------------|         |--------------|
| Task Runner  |  | Task Runner  |         | Task Runner  |
| Docker       |  | Exec         |         | Docker       |
| Consul Agent |  | Consul Agent |         | Consul Agent |
+------+-------+  +------+-------+         +------+-------+
       |                 |                        |
       v                 v                        v
  Containers        Containers               Containers

+-------------------+       +-------------------+
| Consul Servers    |       | Vault Cluster     |
| Service Discovery |       | Secret Management |
+-------------------+       +-------------------+
```

---

# 18 核心总结

Nomad 的关键概念：

| 概念       | 含义         |
|------------|--------------|
| Job        | 应用定义     |
| TaskGroup  | 调度单位     |
| Allocation | 运行实例     |
| Task       | 实际程序     |

调度 pipeline：

> **Evaluation → Scheduler → Plan → Allocation**

系统组件职责：

| 组件   | 职责             |
|--------|------------------|
| Nomad  | 调度             |
| Consul | 网络与服务发现   |
| Vault  | Secrets          |

Nomad 本质上是：

> **Distributed Process Scheduler**
