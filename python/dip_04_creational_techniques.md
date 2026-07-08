[← 返回目录](dip_00_index.md) | [上一篇: Part 3 注入方式类](dip_03_injection_techniques.md)

# Part 4 - 创建型类：⑦ Factory+DIP ⑧ Strategy ⑨ Callback Injection ⑩ Functional DI

## ⑦ Factory + DIP

### 原理

当"选择哪个具体实现"这件事本身也是一种需要独立管理的知识（依赖配置、环境变量、设备型号等），
就不适合让调用方在业务代码里手写 `if/else` 去 `new` 具体类。**Factory** 把"选择与创建
具体实现"这件事封装成一个独立的函数或类，调用方只从 Factory 拿到一个**已经满足抽象契约**的
对象，全程不需要知道具体类型名。

- **依赖方向**：业务代码依赖抽象类型 + Factory 的返回值类型（仍然是抽象类型）；只有 Factory
  内部才允许 `import` 所有具体实现。可以理解成把"选择依赖"的知识从业务代码里*搬出来*，集中到
  一个专门负责组装的模块（Composition Root 的一种局部形式）。
- **适合规模**：中大型项目；尤其是"具体实现的选择逻辑本身比较复杂"（需要读配置、判断环境、
  做能力探测）的场景。
- **优点**：把"选择逻辑"和"业务逻辑"彻底分离，两者可以独立演化、独立测试；新增实现只需要修改
  Factory 一处。
- **缺点**：多了一层间接性，简单场景下可能显得过度设计；如果 Factory 本身写得很"胖"（塞了太多
  业务判断），也会变成新的耦合热点。

### 示例 1：RepositoryFactory —— 依配置选择 MySQL / PostgreSQL / SQLite

```python
from __future__ import annotations

from typing import Protocol


class OrderRepository(Protocol):
    def save(self, order_id: str) -> None: ...


class MySQLOrderRepository:
    def save(self, order_id: str) -> None:
        print(f"[MySQL] save order {order_id}")


class PostgresOrderRepository:
    def save(self, order_id: str) -> None:
        print(f"[Postgres] save order {order_id}")


class SQLiteOrderRepository:
    def save(self, order_id: str) -> None:
        print(f"[SQLite] save order {order_id}")


class RepositoryFactory:
    """把"选择哪个数据库实现"这件事集中到一处，业务代码不需要知道分支逻辑。"""

    _REGISTRY = {
        "mysql": MySQLOrderRepository,
        "postgres": PostgresOrderRepository,
        "sqlite": SQLiteOrderRepository,
    }

    @classmethod
    def create(cls, backend: str) -> OrderRepository:
        try:
            return cls._REGISTRY[backend]()
        except KeyError as exc:
            raise ValueError(f"unknown backend: {backend}") from exc


class OrderService:
    """High-level Module：只依赖 OrderRepository，甚至不知道 Factory 的存在。"""

    def __init__(self, repo: OrderRepository) -> None:
        self._repo = repo

    def checkout(self, order_id: str) -> None:
        self._repo.save(order_id)


if __name__ == "__main__":
    backend = "postgres"  # 通常来自环境变量 / 配置文件
    repo = RepositoryFactory.create(backend)
    OrderService(repo).checkout("O-100")
```

#### 对象关系图

```
+-------------------+ create() +--------------------+
|  RepositoryFactory  | -------> |  OrderRepository (P) |
+-------------------+          +--------------------+
                                  ^        ^        ^
                                  |        |        |
                             MySQL   Postgres   SQLite
                                  ^
                                  | 依赖(返回值类型)
                          +----------------+
                          |  OrderService   |
                          +----------------+
```

#### Object Flow

```
main()
  |
  v
读取配置 backend="postgres"
  |
  v
RepositoryFactory.create(backend)   # Factory 内部做分支判断
  |
  v
返回 PostgresOrderRepository()（以 OrderRepository 类型暴露）
  |
  v
OrderService(repo)   # 业务代码只拿到抽象类型
  |
  v
service.checkout(order_id) -> repo.save(order_id)
```

#### Dependency Graph

```
OrderService
     |
     v
OrderRepository (Abstraction)
     ^                      ^
     |                      |
MySQL/Postgres/SQLite   RepositoryFactory
   (Detail)             (仅 Factory 知道所有 Detail)
```

#### 为什么符合 DIP？

`OrderService` 依赖 `OrderRepository` 抽象，`RepositoryFactory` 是唯一同时认识抽象和所有
具体实现的模块——这正是"组合根"（Composition Root）思想的雏形。业务代码不需要包含任何
`if backend == ...` 分支，新增一种数据库只需要修改 `RepositoryFactory._REGISTRY`，其余代码
零改动。

