[← 返回目录](dip_00_index.md) | [上一篇: Part 8 真实项目案例](dip_08_real_project_case_study.md)

# Part 9 - Pythonic 的 DIP：什么时候用 Protocol / ABC / Duck Typing / 完全不用接口

## 为什么 Python 不像 Java 那样强调 interface

Java 之所以从语言层面强制要求 `interface`，根本原因在于它是**静态、名义类型（nominal
typing）**语言：编译器在编译期就要确定每个变量的类型，而 Java 早期（Java 8 之前）没有类型
推断、没有结构化类型系统，唯一能表达"这个类型可以是多种具体类型之一"的方式就是显式声明一个
`interface` 并 `implements` 它。换句话说，**在 Java 里，`interface` 几乎是实现多态和解耦的
唯一语言机制**。

Python 从设计哲学上完全不同：

1. **鸭子类型是语言的默认行为**，不需要任何声明——任何拥有正确方法名的对象都能被使用。
   "如果它走起来像鸭子、叫起来像鸭子"这句话本身就是 Python 社区的口头禅。
2. **EAFP（Easier to Ask Forgiveness than Permission）**是 Python 推荐的编程风格：先尝试
   调用，出错再处理，而不是像 Java 那样先用 `instanceof`（对应 Python 的 `isinstance`）
   "问许可"。这意味着 Python 代码天生就不那么依赖显式接口检查。
3. **`typing.Protocol`（PEP 544，2019 年才引入）本质上是"把已经存在了几十年的鸭子类型习惯，
   事后补上一层可选的静态类型检查"**，而不是 Python 发明了一个新的接口机制强加给语言。
4. Python 的"我们都是成年人"（We're all consenting adults）哲学：语言更倾向信任开发者，
   用文档字符串和命名约定表达契约，而不是用编译器强制。

这不代表 Python 不需要接口，而是意味着**"要不要用接口、用哪种接口"在 Python 里是一个需要
主动权衡的设计决策，而不是语言强加的默认项**。下面给出大量场景化示例，帮助建立这个判断力。

## 什么时候 Protocol 更好

### 场景 1：需要接入你不拥有、无法修改的第三方对象

```python
from typing import Protocol


class Writable(Protocol):
    def write(self, data: str) -> int: ...


def dump_report(sink: Writable, content: str) -> None:
    sink.write(content)


import sys
dump_report(sys.stdout, "hello")          # sys.stdout 天然满足 Writable，无需任何包装
dump_report(open("/tmp/out.txt", "w"), "hello")  # 文件对象同样天然满足
```

标准库的 `sys.stdout`、文件对象、`io.StringIO` 都"恰好"有 `write()` 方法，用 Protocol 可以
直接把它们当依赖注入进来，不需要写一个继承适配类。如果用 ABC，你没有办法让 `sys.stdout`
"回过头去"继承你定义的抽象基类。

### 场景 2：给测试写极简 Fake 对象，不想背负继承的样板代码

```python
class FakeClock:
    def now(self) -> float:
        return 1_700_000_000.0
```

只要业务代码依赖的 `Clock` 是一个 Protocol，`FakeClock` 完全不需要 `class FakeClock(Clock):`
这行继承声明，测试代码可以写得极其精简。

### 场景 3：一个类需要同时满足多个互不相关的契约

```python
class Cache(Protocol):
    def get(self, key: str) -> object | None: ...
    def set(self, key: str, value: object) -> None: ...


class Metric(Protocol):
    def increment(self, name: str) -> None: ...


class RedisClient:
    """真实的 Redis 客户端类，同时"恰好"满足 Cache 和 Metric 两个契约。"""

    def get(self, key: str) -> object | None: ...
    def set(self, key: str, value: object) -> None: ...
    def increment(self, name: str) -> None: ...
```

用 ABC 的话，`RedisClient` 就得写 `class RedisClient(Cache, Metric):`，一旦这两个抽象基类
之间有 MRO 冲突（比如都定义了同名但语义不同的方法），会非常棘手。Protocol 不存在这个问题，
因为它根本不参与继承关系。

## 什么时候 ABC 更好

### 场景 1：需要在实例化阶段就强制校验"子类是否实现完整"

```python
from abc import ABC, abstractmethod


class DeviceDriver(ABC):
    @abstractmethod
    def connect(self) -> None: ...
    @abstractmethod
    def disconnect(self) -> None: ...


class BrokenDriver(DeviceDriver):
    def connect(self) -> None:
        pass
    # 忘记实现 disconnect()


BrokenDriver()  # TypeError: Can't instantiate abstract class BrokenDriver with abstract method disconnect
```

如果用 Protocol，这个错误要等到真的调用 `disconnect()` 那一刻才会在运行时报
`AttributeError`；用 ABC，问题在对象**被创建**的那一刻就会暴露，对大团队协作、插件系统这种
"作者和使用者不是同一批人"的场景尤其重要。

### 场景 2：需要在基类里提供共享的默认实现（Template Method）

```python
from abc import ABC, abstractmethod


class DeviceHealthCheck(ABC):
    def run(self, host: str) -> bool:
        """Template Method：固定流程，子类只需要实现"怎么探测"这一步。"""
        try:
            reachable = self._probe(host)
        except Exception:
            return False
        return reachable

    @abstractmethod
    def _probe(self, host: str) -> bool: ...


class PingHealthCheck(DeviceHealthCheck):
    def _probe(self, host: str) -> bool:
        print(f"ping {host}")
        return True
```

