# Python 复杂 Object 组织心智模式（Complex Object Organization）

> 核心思想：
>
> **不要思考代码如何写，而要思考现实世界中的对象如何存在、如何关联、如何流动。**
>
> Python 最大的优势并不是语法，而是：
>
> > **能够把复杂业务建模成一棵对象树（Object Graph），然后让对象在不同层之间流动。**

---

# 一、复杂 Object 的四层心智模型

```
                Business World
                      │
                      ▼
          ┌──────────────────────┐
          │     Domain Object    │
          │ User Order Device ...│
          └──────────────────────┘
                      │
             Object Composition
                      │
                      ▼
          ┌──────────────────────┐
          │     Object Graph     │
          └──────────────────────┘
                      │
            Business Operation
                      │
                      ▼
          ┌──────────────────────┐
          │     Object Flow      │
          └──────────────────────┘
                      │
             Persistence / API
                      ▼
```

复杂系统其实就是：

> **Object + Relationship + Flow**

而不是：

```
Function + Dict + Global Variable
```

---

# 二、第一层：Object First（先建对象）

不要先写函数。

先问：

现实世界有哪些对象？

例如网络自动化：

```
Router
Interface
VRF
BGPNeighbor
Route
ACL
Policy
```

而不是：

```
show_interface()
show_route()
parse()
```

例如：

```python
class Interface:
    def __init__(self, name, ip):
        self.name = name
        self.ip = ip
```

Router：

```python
class Router:
    def __init__(self, hostname):
        self.hostname = hostname
        self.interfaces = []
```

然后：

```
Router
 ├── Interface
 ├── Interface
 ├── Interface
```

而不是：

```
router_dict = {
    "hostname":...
}
```

对象天然就是现实世界。

---

# 三、Object 不应该孤立存在

真正的系统：

对象一定组成网络。

例如：

```
User
 ├── Orders
 │      ├── Items
 │      ├── Items
 │      └── Items
 │
 └── Address
```

Python：

```python
class User:
    def __init__(self, name):
        self.name = name
        self.orders = []
```

Order：

```python
class Order:
    def __init__(self):
        self.items = []
```

Item：

```python
class Item:
    def __init__(self, name):
        self.name = name
```

构建：

```python
user = User("Alice")

order = Order()

order.items.append(Item("Apple"))
order.items.append(Item("Orange"))

user.orders.append(order)
```

对象图：

```
User
 │
 ├────Order
 │      │
 │      ├──Item
 │      └──Item
```

这就是：

> Object Graph

---

# 四、Composition（组合）优先

Python 推荐：

> Has-A

而不是：

> Is-A

例如：

不要：

```python
class Router(Device):
    ...
```

更推荐：

```python
class Router:
    def __init__(self):
        self.interfaces = []
        self.vrfs = []
        self.routes = []
```

Router：

拥有：

```
Interface

VRF

BGP

OSPF

ACL
```

而不是继承一堆。

复杂系统基本都是：

```
Composition
```

不是：

```
Inheritance
```

---

# 五、一层对象只负责一种抽象

例如：

错误：

```python
Router

里面既有：

SSH

JSON

CLI

业务

数据库
```

正确：

```
Router
    │
    ▼
Interface

VRF

Neighbor

Route
```

Router：

不知道 SSH。

Router：

不知道数据库。

Router：

只描述：

> Router 是什么。

---

# 六、Object Graph（对象图）

例如：

```
DC
 ├── Router
 │      ├── Interface
 │      ├── Interface
 │      └── BGP
 │
 ├── Router
 │      ├── Interface
 │      └── OSPF
 │
 └── Switch
```

Python：

```python
class Datacenter:
    def __init__(self):
        self.devices = []
```

Device：

```python
class Device:
    def __init__(self):
        self.interfaces = []
```

最终：

```
Datacenter
      │
      ├────Router
      │      │
      │      ├──Interface
      │      ├──Interface
      │      └──Neighbor
      │
      └────Switch
```

整个系统：

就是：

一张对象图。

---

# 七、对象应该自己管理自己

例如：

错误：

```python
router.interfaces.append(...)
```

到处写。

推荐：

```python
class Router:

    def add_interface(self, interface):
        self.interfaces.append(interface)
```

以后：

```python
router.add_interface(iface)
```

为什么？

以后：

可以：

```
检查重复

记录日志

维护索引

更新缓存
```

全部封装。

对象负责维护自己的状态。

---

# 八、复杂对象不要暴露内部结构

例如：

不要：