#### 如果违反 DIP

若把选择逻辑内联到 `OrderService.__init__`：

```python
def __init__(self, backend: str) -> None:
    if backend == "mysql": self._repo = MySQLOrderRepository()
    elif backend == "postgres": ...
```

- 业务类既要承担"选择实现"又要承担"业务规则"两种职责，违反单一职责原则。
- 每加一种数据库都要改 `OrderService` 源码，回归测试范围被迫扩大到核心业务类。

### 示例 2：ConnectorFactory —— 依设备能力选择 SSH / NETCONF / gNMI

```python
from __future__ import annotations

from typing import Protocol


class DeviceConnector(Protocol):
    def fetch_state(self) -> dict: ...


class SSHConnector:
    def fetch_state(self) -> dict:
        print("[SSH] show interface status")
        return {"proto": "ssh"}


class NetconfConnector:
    def fetch_state(self) -> dict:
        print("[NETCONF] <get><interfaces-state/></get>")
        return {"proto": "netconf"}


class GnmiConnector:
    def fetch_state(self) -> dict:
        print("[gNMI] Subscribe(interfaces/state)")
        return {"proto": "gnmi"}


class ConnectorFactory:
    """按设备上报的能力集选择最合适的连接协议，优先级: gNMI > NETCONF > SSH。"""

    @staticmethod
    def create(capabilities: set[str]) -> DeviceConnector:
        if "gnmi" in capabilities:
            return GnmiConnector()
        if "netconf" in capabilities:
            return NetconfConnector()
        if "ssh" in capabilities:
            return SSHConnector()
        raise ValueError("no supported protocol found in capabilities")


class DeviceStateCollector:
    """High-level Module：只依赖 DeviceConnector，不知道协议选择策略。"""

    def __init__(self, connector: DeviceConnector) -> None:
        self._connector = connector

    def collect(self) -> dict:
        return self._connector.fetch_state()


if __name__ == "__main__":
    for caps in ({"ssh"}, {"ssh", "netconf"}, {"ssh", "netconf", "gnmi"}):
        connector = ConnectorFactory.create(caps)
        print(DeviceStateCollector(connector).collect())
```

#### 对象关系图

```
+-------------------+  create()  +------------------------+
|  ConnectorFactory   | ---------> |  DeviceConnector (P)    |
+-------------------+           +------------------------+
                                   ^        ^          ^
                                   |        |          |
                              SSH     NETCONF       gNMI
                                   ^
                                   | 依赖(返回值类型)
                        +---------------------+
                        | DeviceStateCollector  |
                        +---------------------+
```

#### Object Flow

```
main()
  |
  v
设备上报 capabilities 集合（来自 NETCONF hello / gNMI Capabilities）
  |
  v
ConnectorFactory.create(capabilities)   # 按优先级选择协议
  |
  v
DeviceStateCollector(connector)
  |
  v
collector.collect() -> connector.fetch_state()
```

#### Dependency Graph

```
DeviceStateCollector
     |
     v
DeviceConnector (Abstraction)
     ^                     ^
     |                     |
SSH/NETCONF/gNMI       ConnectorFactory
   (Detail)            (唯一知道选择策略的模块)
```

#### 为什么符合 DIP？

"优先用更高效的协议"这条选择策略被封装进 `ConnectorFactory`，`DeviceStateCollector` 只管
拿到一个能 `fetch_state()` 的对象。以后新增协议或调整优先级，只改 Factory，采集逻辑不变。

#### 如果违反 DIP

若采集逻辑内部自己判断协议优先级：采集代码要同时理解三种协议的判断条件和调用方式，协议选择
策略调整（比如以后要根据延迟动态选择而非固定优先级）就必须改动采集主流程代码。

### 示例 3：PaymentGatewayFactory —— 依地区选择支付渠道

```python
from __future__ import annotations

from typing import Protocol


class PaymentGateway(Protocol):
    def charge(self, amount_cents: int) -> str: ...


class StripeGateway:
    def charge(self, amount_cents: int) -> str:
        return f"stripe charged {amount_cents}"


class AliPayGateway:
    def charge(self, amount_cents: int) -> str:
        return f"alipay charged {amount_cents}"


class GatewayFactory:
    _BY_REGION = {"US": StripeGateway, "CN": AliPayGateway}

    @classmethod
    def for_region(cls, region: str) -> PaymentGateway:
        gateway_cls = cls._BY_REGION.get(region, StripeGateway)  # 默认兜底
        return gateway_cls()


class CheckoutService:
    def __init__(self, gateway: PaymentGateway) -> None:
        self._gateway = gateway

    def pay(self, amount_cents: int) -> str:
        return self._gateway.charge(amount_cents)


if __name__ == "__main__":
    for region in ("US", "CN", "FR"):
        gateway = GatewayFactory.for_region(region)
        print(region, "->", CheckoutService(gateway).pay(999))
```

