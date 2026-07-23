# Python queue 详解 —— 数据流模型

> 本文是 [concurrent_process.md](./concurrent_process.md) 中"七、queue 模块"章节的详细展开版。

---

# 一、基本思想

`queue` 模块解决的核心问题是：**多个执行单元（线程/进程/协程）之间需要安全地传递数据，同时把"生产"和"消费"解耦。**

心智模型只有一句话：

> **Producer 只管往队列里放东西，Consumer 只管从队列里取东西，两者互不知道对方的存在。**

```
Producer1 ─┐                      ┌─> Consumer1
Producer2 ─┼──> [ Queue 队列 ] ───┼─> Consumer2
Producer3 ─┘                      └─> Consumer3
```

关键特征：

| 特征 | 说明 |
|------|------|
| 线程安全 | 内部已经用 Lock/Condition 实现好了同步逻辑，使用者不需要手动加锁 |
| 解耦 | 生产者和消费者数量可以任意组合（1对1、1对多、多对多），互不感知 |
| 阻塞语义 | `put()`/`get()` 默认阻塞；队列满/空时自动等待 |
| 背压（Backpressure） | 设置 `maxsize` 后，生产者过快会被自动阻塞，防止内存爆炸 |

`queue` 模块本身提供三种队列类型：

| 类型 | 出队顺序 |
|------|----------|
| `Queue` | FIFO（先进先出） |
| `LifoQueue` | LIFO（后进先出，类似栈） |
| `PriorityQueue` | 按优先级出队（最小值先出） |

---

# 二、心智模式图谱

```
┌─────────────────────────────────────────────────┐
│                  queue 心智模式                    │
│                                                   │
│   put()       : 放一个任务/数据进队列               │
│   get()       : 取一个任务/数据（没有就阻塞等待）     │
│   task_done() : 告诉队列"这个任务我处理完了"         │
│   join()      : 阻塞直到所有任务都被 task_done()    │
│   maxsize     : 队列容量上限，用来控制生产速度        │
└─────────────────────────────────────────────────┘
```

关键词：**解耦（Decoupling）+ 缓冲（Buffering）**

---

# 三、典型代码示例

## 示例1：Queue 基本用法 —— FIFO put/get

```python
from queue import Queue

q = Queue()

q.put(1)
q.put(2)
q.put(3)

print(q.get())   # 1
print(q.get())   # 2
print(q.get())   # 3
```

先进先出：谁先 `put`，谁先被 `get` 出来。

---

## 示例2：完整的多线程生产者消费者模型

```python
import threading
import queue
import time

q = queue.Queue(maxsize=5)

def producer():
    for i in range(10):
        q.put(i)
        print(f"produced {i}")
        time.sleep(0.1)
    q.put(None)   # 用 None 表示"结束"信号

def consumer():
    while True:
        item = q.get()
        if item is None:
            break
        print(f"consumed {item}")
        time.sleep(0.2)

t1 = threading.Thread(target=producer)
t2 = threading.Thread(target=consumer)
t1.start(); t2.start()
t1.join(); t2.join()
```

心智模型：

```
Producer 生产快 (0.1s/个) ──> Queue(容量5) ──> Consumer 消费慢 (0.2s/个)
                                  │
                          队列满了自动阻塞 Producer
                          （天然的"背压"机制）
```

---

## 示例3：task_done() / join() —— 等待所有任务真正被处理完

```python
import threading
import queue

q = queue.Queue()

def worker():
    while True:
        item = q.get()
        print(f"processing {item}")
        q.task_done()   # 告诉队列：这个任务处理完了

for i in range(5):
    q.put(i)

t = threading.Thread(target=worker, daemon=True)
t.start()

q.join()   # 阻塞，直到队列中所有任务都被 task_done() 确认
print("all tasks done")
```

**注意区分**：`q.empty()` 只表示"队列里没有元素了"，但不代表消费者已经把取出来的最后一个元素**处理完**；`q.join()` 才是"等待所有已 put 的任务都被 task_done() 确认完成"的正确姿势。

```
put 5个任务 ──> get 5次（队列已空，但任务可能还在处理中）──> task_done 5次 ──> join() 返回
```

---

## 示例4：多个消费者（worker pool 模式）

```python
import threading
import queue

q = queue.Queue()

def worker(worker_id):
    while True:
        item = q.get()
        if item is None:
            q.put(None)   # 把结束信号传递给下一个 worker，然后自己退出
            break
        print(f"worker-{worker_id} processing {item}")
        q.task_done()

for i in range(20):
    q.put(i)

workers = [threading.Thread(target=worker, args=(i,)) for i in range(4)]
for w in workers:
    w.start()

q.join()
q.put(None)   # 触发第一个 worker 退出（会级联传递给其它 worker）
for w in workers:
    w.join()
```

心智模型：

```
                    ┌──> Worker1
Queue(20个任务) ─────┼──> Worker2   （4个worker并发从同一个队列取任务）
                    ├──> Worker3
                    └──> Worker4
```

这是最常见的"任务池"模式：任意数量的 worker 共同消费同一个队列，天然实现负载均衡（谁先处理完就先取下一个）。

---

## 示例5：LifoQueue —— 后进先出（栈语义）

```python
from queue import LifoQueue

q = LifoQueue()

q.put(1)
q.put(2)
q.put(3)

print(q.get())   # 3 —— 最后放入的先取出
print(q.get())   # 2
print(q.get())   # 1
```

典型用途：深度优先遍历（DFS）、"撤销栈"、需要优先处理最新任务的场景。

---

## 示例6：PriorityQueue —— 按优先级出队

