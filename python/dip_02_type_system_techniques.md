[← 返回目录](dip_00_index.md) | [上一篇: Part 1 本质与方向](dip_01_essence_and_direction.md)

# Part 2 - 类型系统类：① abc.ABC ② typing.Protocol ③ Duck Typing

## ① abc.ABC

### 原理

`abc.ABC`（Abstract Base Class）通过 `@abstractmethod` 声明"子类必须实现的方法"，并利用元类
`ABCMeta` 在**实例化阶段**做强制检查：如果子类没有实现所有抽象方法，`TypeError` 会在
`SomeSubclass()` 被调用的那一刻抛出，而不是等到运行到那个方法才报错。

- **依赖方向**：High-level 模块 import 抽象基类（放在 domain/application 层），Low-level
  模块 import 并继承这个抽象基类。是**显式的名义子类型（nominal subtyping）**——必须写
  `class MySQLRepository(OrderRepository):`，两者之间有真实的继承关系。
- **适合规模**：中大型项目、团队协作、需要在类型层面"锁死"契约的场景；也适合作为插件系统的
  注册基类（`__subclasses__()` 可以拿到所有实现）。
- **优点**：约束力最强，实例化时就能捕获"没实现完"的错误；`isinstance()` 检查天然可用；IDE
  和类型检查器支持好；可以在基类里提供 Template Method（既定义抽象方法，也提供默认实现）。
- **缺点**：要求 Low-level 类显式继承，无法让"现成的、不受你控制的第三方类"直接满足接口（除非用
  `register()`）；比 Protocol 更"重"，小脚本没必要用；多继承时容易和其他基类产生 MRO 问题。

### 示例 1：Repository —— OrderService / OrderRepository / MySQL / SQLite

真实场景：订单服务不应该关心订单到底存在哪种数据库里。

```python
from __future__ import annotations

import sqlite3
from abc import ABC, abstractmethod
from dataclasses import dataclass


@dataclass(frozen=True, slots=True)
class Order:
    order_id: str
    customer_id: str
    amount_cents: int
    status: str = "created"


class OrderRepository(ABC):
    """Abstraction：只声明契约，不涉及任何具体存储技术。"""

    @abstractmethod
    def save(self, order: Order) -> None: ...

    @abstractmethod
    def find_by_id(self, order_id: str) -> Order | None: ...


class MySQLOrderRepository(OrderRepository):
    """Detail：伪 MySQL 驱动（用字典模拟连接池，突出重点而不引入外部依赖）。"""

    def __init__(self, dsn: str) -> None:
        self._dsn = dsn
        self._table: dict[str, Order] = {}
        print(f"[MySQL] connected via dsn={dsn!r}")

    def save(self, order: Order) -> None:
        self._table[order.order_id] = order
        print(f"[MySQL] INSERT INTO orders VALUES ({order.order_id!r}, ...)")

    def find_by_id(self, order_id: str) -> Order | None:
        return self._table.get(order_id)


class SQLiteOrderRepository(OrderRepository):
    """Detail：真实可运行的 SQLite 实现。"""

    def __init__(self, path: str = ":memory:") -> None:
        self._conn = sqlite3.connect(path)
        self._conn.execute(
            "CREATE TABLE IF NOT EXISTS orders "
            "(order_id TEXT PRIMARY KEY, customer_id TEXT, amount_cents INTEGER, status TEXT)"
        )

    def save(self, order: Order) -> None:
        self._conn.execute(
            "INSERT OR REPLACE INTO orders VALUES (?, ?, ?, ?)",
            (order.order_id, order.customer_id, order.amount_cents, order.status),
        )
        self._conn.commit()

    def find_by_id(self, order_id: str) -> Order | None:
        row = self._conn.execute(
            "SELECT order_id, customer_id, amount_cents, status FROM orders WHERE order_id = ?",
            (order_id,),
        ).fetchone()
        return Order(*row) if row else None


class OrderService:
    """High-level Module：只依赖 OrderRepository 这个抽象。"""

    def __init__(self, repo: OrderRepository) -> None:
        self._repo = repo

    def checkout(self, order: Order) -> None:
        if order.amount_cents <= 0:
            raise ValueError("amount must be positive")
        self._repo.save(order)

    def get_order(self, order_id: str) -> Order | None:
        return self._repo.find_by_id(order_id)


if __name__ == "__main__":
    for repo in (MySQLOrderRepository(dsn="mysql://localhost/shop"), SQLiteOrderRepository()):
        service = OrderService(repo)
        service.checkout(Order("O-1", "C-1", 1999))
        print("fetched:", service.get_order("O-1"))
```

#### 对象关系图

```
+-----------------------------------------------------+
|                    OrderService                     |
+----------------------------+------------------------+
                             |
                             v
                 +--------------------------+
                 |   OrderRepository (ABC)  |
                 +--------------------------+
                     ^                 ^
                     |                 |
        +----------------------+   +-----------------------+
        | MySQLOrderRepository |   | SQLiteOrderRepository |
        +----------------------+   +-----------------------+
```

