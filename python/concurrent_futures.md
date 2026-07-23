# Python concurrent.futures 详解 —— 承诺模型（Future）

> 本文是 [concurrent_process.md](./concurrent_process.md) 中"五、concurrent.futures —— 高层统一接口"章节的详细展开版。

---

# 一、基本思想

`concurrent.futures` 解决的核心问题是：**用同一套接口去操作线程池和进程池，不用关心底层是 Thread 还是 Process。**

心智模型只有一句话：

> **现在提交任务，立刻拿到一张"提货单"（Future），以后再凭这张单去取结果。**

```
submit(task) ──> 立即返回 Future（提货单）
                       │
                后台线程/进程执行任务
                       │
                       ▼
             future.result() ──> 阻塞等待，拿到真正的结果
```

关键特征：

| 特征 | 说明 |
|------|------|
| 统一接口 | `ThreadPoolExecutor` 和 `ProcessPoolExecutor` 用法完全一致 |
| Future | 任务尚未完成时的"占位符"对象 |
| 阻塞点 | 只有调用 `.result()` 时才会阻塞等待 |
| 异常处理 | 任务中的异常会被"存起来"，调用 `.result()` 时才重新抛出 |
| 切换成本 | 从 Thread 换成 Process 只需换一个类名，代码逻辑不变 |

适用场景：

- 想要"线程池"或"进程池"，但不想手写 `Pool`/`Lock`/`Queue` 这些底层细节
- 需要统一处理一批"提交任务 → 收集结果"的场景
- 需要把阻塞代码桥接进 `asyncio`（`run_in_executor`）

---

# 二、心智模式图谱

```
┌─────────────────────────────────────────────────┐
│              concurrent.futures 心智模式           │
│                                                   │
│   Executor   : 任务提交入口（线程池或进程池）        │
│   submit()   : 提交单个任务，立即返回 Future        │
│   map()      : 批量提交，按输入顺序返回结果迭代器     │
│   Future     : "未来会有"的结果占位符               │
│   as_completed(): 谁先完成就先返回谁                │
│   wait()     : 等待一批 Future 中的部分或全部完成    │
└─────────────────────────────────────────────────┘
```

关键词：**承诺（Promise）+ 统一抽象（Unification）**

---

# 三、典型代码示例

## 示例1：ThreadPoolExecutor.map —— 最简单的批量并发

```python
from concurrent.futures import ThreadPoolExecutor

def task(x):
    return x * x

with ThreadPoolExecutor(max_workers=4) as pool:
    results = pool.map(task, range(5))

print(list(results))   # [0, 1, 4, 9, 16]
```

`map()` 会按输入顺序返回结果，行为类似内置 `map()`，但底层是并发执行的。

---

## 示例2：submit() + Future.result() —— 提交单个任务

```python
from concurrent.futures import ThreadPoolExecutor

def task(x):
    return x * x

with ThreadPoolExecutor() as pool:
    future = pool.submit(task, 5)
    print(future.result())   # 阻塞直到任务完成，拿到 25
```

心智模型：

```
submit(task, 5)
      │
      ▼
立即返回 Future ──(后台执行 task(5))──> future.result() 阻塞等待 ──> 25
```

---

## 示例3：批量 submit + 收集结果

```python
from concurrent.futures import ThreadPoolExecutor

def task(x):
    return x * x

with ThreadPoolExecutor(max_workers=4) as pool:
    futures = [pool.submit(task, i) for i in range(5)]
    results = [f.result() for f in futures]

print(results)   # [0, 1, 4, 9, 16]
```

和 `map()` 的区别：`submit()` 可以给每个任务传不同的函数/参数组合，更灵活；`map()` 更简洁，适合同一个函数批量处理。

---

## 示例4：as_completed —— 谁先完成就先处理谁

```python
import time
import random
from concurrent.futures import ThreadPoolExecutor, as_completed

def task(x):
    time.sleep(random.uniform(0.1, 1))
    return x

with ThreadPoolExecutor(max_workers=4) as pool:
    futures = {pool.submit(task, i): i for i in range(5)}
    for future in as_completed(futures):
        i = futures[future]
        print(f"task {i} finished with result {future.result()}")
```

与 `map()`/顺序 `.result()` 不同，`as_completed()` **不保证顺序**，而是哪个任务先执行完就先返回哪个——适合"谁先做完就先处理谁"的场景（如展示进度、尽早失败）。

```
提交: [f0, f1, f2, f3, f4]
完成顺序（不确定）: f3 -> f1 -> f0 -> f4 -> f2
```

---

## 示例5：wait() —— 等待部分完成（FIRST_COMPLETED）

```python
import time
from concurrent.futures import ThreadPoolExecutor, wait, FIRST_COMPLETED

def task(x):
    time.sleep(x)
    return x

with ThreadPoolExecutor(max_workers=4) as pool:
    futures = [pool.submit(task, i) for i in [3, 1, 2]]
    done, not_done = wait(futures, return_when=FIRST_COMPLETED)
    print("first done:", [f.result() for f in done])
    print("still running:", len(not_done))
```

`wait()` 支持三种模式：

| 模式 | 说明 |
|------|------|
| `FIRST_COMPLETED` | 只要有一个完成就返回 |
| `FIRST_EXCEPTION` | 只要有一个抛出异常就返回 |
| `ALL_COMPLETED`（默认） | 全部完成才返回 |

---

## 示例6：add_done_callback —— 完成时自动回调

```python
from concurrent.futures import ThreadPoolExecutor

def task(x):
    return x * x

def on_done(future):
    print(f"callback got result: {future.result()}")

with ThreadPoolExecutor() as pool:
    future = pool.submit(task, 4)
    future.add_done_callback(on_done)
```

