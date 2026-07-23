# Python threading 详解 —— 线程模型

> 本文是 [concurrent_process.md](./concurrent_process.md) 中"三、threading —— 线程模型"章节的详细展开版。

---

# 一、基本思想

`threading` 解决的核心问题是：**多个任务需要"同时"进行，但它们大部分时间在等待（IO），而不是在计算。**

心智模型只有一句话：

> **多个工人（Thread），共用一个仓库（Memory）。**

```
                Memory (共享内存)
                       │
        ┌──────────────┼──────────────┐
        │              │              │
        ▼              ▼              ▼
     Thread1        Thread2        Thread3
```

关键特征：

| 特征 | 说明 |
|------|------|
| 内存 | 所有线程共享同一份内存空间（变量、对象直接可见） |
| 调度者 | 操作系统（抢占式调度，随时可能切换） |
| 并行性 | 受 GIL（全局解释器锁）限制，同一时刻只有一个线程执行 Python 字节码 |
| 切换开销 | 比进程小，比协程大 |
| 安全性 | 共享内存 = 需要 Lock/RLock/Semaphore 等同步原语，否则出现竞态条件 |

适用场景：

- 网络请求（requests/urllib）
- 文件 IO
- SSH / Socket 批量操作
- 需要"等待"的任务，且不想引入 asyncio 的复杂度

不适用场景：

- 纯 CPU 计算（矩阵运算、图像处理）——GIL 会让多线程反而变慢，应使用 `multiprocessing`

---

# 二、心智模式图谱

```
┌─────────────────────────────────────────────────┐
│                  threading 心智模式                │
│                                                   │
│   Thread      : 谁去执行任务                       │
│   Lock/RLock  : 谁能进入共享资源                    │
│   Semaphore   : 最多几个人能同时进入                 │
│   Event       : 通知所有人"可以继续了"               │
│   Condition   : 等待某个条件成立再继续               │
│   Barrier     : 等所有人都到齐再一起出发              │
│   Queue       : 线程之间安全传递数据的管道            │
└─────────────────────────────────────────────────┘
```

关键词：**共享（Share）+ 同步（Synchronize）**

---

# 三、典型代码示例

## 示例1：创建并启动一个线程

```python
import threading

def worker():
    print("thread is working")

t = threading.Thread(target=worker)
t.start()   # 启动线程，立即返回，不阻塞主线程
t.join()    # 阻塞主线程，直到 t 执行完毕
```

原理：

```
Main Thread
   │
   ├── t.start() ──> OS 创建线程 ──> worker() 开始并发执行
   │
   └── t.join() ──> 阻塞等待 worker() 结束
```

---

## 示例2：给线程传递参数

```python
import threading

def worker(name, delay):
    print(f"{name} start")
    print(f"{name} done after {delay}s")

t = threading.Thread(target=worker, args=("task-1",), kwargs={"delay": 2})
t.start()
t.join()
```

`args` 是位置参数元组，`kwargs` 是关键字参数字典，和普通函数调用完全一致。

---

## 示例3：批量创建多个线程

```python
import threading

def worker(i):
    print(f"worker {i} running")

threads = []
for i in range(5):
    t = threading.Thread(target=worker, args=(i,))
    threads.append(t)
    t.start()

for t in threads:
    t.join()   # 等待所有线程结束
```

心智模型：

```
Main
 ├── Thread1
 ├── Thread2
 ├── Thread3
 ├── Thread4
 └── Thread5
        │
        ▼
   全部 join() 后 Main 才继续
```

---

## 示例4：继承 Thread 类自定义线程

```python
import threading

class DownloadThread(threading.Thread):
    def __init__(self, url):
        super().__init__()
        self.url = url
        self.result = None

    def run(self):
        # run() 是线程真正执行的入口，start() 会自动调用它
        self.result = f"downloaded: {self.url}"

t = DownloadThread("http://example.com")
t.start()
t.join()
print(t.result)
```

适合封装"有状态"的线程任务（需要保存执行结果）。

---

## 示例5：daemon 线程（守护线程）

```python
import threading
import time

def background_task():
    while True:
        time.sleep(1)

t = threading.Thread(target=background_task, daemon=True)
t.start()

print("main thread exits, daemon thread will be killed automatically")
# 主线程结束时，daemon 线程会被强制终止，不会阻止程序退出
```

对比：

```
daemon=False（默认）        daemon=True
主线程等子线程结束            主线程退出 → 子线程被强制杀死
```

---

## 示例6：Lock —— 互斥锁解决竞态条件

```python
import threading

counter = 0
lock = threading.Lock()

def increment():
    global counter
    for _ in range(100000):
        with lock:      # 等价于 lock.acquire() / lock.release()
            counter += 1

threads = [threading.Thread(target=increment) for _ in range(4)]
for t in threads:
    t.start()
for t in threads:
    t.join()

print(counter)   # 有 Lock 保护，结果稳定为 400000
```

没有 Lock 会发生什么：

```
Thread A: 读取 counter=10
Thread B: 读取 counter=10      ← 同时读到了旧值
Thread A: counter = 10+1 = 11
Thread B: counter = 10+1 = 11  ← 少加了一次！
```

---

## 示例7：RLock —— 可重入锁

```python
import threading

lock = threading.RLock()

def outer():
    with lock:
        inner()   # 同一线程可以再次获取同一把锁，不会死锁

def inner():
    with lock:
        print("inner acquired the same RLock again")

outer()
```

`Lock` 在同一线程内重复 `acquire()` 会死锁；`RLock` 记录"持有者+计数"，同一线程可重复获取。