#### 对象关系图

```
+----------------+ for_region() +---------------------+
|  GatewayFactory  | -----------> |  PaymentGateway (P)   |
+----------------+              +---------------------+
                                   ^                 ^
                                   |                 |
                             Stripe           AliPay
```

#### Object Flow

```
main()
  |
  v
用户所在地区 region="CN"
  |
  v
GatewayFactory.for_region(region)
  |
  v
CheckoutService(gateway)
  |
  v
service.pay(amount) -> gateway.charge(amount)
```

#### Dependency Graph

```
CheckoutService
     |
     v
PaymentGateway (Abstraction)
     ^                  ^
     |                  |
Stripe/AliPay      GatewayFactory
```

#### 为什么符合 DIP？

地区到支付渠道的映射规则集中在一个地方维护，`CheckoutService` 保持对具体渠道的无知，符合
DIP，也让"这条业务规则应该由谁维护"变得一目了然。

#### 如果违反 DIP

若把地区判断散落在多个调用 `CheckoutService` 的地方各自实现一遍：规则会重复、容易出现不一致
（比如某处忘记更新新增地区的映射），维护成本随调用点数量线性增长。

## ⑧ Strategy Pattern

### 原理

Strategy 模式把"一族可以互相替换的算法"各自封装成独立的类，通过同一个抽象接口暴露，宿主对象
持有一个策略并在需要时调用。它与 Constructor/Method Injection 的区别不在"有没有抽象"，而在于
**这些实现之间是"同一件事的不同算法"，而不是"同一种资源的不同后端"**——比如"折扣计算方式"是
算法，"数据库"是资源，两者外在都表现为"多个实现共享一个接口"，但设计意图不同：Strategy 强调
"可以在运行期任意切换算法，且算法之间彼此平等、没有主次"。

- **依赖方向**：Context（宿主）依赖 Strategy 抽象；具体算法类实现该抽象。
- **适合规模**：任何需要"多套算法可插拔"的场景，从小型脚本到大型系统皆可。
- **优点**：新增算法零成本（开闭原则）；算法可以独立单测；避免了大段 `if/elif` 分支。
- **缺点**：策略数量少且不太会变化时，用 Strategy 是过度设计，简单 `if/else` 反而更直接。

### 示例 1：Retry Strategy —— Fixed Interval / Exponential Backoff

```python
from __future__ import annotations

import time
from typing import Protocol


class RetryStrategy(Protocol):
    def wait_seconds(self, attempt: int) -> float: ...
    def max_attempts(self) -> int: ...


class FixedIntervalRetry:
    def __init__(self, interval: float, attempts: int) -> None:
        self._interval = interval
        self._attempts = attempts

    def wait_seconds(self, attempt: int) -> float:
        return self._interval

    def max_attempts(self) -> int:
        return self._attempts


class ExponentialBackoffRetry:
    def __init__(self, base: float, attempts: int) -> None:
        self._base = base
        self._attempts = attempts

    def wait_seconds(self, attempt: int) -> float:
        return self._base * (2 ** (attempt - 1))

    def max_attempts(self) -> int:
        return self._attempts


class DevicePoller:
    """Context：不知道具体重试算法，只按 Strategy 提供的节奏重试。"""

    def __init__(self, strategy: RetryStrategy) -> None:
        self._strategy = strategy

    def poll(self, device_id: str, action) -> bool:
        for attempt in range(1, self._strategy.max_attempts() + 1):
            if action():
                print(f"[Poller] {device_id} succeeded on attempt {attempt}")
                return True
            wait = self._strategy.wait_seconds(attempt)
            print(f"[Poller] attempt {attempt} failed, sleep {wait}s")
            time.sleep(0)  # 演示中不真的 sleep
        return False


if __name__ == "__main__":
    calls = {"n": 0}

    def flaky_action() -> bool:
        calls["n"] += 1
        return calls["n"] >= 3

    DevicePoller(ExponentialBackoffRetry(base=1, attempts=5)).poll("RPD-01", flaky_action)
```

#### 对象关系图

```
+---------------+                +-----------------------+
|  DevicePoller   | -------------> |  RetryStrategy (P)     |
+---------------+                +-----------------------+
                                    ^                   ^
                                    |                   |
                       FixedIntervalRetry   ExponentialBackoffRetry
```

#### Object Flow

