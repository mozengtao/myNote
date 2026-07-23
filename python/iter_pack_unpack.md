# Python 可迭代对象 Packing 与 Unpacking（Iterable Packing / Unpacking）

> **一句话理解：**
>
> - **Packing（打包）**：把多个独立对象收集成一个容器（Collection）。
> - **Unpacking（解包）**：把一个容器中的元素重新展开成多个独立对象。
>
> **心智模式：**
>
> ```text
> 多个对象
>     │
>     │ Packing
>     ▼
> 一个 Iterable（list/tuple/set/generator/...）
>     │
>     │ Unpacking (*)
>     ▼
> 多个对象
> ```
>
> Python 中大量语法都是围绕这一思想设计的，例如：
>
> - 函数调用
> - 参数传递
> - 多变量赋值
> - asyncio.gather()
> - zip()
> - list/tuple/set 合并
> - 字典合并（**）

---

# 1. Packing（打包）

## 示例1：Tuple Packing

多个对象自动打包成 tuple

```python
a = 1
b = 2
c = 3

t = a, b, c

print(t)
```

输出

```text
(1, 2, 3)
```

等价于

```python
t = (1, 2, 3)
```

心智模型

```text
    1
    2
    3
    │
    ▼
   Tuple
 ┌────────────┐
 │1│2│3│
 └────────────┘
```

---

## 示例2：函数中的 *args（Packing）

```python
def foo(*args):
    print(args)

foo(10, 20, 30)
```

输出

```text
(10, 20, 30)
```

这里：

```python
10
20
30
```

被自动打包成

```python
args = (10, 20, 30)
```

心智模型

```text
多个参数
   │
   ▼
args(tuple)
```

---

## 示例3：字典 Packing (**kwargs)

```python
def foo(**kwargs):
    print(kwargs)

foo(name="Tom", age=20)
```

输出

```text
{'name': 'Tom', 'age': 20}
```

多个 Key-Value

打包成

```python
dict
```

---

# 2. Unpacking（解包）

## 示例1：Tuple 解包

```python
t = (1, 2, 3)

a, b, c = t

print(a, b, c)
```

输出

```text
1 2 3
```

心智模型

```text
Tuple
 ┌──────────┐
 │1│2│3│
 └──────────┘
  │ │ │
  ▼ ▼ ▼
  a b c
```

---

## 示例2：List 解包

```python
nums = [10, 20, 30]

a, b, c = nums
```

只要是 iterable

都可以解包。

---

## 示例3：Generator 解包

```python
gen = (i for i in range(3))

a, b, c = gen

print(a, b, c)
```

输出

```text
0 1 2
```

Generator 也是 iterable。

---

# 3. * 在函数调用中的作用

这是最重要的用法。

例如：

```python
def add(a, b, c):
    return a + b + c

nums = [1, 2, 3]

print(add(*nums))
```

实际上

```python
add(*nums)
```

等价于

```python
add(1, 2, 3)
```

心智模型

```text
List
 ┌────────────┐
 │1│2│3│
 └────────────┘
      │
      │ *
      ▼
  1,2,3
      │
      ▼
函数参数
```

---

# 4. Generator + *

例如

```python
gen = (i for i in range(5))

print(*gen)
```

输出

```text
0 1 2 3 4
```

Generator

```text
generator
   │
   ├──0
   ├──1
   ├──2
   └──...
```

经过 *

```text
0,1,2,3,4
```

---

# 5. asyncio.gather() 中的 *

例如

```python
async def increment():
    ...

await asyncio.gather(
    *(increment() for _ in range(100))
)
```

实际上

Generator

```text
generator
    │
    ├── coro1
    ├── coro2
    ├── ...
    └── coro100
```

经过 *

变成

```python
asyncio.gather(
    coro1,
    coro2,
    ...
    coro100
)
```

而不是

```python
asyncio.gather(generator)
```

这是 asyncio 中最经典的 unpacking 应用。

---

# 6. List 合并

```python
a = [1, 2]
b = [3, 4]

c = [*a, *b]

print(c)
```

输出

```text
[1, 2, 3, 4]
```

相当于

```text
a
 │
 ▼
1 2

b
 │
 ▼
3 4

↓

1 2 3 4
```

---

# 7. Tuple 合并

```python
t1 = (1, 2)
t2 = (3, 4)

t = (*t1, *t2)

print(t)
```

输出

```text
(1, 2, 3, 4)
```

---

# 8. Set 合并