```python
router.interfaces[0].neighbors[3].ip
```

这种：

调用者知道太多。

推荐：

```python
router.get_neighbor("10.0.0.1")
```

或者：

```python
router.find_interface("eth0")
```

对象负责查找。

而不是外部。

---

# 九、Object Factory

复杂对象不要自己 new。

例如：

错误：

```python
router = Router()

router.interfaces.append(...)

router.routes.append(...)
```

推荐：

```python
router = RouterFactory.create_from_json(data)
```

Factory：

负责：

```
解析JSON

创建对象

建立引用

检查合法性
```

业务：

不用关心。

例如：

```python
router = RouterFactory.create(cli_output)
```

得到：

```
完整对象图
```

---

# 十、Builder

对象非常复杂时：

Builder 更合适。

例如：

```python
builder = DeviceBuilder()

builder.add_interface(...)

builder.add_route(...)

builder.add_vrf(...)

device = builder.build()
```

Builder：

逐步构造。

适合：

CLI

YAML

JSON

XML

转换。

---

# 十一、对象之间引用

例如：

Interface：

属于：

Router

```
Router
     │
     ▼
 Interface
```

Python：

```python
class Interface:

    def __init__(self, router):
        self.router = router
```

这样：

Interface：

可以：

```
self.router.hostname
```

对象互相引用。

形成：

```
Object Graph
```

而不是：

Tree。

---

# 十二、避免 God Object

错误：

```
Router

10000 行

SSH

REST

CLI

Database

Logger

Scheduler

Parser

BGP

OSPF

VRF
```

这是：

God Object。

正确：

```
Router

Interface

Route

Neighbor

ACL

Policy
```

每个对象：

负责：

一种概念。

---

# 十三、Repository 管理对象

Repository：

负责：

```
对象集合
```

例如：

```python
class RouterRepository:

    def __init__(self):
        self.routers = {}
```

增加：

```python
repo.add(router)
```

查询：

```python
repo.find(hostname)
```

业务：

永远：

不要：

```
router_list[10]
```

应该：

```
repo.find(...)
```

---

# 十四、Service 不拥有对象

Service：

只：

消费对象。

例如：

```python
class DeployService:

    def deploy(self, router):
        ...
```

而不是：

```
class DeployService:

    self.router = ...
```

Service：

无状态。

对象：

一直流动。

---

# 十五、Workflow 驱动 Object Flow

Workflow：

组织：

```
Load
 ↓
Build Object
 ↓
Validate
 ↓
Transform
 ↓
Deploy
 ↓
Report
```

例如：

```python
router = repo.load()

validator.validate(router)

generator.generate(router)

deployer.deploy(router)
```

对象：

一直：

向下流。

没有：

Dict。

没有：

Global。

没有：

共享变量。

---

# 十六、网络自动化完整示例

```
Inventory YAML
        │
        ▼
InventoryLoader
        │
        ▼
RouterFactory
        │
        ▼
Router
 ├──Interface
 ├──VRF
 ├──Neighbor
 └──Route
        │
        ▼
Validator
        │
        ▼
Renderer
        │
        ▼
SSHService
        │
        ▼
Device
```

Python：

```python
inventory = InventoryLoader.load("inventory.yaml")

router = RouterFactory.build(inventory)

Validator().validate(router)

commands = ConfigRenderer().render(router)

SSHService().deploy(router, commands)
```

整个过程中：

流动的是：

```
Router Object
```

而不是：

```
dict
```

---

# 十七、复杂 Object 的组织原则（Checklist）

| 原则 | 推荐做法 | 避免 |
|------|----------|------|
| Object First | 先建领域对象，再写逻辑 | 先写函数、后补对象 |
| Composition | 使用组合（Has-A）组织对象 | 深层继承（Is-A） |
| Object Graph | 将对象组织成树或图 | 大量独立对象、全局变量 |
| Single Responsibility | 每个对象只负责一种领域概念 | God Object（万能对象） |
| Encapsulation | 对象管理自己的状态和行为 | 外部直接修改内部属性 |
| Hide Structure | 提供领域方法访问对象 | 长链式访问内部结构 |
| Factory / Builder | 统一创建复杂对象 | 到处手工实例化和拼装 |
| Repository | 集中管理对象生命周期与查询 | 到处传递列表、字典 |
| Service | 无状态，消费对象完成业务 | Service 持有和管理对象状态 |
| Object Flow | 对象在 Loader → Validator → Service → Renderer → Deployer 间流动 | 各层频繁转换为 dict 或全局共享数据 |

