# Python 对象流驱动（Object Flow Driven）心智模型

> **核心思想**
>
> Python 最大的优势不是语法，而是：
>
> **Everything is an Object**
>
> 程序真正流动的不是字符串、JSON、dict，而是**对象(Object)**。
>
> 整个系统，本质就是：
>
> ```
> Object
>     │
>     ▼
> Service
>     │
>     ▼
> Workflow
>     │
>     ▼
> Repository
>     │
>     ▼
> Infrastructure
> ```
>
> 从架构角度看，Python 更像：
>
> > **Object Flow Driven Programming（对象流驱动）**

---

# 一、Everything is an Object

很多语言的数据与行为是分开的。

例如 Shell：

```
stdout
↓

grep

↓

awk

↓

sed
```

数据只是文本。

而 Python：

```
Customer
Order
Switch
Device
Interface
Vlan
Route
Policy
```

这些全部都是对象。

例如：

```python
class Device:
    def __init__(self, hostname, ip):
        self.hostname = hostname
        self.ip = ip

    def connect(self):
        print(f"Connecting {self.hostname}")
```

创建对象：

```python
device = Device("leaf01", "10.1.1.1")
```

这里：

```
device
```

不是数据。

而是：

```
对象
    +
状态(state)
    +
行为(method)
```

所以 Python 世界中真正流动的是：

```
Device
```

而不是：

```
{
    "hostname": "...",
    "ip": "..."
}
```

---

## 心智模型

```
现实世界
      │
      ▼
领域对象(Object)
      │
      ▼
对象在系统中流动
```

不是：

```
JSON 在流动
```

而是：

```
Device
Interface
Route
Policy
```

在流动。

---

# 二、现实世界如何建模

假设真实网络：

```
+----------------------+
| Core Switch          |
+----------------------+
| hostname             |
| mgmt ip              |
| interfaces           |
| vlans                |
| routes               |
+----------------------+
```

Python：

```python
class Interface:

    def __init__(self, name, admin_up):
        self.name = name
        self.admin_up = admin_up


class Vlan:

    def __init__(self, vlan_id):
        self.vlan_id = vlan_id


class Device:

    def __init__(self, hostname, ip):
        self.hostname = hostname
        self.ip = ip

        self.interfaces = []
        self.vlans = []
```

然后：

```
Device
 ├── Interface
 ├── Interface
 ├── Interface
 ├── Vlan
 ├── Vlan
```

这就是：

> 面向领域建模（Domain Modeling）

---

## 心智模型

现实：

```
交换机
```

↓

Python：

```
Device
```

现实：

```
端口
```

↓

Python：

```
Interface
```

现实：

```
VLAN
```

↓

Python：

```
Vlan
```

不是：

```
dict
dict
dict
```

而是：

```
对象
```

---

# 三、对象如何流动（Object Flow）

假设：

```
Inventory
```

读取数据库。

Repository：

```python
class DeviceRepository:

    def load_devices(self):

        return [
            Device("leaf01", "10.1.1.1"),
            Device("leaf02", "10.1.1.2")
        ]
```

Workflow：

```python
repo = DeviceRepository()

devices = repo.load_devices()
```

得到：

```
devices

↓

Device
Device
```

接下来：

```
devices
```

继续流向：

```
HealthCheckService
```

---

Service：

```python
class HealthCheckService:

    def check(self, device):

        print(device.hostname)
```

Workflow：

```python
svc = HealthCheckService()

for d in devices:
    svc.check(d)
```

对象一直没有变化：

```
Repository

↓

Device

↓

Workflow

↓

Service

↓

Workflow
```

始终都是：

```
Device
```

不是：

```
JSON
```

---

## 心智模型

```
Device

↓

Health Service

↓

Backup Service

↓

Report Service
```

对象一直流动。

---

# 四、Service 之间对象流动

例如：

```
Inventory

↓

Health

↓

Config

↓

Backup

↓

Report
```

Python：

```python
devices = repo.load()

healthy = health.check(devices)

configured = config.deploy(healthy)

backup.save(configured)

report.generate(configured)
```

注意：

整个过程中：

```
Device
```

一直存在。

不是：

```
dict

↓

JSON

↓

XML

↓

dict
```

---

真正的数据流：

```
Device
    │
    ▼
Health
    │
    ▼
Device
    │
    ▼
Deploy
    │
    ▼
Device
    │
    ▼
Backup
```

---

# 五、Repository 的职责

Repository：

只负责：

```
数据库
REST
YAML
Redis
NetBox
```

转换成：

```
Device
```

例如：

```python
class DeviceRepository:

    def load(self):

        raw = [
            {
                "hostname":"leaf01",
                "ip":"10.1.1.1"
            }
        ]

        return [
            Device(x["hostname"], x["ip"])
            for x in raw
        ]
```

业务层：

永远不知道：

```
JSON
```

只知道：

```
Device
```

---

## 心智模型

Repository：

```
Infrastructure

↓

Object
```

而不是：

```
Infrastructure

↓

dict
```

---

# 六、Workflow 是对象流水线

Workflow：

```python
def run():

    devices = repo.load()

    devices = health.check(devices)

    devices = deploy.deploy(devices)

    report.generate(devices)
```

ASCII：

```
Repository

↓

Device List

↓

Health

↓

Device List

↓

Deploy

↓

Device List

↓

Report
```

Workflow：

不关心：

```
SSH
数据库
HTTP
```

只关心：

```
对象如何流动
```

---

# 七、业务与基础设施隔离

错误写法：

```python
def deploy():

    response = requests.get(...)

    data = response.json()

    print(data["hostname"])
```

业务直接依赖：

```
HTTP
JSON
```

应该：

