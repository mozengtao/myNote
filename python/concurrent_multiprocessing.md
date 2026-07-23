# Python multiprocessing 详解 —— 多进程模型

> 本文是 [concurrent_process.md](./concurrent_process.md) 中"四、multiprocessing —— 多进程"章节的详细展开版。

---

# 一、基本思想

`multiprocessing` 解决的核心问题是：**CPU 密集型任务，需要真正利用多核并行计算。**

心智模型只有一句话：

> **每个进程都是一个独立的 Python 解释器，各自拥有自己的内存空间。**

```
   Process1              Process2              Process3
┌───────────┐         ┌───────────┐         ┌───────────┐
│ Memory A  │         │ Memory B  │         │ Memory C  │
│ 独立解释器 │          │ 独立解释器 │         │ 独立解释器 │
└───────────┘         └───────────┘         └───────────┘
      │                     │                     │
      └─────────────────────┼─────────────────────┘
                            │
                   通过 IPC（Queue/Pipe/Manager）通信
```

关键特征：

| 特征 | 说明 |
|------|------|
| 内存 | 每个进程独立，默认互不可见，需要显式跨进程通信 |
| 调度者 | 操作系统 |
| 并行性 | 真正的并行，没有 GIL 限制，能跑满多核 CPU |
| 切换/创建开销 | 远大于线程（要复制/重建解释器状态） |
| 安全性 | 没有共享内存，天然没有竞态条件（除非显式使用 Manager/SharedMemory） |

适用场景：

- 图像/视频处理
- 大规模数值计算（不使用 numpy 多线程后端时）
- 数据压缩/加密
- 任何"纯计算、几乎不等待 IO"的任务

不适用场景：

- 大量小任务的 IO 等待（进程创建/切换开销远高于线程，收益不划算）

---

# 二、心智模式图谱

```
┌─────────────────────────────────────────────────┐
│               multiprocessing 心智模式             │
│                                                   │
│   Process       : 独立的解释器 + 独立内存           │
│   Pool          : 一批可复用的 Worker 进程          │
│   Queue         : 跨进程安全传递数据（基于 IPC）     │
│   Pipe          : 两个进程之间的双向直连通道         │
│   Lock          : 跨进程互斥（保护共享资源）         │
│   Manager       : 提供可跨进程共享的 dict/list 等   │
│   SharedMemory  : 真正的共享内存块（零拷贝）         │
└─────────────────────────────────────────────────┘
```

关键词：**隔离（Isolation）+ 通信（IPC）**

---

# 三、典型代码示例

## 示例1：创建并启动一个进程

```python
from multiprocessing import Process

def worker():
    print("process is working")

if __name__ == "__main__":
    p = Process(target=worker)
    p.start()
    p.join()
```

> **注意**：Windows/macOS 默认用 `spawn` 方式创建子进程，会重新 import 主模块，因此必须把入口代码放在 `if __name__ == "__main__":` 里，否则会导致递归创建进程甚至报错。

---

## 示例2：给子进程传递参数

```python
from multiprocessing import Process

def worker(name, delay):
    print(f"{name} running, delay={delay}")

if __name__ == "__main__":
    p = Process(target=worker, args=("task-1",), kwargs={"delay": 2})
    p.start()
    p.join()
```

和 `threading.Thread` 用法几乎一致——这是 `multiprocessing` API 有意设计成和 `threading` 对称的结果。

---

## 示例3：Pool.map —— 批量并行计算

```python
from multiprocessing import Pool

def square(x):
    return x * x

if __name__ == "__main__":
    with Pool(processes=4) as pool:
        results = pool.map(square, range(10))
    print(results)
```

心智模型：

```
任务列表 [0,1,2,...,9]
        │
        ▼
     任务队列
        │
   ┌────┼────┬────┐
   ▼    ▼    ▼    ▼
  P1   P2   P3   P4     (4个 Worker 进程并行处理)
   │    │    │    │
   └────┴────┴────┘
        │
        ▼
   按顺序收集结果
```

