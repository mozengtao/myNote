# Python —— 协议（Protocol）

> **核心驱动力：对象能做什么，由它支持哪些协议决定。**
> Python 程序员不问"这是什么类型"，而问"这个对象实现了哪些魔术方法（协议）"。

---

## 心智模型图解

```
Object（对象）
     │
     ▼
Protocol（它实现了哪些魔术方法？）
     │
     ▼
Behavior（因此它能参与哪些语法/内置函数）
```

Python 里几乎所有"看起来像语法糖"的行为，背后都是某个魔术方法（dunder method）在起作用。
`for x in obj` 能跑，是因为 `obj` 实现了 `__iter__`；`obj[0]` 能用，是因为 `obj` 实现了 `__getitem__`。
只要一个对象"长得像"某个协议要求的样子，Python 就认它——这就是 Duck Typing。

---

## 核心驱动力详解

- **协议优先于继承**：Python 不强制你继承某个基类才能参与某种行为，只要实现约定的方法即可。
- **魔术方法是协议的入口**：`__len__`、`__iter__`、`__enter__`、`__call__` 等，都是"我支持某种行为"的声明。
- **内置函数是协议的使用者**：`len(x)` 内部调用 `x.__len__()`；`for x in y` 内部调用 `y.__iter__()`。
- **`typing.Protocol` 把这套隐式约定显式化**，让静态类型检查器也能理解"结构化类型"。

理解了这一点，你看到任何 Python 语法糖，第一反应应该是："这背后对应哪个协议/魔术方法？"

---

## 典型代码片段

### 1. 迭代协议 `__iter__` / `__next__`

```python
class CountDown:
    def __init__(self, start):
        self.current = start

    def __iter__(self):
        return self

    def __next__(self):
        if self.current <= 0:
            raise StopIteration
        self.current -= 1
        return self.current + 1

for n in CountDown(3):
    print(n)  # 3 2 1
```

**心智模型解读**：`for` 循环不关心 `CountDown` 是什么类，只关心它是否实现了迭代协议
（`__iter__` 返回一个实现了 `__next__` 的对象）。这就是"协议决定行为"的最直接体现。

### 2. 序列协议 `__len__` / `__getitem__` —— 鸭子类型的经典案例

```python
class Deck:
    def __init__(self):
        self._cards = [f"{r}{s}" for s in "SHDC" for r in range(1, 14)]

    def __len__(self):
        return len(self._cards)

    def __getitem__(self, idx):
        return self._cards[idx]

deck = Deck()
print(len(deck))       # 使用了 __len__
print(deck[0])          # 使用了 __getitem__
print(deck[-1])
for card in deck:       # 没有定义 __iter__，Python 会自动用 __getitem__ 实现迭代！
    pass
```

**心智模型解读**：即便没有显式实现 `__iter__`，只要实现了 `__getitem__`，
Python 也会把它当作一种"序列协议"的实现，自动支持 `for`、索引、切片。这是"协议叠加"的体现。

### 3. 上下文管理器协议 `__enter__` / `__exit__`

```python
class Timer:
    def __enter__(self):
        import time
        self._start = time.time()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        import time
        print(f"elapsed: {time.time() - self._start:.3f}s")
        return False  # 不吞掉异常

with Timer():
    sum(range(1_000_000))
```

**心智模型解读**：`with` 语句不知道 `Timer` 的具体实现，只知道调用 `__enter__` 拿到资源，
最后一定调用 `__exit__` 释放/收尾——这是协议对"确定性收尾"的保证，而不依赖类型系统。

### 4. 相等与哈希协议 `__eq__` / `__hash__`

```python
class Point:
    def __init__(self, x, y):
        self.x, self.y = x, y

    def __eq__(self, other):
        return isinstance(other, Point) and (self.x, self.y) == (other.x, other.y)

    def __hash__(self):
        return hash((self.x, self.y))

points = {Point(1, 2), Point(1, 2), Point(3, 4)}
print(len(points))  # 2，因为 __eq__/__hash__ 协议让集合认出前两个是"同一个值"
```

**心智模型解读**：`set`/`dict` 判断"两个对象是否算同一个 key"，完全依赖 `__eq__` 与 `__hash__`
这两个协议方法，而不是看它们是不是同一个内存对象（那是 `is`，另一个协议层面）。

### 5. 可调用协议 `__call__` —— 让对象"看起来像函数"

```python
class Multiplier:
    def __init__(self, factor):
        self.factor = factor

    def __call__(self, x):
        return x * self.factor

double = Multiplier(2)
print(double(21))  # 42，double 是对象，但用起来和函数一模一样
```

**心智模型解读**：函数和"实现了 `__call__` 的对象"在使用上没有区别——Python 不关心
你是不是 `function` 类型，只关心你能不能被 `()` 调用。装饰器、函数式风格的类都依赖这一点。

### 6. 鸭子类型函数 —— 不检查类型，只要求"支持某个行为"

```python
def read_all(source) -> str:
    return source.read()

import io
read_all(io.StringIO("hello"))  # 文件对象
read_all(open("/etc/hostname"))  # 真实文件
```

**心智模型解读**：`read_all` 不写任何 `isinstance` 检查，它信任"只要传进来的东西有 `.read()` 方法，
就能正常工作"。这正是"If it walks like a duck and quacks like a duck, it's a duck"。

### 7. 运算符重载协议 `__add__`

```python
class Vector:
    def __init__(self, x, y):
        self.x, self.y = x, y

    def __add__(self, other):
        return Vector(self.x + other.x, self.y + other.y)

    def __repr__(self):
        return f"Vector({self.x}, {self.y})"

print(Vector(1, 2) + Vector(3, 4))  # Vector(4, 6)
```

