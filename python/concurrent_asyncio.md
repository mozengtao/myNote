# Python asyncio 详解 —— 协程模型

> 本文是 [concurrent_process.md](./concurrent_process.md) 中"六、asyncio —— 协程模型"章节的详细展开版。
> 如果想先理解 `async`/`await` 语法本身的心智模型，可以先看 [async_mental_model.md](./async_mental_model.md) 和 [async_await_guide.md](./async_await_guide.md)；本文聚焦在 **asyncio 模块 API** 本身（EventLoop、Task、Queue、Lock 等）。

---

# 一、基本思想

`asyncio` 解决的核心问题是：**海量网络连接需要"等待"，但又不想为每个连接开一个线程（太重）。**

心智模型只有一句话：

> **单个工人（单线程），遇到要等的事就主动放下手里的活，切去做别的事；不是被抢走 CPU，而是自己主动让出。**

```
Task1 ──await──┐
               │
Task2 ──await──┼──> EventLoop（单线程调度器）── 谁的等待结束了就唤醒谁
               │
Task3 ──await──┘
```

关键特征：

| 特征 | 说明 |
|------|------|
| 内存 | 单线程，天然共享内存，不需要 Lock 保护"数据竞争"（但仍需要 Lock 保护"状态一致性"） |
| 调度者 | Event Loop（应用层调度，不是操作系统） |
| 并行性 | 不是并行，是并发——同一时刻只有一段代码在运行 |
| 切换点 | 只有 `await` 处才可能发生切换（协作式，不会被随意打断） |
| 适用规模 | 可以轻松管理成千上万个并发任务（远超线程数量上限） |

适用场景：

- HTTP Server / Web 框架（FastAPI、aiohttp）
- 大规模爬虫（数万并发连接）
- WebSocket / 长连接网关
- 数据库连接池的异步驱动（asyncpg、aioredis）

不适用场景：

- CPU 密集计算（asyncio 不会让计算变快，反而因为不能让出 CPU 而卡住整个事件循环）
- 调用的第三方库都是同步阻塞的（此时应该用 `run_in_executor` 桥接线程池，见 [concurrent_futures.md](./concurrent_futures.md)）

---

# 二、心智模式图谱

```
┌─────────────────────────────────────────────────┐
│                 asyncio 心智模式                   │
│                                                   │
│   Coroutine  : "菜谱"，定义了怎么做，但还没开始做     │
│   EventLoop  : 调度中心，决定谁在什么时候运行         │
│   Task       : Coroutine + 被 EventLoop 接管调度   │
│   await      : 主动让出控制权的唯一时机              │
│   Future     : 一个尚未有结果的异步操作占位符         │
│   Queue      : 协程之间安全传递数据的管道            │
│   Lock       : 保护协程间共享状态的一致性             │
└─────────────────────────────────────────────────┘
```

关键词：**协作（Cooperation）+ 单线程调度（Single-threaded Scheduling）**

---

# 三、典型代码示例

## 示例1：定义并运行一个协程

```python
import asyncio

async def main():
    print("hello")
    print("world")

asyncio.run(main())
```

`async def` 定义的函数调用后**不会立即执行**，而是返回一个协程对象；只有交给 `asyncio.run()`（或被 `await`）后，代码才真正开始运行。

```python
coro = main()          # 此时函数体一行都没执行
print(type(coro))      # <class 'coroutine'>
```

---

## 示例2：await 调用另一个协程

```python
import asyncio

async def fetch_data():
    print("start fetching")
    await asyncio.sleep(1)   # 模拟网络等待
    print("done fetching")
    return {"data": 123}

async def main():
    result = await fetch_data()
    print(result)

asyncio.run(main())
```

`await` 做了两件事：**暂停当前协程**、**把控制权交还给 EventLoop**，直到被等待的操作完成后再恢复执行。

---

## 示例3：create_task —— 并发运行多个协程

```python
import asyncio

async def worker(name, delay):
    await asyncio.sleep(delay)
    print(f"{name} done")
    return name

async def main():
    t1 = asyncio.create_task(worker("A", 2))
    t2 = asyncio.create_task(worker("B", 1))
    # 两个任务此时已经开始并发执行（不是串行等待）
    result1 = await t1
    result2 = await t2
    print(result1, result2)

asyncio.run(main())
# 总耗时约2秒（并发），不是3秒（串行）
```

