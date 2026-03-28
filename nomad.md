What is Nomad?  
> Nomad is a flexible scheduler and workload orchestrator that enables you to deploy and manage any application across on-premise and cloud infrastructure at scale.  

[Nomad Service Discovery 工作原理](nomad-service-discovery-principle.md)  
[Nomad Client–Server Architecture（系统化整理）](./Nomad_Client_Server_Architecture.md)  
[]()  

[**Introduction to Nomad**](https://developer.hashicorp.com/nomad/tutorials/get-started/gs-overview)  
[Tutorials](https://developer.hashicorp.com/nomad/tutorials)  
[Consul](https://developer.hashicorp.com/consul)  
[From Zero to WOW!](https://medium.com/hashicorp-engineering/hashicorp-nomad-from-zero-to-wow-1615345aa539)  
[Nomad Commands](https://developer.hashicorp.com/nomad/docs/commands)  
[Nomad job specification](https://developer.hashicorp.com/nomad/docs/job-specification)  
[HCL2](https://developer.hashicorp.com/nomad/docs/job-specification/hcl2)  
> Nomad uses the Hashicorp Configuration Language - HCL - designed to allow concise descriptions of the required steps to get to a job file  
[**Nomad Agent Configuration**](https://developer.hashicorp.com/nomad/docs/configuration)  
[Mastering HashiCorp Nomad: A Comprehensive Guide for Deploying and Managing Workloads](https://medium.com/@williamwarley/mastering-hashicorp-nomad-a-comprehensive-guide-for-deploying-and-managing-workloads-aa8720c2620b)  
[]()  
[]()  
[]()  

## Nomad 整体架构
### 单 Region 视图
```
+-------------------+          +---------------------+
|   Users / CLI     |          |   Nomad Servers     |
|   API / UI        |   RPC    |   (3–5 nodes)       |
|                   |<-------->|   Raft Consensus    |
+-------------------+          |   Leader + Followers|
                               |   - Accept Jobs     |
                               |   - Scheduling      |
                               |   - State Management|
                               +---------------------+
                                          |
                                          | Allocations (via RPC + Gossip)
                                          v
                               +-------------------------+
                               |   Nomad Clients         |
                               |   (hundreds–thousands)  |
                               |   - Execute Tasks       |
                               |   - Report Status       |
                               |   - Drivers: docker,    |
                               |      exec, java, qemu...|
                               +-------------------------+

Servers：负责控制平面（control plane），运行在少量（推荐 3 或 5 台）专用节点上，使用 Raft 实现强一致性。
Clients：负责数据平面（data plane），运行在大量 worker 节点上，执行实际工作负载。
通信协议：主要通过 RPC（gRPC-like） + Gossip（Serf）实现心跳、状态同步和服务发现。
```
### 多 Region / 联邦架构（可选）
```
 Region A                               Region B
[ Servers A ]  <--- Federation --->  [ Servers B ]
     |                                     |
     v                                     v
[ Clients A ]                         [ Clients B ]

每个 Region 独立运行自己的 Server 集群。
通过 Federation 实现跨 Region 作业转发和服务发现（较少使用）。
```

### 核心概念层次结构
```
Job                  ← 用户提交的最顶层声明式规范（desired state）
├── ID / Name
├── Type (service / batch / system / sysbatch)
├── Priority
├── Region / Datacenter constraints
│
└── Group(s)         ← 任务组（co-location 单元，必须同节点运行）
    ├── Count        ← 副本数（期望的 Allocation 数量）
    ├── Constraint / Affinity
    ├── Network (ports, mode: host / bridge)
    ├── Service (Consul / Nomad 注册，健康检查，可启用 Connect)
    │
    └── Task(s)      ← 最小执行单元
        ├── Driver (docker / exec / java / qemu / podman / raw_exec ...)
        ├── Config (image, command, args, env ...)
        ├── Resources (cpu, memory, disk)
        ├── Artifacts / Templates
        ├── Logs / Vault / Consul Template integration

运行时关键对象
   Allocation
      一个 Group 的具体运行实例（placement result）。
      一个 Job 的一个 Group + count = n → 会产生 n 个 Allocation。
      Allocation 被调度到特定 Client 上，包含该 Group 内所有 Task 的运行时信息。
   Evaluation
      调度触发器。当 Job 提交、更新、节点故障、资源变化时，Server 创建 Evaluation。
      Scheduler 根据 Evaluation 决定放置位置。
   Deployment
      Job 更新时的滚动部署对象。支持 canary、blue-green、auto_revert 等策略。
```

#### 组件职责说明

| 组件 | 角色 | 数量建议 | 主要职责 | 高可用机制 |
|------|------|----------|----------|------------|
| **Server** | Control Plane | 3–5 台 | 接收 Job、Raft 共识、调度决策、状态管理 | Raft Leader 选举 |
| **Client** | Data Plane | 成百上千台 | 执行 Task、资源上报、Driver 运行 | 无状态，可水平扩展 |
| **Driver** | 执行引擎 | — | 实际运行容器/进程/VM（docker/exec/java 等） | 依附于 Client |

### 调度流程简图
```
User → nomad job run job.nomad
          ↓
      Nomad Server (Leader)
          ↓
Create Job → Create Evaluation
          ↓
Scheduler (binpack / spread / custom)
          ↓
Find suitable Clients → Create Allocation(s)
          ↓
Send Allocation to Client(s)
          ↓
Client → Driver → Start Task(s)
          ↓
Client → Heartbeat + Status → Server
```

## 🎯 设计哲学与优势

| 特性 | 说明 |
|------|------|
| **单一二进制** | Server 和 Client 共用同一 binary，通过配置区分角色 |
| **原生多工作负载** | 无需插件即可运行容器、二进制、Java、VM 等 |
| **极简设计** | 相比 Kubernetes，配置文件更短，概念更少，学习曲线平缓 |
| **集成友好** | 原生支持 Consul（服务发现）、Vault（密钥）、Terraform（IaC） |

### Nomad agent
Nomad agent 是 HashiCorp Nomad 中最核心的运行实体，它是一个单一的可执行二进制文件（nomad），负责在节点上实现 Nomad 的全部功能。

```
用户 → CLI / API
       ↓
Nomad Server Agents (Raft 集群)  ← 这是 server 角色的 agent
       ↓
调度 → Allocation
       ↓
Nomad Client Agents              ← 这是 client 角色的 agent
       ↓
执行 Task (docker / exec / java ...)
```

简单来说：
**Nomad agent = Nomad 的运行进程**
通过不同的启动参数和配置，它可以扮演三种角色（或角色组合）：
1. Server 角色
2. Client 角色
3. Dev 模式（同时包含 Server + Client）

#### 关键点总结

- Nomad 集群中每一台参与的机器上都必须运行一个 Nomad agent 进程。
- 同一台机器上可以（但不推荐生产环境）同时启用 server 和 client
- agent 是 Nomad 的唯一运行形式：没有独立的 server 二进制或 client 二进制，一切都通过 nomad agent + 配置实现
- 生产环境推荐：
   - 3–5 台纯 server agent（不启用 client）
   - 大量纯 client agent（不启用 server）