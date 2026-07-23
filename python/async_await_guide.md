# Python `async` / `await` 完全指南
> **目标**
>
> 阅读完本文后，你应该能够回答下面几个问题：
>
> - async 到底是什么？
> - await 到底等待了什么？
> - coroutine、Task、Future 有什么区别？
> - 为什么 async 不会自动并发？
> - asyncio.create_task() 为什么能实现并发？
> - Python AsyncIO 的底层到底如何工作？
> - 什么场景应该使用 async？什么时候千万不要使用？

---

# 一、为什么会有 async？

假设我们需要同时下载三个网页。

普通写法：

```python
download("A")
download("B")
download("C")
```

执行过程：

```
A -----------> Done
               B -----------> Done
                              C -----------> Done
```

CPU 大部分时间其实在：

```
等待网络...
等待网络...
等待网络...
```

CPU 什么都没干。

如果等待网络期间可以去做别的事情：

```
A 等待中
        ↓
开始 B

B 等待中
        ↓
开始 C

C 等待中
        ↓
继续 A
继续 B
继续 C
```

CPU 利用率立即提高。

这就是 AsyncIO 存在的原因：

> **等待 IO 时，把 CPU 让出去。**

---

# 二、核心心智模型

一句话：

> **async = 可以暂停的函数**
>
> **await = 暂停点**

普通函数：

```
func()

开始
↓
一直执行
↓
结束
```

async 函数：

```
async func()

开始
↓
执行
↓
await
↓↓↓↓
暂停
↓↓↓↓
恢复
↓
继续执行
↓
结束
```

所以：

> async 不是多线程。

而是：

> **可暂停函数（Suspendable Function）**

---

# 三、async 定义 Coroutine

最简单例子：

```python
async def hello():
    print("Hello")
```

注意：

```
hello()
```

不会执行。

而是返回：

```
Coroutine Object
```

例如：

```python
coro = hello()

print(coro)
```

输出：

```
<coroutine object hello at 0x...>
```

这说明：

```
async function
        │
        ▼
Coroutine Object
```

真正执行需要 Event Loop。

---

# 四、第一个完整示例

```python
import asyncio

async def hello():
    print("Hello")
    await asyncio.sleep(1)
    print("World")

asyncio.run(hello())
```

输出：

```
Hello
（一秒）
World
```

执行流程：

```
run()

↓

hello()

↓

print()

↓

await sleep()

↓

暂停

↓

Event Loop

↓

1 秒到

↓

恢复

↓

print()
```

---

# 示例1：await asyncio.sleep()

```python
import asyncio

async def main():
    print("start")
    await asyncio.sleep(2)
    print("end")

asyncio.run(main())
```

sleep 不会阻塞线程。

而是：

```
main
 │
 ▼
sleep()

↓

告诉 EventLoop：

2 秒后叫醒我

↓

CPU 去干别的
```

---

# 示例2：多个 await（仍然串行）

```python
async def main():
    await asyncio.sleep(1)
    print("A")

    await asyncio.sleep(1)
    print("B")
```

时间：

```
sleep

↓

1s

↓

A

↓

sleep

↓

1s

↓

B
```

总共：

```
2 秒
```

因为：

```
await
一个一个执行
```

---

# 示例3：async 不代表并发

很多新人误区：

```python
async def work():
    await asyncio.sleep(1)

async def main():
    await work()
    await work()
```

很多人以为：

```
1 秒
```

实际上：

```
2 秒
```

因为：

```
work1

结束

↓

work2
```

**黄金法则：**

> await 一个 coroutine，本质仍然是串行。

---

# 示例4：create_task()

真正开始并发：

```python
async def work(name):
    print(name, "start")
    await asyncio.sleep(2)
    print(name, "end")

async def main():

    t1 = asyncio.create_task(work("A"))
    t2 = asyncio.create_task(work("B"))

    await t1
    await t2

asyncio.run(main())
```

输出：

```
A start
B start

2 秒后

A end
B end
```

ASCII：

```
Task A

sleep
│
│
│

Task B

sleep
│
│
│

EventLoop

轮流调度
```

---

# 示例5：asyncio.gather()

更推荐：

```python
await asyncio.gather(
    work("A"),
    work("B"),
    work("C")
)
```

时间线：

```
A─────┐
      │
B─────┼──2s
      │
C─────┘
```

所有 Coroutine 一起执行。

---

# 示例6：返回值

```python
async def calc(x):
    await asyncio.sleep(1)
    return x * 2

async def main():
    result = await calc(10)
    print(result)

asyncio.run(main())
```

输出：

```
20
```

await：

不仅等待，

还能得到：

```
return value
```

---

# 示例7：Producer / Consumer

```python
import asyncio

queue = asyncio.Queue()

async def producer():
    for i in range(5):
        await queue.put(i)

async def consumer():
    while True:
        item = await queue.get()
        print(item)
        queue.task_done()

async def main():
    asyncio.create_task(consumer())
    await producer()
    await queue.join()

asyncio.run(main())
```

这是 AsyncIO 最经典模式。

```
Producer

↓

Queue

↓

Consumer
```

---

# 示例8：异步 HTTP

```python
import aiohttp
import asyncio

async def fetch(url):
    async with aiohttp.ClientSession() as session:
        async with session.get(url) as resp:
            return await resp.text()
```

这里：

```
等待 HTTP

↓

EventLoop 去执行其它 Task
```

所以：

1000 个请求

不是：

```
1000 个线程
```

而是：

```
1 个线程
1000 个 Coroutine
```

---

# 示例9：async for

异步迭代：

```python
async for item in stream:
    print(item)
```

等价：

```
等待下一条数据

↓

数据来了

↓

继续循环
```

适合：

- websocket
- 网络流
- 文件流
- Kafka

