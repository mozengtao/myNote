[← 返回目录](dip_00_index.md) | [上一篇: Part 2 类型系统类](dip_02_type_system_techniques.md)

# Part 3 - 注入方式类：④ Constructor ⑤ Setter ⑥ Method Injection

这三种技巧回答的是**"抽象依赖是在什么时机、通过什么方式，被交到高层对象手上的"**。它们本身不
决定"要不要用抽象"（那是 ABC/Protocol/Duck Typing 决定的），而是决定依赖的**传递时机**：对象
创建时（Constructor）、对象创建后任意时刻（Setter）、还是每次方法调用时（Method）。三者可以
自由组合类型系统技巧使用。

## ④ Constructor Injection（构造函数注入）

### 原理

依赖在对象**创建的那一刻**通过 `__init__` 参数一次性给齐，之后作为不可变（或至少不轻易变化）的
实例属性存在。这是 Python 里最常见、最推荐的 DI 方式。

- **依赖方向**：与 ABC/Protocol 配合使用时，`__init__` 参数类型标注为抽象类型，依赖方向依旧
  是"高层依赖抽象"；Constructor Injection 只是决定了"什么时候把满足这个抽象的对象交给它"。
- **适合规模**：几乎所有规模都适用，是默认首选方式；尤其适合"依赖在对象生命周期内不应该变化"的
  场景（一个 `OrderService` 从创建到销毁应该一直用同一个 `Repository`）。
- **优点**：对象一旦构造完成即处于完全可用状态（没有"忘记设置某个依赖就调用方法"导致的
  `None` 异常）；依赖是显式的、一眼能看出这个对象需要什么协作者；配合 `frozen=True`/私有属性
  可以做到依赖不可变，线程安全性更好。
- **缺点**：如果依赖链很深，构造函数参数会变多（可以用 Factory 或 IoC 容器缓解，见 Part 4/5）；
  不适合"依赖在运行期需要动态替换"的场景（那种情况该用 Setter 或 Method Injection）。

### 示例 1：InventoryService / StockRepository（Postgres / In-Memory）

真实场景：库存服务在整个生命周期内应该固定绑定同一个数据源，不应该中途更换。

```python
from __future__ import annotations

from dataclasses import dataclass
from typing import Protocol


@dataclass(slots=True)
class StockItem:
    sku: str
    quantity: int


class StockRepository(Protocol):
    def get(self, sku: str) -> StockItem | None: ...
    def update_quantity(self, sku: str, quantity: int) -> None: ...


class InMemoryStockRepository:
    def __init__(self) -> None:
        self._data: dict[str, StockItem] = {}

    def seed(self, item: StockItem) -> None:
        self._data[item.sku] = item

    def get(self, sku: str) -> StockItem | None:
        return self._data.get(sku)

    def update_quantity(self, sku: str, quantity: int) -> None:
        self._data[sku].quantity = quantity


class PostgresStockRepository:
    def __init__(self, dsn: str) -> None:
        self._dsn = dsn
        self._data: dict[str, StockItem] = {}
        print(f"[Postgres] connected dsn={dsn}")

    def get(self, sku: str) -> StockItem | None:
        print(f"[Postgres] SELECT * FROM stock WHERE sku = {sku!r}")
        return self._data.get(sku)

    def update_quantity(self, sku: str, quantity: int) -> None:
        print(f"[Postgres] UPDATE stock SET quantity = {quantity} WHERE sku = {sku!r}")
        self._data[sku] = StockItem(sku, quantity)


class InventoryService:
    """High-level Module：依赖在构造时一次性注入，整个生命周期内不再更换。"""

    def __init__(self, repo: StockRepository) -> None:
        self._repo = repo  # 私有属性，构造后不再暴露 setter

    def reserve(self, sku: str, amount: int) -> bool:
        item = self._repo.get(sku)
        if item is None or item.quantity < amount:
            return False
        self._repo.update_quantity(sku, item.quantity - amount)
        return True


if __name__ == "__main__":
    repo = InMemoryStockRepository()
    repo.seed(StockItem("SKU-1", 10))
    service = InventoryService(repo)  # 构造函数注入
    print("reserve 3:", service.reserve("SKU-1", 3))
    print("reserve 100:", service.reserve("SKU-1", 100))
```

