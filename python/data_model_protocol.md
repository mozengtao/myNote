# Python Data Model 全部 Protocol 总结（Mental Model + Usage）

> **一句话理解 Python Data Model**
>
> Python 并不是依赖继承（Inheritance）来组织语言，而是依赖 **Protocol（协议）**。
>
> **Protocol = Python Runtime 与 Object 之间的一套约定。**
>
> 只要对象实现了某些特殊方法（Magic Methods），Python Runtime 就知道如何与它交互。

---

# 一、整体心智模型

```
                Python Source Code
                       │
                       ▼
              Python Syntax Sugar
                       │
        ┌──────────────┼──────────────┐
        │              │              │
       len()          for          with
       +              in           await
       []             ()           async for
        │              │              │
        └──────────────┼──────────────┘
                       ▼
                Python Runtime
                       │
                       ▼
             Python Data Model
                       │
        ┌──────────────┼──────────────┐
        ▼              ▼              ▼
  Sequence Protocol Iterator Protocol Descriptor Protocol
 Callable Protocol Context Protocol Number Protocol ...
                       │
                       ▼
                 User-defined Object
```

可以把 Data Model 想象成：

```
Python Runtime
      │
      │ "我需要一个对象支持 XXX 操作"
      ▼
Object Protocol
      │
      ▼
Magic Methods
```

所以：

> **Python 并不是调用语法，而是在调用 Protocol。**

---

# 二、Python Data Model 包含哪些 Protocol？

官方 Data Model 可以抽象成如下体系：

```
Python Object
│
├── Object Protocol
├── Attribute Protocol
├── Construction Protocol
├── Representation Protocol
├── Comparison Protocol
├── Boolean Protocol
├── Hash Protocol
├── Container Protocol
├── Sequence Protocol
├── Mapping Protocol
├── Iterator Protocol
├── Generator Protocol
├── Callable Protocol
├── Numeric Protocol
├── Descriptor Protocol
├── Context Manager Protocol
├── Async Protocol
├── Buffer Protocol
├── Class Creation Protocol
└── Pickle Protocol
```

下面逐个介绍。

---

# 三、Object Protocol（对象协议）

所有对象都拥有。

## Magic Methods

```
__new__()
__init__()
__del__()
```

作用：

```
对象创建
对象初始化
对象销毁
```

生命周期：

```
allocate memory

↓

__new__()

↓

__init__()

↓

use object

↓

GC

↓

__del__()
```

典型应用：

- immutable object
- singleton
- object cache

---

# 四、Representation Protocol（对象表示协议）

作用：

决定对象如何打印。

Magic Methods：

```
__repr__()

__str__()

__format__()

__bytes__()
```

对应语法：

```
print(obj)

repr(obj)

f"{obj}"

bytes(obj)
```

心智模型：

```
Object

↓

Representation

↓

String
```

---

# 五、Attribute Access Protocol（属性访问协议）

Magic Methods：

```
__getattribute__()

__getattr__()

__setattr__()

__delattr__()

__dir__()
```

作用：

控制属性访问。

```
obj.x

↓

__getattribute__()

↓

找不到？

↓

__getattr__()
```

应用：

- ORM
- Proxy
- Lazy Loading
- RPC Client

---

# 六、Descriptor Protocol（描述符协议）

Magic Methods：

```
__get__()

__set__()

__delete__()
```

作用：

控制属性本身。

例如：

```
@property

classmethod

staticmethod

cached_property

dataclass

attrs

Pydantic
```

心智模型：

```
attribute

↓

descriptor

↓

real value
```

Descriptor 是 Python 面向对象最重要的机制之一。

---

# 七、Container Protocol（容器协议）

Magic Methods：

```
__contains__()

__getitem__()

__setitem__()

__delitem__()
```

支持：

```
x in obj

obj[i]

obj[i]=v

del obj[i]
```

ASCII：

```
container

↓

index

↓

getitem
```

应用：