---

# 十八、最终心智模型

```
                 Real World
                      │
                      ▼
              Domain Modeling
                      │
                      ▼
               Domain Objects
                      │
              (Composition)
                      ▼
                 Object Graph
                      │
              (Object Flow)
                      ▼
 Loader → Factory → Repository → Service → Workflow
                      │
                      ▼
             Infrastructure Layer
        (Database / REST / SSH / CLI)
```

可以将 Python 复杂对象组织总结为一句话：

> **以领域对象（Domain Object）为中心，通过组合（Composition）构建对象图（Object Graph），再让对象沿着业务流程（Object Flow）在 Loader、Repository、Service、Workflow 和 Infrastructure 之间流动，而不是在各层之间频繁传递 `dict`、全局变量或零散数据。**

---

# 十九、五个不同场景的完整代码示例

前面所有小节都只用了两个场景讲心智模型：网络自动化（`Router` / `Interface` / `VRF`）和电商下单（`User` / `Order` / `Item`）。

为了说明这套心智模型具有跨领域的普适性，[`python/complex_obj_organization_examples/`](complex_obj_organization_examples) 目录下补充了 5 个**完全不同场景**的完整可运行 Python 示例。每个示例都是独立子目录，包含 `main.py`（完整代码，关键设计决策处有注释）和 `README.md`（场景说明 + 对应的心智模型原则）。

| 示例 | 场景 | 重点演示的心智模型原则 |
|---|---|---|
| [`library_lending_system`](complex_obj_organization_examples/library_lending_system) | 图书馆借阅系统 | Object Graph、Encapsulation（对象自己管理状态）、Repository |
| [`smart_home_control`](complex_obj_organization_examples/smart_home_control) | 智能家居设备控制 | Composition（Has-A）、Hide Structure、Single Responsibility |
| [`bank_account_transfer`](complex_obj_organization_examples/bank_account_transfer) | 银行账户转账 | Encapsulation（余额只能通过方法修改）、Service 无状态、Repository |
| [`restaurant_order_kitchen`](complex_obj_organization_examples/restaurant_order_kitchen) | 餐厅点单与厨房工作流 | Factory、Object Flow、Workflow 驱动 Object Flow |
| [`company_org_payroll`](complex_obj_organization_examples/company_org_payroll) | 公司组织架构与工资单 | Builder、避免 God Object、Composition |

这 5 个原则均对应本文第十七节的 Checklist 表格。运行方式：

```bash
cd python/complex_obj_organization_examples/<示例目录名>
python3 main.py
```

---

# 二十、分层架构完整示例：DDD + Object Flow + Infrastructure Layer

前面的示例都把所有对象放在同一个文件里。当项目规模变大、并且明确要对接外部系统（例如通过 SSH 操作网络设备）时，通常会进一步按**层**拆成多个模块。这是网络自动化项目中非常常见的组织方式。

## 推荐的心智模型

按照 DDD + Object Flow + Infrastructure Layer 去组织：

```
Application
    |
    v
Workflow
    |
    v
Domain Object
    |
    v
Infrastructure
    |
    v
Paramiko
```

对应目录：

```
project/
+-- app.py                      # 程序入口
|
+-- infrastructure/
|   +-- ssh_client.py
|   +-- interactive_shell.py
|
+-- domain/
|   +-- vmc.py
|   +-- vmc_service.py
|
+-- workflow/
    +-- reboot_workflow.py
```

整个程序变成：

```
Application
    |
    v
Workflow
    |
    v
VMCService
    |
    v
InteractiveShell
    |
    v
Paramiko
```

## 第一层：Infrastructure（基础设施层）——只负责 SSH

这一层只知道怎么建立 SSH 连接、怎么打开一个 shell，完全不知道什么是 VMC，也不知道任何业务命令。

```python
# infrastructure/ssh_client.py
import paramiko


class SSHConnection:

    def __init__(self, host, user, password):
        self.host = host
        self.user = user
        self.password = password
        self.client = None

    def connect(self):
        self.client = paramiko.SSHClient()
        self.client.set_missing_host_key_policy(
            paramiko.AutoAddPolicy()
        )
        self.client.connect(
            hostname=self.host,
            username=self.user,
            password=self.password,
            allow_agent=False,
            look_for_keys=False,
        )

    def open_shell(self):
        return self.client.invoke_shell(width=220)

    def close(self):
        self.client.close()
```

这里只有 SSH Connection，没有任何业务。