---

# 示例10：async with

例如：

```python
async with lock:
    ...
```

本质：

```python
await lock.__aenter__()

try:
    ...
finally:
    await lock.__aexit__()
```

对应：

```
同步：

with

↓

__enter__()

↓

__exit__()
```

变成：

```
async with

↓

await __aenter__()

↓

await __aexit__()
```

---

# 五、asyncio Task 到底是什么？

Coroutine：

```
只是代码
```

Task：

```
Coroutine

+

调度状态

+

生命周期
```

关系：

```
async function

↓

Coroutine

↓

Task

↓

EventLoop
```

Task 才能：

- 被调度
- 被取消
- 查询状态

例如：

```python
task = asyncio.create_task(work())
```

---

# 六、Event Loop 工作原理

核心流程：

```
            Event Loop

         ┌──────────────┐
         │ Ready Queue  │
         └──────┬───────┘
                │
                ▼
          运行 Coroutine

                │

          遇到 await

                │

      注册 IO / Timer

                │

      Coroutine 暂停

                │

IO 完成 / Timer 到

                │

加入 Ready Queue

                │

继续执行
```

整个过程中：

线程没有切换。

只是：

```
Coroutine

暂停

恢复

暂停

恢复
```

---

# 七、Coroutine、Task、Future 的关系

```
async function

↓

Coroutine

↓

Task

↓

Future
```

理解为：

```
Coroutine

↓

等待执行

Task

↓

正在执行

Future

↓

未来结果
```

最重要的是：

```
await Future
```

最终都会等待：

```
Future 完成
```

---

# 八、什么时候使用 async？

非常适合：

✅ HTTP 请求

✅ Socket

✅ 数据库

✅ WebSocket

✅ RPC

✅ Kafka

✅ Redis

✅ 文件 IO（异步库）

共同特点：

```
大量等待 IO
```

---

# 九、什么时候不要使用 async？

例如：

```python
for i in range(100000000):
    ...
```

CPU 密集：

```
计算

计算

计算
```

没有等待。

AsyncIO 无法提升速度。

此时：

建议：

- multiprocessing
- ProcessPoolExecutor
- Cython
- Rust
- C 扩展

---

# 十、AsyncIO 黄金法则（心智模型）

## 心智模型一：Coroutine 是"可以暂停的函数"

```
普通函数

开始
↓

结束


Coroutine

开始

↓

await

↓

暂停

↓

恢复

↓

结束
```

---

## 心智模型二：await 不是"等待"

更准确地说：

```
await

=

把控制权交回 Event Loop
```

不是：

```
CPU 空等
```

而是：

```
我先停一下，
你去执行别人。
```

---

## 心智模型三：Event Loop 是调度器

```
Task A

↓

Task B

↓

Task C

↓

Task A

↓

Task C
```

不停切换。

---

## 心智模型四：Task = Coroutine + 生命周期

```
Coroutine

↓

Task

↓

Running

↓

Done
```

---

## 心智模型五：AsyncIO 本质是协作式调度（Cooperative Scheduling）

线程：

```
OS 抢占
```

AsyncIO：

```
程序主动：

await

↓

让出 CPU
```

---

# 十一、最重要的黄金法则（建议牢记）

| 黄金法则 | 说明 |
|-----------|------|
| `async` 定义的是可暂停函数，而不是线程 | 调用后返回 Coroutine 对象，不会立即执行 |
| `await` 是协作式让出控制权，而不是阻塞线程 | 当前 Coroutine 暂停，Event Loop 调度其他任务 |
| `await` 一个 Coroutine 默认仍是串行 | `await a(); await b()` 不会并发 |
| 并发需要把 Coroutine 注册为 Task | 使用 `asyncio.create_task()` 或 `asyncio.gather()` |
| Event Loop 是整个 AsyncIO 的核心 | 所有 Task 都由它统一调度 |
| Coroutine 是代码，Task 是可调度实体 | Task 封装了状态、取消、异常、结果等生命周期信息 |
| AsyncIO 的优势来自 IO 等待期间的调度 | IO 密集型收益显著，CPU 密集型几乎无收益 |
| 每个 `await` 都是一个潜在的调度点 | 设计代码时应考虑共享状态的一致性 |
| `async for` 和 `async with` 是异步协议的体现 | 对应 `__aiter__/__anext__` 与 `__aenter__/__aexit__` |
| 理解 Future → Task → Coroutine → Event Loop 的关系，是掌握 AsyncIO 的关键 | 将整个执行模型串联成完整心智图 |

---

# 十二、完整的 AsyncIO 心智架构图

```text
                    async def
                         │
                         ▼
                  Coroutine Object
                         │
          await          │ create_task()
             │           ▼
             └──────►   Task
                         │
                         ▼
                  Event Loop Scheduler
                  ┌───────────────┐
                  │ Ready Queue   │
                  │ Timer Queue   │
                  │ IO Selector   │
                  └───────┬───────┘
                          │
                调度可运行 Task
                          │
                          ▼
                Task 执行到 await
                          │
        ┌─────────────────┴─────────────────┐
        │                                   │
        ▼                                   ▼
   等待 IO 完成                       等待 Timer 到期
        │                                   │
        └─────────────────┬─────────────────┘
                          ▼
                 Task 重新进入 Ready Queue
                          │
                          ▼
                     恢复执行 (resume)
                          │
                          ▼
                     return / exception
                          │
                          ▼
                     Future 完成
```

> **一句话总结：**
>
> **`async` 定义"可暂停的函数"，`await` 定义"暂停点"，`Task` 管理 Coroutine 的生命周期，`Event Loop` 负责调度所有 Task；AsyncIO 的本质是通过协作式调度，在等待 IO 时让 CPU 去执行其他任务，从而以单线程实现高并发。**