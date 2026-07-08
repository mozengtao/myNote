[← 返回目录](dip_00_index.md) | [上一篇: Part 6 对比表](dip_06_comparison_table.md)

# Part 7 - DIP 与设计模式 / 架构风格

一个重要认知：**DIP 不是众多设计模式中平起平坐的一个，而是几乎所有"解耦类"模式和架构风格的
底层原理**。GoF 23 种设计模式里至少一半在解决"如何让两个东西不直接依赖"的问题，而它们的共同
底层机制正是"引入一个抽象，让原本互相依赖的两方都转而依赖这个抽象"——这正是 DIP 的定义。可以说
DIP 是"道"，具体模式是"术"。

## Factory（工厂）

**关系**：Factory 负责"生产"满足某个抽象的对象，调用方因此不需要知道具体类，只需要依赖 Factory
的返回值类型（抽象）。Factory 本身是**允许知道所有具体实现**的少数几个模块之一（它就是为了
封装这份知识而存在的）。Part 4 的 ⑦ 已经给出三个完整示例（RepositoryFactory /
ConnectorFactory / GatewayFactory），此处不再重复，仅给出一个更聚焦"Factory 如何服务 DIP"
的最小示例：

```python
from __future__ import annotations

import os
from typing import Protocol


class Logger(Protocol):
    def log(self, message: str) -> None: ...


class JsonLogger:
    def log(self, message: str) -> None:
        print(f'{{"message": "{message}"}}')


class PlainTextLogger:
    def log(self, message: str) -> None:
        print(message)


def logger_factory() -> Logger:
    """Factory 是唯一读取环境变量、知道两种具体 Logger 存在的地方。"""
    return JsonLogger() if os.getenv("LOG_FORMAT") == "json" else PlainTextLogger()


class App:
    def __init__(self, logger: Logger) -> None:
        self._logger = logger

    def run(self) -> None:
        self._logger.log("app started")


if __name__ == "__main__":
    App(logger_factory()).run()
```

**为什么依赖 DIP**：`App` 依赖 `Logger` 抽象，`logger_factory()` 是唯一同时认识
`JsonLogger`/`PlainTextLogger` 具体类型和环境变量细节的地方——Factory 模式的存在意义，正是
把"选择该依赖谁的具体实现"这件事从业务代码里剥离出来，本身就是 DIP 的直接应用。

## Strategy（策略）

**关系**：Strategy 是"多个平等的可替换算法共享一个抽象接口"，Context 对象依赖这个抽象接口，
具体算法类实现它——这正是 DIP 第二句话（"Detail 依赖 Abstraction"）最直接的体现。Part 4 的
⑧ 已给出三个完整示例（重试/负载均衡/压缩策略）。这里给一个新示例，展示 Strategy 与"开闭原则"
的联动：

```python
from __future__ import annotations

from typing import Protocol


class ShippingCostStrategy(Protocol):
    def calculate(self, weight_kg: float, distance_km: float) -> float: ...


class FlatRateStrategy:
    def calculate(self, weight_kg: float, distance_km: float) -> float:
        return 9.99


class WeightBasedStrategy:
    def calculate(self, weight_kg: float, distance_km: float) -> float:
        return weight_kg * 1.5


class DistanceBasedStrategy:
    def calculate(self, weight_kg: float, distance_km: float) -> float:
        return distance_km * 0.05


class Checkout:
    def __init__(self, strategy: ShippingCostStrategy) -> None:
        self._strategy = strategy

    def total_shipping_cost(self, weight_kg: float, distance_km: float) -> float:
        return round(self._strategy.calculate(weight_kg, distance_km), 2)


if __name__ == "__main__":
    for strategy in (FlatRateStrategy(), WeightBasedStrategy(), DistanceBasedStrategy()):
        print(strategy.__class__.__name__, "->", Checkout(strategy).total_shipping_cost(4.0, 120.0))
```

**为什么依赖 DIP**：`Checkout` 永远不知道运费是怎么算出来的，新增一种算法（比如"分区阶梯价"）
只需新增一个类，`Checkout` 零改动，这就是 DIP 带来的开闭性。