```python
s1 = {1, 2}
s2 = {2, 3}

s = {*s1, *s2}

print(s)
```

输出

```text
{1, 2, 3}
```

---

# 9. Dictionary 解包（**）

例如

```python
d1 = {"a": 1}
d2 = {"b": 2}

d = {**d1, **d2}

print(d)
```

输出

```text
{'a': 1, 'b': 2}
```

这里

```python
**
```

表示

把

```python
key:value
```

展开。

心智模型

```text
dict1

a→1

dict2

b→2

↓

a→1
b→2
```

---

# 10. zip() + 解包

例如

```python
pairs = [
    ("Tom", 18),
    ("Jerry", 20),
]

names, ages = zip(*pairs)

print(names)
print(ages)
```

输出

```text
('Tom', 'Jerry')
(18, 20)
```

这里

```python
*pairs
```

变成

```python
zip(
    ("Tom",18),
    ("Jerry",20)
)
```

这是 Python 中非常经典的技巧。

---

# 11. 扩展解包（Extended Unpacking）

Python 允许一个变量接收剩余元素。

```python
nums = [1,2,3,4,5]

a, *middle, b = nums

print(a)
print(middle)
print(b)
```

输出

```text
1
[2,3,4]
5
```

心智模型

```text
1 2 3 4 5

│ └────┘ │

a middle b
```

---

# 12. Packing 与 Unpacking 对应关系

## Packing

```python
def foo(*args):
    ...
```

调用

```python
foo(1,2,3)
```

发生

```text
1
2
3

↓

args

↓

(1,2,3)
```

---

## Unpacking

```python
nums = (1,2,3)

foo(*nums)
```

发生

```text
(1,2,3)

↓

1
2
3

↓

foo(1,2,3)
```

它们互为镜像。

---

# 13. Python 中 * 与 ** 的完整用途

| 语法 | 作用 | 示例 |
|------|------|------|
| `*args` | Packing 参数 | `def foo(*args)` |
| `**kwargs` | Packing 关键字参数 | `def foo(**kwargs)` |
| `*iterable` | 函数调用解包 | `foo(*nums)` |
| `**dict` | 关键字参数解包 | `foo(**config)` |
| `[*a,*b]` | List 合并 | `[1,2,3,4]` |
| `(*a,*b)` | Tuple 合并 | `(1,2,3,4)` |
| `{*a,*b}` | Set 合并 | `{1,2,3}` |
| `{**d1,**d2}` | Dict 合并 | `{...}` |
| `a,*b,c = seq` | Extended Unpacking | 接收剩余元素 |

---

# 黄金法则（Golden Rules）

## 法则一：Packing 是"收集"

```text
多个对象
      │
      ▼
一个容器
```

例如

```python
*args
**kwargs
```

都是收集。

---

## 法则二：Unpacking 是"展开"

```text
一个 Iterable
        │
        ▼
多个对象
```

例如

```python
foo(*nums)
```

就是把 iterable 展开。

---

## 法则三：* 作用于位置参数，** 作用于键值参数

```text
*
↓

1
2
3

**

↓

key:value
```

---

## 法则四：`*` 不关心对象类型，只关心是否是 Iterable

下面这些都可以解包：

```python
list
tuple
set
range
generator
str
dict（默认展开 key）
```

例如

```python
print(*"Python")
```

输出

```text
P y t h o n
```

---

# 最终心智模型（Mental Model）

```text
                   Packing
多个对象 ─────────────────────► 一个 Iterable
    ▲                              │
    │                              │
    │                              │
    └────────────── Unpacking ◄────┘
                 (* / **)

------------------------------------------------

函数定义
foo(*args)
        ▲
        │ Packing

函数调用
foo(*iterable)
        │
        ▼ Unpacking

------------------------------------------------

多个 coroutine
        │
        ▼
Generator
        │
        ▼
*(generator)
        │
        ▼
gather(coro1,coro2,...)
        │
        ▼
Event Loop 并发调度
```

> **核心心智模式：**
>
> **Packing 是"把多个对象收集成一个容器"，Unpacking 是"把一个容器重新展开成多个对象"。**
>
> `*` 表示**按位置展开（Positional Unpacking）**，`**` 表示**按键值展开（Keyword Unpacking）**。Python 的函数调用、参数传递、容器构造、`asyncio.gather()`、`zip()` 等大量语法，本质上都建立在这一套统一的「收集 ↔ 展开」模型之上。