```
main()
  |
  v
create ExponentialBackoffRetry(base, attempts)
  |
  v
DevicePoller(strategy)
  |
  v
poller.poll(device_id, action)
  |
  v
for attempt in range(...): action() -> 失败则 strategy.wait_seconds(attempt)
```

#### Dependency Graph

```
DevicePoller
     |
     v
RetryStrategy (Abstraction)
     ^
     |
FixedIntervalRetry / ExponentialBackoffRetry (Detail)
```

#### 为什么符合 DIP？

轮询主流程（"没成功就重试，直到用完次数"）与"具体怎么计算等待时间"完全分离，新增
"带随机抖动的退避算法"只需要新写一个策略类。

#### 如果违反 DIP

若 `DevicePoller` 内部直接写死 `time.sleep(2 ** attempt)`：换一种重试节奏（比如从指数退避改
成固定间隔）需要直接修改轮询主流程，还可能牵连到重试次数、失败处理等无关逻辑。

### 示例 2：Load Balancing Strategy —— Round Robin / Least Connections

```python
from __future__ import annotations

from dataclasses import dataclass, field
from typing import Protocol


@dataclass(slots=True)
class Backend:
    host: str
    active_connections: int = 0


class LoadBalancingStrategy(Protocol):
    def select(self, backends: list[Backend]) -> Backend: ...


class RoundRobinStrategy:
    def __init__(self) -> None:
        self._index = 0

    def select(self, backends: list[Backend]) -> Backend:
        backend = backends[self._index % len(backends)]
        self._index += 1
        return backend


class LeastConnectionsStrategy:
    def select(self, backends: list[Backend]) -> Backend:
        return min(backends, key=lambda b: b.active_connections)


class LoadBalancer:
    def __init__(self, strategy: LoadBalancingStrategy) -> None:
        self._strategy = strategy

    def route(self, backends: list[Backend]) -> Backend:
        chosen = self._strategy.select(backends)
        chosen.active_connections += 1
        return chosen


if __name__ == "__main__":
    pool = [Backend("10.0.0.1", 3), Backend("10.0.0.2", 1), Backend("10.0.0.3", 5)]
    lb = LoadBalancer(LeastConnectionsStrategy())
    print("routed to:", lb.route(pool).host)
```

#### 对象关系图

```
+----------------+               +--------------------------------+
|  LoadBalancer    | -------------> |  LoadBalancingStrategy (P)      |
+----------------+               +--------------------------------+
                                    ^                          ^
                                    |                          |
                          RoundRobinStrategy       LeastConnectionsStrategy
```

#### Object Flow

```
main()
  |
  v
LoadBalancer(LeastConnectionsStrategy())
  |
  v
lb.route(backends)
  |
  v
strategy.select(backends) -> 挑选连接数最少的后端
  |
  v
active_connections += 1
```

#### Dependency Graph

```
LoadBalancer
     |
     v
LoadBalancingStrategy (Abstraction)
     ^
     |
RoundRobinStrategy / LeastConnectionsStrategy (Detail)
```

#### 为什么符合 DIP？

`LoadBalancer.route()` 不关心路由算法细节，只负责"选出来之后把连接数 +1"这条通用规则；换算法
只需要换一个 Strategy 实例。

#### 如果违反 DIP

若把路由算法写死在 `route()` 里：想 A/B 测试两种负载均衡算法的效果，必须复制一份几乎一样的
`LoadBalancer` 类，或者塞一个 `if algorithm == ...` 参数四处传递。

### 示例 3：Compression Strategy —— Gzip / Zstd（文件归档服务）

```python
from __future__ import annotations

import gzip
import zlib
from typing import Protocol


class CompressionStrategy(Protocol):
    def compress(self, data: bytes) -> bytes: ...
    def decompress(self, data: bytes) -> bytes: ...


class GzipCompression:
    def compress(self, data: bytes) -> bytes:
        return gzip.compress(data)

    def decompress(self, data: bytes) -> bytes:
        return gzip.decompress(data)


class ZlibCompression:
    """用 zlib 模拟 Zstd 的效果，避免引入第三方 zstandard 依赖。"""

    def compress(self, data: bytes) -> bytes:
        return zlib.compress(data, level=9)

    def decompress(self, data: bytes) -> bytes:
        return zlib.decompress(data)


class ArchiveService:
    def __init__(self, strategy: CompressionStrategy) -> None:
        self._strategy = strategy

    def archive(self, data: bytes) -> bytes:
        compressed = self._strategy.compress(data)
        print(f"archived {len(data)} bytes -> {len(compressed)} bytes")
        return compressed

    def restore(self, blob: bytes) -> bytes:
        return self._strategy.decompress(blob)


if __name__ == "__main__":
    payload = b"log line " * 1000
    for strategy in (GzipCompression(), ZlibCompression()):
        service = ArchiveService(strategy)
        blob = service.archive(payload)
        assert service.restore(blob) == payload
```