#### Object Flow

```
main()
  |
  v
create MySQLOrderRepository(dsn=...)   # 或 SQLiteOrderRepository()
  |
  v
inject into OrderService(repo)
  |
  v
OrderService.checkout(order)
  |
  v
repo.save(order)
  |
  v
MySQL / SQLite 实际写入
```

#### Dependency Graph

```
OrderService
     |
     v
OrderRepository (Abstraction)
     ^
     |
MySQLOrderRepository / SQLiteOrderRepository (Detail)
```

#### 为什么符合 DIP？

`OrderService` 所在模块只 `import` 了 `OrderRepository` 这一个抽象类型，从未出现
`import sqlite3` 或任何 MySQL 驱动的字样。业务规则"金额必须为正"完全独立于存储技术。反过来，
`MySQLOrderRepository` 和 `SQLiteOrderRepository` 都必须 `import OrderRepository` 并继承它——
数据库细节依赖了业务定义的抽象，而不是业务依赖数据库。以后要接入 PostgreSQL、DynamoDB、甚至
"先写内存队列异步落库"，只需要新增一个 `OrderRepository` 的子类，`OrderService` 一行都不用改。

#### 如果违反 DIP

```
+----------------+        +----------------+
|  OrderService  |------->|  MySQL 驱动细节  |
+----------------+ import +----------------+
```

- 单元测试 `OrderService.checkout()` 必须先起一个 MySQL（或者维护一个笨重的 mock 覆盖具体驱动
  API），无法通过一个轻量假对象验证业务规则。
- 想支持 SQLite/PostgreSQL 需要在 `OrderService` 内部写 `if db_type == "mysql": ... elif ...`，
  每加一种存储都要改业务代码，违反开闭原则，修改成本随实现数量线性增长。
- `OrderService` 和具体数据库强耦合，数据库驱动升级/换库都会直接波及业务代码，回归测试范围
  被迫扩大到整个订单模块。

### 示例 2：Payment Gateway —— Stripe / PayPal / AliPay

真实场景：结账流程要支持多种支付渠道，且要能在测试中完全不发起真实网络请求。

```python
from __future__ import annotations

from abc import ABC, abstractmethod
from dataclasses import dataclass


@dataclass(frozen=True, slots=True)
class PaymentResult:
    success: bool
    transaction_id: str
    gateway: str


class PaymentGateway(ABC):
    """Abstraction：结账流程需要的全部支付能力，与具体渠道无关。"""

    @abstractmethod
    def charge(self, amount_cents: int, currency: str, token: str) -> PaymentResult: ...

    @abstractmethod
    def refund(self, transaction_id: str) -> bool: ...


class StripeGateway(PaymentGateway):
    def charge(self, amount_cents: int, currency: str, token: str) -> PaymentResult:
        print(f"[Stripe] charging {amount_cents} {currency} with token={token}")
        return PaymentResult(True, f"stripe_tx_{token[:6]}", "stripe")

    def refund(self, transaction_id: str) -> bool:
        print(f"[Stripe] refunding {transaction_id}")
        return True


class PayPalGateway(PaymentGateway):
    def charge(self, amount_cents: int, currency: str, token: str) -> PaymentResult:
        print(f"[PayPal] REST create-order amount={amount_cents} currency={currency}")
        return PaymentResult(True, f"pp_tx_{token[:6]}", "paypal")

    def refund(self, transaction_id: str) -> bool:
        print(f"[PayPal] REST refund {transaction_id}")
        return True


class AliPayGateway(PaymentGateway):
    def charge(self, amount_cents: int, currency: str, token: str) -> PaymentResult:
        print(f"[AliPay] alipay.trade.pay amount={amount_cents} currency={currency}")
        return PaymentResult(True, f"alipay_tx_{token[:6]}", "alipay")

    def refund(self, transaction_id: str) -> bool:
        print(f"[AliPay] alipay.trade.refund {transaction_id}")
        return True


class CheckoutService:
    """High-level Module：结账流程，完全不知道具体走的是哪个支付渠道。"""

    def __init__(self, gateway: PaymentGateway) -> None:
        self._gateway = gateway

    def pay(self, amount_cents: int, currency: str, token: str) -> PaymentResult:
        result = self._gateway.charge(amount_cents, currency, token)
        if not result.success:
            raise RuntimeError("payment failed")
        return result


if __name__ == "__main__":
    for gw in (StripeGateway(), PayPalGateway(), AliPayGateway()):
        checkout = CheckoutService(gw)
        print(checkout.pay(4999, "USD", "tok_visa_123456"))
```

#### 对象关系图

```
                CheckoutService
                       |
                       v
                 PaymentGateway (ABC)
                ^      ^        ^
                |      |        |
          Stripe    PayPal    AliPay
```

#### Object Flow

```
main()
  |
  v
choose concrete gateway (Stripe / PayPal / AliPay)
  |
  v
inject into CheckoutService(gateway)
  |
  v
CheckoutService.pay(amount, currency, token)
  |
  v
gateway.charge(...)
  |
  v
第三方支付平台 HTTP/SDK 调用
```

