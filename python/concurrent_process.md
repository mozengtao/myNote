# Python 并发处理（Concurrent Processing）全景总结
> **核心思想：Python 提供的是"多种并发模型"，没有一种模型适用于所有问题。**
>
> 学习 Python 并发，不要先记模块，而要先建立 **CPU、线程、事件循环、进程** 四层心智模型。

---

# 一、Python 并发整体架构

```
                    Python Concurrent Processing

                 ┌─────────────────────────────┐
                 │         主程序(Main)         │
                 └──────────────┬──────────────┘
                                │
        ┌───────────────────────┼─────────────────────────┐
        │                       │                         │
        ▼                       ▼                         ▼
   Thread(线程)           Process(进程)             Event Loop(事件循环)
 threading                multiprocessing             asyncio
 concurrent.futures       concurrent.futures

        │                       │                         │
        ▼                       ▼                         ▼
     共用内存                独立内存空间                单线程调度
     GIL限制                真正并行                   协作式并发
```

整个 Python 并发实际上可以分成四种思想：

| 模型 | 是否真正并行 | 是否共享内存 | 主要解决问题 |
|------------|-------------|------------|----------------|
| threading | × | √ | IO并发 |
| multiprocessing | √ | × | CPU计算 |
| asyncio | × | √ | 大量网络IO |
| concurrent.futures | 取决于Executor | 取决于Executor | 统一接口 |

---

# 二、Python 并发模块全家桶

```
Concurrent Processing

├── threading
│      Thread
│      Lock
│      RLock
│      Semaphore
│      Event
│      Condition
│      Barrier
│      Queue
│
├── multiprocessing
│      Process
│      Pool
│      Queue
│      Pipe
│      Lock
│      Manager
│      SharedMemory
│
├── concurrent.futures
│      ThreadPoolExecutor
│      ProcessPoolExecutor
│      Future
│
├── asyncio
│      EventLoop
│      Coroutine
│      Task
│      Future
│      Queue
│      Lock
│
└── queue
       Queue
       LifoQueue
       PriorityQueue
```

---

# 三、threading —— 线程模型

> 详细版：[concurrent_threading.md](./concurrent_threading.md)

## 心智模式

```
多个工人
共享一个仓库

        Thread1
           │
           │
Memory <───┼────> Thread2
           │
           │
        Thread3
```

线程最大的特点：

> **共享内存，切换快，但受到 GIL 限制。**

适用于：

- Socket
- HTTP
- 文件IO
- SSH
- 数据采集

---

## 示例1：创建线程

```python
import threading

def worker():
    print("working")

t = threading.Thread(target=worker)

t.start()
t.join()
```

原理：

```
Main Thread

start()

↓

OS 创建线程

↓

worker()

↓

join()

↓

等待结束
```

---

## 示例2：多个线程

```python
for i in range(5):
    threading.Thread(target=worker).start()
```

心智模型：

```
Main

 ├────Thread1
 ├────Thread2
 ├────Thread3
 ├────Thread4
 └────Thread5
```

---

## 示例3：Lock

```python
lock = threading.Lock()

with lock:
    counter += 1
```

核心思想：

```
多个线程

↓

竞争资源

↓

一次只能一个进入
```

---

## 示例4：Queue

```python
from queue import Queue

q = Queue()

q.put(10)

print(q.get())
```

Queue 是：

> **线程安全的数据流。**

---

# 四、multiprocessing —— 多进程

> 详细版：[concurrent_multiprocessing.md](./concurrent_multiprocessing.md)

## 心智模式

```
Process1

Memory A

Process2

Memory B

Process3

Memory C
```

特点：

> **每个进程拥有自己的 Python Interpreter。**

因此：

没有 GIL。

真正利用多核。

---

## 示例1：Process

```python
from multiprocessing import Process

def worker():
    print("working")

p = Process(target=worker)

p.start()
p.join()
```

---

## 示例2：Pool

```python
from multiprocessing import Pool

def square(x):
    return x*x

with Pool() as pool:
    print(pool.map(square, range(5)))
```

Pool：

```
任务

↓

任务队列

↓

多个Worker Process

↓

结果返回
```

---

## 示例3：Queue

```python
from multiprocessing import Queue

q = Queue()

q.put(100)

print(q.get())
```

这里 Queue 通过 IPC 通信。

不是共享内存。

---

# 五、concurrent.futures —— 高层统一接口

> 详细版：[concurrent_futures.md](./concurrent_futures.md)

它最大的价值：

统一 Thread 和 Process。

---

## 示例1：ThreadPoolExecutor

```python
from concurrent.futures import ThreadPoolExecutor

def task(x):
    return x*x

with ThreadPoolExecutor() as pool:
    results = pool.map(task, range(5))

print(list(results))
```

---

## 示例2：ProcessPoolExecutor

```python
from concurrent.futures import ProcessPoolExecutor

with ProcessPoolExecutor() as pool:
    results = pool.map(task, range(5))
```

唯一变化：

```
ThreadPoolExecutor

↓

共享内存


ProcessPoolExecutor

↓

多进程
```

---

## 示例3：Future

```python
future = pool.submit(task, 5)

print(future.result())
```

Future：

> **未来会得到的结果。**

心智模式：

```
submit()

↓

立即返回 Future

↓

后台执行

↓

result()

↓

阻塞等待
```

---

# 六、asyncio —— 协程模型

> 详细版：[concurrent_asyncio.md](./concurrent_asyncio.md)

这是 Python 最现代的并发方式。

## 心智模式

```
Task1

await

↓

Task2

await

↓

Task3

↓

EventLoop

↓

调度
```

注意：

**不是抢占。**

而是：

**主动让出CPU。**

---

## 示例1：async

```python
async def work():
    print("start")
```