#### 对象关系图

```
+----------------+               +------------------------------+
|  ArchiveService  | -------------> |  CompressionStrategy (P)      |
+----------------+               +------------------------------+
                                    ^                        ^
                                    |                        |
                          GzipCompression         ZlibCompression
```

#### Object Flow

```
main()
  |
  v
ArchiveService(strategy)
  |
  v
service.archive(data) -> strategy.compress(data)
  |
  v
service.restore(blob) -> strategy.decompress(blob)
```

#### Dependency Graph

```
ArchiveService
     |
     v
CompressionStrategy (Abstraction)
     ^
     |
GzipCompression / ZlibCompression (Detail)
```

#### 为什么符合 DIP？

归档服务的"打印压缩前后字节数"这条统一行为独立于具体压缩算法，替换压缩算法（甚至按文件类型
动态选择算法）都不影响 `ArchiveService` 主体逻辑。

#### 如果违反 DIP

若直接在 `archive()` 里写死 `gzip.compress`：想支持多种压缩率/算法组合（比如冷数据用高压缩比
慢算法，热数据用低压缩比快算法）就必须在方法内部堆条件分支。

## ⑨ Callback Injection（回调注入）

### 原理

把"某个时间点该做什么"表示为一个**函数**（而不是一个完整对象），由调用方传入。这是 DIP 在
"最小可能抽象"上的极致体现——契约不是一个带方法的接口，而是**一个函数签名**（输入什么、返回
什么）。

- **依赖方向**：宿主对象/函数依赖"一个符合特定签名的可调用对象"这个抽象，调用方决定具体传
  哪个函数（可以是普通函数、`lambda`、绑定方法、甚至是一个实现了 `__call__` 的对象）。
- **适合规模**：事件通知、进度上报、错误处理钩子等"以时间点为中心"的场景，比"多种可替换算法"
  的 Strategy 更轻量。
- **优点**：极致轻量，不需要定义类或接口；天然支持"传多个回调"（成功回调、失败回调分开）。
- **缺点**：回调一多容易出现"回调地狱"，可读性下降；比 Strategy 更难承载"一组相关联的多个
  方法"（如果一个契约天然需要 3-4 个相关方法，应该用 Protocol/ABC 而不是拆成多个回调参数）。

### 示例 1：固件升级进度回调

```python
from __future__ import annotations

from typing import Callable

ProgressCallback = Callable[[str, int], None]  # (stage, percent) -> None


class FirmwareUpgradeJob:
    """High-level Module：只依赖"progress 回调"这个函数签名，不关心 UI 怎么展示进度。"""

    def __init__(self, on_progress: ProgressCallback) -> None:
        self._on_progress = on_progress

    def run(self, device_id: str) -> None:
        stages = [("download", 30), ("verify", 60), ("flash", 90), ("reboot", 100)]
        for stage, percent in stages:
            self._on_progress(stage, percent)


def cli_progress_bar(stage: str, percent: int) -> None:
    print(f"[CLI] {stage}: {'#' * (percent // 10)} {percent}%")


def collect_into_list(sink: list[tuple[str, int]]) -> ProgressCallback:
    def _callback(stage: str, percent: int) -> None:
        sink.append((stage, percent))
    return _callback


if __name__ == "__main__":
    FirmwareUpgradeJob(cli_progress_bar).run("RPD-01")

    events: list[tuple[str, int]] = []
    FirmwareUpgradeJob(collect_into_list(events)).run("RPD-02")
    print("collected for assertions in tests:", events)
```

#### 对象关系图

```
+---------------------+  持有  +---------------------------+
|  FirmwareUpgradeJob   | -----> | ProgressCallback (函数签名)  |
+---------------------+       +---------------------------+
                                 ^                     ^
                                 |                     |
                      cli_progress_bar      collect_into_list(...)
```

#### Object Flow

```
main()
  |
  v
定义/选择一个符合 (stage, percent) -> None 签名的函数
  |
  v
FirmwareUpgradeJob(on_progress)
  |
  v
job.run(device_id)
  |
  v
for stage, percent in stages: self._on_progress(stage, percent)
```

#### Dependency Graph

```
FirmwareUpgradeJob
     |
     v
ProgressCallback (Abstraction = 函数签名)
     ^
     |
cli_progress_bar / collect_into_list(...) (Detail)
```

#### 为什么符合 DIP？

