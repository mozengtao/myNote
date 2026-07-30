# Python Tuple Construction：构造 Tuple 的完整心智模式

> **核心思想：**
>
> Python 中，**tuple 并不是只有一种创建方式，而是根据数据来源(Data Source)进行构造。**
>
> 和 list 最大的不同：**tuple 是不可变(immutable)的**，一旦构造完成，就不能再原地修改。
> 所以 tuple 没有 `append()`、`extend()` 这类原地方法，任何"改变"都意味着**重新构造一个新 tuple**。
>
> 可以把所有 tuple 的创建方式统一理解为：
>
> ```
>                 Data Source
>                      │
>      ┌───────────────┼────────────────┐
>      │               │                │
> Literal          Iterable        Existing Tuple
>      │               │                │
>      └───────────────┼────────────────┘
>                      │
>              Build（一次性构造）
>                      │
>                    Tuple
> ```
>
> 因此，可以把 tuple construction 分成六大类：
>
> 1. Literal（直接写出来）
> 2. Packing（打包，不带括号）
> 3. From Iterable（从可迭代对象构造）
> 4. Comprehension（用 genexpr + tuple()）
> 5. Merge / Repeat（多个 tuple 组合、重复）
> 6. Copy（已有 tuple 构造"新" tuple）

---

# 一、Tuple Literal（最直接）

## 示例1：空 tuple

```python
t = ()
```

等价于

```python
t = tuple()
```

---

## 示例2：单元素陷阱（经典坑）

```python
t = (1,)
```

结果

```
(1,)
```

**注意：**

```python
t = (1)
```

这不是 tuple，而是 `int`。因为决定 tuple 的是**逗号**，不是括号。

```
type((1))    # <class 'int'>
type((1,))   # <class 'tuple'>
```

---

## 示例3：普通 literal

```python
point = (1, 2, 3)
```

最常见。

---

## 示例4：混合类型

```python
obj = (
    1,
    "hello",
    True,
    None,
    [1, 2],
)
```

Tuple 可以存放任何对象，包括可变对象（如 list）。但 tuple 本身不可变，指的是**不能替换元素引用**，不代表元素内部不能变。

---

# 二、Tuple Packing（打包，不带括号）

Python 的一个特性：**逗号才是 tuple 的本质，括号只是可选的分组符号。**

## 示例5：不带括号打包

```python
t = 1, 2, 3
```

结果

```
(1, 2, 3)
```

---

## 示例6：函数返回多个值

```python
def minmax(nums):
    return min(nums), max(nums)

result = minmax([3, 1, 4, 1, 5])
```

结果

```
(1, 5)
```

本质上就是打包成一个 tuple 返回。

---

## 示例7：单元素打包同样需要逗号

```python
t = 1,
```

结果

```
(1,)
```

---

# 三、tuple() Constructor

tuple() 的本质：

> **把任何 Iterable 消费掉，生成一个 tuple。**

心智模型：

```
Iterable
    │
tuple()
    │
 Tuple
```

---

## 示例8：list → tuple

```python
lst = [1, 2, 3]

t = tuple(lst)
```

结果

```
(1, 2, 3)
```

---

## 示例9：set → tuple

```python
s = {3, 1, 2}

t = tuple(s)
```

注意：

set 无序，结果顺序不保证。

---

## 示例10：string → tuple

```python
tuple("hello")
```

得到

```
('h', 'e', 'l', 'l', 'o')
```

每个字符都是一个元素。

---

## 示例11：range() → tuple

```python
t = tuple(range(5))
```

得到

```
(0, 1, 2, 3, 4)
```

---

## 示例12：dict → tuple

```python
d = {
    "a": 1,
    "b": 2,
}

tuple(d)
```

得到

```
('a', 'b')
```

因为 dict 默认迭代 key。

---

## 示例13：dict.items() → tuple

```python
tuple(d.items())
```

得到

```
(('a', 1), ('b', 2))
```

一个"元组的元组"，非常适合表示不可变的键值对集合。

---

# 四、Unpacking 相关构造

## 示例14：Iterable Unpacking（Python 3）

```python
a = (1, 2)
b = (3, 4)

c = (*a, *b)
```

得到

```
(1, 2, 3, 4)
```

---

## 示例15：多重赋值中的解包打包（*rest）

```python
first, *rest = [1, 2, 3, 4]
```

结果

```
first = 1
rest = [2, 3, 4]   # 注意：rest 是 list，不是 tuple
```

---

## 示例16：*rest 构造新 tuple

```python
head, *middle, tail = (1, 2, 3, 4, 5)

new_t = (head, tuple(middle), tail)
```

结果

```
(1, (2, 3, 4), 5)
```

---

# 五、Generator → tuple（没有 Tuple Comprehension）

Python **没有**专门的 "tuple comprehension" 语法：

```python
(x*x for x in range(5))
```

这个写法得到的是一个**generator**，不是 tuple！

```
<generator object <genexpr> at 0x...>
```

想要 tuple，必须显式用 `tuple()` 包裹。

---

## 示例17：genexpr + tuple()

```python
g = (
    x*x
    for x in range(5)
)

t = tuple(g)
```

得到

```
(0, 1, 4, 9, 16)
```

---

## 示例18：直接一步到位

```python
t = tuple(x*x for x in range(5))
```

结果同上。这是最常见的"tuple comprehension 平替"写法。

---

## 示例19：带过滤条件

```python
t = tuple(
    x
    for x in range(10)
    if x % 2 == 0
)
```

得到

