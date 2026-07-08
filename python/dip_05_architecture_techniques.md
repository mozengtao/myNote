[← 返回目录](dip_00_index.md) | [上一篇: Part 4 创建型类](dip_04_creational_techniques.md)

# Part 5 - 架构类：⑪ Plugin Architecture ⑫ IoC Container

## ⑪ Plugin Architecture（插件架构）

### 原理

Plugin Architecture 把 DIP 从"单个依赖的替换"扩展到"**运行时动态发现并加载一批未知数量、未知
具体类型的实现**"。核心（Core）系统只定义插件必须满足的抽象契约和一个**注册表
（Registry）**，具体插件在自己的模块里"自我注册"（通过装饰器、`__init_subclass__`、或者
`importlib.metadata.entry_points`），核心系统在运行时通过名字查表拿到实现，全程不需要在源码
里 `import` 任何具体插件模块。

- **依赖方向**：核心系统依赖抽象契约 + Registry；插件模块依赖（import）核心系统暴露的抽象契约
  和注册装饰器，反过来注册到 Registry 里。核心系统的源码里完全不出现任何具体插件的名字——这是
  比 Factory 更彻底的倒置，因为连"有哪些实现"这份清单，核心系统都不需要硬编码。
- **适合规模**：需要支持第三方扩展、或者实现数量会持续增长（新设备型号、新导出格式、新认证
  方式）且团队希望"新增一种实现不用碰核心代码仓库"的大型系统。
- **优点**：真正的开闭原则——加新插件只需要新增一个文件；支持第三方或独立团队贡献插件而不需要
  修改核心代码；可以做到运行时按配置启用/禁用插件。
- **缺点**：架构复杂度显著上升（需要设计注册机制、加载时机、命名冲突处理）；调试时"这个实现到底
  是从哪加载进来的"有时不直观；对小型项目是过度设计。

### 示例 1：Report Exporter 插件系统（CSV / PDF / JSON 通过装饰器自注册）

```python
from __future__ import annotations

from typing import Callable, Protocol


class ReportExporter(Protocol):
    def export(self, rows: list[dict]) -> str: ...


class ExporterRegistry:
    """Core：只维护一张"名字 -> 实现"的表，不知道具体有哪些插件。"""

    _plugins: dict[str, ReportExporter] = {}

    @classmethod
    def register(cls, name: str) -> Callable[[type], type]:
        def decorator(exporter_cls: type) -> type:
            cls._plugins[name] = exporter_cls()
            return exporter_cls
        return decorator

    @classmethod
    def get(cls, name: str) -> ReportExporter:
        if name not in cls._plugins:
            raise ValueError(f"unknown exporter plugin: {name}")
        return cls._plugins[name]

    @classmethod
    def available(cls) -> list[str]:
        return sorted(cls._plugins)


# ---- 以下是"插件"，可以拆分到独立文件/独立包，核心代码不需要 import 它们 ----

@ExporterRegistry.register("csv")
class CsvExporterPlugin:
    def export(self, rows: list[dict]) -> str:
        if not rows:
            return ""
        header = ",".join(rows[0].keys())
        lines = [",".join(str(v) for v in row.values()) for row in rows]
        return "\n".join([header, *lines])


@ExporterRegistry.register("json")
class JsonExporterPlugin:
    def export(self, rows: list[dict]) -> str:
        import json
        return json.dumps(rows)


class ReportService:
    """High-level Module：按名字向 Registry 要一个导出器，从不 import 具体插件类。"""

    def export(self, rows: list[dict], format_name: str) -> str:
        exporter = ExporterRegistry.get(format_name)
        return exporter.export(rows)


if __name__ == "__main__":
    print("available plugins:", ExporterRegistry.available())
    rows = [{"device": "RPD-01", "status": "online"}]
    print(ReportService().export(rows, "csv"))
    print(ReportService().export(rows, "json"))
```

#### 对象关系图

