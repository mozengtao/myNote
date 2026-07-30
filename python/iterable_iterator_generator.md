# Python 可迭代对象（Iterable）、迭代器（Iterator）、生成器（Generator）完整理解

> **一句话总结：**
>
> - **Iterable（可迭代对象）**：可以"产生迭代器"的对象（数据集合）
> - **Iterator（迭代器）**：真正负责"一个一个产生元素"的对象（取数据的人）
> - **Generator（生成器）**：一种特殊的 Iterator，由 Python 自动帮你写好的迭代器（懒加载）

可以先建立下面这张图。

```text
                    Iterable
               （可以被遍历的数据）
                       │
         iter(obj)     │
──────────────────────▶│
                       ▼
                  Iterator
             （真正负责next()）
                       │
                next(iterator)
                       ▼
          value1 → value2 → value3 ...
                       │
                  StopIteration
```

而 Generator 则是 Iterator 的一种特殊实现：

```text
Iterable
    │
    ▼
Iterator
    ▲
    │
Generator
```

所以：

> **Generator ⊂ Iterator，而 Iterator 来自 Iterable。**

---

# 一、什么是 Iterable（可迭代对象）

## 本质

Iterable 是：

> **能够产生 Iterator 的对象。**

Python 判断：

```python
iter(obj)
```

是否可以成功。

如果可以：

```
obj 是 Iterable
```

否则：

```
TypeError
```

例如：

```python
numbers = [1,2,3]

iter(numbers)
```

返回

```python
<list_iterator ...>
```

说明：

```
list
    │
    ▼
list_iterator
```

list 本身不是 Iterator。

list 只是能够制造 Iterator。

---

## 常见 Iterable

```python
list
tuple
dict
set
str
bytes
range
deque
file
generator
```

例如：

```python
for x in "hello":
    print(x)
```

实际上：

```python
it = iter("hello")

while True:
    try:
        x = next(it)
        print(x)
    except StopIteration:
        break
```

---

# 二、什么是 Iterator（迭代器）

Iterator 才是真正负责：

> **一次返回一个元素。**

必须满足两个协议：

```python
__iter__()
__next__()
```

例如：

```python
it = iter([1,2,3])

next(it)
```

输出：

```
1
```

再来：

```python
next(it)
```

输出：

```
2
```

再来：

```
3
```

最后：

```
StopIteration
```

所以 Iterator 就像：

```text
[1,2,3]

        ▲
        │
     当前游标
```

每调用一次

```
next()
```

游标向后移动。

---

## Iterator 是一次性的

例如：

```python
it = iter([1,2,3])

list(it)
```

得到

```
[1,2,3]
```

再来：

```python
list(it)
```

得到：

```
[]
```

因为：

```
已经走到末尾了。
```

---

# 三、什么是 Generator（生成器）

Generator：

> **Python 自动帮你实现好的 Iterator。**

例如：

```python
def numbers():
    yield 1
    yield 2
    yield 3
```

调用：

```python
g = numbers()
```

注意：

这里没有执行。

只是得到：

```
generator object
```

然后：

```python
next(g)
```

输出：

```
1
```

继续：

```python
next(g)
```

得到：

```
2
```

Generator 内部自己保存：

```
程序执行到哪里了
局部变量
调用栈
```

因此可以暂停恢复。

---

# Generator 为什么叫"生成器"

因为：

它不是提前把数据放进 list。

而是：

```
需要的时候
↓

计算一个

↓

返回一个

↓

继续计算
```

例如：

```python
def squares():
    n = 0
    while True:
        yield n*n
        n += 1
```

永远不会占满内存。

---

# 四、Iterable、Iterator、Generator 三者关系

## 第一层关系

```text
Iterable
    │
iter()
    ▼
Iterator
```

例如：

```python
numbers = [1,2,3]

it = iter(numbers)
```

```
list
 ↓
list_iterator
```

---

## 第二层关系

Generator 就是一种 Iterator。

例如：

```python
g = (x*x for x in range(5))
```

这里：

```
Generator
```

同时也是：

```
Iterator
```

所以：

```python
next(g)
```

合法。

---

## Generator 自己也是 Iterable

Generator：

实现了：

```python
__iter__()
```

而且：

```python
g.__iter__() is g
```

因此：

```
Generator
    │
既是 Iterator
也是 Iterable
```

因为：

Iterator 自己返回自己。

---

# 五、为什么 list 不是 Iterator

很多初学者误以为：

```
list
就是 Iterator
```

其实不是。

例如：

```python
numbers = [1,2,3]

next(numbers)
```

报错：

```
TypeError
```

因为：

```
list
没有
__next__()
```

但是：

```python
it = iter(numbers)

next(it)
```

可以。

所以：

```
list
负责保存数据

iterator
负责读取数据
```

