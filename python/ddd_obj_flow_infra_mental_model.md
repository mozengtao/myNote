# Python：DDD + Object Flow + Infrastructure Layer 的组织心智模型

> 适用于：
>
> - 网络自动化
> - 运维平台
> - 云平台
> - 自动化测试
> - AI Agent
> - Backend Service
> - DevOps Tool

整个系统可以用一句话概括：

> **DDD 决定"对象是什么"，Object Flow 决定"对象如何流动"，Infrastructure Layer 决定"对象如何和外界通信"。**

可以把整个程序理解成：

```
                Business World
                     │
                     ▼
             Domain Objects (DDD)
                     │
                     ▼
            Object Flow (Workflow)
                     │
                     ▼
         Infrastructure Layer
                     │
                     ▼
     SSH / HTTP / DB / Kafka / Redis
                     │
                     ▼
                Real World
```

整个系统实际上就是：

> **现实世界 → Domain Object → Object Flow → Infrastructure → 现实世界**

---

# 一、整个项目的分层

推荐采用下面这种组织方式。

```
project/

    domain/
        device.py
        vmc.py
        server.py
        topology.py

    service/
        reboot.py
        deploy.py
        healthcheck.py

    repository/
        vmc_repository.py
        server_repository.py

    workflow/
        reboot_workflow.py
        deploy_workflow.py

    infrastructure/
        ssh.py
        rest.py
        database.py
        kafka.py
        logger.py

    config/
        config.py

    main.py
```

可以理解成：

```
            Domain Layer
               │
            描述现实世界

               ▼

            Service Layer
             描述业务规则

               ▼

          Workflow Layer
            描述业务流程

               ▼

         Infrastructure Layer
          描述如何访问世界
```

这四层几乎覆盖所有 Python 工程。

---

# 二、DDD：负责建模

DDD 最大职责只有一句话：

> **把现实世界变成对象。**

例如：

现实世界：

```
Server

Host

Device

Interface

Port

User

Policy

Route
```

全部变成对象：

```
Server()

Device()

Interface()

Policy()

Route()
```

例如：

```
Server

├── hostname
├── ip
├── version
├── vmcs
└── reboot()
```

Python：

```python
class Server:

    def __init__(self, hostname, ip):
        self.hostname = hostname
        self.ip = ip
        self.vmcs = []

    def reboot(self):
        ...
```

注意：

Server 并不知道 SSH。

也不知道 HTTP。

更不知道数据库。

它只是：

> **现实世界 Server 的抽象。**

---

# 三、Domain Object 应该保存什么？

只保存：

```
状态(State)

+

行为(Behavior)
```

例如：

```
Device

状态：

hostname

ip

status

Behavior:

connect()

reboot()

upgrade()

health_check()
```

不要保存：

```
SSHClient

Database

Requests

Redis

Kafka
```

那些属于 Infrastructure。

---

# 四、Service Layer：业务规则

很多人误解 Service。

Service 不负责：

```
SSH

HTTP

SQL
```

Service 真正负责：

> **业务规则（Business Rule）**

例如：

```
重启设备：

如果：

offline

↓

不能重启

如果：

master

↓

先切 slave

↓

再 reboot
```

这些都是：

业务规则。

例如：

```python
class RebootService:

    def reboot(self, vmc):

        if not vmc.online:
            raise Exception()

        if vmc.master:
            vmc.switch_slave()

        vmc.reboot()
```

注意：

这里没有 SSH。

只有：

```
业务规则
```

---

# 五、Workflow：Object Flow

Workflow 是很多 Python 项目的灵魂。

一句话：

> **Workflow 决定对象如何流动。**

例如：

```
读取 Server

↓

找到 Device

↓

找到 VMC

↓

生成 Command

↓

SSH

↓

返回 Result

↓

更新对象

↓

输出 Report
```

画出来：

```
Server
    │

    ▼

Device
    │

    ▼

VMC
    │

    ▼

Command
    │

    ▼

Result
    │

    ▼

Report
```

注意：

整个 Workflow 都是在：

> **对象之间流动。**

不是字符串流动。

---

# 六、Object Flow 的本质

Object Flow：

就是：

```
Object A

↓

Object B

↓

Object C

↓

Object D
```

例如：

```
Inventory

↓

Server

↓

VMC

↓

SSHCommand

↓

SSHResult

↓

HealthReport
```

几乎所有 Python 自动化都是这样。

ASCII：

```
Inventory
      │
      ▼
 Server
      │
      ▼
 Device
      │
      ▼
 Command
      │
      ▼
 Result
      │
      ▼
 Report
```

对象越来越具体。

