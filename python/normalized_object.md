# Python 常见数据对象承载方式（Normalized Python Object）

> 核心思想：
>
> **Python 中的数据对象(Data Object)是程序内部对现实世界或业务概念的抽象。**
>
> Parser、数据库、JSON、API、配置文件等所有输入，最终都会转换成某一种 Python 数据对象。
>
> 因此，学习各种数据对象的目的，不是记住语法，而是建立**"数据建模(Data Modeling)"**的心智模式。

---

# 一、统一视角

任何数据都会经历下面的生命周期：

```text
          外部世界
              │
              │
      JSON / YAML / CLI
              │
              ▼
        Python Object
              │
              ▼
      Business Logic
              │
              ▼
 CSV / JSON / Database / API
```

Python Object 就是整个程序内部的数据载体。

---

# 二、Python 数据对象的演化路线

```
简单 ----------------------------------------------> 复杂

tuple
   │
namedtuple
   │
dict
   │
TypedDict
   │
dataclass
   │
class
   │
attrs
   │
Pydantic
```

它们其实代表了不同层次的数据建模能力。

---

# 三、tuple —— 顺序数据（Positional Object）

## 心智模型

```
对象

↓

固定顺序的数据
```

数据靠**位置**表示意义。

例如：

```
(pid,user,cpu)
```

而不是：

```
pid=...
```

---

## 示例1：二维坐标

```python
point = (100, 200)

x, y = point
```

---

## 示例2：RGB颜色

```python
color = (255, 128, 64)

r, g, b = color
```

---

## 示例3：数据库查询

```python
row = ("Alice", 20, "Canada")

name, age, country = row
```

---

## 核心处理代码

### 示例1：访问字段（按位置索引）

```python
row = ("Alice", 20, "Canada")

name = row[0]
age = row[1]
```

---

### 示例2：遍历多条记录

```python
rows = [
    ("Alice", 20, "Canada"),
    ("Bob", 17, "USA"),
]

for name, age, country in rows:
    print(name, age, country)
```

---

### 示例3：过滤记录

```python
adults = [r for r in rows if r[1] >= 18]
```

---

### 示例4：更新（不可变，只能重建）

```python
row = ("Alice", 20, "Canada")

row = (row[0], 21, row[2])
```

---

### 示例5：转换为 dict（序列化）

```python
fields = ("name", "age", "country")
row = ("Alice", 20, "Canada")

d = dict(zip(fields, row))
```

---

适合：

- 临时数据
- 返回多个值
- 数学数据

---

# 四、namedtuple —— 有名字的 tuple

## 心智模型

```
tuple

+

字段名
```

仍然不可变。

---

## 示例1：二维坐标

```python
from collections import namedtuple

Point = namedtuple("Point", ["x", "y"])

p = Point(100, 200)

print(p.x)
```

---

## 示例2：网络地址

```python
Endpoint = namedtuple(
    "Endpoint",
    [
        "ip",
        "port",
    ],
)
```

---

## 示例3：解析 CSV

```python
Employee = namedtuple(
    "Employee",
    [
        "name",
        "age",
        "salary",
    ],
)
```

---

## 核心处理代码

### 示例1：按名字访问字段

```python
Point = namedtuple("Point", ["x", "y"])

p = Point(100, 200)

print(p.x, p.y)
```

---

### 示例2：遍历多条记录

```python
points = [Point(0, 0), Point(1, 1), Point(2, 4)]

for p in points:
    print(p.x, p.y)
```

---

### 示例3：过滤记录

```python
employees = [
    Employee("Alice", 20, 5000),
    Employee("Bob", 35, 8000),
]

young = [e for e in employees if e.age < 30]
```

---

### 示例4：更新（`_replace` 生成新对象）

```python
p2 = p._replace(x=999)
```

---

### 示例5：转换为 dict

```python
d = p._asdict()
```

---

适合：

- 轻量只读对象
- 高性能数据

---

# 五、dict —— 最灵活的数据对象

## 心智模型

```
对象

↓

属性集合(Attribute Map)
```

数据靠：

```
Key

↓

Value
```

表达意义。

---

## 示例1：CLI Parser

```python
record = {
    "cm": "...",
    "ucid": "0/13",
    "uncorrectable": 47,
}
```

---

## 示例2：JSON

```python
user = {
    "name": "Tom",
    "age": 30,
}
```

---

## 示例3：配置文件

```python
config = {
    "host": "...",
    "port": 8080,
}
```

---

## 核心处理代码

### 示例1：安全访问字段

```python
uncorrectable = record.get("uncorrectable", 0)
```

---

### 示例2：遍历字段

```python
for key, value in record.items():
    print(key, value)
```

---

### 示例3：过滤记录