```
+----------------+  register()  +---------------------+
|  Exporter 插件们  | -----------> |  ExporterRegistry     |
+----------------+             +----------+----------+
                                            | get(name)
                                            v
                                  +------------------+
                                  |  ReportService     |
                                  +------------------+
```

#### Object Flow

```
模块加载期（import 时）
  |
  v
每个插件类用 @ExporterRegistry.register("csv") 自我登记
  |
  v
运行期: ReportService().export(rows, "csv")
  |
  v
ExporterRegistry.get("csv") -> 查表返回已注册实例
  |
  v
exporter.export(rows)
```

#### Dependency Graph

```
ReportService  --依赖-->  ReportExporter (Abstraction) <--实现--  CsvExporterPlugin / JsonExporterPlugin
     |                                ^
     v                                |
ExporterRegistry <--注册(import 契约与装饰器)-- 插件模块
```

#### 为什么符合 DIP？

`ReportService` 的源码里，从头到尾没有出现 `CsvExporterPlugin` 或 `JsonExporterPlugin` 这
两个名字——它只认识 `ExporterRegistry.get(name)` 返回的抽象类型。真正"认识"具体插件的，只有
插件自己（它 import 了 `ExporterRegistry` 和 `ReportExporter` 契约，反向注册自己）。这是本
教程里"倒置程度最彻底"的例子：核心系统甚至不需要知道插件的总数和名单。

#### 如果违反 DIP

```
ReportService.export()
    if format_name == "csv": ...
    elif format_name == "json": ...
    elif format_name == "pdf": ...
```

- 每新增一种导出格式，都要回到 `ReportService` 里加一个分支，核心代码仓库必须为每个新格式
  重新发布。
- 无法让外部团队/插件市场独立开发和分发新格式支持——他们没有权限修改核心仓库的 `if/elif`。
- 想要"按客户定制启用哪些格式"变得困难，因为所有格式的实现代码都耦合在同一个方法体内。

### 示例 2：网络设备驱动插件（SSH / NETCONF / gNMI 通过 `__init_subclass__` 自注册）

```python
from __future__ import annotations

from abc import ABC, abstractmethod


class DeviceDriver(ABC):
    """Core 定义的抽象契约 + 自注册机制。"""

    protocol_name: str
    _registry: dict[str, type["DeviceDriver"]] = {}

    def __init_subclass__(cls, **kwargs) -> None:
        super().__init_subclass__(**kwargs)
        DeviceDriver._registry[cls.protocol_name] = cls

    @abstractmethod
    def execute(self, command: str) -> str: ...

    @classmethod
    def create(cls, protocol_name: str) -> "DeviceDriver":
        driver_cls = cls._registry[protocol_name]
        return driver_cls()

    @classmethod
    def supported_protocols(cls) -> list[str]:
        return sorted(cls._registry)


# ---- 插件：只要继承 DeviceDriver 并声明 protocol_name，就会被自动收录 ----

class SshDriver(DeviceDriver):
    protocol_name = "ssh"

    def execute(self, command: str) -> str:
        return f"[SSH] {command}"


class NetconfDriver(DeviceDriver):
    protocol_name = "netconf"

    def execute(self, command: str) -> str:
        return f"[NETCONF] rpc({command})"


class GnmiDriver(DeviceDriver):
    protocol_name = "gnmi"

    def execute(self, command: str) -> str:
        return f"[gNMI] Get(path={command})"


class DeviceCli:
    """High-level Module：按协议名向 DeviceDriver.create() 要驱动，不知道具体有哪些协议。"""

    def run(self, protocol_name: str, command: str) -> str:
        driver = DeviceDriver.create(protocol_name)
        return driver.execute(command)


if __name__ == "__main__":
    print("supported:", DeviceDriver.supported_protocols())
    print(DeviceCli().run("netconf", "get-config"))
```

#### 对象关系图