`Pool.map` 会阻塞，直到所有任务完成并按输入顺序收集好结果。

---

## 示例4：Pool.apply_async —— 异步提交 + 回调

```python
from multiprocessing import Pool

def compute(x):
    return x ** 2

def on_done(result):
    print(f"got result: {result}")

if __name__ == "__main__":
    with Pool(4) as pool:
        for i in range(5):
            pool.apply_async(compute, args=(i,), callback=on_done)
        pool.close()
        pool.join()   # 等待所有异步任务执行完
```

`apply_async` 立即返回，不阻塞主进程；结果通过 `callback` 在**主进程**中异步接收（回调本身运行在主进程的一个内部线程里）。

---

## 示例5：multiprocessing.Queue —— 跨进程通信

```python
from multiprocessing import Process, Queue

def producer(q):
    for i in range(5):
        q.put(i)
    q.put(None)   # 结束信号

def consumer(q):
    while True:
        item = q.get()
        if item is None:
            break
        print(f"consumed {item}")

if __name__ == "__main__":
    q = Queue()
    p1 = Process(target=producer, args=(q,))
    p2 = Process(target=consumer, args=(q,))
    p1.start(); p2.start()
    p1.join(); p2.join()
```

和 `queue.Queue` 名字一样，但底层实现完全不同：

```
threading.Queue    : 基于内存 Lock/Condition（同进程内共享内存）
multiprocessing.Queue : 基于管道 + 序列化（pickle），跨进程通信
```

放入队列的对象必须**可被 pickle 序列化**。

---

## 示例6：Pipe —— 双向直连通道

```python
from multiprocessing import Process, Pipe

def worker(conn):
    conn.send("hello from child")
    print("child received:", conn.recv())
    conn.close()

if __name__ == "__main__":
    parent_conn, child_conn = Pipe()
    p = Process(target=worker, args=(child_conn,))
    p.start()
    print("parent received:", parent_conn.recv())
    parent_conn.send("hello from parent")
    p.join()
```

`Pipe()` 返回一对连接对象，专门用于"两个进程之间"的点对点通信，比 `Queue`（支持多生产者多消费者）更轻量。

```
Parent ──conn.send()──> Pipe ──conn.recv()──> Child
Parent <─conn.recv()── Pipe <──conn.send()── Child
```

---

## 示例7：Lock —— 跨进程互斥锁

```python
from multiprocessing import Process, Lock, Value

def increment(lock, counter):
    for _ in range(10000):
        with lock:
            counter.value += 1

if __name__ == "__main__":
    lock = Lock()
    counter = Value("i", 0)   # 共享的整型（详见示例9）

    processes = [Process(target=increment, args=(lock, counter)) for _ in range(4)]
    for p in processes:
        p.start()
    for p in processes:
        p.join()

    print(counter.value)   # 40000
```

思路和 `threading.Lock` 一致，只是这里保护的是**跨进程共享内存**（`Value`/`Array`/`SharedMemory`），而不是普通 Python 对象。

---

## 示例8：Manager —— 共享 dict / list

```python
from multiprocessing import Process, Manager

def worker(shared_dict, key, value):
    shared_dict[key] = value

if __name__ == "__main__":
    with Manager() as manager:
        shared_dict = manager.dict()
        processes = [
            Process(target=worker, args=(shared_dict, f"key{i}", i))
            for i in range(5)
        ]
        for p in processes:
            p.start()
        for p in processes:
            p.join()
        print(dict(shared_dict))
```

`Manager` 会启动一个独立的"管理进程"，其它进程通过代理对象（Proxy）远程操作它内部真实的 dict/list，本质上仍然是 IPC，只是接口封装成了"像本地对象一样使用"。

```
Process A ─┐
Process B ─┼─> Manager 进程（真正持有 dict）
Process C ─┘        通过网络/管道代理访问
```

---

## 示例9：Value / Array —— 共享基础类型内存