#### 对象关系图

```
+-----------------+       +---------------------+
| InventoryService | ----> | StockRepository (P)  |
+-----------------+       +---------------------+
                            ^                 ^
                            |                 |
                  InMemoryStockRepository  PostgresStockRepository
```

#### Object Flow

```
main()
  |
  v
create InMemoryStockRepository() / PostgresStockRepository(dsn)
  |
  v
InventoryService(repo)   # 构造函数注入，一次性完成
  |
  v
service.reserve(sku, amount)
  |
  v
repo.get() -> repo.update_quantity()
```

#### Dependency Graph

```
InventoryService
     |
     v
StockRepository (Abstraction)
     ^
     |
InMemoryStockRepository / PostgresStockRepository (Detail)
```

#### 为什么符合 DIP？

`InventoryService` 只在构造函数签名里出现 `StockRepository` 这个抽象类型，`reserve()` 方法
体内没有任何和具体存储相关的代码。因为依赖是构造时注入且不再改变，`InventoryService` 的整个
生命周期都保持对同一个抽象实现的稳定引用，便于测试（一次构造即可反复调用断言）。

#### 如果违反 DIP

若 `InventoryService.__init__` 内部自己 `self._repo = PostgresStockRepository(DSN)`：

- 单测必须真的连 Postgres，或者用猴子补丁（monkeypatch）侵入式地替换类属性，测试代码变得脆弱。
- 想让同一个服务在不同环境用不同数据源（本地内存 / 测试库 / 生产库），必须在类内部写环境判断
  逻辑，`InventoryService` 承担了本不属于它的"环境配置"职责。

### 示例 2：ShippingService / CourierClient（FedEx / UPS）

真实场景：物流下单服务应该支持多家快递公司，且每个 `ShippingService` 实例应该固定绑定一家。

```python
from __future__ import annotations

from dataclasses import dataclass
from typing import Protocol


@dataclass(frozen=True, slots=True)
class ShipmentLabel:
    tracking_id: str
    carrier: str


class CourierClient(Protocol):
    def create_shipment(self, address: str, weight_kg: float) -> ShipmentLabel: ...


class FedExClient:
    def create_shipment(self, address: str, weight_kg: float) -> ShipmentLabel:
        print(f"[FedEx] SOAP CreateShipment(address={address}, weight={weight_kg})")
        return ShipmentLabel(tracking_id="FDX123456", carrier="fedex")


class UpsClient:
    def create_shipment(self, address: str, weight_kg: float) -> ShipmentLabel:
        print(f"[UPS] REST POST /shipments address={address} weight={weight_kg}")
        return ShipmentLabel(tracking_id="1Z999AA1", carrier="ups")


class ShippingService:
    """High-level Module：构造时绑定唯一的 CourierClient。"""

    def __init__(self, courier: CourierClient) -> None:
        self._courier = courier

    def ship_order(self, address: str, weight_kg: float) -> ShipmentLabel:
        if weight_kg <= 0:
            raise ValueError("weight must be positive")
        return self._courier.create_shipment(address, weight_kg)


if __name__ == "__main__":
    for courier in (FedExClient(), UpsClient()):
        service = ShippingService(courier)
        print(service.ship_order("221B Baker Street", 2.5))
```

#### 对象关系图

```
+------------------+       +-------------------+
|  ShippingService  | ----> |  CourierClient (P) |
+------------------+       +-------------------+
                             ^               ^
                             |               |
                       FedExClient      UpsClient
```

#### Object Flow

```
main()
  |
  v
create FedExClient() / UpsClient()
  |
  v
ShippingService(courier)   # 构造函数注入
  |
  v
service.ship_order(address, weight)
  |
  v
courier.create_shipment(...)
```

#### Dependency Graph

```
ShippingService
     |
     v
CourierClient (Abstraction)
     ^
     |
FedExClient / UpsClient (Detail)
```

#### 为什么符合 DIP？