```
+---------------------------+  __init_subclass__ 自注册  +------------------------+
|  SshDriver/NetconfDriver/... | -------------------------> |  DeviceDriver._registry |
+---------------------------+                            +-----------+------------+
                                                                       | create(name)
                                                                       v
                                                            +----------------+
                                                            |  DeviceCli       |
                                                            +----------------+
```

#### Object Flow

```
模块加载期
  |
  v
class SshDriver(DeviceDriver): protocol_name="ssh"
  -> __init_subclass__ 钩子自动把 {"ssh": SshDriver} 写入 _registry
  |
  v
运行期: DeviceCli().run("netconf", "get-config")
  |
  v
DeviceDriver.create("netconf") -> 从 _registry 查表实例化
  |
  v
driver.execute(command)
```

#### Dependency Graph

```
DeviceCli --依赖--> DeviceDriver (Abstraction) <--继承--  SshDriver/NetconfDriver/GnmiDriver
```

#### 为什么符合 DIP？

利用 Python 的 `__init_subclass__` 钩子，插件"继承即注册"，核心 `DeviceCli` 只依赖
`DeviceDriver.create()` 这一个抽象入口。新增 RESTCONF 驱动，只需要新建一个继承
`DeviceDriver` 的类文件，甚至不需要修改任何现有文件的 import 语句。

#### 如果违反 DIP

若 `DeviceCli.run()` 内部手写协议判断分支：每新增一种协议，`DeviceCli` 和所有协议实现代码
就被绑定在同一个文件里维护，无法把"设备驱动"拆分成可以独立发布、独立版本管理的子模块。

### 示例 3：认证插件（按配置动态加载 Provider）

```python
from __future__ import annotations

import importlib
from typing import Protocol


class AuthPlugin(Protocol):
    def verify(self, token: str) -> bool: ...


class LocalAuthPlugin:
    def verify(self, token: str) -> bool:
        return token == "local-secret"


class OidcAuthPlugin:
    def verify(self, token: str) -> bool:
        return token.startswith("oidc_")


_PLUGIN_MODULE = __name__  # 演示用：真实项目里插件通常来自独立的包路径


def load_plugin(dotted_class_name: str) -> AuthPlugin:
    """Core：通过配置里的字符串路径动态导入插件类，源码里不出现任何具体插件名字。"""
    module_path, _, class_name = dotted_class_name.rpartition(".")
    module = importlib.import_module(module_path)
    plugin_cls = getattr(module, class_name)
    return plugin_cls()


class Gateway:
    def __init__(self, auth: AuthPlugin) -> None:
        self._auth = auth

    def handle(self, token: str) -> str:
        return "200 OK" if self._auth.verify(token) else "401 Unauthorized"


if __name__ == "__main__":
    # 生产环境中，这个字符串通常来自配置文件/环境变量，例如 AUTH_PLUGIN=myauth.plugins.OidcAuthPlugin
    plugin = load_plugin(f"{_PLUGIN_MODULE}.OidcAuthPlugin")
    print(Gateway(plugin).handle("oidc_abc123"))
```

#### 对象关系图

```
配置文件: AUTH_PLUGIN="pkg.module.OidcAuthPlugin"
                |
                v
        +----------------+  importlib 动态加载  +------------------+
        |  load_plugin()   | -------------------> |  AuthPlugin (P)   |
        +----------------+                      +------------------+
                                                    ^              ^
                                                    |              |
                                       LocalAuthPlugin      OidcAuthPlugin
```

#### Object Flow

```
应用启动
  |
  v
读取配置字符串 "module.ClassName"
  |
  v
load_plugin(dotted_name) -> importlib.import_module + getattr
  |
  v
Gateway(plugin)
  |
  v
gateway.handle(token) -> plugin.verify(token)
```

#### Dependency Graph

```
Gateway --依赖--> AuthPlugin (Abstraction) <--满足--  LocalAuthPlugin / OidcAuthPlugin
                        ^
                        | (运行期字符串路径解析，源码零耦合)
                   load_plugin()
```

#### 为什么符合 DIP？

