[how async/await works](how_async_await_works.py)  
[async mental model](async_mental_model.md)



# Python asyncio 关键 API 速查表

> **适用版本**：Python 3.7+  
> **核心理念**：单线程协作式多任务，非阻塞 I/O

---

## 1. 入口与事件循环 (Entry & Loop)
程序的启动点，负责管理事件循环的生命周期。

| API | 作用 | 示例 |
| :--- | :--- | :--- |
| **`asyncio.run(coro)`** | **最高层入口**。创建循环，运行协程，自动关闭循环。 | `asyncio.run(main())` |
| `asyncio.get_event_loop()` | 获取当前事件循环对象。 | `loop = asyncio.get_event_loop()` |
| `loop.run_until_complete()` | 运行协程直到完成（旧式写法）。 | `loop.run_until_complete(coro)` |

---

## 2. 任务创建与调度 (Tasks & Scheduling)
实现并发执行的核心，用于同时运行多个协程。

| API | 作用 | 示例 |
| :--- | :--- | :--- |
| **`asyncio.create_task(coro)`** | **后台调度**。立即调度协程执行，返回 Task 对象。 | `task = asyncio.create_task(fetch())` |
| **`asyncio.gather(*coros)`** | **并发等待**。并发运行多个协程，等待全部完成。 | `res = await asyncio.gather(t1, t2)` |
| `asyncio.wait(tasks)` | 底层等待，返回 (done, pending) 集合。 | `done, pending = await asyncio.wait(tasks)` |
| `asyncio.as_completed(tasks)` | 按完成顺序迭代返回结果。 | `for res in asyncio.as_completed(tasks): ...` |
| `task.cancel()` | 请求取消任务。 | `task.cancel()` |

---

## 3. 时间与超时控制 (Time & Timeout)
用于非阻塞等待和防止任务无限挂起。

| API | 作用 | 示例 |
| :--- | :--- | :--- |
| **`asyncio.sleep(delay)`** | **非阻塞休眠**。挂起当前任务，不阻塞事件循环。 | `await asyncio.sleep(1)` |
| **`asyncio.wait_for(coro, timeout)`** | **超时控制**。超时抛出 `TimeoutError`。 | `await asyncio.wait_for(coro, 5.0)` |
| `asyncio.timeout(delay)` | 上下文管理器形式的超时 (Python 3.11+)。 | `async with asyncio.timeout(5): ...` |

---

## 4. 同步原语 (Synchronization)
用于协程间的状态协调和共享资源保护（非线程锁）。

| API | 作用 | 示例 |
| :--- | :--- | :--- |
| **`asyncio.Lock()`** | **互斥锁**。防止并发修改共享资源。 | `async with lock: ...` |
| `asyncio.Event()` | 事件标志。用于任务间通知。 | `await event.wait(); event.set()` |
| `asyncio.Semaphore(n)` | 信号量。限制最大并发数量。 | `async with sem: ...` |
| `asyncio.Condition()` | 条件变量。等待特定条件成立。 | `await cond.wait()` |

---

## 5. 队列 (Queues)
用于生产者 - 消费者模式的安全数据传递。

| API | 作用 | 示例 |
| :--- | :--- | :--- |
| **`asyncio.Queue()`** | **FIFO 队列**。异步 put/get。 | `await queue.put(item)` |
| `queue.join()` | 等待队列所有任务处理完毕。 | `await queue.join()` |
| `queue.task_done()` | 标记队列任务完成。 | `queue.task_done()` |

---

## 6. 线程与进程集成 (Threading & Processes)
在异步代码中安全调用阻塞代码。

| API | 作用 | 示例 |
| :--- | :--- | :--- |
| **`asyncio.to_thread(func)`** | **线程运行**。在单独线程运行阻塞函数 (3.9+)。 | `await asyncio.to_thread(sync_func)` |
| `loop.run_in_executor()` | 底层执行器调用。 | `await loop.run_in_executor(None, func)` |
| `asyncio.create_subprocess_exec()` | 异步启动子进程。 | `await asyncio.create_subprocess_exec(...)` |

---

## 7. 最佳实践与避坑 (Best Practices)

### ✅ 推荐做法
1. **全链路异步**：确保 I/O 操作均使用异步库（如 `aiohttp`, `asyncpg`）。
2. **异常处理**：在 `gather` 中使用 `return_exceptions=True` 或在协程内部 `try/except`。
3. **资源管理**：使用 `async with` 管理锁、会话、连接等资源。
4. **入口规范**：使用 `asyncio.run()` 作为程序唯一入口。

### ❌ 常见陷阱
1. **阻塞事件循环**：严禁在 `async` 函数中使用 `time.sleep()` 或同步 I/O。
2. **忘记 await**：调用协程函数未加 `await` 会导致协程对象未被调度。
3. **任务丢失**：`create_task` 创建的任务若主程序退出前未 `await`，可能被强制取消。
4. **CPU 密集误区**：异步无法加速 CPU 密集计算，此类场景请用 `multiprocessing`。

---

## 8. 速查口诀
- **启动**：`asyncio.run`
- **并发**：`create_task` + `gather`
- **暂停**：`asyncio.sleep` (别用 time.sleep)
- **超时**：`wait_for`
- **锁**：`asyncio.Lock`
- **阻塞转异步**：`to_thread`