Protocol 无法提供"共享的默认方法实现"（它只是一个结构声明），凡是需要"固定骨架 + 可变步骤"
的场景，ABC（配合模板方法模式）是唯一自然的选择。

### 场景 3：需要用 `isinstance()` 对一批插件做运行时分类

```python
from abc import ABC, abstractmethod


class ExportPlugin(ABC):
    @abstractmethod
    def export(self, rows: list[dict]) -> str: ...


def find_all_exporters() -> list[type[ExportPlugin]]:
    return ExportPlugin.__subclasses__()  # ABC 天然支持遍历所有已注册子类
```

`ABCMeta` 天然维护子类注册表（`__subclasses__()`），配合 Part 5 的插件架构手法非常顺手；
Protocol 没有这种"我要拿到所有实现了这个契约的类"的能力（因为它压根不追踪谁满足了它）。

## 什么时候 Duck Typing 更好

### 场景 1：一次性脚本 / 内部工具，生命周期以天计

```python
def send_all(channels, message):
    for ch in channels:
        ch.send(message)
```

给一个只会被用一次、下周就删掉的运维脚本定义 `Protocol` 纯属浪费时间——没有第二个开发者会
读这段代码、没有必要为可能永远不会发生的"未来扩展"预先设计契约。

### 场景 2：探索性/原型阶段，接口形状还在快速变化

在需求还不明确、方法签名可能一天改好几次的原型阶段，过早定义 `Protocol`/`ABC` 会带来"接口
和实现要保持同步"的额外维护负担，拖慢迭代速度。等设计稳定下来后再"回填"接口是更务实的顺序
（这也是很多资深工程师推荐的"先写脏代码验证思路，再重构出抽象"的实践）。

### 场景 3：测试代码里的临时替身，只用一次

```python
class _Spy:
    def __init__(self):
        self.calls = []
    def send(self, message):
        self.calls.append(message)

spy = _Spy()
alert_service_under_test = AlertService([spy])
```

给单测文件里一个只用一次、只在这一个测试函数里出现的替身对象定义 Protocol，收益远小于成本。

## 什么时候根本不用接口

### 场景 1：只有一个实现，且没有任何迹象表明未来会有第二个

```python
class InvoiceNumberGenerator:
    def __init__(self) -> None:
        self._counter = 0

    def next(self) -> str:
        self._counter += 1
        return f"INV-{self._counter:06d}"
```

如果发票编号生成规则在可预见的未来只有一种算法，为它定义一个 `InvoiceNumberGeneratorProtocol`
纯属"为了抽象而抽象"——这是过度设计（over-engineering）的典型反模式，YAGNI（You Aren't
Gonna Need It）原则在这里适用。

### 场景 2：纯数据结构 / 值对象，没有"行为"需要被替换

```python
from dataclasses import dataclass


@dataclass(frozen=True, slots=True)
class Money:
    amount_cents: int
    currency: str
```

`Money` 是一个不可变值对象，不存在"多种实现"的问题（`Money` 就是 `Money`，不会有
`MySQLMoney`/`RedisMoney`），给它套接口没有任何意义。

### 场景 3：模块内部的私有辅助函数

```python
def _parse_mtu(raw: str) -> int:
    return int(raw.strip())
```

只在同一个模块内部被调用、不会被外部替换或注入的私有实现细节，不需要抽象化，直接写成普通
函数/方法即可——DIP 只在"跨越模块/层次边界"时才有意义，模块内部的私有实现细节不属于这个
讨论范围。

## 决策流程图

```
+---------------------------------------------------------------------+
|              这个依赖需要被替换/Mock/跨团队协作吗？                     |
|                              |                                       |
|                    否 -----> 不用接口，直接写具体类/函数                |
|                              |                                       |
|                             是                                       |
|                              v                                       |
|        这是否只是一次性脚本/原型/测试里的临时替身？                       |
|                              |                                       |
|                    是 -----> 用 Duck Typing，不要浪费时间               |
|                              |                                       |
|                             否                                       |
|                              v                                       |
|      需要接第三方现成对象，或者一个类要同时满足多个不相关契约吗？          |
|                              |                                       |
|                    是 -----> 用 typing.Protocol                       |
|                              |                                       |
|                             否                                       |
|                              v                                       |
|   需要实例化即校验完整性，或需要共享默认实现(Template Method)，          |
|   或需要 isinstance()/子类注册表来管理一批插件吗？                       |
|                              |                                       |
|                    是 -----> 用 abc.ABC                               |
|                              |                                       |
|                             否 -----> 优先 typing.Protocol（默认更     |
|                                        Pythonic、耦合更低）             |
+---------------------------------------------------------------------+
```

## 一句话总结

```
+------------------------------------------------------------------+
| Duck Typing : 临时、一次性、双方是"熟人"（同一个人/同一个上下文）写的代码 |
| Protocol    : 结构优先、想接第三方对象、想要零侵入 -> 默认首选           |
| ABC         : 需要强约束、需要共享默认实现、需要子类注册表               |
| 不用接口     : 只有一个实现且未来大概率也不会变、纯数据对象、私有细节      |
+------------------------------------------------------------------+
```

下一步：进入 [Part 10 - 大型项目最佳实践](dip_10_best_practices.md)。