这是插件架构里最彻底的一种形式：核心代码**连插件类的名字都不出现在源码里**，只在配置文件中
以字符串形式存在，通过 `importlib` 在运行期解析。`Gateway` 只依赖 `AuthPlugin.verify()` 这
一个契约。这种手法常见于 Django（`INSTALLED_APPS`）、pytest（插件发现）、`setuptools`
entry points 等生态。

#### 如果违反 DIP

若认证方式在代码里写死 `import` 语句：切换认证方式需要改代码、重新发布，无法做到"运维人员改
一行配置就切换认证策略，不需要开发介入、不需要重新部署代码包"。

## ⑫ IoC Container（如 dependency_injector）

### 原理

当对象图（object graph）变得复杂——`OrderService` 需要 `Repository`，`Repository` 需要
`ConnectionPool`，`ConnectionPool` 需要 `Config`，`Config` 需要从环境变量读取——手写
Composition Root 会变得冗长且容易出错。**IoC（Inversion of Control）容器**（如
`dependency_injector` 库）提供一种声明式的方式描述"每个对象怎么构造、依赖谁"，容器负责按
依赖关系自动组装整张对象图，并管理对象的生命周期（每次新建 / 全局单例 / 按请求作用域）。

- **依赖方向**：业务代码本身仍然只依赖抽象类型（不变）；变化的是"组装这件事"从手写代码变成了
  一份声明式配置（Container），容器成为整个应用里**唯一**知道所有具体实现的地方，是
  Composition Root 的进阶、系统化版本。
- **适合规模**：大型应用、微服务、依赖数量多且需要区分生命周期（单例 vs 每次请求新建）的
  Web 后端；小型脚本引入 IoC 容器通常是过度设计。
- **优点**：对象图集中声明，一眼能看清整个应用有哪些组件、怎么连接；天然支持生命周期管理
  （Singleton/Factory/Resource）；测试时可以用 `container.override()` 轻松替换某个依赖。
- **缺点**：引入新概念和新库，学习成本高于手写 DI；容器配置本身也是一种"魔法"，过度使用会让
  依赖关系变得不直观（IDE 跳转不到具体实现）；对小项目是不必要的复杂度。

> 以下示例依赖第三方库 `dependency_injector`（`pip install dependency-injector`）。

### 示例 1：Web 后端服务图（Config → Repository → Service）

```python
from __future__ import annotations

from dependency_injector import containers, providers
from typing import Protocol


class UserRepository(Protocol):
    def find(self, user_id: str) -> dict | None: ...


class SqlUserRepository:
    def __init__(self, dsn: str) -> None:
        self._dsn = dsn
        print(f"[SQL] connected to {dsn}")

    def find(self, user_id: str) -> dict | None:
        return {"id": user_id, "name": "Alice"}


class UserService:
    def __init__(self, repo: UserRepository) -> None:
        self._repo = repo

    def get_profile(self, user_id: str) -> dict | None:
        return self._repo.find(user_id)


class Container(containers.DeclarativeContainer):
    """唯一知道所有具体实现的地方（应用的 Composition Root）。"""

    config = providers.Configuration()

    user_repository = providers.Singleton(
        SqlUserRepository,
        dsn=config.database.dsn,
    )

    user_service = providers.Factory(
        UserService,
        repo=user_repository,
    )


if __name__ == "__main__":
    container = Container()
    container.config.database.dsn.from_value("postgresql://localhost/app")

    service = container.user_service()  # 容器自动组装 UserService(SqlUserRepository(dsn=...))
    print(service.get_profile("U-1"))
```

#### 对象关系图

```
+-----------+ config.database.dsn +----------------------+
|  Config     | -------------------> |  SqlUserRepository    |
+-----------+                      +----------+-----------+
                                                | providers.Singleton
                                                v
                                     +----------------------+
                                     |  UserService           |
                                     +----------------------+
                                                ^
                                                | providers.Factory
                                     +----------------------+
                                     |  Container (IoC)       |
                                     +----------------------+
```

#### Object Flow