这里只是 Coroutine。

没有运行。

---

## 示例2：await

```python
async def work():

    await asyncio.sleep(1)

    print("done")
```

await：

```
暂停当前任务

↓

EventLoop

↓

调度其它任务
```

---

## 示例3：create_task

```python
t = asyncio.create_task(work())

await t
```

Task：

就是：

Coroutine + 调度。

---

## 示例4：gather

```python
await asyncio.gather(

    work1(),

    work2(),

    work3()
)
```

效果：

```
Task1

Task2

Task3

↓

一起调度
```

---

## 示例5：Queue

```python
q = asyncio.Queue()

await q.put(1)

print(await q.get())
```

注意：

必须 await。

---

# 七、queue 模块

> 详细版：[concurrent_queue.md](./concurrent_queue.md)

线程之间最常见通信方式。

```python
from queue import Queue

q = Queue()

q.put("job")

job = q.get()

q.task_done()
```

典型：

```
Producer

↓

Queue

↓

Consumer
```

---

# 八、各种同步原语

## Lock

互斥锁

```
一个人进入
```

---

## RLock

可重入锁

```
同一线程

可以重复获得
```

---

## Semaphore

信号量

```
厕所

有3个坑位

一次最多3人
```

---

## Event

事件通知

```
Worker

等待

↓

Main

set()

↓

全部继续
```

---

## Condition

条件变量

```
等待条件满足

↓

notify()

↓

继续
```

---

## Barrier

屏障

```
Thread1

Thread2

Thread3

↓

全部到达

↓

一起继续
```

---

# 九、不同并发方案如何选择？

| 场景 | 推荐方案 | 原因 |
|--------|----------|------|
| HTTP Server | asyncio | 高并发IO |
| Web Crawling | asyncio | 数万连接 |
| SSH批量执行 | ThreadPoolExecutor | IO等待多 |
| 文件复制 | ThreadPoolExecutor | IO密集 |
| Socket服务器 | asyncio | EventLoop |
| 视频编码 | multiprocessing | CPU密集 |
| 图像处理 | ProcessPoolExecutor | 多核 |
| NumPy计算 | multiprocessing | CPU计算 |
| 数据采集 | threading | 简单易用 |
| Producer Consumer | Queue | 解耦生产消费 |

---

# 十、四种模型本质区别

| 模型 | 调度者 | 是否共享内存 | 是否真正并行 | 最适合 |
|---------|----------|--------------|--------------|------------|
| Thread | OS | √ | ×（受GIL） | IO |
| Process | OS | × | √ | CPU |
| Coroutine | EventLoop | √ | × | 大量IO |
| Future | Executor | 取决于Executor | 取决于Executor | 统一抽象 |

---

# 十一、Python 并发的统一抽象

```
                    Concurrent

                         │
      ┌──────────────────┼──────────────────┐
      │                  │                  │
      ▼                  ▼                  ▼
 Thread             Process           Coroutine
 (OS调度)           (OS调度)         (EventLoop调度)

      │                  │                  │
      └──────────────────┴──────────────────┘
                         ▼
                    执行一个 Task
                         │
                         ▼
                  返回一个 Future
                         │
                         ▼
                     获取 Result
```

可以进一步抽象为：

```
任务(Task)
      │
      ▼
调度(Scheduler)
      │
      ▼
执行(Worker)
      │
      ▼
Future
      │
      ▼
Result
```

---

# 十二、核心心智模型（黄金法则）

## ① Thread：共享内存模型

```
多个线程

共享一块内存

通过 Lock 保证安全
```

关键词：**共享（Share）**

---

## ② Process：隔离模型

```
多个进程

各自拥有独立解释器

通过 IPC 通信
```

关键词：**隔离（Isolation）**

---

## ③ Asyncio：协作模型

```
Task

主动 await

↓

EventLoop

切换其它 Task
```

关键词：**协作（Cooperation）**

---

## ④ Future：承诺模型

```
现在提交

↓

以后获得结果
```

关键词：**承诺（Promise）**

---

## ⑤ Queue：数据流模型

```
Producer

↓

Queue

↓

Consumer
```

关键词：**解耦（Decoupling）**

---

# 十三、终极统一心智模式

学习 Python 并发时，不要把 `threading`、`multiprocessing`、`asyncio`、`concurrent.futures` 看成四套毫不相关的 API，而应将它们统一理解为一个四层抽象：

```
                 并发（Concurrent Processing）
                           │
          ┌────────────────┼────────────────┐
          │                │                │
       Task（做什么）   Scheduler（谁调度） Worker（谁执行）
          │                │                │
          └────────────────┴────────────────┘
                           │
                      Result / Future
```

其中：

- **Task（任务）**：需要完成的工作，如发送 HTTP 请求、读取文件、计算数据。
- **Scheduler（调度器）**：决定任务何时运行。线程和进程由操作系统调度，协程由 Event Loop 调度。
- **Worker（执行者）**：真正执行任务的实体，可以是线程（Thread）、进程（Process）或协程（Coroutine/Task）。
- **Future（未来结果）**：任务完成前的占位符，屏蔽同步与异步执行方式，为调用者提供统一的结果获取接口。

**黄金法则：**

- **CPU 密集型 → Process（利用多核，绕过 GIL）**
- **普通 IO 密集型 → Thread（简单直接，共享内存）**
- **海量网络连接 → Asyncio（事件驱动，协作调度）**
- **希望统一编程接口 → concurrent.futures（Future + Executor）**
- **线程/协程间解耦通信 → Queue（数据流）**

因此，Python 并发真正的心智模式不是“学习四个模块”，而是理解**任务 → 调度 → 执行 → 结果**这一统一的并发执行框架，各种模块只是这一框架在不同场景下的具体实现。