```python
records = [
    {"cm": "cm1", "uncorrectable": 0},
    {"cm": "cm2", "uncorrectable": 47},
]

bad = [r for r in records if r["uncorrectable"] > 0]
```

---

### 示例4：更新字段

```python
record["uncorrectable"] += 1

record.update({"ucid": "0/14"})
```

---

### 示例5：转换为 JSON（序列化）

```python
import json

json.dumps(record)
```

---

适合：

- ETL
- CLI Parser
- Log Parser
- JSON

---

# 六、TypedDict —— 带类型的 dict

## 心智模型

```
dict

+

静态类型
```

运行时仍然是 dict。

IDE 能检查：

```
字段

类型

补全
```

---

## 示例1：网络设备

```python
class Device(TypedDict):

    hostname: str

    ip: str
```

---

## 示例2：日志

```python
class LogRecord(TypedDict):

    level: str

    message: str
```

---

## 示例3：Parser Record

```python
class Channel(TypedDict):

    cm: str

    ucid: str

    no_burst: int
```

---

## 核心处理代码

### 示例1：访问字段（依然是 dict 语法）

```python
channel: Channel = {"cm": "cm1", "ucid": "0/13", "no_burst": 3}

no_burst = channel["no_burst"]
```

---

### 示例2：遍历多条记录

```python
channels: list[Channel] = [
    {"cm": "cm1", "ucid": "0/13", "no_burst": 3},
    {"cm": "cm2", "ucid": "0/14", "no_burst": 0},
]

for ch in channels:
    print(ch["cm"], ch["no_burst"])
```

---

### 示例3：过滤记录

```python
bad = [c for c in channels if c["no_burst"] > 0]
```

---

### 示例4：更新字段

```python
channel["no_burst"] += 1
```

---

### 示例5：转换为 JSON（序列化）

```python
import json

json.dumps(channel)
```

---

适合：

中大型 Parser。

---

# 七、dataclass —— 数据对象（推荐）

## 心智模型

```
现实对象

↓

Python 对象
```

重点：

> 数据有明确结构，但行为较少。

---

## 示例1：CM Channel

```python
@dataclass
class Channel:

    cm: str

    ucid: str

    uncorrectable: int
```

---

## 示例2：学生

```python
@dataclass
class Student:

    name: str

    age: int

    score: float
```

---

## 示例3：配置

```python
@dataclass
class Config:

    host: str

    port: int
```

---

## 核心处理代码

### 示例1：属性访问

```python
channel = Channel(cm="cm1", ucid="0/13", uncorrectable=47)

print(channel.uncorrectable)
```

---

### 示例2：遍历多条记录

```python
channels = [
    Channel(cm="cm1", ucid="0/13", uncorrectable=47),
    Channel(cm="cm2", ucid="0/14", uncorrectable=0),
]

for ch in channels:
    print(ch.cm, ch.uncorrectable)
```

---

### 示例3：过滤记录

```python
bad = [c for c in channels if c.uncorrectable > 0]
```

---

### 示例4：更新（`dataclasses.replace` 生成新对象）

```python
from dataclasses import replace

new_channel = replace(channel, uncorrectable=0)
```

---

### 示例5：转换为 dict（序列化）

```python
from dataclasses import asdict

asdict(channel)
```

---

适合：

- 配置对象
- Parser 输出
- DTO（Data Transfer Object）
- Domain Model

---

# 八、class —— 领域对象（Domain Object）

## 心智模型

```
数据

+

行为
```

对象不仅保存数据，

还能：

```
计算

验证

修改状态

执行业务
```

---

## 示例1：BankAccount

```python
class BankAccount:

    def deposit(self):

        ...

    def withdraw(self):

        ...
```

---

## 示例2：Router

```python
class Router:

    def add_route(self):

        ...

    def lookup(self):

        ...
```

---

## 示例3：Packet

```python
class Packet:

    def checksum(self):

        ...

    def encode(self):

        ...
```

---

## 核心处理代码

### 示例1：属性/方法访问

```python
account = BankAccount(balance=100)

print(account.balance)
```

---

### 示例2：遍历并调用行为方法

```python
accounts = [BankAccount(balance=100), BankAccount(balance=-20)]

for acc in accounts:
    acc.deposit(50)
```

---

### 示例3：结合业务规则过滤

```python
overdrawn = [a for a in accounts if a.balance < 0]
```

---

### 示例4：通过方法修改状态（而不是直接赋值）

```python
account.withdraw(50)
```

---

### 示例5：自定义转换方法（序列化）

```python
class BankAccount:

    def to_dict(self):

        return {"balance": self.balance}
```

---

适合：

DDD

复杂业务

状态机

---

# 九、attrs —— dataclass 增强版

## 心智模型

```
dataclass

+

高级能力
```

例如：

```
validator

converter

slots

immutable
```

---

## 示例1：配置校验

```python
@attrs.define
class Config:

    port: int
```