```
main()
  |
  v
Container() 实例化，config 从环境/配置文件加载
  |
  v
container.user_service()
  |
  v
容器按依赖图自动求值: SqlUserRepository(dsn=config...) -> UserService(repo=...)
  |
  v
service.get_profile(user_id) -> repo.find(user_id)
```

#### Dependency Graph

```
UserService  --依赖-->  UserRepository (Abstraction)  <--实现--  SqlUserRepository
     ^                                                                 ^
     |                                                                 |
     +----------------------- Container 统一组装 --------------------+
```

#### 为什么符合 DIP？

`UserService` 和 `SqlUserRepository` 彼此都不知道对方是怎么被创建、怎么被连接起来的——这些
"组装知识"全部收敛进 `Container`。业务代码（`UserService.get_profile`）里没有任何一行涉及
"怎么拿到 repo 实例"，天然满足 DIP；容器只是把手写 Composition Root 的工作自动化、声明化了。

#### 如果违反 DIP

若在每个需要 `UserService` 的地方手写 `UserService(SqlUserRepository("postgresql://..."))`：
DSN 字符串、生命周期策略（要不要单例）会散落在代码库的各个角落，一旦要调整数据库连接参数，
需要全局搜索替换，且容易遗漏。

### 示例 2：网络自动化服务图（Config → DeviceConnector → InspectionWorkflow）

```python
from __future__ import annotations

from dependency_injector import containers, providers
from typing import Protocol


class DeviceConnector(Protocol):
    def fetch(self, host: str) -> str: ...


class SshConnector:
    def __init__(self, username: str, timeout: int) -> None:
        self._username = username
        self._timeout = timeout

    def fetch(self, host: str) -> str:
        return f"[SSH user={self._username} timeout={self._timeout}] show run @ {host}"


class InspectionWorkflow:
    def __init__(self, connector: DeviceConnector) -> None:
        self._connector = connector

    def run(self, host: str) -> str:
        return self._connector.fetch(host)


class NetworkContainer(containers.DeclarativeContainer):
    config = providers.Configuration()

    ssh_connector = providers.Factory(
        SshConnector,
        username=config.ssh.username,
        timeout=config.ssh.timeout,
    )

    inspection_workflow = providers.Factory(
        InspectionWorkflow,
        connector=ssh_connector,
    )


if __name__ == "__main__":
    container = NetworkContainer()
    container.config.ssh.username.from_value("admin")
    container.config.ssh.timeout.from_value(10)

    workflow = container.inspection_workflow()
    print(workflow.run("10.0.0.1"))
```

#### 对象关系图

```
+-----------+ config.ssh.* +-------------------+
|  Config     | -----------> |  SshConnector       |
+-----------+              +----------+---------+
                                       | providers.Factory
                                       v
                             +----------------------+
                             |  InspectionWorkflow    |
                             +----------------------+
                                       ^
                                       | providers.Factory
                             +----------------------+
                             |  NetworkContainer      |
                             +----------------------+
```

#### Object Flow

```
main()
  |
  v
NetworkContainer()，从配置注入 ssh.username / ssh.timeout
  |
  v
container.inspection_workflow()
  |
  v
容器自动组装: SshConnector(username, timeout) -> InspectionWorkflow(connector)
  |
  v
workflow.run(host) -> connector.fetch(host)
```

#### Dependency Graph

```
InspectionWorkflow --依赖--> DeviceConnector (Abstraction) <--实现-- SshConnector
                                                                        ^
                                                                        |
                                                            NetworkContainer 统一组装
```

#### 为什么符合 DIP？

`InspectionWorkflow` 只依赖 `DeviceConnector` 结构化契约；`providers.Factory` 意味着每次
请求 `inspection_workflow()` 都会创建全新实例（区别于 `Singleton`），这对"每次巡检任务应该
用独立连接"的场景很合适——生命周期策略本身也成为一种可以在容器里声明、无需触碰业务代码的配置。

#### 如果违反 DIP