#### Dependency Graph

```
CheckoutService
     |
     v
PaymentGateway (Abstraction)
     ^
     |
StripeGateway / PayPalGateway / AliPayGateway (Detail)
```

#### 为什么符合 DIP？

`CheckoutService` 只依赖 `charge()` / `refund()` 这两个契约，不知道 Stripe 用的是 REST +
Webhook、PayPal 用的是 Order API、支付宝用的是签名 + 回调。新增 Google Pay 只需要新写一个
`PaymentGateway` 子类，`CheckoutService` 代码零改动，符合开闭原则的同时也满足 DIP。

#### 如果违反 DIP

若 `CheckoutService` 内部写死 `import stripe; stripe.Charge.create(...)`：

```
CheckoutService --------import--------> stripe SDK
```

- 想支持多支付渠道，必须在 `pay()` 里堆 `if provider == "stripe": ... elif provider == "paypal": ...`，
  每种支付渠道的 SDK 差异（同步/异步、字段命名、错误码体系）全部污染业务方法。
- 单测必须 mock 掉 `stripe` 这个第三方包的内部对象结构，一旦 SDK 升级，测试和业务代码一起碎。
- 无法在运行时按 A/B 测试或按地区动态切换支付渠道，因为渠道选择和调用逻辑耦合在一起。

### 示例 3：Device Client —— SSH / NETCONF / RESTCONF

真实场景（网络自动化）：设备巡检 workflow 不应该关心底层用哪种协议连接设备。

```python
from __future__ import annotations

from abc import ABC, abstractmethod


class DeviceClient(ABC):
    """Abstraction：网络设备的通用操作契约，与具体协议无关。"""

    @abstractmethod
    def connect(self, host: str) -> None: ...

    @abstractmethod
    def get_config(self) -> str: ...

    @abstractmethod
    def disconnect(self) -> None: ...


class SSHDeviceClient(DeviceClient):
    def connect(self, host: str) -> None:
        self._host = host
        print(f"[SSH] opening paramiko session to {host}:22")

    def get_config(self) -> str:
        print("[SSH] running 'show running-config'")
        return "hostname RPD-01\ninterface Gi0/0\n"

    def disconnect(self) -> None:
        print(f"[SSH] closing session to {self._host}")


class NetconfDeviceClient(DeviceClient):
    def connect(self, host: str) -> None:
        self._host = host
        print(f"[NETCONF] ncclient.manager.connect({host}:830)")

    def get_config(self) -> str:
        print("[NETCONF] <get-config><running/></get-config>")
        return "<config><hostname>RPD-01</hostname></config>"

    def disconnect(self) -> None:
        print(f"[NETCONF] session.close_session() to {self._host}")


class RestconfDeviceClient(DeviceClient):
    def connect(self, host: str) -> None:
        self._host = host
        print(f"[RESTCONF] base_url=https://{host}/restconf")

    def get_config(self) -> str:
        print("[RESTCONF] GET /restconf/data/ietf-interfaces:interfaces")
        return '{"hostname": "RPD-01"}'

    def disconnect(self) -> None:
        print(f"[RESTCONF] no persistent session to close for {self._host}")


class DeviceInspectionWorkflow:
    """High-level Module：巡检流程，只依赖 DeviceClient 抽象。"""

    def __init__(self, client: DeviceClient) -> None:
        self._client = client

    def run(self, host: str) -> str:
        self._client.connect(host)
        try:
            return self._client.get_config()
        finally:
            self._client.disconnect()


if __name__ == "__main__":
    for client in (SSHDeviceClient(), NetconfDeviceClient(), RestconfDeviceClient()):
        workflow = DeviceInspectionWorkflow(client)
        print("config snapshot:", workflow.run("10.0.0.1"))
```

#### 对象关系图

```
             DeviceInspectionWorkflow
                       |
                       v
                 DeviceClient (ABC)
             ^          ^           ^
             |          |           |
       SSHDeviceClient  NetconfDeviceClient  RestconfDeviceClient
```

#### Object Flow

```
main()
  |
  v
create concrete DeviceClient (SSH/NETCONF/RESTCONF)
  |
  v
inject into DeviceInspectionWorkflow(client)
  |
  v
workflow.run(host)
  |
  v
client.connect() -> client.get_config() -> client.disconnect()
  |
  v
真实设备（通过对应协议栈）
```

#### Dependency Graph

```
DeviceInspectionWorkflow
     |
     v
DeviceClient (Abstraction)
     ^
     |
SSHDeviceClient / NetconfDeviceClient / RestconfDeviceClient (Detail)
```

#### 为什么符合 DIP？

`DeviceInspectionWorkflow` 不知道 SSH 用的是交互式 shell 解析文本、NETCONF 用的是 XML RPC、
RESTCONF 用的是 HTTP JSON——它只知道 `connect/get_config/disconnect` 三个动作。未来新增 gNMI
（基于 gRPC 流式订阅）只需要新增一个 `GnmiDeviceClient(DeviceClient)`，巡检 workflow 不需要
任何改动，甚至可以让同一个 workflow 同时巡检"新旧混合"的设备群（部分走 SSH、部分走 NETCONF）。