"重量必须为正"这条业务校验规则和"用 SOAP 还是 REST 创建运单"这个技术细节被彻底分离。新增
DHL、顺丰只需要新写一个 `CourierClient` 实现，`ShippingService` 不用改。

#### 如果违反 DIP

若把快递商作为字符串参数、内部 `if carrier == "fedex": ...`：

- `ShippingService` 要同时了解所有快递公司的 SDK 调用方式，任何一家 SDK 升级都要改这个类。
- 无法给"重量校验"单独写单测，因为它和具体快递商分支逻辑绑在一起。

### 示例 3：AuthService / IdentityProvider（LDAP / OAuth）

真实场景：认证服务在部署时固定绑定一种身份源（企业内网用 LDAP，SaaS 产品用 OAuth）。

```python
from __future__ import annotations

from dataclasses import dataclass
from typing import Protocol


@dataclass(frozen=True, slots=True)
class AuthenticatedUser:
    user_id: str
    display_name: str


class IdentityProvider(Protocol):
    def authenticate(self, username: str, credential: str) -> AuthenticatedUser | None: ...


class LdapIdentityProvider:
    def __init__(self, ldap_uri: str) -> None:
        self._ldap_uri = ldap_uri

    def authenticate(self, username: str, credential: str) -> AuthenticatedUser | None:
        print(f"[LDAP {self._ldap_uri}] BIND uid={username}")
        return AuthenticatedUser(user_id=username, display_name=username.title())


class OAuthIdentityProvider:
    def __init__(self, issuer: str) -> None:
        self._issuer = issuer

    def authenticate(self, username: str, credential: str) -> AuthenticatedUser | None:
        print(f"[OAuth {self._issuer}] verifying token for {username}")
        return AuthenticatedUser(user_id=username, display_name=username.title())


class AuthService:
    """High-level Module：构造时绑定唯一的身份源实现。"""

    def __init__(self, provider: IdentityProvider) -> None:
        self._provider = provider

    def login(self, username: str, credential: str) -> AuthenticatedUser:
        user = self._provider.authenticate(username, credential)
        if user is None:
            raise PermissionError("invalid credentials")
        return user


if __name__ == "__main__":
    for provider in (LdapIdentityProvider("ldap://corp.local"), OAuthIdentityProvider("https://auth.corp.com")):
        print(AuthService(provider).login("mmorris", "secret"))
```

#### 对象关系图

```
+--------------+       +------------------------+
|  AuthService  | ----> | IdentityProvider (P)   |
+--------------+       +------------------------+
                          ^                    ^
                          |                    |
                LdapIdentityProvider    OAuthIdentityProvider
```

#### Object Flow

```
main() / 应用启动
  |
  v
根据部署环境创建 LdapIdentityProvider 或 OAuthIdentityProvider
  |
  v
AuthService(provider)  # 构造函数注入，部署期确定，运行期不变
  |
  v
service.login(username, credential)
  |
  v
provider.authenticate(...)
```

#### Dependency Graph

```
AuthService
     |
     v
IdentityProvider (Abstraction)
     ^
     |
LdapIdentityProvider / OAuthIdentityProvider (Detail)
```

#### 为什么符合 DIP？

企业级软件经常需要"同一套代码，按客户部署环境切换身份源"。`AuthService` 只依赖
`authenticate()` 契约，具体是 LDAP bind 还是 OAuth token 校验完全在 Detail 层解决。

#### 如果违反 DIP

若 `AuthService` 内部写死 `import ldap3` 并调用其连接 API：

- 无法给不需要 LDAP 环境的客户交付同一份代码，必须维护多个分支/多份 `AuthService` 拷贝。
- 单测必须伪造 LDAP 服务器协议细节，而不是简单构造一个假的 `IdentityProvider`。

## ⑤ Setter Injection（属性/方法注入）

### 原理

依赖在对象创建**之后**，通过一个专门的 setter 方法（或可写属性）被设置或替换。适用于"依赖在
对象生命周期中可能需要被替换"，或者"依赖是可选的、可以有一个默认值，需要时再覆盖"的场景。

- **依赖方向**：不变，仍然是"高层依赖抽象类型"；变化的只是注入时机——从"构造时固定"变成
  "运行期可重新绑定"。