```
list

dict

tuple

numpy

pandas
```

---

# 八、Sequence Protocol（序列协议）

Magic Methods：

```
__len__()

__getitem__()
```

作用：

让对象变成 Sequence。

例如：

```
for

reversed()

len()

slice
```

实现：

```
__len__()

__getitem__()
```

Python 就能自动：

```
for x in obj
```

---

# 九、Mapping Protocol（映射协议）

Magic Methods：

```
__getitem__()

__setitem__()

keys()

items()
```

用于：

```
dict-like object
```

例如：

```
config

json

request.args
```

---

# 十、Iterator Protocol（迭代器协议）

最经典 Protocol。

Magic Methods：

```
__iter__()

__next__()
```

对应：

```
for

next()

list()

tuple()
```

心智模型：

```
Iterable

↓

__iter__()

↓

Iterator

↓

__next__()

↓

StopIteration
```

ASCII：

```
Collection

↓

Iterator

↓

next

↓

value

↓

StopIteration
```

---

# 十一、Generator Protocol（生成器协议）

Generator 同时也是 Iterator。

Magic Methods：

```
send()

throw()

close()
```

关键词：

```
yield

yield from
```

心智模型：

```
Generator

↓

pause

↓

resume

↓

pause

↓

resume
```

保存的是：

```
Stack Frame

Local Variables

Instruction Pointer
```

Generator 是：

```
Coroutine 的基础
```

---

# 十二、Callable Protocol（可调用协议）

Magic Method：

```
__call__()
```

作用：

对象像函数。

例如：

```
obj()

↓

__call__()
```

应用：

```
Decorator

Functor

Model()

Loss()

Transform()
```

例如：

```
PyTorch

TensorFlow
```

大量使用。

---

# 十三、Numeric Protocol（数值协议）

Magic Methods：

```
__add__()

__sub__()

__mul__()

__truediv__()

__floordiv__()

__mod__()

__pow__()

__neg__()

__abs__()
```

支持：

```
+

-

*

/

%

**

abs()

-
```

心智模型：

```
Operator

↓

Protocol

↓

Method
```

例如：

```
+

↓

__add__
```

---

# 十四、Comparison Protocol（比较协议）

Magic Methods：

```
__eq__()

__lt__()

__le__()

__gt__()

__ge__()

__ne__()
```

对应：

```
==

<

<=

>

>=

!=
```

应用：

```
sort()

heapq

bisect
```

---

# 十五、Boolean Protocol（布尔协议）

Magic Methods：

```
__bool__()

__len__()
```

对应：

```
if obj

while obj

bool(obj)
```

Python 判断顺序：

```
__bool__

↓

没有？

↓

__len__

↓

0?

↓

False
```

---

# 十六、Hash Protocol（哈希协议）

Magic Methods：

```
__hash__()

__eq__()
```

支持：

```
dict

set
```

规则：

```
equal

↓

same hash
```

---

# 十七、Context Manager Protocol（上下文协议）

Magic Methods：

```
__enter__()

__exit__()
```

对应：

```
with obj:
```

心智模型：

```
Acquire Resource

↓

work

↓

Release Resource
```

例如：

```
File

Lock

Database

Transaction
```

---

# 十八、Async Protocol（异步协议）

Magic Methods：

```
__await__()

__aiter__()

__anext__()

__aenter__()

__aexit__()
```

对应：

```
await

async for

async with
```

ASCII：

```
await

↓

Coroutine

↓

Event Loop
```

---

# 十九、Class Creation Protocol（类创建协议）

Magic Methods：

```
__prepare__()

__new__()

__init_subclass__()

__set_name__()
```

作用：

控制类创建。

流程：

```
class

↓

metaclass

↓

namespace

↓

type object
```

应用：

```
ORM

Enum

ABC

dataclass
```

---

# 二十、Buffer Protocol（缓冲区协议）

用于：

```
bytes

bytearray

memoryview

numpy
```

特点：

```
Zero Copy
```