## Bridge（桥接）

**关系**：Bridge 把一个类型的"两个独立变化维度"拆成两条继承体系（Abstraction 体系和
Implementor 体系），两者通过组合而非继承关联，Abstraction 只依赖 Implementor 的抽象接口。
这是"用组合替代继承，并用 DIP 让两个维度独立演化"的经典案例。

真实场景：告警的"紧急程度"（Simple / Urgent）和"发送渠道"（Email / SMS）是两个独立变化的
维度——如果用继承组合会产生 `SimpleEmailAlert`、`UrgentEmailAlert`、`SimpleSmsAlert`、
`UrgentSmsAlert` 四个类，且每新增一个维度的选项就是笛卡尔积式膨胀。Bridge 把它拆开：

```python
from __future__ import annotations

from typing import Protocol


class AlertChannel(Protocol):
    """Implementor 抽象：发送渠道，与告警的紧急程度无关。"""

    def deliver(self, text: str) -> None: ...


class EmailAlertChannel:
    def deliver(self, text: str) -> None:
        print(f"[Email] {text}")


class SmsAlertChannel:
    def deliver(self, text: str) -> None:
        print(f"[SMS] {text}")


class Alert:
    """Abstraction：只依赖 AlertChannel 接口，不关心具体渠道实现。"""

    def __init__(self, channel: AlertChannel) -> None:
        self._channel = channel

    def notify(self, message: str) -> None:
        self._channel.deliver(message)


class UrgentAlert(Alert):
    """Abstraction 的精细化子类：只改变"消息怎么包装"，不涉及渠道细节。"""

    def notify(self, message: str) -> None:
        self._channel.deliver(f"!!! URGENT !!! {message}")


if __name__ == "__main__":
    for channel in (EmailAlertChannel(), SmsAlertChannel()):
        Alert(channel).notify("disk usage 80%")
        UrgentAlert(channel).notify("disk usage 99%")
```

### 对象关系图

```
              Alert / UrgentAlert (Abstraction 维度)
                          |
                          v
                  AlertChannel (Protocol)
                    ^              ^
                    |              |
          EmailAlertChannel   SmsAlertChannel  (Implementor 维度)
```

**为什么依赖 DIP**：`Alert`（及其子类 `UrgentAlert`）这条"紧急程度"继承体系，只依赖
`AlertChannel` 这个抽象，完全不知道 `EmailAlertChannel`/`SmsAlertChannel` 的存在细节。两个
维度（紧急程度 x 渠道）可以独立增加选项而互不影响——这正是把 DIP 同时应用在"两个方向"上的
结果。

## Adapter（适配器）

**关系**：Adapter 让一个"接口不兼容"的现有类，通过包一层适配，变得满足调用方期待的抽象接口。
它是 DIP 的"事后补救"版本：当 Low-level 模块的接口形状是历史遗留、无法修改时，用 Adapter
在中间插入一层，让调用方依然只依赖抽象。

```python
from __future__ import annotations

from typing import Protocol


class DeviceClient(Protocol):
    """现代系统期望的统一契约。"""

    def get_config(self) -> str: ...


class LegacyXmlRpcDevice:
    """遗留系统：接口形状完全不同，无法修改（可能是第三方闭源库）。"""

    def fetch_config_via_xmlrpc(self) -> bytes:
        return b"<config><hostname>LEGACY-01</hostname></config>"


class LegacyDeviceAdapter:
    """Adapter：把 LegacyXmlRpcDevice 的接口适配成 DeviceClient 契约。"""

    def __init__(self, legacy: LegacyXmlRpcDevice) -> None:
        self._legacy = legacy

    def get_config(self) -> str:
        return self._legacy.fetch_config_via_xmlrpc().decode("utf-8")


class DeviceInspectionService:
    def __init__(self, client: DeviceClient) -> None:
        self._client = client

    def inspect(self) -> str:
        return self._client.get_config()


if __name__ == "__main__":
    adapter = LegacyDeviceAdapter(LegacyXmlRpcDevice())
    print(DeviceInspectionService(adapter).inspect())
```