回调会在任务完成的那个瞬间被自动调用（在执行任务的线程/进程池的内部线程里），不需要主线程主动去 `.result()`。

---

## 示例7：异常处理 —— 异常被"存起来"，调用 result() 时才抛出

```python
from concurrent.futures import ThreadPoolExecutor

def risky(x):
    if x == 3:
        raise ValueError(f"bad value: {x}")
    return x

with ThreadPoolExecutor() as pool:
    futures = [pool.submit(risky, i) for i in range(5)]
    for f in futures:
        try:
            print(f.result())
        except ValueError as e:
            print(f"caught: {e}")
```

**关键心智模型**：任务内部抛出的异常**不会立即让程序崩溃**，而是被 Future 静默"收纳"，只有显式调用 `.result()`（或 `.exception()`）时才会重新抛出——这是与直接跑一个普通函数最大的行为差异。

```python
# 也可以主动检查是否有异常，而不抛出
exc = f.exception()
if exc is not None:
    print("task failed:", exc)
```

---

## 示例8：result(timeout=...) —— 超时控制

```python
from concurrent.futures import ThreadPoolExecutor, TimeoutError
import time

def slow_task():
    time.sleep(5)
    return "done"

with ThreadPoolExecutor() as pool:
    future = pool.submit(slow_task)
    try:
        print(future.result(timeout=2))
    except TimeoutError:
        print("task did not finish within 2 seconds")
```

注意：超时只是让 `.result()` 停止等待并抛出 `TimeoutError`，**并不会真正终止后台正在执行的任务**（尤其是线程池——线程无法被强制杀死）。

---

## 示例9：cancel() —— 取消尚未开始的任务

```python
from concurrent.futures import ThreadPoolExecutor
import time

def task(x):
    time.sleep(1)
    return x

with ThreadPoolExecutor(max_workers=1) as pool:
    f1 = pool.submit(task, 1)   # 立即开始执行（占用唯一的 worker）
    f2 = pool.submit(task, 2)   # 排队等待
    cancelled = f2.cancel()     # 尝试取消排队中的任务
    print("cancelled:", cancelled)   # True，因为它还没开始执行
    print(f1.result())
```

`cancel()` 只对**还未开始执行**的任务有效；一旦任务已经在运行，`cancel()` 会返回 `False`，无法中途打断。

---

## 示例10：ProcessPoolExecutor —— CPU 密集型任务

```python
from concurrent.futures import ProcessPoolExecutor

def cpu_bound(n):
    return sum(i * i for i in range(n))

if __name__ == "__main__":
    with ProcessPoolExecutor(max_workers=4) as pool:
        results = list(pool.map(cpu_bound, [10_000_000] * 4))
    print(results)
```

用法与 `ThreadPoolExecutor` 完全一致——这正是 `concurrent.futures` 的核心价值：**只换类名，业务代码不用改**。

```
ThreadPoolExecutor  ──> 共享内存，适合 IO 密集
ProcessPoolExecutor ──> 独立内存，适合 CPU 密集
```

---

## 示例11：ProcessPoolExecutor + submit —— 收集多个 CPU 任务的结果

```python
from concurrent.futures import ProcessPoolExecutor, as_completed

def cpu_bound(n):
    return n, sum(i * i for i in range(n))

if __name__ == "__main__":
    sizes = [1_000_000, 2_000_000, 3_000_000]
    with ProcessPoolExecutor() as pool:
        futures = [pool.submit(cpu_bound, n) for n in sizes]
        for f in as_completed(futures):
            n, result = f.result()
            print(f"n={n}, result={result}")
```

---

## 示例12：run_in_executor —— 把线程池/进程池桥接进 asyncio

```python
import asyncio
from concurrent.futures import ThreadPoolExecutor

def blocking_io():
    import time
    time.sleep(2)
    return "blocking io done"

async def main():
    loop = asyncio.get_running_loop()
    with ThreadPoolExecutor() as pool:
        # 把阻塞函数丢进线程池执行，asyncio 事件循环不会被阻塞
        result = await loop.run_in_executor(pool, blocking_io)
        print(result)

asyncio.run(main())
```

心智模型：

```
asyncio 事件循环（单线程，不能阻塞）
        │
        │ run_in_executor(pool, blocking_func)
        ▼
   线程池/进程池（专门处理阻塞代码）
        │
        ▼
   await 拿到结果，事件循环继续调度其它协程
```

这是连接"协作式并发（asyncio）"与"传统阻塞代码（第三方同步库）"最常用的桥梁，详见 [concurrent_asyncio.md](./concurrent_asyncio.md)。

---

# 四、Thread 池 vs Process 池怎么选

| 场景 | 选择 | 原因 |
|------|------|------|
| 网络请求、文件 IO | ThreadPoolExecutor | IO 等待期间可以切换到其它线程，共享内存传参方便 |
| CPU 密集计算 | ProcessPoolExecutor | 绕过 GIL，真正利用多核 |
| 任务需要传递大对象 | ThreadPoolExecutor | 进程池需要 pickle 序列化，大对象开销大 |
| 任务本身不稳定/可能崩溃 | ProcessPoolExecutor | 进程隔离，一个任务崩溃不会拖垮主进程 |

---

# 五、一句话总结

> **concurrent.futures 是"承诺模型"：提交任务立即拿到 Future，何时真正阻塞取决于你何时调用 `.result()`；它用同一套 API 统一了线程池与进程池，让你只需在两者之间切换类名，而不必改动业务逻辑。**