应用：

```
socket

image

network

shared memory
```

这是高性能 Python 的核心之一。

---

# 二十一、Pickle Protocol（序列化协议）

Magic Methods：

```
__reduce__()

__reduce_ex__()

__getstate__()

__setstate__()
```

作用：

```
pickle.dumps()

pickle.loads()
```

应用：

```
cache

RPC

distributed
```

---

# 二十二、完整关系图

```
                           Python Runtime
                                  │
                                  ▼
                       +----------------------+
                       |   Data Model         |
                       +----------------------+
                                  │
     ┌──────────────┬─────────────┴─────────────┬───────────────┐
     ▼              ▼                           ▼               ▼
Object         Attribute                  Construction     Representation
Protocol       Protocol                    Protocol         Protocol
     │              │                           │               │
     ├──────────────┼───────────────────────────┼───────────────┤
     ▼              ▼                           ▼               ▼
Container     Descriptor                  Callable         Context Manager
Protocol      Protocol                    Protocol         Protocol
     │              │                           │               │
     ▼              ▼                           ▼               ▼
Sequence      Mapping                    Numeric         Comparison
Protocol      Protocol                   Protocol        Protocol
     │              │                           │               │
     └──────────────┼───────────────────────────┼───────────────┘
                    ▼
              Iterator Protocol
                    │
                    ▼
             Generator Protocol
                    │
                    ▼
              Coroutine Protocol
                    │
                    ▼
               Async Protocol
```

---

# 二十三、学习路线（推荐顺序）

按照依赖关系学习，能更容易形成完整的心智模型：

## 第一阶段：对象基础

- Object 生命周期
- Identity / Type / Value
- `__new__`
- `__init__`
- `__del__`

建立 **Everything is an Object** 的认知。

---

## 第二阶段：最常用 Protocol

- Attribute Protocol
- Representation Protocol
- Boolean Protocol
- Comparison Protocol
- Hash Protocol

理解对象如何与解释器交互。

---

## 第三阶段：容器与迭代

- Container Protocol
- Sequence Protocol
- Mapping Protocol
- Iterator Protocol
- Generator Protocol

理解 **Object Flow（对象流）** 与 **Data Flow（数据流）**。

---

## 第四阶段：函数与资源管理

- Callable Protocol
- Context Manager Protocol
- Numeric Protocol

理解对象如何像函数、运算符和资源管理器一样工作。

---

## 第五阶段：高级机制

- Descriptor Protocol
- Class Creation Protocol
- Async Protocol
- Buffer Protocol
- Pickle Protocol

这是大多数 Python 框架（如 Django、SQLAlchemy、Pydantic、FastAPI 等）实现高级特性的基础。

---

# 二十四、最终心智模型

```
                Python 是 Protocol-Oriented Language

                           Python Syntax
                                  │
                                  ▼
                           Python Runtime
                                  │
                                  ▼
                         Python Data Model
                                  │
      ┌─────────────── 所有协议（Protocols） ────────────────┐
      │                                                    │
      │  Object       Iterator      Generator              │
      │  Attribute    Sequence      Mapping                │
      │  Callable     Numeric       Comparison             │
      │  Descriptor   Context       Async                  │
      │  Hash         Boolean       Buffer                 │
      │  Pickle       Class Creation                       │
      └────────────────────────────────────────────────────┘
                                  │
                                  ▼
                           User-defined Objects
                                  │
                                  ▼
                    "对象实现协议，解释器调用协议"
```

> **核心心智模式：**
>
> **Python Data Model = 一组协议（Protocols） + 一组特殊方法（Magic Methods） + 一套解释器调度规则。**
>
> **语法只是糖（Syntax Sugar），真正驱动 Python 行为的是对象实现的协议。** 当对象实现了某个协议，对应的语言特性（如 `for`、`with`、`+`、`await`、`len()` 等）便会自动工作。这也是 Python “鸭子类型（Duck Typing）”和高度可扩展性的根本原因。