**为什么依赖 DIP**：`DeviceInspectionService` 只认识 `DeviceClient.get_config()`；遗留系统
`LegacyXmlRpcDevice` 完全不需要修改（往往也没法修改），`LegacyDeviceAdapter` 承担了"让 Detail
满足 Abstraction"这一职责。没有 DIP 的抽象契约，Adapter 就没有"该适配成什么形状"的目标。

## Facade（外观）

**关系**：Facade 为一组复杂子系统提供一个简化的统一入口。它本身通常**不直接**制造抽象倒置，
但一个设计良好的 Facade 内部会依赖各子系统的抽象接口（而不是具体实现），这样 Facade 自己也
可以在不同环境下拼装不同的子系统实现。

```python
from __future__ import annotations

from typing import Protocol


class BuildSystem(Protocol):
    def build(self) -> bool: ...


class TestRunner(Protocol):
    def run_tests(self) -> bool: ...


class Deployer(Protocol):
    def deploy(self) -> bool: ...


class MakeBuildSystem:
    def build(self) -> bool:
        print("[make] building...")
        return True


class PytestRunner:
    def run_tests(self) -> bool:
        print("[pytest] running tests...")
        return True


class K8sDeployer:
    def deploy(self) -> bool:
        print("[kubectl] deploying...")
        return True


class ReleaseFacade:
    """Facade：内部依赖三个抽象子系统接口，对外只暴露一个 release() 方法。"""

    def __init__(self, build: BuildSystem, test: TestRunner, deploy: Deployer) -> None:
        self._build = build
        self._test = test
        self._deploy = deploy

    def release(self) -> bool:
        return self._build.build() and self._test.run_tests() and self._deploy.deploy()


if __name__ == "__main__":
    facade = ReleaseFacade(MakeBuildSystem(), PytestRunner(), K8sDeployer())
    print("release success:", facade.release())
```

**为什么依赖 DIP**：如果 `ReleaseFacade` 内部直接 `import` 并调用具体的 `make`/`pytest`/
`kubectl` 命令行细节，就无法在测试环境用假的子系统验证"三步骤按顺序执行、任一步失败就中止"这
条编排逻辑。依赖抽象子系统接口，让 Facade 的编排逻辑本身也可测试、可替换。

## Command（命令）

**关系**：Command 把"一个请求"封装成一个对象（而不是直接的方法调用），调用方（Invoker）只
依赖 `Command` 抽象的 `execute()`/`undo()`，不知道具体命令要做什么。这让"记录命令历史、支持
撤销、支持排队/延迟执行"成为可能。

```python
from __future__ import annotations

from typing import Protocol


class DeviceConfigCommand(Protocol):
    def execute(self) -> None: ...
    def undo(self) -> None: ...


class SetMtuCommand:
    def __init__(self, device_state: dict, new_mtu: int) -> None:
        self._state = device_state
        self._new_mtu = new_mtu
        self._old_mtu: int | None = None

    def execute(self) -> None:
        self._old_mtu = self._state.get("mtu")
        self._state["mtu"] = self._new_mtu
        print(f"[Command] mtu -> {self._new_mtu}")

    def undo(self) -> None:
        self._state["mtu"] = self._old_mtu
        print(f"[Command] mtu rolled back -> {self._old_mtu}")


class ConfigCommandInvoker:
    """Invoker：只依赖 DeviceConfigCommand 抽象，维护一个可撤销的历史栈。"""

    def __init__(self) -> None:
        self._history: list[DeviceConfigCommand] = []

    def run(self, command: DeviceConfigCommand) -> None:
        command.execute()
        self._history.append(command)

    def undo_last(self) -> None:
        if self._history:
            self._history.pop().undo()


if __name__ == "__main__":
    device_state = {"mtu": 1500}
    invoker = ConfigCommandInvoker()
    invoker.run(SetMtuCommand(device_state, 9000))
    print(device_state)
    invoker.undo_last()
    print(device_state)
```