对比串行 `await worker("A", 2); await worker("B", 1)`（总耗时3秒），`create_task` 让两个任务真正**同时**开始排队等待。

```
不用 create_task（串行）:  |--A(2s)--||--B(1s)--|   总共 3s
用 create_task（并发）:    |--A(2s)--|
                          |--B(1s)--|             总共 2s
```

---

## 示例4：gather —— 并发收集多个结果

```python
import asyncio

async def worker(i):
    await asyncio.sleep(1)
    return i * i

async def main():
    results = await asyncio.gather(
        worker(1),
        worker(2),
        worker(3),
    )
    print(results)   # [1, 4, 9]

asyncio.run(main())
```

`gather` 是 `create_task` + `await` 的语法糖，专门用于"并发跑一批协程，等全部完成后按顺序收集结果"。

---

## 示例5：gather + return_exceptions —— 部分任务失败不影响其它任务

```python
import asyncio

async def worker(i):
    if i == 2:
        raise ValueError(f"task {i} failed")
    return i

async def main():
    results = await asyncio.gather(
        worker(1), worker(2), worker(3),
        return_exceptions=True,   # 异常也会作为结果收集，不会中断其它任务
    )
    print(results)   # [1, ValueError('task 2 failed'), 3]

asyncio.run(main())
```

默认情况下（`return_exceptions=False`），只要有一个任务抛异常，`gather` 会立即抛出该异常（但其它任务仍会在后台继续跑完，只是你拿不到它们的结果）。

---

## 示例6：asyncio.Queue —— 协程间生产者消费者

```python
import asyncio

async def producer(q):
    for i in range(5):
        await q.put(i)
        print(f"produced {i}")
    await q.put(None)   # 结束信号

async def consumer(q):
    while True:
        item = await q.get()
        if item is None:
            break
        print(f"consumed {item}")

async def main():
    q = asyncio.Queue()
    await asyncio.gather(producer(q), consumer(q))

asyncio.run(main())
```

和 `queue.Queue`/`multiprocessing.Queue` 语义一致，但 `put()`/`get()` 都必须 `await`——因为队列满/空时协程需要"让出控制权"而不是阻塞整个线程。

---

## 示例7：asyncio.Lock —— 保护共享状态

```python
import asyncio

counter = 0
lock = asyncio.Lock()

async def increment():
    global counter
    async with lock:
        current = counter
        await asyncio.sleep(0)   # 模拟一次"让出"的机会
        counter = current + 1

async def main():
    await asyncio.gather(*(increment() for _ in range(100)))
    print(counter)   # 100

asyncio.run(main())
```

即使 asyncio 是单线程，**只要两次操作之间存在 `await`，就有可能被交叉执行**，从而产生竞态条件——所以在关键区间内如果有 `await`，仍然需要 `asyncio.Lock` 保护逻辑的原子性。

---

## 示例8：wait_for —— 超时控制

```python
import asyncio

async def slow_task():
    await asyncio.sleep(5)
    return "done"

async def main():
    try:
        result = await asyncio.wait_for(slow_task(), timeout=2)
        print(result)
    except asyncio.TimeoutError:
        print("task timed out after 2 seconds")

asyncio.run(main())
```

与 `Future.result(timeout=...)` 不同，`wait_for` 超时后会**真正取消**那个协程（内部调用 `task.cancel()`），而不只是停止等待。

---

## 示例9：asyncio.Event —— 事件通知

```python
import asyncio

async def waiter(event):
    print("waiting for event...")
    await event.wait()
    print("event received, continue!")

async def main():
    event = asyncio.Event()
    asyncio.create_task(waiter(event))
    await asyncio.sleep(2)
    event.set()          # 通知所有等待者
    await asyncio.sleep(0.1)   # 给 waiter 一点时间打印

asyncio.run(main())
```

语义与 `threading.Event` 一致，只是等待/通知的对象是协程而不是线程。

---

## 示例10：Semaphore —— 限制并发请求数量

