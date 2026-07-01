# Python Domain Modeling 心智模式（Domain-Driven Object Modeling）

> 核心思想：
>
> **不是围绕数据库建模，也不是围绕API建模，而是围绕"现实世界中的业务对象（Domain Object）"建模。**
>
> Python 天然是一门 Object-Oriented + Duck Typing + Composition Friendly 的语言，因此特别适合 Domain Modeling。

---

# 一、什么是 Domain Modeling

很多初学者理解 Python：

```
Python
    ↓
函数
    ↓
调用数据库
    ↓
返回JSON
```

但是大型 Python 项目几乎不是这样。

真正的软件世界更像：

```
现实世界
      │
      ▼
Domain Object
      │
      ▼
Business Rule
      │
      ▼
Workflow
      │
      ▼
Infrastructure
(Database/API/File/CLI)
```

也就是说：

**现实世界 → 建模 → 对象流动 → 业务规则**

而不是

```
数据库
   ↓
SQL
   ↓
if else
```

---

# 二、Domain Modeling 的第一原则

> 一切业务都应该先找到"对象（Object）"

例如网络自动化：

现实世界：

```
Router
Switch
Interface
VLAN
BGP Neighbor
VRF
ACL
Policy
Device
```

这些都应该成为 Python 对象。

例如：

```python
class Interface:

    def __init__(self, name, ip, status):
        self.name = name
        self.ip = ip
        self.status = status
```

以后所有代码，都围绕 Interface 展开。

而不是：

```python
{
    "name": "...",
    "ip": "...",
    "status": "..."
}
```

对象比 dict 更有语义。

---

# 三、对象不是数据库记录

很多人误以为：

```
数据库一行
      ==
Python Object
```

其实不是。

真正关系：

```
Database
      │
      ▼
Repository
      │
      ▼
Domain Object
      │
      ▼
Business Logic
```

例如：

数据库：

```
interfaces

id
name
ip
status
```

Repository：

```python
class InterfaceRepository:

    def get(self, name):
        ...
```

Repository 返回：

```
Interface
```

而不是：

```
dict
```

---

# 四、Domain Object 才是真正的业务语言

例如：

不要写：

```python
if interface["status"] == "up":
```

而应该：

```python
if interface.is_up():
```

对象自己知道：

```
我是不是UP
```

例如：

```python
class Interface:

    def is_up(self):
        return self.status == "up"
```

业务代码：

```python
if iface.is_up():
    ...
```

可读性立即提升。

---

# 五、对象拥有自己的行为（Behavior）

很多人对象只有数据：

```
Interface

name
ip
status
```

这是贫血模型（Anemic Model）。

真正 Domain Object：

```
Interface

name
ip
status

shutdown()
enable()

is_up()

change_ip()

summary()
```

例如：

```python
class Interface:

    def shutdown(self):
        self.status = "down"

    def enable(self):
        self.status = "up"

    def is_up(self):
        return self.status == "up"
```

对象自己维护自己的状态。

---

# 六、对象之间组成现实世界

现实：

```
Router
    │
    ├────Interface
    ├────Interface
    └────Interface
```

Python：

```python
class Router:

    def __init__(self, hostname):
        self.hostname = hostname
        self.interfaces = []
```

添加接口：

```python
router.interfaces.append(
    Interface("eth0", "10.0.0.1", "up")
)
```

整个对象树：

```
Router

 ├── Interface
 ├── Interface
 ├── Interface
```

就是现实世界。

---

# 七、对象之间不断流动（Object Flow）

真正 Python 项目：

```
Repository
      │
      ▼
Device Object
      │
      ▼
Workflow
      │
      ▼
Service
      │
      ▼
Repository
```

例如：

```
读取设备

↓

Device

↓

Health Check

↓

Device

↓

Generate Report

↓

Save
```

对象不断流动。

而不是：

```
dict

↓

dict

↓

dict
```

---

# 八、示例一：网络设备巡检

Repository：

```python
class DeviceRepository:

    def list_devices(self):
        return [
            Device("R1"),
            Device("R2")
        ]
```

Domain：

```python
class Device:

    def __init__(self, hostname):
        self.hostname = hostname
        self.interfaces = []

    def add_interface(self, interface):
        self.interfaces.append(interface)

    def active_interfaces(self):
        return [
            i
            for i in self.interfaces
            if i.is_up()
        ]
```

Workflow：

```python
repo = DeviceRepository()

devices = repo.list_devices()

for dev in devices:

    active = dev.active_interfaces()

    print(dev.hostname, len(active))
```

对象流：

```
Repository

↓

Device

↓

Workflow

↓

Report
```

---

# 九、示例二：BGP Domain Modeling

现实：

```
Router

    ↓

Neighbor

    ↓

Session
```