**为什么依赖 DIP**：`ConfigCommandInvoker` 不知道命令具体改的是 MTU、VLAN 还是接口状态——它
只管"执行并记录历史，需要时调用 undo"。新增一种配置命令（`SetVlanCommand`）不需要改动
Invoker，撤销栈天然支持任意命令类型的混合历史。

## Repository（仓储）

**关系**：Repository 是 DIP 在"数据访问"场景下最经典的应用——领域/应用层定义
`Repository` 抽象接口，基础设施层实现具体存储技术。Part 2/3 已给出多个完整示例
（`OrderRepository`/`StockRepository`），这里换一个角度：展示 Repository 抽象如何配合
**Specification 模式**支持复杂查询而不泄漏存储细节。

```python
from __future__ import annotations

from dataclasses import dataclass
from typing import Protocol


@dataclass(frozen=True, slots=True)
class Device:
    device_id: str
    status: str
    firmware: str


class DeviceSpecification(Protocol):
    def is_satisfied_by(self, device: Device) -> bool: ...


class OfflineSpec:
    def is_satisfied_by(self, device: Device) -> bool:
        return device.status == "offline"


class OutdatedFirmwareSpec:
    def __init__(self, min_version: str) -> None:
        self._min_version = min_version

    def is_satisfied_by(self, device: Device) -> bool:
        return device.firmware < self._min_version


class DeviceRepository(Protocol):
    def find_matching(self, spec: DeviceSpecification) -> list[Device]: ...


class InMemoryDeviceRepository:
    def __init__(self, devices: list[Device]) -> None:
        self._devices = devices

    def find_matching(self, spec: DeviceSpecification) -> list[Device]:
        return [d for d in self._devices if spec.is_satisfied_by(d)]


if __name__ == "__main__":
    repo: DeviceRepository = InMemoryDeviceRepository(
        [
            Device("RPD-01", "online", "1.2.0"),
            Device("RPD-02", "offline", "1.5.0"),
            Device("RPD-03", "online", "1.0.0"),
        ]
    )
    print("offline:", repo.find_matching(OfflineSpec()))
    print("outdated:", repo.find_matching(OutdatedFirmwareSpec("1.2.0")))
```

**为什么依赖 DIP**：查询条件本身也被抽象成 `DeviceSpecification` 契约，`DeviceRepository`
既不知道具体存了多少设备、也不知道调用方到底想查什么条件——存储技术、查询条件、业务规则三者
被 DIP 彻底解耦。

## Service Layer（服务层）

**关系**：Service Layer（应用服务层）是"用例编排"的家——它把领域对象、Repository、外部
Gateway 组合起来完成一个完整业务流程。Service Layer 本身就是 DIP 里 High-level Module 的
典型代表：它只依赖各种抽象接口，从不直接依赖任何 Detail。

```python
from __future__ import annotations

from dataclasses import dataclass
from typing import Protocol


@dataclass(slots=True)
class Order:
    order_id: str
    amount_cents: int
    paid: bool = False


class OrderRepository(Protocol):
    def save(self, order: Order) -> None: ...


class PaymentGateway(Protocol):
    def charge(self, amount_cents: int) -> bool: ...


class NotificationSender(Protocol):
    def send(self, message: str) -> None: ...


class OrderApplicationService:
    """Service Layer：编排领域对象与三个抽象协作者，完成"结账"这个完整用例。"""

    def __init__(
        self,
        repo: OrderRepository,
        gateway: PaymentGateway,
        notifier: NotificationSender,
    ) -> None:
        self._repo = repo
        self._gateway = gateway
        self._notifier = notifier

    def checkout(self, order: Order) -> None:
        if not self._gateway.charge(order.amount_cents):
            raise RuntimeError("payment failed")
        order.paid = True
        self._repo.save(order)
        self._notifier.send(f"order {order.order_id} paid successfully")


class InMemoryOrderRepository:
    def save(self, order: Order) -> None:
        print(f"[Repo] saved {order}")


class AlwaysApprovePaymentGateway:
    def charge(self, amount_cents: int) -> bool:
        print(f"[Gateway] charging {amount_cents}")
        return True


class PrintNotifier:
    def send(self, message: str) -> None:
        print(f"[Notify] {message}")


if __name__ == "__main__":
    service = OrderApplicationService(
        InMemoryOrderRepository(), AlwaysApprovePaymentGateway(), PrintNotifier()
    )
    service.checkout(Order("O-1", 4999))
```