## 第二层：Interactive Shell——负责人机交互协议

负责 `send()` / `recv_until()` / `drain()`，仍然完全不知道 VMC。

```python
# infrastructure/interactive_shell.py
import time


class InteractiveShell:

    def __init__(self, channel):
        self.channel = channel

    def send(self, cmd):
        self.channel.send(cmd + "\n")

    def recv_until(self, marker, timeout=30):
        buf = ""
        deadline = time.time() + timeout

        while time.time() < deadline:
            if self.channel.recv_ready():
                chunk = self.channel.recv(4096).decode()
                print(chunk, end="")
                buf += chunk
                if marker in buf:
                    return buf
            time.sleep(0.1)

        raise TimeoutError(marker)

    def drain(self):
        time.sleep(1)
        while self.channel.recv_ready():
            print(self.channel.recv(4096).decode(), end="")
```

## 第三层：Domain——第一次出现"VMC 是什么"

```python
# domain/vmc.py
from dataclasses import dataclass


@dataclass
class VMC:
    name: str
```

它只是业务对象，没有 SSH。

## 第四层：Domain Service——表达"VMC 能做什么"

这里开始表达"VMC 能做什么"，而不是"SSH 怎么发"。

```python
# domain/vmc_service.py
class VMCService:

    PROMPT = "Are you sure?"

    def __init__(self, shell):
        self.shell = shell

    def reboot(self, vmc):
        print("=" * 60)
        print("Reboot", vmc.name)
        print("=" * 60)

        self.shell.send(
            f"vmc {vmc.name} reboot keep-current-version false"
        )
        self.shell.recv_until(self.PROMPT)

        self.shell.send("yes")
        self.shell.drain()
```

这里完全没有 `recv()`、`channel`、`socket`，业务非常干净。

## 第五层：Workflow——表示整个流程

```
连接
  |
  v
登录
  |
  v
遍历
  |
  v
重启
  |
  v
关闭
```

```python
# workflow/reboot_workflow.py
from domain.vmc import VMC


class RebootWorkflow:

    def __init__(self, vmc_service):
        self.service = vmc_service

    def execute(self, vmc_names):
        for name in vmc_names:
            vmc = VMC(name)
            self.service.reboot(vmc)
```

Workflow 不知道 SSH，不知道 Paramiko，不知道 `recv()`。

## 第六层：Application——最后才开始装配对象

```python
# app.py
from infrastructure.ssh_client import SSHConnection
from infrastructure.interactive_shell import InteractiveShell

from domain.vmc_service import VMCService

from workflow.reboot_workflow import RebootWorkflow

HOST = "192.168.244.43"
USER = "admin"
PASSWORD = "admin"

VMC_NAMES = [
    "astatine0",
    "barium0",
    "bohrium",
]


def main():
    conn = SSHConnection(HOST, USER, PASSWORD)
    conn.connect()

    shell = InteractiveShell(conn.open_shell())
    shell.drain()

    service = VMCService(shell)
    workflow = RebootWorkflow(service)
    workflow.execute(VMC_NAMES)

    conn.close()


if __name__ == "__main__":
    main()
```

## 最后的对象流（Object Flow）

整个程序的数据流变成：

```
                main()
                  |
                  v
         SSHConnection.connect()
                  |
                  v
           InteractiveShell
                  |
                  v
            RebootWorkflow
                  |
          create VMC(name)
                  |
                  v
        VMCService.reboot(vmc)
                  |
                  v
      shell.send(command_string)
                  |
                  v
        Paramiko Channel.send()
                  |
             SSH Server
                  |
        Paramiko Channel.recv()
                  |
                  v
     InteractiveShell.recv_until()
                  |
                  v
        VMCService（业务决策）
```

每一层只依赖它下面一层暴露出来的"接口"，完全不知道再下面几层具体怎么实现——这正是 Infrastructure 层可以随意替换成假实现（用于测试/演示）而上层代码不用改的原因。

## 完整可运行代码示例

[`python/complex_obj_organization_examples/vmc_reboot_automation/`](complex_obj_organization_examples/vmc_reboot_automation) 目录下提供了本节对应的完整可运行代码。为了让示例不依赖真实 VMC 设备、也不需要安装 `paramiko`，示例里额外提供了一个 `FakeSSHConnection` 来模拟设备交互，与真实的 `SSHConnection` 接口完全一致，用来演示 Infrastructure 层可替换这一点。运行方式：

```bash
cd python/complex_obj_organization_examples/vmc_reboot_automation
python3 main.py
```