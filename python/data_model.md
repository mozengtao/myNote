# Python Data Model 心智模型：Protocol（协议）+ Duck Typing（鸭子类型）

> **一句话总结**
>
> Python 不关心对象"是什么（Type）"，而关心对象"能做什么（Behavior）"。
>
> 如果对象实现了某组特殊方法（Magic Methods / Special Methods），Python 就认为它遵循了某种 **Protocol（协议）**，各种内置函数和语言特性就会自动支持它。

---

# 一、Python Data Model 的核心思想

很多语言：

```
List
   ▲
   │
MyList
```

通过继承获得能力。

或者

```
implements IList
```

通过接口获得能力。

而 Python 更像这样：

```
             有没有 __len__ ?
                     │
          Yes ───────┘
                     │
                可以 len()

────────────────────────────────

         有没有 __getitem__ ?
                     │
          Yes ───────┘
                     │
      可以 [] / slice / choice()

────────────────────────────────

         有没有 __iter__ ?
                     │
          Yes ───────┘
                     │
             可以 for ...

────────────────────────────────

        有没有 __contains__ ?
                     │
          Yes ───────┘
                     │
              可以 in
```

Python 根本不会问：

> 这是 list 吗？

而是问：

> 有没有我需要的方法？

这就是

> **Protocol + Duck Typing**

---

# 二、Python Data Model 全景图

```
                    Python Object
                          │
        ┌─────────────────┼─────────────────┐
        │                 │                 │
   Sequence           Iterator          Numeric
   Protocol           Protocol          Protocol
        │                 │                 │
        ▼                 ▼                 ▼
  __len__()          __iter__()         __add__()
  __getitem__()      __next__()         __sub__()
  __contains__()                        __mul__()
```

各种 Python 内置函数都是：

```
Builtin Function
        │
        ▼
calls Magic Method
        │
        ▼
 Your Object
```

例如：

```
len(deck)
      │
      ▼
deck.__len__()

────────────────────

deck[0]
      │
      ▼
deck.__getitem__(0)

────────────────────

for x in deck
      │
      ▼
deck.__iter__()

────────────────────

x in deck
      │
      ▼
deck.__contains__()
```

---

# 三、Sequence Protocol（序列协议）

## 1）__len__()

### 对应操作

```
len(obj)
```

### 底层行为

```
len(obj)

    │

    ▼

obj.__len__()
```

### 示例

```python
class Team:

    def __init__(self):
        self.players = ["Alice", "Bob", "Charlie"]

    def __len__(self):
        return len(self.players)


team = Team()

print(len(team))
```

输出

```
3
```

---

## 2）__getitem__()

支持

```
obj[index]

obj[start:end]

obj[::-1]
```

### 底层行为

```
obj[3]

↓

obj.__getitem__(3)
```

切片：

```
obj[1:5]

↓

obj.__getitem__(slice(1,5,None))
```

### 示例

```python
class Squares:

    def __getitem__(self, index):
        return index * index


s = Squares()

print(s[5])
```

输出

```
25
```

---

### slice 示例

```python
class Demo:

    def __getitem__(self, item):
        print(type(item))
        print(item)
        return item


d = Demo()

d[:10]
```

输出

```
<class 'slice'>

slice(0,10,None)
```

---

## 3）__setitem__()

支持

```
obj[index] = value
```

底层：

```
obj.__setitem__(index, value)
```

示例

```python
class Counter:

    def __init__(self):
        self.data = {}

    def __setitem__(self, key, value):
        self.data[key] = value


c = Counter()

c["age"] = 18
```

---

## 4）__delitem__()

```
del obj[index]
```

↓

```
obj.__delitem__(index)
```

---

## 5）__contains__()

对应

```
x in obj
```

底层：

```
obj.__contains__(x)
```

示例

```python
class Team:

    def __init__(self):
        self.players = {"Alice", "Bob"}

    def __contains__(self, item):
        return item in self.players


team = Team()

print("Alice" in team)
```

---

### 如果没有 __contains__

Python 自动退化：

```
for item in obj:

    if item == target:

        return True
```

即：

```
x in obj

     │
     ▼

__contains__ ?

     │
 No  │
     ▼

__iter__()

     │
 No  │
     ▼

__getitem__(0)

__getitem__(1)

...

IndexError
```

---

# 四、Iterator Protocol（迭代器协议）