**为什么依赖 DIP**：`OrderApplicationService` 的构造函数是"这个用例需要哪些抽象协作者"的
一份清单，三个协作者全部是抽象类型。这正是 Service Layer 模式的核心价值——把"编排顺序"这个
业务知识和"每一步具体怎么做"的技术细节彻底分离。

## Hexagonal Architecture 与 Ports & Adapters

**关系**：这两个名字（Alistair Cockburn 提出 Hexagonal Architecture，后来他自己也用
Ports & Adapters 指代同一个概念）描述的是**同一种架构风格**：应用核心（domain +
application）位于"六边形"中心，只对外暴露一组抽象接口（**Port**），外部世界（数据库、消息
队列、CLI、HTTP API、第三方系统）通过实现或调用这些 Port 的**Adapter** 与核心交互。
"Port" 就是 DIP 里的 Abstraction，"Adapter" 就是 Detail——Hexagonal Architecture 本质上是
"把 DIP 系统性地应用到整个应用边界"的架构级实践。

```python
from __future__ import annotations

from typing import Protocol


# ---- Port（六边形边界上的抽象接口，属于应用核心）----

class CreateOrderPort(Protocol):
    """Inbound/Driving Port：外部世界"驱动"核心执行用例的入口。"""

    def create_order(self, customer_id: str, amount_cents: int) -> str: ...


class OrderRepositoryPort(Protocol):
    """Outbound/Driven Port：核心"驱动"外部基础设施完成持久化。"""

    def save(self, order_id: str, customer_id: str, amount_cents: int) -> None: ...


# ---- Application Core：只依赖 Port，位于六边形中心 ----

class OrderUseCase(CreateOrderPort):
    def __init__(self, repo: OrderRepositoryPort) -> None:
        self._repo = repo

    def create_order(self, customer_id: str, amount_cents: int) -> str:
        order_id = f"O-{customer_id}-{amount_cents}"
        self._repo.save(order_id, customer_id, amount_cents)
        return order_id


# ---- Adapter：六边形外部，实现/调用 Port ----

class SqlOrderRepositoryAdapter:
    """Driven Adapter：实现 OrderRepositoryPort，对接真实数据库。"""

    def save(self, order_id: str, customer_id: str, amount_cents: int) -> None:
        print(f"[SQL] INSERT order {order_id} for {customer_id}, amount={amount_cents}")


class HttpControllerAdapter:
    """Driving Adapter：把 HTTP 请求转换成对 CreateOrderPort 的调用。"""

    def __init__(self, use_case: CreateOrderPort) -> None:
        self._use_case = use_case

    def handle_post_orders(self, request_body: dict) -> dict:
        order_id = self._use_case.create_order(request_body["customer_id"], request_body["amount_cents"])
        return {"order_id": order_id, "status": 201}


if __name__ == "__main__":
    core = OrderUseCase(SqlOrderRepositoryAdapter())
    controller = HttpControllerAdapter(core)
    print(controller.handle_post_orders({"customer_id": "C-1", "amount_cents": 2999}))
```

### 六边形架构图

```
                         Driving Adapters (左侧，驱动核心)
                    +-----------------------------------+
                    |   HttpControllerAdapter            |
                    |   CliAdapter / GrpcAdapter (示例)   |
                    +------------------+------------------+
                                       | 调用 Port
                                       v
                    +-----------------------------------+
                    |        Application Core            |
                    |    OrderUseCase (实现 CreateOrderPort)|
                    |    只依赖 Port，不知道任何 Adapter    |
                    +------------------+------------------+
                                       | 调用 Port
                                       v
                    +-----------------------------------+
                    |   Driven Adapters (右侧，被核心驱动)  |
                    |   SqlOrderRepositoryAdapter         |
                    +-----------------------------------+
```