```python
from queue import PriorityQueue

q = PriorityQueue()

q.put((3, "low priority task"))
q.put((1, "high priority task"))
q.put((2, "medium priority task"))

while not q.empty():
    priority, task = q.get()
    print(f"[{priority}] {task}")

# 输出顺序：
# [1] high priority task
# [2] medium priority task
# [3] low priority task
```

`PriorityQueue` 内部用堆（heapq）实现，放入的元素必须可比较（通常用 `(priority, data)` 元组，数字越小优先级越高）。

> 提示：如果两个任务的 `priority` 相同，而 `data` 又不可比较（如 dict），会报 `TypeError`。常见解决办法是加一个自增序号做 tie-breaker：`(priority, seq, data)`。

---

## 示例7：put(block=False) —— 队列满时不阻塞，直接处理异常

```python
from queue import Queue, Full

q = Queue(maxsize=2)
q.put(1)
q.put(2)

try:
    q.put(3, block=False)   # 队列已满，不等待，直接抛异常
except Full:
    print("queue is full, drop this task")
```

适合"宁可丢弃任务，也不要阻塞生产者"的场景（如限流、降级）。

---

## 示例8：get(timeout=...) —— 队列空时限时等待

```python
from queue import Queue, Empty

q = Queue()

try:
    item = q.get(timeout=2)   # 最多等待2秒
except Empty:
    print("no task available within 2 seconds")
```

适合"消费者需要定期检查其它退出条件"的场景，避免永久阻塞在 `get()` 上。

---

## 示例9：SimpleQueue —— 更轻量的 FIFO 队列（Python 3.7+）

```python
from queue import SimpleQueue

q = SimpleQueue()

q.put("task-1")
q.put("task-2")

print(q.get())   # task-1
print(q.get())   # task-2
print(q.empty())  # False -> True after both gets
```

`SimpleQueue` 是 `Queue` 的精简版：不支持 `maxsize`、`task_done()`/`join()`，但性能更好，且保证 `put()` 永不阻塞——适合只需要"简单传递数据"、不需要背压/任务追踪的场景。

---

## 示例10：multiprocessing.Queue 对比 —— 跨进程版本

```python
from multiprocessing import Process, Queue as MPQueue

def worker(q):
    q.put("hello from child process")

if __name__ == "__main__":
    q = MPQueue()
    p = Process(target=worker, args=(q,))
    p.start()
    print(q.get())
    p.join()
```

对比：

| | `queue.Queue` | `multiprocessing.Queue` |
|---|---|---|
| 通信双方 | 同进程内的线程 | 不同进程 |
| 底层实现 | 内存 + Lock/Condition | 管道 + pickle 序列化 |
| 数据要求 | 任意 Python 对象 | 必须可被 pickle 序列化 |
| 速度 | 快（无需序列化） | 慢（需要跨进程传输+序列化） |

详见 [concurrent_multiprocessing.md](./concurrent_multiprocessing.md)。

---

## 示例11：asyncio.Queue 对比 —— 协程版本

```python
import asyncio

async def producer(q):
    for i in range(3):
        await q.put(i)

async def consumer(q):
    for _ in range(3):
        item = await q.get()
        print(f"consumed {item}")

async def main():
    q = asyncio.Queue()
    await asyncio.gather(producer(q), consumer(q))

asyncio.run(main())
```

对比：

| | `queue.Queue` | `asyncio.Queue` |
|---|---|---|
| 使用方 | 线程 | 协程 |
| `put`/`get` | 同步阻塞调用 | 必须 `await` |
| 满/空时 | 阻塞整个线程 | 挂起当前协程，让出给 EventLoop |

详见 [concurrent_asyncio.md](./concurrent_asyncio.md)。

---

## 示例12：三种 Queue 对比小结

```python
from queue import Queue
from multiprocessing import Queue as MPQueue
import asyncio

# 1. 线程之间通信
thread_q = Queue()

# 2. 进程之间通信
process_q = MPQueue()

# 3. 协程之间通信（必须在 async 函数里创建/使用）
async def demo():
    coroutine_q = asyncio.Queue()
    await coroutine_q.put(1)
    print(await coroutine_q.get())
```

```
┌───────────────┬───────────────┬───────────────────┐
│ queue.Queue   │ multiprocessing.Queue │ asyncio.Queue │
├───────────────┼───────────────┼───────────────────┤
│ 线程间通信     │ 进程间通信      │ 协程间通信          │
│ 同步阻塞       │ 同步阻塞+序列化 │ 异步 await          │
│ 共享内存       │ IPC 管道       │ 单线程共享内存        │
└───────────────┴───────────────┴───────────────────┘
```

选择哪一种，取决于你的并发单位是线程、进程，还是协程——**接口名字相同（`put`/`get`），但底层通信机制完全不同**，不能混用（比如不能把 `asyncio.Queue` 传给普通线程使用）。

---

# 四、常见陷阱

| 陷阱 | 说明 |
|------|------|
| 用 `empty()` 判断任务是否处理完 | 应该用 `task_done()` + `join()`，`empty()` 只反映队列元素数量 |
| 忘记设置 `maxsize` | 生产者远快于消费者时可能导致内存无限增长 |
| 混用不同 Queue 类型 | `asyncio.Queue` 不能被普通线程调用（没有 `await` 语法支持） |
| PriorityQueue 元素不可比较 | 需要用元组 `(priority, seq, data)` 避免 `TypeError` |
| 结束信号（`None`）设计不严谨 | 多消费者场景需要考虑"哨兵值"如何传递给所有 worker（示例4的级联传递技巧） |

---

# 五、一句话总结

> **queue 是"数据流模型"：Producer 只管生产、Consumer 只管消费，Queue 负责在中间做缓冲、限流和线程/进程安全保证；线程用 `queue.Queue`，进程用 `multiprocessing.Queue`，协程用 `asyncio.Queue`——接口相似，但要按照你的并发单位选择对应的实现。**