```python
from multiprocessing import Process, Value, Array

def worker(shared_value, shared_array):
    shared_value.value += 1
    for i in range(len(shared_array)):
        shared_array[i] *= 2

if __name__ == "__main__":
    v = Value("i", 0)              # 'i' = C int 类型
    arr = Array("i", [1, 2, 3, 4])

    p = Process(target=worker, args=(v, arr))
    p.start()
    p.join()

    print(v.value)        # 1
    print(list(arr))      # [2, 4, 6, 8]
```

`Value`/`Array` 基于共享内存（`multiprocessing.sharedctypes`），比 `Manager` 更轻量、速度更快，但只能存储 C 兼容的基础类型。

---

## 示例10：SharedMemory —— 真正的零拷贝共享内存块（Python 3.8+）

```python
from multiprocessing import shared_memory, Process
import numpy as np

def worker(shm_name, shape, dtype):
    shm = shared_memory.SharedMemory(name=shm_name)
    arr = np.ndarray(shape, dtype=dtype, buffer=shm.buf)
    arr[:] = arr * 2     # 直接修改共享内存中的数据
    shm.close()

if __name__ == "__main__":
    data = np.array([1, 2, 3, 4], dtype=np.int64)
    shm = shared_memory.SharedMemory(create=True, size=data.nbytes)
    shared_arr = np.ndarray(data.shape, dtype=data.dtype, buffer=shm.buf)
    shared_arr[:] = data[:]

    p = Process(target=worker, args=(shm.name, data.shape, data.dtype))
    p.start()
    p.join()

    print(shared_arr)   # [2 4 6 8] —— 子进程的修改直接反映在共享内存上
    shm.close()
    shm.unlink()
```

与 `Queue`/`Pipe`（需要序列化+拷贝数据）不同，`SharedMemory` 是**真正的同一块物理内存**，特别适合大数组（如 NumPy）在进程间传递，避免昂贵的 pickle 拷贝开销。

---

## 示例11：daemon 进程

```python
from multiprocessing import Process
import time

def background_task():
    while True:
        time.sleep(1)

if __name__ == "__main__":
    p = Process(target=background_task, daemon=True)
    p.start()
    print("main process exits, daemon process will be terminated")
```

与 daemon 线程语义一致：主进程退出时，daemon 子进程会被强制终止。

---

## 示例12：ProcessPoolExecutor 风格对比（进程池的另一种写法）

```python
from concurrent.futures import ProcessPoolExecutor

def cpu_bound_task(n):
    return sum(i * i for i in range(n))

if __name__ == "__main__":
    with ProcessPoolExecutor(max_workers=4) as pool:
        results = list(pool.map(cpu_bound_task, [10_000_000] * 4))
    print(results)
```

`concurrent.futures.ProcessPoolExecutor` 底层也是基于 `multiprocessing`，但提供了和线程池一致的 `Future` 接口（详见 [concurrent_futures.md](./concurrent_futures.md)）。多数新代码更推荐直接用它，而不是手写 `multiprocessing.Pool`。

---

# 四、常见陷阱

| 陷阱 | 说明 |
|------|------|
| 忘记 `if __name__ == "__main__":` | 在 Windows/macOS(spawn) 下会导致递归创建进程或报错 |
| 传递不可 pickle 的对象 | lambda、打开的文件句柄、数据库连接等无法跨进程传递 |
| 以为进程间内存共享 | 默认完全隔离，必须用 Queue/Pipe/Manager/SharedMemory 显式通信 |
| 进程创建开销被忽视 | 大量小任务用进程池反而比线程池慢 |
| 子进程异常不会自动抛到主进程 | 需要通过返回值/Queue 把异常传回主进程处理 |

---

# 五、一句话总结

> **multiprocessing 是"独立内存 + 真正并行"的模型，天然适合 CPU 密集型任务；代价是进程创建开销大，且必须通过 IPC（Queue/Pipe/Manager/SharedMemory）显式地在进程之间搬运数据。**