Repository：

```python
class DeviceRepository:

    def load():

        response = requests.get(...)

        ...

        return Device(...)
```

业务：

```python
device = repo.load()

deploy(device)
```

于是：

业务完全不知道：

```
HTTP
```

不知道：

```
requests
```

不知道：

```
json
```

只认识：

```
Device
```

---

## 心智模型

```
Infrastructure

↓

Repository

↓

Object

↓

Service

↓

Workflow
```

---

# 八、真实网络自动化项目

假设：

每天：

```
NetBox

↓

SSH

↓

检查接口

↓

生成报告
```

架构：

```
          NetBox API
               │
               ▼
      DeviceRepository
               │
               ▼
      Device Objects
               │
               ▼
      InterfaceService
               │
               ▼
      ReportService
               │
               ▼
            Markdown
```

---

Repository：

```python
class DeviceRepository:

    def load(self):

        # NetBox REST API

        return [
            Device("leaf01","10.1.1.1"),
            Device("leaf02","10.1.1.2")
        ]
```

---

SSH Service：

```python
class InterfaceService:

    def collect(self, device):

        device.interfaces = [
            Interface("Eth1/1", True),
            Interface("Eth1/2", False)
        ]

        return device
```

---

Report：

```python
class ReportService:

    def generate(self, devices):

        for d in devices:

            print(d.hostname)

            for intf in d.interfaces:
                print(intf.name, intf.admin_up)
```

Workflow：

```python
repo = DeviceRepository()

collector = InterfaceService()

report = ReportService()

devices = repo.load()

devices = [
    collector.collect(d)
    for d in devices
]

report.generate(devices)
```

整个过程中：

```
Device
```

一直存在。

---

数据流：

```
REST

↓

Repository

↓

Device

↓

SSH

↓

Device

↓

Report

↓

Markdown
```

---

# 九、进一步演进：面向对象流水线

可以把 Workflow 看成对象流水线：

```python
devices = (
    repo.load()
)

devices = (
    health.check(devices)
)

devices = (
    deploy.push(devices)
)

devices = (
    backup.save(devices)
)

report.generate(devices)
```

对应：

```
Object

↓

Transform

↓

Transform

↓

Transform

↓

Output
```

每一步：

输入：

```
Object
```

输出：

```
Object
```

与 Linux Pipeline 的区别：

Linux：

```
Text

↓

Text

↓

Text
```

Python：

```
Object

↓

Object

↓

Object
```

---

# 十、完整项目目录示例

```
network_automation/

├── domain/
│   ├── device.py
│   ├── interface.py
│   └── vlan.py
│
├── repositories/
│   ├── netbox_repository.py
│   └── inventory_repository.py
│
├── services/
│   ├── ssh_service.py
│   ├── health_service.py
│   ├── deploy_service.py
│   └── report_service.py
│
├── workflows/
│   └── daily_audit.py
│
├── infrastructure/
│   ├── ssh_client.py
│   ├── http_client.py
│   └── database.py
│
└── main.py
```

---

# 十一、Object Flow Driven 心智模型总结

## 1. Everything is an Object

```
现实世界
      │
      ▼
领域对象(Object)
```

对象封装了数据与行为，是系统中的基本单元。

---

## 2. Domain Modeling（领域建模）

```
现实实体
    │
    ▼
Device
Interface
Route
Policy
Vlan
```

避免在业务层直接操作裸 `dict` 或 JSON。

---

## 3. Object Flow（对象流）

```
Repository
      │
      ▼
Device
      │
      ▼
Service A
      │
      ▼
Service B
      │
      ▼
Workflow
```

对象在各层之间持续流动，保持领域语义。

---

## 4. Repository（基础设施隔离）

```
REST API / DB / YAML
          │
          ▼
Repository
          │
          ▼
Domain Object
```

Repository 屏蔽数据来源，使业务层仅依赖对象。

---

## 5. Service（业务规则）

```
Object
   │
   ▼
Business Logic
   │
   ▼
Object
```

Service 不关心数据如何获取，只负责业务处理。

---

## 6. Workflow（对象流水线）

```
Object
   │
   ▼
Health Check
   │
   ▼
Deploy
   │
   ▼
Backup
   │
   ▼
Report
```

Workflow 负责编排对象的流转顺序。

---

## 7. Infrastructure（基础设施）

```
SSH
REST
Database
Filesystem
```

所有外部依赖都应封装在基础设施层，不泄漏到业务层。

---

## 8. 与 Linux Pipeline 的对比

| Linux Pipeline | Python Object Flow |
|---------------|--------------------|
| 文本（Text）流动 | 对象（Object）流动 |
| `grep` / `awk` / `sed` | Service / Repository / Workflow |
| `stdin/stdout` | 方法参数 / 返回值 |
| 面向字节流 | 面向领域对象 |
| 管道连接命令 | 对象连接业务逻辑 |

---

## 9. 核心心智模型（Architecture View）

```text
                  Real World
                       │
                       ▼
               Domain Modeling
                       │
                       ▼
                 Domain Objects
                       │
          ┌────────────┼────────────┐
          ▼            ▼            ▼
     Repository     Service     Workflow
          │            │            │
          └────────────┼────────────┘
                       ▼
              Object Flow Pipeline
                       │
                       ▼
               Infrastructure
        (REST / SSH / DB / File)
```

> **一句话总结：**
>
> Python 的架构设计，应当让**领域对象（Object）成为系统中的第一公民**。Repository 将外部数据转换为对象，Service 在对象上实现业务规则，Workflow 编排对象的流动，而基础设施负责与外部世界交互。整个系统围绕 **Object Flow** 展开，而不是围绕 JSON、字符串或数据库记录展开。