#### 如果违反 DIP

```
DeviceInspectionWorkflow --------import--------> paramiko / ncclient / requests
```

- Workflow 里混入 `if protocol == "ssh": paramiko.SSHClient()... elif protocol == "netconf":
  ncclient.manager.connect(...)`，协议分支逻辑和巡检业务逻辑绞在一起，函数迅速膨胀成"上帝方法"。
- 无法对 workflow 做纯逻辑单测（比如"巡检失败要不要重试 3 次"），因为每次测试都会真的尝试建立
  SSH/NETCONF 连接。
- 团队里负责"协议适配"的人和负责"巡检业务流程"的人无法并行开发——他们改的是同一个文件。

## ② typing.Protocol

### 原理

`typing.Protocol`（PEP 544）提供**结构化子类型（structural subtyping，即"鸭子类型 + 静态类型
检查"）**：只要一个类"长得像"（拥有同名同签名的方法/属性），它就被认为满足这个 Protocol，**完全
不需要显式继承**。运行时不做强制检查（除非用 `@runtime_checkable` 配合 `isinstance()`，但那也
只检查方法是否存在，不检查签名）；真正的把关工作交给 `mypy` / `pyright` 等静态类型检查器。

- **依赖方向**：与 ABC 相同——High-level 定义 Protocol，Low-level 实现它满足的方法集合；但
  **Low-level 甚至不需要 `import` 这个 Protocol**，只要方法签名对得上即可（隐式满足）。这使得
  依赖方向的"倒置"更加彻底：Low-level 可以完全不知道 Abstraction 的存在。
  也常见于 Low-level 显式 import Protocol 仅用于类型注解自查（可选，非强制）。
- **适合规模**：中大型项目，尤其是需要兼容"第三方现成对象"（无法继承它们）或者需要给标准库
  对象（如 `IO`、`SupportsWrite`）当作依赖注入目标的场景；也很适合 FastAPI / 现代 Python 后端。
- **优点**：解耦更彻底（不需要共同祖先）；对已有类零侵入，接第三方 SDK 对象非常自然；仍然享有
  完整的静态类型检查和 IDE 补全。
- **缺点**：运行时没有强制保证（写错方法签名，运行时才会在调用处报 `AttributeError`）；团队若
  不用类型检查工具，Protocol 的价值会大打折扣；`@runtime_checkable` 的 `isinstance` 检查只看
  方法名，不看参数类型，容易造成"假满足"。

### 示例 1：Notification Channel —— Email / SMS / Slack

真实场景：告警系统要把消息发到不同渠道，且要能直接把"第三方 SDK 客户端对象"当作渠道用，而不用
为每个 SDK 写继承包装类。

```python
from __future__ import annotations

from typing import Protocol


class NotificationChannel(Protocol):
    """Abstraction：结构化契约，谁都不需要显式继承它。"""

    def send(self, subject: str, body: str) -> bool: ...


class EmailChannel:
    """Detail：完全不 import NotificationChannel，靠方法签名"隐式"满足契约。"""

    def __init__(self, smtp_host: str) -> None:
        self._smtp_host = smtp_host

    def send(self, subject: str, body: str) -> bool:
        print(f"[Email via {self._smtp_host}] subject={subject!r} body={body!r}")
        return True


class SmsChannel:
    def __init__(self, api_key: str) -> None:
        self._api_key = api_key

    def send(self, subject: str, body: str) -> bool:
        print(f"[SMS api_key={self._api_key[:4]}***] {subject}: {body[:20]}")
        return True


class SlackChannel:
    def __init__(self, webhook_url: str) -> None:
        self._webhook_url = webhook_url

    def send(self, subject: str, body: str) -> bool:
        print(f"[Slack POST {self._webhook_url}] *{subject}*\n{body}")
        return True


class AlertService:
    """High-level Module：只依赖 NotificationChannel 这个结构化契约。"""

    def __init__(self, channels: list[NotificationChannel]) -> None:
        self._channels = channels

    def broadcast(self, subject: str, body: str) -> int:
        return sum(1 for ch in self._channels if ch.send(subject, body))


if __name__ == "__main__":
    service = AlertService(
        [
            EmailChannel("smtp.corp.com"),
            SmsChannel("sk_live_abcdef"),
            SlackChannel("https://hooks.slack.com/services/xxx"),
        ]
    )
    print("delivered:", service.broadcast("CPU High", "node-3 CPU > 90% for 5min"))
```

#### 对象关系图

```
                    AlertService
                          |
                          v
             NotificationChannel (Protocol，结构化契约)
              (无继承箭头，仅方法签名匹配)
            EmailChannel  SmsChannel  SlackChannel
```

#### Object Flow