## 1）__iter__()

支持

```
for
```

```
list()
```

```
tuple()
```

```
set()
```

等等。

### 示例

```python
class Countdown:

    def __init__(self, n):
        self.n = n

    def __iter__(self):
        while self.n > 0:
            yield self.n
            self.n -= 1


for x in Countdown(5):
    print(x)
```

---

## 2）__next__()

真正的 Iterator

```python
class Counter:

    def __init__(self):
        self.i = 0

    def __iter__(self):
        return self

    def __next__(self):

        if self.i == 5:
            raise StopIteration

        self.i += 1
        return self.i
```

---

### for 的底层

```
iter(obj)

↓

obj.__iter__()

↓

iterator

↓

__next__()

↓

StopIteration
```

---

# 五、为什么 FrenchDeck 没有 __iter__ 仍然能 for？

因为：

Python 有 fallback。

```
__iter__ ?

      │
 Yes ─┘

      ▼

 iterator

────────────────────────────

 No

      ▼

obj[0]

obj[1]

obj[2]

...

IndexError
```

即：

```
for x in obj
```

退化成

```
index = 0

while True:

    try:

        item = obj[index]

    except IndexError:

        break

    index += 1
```

因此

实现

```
__getitem__()
```

就已经可以 for。

---

# 六、Reversed Protocol

```
reversed(obj)
```

优先：

```
__reversed__()
```

否则：

```
len()

+

getitem()
```

即：

```
for i in range(len(obj)-1,-1,-1):

    obj[i]
```

---

# 七、Random Protocol

例如：

```python
from random import choice
```

choice()

实际上只需要：

```
len()

+

getitem()
```

伪代码：

```python
i = randint(0, len(obj)-1)

return obj[i]
```

所以

FrenchDeck

不用写任何代码：

```
choice(deck)
```

即可工作。

---

# 八、Callable Protocol

实现：

```
__call__()
```

对象即可变成函数。

示例：

```python
class Adder:

    def __call__(self, x, y):
        return x + y


add = Adder()

print(add(3,4))
```

实际上：

```
add(3,4)

↓

add.__call__(3,4)
```

---

# 九、String Protocol

```
print(obj)

↓

__str__()
```

```
repr(obj)

↓

__repr__()
```

示例

```python
class User:

    def __repr__(self):
        return "User(admin)"

    def __str__(self):
        return "admin"


u = User()

print(u)

print(repr(u))
```

---

# 十、Comparison Protocol

支持

```
<

<=

>

>=

==

!=
```

分别对应

```
__lt__()

__le__()

__gt__()

__ge__()

__eq__()

__ne__()
```

示例

```python
class Score:

    def __init__(self, score):
        self.score = score

    def __lt__(self, other):
        return self.score < other.score
```

于是

```
a < b
```

变成

```
a.__lt__(b)
```

---

# 十一、Numeric Protocol

```
+

-

*

/

%

**
```

对应：

```
__add__()

__sub__()

__mul__()

__truediv__()

__mod__()

__pow__()
```

例如：

```python
class Vector:

    def __init__(self,x,y):
        self.x=x
        self.y=y

    def __add__(self,other):
        return Vector(
            self.x+other.x,
            self.y+other.y
        )
```

于是：

```
a+b

↓

a.__add__(b)
```

---

# 十二、Boolean Protocol

```
if obj:
```

底层：

优先：

```
__bool__()
```

否则：

```
__len__()
```

即

```
__bool__()

     │

     No

     ▼

len(obj)!=0
```

例如：

```python
class Bag:

    def __len__(self):
        return 0


if Bag():
    print("True")
else:
    print("False")
```

输出：

```
False
```

---

# 十三、Context Manager Protocol

```
with obj:
```

调用：

```
__enter__()

↓

代码块

↓

__exit__()
```

示例

```python
class Timer:

    def __enter__(self):
        print("start")

    def __exit__(self,exc_type,exc,tb):
        print("end")
```

---

# 十四、Attribute Access Protocol

访问：

```
obj.x
```

可能调用：

```
__getattribute__()
```

找不到：

```
__getattr__()
```

赋值：

```
__setattr__()
```

删除：

```
__delattr__()
```

---

# 十五、Python 常见 Special Methods 一览表