**为什么依赖 DIP**：无论是 Driving Port 还是 Driven Port，箭头永远指向 Application Core
定义的抽象；Adapter（无论左侧右侧）都依赖 Port，而不是 Core 依赖 Adapter。这正是 DIP 里
"高层不依赖低层，双方都依赖抽象"这句话在架构级别的完整实现——六边形的"边界线"本身就是一圈
由 Port 组成的抽象层。

## Clean Architecture

**关系**：Robert C. Martin 的 Clean Architecture 把系统分成 Entities（企业业务规则）→
Use Cases（应用业务规则）→ Interface Adapters（控制器/展示器/网关实现）→ Frameworks &
Drivers（Web 框架/数据库/UI）四层同心圆，核心规则是"**依赖只能指向圆心**"（The Dependency
Rule）。这条规则就是 DIP 的架构级表达形式：越靠外层越"细节"，越靠内层越"抽象/稳定"，源代码
依赖必须从外圈指向内圈。

```python
from __future__ import annotations

from dataclasses import dataclass
from typing import Protocol


# ---- Entities（最内圈：企业级业务规则，零外部依赖）----

@dataclass(slots=True)
class Order:
    order_id: str
    amount_cents: int

    def validate(self) -> None:
        if self.amount_cents <= 0:
            raise ValueError("amount must be positive")


# ---- Use Cases（第二圈：只依赖 Entities 和自己定义的抽象端口）----

class OrderOutputPort(Protocol):
    def present(self, order_id: str) -> None: ...


class OrderGatewayPort(Protocol):
    def persist(self, order: Order) -> None: ...


class PlaceOrderUseCase:
    def __init__(self, gateway: OrderGatewayPort, output: OrderOutputPort) -> None:
        self._gateway = gateway
        self._output = output

    def execute(self, order: Order) -> None:
        order.validate()
        self._gateway.persist(order)
        self._output.present(order.order_id)


# ---- Interface Adapters（第三圈：实现端口，转换成外层框架能理解的形式）----

class SqlOrderGateway:
    def persist(self, order: Order) -> None:
        print(f"[SQL] persist {order}")


class JsonConsolePresenter:
    def present(self, order_id: str) -> None:
        print(f'{{"order_id": "{order_id}", "status": "created"}}')


# ---- Frameworks & Drivers（最外圈：组装一切，即 main）----

if __name__ == "__main__":
    use_case = PlaceOrderUseCase(SqlOrderGateway(), JsonConsolePresenter())
    use_case.execute(Order("O-1", 4999))
```

### Clean Architecture 同心圆图

```
+-----------------------------------------------------------+
|  Frameworks & Drivers (Web/DB/UI)                            |
|  +-------------------------------------------------------+  |
|  |  Interface Adapters (Controllers/Gateways/Presenters)   |  |
|  |  +---------------------------------------------------+ |  |
|  |  |  Use Cases (PlaceOrderUseCase)                      | |  |
|  |  |  +-------------------------------------------------+| |  |
|  |  |  |  Entities (Order)                                || |  |
|  |  |  +-------------------------------------------------+| |  |
|  |  +---------------------------------------------------+ |  |
|  +-------------------------------------------------------+  |
+-----------------------------------------------------------+
          依赖方向：永远从外圈箭头指向内圈，绝不反向
```

**为什么依赖 DIP**：`PlaceOrderUseCase` 位于第二圈，依赖它自己定义的 `OrderGatewayPort`/
`OrderOutputPort`；第三圈的 `SqlOrderGateway`/`JsonConsolePresenter` 反过来依赖并实现这两
个端口。依赖箭头从外圈（细节更多）指向内圈（更稳定），Entities 不依赖任何人——这与 DIP 的
"High-level 不依赖 Low-level"完全一致，只是 Clean Architecture 把它系统化成了四个同心圆。

## DDD（Domain-Driven Design）