```
(0, 2, 4, 6, 8)
```

---

# 六、+ 合并 与 * 重复

## 示例20：+ 合并

```python
a = (1, 2)
b = (3, 4)

c = a + b
```

得到

```
(1, 2, 3, 4)
```

特点：

产生**新的** tuple。

---

## 示例21：* 重复构造

```python
zeros = (0,) * 5
```

得到

```
(0, 0, 0, 0, 0)
```

注意：`(0,)` 的逗号不能省略，否则 `(0) * 5` 结果是 `0`（int 的乘法）。

---

# 七、zip() / enumerate()：本质是 tuple 的生成器

## 示例22：zip()

```python
a = (1, 2)
b = ("A", "B")

tuple(zip(a, b))
```

得到

```
((1, 'A'), (2, 'B'))
```

zip 内部产出的每一项本身就是 tuple。

---

## 示例23：enumerate()

```python
tuple(
    enumerate(["a", "b", "c"])
)
```

得到

```
((0, 'a'), (1, 'b'), (2, 'c'))
```

---

# 八、具名元组 namedtuple（进阶构造方式）

namedtuple 本质：

> **一个既是 tuple、又可以用字段名访问的不可变对象。**

## 示例24：定义并构造

```python
from collections import namedtuple

Point = namedtuple("Point", ["x", "y"])

p = Point(1, 2)
```

结果

```
Point(x=1, y=2)
```

它本质仍然是一个 tuple：

```python
tuple(p)     # (1, 2)
p[0]         # 1
p.x          # 1
```

---

## 示例25：从 dict 构造 namedtuple

```python
d = {"x": 1, "y": 2}

p = Point(**d)
```

结果

```
Point(x=1, y=2)
```

---

# 九、Copy Construction（与 list 的关键区别）

由于 tuple 不可变，"复制"这件事和 list 完全不同：

> **对不可变对象来说，`tuple(t)` 或切片 `t[:]` 往往直接返回同一个对象，而不会真正分配新内存。**

## 示例26：tuple() 复制

```python
t = (1, 2, 3)

t2 = tuple(t)

t is t2    # True，Python 做了优化，直接复用同一个对象
```

---

## 示例27：切片复制

```python
t3 = t[:]

t is t3    # True，同样的优化
```

---

## 示例28：list 对比（划重点）

```python
lst = [1, 2, 3]

lst2 = list(lst)

lst is lst2    # False，list 每次都会创建新对象
```

**结论：** tuple 因为不可变，"copy" 更多是概念上的，Python 会做只读复用优化；list 因为可变，必须每次分配新对象，否则会共享引用带来副作用。

---

# Tuple Construction 总结（按数据来源）

| 数据来源 | 常用方法 | 是否创建新 Tuple | 典型场景 |
|-----------|----------|----------------|----------|
| Literal | `()` / `(a, b, c)` | ✓ | 已知固定元素 |
| Single Element | `(x,)` | ✓ | 单元素 tuple（易踩坑） |
| Packing | `a, b, c` | ✓ | 多返回值、隐式打包 |
| Iterable | `tuple(iterable)` | ✓ | list、set、string、range、dict |
| Unpacking | `(*a, *b)` | ✓ | 拼接多个 tuple |
| Generator | `tuple(genexpr)` | ✓ | 替代"tuple comprehension" |
| Merge | `+` | ✓ | 拼接两个 tuple |
| Repeat | `*` | ✓ | 初始化固定长度只读序列 |
| Pairing | `zip()` / `enumerate()` | ✓ | 多序列配对，元素本身即 tuple |
| Named | `namedtuple()` | ✓ | 需要字段名访问的不可变记录 |
| Copy | `tuple()`、`[:]` | 通常复用同一对象 | 只读场景下的"复制" |

---

# Tuple Construction 的统一心智模型（Golden Rule）

所有 tuple 构造方式都可以统一为一个数据流过程：

```text
                Data Source
                     │
     ┌───────────────┼──────────────────────┐
     │               │                      │
 Literal          Iterable              Existing Tuple
     │               │                      │
     └───────────────┼──────────────────────┘
                     │
              Build（一次性构造，无原地修改）
                     │
    ┌────────────────┼──────────────────────────┐
    │                │                          │
 packing        tuple(iterable)            unpacking(*)
 (a,b,c)        tuple(genexpr)             namedtuple()
 zip()          enumerate()                +  /  *
                     │
                     ▼
                 New Tuple
```

**黄金法则：**

- **已知固定元素、不会再变** → 使用 **Tuple Literal (`()`)**
- **只有一个元素** → 千万别忘记逗号 **`(x,)`**，否则不是 tuple
- **已有可迭代对象，需要变成不可变序列** → 使用 **`tuple(iterable)`**
- **需要转换或过滤** → 没有 tuple comprehension，用 **`tuple(genexpr)`** 代替
- **函数需要返回多个值** → 直接用 **Packing（`a, b, c`）**，Python 自动打包成 tuple
- **组合多个 tuple** → 使用 **`+`** 或 **Iterable Unpacking (`*`)**
- **初始化固定长度的只读序列** → 使用 **`*`**（注意单元素逗号 `(x,) * n`）
- **需要按字段名访问元素** → 使用 **`namedtuple()`**
- **"复制"一个 tuple** → 本质上是只读操作，Python 通常直接复用原对象，无需担心像 list 那样的共享引用陷阱
- **一旦构造完成就不能原地修改** → 没有 `append()`/`extend()`，任何"修改"都必须重新构造一个新 tuple