- **适合规模**：长生命周期的对象（daemon、worker、单例服务）需要在运行期热切换依赖（比如
  日志后端、监控后端、Feature Flag 提供方）时最合适；小型脚本一般用不到。
- **优点**：允许运行期动态替换依赖，不需要重新创建整个对象；配合默认实现可以让某些依赖变成
  "可选"的（不设置就用默认的空实现/Console 实现）。
- **缺点**：对象存在"构造完成但依赖还没设置好"的中间状态，如果忘记调用 setter，可能在运行时
  才报错（不像 Constructor Injection 那样"构造即完整"）；依赖的可变性也让并发场景下的线程安全
  变得更复杂。

### 示例 1：长驻 Worker 的 Logger 热切换

真实场景：一个常驻后台的巡检 Worker，启动时用 Console 输出，接入生产监控后可以动态切换到
集中式日志而不需要重启进程。

```python
from __future__ import annotations

from typing import Protocol


class Logger(Protocol):
    def log(self, message: str) -> None: ...


class ConsoleLogger:
    def log(self, message: str) -> None:
        print(f"[console] {message}")


class CentralizedLogger:
    def __init__(self, endpoint: str) -> None:
        self._endpoint = endpoint

    def log(self, message: str) -> None:
        print(f"[POST {self._endpoint}/logs] {message}")


class PollingWorker:
    """High-level Module：默认使用 ConsoleLogger，运行期可通过 setter 热切换。"""

    def __init__(self) -> None:
        self._logger: Logger = ConsoleLogger()  # 有一个安全的默认实现

    def set_logger(self, logger: Logger) -> None:
        self._logger = logger

    def poll_once(self, device_id: str) -> None:
        self._logger.log(f"polling {device_id}")


if __name__ == "__main__":
    worker = PollingWorker()
    worker.poll_once("RPD-01")  # 用默认 ConsoleLogger

    worker.set_logger(CentralizedLogger("https://logs.corp.com"))  # 运行期切换
    worker.poll_once("RPD-02")
```

#### 对象关系图

```
+----------------+  默认  +----------------+
|  PollingWorker  | -----> | ConsoleLogger   |
+-------+--------+        +----------------+
        | set_logger() 运行期替换
        v
+----------------------+
|  CentralizedLogger     |
+----------------------+
                         ^
     两者都实现 Logger (Protocol)
```

#### Object Flow

```
main()
  |
  v
PollingWorker()  # 内部已有默认 ConsoleLogger，可直接工作
  |
  v
worker.poll_once(...)   # 用默认 logger
  |
  v
worker.set_logger(CentralizedLogger(...))   # setter 注入，替换依赖
  |
  v
worker.poll_once(...)   # 后续调用改用新 logger
```

#### Dependency Graph

```
PollingWorker
     |
     v
Logger (Abstraction)
     ^
     |
ConsoleLogger / CentralizedLogger (Detail)
```

#### 为什么符合 DIP？

`PollingWorker` 全程只认识 `Logger` 这个契约，不知道背后是打印到终端还是 POST 到日志平台。
Setter Injection 额外解决了"这个 Worker 是长驻进程，接入监控系统的时机往往晚于进程启动"这个
实际工程问题——不需要为了换个日志后端重启整个巡检进程。

#### 如果违反 DIP

若 `PollingWorker` 内部固定 `print(...)`：

- 想要接入集中式日志，必须停掉 Worker、改代码、重新部署，而不是热切换。
- 无法在同一个 Worker 实例上，为不同批次的轮询任务临时切换日志详细程度或目的地。

### 示例 2：Feature Flag Provider 的运行期替换

真实场景：应用启动时先用本地静态配置作为 Feature Flag 来源，等到远程配置中心就绪后，再切换
成远程动态源，避免启动时序依赖阻塞主流程。