升级流程的"分几个阶段、每阶段到百分之多少"是业务知识，"进度怎么展示"（打印到终端、写进测试
断言列表、推送 WebSocket）是细节。用一个函数当作最小化的抽象契约，比强行定义一个只有一个方法
的 `ProgressReporter` 类更轻量、更 Pythonic。

#### 如果违反 DIP

若升级流程内部直接 `print(f"{stage}: {percent}%")`：无法在单测里断言"确实按顺序上报了 4 个
阶段"，也无法把进度同时推送到多个前端（CLI + Web UI），只能反复修改 `run()` 方法本身。

### 示例 2：设备轮询的成功 / 失败回调

```python
from __future__ import annotations

from typing import Callable

OnSuccess = Callable[[dict], None]
OnError = Callable[[Exception], None]


class DevicePollingTask:
    def __init__(self, on_success: OnSuccess, on_error: OnError) -> None:
        self._on_success = on_success
        self._on_error = on_error

    def execute(self, should_fail: bool) -> None:
        try:
            if should_fail:
                raise TimeoutError("device unreachable")
            self._on_success({"status": "online"})
        except Exception as exc:  # noqa: BLE001 - 演示统一错误回调
            self._on_error(exc)


def log_success(result: dict) -> None:
    print(f"[OK] {result}")


def alert_on_error(exc: Exception) -> None:
    print(f"[ALERT] polling failed: {exc}")


if __name__ == "__main__":
    task = DevicePollingTask(on_success=log_success, on_error=alert_on_error)
    task.execute(should_fail=False)
    task.execute(should_fail=True)
```

#### 对象关系图

```
+---------------------+  持有两个回调  +----------------------------+
|  DevicePollingTask    | ------------> | OnSuccess / OnError (函数签名) |
+---------------------+              +----------------------------+
                                        ^                      ^
                                        |                      |
                              log_success           alert_on_error
```

#### Object Flow

```
main()
  |
  v
DevicePollingTask(on_success=log_success, on_error=alert_on_error)
  |
  v
task.execute(should_fail)
  |
  v
成功 -> on_success(result)   /   异常 -> on_error(exc)
```

#### Dependency Graph

```
DevicePollingTask
     |
     v
OnSuccess / OnError (Abstraction = 函数签名)
     ^
     |
log_success / alert_on_error (Detail)
```

#### 为什么符合 DIP？

轮询任务的"try/except 统一处理"逻辑与"成功/失败之后具体做什么"分离。可以给不同重要性的设备
注入不同的错误回调（核心设备失败要发工单，边缘设备失败只记日志），无需改动轮询任务本身。

#### 如果违反 DIP

若 `execute()` 内部直接 `print` 结果或直接调用某个具体的告警系统 SDK：轮询任务和告警系统
强耦合，测试轮询逻辑时会附带触发真实告警。

### 示例 3：订单完成事件钩子（Webhook 风格回调列表）

```python
from __future__ import annotations

from dataclasses import dataclass, field
from typing import Callable

OrderCompletedHook = Callable[["OrderCompletedEvent"], None]


@dataclass(frozen=True, slots=True)
class OrderCompletedEvent:
    order_id: str
    total_cents: int


class OrderCompletionPublisher:
    """High-level Module：维护一组回调，触发时逐个调用，不知道回调具体做什么。"""

    def __init__(self) -> None:
        self._hooks: list[OrderCompletedHook] = []

    def register(self, hook: OrderCompletedHook) -> None:
        self._hooks.append(hook)

    def publish(self, event: OrderCompletedEvent) -> None:
        for hook in self._hooks:
            hook(event)


def send_confirmation_email(event: OrderCompletedEvent) -> None:
    print(f"[Email] order {event.order_id} confirmed, total={event.total_cents}")


def update_loyalty_points(event: OrderCompletedEvent) -> None:
    points = event.total_cents // 100
    print(f"[Loyalty] +{points} points for order {event.order_id}")


if __name__ == "__main__":
    publisher = OrderCompletionPublisher()
    publisher.register(send_confirmation_email)
    publisher.register(update_loyalty_points)
    publisher.publish(OrderCompletedEvent("O-1", 4999))
```

#### 对象关系图

```
+---------------------------+  持有列表  +-----------------------------+
|  OrderCompletionPublisher   | ---------> | OrderCompletedHook (函数签名) |
+---------------------------+           +-----------------------------+
                                           ^                        ^
                                           |                        |
                             send_confirmation_email    update_loyalty_points
```

#### Object Flow

```
main()
  |
  v
publisher.register(send_confirmation_email)
publisher.register(update_loyalty_points)
  |
  v
publisher.publish(event)
  |
  v
for hook in self._hooks: hook(event)
```