信息越来越丰富。

---

# 七、Infrastructure Layer

Infrastructure：

一句话：

> **负责和外界通信。**

例如：

```
SSH

HTTP

Redis

Kafka

MySQL

Filesystem

Email
```

全部属于：

```
Infrastructure
```

例如：

```
SSHClient
```

负责：

```
send()

recv()

connect()
```

例如：

```python
class SSHClient:

    def execute(self, command):
        ...
```

SSHClient：

不知道：

```
VMC

Device

Policy

Workflow
```

它只知道：

```
Command
```

---

# 八、Infrastructure 不应该知道业务

例如：

错误：

```python
class SSHClient:

    def reboot_vmc(self):
        ...
```

为什么？

因为：

SSH 不知道：

```
什么叫 VMC
```

SSH 应该只有：

```python
execute(command)
```

这样：

Infrastructure：

保持：

```
Generic
```

业务：

放到：

Service。

---

# 九、Repository 的职责

Repository：

负责：

> **对象的来源。**

例如：

```
数据库

↓

Server Object
```

或者：

```
REST API

↓

Device Object
```

Repository：

负责：

```
JSON

↓

Object
```

例如：

```
API

↓

JSON

↓

Device()
```

Workflow 永远不要解析 JSON。

Workflow 只拿：

```
Device
```

---

# 十、Infrastructure 与 Repository 的关系

很多人混淆。

实际上：

```
Infrastructure

负责：

HTTP GET

Repository

负责：

JSON

↓

Object
```

例如：

```
HTTPClient

↓

GET

↓

JSON

↓

Repository

↓

Device()
```

ASCII：

```
HTTP

↓

JSON

↓

Repository

↓

Device
```

---

# 十一、完整 Object Flow

例如：

```
Inventory API

↓

JSON

↓

Repository

↓

Server

↓

Workflow

↓

Device

↓

Service

↓

Command

↓

SSH

↓

Result

↓

Device

↓

Report
```

画出来：

```
Inventory
      │
      ▼
 Repository
      │
      ▼
  Server Object
      │
      ▼
 Workflow
      │
      ▼
 Device Object
      │
      ▼
 Service
      │
      ▼
 Command
      │
      ▼
 SSH Client
      │
      ▼
 Result
      │
      ▼
 Device
      │
      ▼
 Report
```

这是 Python 自动化项目最经典的数据流。

---

# 十二、每层关注点（Separation of Concerns）

```
Domain

关注：

对象是什么

-------------------------

Service

关注：

业务规则是什么

-------------------------

Workflow

关注：

业务如何执行

-------------------------

Repository

关注：

对象来自哪里

-------------------------

Infrastructure

关注：

如何访问外部系统
```

对应一句话：

```
Domain
What

↓

Service
Rule

↓

Workflow
Flow

↓

Repository
Source

↓

Infrastructure
Connection
```

---

# 十三、依赖关系（Dependency Direction）

推荐依赖方向：

```
Workflow
    │
    ▼
Service
    │
    ▼
Repository
    │
    ▼
Infrastructure

Domain
↑
所有层都依赖 Domain
```

更完整表示为：

```
                 Domain
                    ▲
        ┌───────────┼───────────┐
        │           │           │
        │           │           │
 Repository      Service     Workflow
        │           ▲           │
        └───────────┴───────────┘
                    │
                    ▼
            Infrastructure
```

原则：

- **Domain** 不依赖任何业务实现或基础设施。
- **Service** 依赖 Domain，表达业务规则。
- **Workflow** 编排 Service、Repository，驱动对象流。
- **Repository** 利用 Infrastructure 获取/保存数据，并负责对象重建。
- **Infrastructure** 提供通用技术能力，不包含业务语义。

---

# 十四、最终心智模型（Architecture Mind Map）

```
                Real World
                     │
                     ▼
            Domain Modeling
                     │
         (Everything becomes Object)
                     │
                     ▼
             Domain Objects
                     │
                     ▼
          Service (Business Rule)
                     │
                     ▼
         Workflow (Object Flow)
                     │
                     ▼
 Repository (Load / Save Objects)
                     │
                     ▼
 Infrastructure (SSH/HTTP/DB/Kafka)
                     │
                     ▼
              External Systems
```

可以将整个架构浓缩为一句口诀：

> **Domain 建模世界（What），Service 定义规则（Rule），Workflow 驱动对象流（Flow），Repository 管理对象来源与持久化（Source），Infrastructure 屏蔽技术细节（Connection）。对象始终作为系统中的核心载体，在各层之间流动，而字符串、JSON、SQL、SSH 命令等都只是对象流动过程中的中间表示。**