**关系**：DDD 战术层面的核心接口——Repository、Domain Service、Anti-Corruption Layer——
全部建立在 DIP 之上。DDD 强调"领域模型是系统的核心资产"，因此**领域层绝不能依赖基础设施层**，
这与 DIP 的表述完全同构：领域层是 High-level Module，基础设施层是 Low-level Module，两者都
依赖领域层自己定义的抽象（Repository 接口、Domain Service 接口）。

```python
from __future__ import annotations

from dataclasses import dataclass, field
from typing import Protocol


# ---- Domain 层：聚合根 + 领域仓储接口（抽象定义在 Domain 内部）----

@dataclass(slots=True)
class LineItem:
    sku: str
    quantity: int


@dataclass(slots=True)
class ShoppingCart:
    """聚合根：封装不变量（比如单笔购物车商品种类不能超过 20）。"""

    cart_id: str
    items: list[LineItem] = field(default_factory=list)

    def add_item(self, sku: str, quantity: int) -> None:
        if len(self.items) >= 20:
            raise ValueError("cart cannot exceed 20 distinct items")
        self.items.append(LineItem(sku, quantity))


class ShoppingCartRepository(Protocol):
    """Domain 层定义的仓储接口——这是"领域不依赖基础设施"的关键。"""

    def save(self, cart: ShoppingCart) -> None: ...
    def get(self, cart_id: str) -> ShoppingCart | None: ...


# ---- Application 层：只依赖 Domain 层的抽象 ----

class AddToCartUseCase:
    def __init__(self, repo: ShoppingCartRepository) -> None:
        self._repo = repo

    def execute(self, cart_id: str, sku: str, quantity: int) -> None:
        cart = self._repo.get(cart_id) or ShoppingCart(cart_id)
        cart.add_item(sku, quantity)
        self._repo.save(cart)


# ---- Infrastructure 层：实现 Domain 定义的抽象，反过来依赖 Domain ----

class InMemoryShoppingCartRepository:
    def __init__(self) -> None:
        self._store: dict[str, ShoppingCart] = {}

    def save(self, cart: ShoppingCart) -> None:
        self._store[cart.cart_id] = cart

    def get(self, cart_id: str) -> ShoppingCart | None:
        return self._store.get(cart_id)


if __name__ == "__main__":
    use_case = AddToCartUseCase(InMemoryShoppingCartRepository())
    use_case.execute("cart-1", "SKU-1", 2)
    use_case.execute("cart-1", "SKU-2", 1)
```

**为什么依赖 DIP**：`ShoppingCartRepository` 这个接口定义在 Domain 层（和 `ShoppingCart`
聚合根同一个模块），而不是定义在 Infrastructure 层。`InMemoryShoppingCartRepository` 必须
反过来依赖 Domain 层。这正是 DDD 里经常被强调的"领域层是核心，基础设施层是插件"这句话的
精确技术含义——本质上就是 DIP。

## 十二种模式与 DIP 关系速查

```
+---------------------------------------------------------------------+
| Factory      : 把"选择/创建 Detail"的知识封装起来，让调用方只依赖抽象   |
| Strategy     : 多个 Detail 平等地实现同一 Abstraction，运行期可互换    |
| Bridge       : 两个独立变化维度各自面向抽象，用组合代替继承             |
| Adapter      : 让无法修改的 Detail"事后"满足调用方期待的 Abstraction   |
| Facade       : 简化入口内部仍应依赖各子系统的抽象，而非具体实现         |
| Command      : 把"请求"本身抽象化，Invoker 只依赖 Command 接口        |
| Repository   : DIP 在数据访问场景的经典应用                           |
| Service Layer: High-level Module 的教科书形态，只编排抽象协作者        |
| Hexagonal/Ports&Adapters: 把 DIP 系统化为"六边形边界上的一圈抽象"      |
| Clean Architecture: 把 DIP 系统化为"依赖只能指向圆心"的同心圆规则       |
| DDD          : 领域层定义抽象、基础设施层反向依赖，是 DIP 的战略级体现  |
+---------------------------------------------------------------------+
```

下一步：进入 [Part 8 - network-automation 真实项目案例](dip_08_real_project_case_study.md)，
把以上所有原理落地到一个完整的可运行项目。