```python
import asyncio

sem = asyncio.Semaphore(3)   # 最多 3 个并发

async def fetch(i):
    async with sem:
        print(f"task {i} started")
        await asyncio.sleep(1)
        print(f"task {i} finished")

async def main():
    await asyncio.gather(*(fetch(i) for i in range(10)))

asyncio.run(main())
```

典型用途：限制同时发出的 HTTP 请求数量，避免对目标服务器造成过大压力（"网络世界的连接池"）。

---

## 示例11：run_in_executor —— 混合阻塞代码

```python
import asyncio
import time

def blocking_computation():
    time.sleep(2)   # 假设这是一段无法改写成 async 的阻塞代码
    return "computation done"

async def main():
    loop = asyncio.get_running_loop()
    # 丢进默认线程池执行，不阻塞事件循环
    result = await loop.run_in_executor(None, blocking_computation)
    print(result)

asyncio.run(main())
```

**重要心智模型**：任何没有 `await` 点的耗时同步调用，只要直接写在协程里，就会**卡住整个 EventLoop**（所有其它任务都无法运行）。必须通过 `run_in_executor` 丢给线程池/进程池执行。

---

## 示例12：取消任务 —— cancel()

```python
import asyncio

async def long_task():
    try:
        await asyncio.sleep(10)
        print("finished normally")
    except asyncio.CancelledError:
        print("task was cancelled, cleaning up...")
        raise   # 通常应该重新抛出，让取消行为正确传播

async def main():
    task = asyncio.create_task(long_task())
    await asyncio.sleep(1)
    task.cancel()
    try:
        await task
    except asyncio.CancelledError:
        print("main: confirmed task cancellation")

asyncio.run(main())
```

`cancel()` 会在目标协程的下一个 `await` 处注入 `CancelledError`——所以取消不是"立即生效"，而是"在下一次让出控制权的地方生效"。

---

## 示例13：TaskGroup —— 结构化并发（Python 3.11+）

```python
import asyncio

async def worker(i):
    if i == 2:
        raise ValueError(f"worker {i} failed")
    await asyncio.sleep(1)
    return i

async def main():
    try:
        async with asyncio.TaskGroup() as tg:
            tasks = [tg.create_task(worker(i)) for i in range(4)]
        print([t.result() for t in tasks])
    except* ValueError as eg:
        print("caught exceptions:", eg.exceptions)

asyncio.run(main())
```

`TaskGroup` 相比 `gather` 的优势：**只要组内任意任务失败，会自动取消组内其它所有任务**，避免"孤儿任务"在后台悄悄继续运行，是官方推荐的"结构化并发"写法。

---

## 示例14：async for + async 生成器 —— 异步迭代

```python
import asyncio

async def async_range(n):
    for i in range(n):
        await asyncio.sleep(0.1)
        yield i

async def main():
    async for i in async_range(5):
        print(f"got {i}")

asyncio.run(main())
```

`async for` 用于遍历"异步生成器"，每次取下一个值时都可能需要等待（如从网络流式读取数据），是 `asyncio.Queue`、数据库游标等常见的迭代方式。

---

# 四、常见陷阱

| 陷阱 | 说明 |
|------|------|
| 忘记 `await` | 协程对象被创建但从未真正执行，代码"看起来跑了但什么都没发生" |
| 在协程里写同步阻塞代码 | 会卡住整个 EventLoop，所有并发任务全部停滞 |
| 用 `asyncio` 做 CPU 计算 | 不会变快，反而因为独占事件循环而变慢；应该用 `run_in_executor` 桥接进程池 |
| 创建 `create_task` 后不保存引用 | Task 可能被垃圾回收提前终止（务必持有引用或加入集合） |
| 混用 `asyncio.run()` 与已有事件循环 | 会抛出 `RuntimeError: asyncio.run() cannot be called from a running event loop` |

---

# 五、一句话总结

> **asyncio 是"协作模型"：单线程 + EventLoop 调度，代码只在 `await` 处主动让出控制权；它用极低的切换成本支撑海量并发连接，但要求所有耗时操作都必须是"可等待的"，否则会卡死整个事件循环。**