**心智模型解读**：`+` 运算符本身没有特殊魔力，`a + b` 只是 `a.__add__(b)` 的语法糖。
只要类实现了对应协议，任意自定义类型都能"合法"地参与内置运算符。

### 8. `typing.Protocol` —— 把隐式协议显式化，供静态类型检查

```python
from typing import Protocol

class SupportsClose(Protocol):
    def close(self) -> None: ...

def shutdown(resource: SupportsClose) -> None:
    resource.close()

class Connection:
    def close(self) -> None:
        print("connection closed")

shutdown(Connection())  # Connection 没有继承 SupportsClose，但"结构"匹配即可
```

**心智模型解读**：`Protocol` 不要求显式继承，类型检查器只看"结构是否匹配"（有没有对应方法/签名），
这把 Duck Typing 的隐式约定，变成了类型检查器也能验证的"结构化类型"。

### 9. 字符串表示协议 `__repr__` / `__str__`

```python
class Money:
    def __init__(self, cents):
        self.cents = cents

    def __repr__(self):
        return f"Money(cents={self.cents})"  # 给开发者看，力求无歧义

    def __str__(self):
        return f"${self.cents / 100:.2f}"     # 给用户看，力求友好

m = Money(1050)
print(m)          # $10.50  -> __str__
print([m])        # [Money(cents=1050)] -> 容器打印元素时用 __repr__
```

**心智模型解读**：`print(obj)` 走 `__str__`，而调试/日志/容器打印默认走 `__repr__`。
两者是"面向用户"与"面向开发者"两种不同的展示协议，Python 把它们拆成两个独立方法。

### 10. 生成器 —— 迭代协议的语法糖

```python
def fibonacci(limit):
    a, b = 0, 1
    while a < limit:
        yield a
        a, b = b, a + b

for n in fibonacci(20):
    print(n)
```

**心智模型解读**：`yield` 让 Python 自动帮你生成一个同时实现了 `__iter__` 和 `__next__` 的对象，
本质上和第 1 个例子的 `CountDown` 类是同一件事，只是协议实现被语言隐藏起来了。

### 11. 容器成员协议 `__contains__`

```python
class IPRange:
    def __init__(self, start, end):
        self.start, self.end = start, end

    def __contains__(self, ip):
        return self.start <= ip <= self.end

subnet = IPRange(100, 200)
print(150 in subnet)  # True，触发 __contains__
print(250 in subnet)  # False
```

**心智模型解读**：`in` 关键字并不要求对象是 `list`/`set`，只要求它实现 `__contains__`
（或者退化到用 `__iter__`/`__getitem__` 遍历查找），这又是一次"协议替代类型检查"的例子。

---

## 黄金法则

> **不要想着对象属于什么类，而要想着它支持什么协议。**

看到一段 Python 代码里的某种语法行为（`for`、`with`、`in`、`+`、`()`调用），
第一反应应该是去找对应的魔术方法，而不是去查这个对象的类继承链。

---

## 常见误区对比

### 误区一：用 `isinstance` 类型判断代替协议检查

```python
# 错误心智模型：像静态语言一样先判断具体类型
def total_length(items):
    total = 0
    for item in items:
        if isinstance(item, (list, tuple, str)):
            total += len(item)
    return total
```

```python
# Python 习惯写法：信任协议，用 try 或 hasattr 探测行为
def total_length(items):
    total = 0
    for item in items:
        total += len(item)  # 只要实现了 __len__ 就能工作，无需关心具体类型
    return total
```

**为什么后者更好**：任何自定义容器类，只要实现了 `__len__`，都能直接被这个函数处理，
不需要提前在 `isinstance` 列表里"注册"，这正是协议带来的开放式扩展性。

### 误区二：手写迭代逻辑，而不是实现迭代协议

```python
# 错误心智模型：把"可迭代"实现成一个返回 list 的普通方法
class Batch:
    def __init__(self, items):
        self.items = items

    def get_items(self):
        return list(self.items)

for x in b.get_items():  # 必须记住调用 get_items()，无法直接 for x in b
    ...
```

```python
# Python 习惯写法：实现 __iter__，让对象本身就是可迭代的
class Batch:
    def __init__(self, items):
        self.items = items

    def __iter__(self):
        return iter(self.items)

for x in b:  # 可以直接迭代，还能被 list(b)、sum(b)、in b 等一切迭代相关操作使用
    ...
```

**为什么后者更好**：实现协议后，对象能自动融入 `for`、`list()`、`sorted()`、`in` 等一切依赖
迭代协议的内置机制，而不是只能靠一个专有方法名被外部代码"记住怎么用"。

---

## 快速上手 Checklist

- [ ] 看到 `for`/`in`/`with`/`+`/`len()`/`()`调用，能立刻说出它对应的魔术方法吗？
- [ ] 写自定义容器类时，第一反应是实现 `__iter__`/`__getitem__`，而不是加一个 `get_items()` 方法吗？
- [ ] 判断"这个参数能不能用"时，是看它支持的方法（鸭子类型），还是先写一堆 `isinstance`？
- [ ] 知道 `__repr__`（给开发者）和 `__str__`（给用户）的分工吗？
- [ ] 遇到需要跨类型通用的函数签名时，会想到用 `typing.Protocol` 而不是强制继承一个基类吗？

---

上一篇：[Shell —— 数据流](shell.md) ・ 下一篇：[Go —— 组合](go.md)
