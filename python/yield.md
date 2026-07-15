# Python `yield` / `yield from` 完全心智模型
## —— 从 Generator、Iterator 到 Python Data Model

> **一句话总结**
>
> `yield` 的本质不是"返回多个值"，而是**暂停函数（Suspend Function），保存整个执行现场（Execution State），以后从暂停点继续执行**。
>
> Generator 本质上就是 **Python 自动帮你实现的 Iterator**。

---

# 一、先建立整体心智模型（最重要）

理解 `yield` 最好的方式，不是把它当作一种语法，而是把它看成 Python Data Model 中的一种对象。

```
                      Generator Function
                               │
                           调用函数
                               │
                               ▼
                      Generator Object
                               │
             自动实现 Iterator Protocol
                               │
         ┌─────────────────────┴──────────────────────┐
         │                                            │
      __iter__()                               __next__()
         │                                            │
         └─────────────────────┬──────────────────────┘
                               ▼
                     每次恢复到 yield
                               │
                          产生一个对象
                               │
                          再次暂停执行
```

因此：

```
yield
    │
    ▼
Generator
    │
    ▼
Iterator
    │
    ▼
Python Data Model
```

也就是说：

> **yield 是 Iterator Protocol 的一种语言级实现。**

---

# 二、普通函数 VS Generator

## 普通函数

```python
def add():
    print("A")
    print("B")
    return 100
```

执行：

```
add()

↓

print A

↓

print B

↓

return

↓

Frame Destroyed
```

执行流程：

```
Caller

   │
   ▼

Function

   │

执行到底

   │

return

   │

Frame 消失
```

所有局部变量全部销毁。

---

## Generator

```python
def counter():

    print("A")

    yield 1

    print("B")

    yield 2

    print("C")
```

执行流程：

```
Caller

   │
   ▼

Generator

   │

执行

   │

yield

   │

暂停

──────────────

Frame 保留

──────────────

next()

继续

──────────────

yield

暂停

──────────────

next()

结束
```

区别：

```
return

↓

Destroy Frame

------------------------

yield

↓

Freeze Frame
```

---

# 三、经典示例一：yield 的本质

```python
def counter():

    print("Start")

    x = 100

    yield x

    print("Resume")

    x += 1

    yield x

    print("Finish")


g = counter()
```

此时：

```
什么都没有执行
```

Generator 是 Lazy。

---

第一次：

```python
next(g)
```

执行：

```
Start

↓

x = 100

↓

yield x
```

返回：

```
100
```

但是：

```
Frame

没有销毁
```

保存内容：

```
Generator Frame

────────────────────

Local Variables

x =100

────────────────────

Instruction Pointer

↓

yield x

────────────────────

Call Stack

────────────────────
```

整个执行现场被冻结。

---

第二次：

```python
next(g)
```

恢复：

```
Resume

↓

x +=1

↓

yield
```

返回：

```
101
```

Frame 再次冻结。

---

第三次：

```
next(g)
```

恢复：

```
Finish

↓

StopIteration

↓

Frame Destroyed
```

---

# 四、yield 到底保存了什么？

Generator 保存的是整个执行状态（Execution State）。

```
Generator Frame

────────────────────────────

① 局部变量(Local Variables)

x =100

name="Alice"

items=[...]

────────────────────────────

② 程序计数器(Instruction Pointer)

执行到了哪一行？

↓

yield x

────────────────────────────

③ 调用栈(Frame)

函数调用状态

────────────────────────────

④ 异常处理状态

try

except

finally

────────────────────────────
```

一句话：

> **yield 保存的是整个 Frame（栈帧），而不仅仅是变量。**

---

# 五、为什么 Generator 能 for？

来看：

```python
def numbers():

    yield 1

    yield 2

    yield 3
```

Python 自动生成：

```
Generator Object

↓

__iter__()

↓

return self

↓

__next__()
```

所以：

```
for x in numbers()
```

实际上：

```
iterator = iter(numbers())

↓

next()

↓

yield 1

↓

next()

↓

yield 2

↓

next()

↓

yield 3

↓

StopIteration
```

即：

```
yield

↓

Generator

↓

Iterator

↓

for
```

---

# 六、Generator 和 Iterator 的关系

很多人容易混淆：

```
Iterator

≠

Generator
```

实际上：

```
Iterator

只是协议

(__iter__ + __next__)
```

例如：

```python
class Counter:

    def __init__(self):
        self.n = 0

    def __iter__(self):
        return self

    def __next__(self):

        self.n += 1

        if self.n > 5:
            raise StopIteration

        return self.n
```

这是一个 Iterator。

完全没有 yield。

---

Generator：

```python
def counter():

    for i in range(1,6):

        yield i
```

Python 自动生成：

```
Iterator

+

状态保存

+

恢复执行
```

所以：

```
Generator

=

Iterator

+

Suspend/Resume
```

---

# 七、yield 的经典应用：流式处理（Streaming）

不用 Generator：

```python
def read_log():

    lines=[]

    with open("server.log") as f:

        for line in f:

            lines.append(line)

    return lines
```

问题：

```
整个文件

全部进入内存
```

```
2GB

↓

2GB RAM
```

---

Generator：

```python
def read_log():

    with open("server.log") as f:

        for line in f:

            yield line
```

执行：

```
磁盘

↓

读取一行

↓

yield

↓

消费者处理

↓

继续读下一行
```

内存：

```
永远只有一行
```

这就是：