若在巡检脚本的每个入口都手写 `SshConnector(username="admin", timeout=10)`：连接参数硬编码
散落各处，且没有统一地方管理"这个连接对象该是单例还是每次新建"这类生命周期决策。

### 示例 3：多环境容器（开发 / 生产 Provider 覆盖）

```python
from __future__ import annotations

from dependency_injector import containers, providers
from typing import Protocol


class NotificationSender(Protocol):
    def send(self, message: str) -> None: ...


class ConsoleSender:
    def send(self, message: str) -> None:
        print(f"[console-dev] {message}")


class SlackSender:
    def __init__(self, webhook: str) -> None:
        self._webhook = webhook

    def send(self, message: str) -> None:
        print(f"[Slack {self._webhook}] {message}")


class AlertService:
    def __init__(self, sender: NotificationSender) -> None:
        self._sender = sender

    def fire(self, message: str) -> None:
        self._sender.send(message)


class AppContainer(containers.DeclarativeContainer):
    config = providers.Configuration()

    # 默认（开发环境）用 ConsoleSender
    notification_sender = providers.Singleton(ConsoleSender)

    alert_service = providers.Factory(
        AlertService,
        sender=notification_sender,
    )


def configure_for_production(container: AppContainer, webhook: str) -> None:
    """生产环境启动时，用 override 把默认 Provider 换成真实实现，业务代码零改动。"""
    container.notification_sender.override(providers.Singleton(SlackSender, webhook=webhook))


if __name__ == "__main__":
    container = AppContainer()
    container.alert_service().fire("dev environment alert")  # ConsoleSender

    configure_for_production(container, "https://hooks.slack.com/services/prod")
    container.alert_service().fire("prod environment alert")  # SlackSender
```

#### 对象关系图

```
+------------------+  默认  +----------------+
|  AppContainer      | -----> |  ConsoleSender   |
+---------+--------+       +----------------+
          | override() (生产环境启动时)
          v
+----------------+
|  SlackSender     |
+----------------+
       ^
       两者都实现 NotificationSender (Protocol)
```

#### Object Flow

```
开发环境启动: AppContainer() -> alert_service().fire() 使用 ConsoleSender
  |
  v
生产环境启动: configure_for_production(container, webhook)
  |
  v
container.notification_sender.override(Singleton(SlackSender, webhook))
  |
  v
后续 alert_service().fire() 全部改用 SlackSender，AlertService 源码零改动
```

#### Dependency Graph

```
AlertService --依赖--> NotificationSender (Abstraction) <--实现-- ConsoleSender / SlackSender
                                                                       ^
                                                                       |
                                                        AppContainer.override() 按环境切换
```

#### 为什么符合 DIP？

`AlertService` 全程只依赖 `NotificationSender` 契约；"开发环境用 Console，生产环境用 Slack"
这条环境相关的知识被隔离在容器配置层，甚至可以做到不重启进程、仅调用一次 `override()` 就切换
整条依赖链，这是手写 Constructor Injection 很难优雅做到的能力。

#### 如果违反 DIP

若 `AlertService` 内部根据环境变量判断该用哪个 sender：环境判断逻辑会散落进每一个类似的服务
类里，且没有一个统一的地方可以在测试中"整体替换"某个依赖，测试隔离性变差。

## 两者速查

```
+---------------------------------------------------------------------+
|                  Plugin Architecture         IoC Container            |
+---------------------------------------------------------------------+
| 解决的问题  | 实现数量/名单未知、可扩展   | 对象图复杂、生命周期管理    |
| 谁知道全部实现 | 没有人知道（去中心化注册）| Container 一个地方知道     |
| 典型场景    | 插件市场、多协议驱动        | 大型 Web/微服务后端         |
| 额外复杂度  | 注册机制设计               | 引入第三方库+声明式配置学习  |
+---------------------------------------------------------------------+
```

下一步：进入 [Part 6 - 12 种实现方式对比表](dip_06_comparison_table.md)。
