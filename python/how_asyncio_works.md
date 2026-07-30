# Python asyncio 底层运行机制：Task 调度、await 挂起、Future 驱动机制

> **一句话总结**
>
> **asyncio 是一个基于 Event Loop 的协作式（Cooperative）调度器。**
>
> **Coroutine 描述代码，Task 调度 Coroutine，Future 承载挂起状态，Event Loop 驱动整个系统运行。**
>
> **真正负责"暂停/恢复"的永远都是 Future。**

---

# 一、先建立整个系统的架构图

```
                 User Code
                     │
                     ▼
             async def coroutine()
                     │
                     ▼
             asyncio.create_task()
                     │
                     ▼
                  Task
                     │
                     ▼
               Event Loop
                     │
      ┌──────────────┴───────────────┐
      ▼                              ▼
 Ready Queue                  IO Selector
(Task Ready to Run)      (epoll/kqueue/iocp)
      │                              │
      └──────────────┬───────────────┘
                     ▼
                  Future
                     ▲
                     │
             IO completed /
             Timer expired /
             Future.set_result()
```

可以把它理解成：

```
Coroutine = 要执行什么

Task = 谁负责执行

Future = 等什么

EventLoop = 谁安排大家什么时候执行
```

这是 asyncio 最重要的四个角色。

---

# 二、asyncio 中只有三种合法 Awaitable

Python 官方定义：

```
Awaitable
├── Coroutine
├── Task
└── Future
```

例如

```python
await coro()

await asyncio.create_task(coro())

await future
```

全部合法。

除此之外：

```
await list()          ❌

await dict()          ❌

await object()        ❌
```

全部非法。

原因非常简单：

**await 只能等待"未来某个时间会完成"的对象。**

而 Python 规定：

只有 Coroutine、Task、Future 三类对象具有这种能力。

---

# 三、Coroutine 到底是什么？

例如

```python
async def download():
    ...
```

调用：

```python
coro = download()
```

注意：

这里没有运行。

只是创建：

```
Coroutine Object
```

里面保存：

```
下一条要执行的位置

局部变量

执行状态

异常状态
```

可以理解成：

```
Coroutine
≈

保存了一份函数执行现场
```

类似：

```
Program Counter

Stack Frame

Local Variables
```

但是：

**Coroutine 不会自己运行。**

必须有人驱动。

那就是：

```
Task
```

---

# 四、Task 的职责是什么？

Task 的作用只有一句话：

> **不断推进 Coroutine 往前运行。**

内部大概就是：

```
while coroutine 没结束：

    coroutine.send(...)
```

可以理解成：

```
Task
↓

send()

↓

Coroutine

↓

执行一点点

↓

返回

↓

Task

↓

再次 send()

...
```

Task 本身就是 Coroutine 的 Driver。

---

# 五、Coroutine 为什么能暂停？

例如：

```python
async def work():

    print("A")

    await asyncio.sleep(3)

    print("B")
```

很多人误以为：

```
sleep()

暂停 CPU
```

实际上：

不是。

真正发生的是：

```
await

↓

Coroutine 返回控制权

↓

Task 停止继续 send()

↓

EventLoop 去运行别人
```

注意：

CPU 没睡。

线程没睡。

暂停的是：

```
当前 Coroutine
```

---

# 六、await 到底发生了什么？

这是 asyncio 最重要的一步。

例如：

```python
await future
```

底层大概变成：

```
yield future
```

Coroutine：

```
执行到 await

↓

返回 Future

↓

暂停自己
```

于是：

Task 收到：

```
哦！

它正在等待这个 Future。
```

Task：

```
停止推进 Coroutine
```

并告诉：

```
EventLoop

这个 Task 先不要运行。
```

所以：

```
await

↓

Coroutine 挂起

↓

Task 停止

↓

等待 Future
```

---

# 七、Future 到底是什么？

Future 可以理解成：

> **一个"未来会产生结果"的占位符。**

例如：

```
现在没有结果

↓

Future

↓

以后某一刻：

Future.set_result(...)
```

例如：

```python
future = Future()

...

future.set_result(100)
```

之后：

```
await future

↓

立即恢复
```

所以：

Future 本质就是：

```
State Machine

PENDING

↓

FINISHED

↓

保存 result
```

内部状态非常简单：

```
Future

├── state
├── result
├── exception
└── callbacks
```

---

# 八、为什么说真正负责挂起的是 Future？