```
Streaming

Lazy Evaluation
```

---

# 八、经典示例二：yield from

来看两个 Generator。

```python
def cpu_logs():

    yield "cpu:20%"

    yield "cpu:30%"
```

```python
def mem_logs():

    yield "mem:60%"

    yield "mem:70%"
```

不用 yield from：

```python
def all_logs():

    for item in cpu_logs():

        yield item

    for item in mem_logs():

        yield item
```

执行：

```
cpu_logs

↓

yield

↓

Caller

↓

继续

↓

mem_logs
```

---

更推荐：

```python
def all_logs():

    yield from cpu_logs()

    yield from mem_logs()
```

效果完全一样。

但是：

```
yield from

自动代理整个 Generator
```

---

执行流程：

```
Caller

   │

   ▼

all_logs

   │

yield from

   │

   ▼

cpu_logs()

   │

yield

   │

Caller

──────────────────

结束

──────────────────

yield from

   │

   ▼

mem_logs()

   │

yield

   │

Caller
```

---

# 九、yield from 的真正作用

很多人理解成：

```
yield from

=

for

yield
```

实际上：

远不止如此。

它还能：

```
自动转发：

next()

send()

throw()

close()
```

所以：

```
yield from

不是语法糖

而是

Generator Delegation
```

即：

```
Generator A

↓

yield from

↓

Generator B

↓

Caller
```

Caller 根本不知道：

```
中间还有一层 Generator。
```

---

# 十、经典应用：Generator Pipeline

例如：

日志分析：

```
server.log

↓

读取

↓

过滤 ERROR

↓

解析

↓

统计
```

代码：

```python
def read():

    for line in open("server.log"):

        yield line


def filter_error(lines):

    for line in lines:

        if "ERROR" in line:

            yield line


def parse(lines):

    for line in lines:

        yield line.split()
```

执行：

```python
pipeline = parse(
                filter_error(
                    read()
                ))

for record in pipeline:

    print(record)
```

数据流：

```
Disk

↓

Generator

↓

Generator

↓

Generator

↓

Consumer
```

整个过程：

```
Lazy

Streaming

Memory Efficient
```

这是 Python Generator 最经典的应用方式。

---

# 十一、Generator 在 Python Data Model 中的位置

```
                Python Data Model

                         │

               Iterator Protocol

         (__iter__ + __next__)

                         ▲

                         │

                 Generator Object

                         ▲

                         │

                 yield / yield from

                         ▲

                         │

              Generator Function
```

即：

```
yield

↓

Generator

↓

Iterator

↓

Protocol

↓

Python Language
```

因此：

```
for

list()

tuple()

sum()

max()

min()

zip()

enumerate()

all()

any()
```

全部支持 Generator。

---

# 十二、yield 与 Python Protocol 的关系

来看：

```
for x in generator
```

实际上：

```
iter(generator)

↓

generator.__iter__()

↓

generator

↓

next(generator)

↓

generator.__next__()

↓

恢复到 yield

↓

返回对象

↓

暂停
```

因此：

```
yield

并没有参与

for

循环。

真正参与的是：

Generator 实现的

Iterator Protocol。
```

---

# 十三、yield 与 yield from 的对比

| 特性 | yield | yield from |
|------|--------|------------|
| 返回一个值 | ✅ | 来自子 Generator |
| 暂停当前 Generator | ✅ | ✅ |
| 保存当前 Frame | ✅ | ✅ |
| 委托另一个 Generator | ❌ | ✅ |
| 自动转发 `next()` | ❌ | ✅ |
| 自动转发 `send()` | ❌ | ✅ |
| 自动转发 `throw()` | ❌ | ✅ |
| 自动转发 `close()` | ❌ | ✅ |
| 典型场景 | 逐个产生对象 | Generator 组合、Pipeline |

---

# 十四、最终心智模型（建议牢记）

```
                     Generator Function
                              │
                           调用函数
                              │
                              ▼
                     Generator Object
                              │
               自动实现 Iterator Protocol
                              │
             ┌────────────────┴────────────────┐
             │                                 │
        __iter__()                       __next__()
             │                                 │
             └────────────────┬────────────────┘
                              ▼
                       恢复执行(Frame)
                              │
                        执行到 yield
                              │
                              ▼
                         返回一个对象
                              │
                              ▼
                       保存整个执行现场
                              │
                              ▼
                         等待 next()
                              │
                              ▼
                          继续执行
```

进一步抽象到 Python Data Model：

```
                 Python Object
                       │
          实现 Iterator Protocol
                       │
            (__iter__ + __next__)
                       │
                       ▼
               Generator Object
                       ▲
                       │
            yield / yield from
                       ▲
                       │
             Generator Function
                       │
                       ▼
             Lazy Evaluation（惰性求值）
             Streaming（流式处理）
             Suspend / Resume（暂停 / 恢复）
             Pipeline（流水线）
```

> **一句话总结：**
>
> - **`yield`**：把普通函数变成一个**可暂停、可恢复**的 Generator，Python 自动将其实现为 Iterator。
> - **`yield from`**：把多个 Generator 连接起来，实现 **Generator Delegation（生成器委托）**，是构建流式 Pipeline 的关键机制。
> - **Python Data Model**：Generator 并不是一种特殊容器，而是 **Iterator Protocol（`__iter__` + `__next__`）** 的语言级实现，其核心价值是将程序从“一次性计算”转变为“按需生产对象”的流式执行模型。