| 协议 | 特殊方法 | 对应语言特性 / 内置函数 | 底层行为 |
|------|----------|-------------------------|----------|
| Sequence | `__len__` | `len(obj)` | `obj.__len__()` |
| Sequence | `__getitem__` | `obj[i]`、切片 | `obj.__getitem__(i)` |
| Sequence | `__setitem__` | `obj[i]=x` | `obj.__setitem__(i,x)` |
| Sequence | `__delitem__` | `del obj[i]` | `obj.__delitem__(i)` |
| Sequence | `__contains__` | `x in obj` | `obj.__contains__(x)` |
| Iterator | `__iter__` | `for`、`iter()` | `obj.__iter__()` |
| Iterator | `__next__` | `next()` | `obj.__next__()` |
| Callable | `__call__` | `obj()` | `obj.__call__()` |
| String | `__str__` | `str()`、`print()` | `obj.__str__()` |
| String | `__repr__` | `repr()` | `obj.__repr__()` |
| Comparison | `__lt__` 等 | `<`、`==` 等 | `obj.__lt__(other)` 等 |
| Numeric | `__add__` 等 | `+`、`-`、`*` 等 | `obj.__add__(other)` 等 |
| Boolean | `__bool__` | `if obj:` | `obj.__bool__()` |
| Context Manager | `__enter__`、`__exit__` | `with` | 自动调用 |
| Attribute | `__getattr__` 等 | 属性访问 | 自动调用 |

---

# 十六、Protocol（协议）与 Duck Typing（鸭子类型）

Python 的各种内置函数实际上都遵循如下工作模式：

```
            Python Builtin

               len(obj)

                  │

                  ▼

        obj 有没有 __len__ ?

                  │

        Yes ──────┘

                  ▼

          调用 __len__()

──────────────────────────────────

             for obj

                  │

                  ▼

       obj 有没有 __iter__ ?

                  │

        Yes ──────┘

                  ▼

             调用 __iter__()

──────────────────────────────────

            x in obj

                  │

                  ▼

     obj 有没有 __contains__ ?

                  │

        Yes ──────┘

                  ▼

        调用 __contains__()

──────────────────────────────────

            obj()

                  │

                  ▼

     obj 有没有 __call__ ?

                  │

        Yes ──────┘

                  ▼

         调用 __call__()
```

因此，Python 的对象能力来自于 **行为（Behavior）**，而不是 **类型（Type）**。

---

# 十七、最终心智模型（最重要）

```
                    Python Object
                           │
         ┌─────────────────┼─────────────────┐
         │                 │                 │
     实现 __len__()    实现 __iter__()   实现 __call__()
         │                 │                 │
         ▼                 ▼                 ▼
     len(obj)          for obj          obj(...)
         │                 │                 │
─────────┼─────────────────┼─────────────────┼──────────
         │                 │                 │
    Python Builtins / Language Features
         │                 │                 │
         ▼                 ▼                 ▼
        Protocol（协议）
                 │
                 ▼
        Duck Typing（鸭子类型）
                 │
                 ▼
    Don't ask "What are you?"
    Ask "What can you do?"
                 │
                 ▼
     实现对应的 Special Method，
     Python 就自动赋予对象相应的语言能力。
```

---

# 十八、学习 Python Data Model 的建议路线

建议按下面的顺序掌握，因为它们几乎覆盖了日常 Python 编程中 90% 以上的数据模型知识：

1. **Sequence Protocol**
   - `__len__`
   - `__getitem__`
   - `__setitem__`
   - `__contains__`

2. **Iterator Protocol**
   - `__iter__`
   - `__next__`
   - `yield`
   - `iter()` / `next()`

3. **Object Representation**
   - `__repr__`
   - `__str__`
   - `__format__`

4. **Callable Objects**
   - `__call__`

5. **Operator Overloading**
   - `__add__`
   - `__sub__`
   - `__mul__`
   - `__eq__`
   - `__lt__`

6. **Truth Value Testing**
   - `__bool__`
   - `__len__`

7. **Context Manager**
   - `__enter__`
   - `__exit__`

8. **Attribute Access**
   - `__getattribute__`
   - `__getattr__`
   - `__setattr__`

掌握这些协议后，再阅读《Fluent Python》的前 13 章，你会发现绝大多数 Python 语言特性都可以统一理解为：**语言特性 → 特殊方法 → 协议（Protocol）→ 对象行为（Behavior）**，这也是 Python Data Model 最核心的设计哲学。