```python
from __future__ import annotations

from typing import Protocol


class FeatureFlagProvider(Protocol):
    def is_enabled(self, flag: str) -> bool: ...


class StaticFlagProvider:
    def __init__(self, flags: dict[str, bool]) -> None:
        self._flags = flags

    def is_enabled(self, flag: str) -> bool:
        return self._flags.get(flag, False)


class RemoteFlagProvider:
    def __init__(self, endpoint: str) -> None:
        self._endpoint = endpoint

    def is_enabled(self, flag: str) -> bool:
        print(f"[GET {self._endpoint}/flags/{flag}]")
        return True  # 模拟远程返回


class CheckoutFeatureGate:
    """High-level Module：启动时用静态兜底配置，就绪后可切换为远程配置。"""

    def __init__(self, provider: FeatureFlagProvider) -> None:
        self._provider = provider

    def set_provider(self, provider: FeatureFlagProvider) -> None:
        self._provider = provider

    def new_checkout_flow_enabled(self) -> bool:
        return self._provider.is_enabled("new_checkout_flow")


if __name__ == "__main__":
    gate = CheckoutFeatureGate(StaticFlagProvider({"new_checkout_flow": False}))
    print("during boot:", gate.new_checkout_flow_enabled())

    gate.set_provider(RemoteFlagProvider("https://flags.corp.com"))
    print("after remote ready:", gate.new_checkout_flow_enabled())
```

#### 对象关系图

```
+----------------------+  启动期  +--------------------+
|  CheckoutFeatureGate   | -------> | StaticFlagProvider  |
+----------+-----------+          +--------------------+
           | set_provider() 运行期替换
           v
+----------------------+
|  RemoteFlagProvider    |
+----------------------+
                         ^
    两者都实现 FeatureFlagProvider (Protocol)
```

#### Object Flow

```
应用启动
  |
  v
CheckoutFeatureGate(StaticFlagProvider(...))   # 构造时给一个安全兜底
  |
  v
远程配置中心连接就绪
  |
  v
gate.set_provider(RemoteFlagProvider(...))    # setter 注入替换
  |
  v
后续调用改用远程动态判断
```

#### Dependency Graph

```
CheckoutFeatureGate
     |
     v
FeatureFlagProvider (Abstraction)
     ^
     |
StaticFlagProvider / RemoteFlagProvider (Detail)
```

#### 为什么符合 DIP？

业务代码"要不要走新结账流程"只依赖 `is_enabled()` 这一个方法，Feature Flag 来源从静态配置
平滑过渡到远程服务的过程，对调用方完全透明。

#### 如果违反 DIP

若业务代码直接读取一个全局字典或直接发 HTTP 请求判断：

- 启动阶段远程配置中心还没连上时会直接抛异常或卡住主流程。
- 无法在测试里简单地把 Flag 状态设为 True/False，需要 mock 掉具体的 HTTP 客户端。

### 示例 3：Metrics Backend 的运行期切换（Prometheus / StatsD）

真实场景：服务默认使用一个空实现（NoOp）避免启动时因监控未就绪而报错，监控组件初始化完成后
再注入真正的实现。

```python
from __future__ import annotations

from typing import Protocol


class MetricsSink(Protocol):
    def increment(self, name: str, value: int = 1) -> None: ...


class NoOpMetricsSink:
    """安全默认值：什么都不做，避免依赖未就绪时报错。"""

    def increment(self, name: str, value: int = 1) -> None:
        pass


class PrometheusMetricsSink:
    def __init__(self, pushgateway: str) -> None:
        self._pushgateway = pushgateway

    def increment(self, name: str, value: int = 1) -> None:
        print(f"[Prometheus push {self._pushgateway}] {name} += {value}")


class StatsdMetricsSink:
    def __init__(self, host: str, port: int = 8125) -> None:
        self._addr = f"{host}:{port}"

    def increment(self, name: str, value: int = 1) -> None:
        print(f"[StatsD UDP {self._addr}] {name}:{value}|c")


class DeviceOnboardingService:
    """High-level Module：默认 NoOp，监控组件就绪后热切换。"""

    def __init__(self) -> None:
        self._metrics: MetricsSink = NoOpMetricsSink()

    def set_metrics_sink(self, sink: MetricsSink) -> None:
        self._metrics = sink

    def onboard(self, device_id: str) -> None:
        print(f"onboarding {device_id}")
        self._metrics.increment("device.onboarded")


if __name__ == "__main__":
    service = DeviceOnboardingService()
    service.onboard("RPD-01")  # 监控未就绪，走 NoOp

    service.set_metrics_sink(PrometheusMetricsSink("http://pushgw:9091"))
    service.onboard("RPD-02")
```