#### Dependency Graph

```
OrderCompletionPublisher
     |
     v
OrderCompletedHook (Abstraction = 函数签名)
     ^
     |
send_confirmation_email / update_loyalty_points (Detail)
```

#### 为什么符合 DIP？

`OrderCompletionPublisher` 是一个极简的"事件总线"，它不知道也不关心注册了哪些具体动作，
新增"给用户发短信""同步给财务系统"都只是新增一个函数并注册，完全不触碰 Publisher 本身。

#### 如果违反 DIP

若订单服务里直接顺序调用 `send_email(); update_points(); sync_finance()`：每新增一个"订单
完成后要做的事情"，都要去修改订单服务主流程，各个动作之间还可能互相抛异常影响彼此执行。

## ⑩ Functional Dependency Injection（函数式依赖注入）

### 原理

把"依赖"从一个对象整体降维成**一个或多个纯函数**，通过参数传递（可以是构造时也可以是调用时）。
这与 Callback Injection 的区别在于：Callback Injection 通常是"事件钩子"（在某个时间点通知
外部），而 Functional DI 是把**核心业务能力本身**表达为函数组合，往往会用到高阶函数（函数
作为返回值/参数）、`functools.partial`、函数管道等函数式编程技巧。

- **依赖方向**：与其他方式相同——高层函数/对象依赖"函数签名"这个最小化抽象，具体函数实现是
  Detail。
- **适合规模**：函数式风格盛行的代码库（数据管道、ETL、校验规则引擎）；不需要维护复杂状态的
  服务。
- **优点**：极致轻量，配合 `functools` 可以做出非常干净的组合式代码；没有类的样板代码。
- **缺点**：当"依赖"本身需要维护内部状态或有多个相关方法时，硬用函数会显得别扭，这时候用
  Protocol/ABC 更合适；纯函数式代码对不熟悉函数式思维的团队有一定学习成本。

### 示例 1：校验规则管道（函数组合）

```python
from __future__ import annotations

from typing import Callable

Rule = Callable[[dict], str | None]  # 返回 None 表示通过，否则返回错误信息


def rule_hostname_required(payload: dict) -> str | None:
    return None if payload.get("hostname") else "hostname is required"


def rule_mtu_range(payload: dict) -> str | None:
    mtu = payload.get("mtu", 0)
    return None if 64 <= mtu <= 9000 else "mtu out of range"


def validate(payload: dict, rules: list[Rule]) -> list[str]:
    """High-level function：只依赖"Rule"这个函数签名，不知道具体校验规则。"""
    return [msg for rule in rules if (msg := rule(payload)) is not None]


if __name__ == "__main__":
    payload = {"hostname": "", "mtu": 20000}
    errors = validate(payload, [rule_hostname_required, rule_mtu_range])
    print(errors)
```

#### 对象关系图

```
+------------+  依赖  +------------------+
|  validate()  | -----> |  Rule (函数签名)   |
+------------+       +------------------+
                        ^                ^
                        |                |
        rule_hostname_required   rule_mtu_range
```

#### Object Flow

```
main()
  |
  v
构造规则函数列表 [rule_hostname_required, rule_mtu_range]
  |
  v
validate(payload, rules)
  |
  v
for rule in rules: rule(payload) -> 收集非 None 的错误信息
```

#### Dependency Graph

```
validate()
     |
     v
Rule (Abstraction = 函数签名)
     ^
     |
rule_hostname_required / rule_mtu_range (Detail)
```

#### 为什么符合 DIP？

`validate()` 是一个完全通用的高阶函数，它对"规则总共有几条、每条检查什么"一无所知；新增规则
只需要写一个同签名的新函数并加入列表，不需要碰 `validate()` 本身，也不需要定义任何类。

#### 如果违反 DIP

若把所有规则检查写成一个大函数体内的连续 `if`：规则的增减、复用（比如另一个场景只想复用其中
一条规则）都变得困难，规则之间无法被独立测试或独立组合。

### 示例 2：函数式 Repository（把存取行为表达为函数对）

```python
from __future__ import annotations

from typing import Callable

SaveFn = Callable[[str, dict], None]
FindFn = Callable[[str], dict | None]


def make_in_memory_repo() -> tuple[SaveFn, FindFn]:
    store: dict[str, dict] = {}

    def save(key: str, value: dict) -> None:
        store[key] = value

    def find(key: str) -> dict | None:
        return store.get(key)

    return save, find


class DeviceRegistrationService:
    """High-level Module：依赖两个函数签名，而不是一个 Repository 对象。"""

    def __init__(self, save: SaveFn, find: FindFn) -> None:
        self._save = save
        self._find = find

    def register(self, device_id: str, meta: dict) -> None:
        self._save(device_id, meta)

    def lookup(self, device_id: str) -> dict | None:
        return self._find(device_id)


if __name__ == "__main__":
    save_fn, find_fn = make_in_memory_repo()
    service = DeviceRegistrationService(save_fn, find_fn)
    service.register("RPD-01", {"model": "E6000"})
    print(service.lookup("RPD-01"))
```