例如：

```python
await asyncio.sleep(1)
```

很多人以为：

```
sleep()

自己会暂停
```

实际上：

sleep()

最后返回的是：

```
Future
```

流程：

```
sleep()

↓

创建 Future

↓

注册 Timer

↓

返回 Future

↓

await Future

↓

Task 停止
```

真正等待的是：

```
Future
```

而不是：

```
sleep()
```

---

# 九、Task 调度全过程

下面是最经典的一张图。

```
create_task()

        │

        ▼

Ready Queue

        │

        ▼

Task._step()

        │

        ▼

Coroutine.send()

        │

        ▼

执行代码

        │

        ▼

遇到 await Future

        │

        ▼

Coroutine yield Future

        │

        ▼

Task 注册 callback

        │

        ▼

Task 暂停

        │

        ▼

EventLoop 调度其他 Task

        │

        ▼

Future 完成

        │

        ▼

callback()

        │

        ▼

Task 放回 Ready Queue

        │

        ▼

Task._step()

        │

        ▼

Coroutine.send(result)

继续运行
```

整个 asyncio 都是在重复这个循环。

---

# 十、Future 如何恢复 Task？

例如：

```
Future

↓

Task 正在等待
```

Task 会提前注册：

```
future.add_done_callback(...)
```

例如：

```
Future

↓

callbacks

↓

Task._wakeup
```

当：

```
future.set_result()
```

立即：

```
遍历 callbacks

↓

Task._wakeup()

↓

Task 放回 Ready Queue
```

于是：

下一轮 Event Loop：

```
Task

↓

继续 send()
```

恢复运行。

---

# 十一、整个 EventLoop 在干什么？

EventLoop 无限循环：

```
while True:

    运行 Ready Queue

    等待 IO

    等待 Timer

    Future 完成

    放回 Ready Queue
```

伪代码：

```python
while True:

    while ready_queue:

        task = ready_queue.pop()

        task.step()

    selector.select(timeout)

    process_events()

    process_timers()
```

这就是 asyncio 的核心。

---

# 十二、Task.step() 的伪代码

官方 CPython 实现远比下面复杂。

核心思想其实就是：

```python
class Task:

    def step(self):

        try:
            future = coro.send(None)

            future.add_done_callback(
                self.wakeup
            )

        except StopIteration as e:
            self.result = e.value
```

恢复：

```python
def wakeup(future):

    ready_queue.append(task)
```

下一轮：

```
step()

↓

send(result)

↓

继续运行
```

---

# 十三、IO 是如何变成 Future 的？

例如：

```python
await reader.read(1024)
```

实际上：

```
reader.read()

↓

创建 Future

↓

socket 注册 epoll

↓

返回 Future
```

EventLoop：

```
epoll_wait()
```

收到：

```
socket readable
```

于是：

```
Future.set_result(data)
```

Task：

```
恢复
```

因此：

```
Socket

↓

epoll

↓

Future

↓

Task

↓

Coroutine
```

---

# 十四、Timer 是如何工作的？

例如：

```python
await asyncio.sleep(3)
```

内部：

```
Future()

↓

call_later()

↓

Timer Heap
```

三秒以后：

```
Timer

↓

Future.set_result(None)

↓

Task Resume
```

整个过程中：

线程没有睡。

CPU 没阻塞。

只是：

```
Future

一直没完成
```

---

# 十五、Task 为什么能够切换？

因为：

```
Task

不会抢占别人。
```

只有：

```
await
```

才会：

```
主动让出执行权
```

所以：

asyncio 是：

```
Cooperative Scheduling

协作式调度
```

不是：

```
Preemptive Scheduling

抢占式调度
```

因此：

```python
while True:
    pass
```

整个 EventLoop：

```
全部卡死
```

因为：

没有：

```
await
```

---

# 十六、await asyncio.sleep(0) 为什么能让出 CPU？

例如：

```python
while True:

    do_some_work()

    await asyncio.sleep(0)
```

sleep(0)：

```
创建 Future

↓

立即下一轮完成

↓

Task 进入 Ready Queue 尾部
```

于是：

```
当前 Task

↓

让别人先运行

↓

自己稍后继续
```

这是 asyncio 最经典的：

```
Yield Point
```

---

# 十七、完整生命周期（黄金流程）