#### 对象关系图

```
+---------------------------+  默认  +------------------+
|  DeviceOnboardingService    | -----> | NoOpMetricsSink   |
+-------------+---------------+       +------------------+
              | set_metrics_sink() 运行期替换
              v
+------------------------+     +--------------------+
|  PrometheusMetricsSink   |     |  StatsdMetricsSink  |
+------------------------+     +--------------------+
                 ^                       ^
                 均实现 MetricsSink (Protocol)
```

#### Object Flow

```
应用启动
  |
  v
DeviceOnboardingService()  # 内置 NoOpMetricsSink，立即可用
  |
  v
service.onboard(...)  # 此时指标被静默丢弃
  |
  v
监控组件初始化完成
  |
  v
service.set_metrics_sink(PrometheusMetricsSink(...))
  |
  v
service.onboard(...)  # 此时指标真正上报
```

#### Dependency Graph

```
DeviceOnboardingService
     |
     v
MetricsSink (Abstraction)
     ^
     |
NoOpMetricsSink / PrometheusMetricsSink / StatsdMetricsSink (Detail)
```

#### 为什么符合 DIP？

业务方法 `onboard()` 从头到尾不知道指标最终会不会被真正发送出去，也不知道发送到 Prometheus
还是 StatsD。"Null Object 模式 + Setter Injection"结合，解决了"依赖尚未就绪时对象也要能正常
工作"的初始化时序问题。

#### 如果违反 DIP

若 `onboard()` 内部直接调用某个全局 Prometheus 客户端：

- 服务启动早期（监控组件还没连上）调用 `onboard()` 会直接抛异常，必须额外写一堆
  `if metrics_client is not None` 的空判断，污染业务方法。
- 无法在测试环境彻底"静音"指标上报，只能 mock 全局对象。

## ⑥ Method Injection（方法参数注入）

### 原理

依赖既不在构造时固定，也不通过 setter 长期持有，而是**每次调用方法时作为参数临时传入**。适用
于"依赖会随每次调用而变化"的场景——这是三种注入方式里粒度最细的一种。

- **依赖方向**：不变；变化的是注入时机精确到"每次方法调用"，宿主对象本身完全不持有这个依赖的
  任何状态。
- **适合规模**：无状态的工具类/纯函数式服务、每次调用都可能用不同策略的场景（如按订单动态选
  折扣策略、按请求动态选路由策略）。
- **优点**：宿主对象保持无状态、天然线程安全；同一个对象可以在同一时刻被多个调用方以不同依赖
  并发使用，互不干扰。
- **缺点**：如果几乎每次调用都传同一个依赖，会显得啰嗦（这时候更适合 Constructor Injection）；
  调用方需要自己负责"选择正确的依赖传进去"，方法签名会变长。

### 示例 1：按订单动态选择的 Pricing Strategy

真实场景：同一个 `PricingEngine` 在同一秒内，可能要用"普通折扣"给普通订单定价，同时用
"会员折扣"给另一个订单定价——依赖必须逐次调用传入，不能固化在对象内部。

```python
from __future__ import annotations

from typing import Protocol


class PricingStrategy(Protocol):
    def apply(self, base_price: float) -> float: ...


class PercentageDiscount:
    def __init__(self, percent: float) -> None:
        self._percent = percent

    def apply(self, base_price: float) -> float:
        return base_price * (1 - self._percent / 100)


class LoyaltyDiscount:
    def apply(self, base_price: float) -> float:
        return base_price - 5.0 if base_price > 20 else base_price


class NoDiscount:
    def apply(self, base_price: float) -> float:
        return base_price


class PricingEngine:
    """High-level Module：无状态，策略通过方法参数逐次注入。"""

    def calculate(self, base_price: float, strategy: PricingStrategy) -> float:
        final_price = strategy.apply(base_price)
        return round(max(final_price, 0.0), 2)


if __name__ == "__main__":
    engine = PricingEngine()  # 单例即可，无状态
    print(engine.calculate(100.0, PercentageDiscount(10)))
    print(engine.calculate(30.0, LoyaltyDiscount()))
    print(engine.calculate(15.0, NoDiscount()))
```