#### 对象关系图

```
+----------------------------+  依赖  +----------------------+
|  DeviceRegistrationService   | -----> |  SaveFn / FindFn (P)   |
+----------------------------+       +----------------------+
                                        ^                  ^
                                        |                  |
                              make_in_memory_repo() 返回的闭包函数
```

#### Object Flow

```
main()
  |
  v
save_fn, find_fn = make_in_memory_repo()   # 闭包捕获内部 store 状态
  |
  v
DeviceRegistrationService(save_fn, find_fn)
  |
  v
service.register(...) -> self._save(key, value)
service.lookup(...)   -> self._find(key)
```

#### Dependency Graph

```
DeviceRegistrationService
     |
     v
SaveFn / FindFn (Abstraction = 函数签名)
     ^
     |
make_in_memory_repo() 内部闭包 (Detail)
```

#### 为什么符合 DIP？

存储行为被表达为两个独立函数而不是一个类，`DeviceRegistrationService` 依赖的是"能存、能查"
这两个最小契约。想换成基于文件或数据库的实现，只需要写一对新的 `save/find` 闭包函数，不需要
定义类、不需要继承。

#### 如果违反 DIP

若 `DeviceRegistrationService` 内部直接维护一个模块级全局字典：无法在同一进程里创建多个互相
隔离的注册表实例（比如多租户场景），全局状态还会让并行测试互相污染。

### 示例 3：函数式通知管道（组合多个发送函数）

```python
from __future__ import annotations

from functools import partial
from typing import Callable

Sender = Callable[[str], None]


def email_sender(smtp_host: str, message: str) -> None:
    print(f"[Email via {smtp_host}] {message}")


def slack_sender(webhook: str, message: str) -> None:
    print(f"[Slack {webhook}] {message}")


def broadcast(message: str, senders: list[Sender]) -> None:
    """High-level function：只依赖 Sender 函数签名列表。"""
    for send in senders:
        send(message)


if __name__ == "__main__":
    senders: list[Sender] = [
        partial(email_sender, "smtp.corp.com"),   # 通过 partial 把配置"烘焙"进函数里
        partial(slack_sender, "https://hooks.slack.com/xxx"),
    ]
    broadcast("Deployment finished", senders)
```

#### 对象关系图

```
+-------------+  依赖  +------------------+
|  broadcast()  | -----> |  Sender (函数签名) |
+-------------+       +------------------+
                         ^                ^
                         |                |
        partial(email_sender, host)   partial(slack_sender, webhook)
```

#### Object Flow

```
main()
  |
  v
用 functools.partial 把配置(host/webhook)绑定进函数，得到统一签名的 Sender
  |
  v
broadcast(message, senders)
  |
  v
for send in senders: send(message)
```

#### Dependency Graph

```
broadcast()
     |
     v
Sender (Abstraction = 函数签名)
     ^
     |
partial(email_sender, ...) / partial(slack_sender, ...) (Detail)
```

#### 为什么符合 DIP？

`broadcast()` 不知道每个 `Sender` 背后绑定了什么配置、走的是什么协议；`functools.partial`
让"配置注入"和"函数调用"自然分离，是函数式风格下实现 Constructor Injection 效果的惯用手法。

#### 如果违反 DIP

若 `broadcast()` 内部直接写死调用两个具体函数并各自传入配置：新增/移除通知渠道都需要改
`broadcast()` 本身，配置和调用逻辑也会紧密纠缠在一起。

## 四者速查

```
+---------------------------------------------------------------------+
|          Factory        Strategy       Callback        Functional DI |
+---------------------------------------------------------------------+
| 抽象粒度  | 返回值类型  | 类/接口       | 函数签名        | 函数签名       |
| 强调点    | 选择与创建  | 算法可替换    | 时间点钩子      | 函数组合        |
| 载体      | 类/静态方法 | 类            | 函数/lambda    | 函数/闭包/partial |
| 适合场景  | 复杂选择逻辑 | 多套等价算法 | 事件通知/进度   | 数据管道/校验规则 |
+---------------------------------------------------------------------+
```

下一步：进入 [Part 5 - 架构类：Plugin Architecture / IoC Container](dip_05_architecture_techniques.md)。