自动验证：

```
port > 0
```

---

## 示例2：单位转换

```python
@attrs.define
class Temperature:

    value: float
```

自动：

```
str

↓

float
```

---

## 示例3：不可变对象

```python
@attrs.frozen
class Point:

    x: int

    y: int
```

---

## 核心处理代码

### 示例1：属性访问

```python
config = Config(port=8080)

print(config.port)
```

---

### 示例2：遍历并校验

```python
configs = [Config(port=8080), Config(port=-1)]

for c in configs:
    validate(c)
```

---

### 示例3：过滤

```python
invalid = [c for c in configs if c.port <= 0]
```

---

### 示例4：更新（`attrs.evolve` 生成新对象，适用于 frozen）

```python
import attrs

new_config = attrs.evolve(config, port=9090)
```

---

### 示例5：转换为 dict（序列化）

```python
attrs.asdict(config)
```

---

适合：

大型工程。

---

# 十、Pydantic —— 边界对象（Boundary Object）

## 心智模型

```
外部数据

↓

自动校验

↓

Python Object
```

用于：

```
API

JSON

配置

数据库
```

---

## 示例1：REST API

```python
class User(BaseModel):

    name: str

    age: int
```

收到：

```json
{
    "name":"Tom",
    "age":"18"
}
```

自动：

```
age -> int
```

---

## 示例2：配置文件

```python
class Settings(BaseModel):

    host: str

    port: int
```

---

## 示例3：Parser

```python
Channel.model_validate(data)
```

自动检查：

```
字段

类型

缺失字段
```

---

## 核心处理代码

### 示例1：属性访问

```python
user = User(name="Tom", age=18)

print(user.age)
```

---

### 示例2：批量构造并遍历

```python
raw_list = [
    {"name": "Tom", "age": "18"},
    {"name": "Amy", "age": "20"},
]

users = [User.model_validate(d) for d in raw_list]

for u in users:
    print(u.name, u.age)
```

---

### 示例3：过滤

```python
adults = [u for u in users if u.age >= 18]
```

---

### 示例4：更新（`model_copy` 生成新对象）

```python
user2 = user.model_copy(update={"age": 19})
```

---

### 示例5：转换为 JSON（序列化）

```python
user.model_dump_json()
```

---

适合：

- FastAPI
- JSON
- YAML
- 配置

---

# 十一、各种对象的定位

```
tuple
│
│  数学数据
│
▼

namedtuple
│
│  轻量对象
│
▼

dict
│
│  数据搬运
│
▼

TypedDict
│
│  类型安全
│
▼

dataclass
│
│  数据模型
│
▼

class
│
│  领域对象
│
▼

attrs
│
│  企业级数据模型
│
▼

Pydantic
│
│  系统边界
│
▼

API / JSON / Config
```

---

# 十二、如何选择

| 数据特点 | 推荐对象 |
|-----------|-----------|
| 返回多个简单值 | tuple |
| 轻量只读对象 | namedtuple |
| JSON、CLI、Log、ETL | dict |
| dict + 类型检查 | TypedDict |
| 数据对象（DTO、Parser 输出） | dataclass ⭐⭐⭐⭐⭐ |
| 数据 + 行为（业务规则） | class |
| dataclass 增强版 | attrs |
| API、配置、JSON 校验 | Pydantic |

---

# 十三、统一的数据建模心智模式

```
                    我正在表示什么？
                            │
          ┌─────────────────┴─────────────────┐
          │                                   │
      只是几个值？                     一个现实世界对象？
          │                                   │
      tuple                          dataclass / class
          │                                   │
          │                           是否只有数据？
          │                                   │
          │                        是 ────────┴──────── 否
          │                        │                    │
          │                   dataclass             class
          │
          │
    来自 JSON / CLI ?
          │
    是 ───┴──── 否
    │
dict / TypedDict
    │
需要自动校验？
    │
   是
    │
Pydantic
```

---

# 十四、黄金法则（建议牢记）

1. **tuple：表示一组有固定顺序的值。**
2. **dict：表示一组键值对，是数据搬运的首选。**
3. **dataclass：表示一个现实世界的数据对象，是现代 Python 数据建模的首选。**
4. **class：表示一个具有状态和行为的领域对象。**
5. **Pydantic：表示系统边界上的数据对象，负责校验、转换和序列化。**

> **一句话总结：**
>
> Python 中各种数据对象并不是相互竞争的关系，而是针对不同抽象层次的建模工具。可以把它们理解为一条逐步增强的建模能力链：**tuple（值）→ dict（属性）→ dataclass（数据模型）→ class（领域模型）→ Pydantic（边界模型）**。实际开发中，应根据数据的复杂度、是否需要行为、是否需要类型校验以及是否位于系统边界来选择最合适的数据对象承载方式。