#### 对象关系图

```
+---------------+   每次调用传入   +------------------------+
|  PricingEngine  | --------------> |  PricingStrategy (P)    |
+---------------+                 +------------------------+
                                    ^         ^          ^
                                    |         |          |
                        PercentageDiscount LoyaltyDiscount NoDiscount
```

#### Object Flow

```
调用方 A: engine.calculate(100.0, PercentageDiscount(10))
调用方 B: engine.calculate(30.0, LoyaltyDiscount())
  |
  v
（两次调用互不干扰，PricingEngine 自身不持有任何策略状态）
  |
  v
strategy.apply(base_price) -> 折后价
```

#### Dependency Graph

```
PricingEngine
     |
     v (每次调用)
PricingStrategy (Abstraction)
     ^
     |
PercentageDiscount / LoyaltyDiscount / NoDiscount (Detail)
```

#### 为什么符合 DIP？

`PricingEngine.calculate()` 不知道折扣是百分比、满减还是无折扣——它只知道"给我一个能
`apply(base_price)` 的东西"。因为策略是逐次传入而非持有状态，`PricingEngine` 本身可以是
一个全局共享的单例，被任意多个并发请求安全复用。

#### 如果违反 DIP

若 `PricingEngine` 构造时固定一种策略：

- 一个引擎实例只能服务一种折扣类型，普通订单和会员订单要分别 `new` 两个引擎实例，即便定价
  的核心逻辑（取最大值、四舍五入）完全一样，也要重复维护两份对象。
- 无法在一次请求里根据订单动态属性（是否会员、是否首单）临时决定用哪种策略。

### 示例 2：按请求动态验证的 Token Verifier

真实场景：网关的同一个 `RequestGuard` 需要同时支持"内部服务用 JWT""第三方合作伙伴用签名验证"
两种校验方式，具体用哪种由请求本身的来源决定，而不是网关对象固定绑定一种。

```python
from __future__ import annotations

from typing import Protocol


class TokenVerifier(Protocol):
    def verify(self, token: str) -> bool: ...


class JwtVerifier:
    def __init__(self, secret: str) -> None:
        self._secret = secret

    def verify(self, token: str) -> bool:
        print(f"[JWT] verifying with secret[:2]={self._secret[:2]}")
        return token.startswith("jwt_")


class HmacSignatureVerifier:
    def __init__(self, partner_key: str) -> None:
        self._partner_key = partner_key

    def verify(self, token: str) -> bool:
        print(f"[HMAC partner_key[:3]={self._partner_key[:3]}] verifying signature")
        return token.startswith("sig_")


class RequestGuard:
    """High-level Module：验证方式随每次请求传入，自身无状态。"""

    def authorize(self, token: str, verifier: TokenVerifier) -> bool:
        if not token:
            return False
        return verifier.verify(token)


if __name__ == "__main__":
    guard = RequestGuard()
    print(guard.authorize("jwt_abc123", JwtVerifier("s3cr3t")))
    print(guard.authorize("sig_xyz789", HmacSignatureVerifier("partner-key-001")))
```

#### 对象关系图

```
+---------------+  每次请求传入  +-----------------------+
|  RequestGuard   | -------------> |  TokenVerifier (P)     |
+---------------+               +-----------------------+
                                  ^                    ^
                                  |                    |
                        JwtVerifier         HmacSignatureVerifier
```

#### Object Flow

```
内部服务请求 -> guard.authorize(token, JwtVerifier(secret))
合作伙伴请求 -> guard.authorize(token, HmacSignatureVerifier(key))
  |
  v
verifier.verify(token) -> True/False
```

#### Dependency Graph

```
RequestGuard
     |
     v (每次调用)
TokenVerifier (Abstraction)
     ^
     |
JwtVerifier / HmacSignatureVerifier (Detail)
```