```
main()
  |
  v
create EmailChannel / SmsChannel / SlackChannel (互不相关的独立类)
  |
  v
inject list into AlertService(channels)
  |
  v
AlertService.broadcast(subject, body)
  |
  v
for ch in channels: ch.send(...)
  |
  v
真实的邮件/短信/Slack API 调用
```

#### Dependency Graph

```
AlertService
     |
     v
NotificationChannel (Abstraction, structural)
     ^ (结构匹配，非继承)
     |
EmailChannel / SmsChannel / SlackChannel (Detail)
```

#### 为什么符合 DIP？

`AlertService` 依赖的是"任何拥有 `send(subject, body) -> bool` 方法的对象"这一结构化契约，
`EmailChannel`、`SmsChannel`、`SlackChannel` 之间没有任何共同的基类，甚至互不知道对方存在。
新增一个渠道（比如企业微信）只需要写一个同签名的类，不需要继承任何东西，也不需要修改
`AlertService`。这比 ABC 更彻底地做到了"高层定义契约，细节各自实现"，因为细节甚至不需要
知道契约类的存在。

#### 如果违反 DIP

若 `AlertService` 直接依赖三个具体类型：

```python
def broadcast(self, email: EmailChannel, sms: SmsChannel, slack: SlackChannel, ...): ...
```

- 渠道数量一多，`AlertService` 的构造函数/方法签名要跟着爆炸式增长。
- 想要"按环境启用不同渠道组合"（生产环境用 Slack+Email，测试环境只用 Email）就需要在
  `AlertService` 内部写大量 `if channel_type == ...`，业务代码和渠道选择逻辑耦合。
- 无法轻松写一个 `FakeChannel` 来验证"广播是否发给了所有渠道"，因为 mock 目标是散落的具体类型
  而不是一个统一契约。

### 示例 2：Serializer —— JSON / XML

真实场景：配置导出模块需要支持多种序列化格式，且要能直接把标准库对象（如 `json` 模块本身）当
依赖注入进来。

```python
from __future__ import annotations

import json
import xml.etree.ElementTree as ET
from typing import Any, Protocol


class Serializer(Protocol):
    """Abstraction：只要求"能把 dict 变成 str，也能把 str 变回 dict"。"""

    def dumps(self, data: dict[str, Any]) -> str: ...

    def loads(self, raw: str) -> dict[str, Any]: ...


class JsonSerializer:
    def dumps(self, data: dict[str, Any]) -> str:
        return json.dumps(data)

    def loads(self, raw: str) -> dict[str, Any]:
        return json.loads(raw)


class XmlSerializer:
    def dumps(self, data: dict[str, Any]) -> str:
        root = ET.Element("config")
        for key, value in data.items():
            child = ET.SubElement(root, key)
            child.text = str(value)
        return ET.tostring(root, encoding="unicode")

    def loads(self, raw: str) -> dict[str, Any]:
        root = ET.fromstring(raw)
        return {child.tag: child.text for child in root}


class ConfigExporter:
    """High-level Module：只依赖 Serializer 结构化契约。"""

    def __init__(self, serializer: Serializer) -> None:
        self._serializer = serializer

    def export(self, config: dict[str, Any]) -> str:
        return self._serializer.dumps(config)

    def import_(self, raw: str) -> dict[str, Any]:
        return self._serializer.loads(raw)


if __name__ == "__main__":
    cfg = {"hostname": "RPD-01", "mtu": 1500}
    for serializer in (JsonSerializer(), XmlSerializer()):
        exporter = ConfigExporter(serializer)
        exported = exporter.export(cfg)
        print(exported, "->", exporter.import_(exported))
```

#### 对象关系图

```
                ConfigExporter
                      |
                      v
             Serializer (Protocol)
              ^                 ^
              |                 |
        JsonSerializer     XmlSerializer
```

#### Object Flow

```
main()
  |
  v
choose JsonSerializer / XmlSerializer
  |
  v
inject into ConfigExporter(serializer)
  |
  v
ConfigExporter.export(config) -> serializer.dumps(config)
  |
  v
字符串写入文件/网络
```

#### Dependency Graph

```
ConfigExporter
     |
     v
Serializer (Abstraction, structural)
     ^
     |
JsonSerializer / XmlSerializer (Detail)
```

#### 为什么符合 DIP？

`ConfigExporter` 不知道 JSON 用逗号分隔、XML 要处理命名空间和转义——它只知道 `dumps/loads`。
以后要支持 YAML、Protobuf，只需要新写一个满足签名的类。因为是结构化契约，甚至可以直接把
一个第三方库里现成的、方法名恰好叫 `dumps/loads` 的对象传进来，不需要包一层继承适配。

#### 如果违反 DIP

若 `ConfigExporter` 内部固定 `import json` 并调用 `json.dumps`：

- 想要同时支持 JSON 和 XML 导出，必须新增参数 `format: str` 并在方法体内 `if format == "json"`，
  每加一种格式都要改 `ConfigExporter` 源码。