```
普通 Lock：      同线程二次 acquire() ──> 死锁
RLock：         同线程二次 acquire() ──> 计数+1，正常通过
```

---

## 示例8：Semaphore —— 限流信号量

```python
import threading
import time

sem = threading.Semaphore(3)   # 最多允许 3 个线程同时进入

def access_resource(i):
    with sem:
        print(f"worker {i} entered")
        time.sleep(1)
        print(f"worker {i} left")

for i in range(10):
    threading.Thread(target=access_resource, args=(i,)).start()
```

心智模型：

```
厕所只有 3 个坑位
        ┌───┬───┬───┐
worker→ │ 1 │ 2 │ 3 │  其余 worker 排队等待
        └───┴───┴───┘
```

典型用途：限制并发请求数、连接池大小。

---

## 示例9：Event —— 事件通知

```python
import threading
import time

event = threading.Event()

def waiter():
    print("waiting for event...")
    event.wait()          # 阻塞，直到 event.set() 被调用
    print("event received, continue!")

t = threading.Thread(target=waiter)
t.start()

time.sleep(2)
event.set()               # 通知所有等待者
t.join()
```

流程：

```
Worker: event.wait() ──(阻塞)──┐
                              │
Main:   event.set()  ─────────┴──> Worker 被唤醒继续执行
```

典型用途：主线程等待"初始化完成"信号、多个线程等待"开始"信号。

---

## 示例10：Condition —— 条件变量（生产者消费者）

```python
import threading
import collections

condition = threading.Condition()
buffer = collections.deque(maxlen=5)

def producer():
    for i in range(10):
        with condition:
            buffer.append(i)
            print(f"produced {i}")
            condition.notify()   # 通知一个等待中的消费者

def consumer():
    for _ in range(10):
        with condition:
            while not buffer:
                condition.wait()  # 缓冲区为空，释放锁并等待
            item = buffer.popleft()
            print(f"consumed {item}")

threading.Thread(target=producer).start()
threading.Thread(target=consumer).start()
```

心智模型：

```
消费者: while 条件不满足: condition.wait()  # 挂起并释放锁
生产者: 修改条件 → condition.notify()        # 唤醒消费者重新检查条件
```

---

## 示例11：Barrier —— 屏障同步

```python
import threading
import time

barrier = threading.Barrier(3)   # 等 3 个线程都到达

def worker(i):
    print(f"worker {i} preparing")
    time.sleep(i)
    barrier.wait()   # 阻塞，直到所有 3 个线程都调用了 wait()
    print(f"worker {i} started together!")

for i in range(3):
    threading.Thread(target=worker, args=(i,)).start()
```

心智模型：

```
Thread1 ──到达──┐
Thread2 ──到达──┼── 全部到达 ──> 一起继续
Thread3 ──到达──┘
```

典型用途：多线程压测时统一起跑时间点、分阶段并行计算的同步点。

---

## 示例12：threading.local() —— 线程本地数据

```python
import threading

local_data = threading.local()

def worker(value):
    local_data.value = value    # 每个线程写入的是自己独立的一份
    print(f"{threading.current_thread().name}: {local_data.value}")

for i in range(3):
    threading.Thread(target=worker, args=(i,)).start()
```

即使 `local_data` 是同一个对象，每个线程读写到的 `.value` 互不干扰：

```
Thread1.local_data.value = 0        Thread2.local_data.value = 1
      （物理上是同一个 local_data，但逻辑上互相隔离）
```

典型用途：数据库连接、请求上下文（每个线程一份，不需要加锁）。

---

## 示例13：queue.Queue —— 线程安全队列实现生产者消费者

```python
import threading
import queue
import time

q = queue.Queue(maxsize=5)

def producer():
    for i in range(10):
        q.put(i)             # 队列满时自动阻塞
        print(f"produced {i}")
    q.put(None)              # 用 None 作为"结束"信号

def consumer():
    while True:
        item = q.get()
        if item is None:
            break
        print(f"consumed {item}")
        q.task_done()

threading.Thread(target=producer).start()
threading.Thread(target=consumer).start()
```

`queue.Queue` 内部已经用 `Lock` + `Condition` 实现好了同步逻辑，是线程间通信的**首选方式**，不需要自己手写 Lock/Condition。

---

## 示例14：多线程 + 全局异常处理

```python
import threading
import traceback

def risky_task():
    raise ValueError("something went wrong")

def safe_wrapper(func):
    try:
        func()
    except Exception:
        traceback.print_exc()

t = threading.Thread(target=safe_wrapper, args=(risky_task,))
t.start()
t.join()
print("main thread continues normally")
```

**重要心智模型**：子线程抛出的异常**不会传播到主线程**，也不会让主线程崩溃——它只会打印到 stderr（或被静默吞掉）。因此生产代码中通常需要自己捕获异常并上报。

---

# 四、常见陷阱

| 陷阱 | 说明 |
|------|------|
| 忘记 `join()` | 主线程可能在子线程完成前就退出 |
| 忘记加锁 | 共享变量读写产生竞态条件，结果不确定 |
| 用多线程做 CPU 密集计算 | 受 GIL 限制，反而比单线程慢（应用 `multiprocessing`） |
| 死锁 | 多个锁交叉获取顺序不一致，导致相互等待 |
| 子线程异常被吞掉 | 需要显式 try/except 并上报 |

---

# 五、一句话总结

> **threading 是"共享内存 + 操作系统抢占调度"的并发模型，天然适合 IO 密集型任务，但必须自己动手管理好同步原语，避免竞态条件与死锁。**
