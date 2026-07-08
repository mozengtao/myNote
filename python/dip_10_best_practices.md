[← 返回目录](dip_00_index.md) | [上一篇: Part 9 Pythonic 的 DIP](dip_09_pythonic_dip.md)

# Part 10 - 大型 Python 项目中的最佳实践

## 分层职责速查

企业级 Python 项目（尤其是 DDD / Clean Architecture 风格）通常按下面的职责划分组织代码，
每一层的边界都由 DIP 守护：

```
+-------------------------------------------------------------------+
|  层            |  职责                          |  依赖谁            |
+-------------------------------------------------------------------+
|  Domain         |  实体、值对象、领域服务、Port    |  不依赖任何人       |
|  Application     |  Service：编排单个用例          |  只依赖 Domain     |
|  Workflow        |  编排多个 Application Service   |  只依赖 Application|
|                  |                                |  / Domain          |
|  Infrastructure  |  Adapter：实现 Port             |  依赖 Domain(Port) |
|  Ports           |  = Domain 里定义的抽象接口      |  (概念上属于 Domain)|
|  Adapters        |  = Infrastructure 的另一个说法  |  依赖 Ports         |
|  API             |  Controller/CLI/gRPC handler    |  依赖 Application  |
|                  |                                |  / Workflow        |
+-------------------------------------------------------------------+
```

这与 Part 8 里 `network-automation` 案例项目的目录结构完全对应，也是 Cosmic Python
（*Architecture Patterns with Python*）一书推崇的组织方式。

## Repository / Service / Workflow 三者的关系与边界

```
+------------------+     +------------------+     +----------------------+
|  Workflow          | --> |  Service (App层)   | --> |  Repository (Domain)  |
|  跨用例业务流程编排   |     |  单一用例编排        |     |  持久化契约            |
+------------------+     +------------------+     +----------------------+
```

一个常见的边界混淆是"Service 到底该不该直接操作 Repository，还是应该经过 Domain Service？"
经验法则：

- **纯 CRUD、没有复杂业务规则** → Application Service 直接调用 Repository。
- **涉及聚合内部不变量、多个实体协作的业务规则** → 把规则下沉到 Domain Service 或聚合根
  自己的方法里，Application Service 只负责"读出聚合 → 调用聚合方法 → 存回去"这三步编排，
  自己不应该包含判断逻辑。

```python
class TransferFundsService:
    """Application Service：只做编排，业务规则委托给聚合根自己。"""

    def __init__(self, accounts: AccountRepository) -> None:
        self._accounts = accounts

    def transfer(self, from_id: str, to_id: str, amount_cents: int) -> None:
        source = self._accounts.get(from_id)
        target = self._accounts.get(to_id)
        source.withdraw(amount_cents)   # 业务规则（余额是否充足）在 Account 聚合根内部
        target.deposit(amount_cents)
        self._accounts.save(source)
        self._accounts.save(target)
```

## Ports & Adapters 的落地命名约定

大型项目里常见两套命名习惯，任选一套但要在团队内保持一致：

```
+---------------------------------------------------------------+
|  命名风格 A（Hexagonal 术语）  |  命名风格 B（更直白）             |
+---------------------------------------------------------------+
|  domain/ports/                |  domain/repository.py             |
|    order_repository_port.py   |  (Protocol 定义直接放在领域模块里) |
|  infrastructure/adapters/     |  infrastructure/                  |
|    sql_order_repository.py    |    sql_order_repository.py        |
+---------------------------------------------------------------+
```

本教程 Part 8 的案例项目采用风格 B（更符合大多数中小型 Python 团队的实际习惯）；风格 A 在
更严格遵循 Hexagonal Architecture 术语的团队/书籍中更常见。

## Factory 与 Object Graph 组装策略

大型项目的对象图往往有数十个组件，组装方式通常分三个层级递进：

```
+-------------------------------------------------------------------+
|  规模        |  推荐组装方式                                        |
+-------------------------------------------------------------------+
|  < 10 个组件  |  手写 Composition Root（一个 main.py / bootstrap.py）|
|  10~50 个组件 |  按子系统拆分多个 Factory 函数，main.py 只调用 Factory |
|  50+ 个组件   |  引入 IoC 容器（如 dependency_injector），声明式组装   |
+-------------------------------------------------------------------+
```

## Object Lifetime：Singleton / Transient / Scoped

对象生命周期管理是大型项目里经常被忽视、但影响深远的设计决策。三种经典生命周期：

```python
from __future__ import annotations

from typing import Protocol


class ConnectionPool(Protocol):
    def acquire(self) -> object: ...


class Config(Protocol):
    def get(self, key: str) -> str: ...


class RequestContext(Protocol):
    """每个请求独立一份，请求结束即销毁。"""
    request_id: str
```

- **Singleton（单例）**：整个应用生命周期内只创建一次，之后所有请求方共享同一个实例。
  适合：数据库连接池、Config、Logger、线程安全的缓存客户端。
  风险：如果单例对象内部保存了"本应属于单次请求"的可变状态，会在并发请求间互相污染。

- **Transient（瞬态）**：每次请求（`container.get(...)`）都创建一个全新实例，用完即弃。
  适合：无副作用的小对象、Strategy 实例、DTO 转换器。
  风险：如果构造成本很高（比如建立新的 TCP 连接），频繁创建会成为性能瓶颈。

- **Scoped（作用域/请求级单例）**：在一个特定作用域内（如一次 HTTP 请求、一次 CLI 命令
  执行）共享同一个实例，作用域结束即销毁。适合：数据库事务对象（同一请求内的多次
  Repository 调用应该在同一个事务里）、请求级别的用户身份上下文。

```python
class UnitOfWork:
    """Scoped 生命周期的典型例子：一次请求内共享同一个事务，跨请求绝不共享。"""

    def __enter__(self) -> "UnitOfWork":
        print("BEGIN TRANSACTION")
        return self

    def __exit__(self, *exc_info) -> None:
        print("COMMIT TRANSACTION")


def handle_request(uow_factory) -> None:
    with uow_factory() as uow:      # 每次请求创建一个新的 Scoped UnitOfWork
        ...                          # 本次请求内的所有 Repository 调用共享这一个 uow
```

用 `dependency_injector` 表达三种生命周期非常直接：

```python
from dependency_injector import containers, providers

class Container(containers.DeclarativeContainer):
    config = providers.Singleton(dict)              # Singleton
    strategy = providers.Factory(object)             # Transient（每次全新实例）
    unit_of_work = providers.ThreadLocalSingleton(object)  # 近似 Scoped（线程级）
```

## Checklist：大型 Python 项目如何组织才算符合 DIP

```
+-------------------------------------------------------------------+
| [ ] domain/ 目录下的任何文件都不 import infrastructure/ 或第三方 SDK |
| [ ] Repository/Gateway/Client 等接口定义在 domain/，而不是           |
|     infrastructure/                                                |
| [ ] Application/Workflow 层的构造函数参数全部是抽象类型               |
| [ ] 只有一个（或几个专门的 Factory/Container）Composition Root       |
|     同时认识所有具体实现                                             |
| [ ] 新增一种技术实现（数据库/协议/支付渠道）不需要修改 domain/ 或      |
|     application/ 任何一行代码                                       |
| [ ] 单元测试可以完全不启动任何真实外部依赖（数据库/网络/文件系统）     |
| [ ] 对象生命周期（Singleton/Transient/Scoped）在组装层显式声明，       |
|     而不是散落在业务代码里"隐式"决定                                 |
+-------------------------------------------------------------------+
```

下一步：进入 [Part 11 - 最终心智模型总结](dip_11_mental_model_summary.md)。