- 无法给 `ConfigExporter` 做隔离测试——它的输出格式和它的业务逻辑（要不要脱敏、要不要加时间戳）
  绑死在一起，测试断言必须写死具体格式的字符串。

### 示例 3：Cache —— Redis / In-Memory

真实场景：领域服务需要缓存计算结果，生产用 Redis，单元测试和本地开发用内存字典，两者要能无缝
互换。

```python
from __future__ import annotations

import time
from typing import Any, Protocol


class Cache(Protocol):
    def get(self, key: str) -> Any | None: ...

    def set(self, key: str, value: Any, ttl_seconds: int = 60) -> None: ...


class InMemoryCache:
    """Detail：适合单测/本地开发，无需起 Redis。"""

    def __init__(self) -> None:
        self._store: dict[str, tuple[Any, float]] = {}

    def get(self, key: str) -> Any | None:
        item = self._store.get(key)
        if item is None:
            return None
        value, expires_at = item
        if time.time() > expires_at:
            del self._store[key]
            return None
        return value

    def set(self, key: str, value: Any, ttl_seconds: int = 60) -> None:
        self._store[key] = (value, time.time() + ttl_seconds)


class RedisCache:
    """Detail：伪 Redis 客户端（用字典模拟 RESP 协议往返，突出接口一致性）。"""

    def __init__(self, host: str, port: int = 6379) -> None:
        self._addr = f"{host}:{port}"
        self._store: dict[str, Any] = {}

    def get(self, key: str) -> Any | None:
        print(f"[Redis {self._addr}] GET {key}")
        return self._store.get(key)

    def set(self, key: str, value: Any, ttl_seconds: int = 60) -> None:
        print(f"[Redis {self._addr}] SETEX {key} {ttl_seconds}")
        self._store[key] = value


class PricingService:
    """High-level Module：只依赖 Cache 结构化契约做加速，不关心缓存后端。"""

    def __init__(self, cache: Cache) -> None:
        self._cache = cache

    def get_price(self, sku: str) -> float:
        cached = self._cache.get(f"price:{sku}")
        if cached is not None:
            return cached
        price = self._expensive_lookup(sku)
        self._cache.set(f"price:{sku}", price, ttl_seconds=300)
        return price

    def _expensive_lookup(self, sku: str) -> float:
        print(f"[DB] computing price for {sku} ...")
        return 19.99


if __name__ == "__main__":
    for cache in (InMemoryCache(), RedisCache("localhost")):
        svc = PricingService(cache)
        print("first call:", svc.get_price("SKU-1"))
        print("second call (cached):", svc.get_price("SKU-1"))
```

#### 对象关系图

```
                PricingService
                      |
                      v
                Cache (Protocol)
              ^                ^
              |                |
        InMemoryCache      RedisCache
```

#### Object Flow

```
main() / 测试 fixture
  |
  v
choose InMemoryCache（测试）or RedisCache（生产）
  |
  v
inject into PricingService(cache)
  |
  v
PricingService.get_price(sku) -> cache.get() -> miss -> _expensive_lookup() -> cache.set()
  |
  v
下一次调用直接命中缓存
```

#### Dependency Graph

```
PricingService
     |
     v
Cache (Abstraction, structural)
     ^
     |
InMemoryCache / RedisCache (Detail)
```

#### 为什么符合 DIP？

`PricingService` 的业务逻辑（"先查缓存，没有就算，算完写回缓存"）完全不依赖缓存后端是 Redis
还是内存字典。测试环境用 `InMemoryCache` 可以做到零依赖、毫秒级单测；生产环境换成
`RedisCache`（甚至以后换成 Memcached）时，`PricingService` 不用改一行代码。

#### 如果违反 DIP

若 `PricingService` 直接 `import redis; redis.Redis(host=...)`：

- 每条单测都需要一个真实的 Redis 实例（或复杂的 fakeredis 依赖），CI 环境搭建成本上升。
- 缓存后端的 API 差异（Redis 的 `setex` vs Memcached 的 `set` 加过期时间参数顺序不同）会
  直接暴露进业务方法，业务代码被基础设施 API 的形状"腐蚀"。

## ③ Duck Typing（无显式契约的隐式协议）

### 原理

Duck Typing 是 Python 最原始、最"轻量"的 DIP 实现方式：**不声明任何接口（无论是 ABC 还是
Protocol），只靠约定俗成的方法名**。"如果它走起来像鸭子、叫起来像鸭子，那它就是鸭子"——调用方
只管调用 `obj.write(...)`，不关心 `obj` 的类型，只要它在运行时真的有 `write` 方法即可。

- **依赖方向**：概念上依然是"High-level 定义了它期望的方法名集合，Low-level 提供满足这些方法名
  的对象"，但这个"契约"只存在于**文档 / 命名约定 / 团队默契**里，代码里没有任何显式类型来承载它。
- **适合规模**：小型脚本、内部工具、探索性 / 原型代码、生命周期很短的胶水代码；也常见于测试代码
  里临时构造一个"看起来像"某个依赖的对象。