```
Coroutine 创建
        │
        ▼
create_task()
        │
        ▼
Task 放入 Ready Queue
        │
        ▼
EventLoop 调度 Task
        │
        ▼
Task.send(None)
        │
        ▼
Coroutine 执行
        │
        ├──────────────┐
        │              │
        │ return       │ await Future
        │              │
        ▼              ▼
Task 完成      Task 注册 Future 回调
        │              │
        │              ▼
        │       Future Pending
        │              │
        │      IO/Timer 完成
        │              │
        │              ▼
        │     Future.set_result()
        │              │
        │              ▼
        └────── Task 唤醒 ───────┐
                                │
                                ▼
                      Ready Queue
                                │
                                ▼
                          下一轮继续执行
```

- Coroutine, Task, Future 完整交互过程
```
                EventLoop
                    │
                    │ 调度
                    ▼
               ┌──────────┐
               │   Task   │
               └──────────┘
                     │
          "继续执行 coroutine"
                     │
                     ▼
             coro.send(...)
                     │
                     ▼
           ┌─────────────────┐
           │   Coroutine     │
           └─────────────────┘
                     │
             执行 Python 代码
                     │
          ┌──────────┴──────────┐
          │                     │
          │ return              │ await Future
          │                     │
          ▼                     ▼
     Coroutine结束         返回 Future 给 Task
                                  │
                                  ▼
                         Task 注册回调
                                  │
                                  ▼
                         Future(Pending)
                                  │
                     IO / Timer 完成
                                  │
                                  ▼
                       Future.set_result()
                                  │
                                  ▼
                      调用 Task.wakeup()
                                  │
                                  ▼
                      Task 重新进入 Ready Queue
                                  │
                                  ▼
                        coro.send(...) 继续执行
```

- 生活中的比喻
    - Coroutine = 一个工程师（知道怎么干活）
    - Task = 项目经理（安排工程师什么时候继续工作）
    - Future = 快递订单（工程师必须等材料送到）
```
Task：
"开始干活。"

↓

Coroutine：
"好的。"

↓

干了一会：

"等等。

我要等快递。"

↓

把 Future（快递单）交给 Task

↓

Task：
"行，

快递到了通知我。"

↓

Task 去安排别的工程师

↓

快递送到了

↓

Future：
"材料到了！"

↓

Task：
"继续干！"

↓

Coroutine：
"收到，继续。"

↓

完成

↓

Task：
"项目结束。"
```

---

# 十八、核心心智模型（建议牢记）

## ① 四层职责分离

| 层次 | 职责 | 本质 |
|------|------|------|
| Coroutine | 保存执行现场 | State Machine（状态机） |
| Task | 驱动 Coroutine 前进 | Scheduler Driver（调度驱动器） |
| Future | 表示未来结果，承载挂起/恢复 | Promise（承诺对象） |
| EventLoop | 调度所有 Task，监听 IO/Timer | Cooperative Scheduler（协作式调度器） |

---

## ② await 的真正含义

```
await

≠ 睡眠

≠ 阻塞线程

≠ 停止 CPU

=

当前 Coroutine
主动把控制权交回 EventLoop
直到等待的 Future 完成后再恢复。
```

---

## ③ asyncio 的完整数据流

```
Coroutine
      │
      ▼
Task.step()
      │
      ▼
Coroutine.send()
      │
      ▼
await Future
      │
      ▼
Future Pending
      │
      ▼
IO / Timer 完成
      │
      ▼
Future.set_result()
      │
      ▼
Task.wakeup()
      │
      ▼
Ready Queue
      │
      ▼
Task.step()
      │
      ▼
Coroutine 继续运行
```

---

# 十九、最终统一心智模型（One Model）

将 asyncio 的执行过程压缩成一句话：

> **Coroutine 负责描述异步逻辑，Task 负责驱动 Coroutine 执行；当 Coroutine 执行到 `await` 时，会将控制权交还给 Task，并产出一个 Future。Task 不再继续推进，而是把恢复逻辑注册到 Future 上，然后退出，让 Event Loop 去调度其他任务。当 IO、定时器或其他异步事件完成后，对应的 Future 被 `set_result()`，触发 Task 唤醒并重新加入 Ready Queue。Event Loop 在下一轮再次调用 `Task._step()`，继续通过 `coroutine.send(...)` 推进 Coroutine，直到最终结束。整个 asyncio 的暂停、恢复与调度，本质上都是围绕 **Future 状态变化驱动 Task 的生命周期** 展开的。**