Python：

```python
class Neighbor:

    def __init__(self, ip, state):
        self.ip = ip
        self.state = state

    def established(self):
        return self.state == "Established"
```

Router：

```python
class Router:

    def __init__(self):
        self.neighbors = []

    def healthy_neighbors(self):
        return [
            n
            for n in self.neighbors
            if n.established()
        ]
```

业务：

```python
healthy = router.healthy_neighbors()
```

没有任何 SQL。

没有 JSON。

没有 dict。

都是对象。

---

# 十、示例三：配置生成

Domain：

```python
class Interface:

    def __init__(self, name, ip):
        self.name = name
        self.ip = ip

    def render(self):
        return f"""
interface {self.name}
 ip address {self.ip}
"""
```

Workflow：

```python
cfg = ""

for iface in interfaces:
    cfg += iface.render()
```

对象自己知道如何生成配置。

---

# 十一、示例四：策略建模

现实：

```
ACL

↓

Rule

↓

Match
```

Python：

```python
class Rule:

    def __init__(self, src, dst):
        self.src = src
        self.dst = dst

    def match(self, packet):
        return (
            packet.src == self.src
            and packet.dst == self.dst
        )
```

ACL：

```python
class ACL:

    def __init__(self):
        self.rules = []

    def allow(self, packet):

        return any(
            rule.match(packet)
            for rule in self.rules
        )
```

对象之间互相协作。

---

# 十二、Workflow 不关心数据来源

Workflow：

```python
devices = repo.list_devices()

for d in devices:
    service.check(d)
```

它不知道：

```
SQLite？

Redis？

NetBox？

REST API？

YAML？

CSV？
```

只知道：

```
Device
```

因此：

```
Infrastructure
      │
      ▼
Repository
      │
      ▼
Device
      │
      ▼
Workflow
```

解耦完成。

---

# 十三、完整对象流（Object Flow）

```
                API
                 │
                 │
                CLI
                 │
                 │
             Database
                 │
                 │
            Repository
                 │
        Device Object
                 │
      ┌──────────┼──────────┐
      │          │          │
 Interface   Neighbor    Route
      │          │          │
      └──────────┼──────────┘
                 │
          Domain Service
                 │
                 ▼
            Workflow
                 │
        Report / Config
                 │
             Repository
```

这里真正流动的是：

```
Device

↓

Interface

↓

Neighbor

↓

Route

↓

ACL

↓

Policy

↓

Report
```

整个系统都是：

> Object Stream（对象流）

而不是：

```
JSON

↓

dict

↓

dict

↓

dict
```

---

# 十四、Python Domain Modeling 的设计原则

| 原则 | 推荐做法 | 避免做法 |
|------|----------|----------|
| 业务建模 | 围绕现实世界对象（Device、Interface、Policy）建模 | 围绕数据库表或 JSON 字段建模 |
| 数据表示 | 使用具有明确语义的 Domain Object | 在系统中长期传递 `dict` |
| 行为归属 | 将业务规则放入对象方法中 | 所有逻辑集中在 `if/else` 和工具函数中 |
| 对象关系 | 使用组合（Composition）表达整体与部分 | 通过多个无关联的字典手工维护关系 |
| 数据访问 | Repository 负责对象创建与持久化 | 在业务逻辑中直接编写 SQL 或 HTTP 请求 |
| 工作流 | Workflow 编排对象协作 | Workflow 操作原始数据结构 |
| 基础设施 | Infrastructure 与 Domain 解耦 | Domain 依赖数据库、网络 API 等实现细节 |
| 状态管理 | 对象维护自身状态与一致性 | 外部代码任意修改对象内部字段 |
| 可测试性 | 使用 Domain Object + Repository 接口便于 Mock | 业务逻辑与数据库、网络强耦合 |
| 核心心智 | **Model the Business, Let Objects Flow（先建模，再让对象流动）** | **Process Raw Data Everywhere（到处处理原始数据）** |

---

# 十五、建立 Domain Modeling 心智模型

```
现实业务
    │
    ▼
识别领域对象（Entity / Value Object）
    │
    ▼
定义对象的属性与行为
    │
    ▼
建立对象之间的组合关系
    │
    ▼
通过 Repository 获取对象
    │
    ▼
Workflow 编排对象协作
    │
    ▼
Service 承载跨对象业务规则
    │
    ▼
Infrastructure 负责持久化与外部系统交互
```

最终形成的思维方式可以概括为：

> **以业务对象为中心（Object-Centric），以对象流（Object Flow）驱动业务流程（Workflow），通过 Repository 隔离基础设施，通过 Service 组织复杂业务规则，从而构建一个高内聚、低耦合、易测试、易扩展的领域模型。**