- **优点**：最少的样板代码，写起来最快；天然兼容任何对象，不需要继承或声明。
  **缺点**：没有任何静态检查兜底，签名不匹配只有运行到那一行才报 `AttributeError`；IDE 补全和
  重构支持差；接口"契约"只存在于人的脑子里，团队一大就容易产生"隐性契约漂移"（有人改了方法名，
  没人发现）。

### 示例 1：Logger —— Console / File（零声明，直接鸭子类型）

```python
from __future__ import annotations

import sys
from datetime import datetime, timezone


class ConsoleLogger:
    def log(self, level: str, message: str) -> None:
        ts = datetime.now(timezone.utc).isoformat()
        print(f"[{ts}] [{level}] {message}", file=sys.stdout)


class FileLogger:
    def __init__(self, path: str) -> None:
        self._path = path

    def log(self, level: str, message: str) -> None:
        ts = datetime.now(timezone.utc).isoformat()
        with open(self._path, "a", encoding="utf-8") as fh:
            fh.write(f"[{ts}] [{level}] {message}\n")


class DeviceUpgradeJob:
    """High-level Module：只要传进来的 logger 有 log(level, message) 方法即可，没有任何类型声明。"""

    def __init__(self, logger) -> None:  # 注意：没有类型注解，纯 Duck Typing
        self._logger = logger

    def run(self, device_id: str) -> None:
        self._logger.log("INFO", f"start upgrading {device_id}")
        self._logger.log("INFO", f"finished upgrading {device_id}")


if __name__ == "__main__":
    for logger in (ConsoleLogger(), FileLogger("/tmp/upgrade.log")):
        DeviceUpgradeJob(logger).run("RPD-01")
```

#### 对象关系图

```
              DeviceUpgradeJob
                     |
                     v
       （无显式类型，仅靠方法名 "log" 约定）
                     |
              ConsoleLogger / FileLogger
```

#### Object Flow

```
main()
  |
  v
create ConsoleLogger() / FileLogger(path)
  |
  v
inject into DeviceUpgradeJob(logger)   # 无接口声明
  |
  v
DeviceUpgradeJob.run(device_id) -> logger.log(level, message)
  |
  v
标准输出 / 日志文件
```

#### Dependency Graph

```
DeviceUpgradeJob
     |
     v
（隐式契约：任何拥有 log(level, message) 方法的对象）
     ^
     |
ConsoleLogger / FileLogger
```

#### 为什么符合 DIP？

即便没有写出 `Protocol` 或 `ABC`，`DeviceUpgradeJob` 依然没有在源码里 `import` 任何具体
Logger 实现，依赖方向在"精神上"仍然是倒置的——业务代码只依赖一个方法名约定，具体写日志到哪里
完全由调用方决定。对于这种"参数极其简单、生命周期极短"的脚本工具，Duck Typing 用最小成本达到了
和 Protocol 类似的解耦效果。

#### 如果违反 DIP

若 `DeviceUpgradeJob` 内部直接 `print(...)` 或者硬编码打开某个日志文件：

- 换一个输出目的地（比如接入公司统一日志平台）需要改 `DeviceUpgradeJob` 源码本身。
- 单测里想断言"确实记录了两条日志"会变得很别扭，只能重定向 `stdout` 或读文件内容，而不能注入
  一个假 logger 收集调用记录。

### 示例 2：Validator —— Schema-based / Rule-based（结构一致但完全无声明）

```python
from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True, slots=True)
class ValidationResult:
    ok: bool
    errors: list[str]


class SchemaValidator:
    """按字段类型做结构校验，模拟 jsonschema 的效果。"""

    def __init__(self, schema: dict[str, type]) -> None:
        self._schema = schema

    def validate(self, payload: dict) -> ValidationResult:
        errors = [
            f"field {name!r} must be {expected.__name__}"
            for name, expected in self._schema.items()
            if not isinstance(payload.get(name), expected)
        ]
        return ValidationResult(ok=not errors, errors=errors)


class RuleValidator:
    """按业务规则做校验（不检查类型，检查取值范围）。"""

    def validate(self, payload: dict) -> ValidationResult:
        errors = []
        if payload.get("mtu", 0) > 9000:
            errors.append("mtu must be <= 9000")
        if not payload.get("hostname"):
            errors.append("hostname is required")
        return ValidationResult(ok=not errors, errors=errors)


class ConfigIntakeService:
    """High-level Module：只要 validator 有 validate(payload) -> ValidationResult 即可。"""

    def __init__(self, validator) -> None:  # Duck Typing，无接口声明
        self._validator = validator

    def submit(self, payload: dict) -> ValidationResult:
        result = self._validator.validate(payload)
        if result.ok:
            print(f"[Intake] accepted: {payload}")
        else:
            print(f"[Intake] rejected: {result.errors}")
        return result


if __name__ == "__main__":
    payload = {"hostname": "RPD-01", "mtu": 1500}
    for validator in (SchemaValidator({"hostname": str, "mtu": int}), RuleValidator()):
        ConfigIntakeService(validator).submit(payload)
```