---

# 六、为什么要区分 Iterable 和 Iterator

原因就是：

**一个数据集合可以被遍历很多次。**

例如：

```python
numbers = [1,2,3]
```

第一次：

```python
for x in numbers:
    ...
```

第二次：

```python
for x in numbers:
    ...
```

都可以。

因为：

每次都会：

```
重新创建 Iterator
```

即：

```text
list

第一次：
iter()
   ↓
iterator1

第二次：
iter()
   ↓
iterator2
```

如果 list 自己就是 Iterator：

第一次遍历结束。

第二次：

就没有数据了。

这显然不是想要的。

---

# 七、Generator 为什么只能遍历一次

例如：

```python
g = (x*x for x in range(5))
```

第一次：

```python
list(g)
```

得到：

```
[0,1,4,9,16]
```

第二次：

```python
list(g)
```

得到：

```
[]
```

因为：

Generator 本身就是 Iterator。

只有一个游标。

---

# 八、for 循环到底干了什么

例如：

```python
for x in data:
    print(x)
```

编译器大概等价：

```python
it = iter(data)

while True:
    try:
        x = next(it)
    except StopIteration:
        break

    print(x)
```

所以：

```
for

↓

iter()

↓

next()

↓

StopIteration
```

---

# 九、自己实现 Iterable

例如：

```python
class MyRange:

    def __init__(self, n):
        self.n = n

    def __iter__(self):
        return MyIterator(self.n)
```

这里：

```
MyRange

只是 Iterable
```

---

对应：

```python
class MyIterator:

    def __init__(self, n):
        self.i = 0
        self.n = n

    def __iter__(self):
        return self

    def __next__(self):
        if self.i >= self.n:
            raise StopIteration

        value = self.i
        self.i += 1
        return value
```

使用：

```python
for x in MyRange(5):
    print(x)
```

---

# 十、Generator 如何替代 Iterator

上面的 Iterator 可以写成：

```python
def my_range(n):

    for i in range(n):
        yield i
```

Python 自动生成：

```
__iter__()

__next__()

状态保存

StopIteration
```

代码减少很多。

---

# 十一、三者的心智模型（最重要）

## 心智模型一：数据仓库 vs 搬运工

```text
list
tuple
set
dict
range
```

像：

```
仓库
```

里面放着很多数据。

它们属于：

```
Iterable
```

---

Iterator：

像：

```
搬运工
```

每次搬一个。

```text
仓库
 │
 ▼
Iterator
 │
 ▼
一个元素
```

---

Generator：

像：

```
现场生产工厂
```

不是仓库。

而是：

```
需要一个

↓

生产一个

↓

交出去

↓

继续生产
```

所以：

Generator 最节省内存。

---

# 十二、三者对比总结

| 特性 | Iterable | Iterator | Generator |
|------|----------|-----------|-----------|
| 保存数据 | ✅ 是 | ❌ 否 | ❌ 否（按需生成） |
| 可用于 for | ✅ | ✅ | ✅ |
| 可调用 next() | ❌ | ✅ | ✅ |
| 可调用 iter() | ✅ | ✅（返回自己） | ✅（返回自己） |
| 是否保存当前位置 | ❌ | ✅ | ✅ |
| 是否可重复遍历 | ✅ 一般可以 | ❌ 不可以 | ❌ 不可以 |
| 是否懒加载 | 不一定 | 是 | 是 |
| 是否实现 `__iter__()` | ✅ | ✅ | ✅ |
| 是否实现 `__next__()` | ❌ | ✅ | ✅ |
| 典型对象 | list、tuple、dict、set、range、str | list_iterator、dict_keyiterator、file iterator | yield 函数、生成器表达式 |

---

# 十三、最终统一心智模型（推荐牢记）

```text
                数据集合（Iterable）
         ┌──────────────────────────────┐
         │ list tuple set dict range... │
         └──────────────┬───────────────┘
                        │
                  iter(obj)
                        │
                        ▼
               迭代器（Iterator）
         ┌──────────────────────────────┐
         │ 保存当前位置（游标）            │
         │ 每次 next() 返回一个元素       │
         └──────────────┬───────────────┘
                        │
                  next(iterator)
                        │
                        ▼
             element1 → element2 → ...
                        │
                 StopIteration

Generator（生成器）

        Generator
             │
             ├── 是 Iterator
             ├── 也是 Iterable
             └── 使用 yield 自动实现状态机

核心记忆：

Iterable = "有什么可以遍历（数据源）"
Iterator = "怎么一个一个遍历（游标）"
Generator = "边计算边遍历（懒生成）"

一句话：

    Iterable 负责提供数据；
    Iterator 负责消费数据；
    Generator 是 Python 提供的最优雅的 Iterator 实现。