#### 为什么符合 DIP？

网关的"没有 token 直接拒绝"这条通用规则和"具体怎么校验 token"完全解耦，且由于校验方式是
按请求传入的，同一个 `RequestGuard` 单例可以同时服务多种客户端类型。

#### 如果违反 DIP

若 `RequestGuard` 内部写死一种校验算法：

- 无法同时支持多种令牌格式，只能为每种客户端类型各写一个网关类，产生大量重复的"无 token 拒绝"
  逻辑。

### 示例 3：按目的地动态选择的 Carrier（路由优化）

真实场景：路由优化器需要在同一批订单里，给不同目的地的包裹分别选用不同承运商的报价接口，
承运商本身不应该被路由优化器长期持有。

```python
from __future__ import annotations

from dataclasses import dataclass
from typing import Protocol


@dataclass(frozen=True, slots=True)
class Quote:
    carrier: str
    cost: float
    eta_days: int


class CarrierQuoteClient(Protocol):
    def quote(self, destination: str, weight_kg: float) -> Quote: ...


class DomesticCarrier:
    def quote(self, destination: str, weight_kg: float) -> Quote:
        return Quote(carrier="domestic", cost=weight_kg * 2.0, eta_days=2)


class InternationalCarrier:
    def quote(self, destination: str, weight_kg: float) -> Quote:
        return Quote(carrier="international", cost=weight_kg * 8.5, eta_days=7)


class RouteOptimizer:
    """High-level Module：承运商按每次调用传入，自身不绑定任何一家。"""

    def cheapest(self, destination: str, weight_kg: float, candidates: list[CarrierQuoteClient]) -> Quote:
        quotes = [c.quote(destination, weight_kg) for c in candidates]
        return min(quotes, key=lambda q: q.cost)


if __name__ == "__main__":
    optimizer = RouteOptimizer()
    best = optimizer.cheapest("Tokyo", 3.0, [DomesticCarrier(), InternationalCarrier()])
    print("cheapest option:", best)
```

#### 对象关系图

```
+-----------------+  每次调用传入  +---------------------------+
|  RouteOptimizer   | -------------> |  CarrierQuoteClient (P)    |
+-----------------+               +---------------------------+
                                    ^                      ^
                                    |                      |
                        DomesticCarrier          InternationalCarrier
```

#### Object Flow

```
optimizer.cheapest(dest, weight, [DomesticCarrier(), InternationalCarrier()])
  |
  v
for c in candidates: c.quote(dest, weight)
  |
  v
min(quotes, key=cost) -> 最优报价
```

#### Dependency Graph

```
RouteOptimizer
     |
     v (每次调用，候选列表)
CarrierQuoteClient (Abstraction)
     ^
     |
DomesticCarrier / InternationalCarrier (Detail)
```

#### 为什么符合 DIP？

`RouteOptimizer` 的"比价取最低"这条算法与具体有哪些承运商、承运商怎么报价完全解耦，调用方
可以按订单目的地动态传入不同的候选承运商集合，`RouteOptimizer` 无需感知承运商总数或种类的
变化。

#### 如果违反 DIP

若 `RouteOptimizer` 构造时固定持有所有承运商列表：

- 新增/下线承运商需要修改 `RouteOptimizer` 的构造逻辑或配置文件耦合点。
- 无法针对"仅比较两家特定承运商"这种一次性场景灵活复用同一个优化器实例。

## 三者速查

```
+-----------------------------------------------------------------+
|                Constructor       Setter          Method          |
+-----------------------------------------------------------------+
| 注入时机     | 创建时一次性   | 创建后任意时刻  | 每次方法调用      |
| 依赖状态     | 不可变(推荐)  | 可变            | 无状态(不持有)    |
| 典型场景     | 常规依赖       | 长驻进程热切换  | 按调用变化的策略   |
| 风险         | 参数可能变多   | 存在未设置的窗口 | 方法签名变长       |
+-----------------------------------------------------------------+
```

下一步：进入 [Part 4 - 创建型类：Factory+DIP / Strategy / Callback Injection / Functional DI](dip_04_creational_techniques.md)。