#### 对象关系图

```
             ConfigIntakeService
                      |
                      v
     （隐式契约："validate(payload) -> ValidationResult"）
                      |
          SchemaValidator / RuleValidator
```

#### Object Flow

```
main()
  |
  v
create SchemaValidator(schema) / RuleValidator()
  |
  v
inject into ConfigIntakeService(validator)
  |
  v
ConfigIntakeService.submit(payload) -> validator.validate(payload)
  |
  v
ValidationResult 返回给调用方决定后续动作
```

#### Dependency Graph

```
ConfigIntakeService
     |
     v
（隐式契约：validate(payload) -> ValidationResult）
     ^
     |
SchemaValidator / RuleValidator
```

#### 为什么符合 DIP？

`ConfigIntakeService` 不关心校验是基于类型、基于取值范围、还是基于正则表达式——它只关心
拿到一个 `ValidationResult`。可以在不同环境组合出不同的校验策略（甚至把多个 validator 串成
一条链），完全不触碰 `ConfigIntakeService` 本身。

#### 如果违反 DIP

若 `submit()` 内部直接写死类型检查和取值范围检查两段 if-else：

- 每新增一条校验规则都要修改 `ConfigIntakeService`，这个类会变成一个不断膨胀的"校验规则大杂烩"。
- 无法针对"只测试 mtu 规则"写一个极简单测，因为规则和业务方法焊在一起。

### 示例 3：Report Exporter —— CSV / PDF（同签名、无共同基类、也无 Protocol）

```python
from __future__ import annotations

import csv
import io


class CsvExporter:
    def export(self, rows: list[dict]) -> str:
        buf = io.StringIO()
        if rows:
            writer = csv.DictWriter(buf, fieldnames=list(rows[0].keys()))
            writer.writeheader()
            writer.writerows(rows)
        return buf.getvalue()


class PdfExporter:
    """用纯文本模拟 PDF 排版逻辑，避免引入 reportlab 等重依赖。"""

    def export(self, rows: list[dict]) -> str:
        lines = ["%PDF-1.4 (simulated)"]
        for row in rows:
            lines.append(" | ".join(f"{k}={v}" for k, v in row.items()))
        return "\n".join(lines)


class ReportGenerator:
    """High-level Module：只依赖"exporter 有 export(rows) -> str"这个隐式约定。"""

    def __init__(self, exporter) -> None:
        self._exporter = exporter

    def generate(self, rows: list[dict]) -> str:
        if not rows:
            raise ValueError("no data to export")
        return self._exporter.export(rows)


if __name__ == "__main__":
    rows = [{"device": "RPD-01", "status": "online"}, {"device": "RPD-02", "status": "offline"}]
    for exporter in (CsvExporter(), PdfExporter()):
        print(ReportGenerator(exporter).generate(rows))
        print("---")
```

#### 对象关系图

```
              ReportGenerator
                     |
                     v
      （隐式契约: export(rows) -> str）
                     |
             CsvExporter / PdfExporter
```

#### Object Flow

```
main()
  |
  v
create CsvExporter() / PdfExporter()
  |
  v
inject into ReportGenerator(exporter)
  |
  v
ReportGenerator.generate(rows) -> exporter.export(rows)
  |
  v
字符串写入文件 / HTTP 响应体
```

#### Dependency Graph

```
ReportGenerator
     |
     v
（隐式契约：export(rows) -> str）
     ^
     |
CsvExporter / PdfExporter
```

#### 为什么符合 DIP？

`ReportGenerator` 的"生成报表前先校验数据非空"这条业务规则，与报表最终是 CSV 还是 PDF 完全
无关。新增 Excel/HTML 导出格式只需要新写一个同签名的类，`ReportGenerator` 不需要改动，依然是
高层策略不依赖具体格式实现细节。

#### 如果违反 DIP

若 `ReportGenerator` 内部直接用 `csv.DictWriter` 拼字符串：

- 想再支持 PDF，必须把 `generate()` 拆成 `generate_csv()` / `generate_pdf()`，调用方还要
  自己决定调哪个方法，"生成报表"这个统一入口被打散。
- 复用"校验数据非空"这条规则变得困难，因为它被焊死在某一种具体格式的导出函数里。

## 三者速查

```
+------------------------------------------------------------------+
|             | abc.ABC    |  typing.Protocol   |  Duck Typing    |
+------------------------------------------------------------------+
| 契约声明方式  | 显式抽象基类 |  结构化类型声明     |  纯命名约定       |
| 子类需继承？  | 需要        |  不需要            |  不需要           |
| 运行时强制？  | 是(实例化时) |  否(需类型检查器)   |  否               |
| 对第三方类友好 | 差(需继承)  |  好(隐式满足)      |  好(无声明成本)   |
| 适合规模      | 中大型      |  中大型           |  小型/脚本/原型    |
+------------------------------------------------------------------+
```

下一步：进入 [Part 3 - 注入方式类：Constructor / Setter / Method Injection](dip_03_injection_techniques.md)。
