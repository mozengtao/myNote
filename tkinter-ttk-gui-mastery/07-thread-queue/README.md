# Layer 7：后台线程与线程安全

> 对应示例代码：[examples/07_background_worker.py](examples/07_background_worker.py)

这是整个教程的重点章节。

---

## A. Why

Layer 6 讲清楚了 `after()` 不会改变回调的执行线程——它调度的函数仍然在主线程里跑。那真正耗时的工作（网络请求、SSH、子进程、长时间设备测试）该怎么办？如果直接在按钮回调里跑这些逻辑，主线程会被整段同步阻塞，`mainloop()` 没有机会处理任何其他事件，窗口表现为"未响应"（在很多操作系统上，标题栏会直接显示"(未响应)"字样）。

这一层要解决的问题：**把耗时工作挪到另一个线程去执行，同时保证"只有主线程能触碰 Tkinter 控件"这条铁律不被打破**。

---

## B. Mental Model

### B.1 整体数据流：生产者-消费者模型

```text
Main Thread
    |
    | start task
    v
Worker Thread
    |
    | queue.put(message)
    v
Queue
    |
    | root.after(...)
    v
Main Thread
    |
    | update widgets
    v
UI
```

这是一个经典的**生产者-消费者模型**：
- **生产者**：Worker Thread，只负责干活、把"发生了什么"打包成消息丢进 `queue.Queue`，对 Tkinter 一无所知。
- **消息队列**：`queue.Queue` 是线程安全的（内部自带锁），这是它被选为"唯一通信渠道"的原因——你不需要自己写锁。
- **消费者**：主线程里由 `after()` 周期性触发的轮询函数，负责把队列中已经攒下的消息逐条取出，根据消息类型更新界面。

### B.2 为什么后台线程不能直接改 Tkinter 控件

Tk（C 层实现）内部的大部分数据结构**不是线程安全的**。如果两个线程同时调用 Tk 相关的函数（哪怕一个只是读、一个只是写），可能出现数据竞争，表现为随机的崩溃、界面绘制错乱，或者看起来"运气好就没事，运气不好就炸"的不稳定行为——这类 bug 极难复现和定位。

因此 Tkinter 的硬性规则是：**只有运行 `mainloop()` 的那个线程（几乎总是主线程）才允许创建、读取、修改任何 Tkinter 对象**（Widget、Variable、甚至包括调用 `widget.after()` 这件事本身，也应该在主线程发起）。后台线程唯一安全的"出口"是把数据放进一个线程安全的通道（`queue.Queue`），然后什么都不碰，等主线程自己来取。

### B.3 为什么选 `queue.Queue` + `after()` 轮询，而不是别的方式

| 候选方案 | 问题 |
|---|---|
| 后台线程直接调用 `widget.config(...)` | 违反 B.2 的线程安全规则，随机崩溃 |
| 后台线程调用 `root.update()` 强制刷新界面 | `update()` 会立即处理所有挂起事件（包括用户的点击），在后台线程调用它本质上还是跨线程操作 Tk 内部状态，同样不安全；而且会打乱事件处理的正常顺序，是公认的反模式 |
| 后台线程通过某种方式"回调"主线程的函数 | 如果这个回调直接执行 Tk 操作，本质和第一种方案一样危险；必须有一个明确的"排队，等主线程主动来取"的中间层 |
| **`queue.Queue` + 主线程 `after()` 轮询（本例做法）** | `Queue` 内部用锁保护，`put`/`get` 跨线程调用是安全的；真正的 Widget 操作永远发生在 `after()` 触发的、运行在主线程的消费函数里 |

### B.4 `threading.Event` 取消信号：请求，而不是强制终止

```text
Main Thread                         Worker Thread
    |                                    |
    | cancel_event.set()                 | for step in steps:
    v                                    |     if cancel_event.is_set():
  （只是把一个标志位设为 True）              |         return   # 自己决定在这里退出
                                         |     do_one_step()
```

Python 的 `threading` 模块**没有提供**"强制杀死一个线程"的安全方式（历史上有一些非官方的 hack，但都不安全，不应该在生产代码里使用）。取消一个线程的正确方式永远是**协作式**的：
1. 主线程设置一个共享的信号（本例用 `threading.Event`，也可以是其他线程安全的标志）。
2. 后台线程在自己代码的"安全点"（比如每完成一步任务）主动检查这个信号，如果发现被要求取消，就自己 `return`（或抛出一个专门的异常再在外层捕获），自然退出。

这意味着：**取消信号的"响应速度"取决于安全点检查的粒度**。如果后台任务里有一段很长时间不检查信号的同步阻塞代码（比如一次很长的网络超时等待），取消请求会被推迟到那段代码结束才生效——这是协作式取消的本质限制，不是实现错误。

### B.5 任务异常如何传回 UI

后台线程里发生的异常，**不会**自动传播到主线程（这是线程模型的基本特性：每个线程有自己独立的调用栈，异常只会沿着抛出异常的那个线程的调用栈向上传播）。如果 Worker 函数内部没有捕获异常，这个异常会导致该线程静默终止（`threading.Thread` 默认只是在标准错误输出打印一段 traceback），**主线程完全不知道任务已经失败**，界面会一直停留在"运行中"状态。

正确做法：Worker 函数用 `try/except Exception` 包住全部逻辑，捕获到异常后，把异常信息打包成一条消息放进队列（本例的 `WorkerMessage("error", payload=str(exc))`），由主线程的消费函数负责展示并恢复界面状态。

### B.6 主线程与 Worker Thread 的职责边界

```text
Main Thread 职责：                    Worker Thread 职责：
  - 创建/销毁线程                       - 执行耗时的实际工作
  - 创建取消信号、设置取消信号            - 在安全点检查取消信号
  - 创建队列                            - 把进度/日志/结果/异常打包放入队列
  - 通过 after() 轮询队列                - 绝不创建/读取/修改任何 Tkinter 对象
  - 所有 Widget 的创建/读取/修改
  - 决定何时启动/何时允许重新启动
```

---

## C. Architecture

| 对象 | 角色 |
|---|---|
| `WorkerMessage` (dataclass) | 后台线程 → 主线程的统一消息"信封"，`kind` 字段区分消息类型 |
| `run_simulated_test()` | 纯函数式的后台任务逻辑，不依赖任何类实例，不引用 Tkinter，可以独立单测（见下方 Exercises） |
| `self._cancel_event` (`threading.Event`) | 主线程 → 后台线程的取消信号通道 |
| `self._message_queue` (`queue.Queue`) | 后台线程 → 主线程的消息通道（线程安全） |
| `self._worker_thread` (`threading.Thread`) | 持有当前运行中的后台线程引用，用于判断"是否还在运行"和关闭时 `join()` |
| `self._poll_after_id` | 当前轮询任务的 `after()` id（和 Layer 6 的 `_after_id` 是同一种模式） |
| `_poll_queue()` | 消费者：唯一允许更新 Widget 的函数 |

---

## D. Complete Code

```python
"""
07_background_worker.py

Layer 7：后台线程与线程安全（threading + queue.Queue + after()）。

运行方式：
    python3 examples/07_background_worker.py

本例模拟一个耗时的"设备测试任务"：
    - 点击"开始"：在后台线程里运行一个会持续几秒、分多个步骤汇报进度的
      模拟任务，主窗口在整个过程中保持可操作（可以拖动、可以点取消）。
    - 后台线程只通过 queue.Queue 向主线程"投递消息"（日志/进度/结果/
      异常），绝不直接调用任何 Tkinter 控件的方法。
    - 主线程通过 root.after() 定期（每 50ms）检查队列里有没有新消息，
      有的话才在主线程里更新界面——这是本例唯一允许更新 Widget 的地方。
    - 点击"取消"：设置一个 threading.Event，后台线程在下一个检查点
      看到这个信号后自行提前结束（不是强制杀死线程，见本层 README）。
    - 防止同一任务重复启动：开始按钮在任务运行期间保持禁用。
    - 任务异常会被捕获并通过队列传回主线程展示，不会让后台线程静默崩溃
      或导致整个进程崩溃。
    - 关闭窗口时设置取消信号，并等待后台线程退出后才真正销毁窗口。
"""

from __future__ import annotations

import queue
import random
import threading
import time
import tkinter as tk
from dataclasses import dataclass
from tkinter import ttk
from typing import Literal

POLL_INTERVAL_MS = 50
TOTAL_STEPS = 8


# ---------------------------------------------------------------------------
# 消息协议：后台线程和主线程之间，只允许通过这些数据结构交流。
#
# 用一个带 kind 字段的 dataclass 作为统一的消息"信封"，比直接传裸字符串/
# 元组更清晰：主线程的消费者函数可以用 match-case（或 if/elif）清楚地
# 区分每种消息类型，而不需要靠猜测元组的位置含义。
# ---------------------------------------------------------------------------
MessageKind = Literal["log", "progress", "done", "error", "cancelled"]


@dataclass(frozen=True)
class WorkerMessage:
    kind: MessageKind
    payload: object = None


def run_simulated_test(
    cancel_event: threading.Event, out_queue: "queue.Queue[WorkerMessage]"
) -> None:
    """在后台线程里执行的函数。

    硬性约束（本函数严格遵守，也是本层最重要的工程规则）：
        1. 绝不直接引用任何 Tkinter 控件对象或变量对象。
        2. 所有要"告诉 UI 的事情"都通过 out_queue.put(...) 发出去。
        3. 用 try/except/finally 包裹全部逻辑，保证无论成功、异常还是
           被取消，都会有一条"终态消息"（done/error/cancelled）被送达，
           让主线程能够把界面状态恢复到可以重新开始的状态。
        4. 取消信号只是"请求"，由本函数在每个安全点自行检查并决定是否
           提前退出，不是从外部强行杀死这个线程。
    """
    try:
        for step in range(1, TOTAL_STEPS + 1):
            if cancel_event.is_set():
                out_queue.put(WorkerMessage("cancelled", payload=step - 1))
                return

            # 模拟这一步骤需要的耗时工作（真实场景中这里可能是 SSH 命令、
            # HTTP 请求、子进程调用等）。
            time.sleep(0.4)

            if random.random() < 0.05:
                # 模拟一次随机的业务异常，验证异常能正确传回主线程。
                raise RuntimeError(f"第 {step} 步模拟设备响应超时")

            out_queue.put(
                WorkerMessage("log", payload=f"第 {step}/{TOTAL_STEPS} 步完成")
            )
            out_queue.put(WorkerMessage("progress", payload=step))

        out_queue.put(WorkerMessage("done", payload=TOTAL_STEPS))
    except Exception as exc:  # noqa: BLE001 - 故意捕获所有异常传回 UI
        out_queue.put(WorkerMessage("error", payload=str(exc)))


class BackgroundWorkerApp:
    def __init__(self, root: tk.Tk) -> None:
        self.root = root

        self._worker_thread: threading.Thread | None = None
        self._cancel_event: threading.Event | None = None
        self._message_queue: "queue.Queue[WorkerMessage]" = queue.Queue()
        self._poll_after_id: str | None = None

        self._build_widgets()
        self.root.protocol("WM_DELETE_WINDOW", self.on_close)

    def _build_widgets(self) -> None:
        container = ttk.Frame(self.root, padding=16)
        container.pack(fill="both", expand=True)

        self.status_var = tk.StringVar(value="状态：就绪")
        ttk.Label(container, textvariable=self.status_var).pack(anchor="w")

        self.progress = ttk.Progressbar(
            container, maximum=TOTAL_STEPS, length=320, mode="determinate"
        )
        self.progress.pack(fill="x", pady=(8, 8))

        btn_row = ttk.Frame(container)
        btn_row.pack(anchor="w")
        self.start_btn = ttk.Button(btn_row, text="开始测试", command=self.on_start)
        self.start_btn.pack(side="left")
        self.cancel_btn = ttk.Button(
            btn_row, text="取消", command=self.on_cancel, state="disabled"
        )
        self.cancel_btn.pack(side="left", padx=(8, 0))

        log_frame = ttk.LabelFrame(container, text="任务日志", padding=4)
        log_frame.pack(fill="both", expand=True, pady=(12, 0))
        self.log_text = tk.Text(log_frame, height=10, state="disabled", wrap="word")
        self.log_text.pack(fill="both", expand=True)

    # ------------------------------------------------------------------
    # 启动 / 取消
    # ------------------------------------------------------------------
    def on_start(self) -> None:
        if self._worker_thread is not None and self._worker_thread.is_alive():
            # 防止同一任务重复启动：按钮状态本身已经会禁用，这里是双重保险。
            return

        self._cancel_event = threading.Event()
        self._worker_thread = threading.Thread(
            target=run_simulated_test,
            args=(self._cancel_event, self._message_queue),
            daemon=True,  # daemon=True：进程退出时不会被这个线程卡住，
                          # 但正常关闭流程仍然应该主动等待线程结束（见 on_close）。
        )

        self.start_btn.config(state="disabled")
        self.cancel_btn.config(state="normal")
        self.progress.config(value=0)
        self.status_var.set("状态：运行中")
        self._clear_log()
        self._log("任务已启动。")

        self._worker_thread.start()
        self._start_polling()

    def on_cancel(self) -> None:
        if self._cancel_event is not None:
            self._cancel_event.set()
            self.status_var.set("状态：正在取消…")
            self.cancel_btn.config(state="disabled")

    # ------------------------------------------------------------------
    # 队列轮询：唯一允许更新 Widget 的地方
    # ------------------------------------------------------------------
    def _start_polling(self) -> None:
        self._poll_after_id = self.root.after(POLL_INTERVAL_MS, self._poll_queue)

    def _poll_queue(self) -> None:
        """这是本例里唯一一个"后台任务结果"和"Tkinter 控件"相遇的地方。

        它本身运行在主线程（由 after() 调度），所以在这里调用任意
        Widget 的方法都是安全的——这正是生产者（后台线程）/消费者
        （主线程里的这个函数）模型的核心价值：生产者只管生产消息，
        所有真正触碰 UI 的操作集中在消费者这一侧完成。
        """
        drained_any = False
        try:
            while True:
                message = self._message_queue.get_nowait()
                drained_any = True
                self._handle_message(message)
        except queue.Empty:
            pass

        thread_alive = self._worker_thread is not None and self._worker_thread.is_alive()
        if thread_alive:
            self._poll_after_id = self.root.after(POLL_INTERVAL_MS, self._poll_queue)
        else:
            self._poll_after_id = None
            if drained_any:
                # 线程已结束且刚处理完最后一批消息，确保按钮状态被正确恢复
                # （正常情况下 done/error/cancelled 分支已经处理过了，这里
                # 是兜底，防止极端时序下漏掉收尾）。
                self._reset_controls_if_idle()

    def _handle_message(self, message: WorkerMessage) -> None:
        if message.kind == "log":
            self._log(str(message.payload))
        elif message.kind == "progress":
            self.progress.config(value=message.payload)
        elif message.kind == "done":
            self.progress.config(value=message.payload)
            self.status_var.set("状态：完成")
            self._log("任务完成。")
            self._reset_controls_if_idle()
        elif message.kind == "cancelled":
            self.status_var.set("状态：已取消")
            self._log(f"任务已取消（已完成 {message.payload} 步）。")
            self._reset_controls_if_idle()
        elif message.kind == "error":
            self.status_var.set("状态：失败")
            self._log(f"任务异常：{message.payload}")
            self._reset_controls_if_idle()

    def _reset_controls_if_idle(self) -> None:
        self.start_btn.config(state="normal")
        self.cancel_btn.config(state="disabled")

    # ------------------------------------------------------------------
    # 日志区
    # ------------------------------------------------------------------
    def _log(self, text: str) -> None:
        self.log_text.config(state="normal")
        self.log_text.insert("end", text + "\n")
        self.log_text.see("end")
        self.log_text.config(state="disabled")

    def _clear_log(self) -> None:
        self.log_text.config(state="normal")
        self.log_text.delete("1.0", "end")
        self.log_text.config(state="disabled")

    # ------------------------------------------------------------------
    # 关闭
    # ------------------------------------------------------------------
    def on_close(self) -> None:
        """关闭窗口前：设置取消信号，并等待后台线程真正退出。

        这里用一个较短的 join 超时，是为了避免在极端情况下（线程卡在
        某个不可中断的系统调用里）无限期挂起整个关闭流程；真实工程中
        应该同时保证耗时操作本身对取消信号有合理的响应粒度。
        """
        if self._poll_after_id is not None:
            self.root.after_cancel(self._poll_after_id)
            self._poll_after_id = None

        if self._cancel_event is not None:
            self._cancel_event.set()

        if self._worker_thread is not None and self._worker_thread.is_alive():
            self._worker_thread.join(timeout=2.0)

        self.root.destroy()


def build_root_window() -> tk.Tk:
    root = tk.Tk()
    root.title("Background Worker Demo")
    root.geometry("420x380")
    return root


def main() -> None:
    root = build_root_window()
    BackgroundWorkerApp(root)
    root.mainloop()


if __name__ == "__main__":
    main()
```

---

## E. Lifecycle

1. 用户点"开始测试"：`on_start` 创建全新的 `threading.Event` 和 `threading.Thread`，禁用"开始"、启用"取消"，启动线程，启动队列轮询（`_start_polling`）。
2. 后台线程里，`run_simulated_test` 独立运行它自己的循环，`time.sleep(0.4)` 只会阻塞**这个后台线程**，完全不影响主线程继续处理 `mainloop()` 里的事件（用户此时依然可以拖动窗口、点击"取消"）。
3. 每完成一步，后台线程把 `log`/`progress` 消息 `put` 进队列——这个调用立刻返回，不等待任何人来取。
4. 主线程这一侧，`_poll_queue` 每 `POLL_INTERVAL_MS` 被 `after()` 触发一次：用 `while True: get_nowait()` 把队列里当前积压的所有消息一次性取完（可能一次 tick 里有好几条，因为后台线程产生消息的速度和轮询频率不是严格对齐的），逐条分发给 `_handle_message` 更新界面。
5. 如果线程还在跑（`is_alive()` 为真），轮询自己重新调度下一次（和 Layer 6 的"链"模式一样）；如果线程已经结束，停止轮询。
6. 用户点"取消"：只是 `cancel_event.set()`，这个调用立刻返回，不会等待后台线程真正停下来——后台线程会在下一次循环检查点自己决定退出，随后通过队列送回 `cancelled` 消息，主线程收到后才真正把界面恢复到"就绪"状态。
7. 用户关闭窗口：`on_close` 取消轮询任务、设置取消信号、`join(timeout=2.0)` 等待线程结束，最后才 `destroy()`。

---

## F. Key APIs

| API | 作用 | 输入 | 输出 / 副作用 | 常见错误 |
|---|---|---|---|---|
| `threading.Thread(target=fn, args=(...), daemon=True)` | 创建一个新线程 | 要运行的函数和参数 | `.start()` 后 `target` 函数在新线程里执行 | 创建后忘了调用 `.start()`；或者在 `target` 函数里直接操作 Tkinter 控件 |
| `thread.is_alive()` | 判断线程是否仍在运行 | 无 | `bool` | 以为线程对象存在就代表任务还在跑（线程对象本身不会消失，但任务可能早就结束了） |
| `thread.join(timeout=...)` | 阻塞当前线程，等待目标线程结束（或超时） | 可选超时秒数 | 等待期间调用方线程被阻塞 | 在主线程的回调里不加 `timeout` 直接 `join()`，如果后台线程卡住会把界面也一起卡死 |
| `queue.Queue()` | 创建一个线程安全的先进先出队列 | 无 | `.put()`/`.get()`/`.get_nowait()` 跨线程调用都是安全的 | 用普通 `list` 代替它手动加锁，容易漏加锁或锁粒度出错 |
| `q.put(item)` | 向队列尾部放入一个元素 | 任意可序列化/可传递对象 | 不阻塞（默认队列无容量上限） | 把 Tkinter 对象本身放进队列传给另一个线程（即使不在那个线程操作它，混用也容易引发误用） |
| `q.get_nowait()` | 立即尝试取出一个元素，没有就抛 `queue.Empty` | 无 | 取出队首元素，或抛异常 | 不捕获 `queue.Empty` 导致轮询函数在队列空的时候直接崩溃 |
| `threading.Event()` | 一个线程安全的布尔信号 | 无 | `.set()`/`.clear()`/`.is_set()`/`.wait(timeout)` | 以为 `.set()` 会让后台线程立刻停止——它只是改了一个标志位，停止与否取决于后台代码是否检查它 |

---

## G. Common Failure

### G.1 窗口偶尔随机崩溃，报一些奇怪的 Tcl 内部错误，且很难稳定复现

- **现象**：程序运行一段时间后随机崩溃，错误信息和具体业务逻辑看起来毫无关系，有时换台机器/换一次运气就不出现。
- **根本原因**：某处后台线程代码直接调用了 Tkinter 控件的方法（最常见的写法是"图省事"直接在 `target` 函数里 `label.config(text=...)`），违反了 B.2 的线程安全规则。因为数据竞争本身是不确定性的，这类 bug 的现象往往"时而发生、时而不发生"。
- **定位方法**：审查所有 `threading.Thread(target=...)` 指向的函数，确认函数体内（包括它调用的其他函数）是否出现了任何 Tkinter 对象的引用；养成习惯——后台函数的参数列表里只应该出现"纯数据"和线程安全的同步原语（`Event`、`Queue`），不应该出现 Widget 或 Variable。
- **修复方案**：把所有 UI 更新挪到 `_poll_queue`/`_handle_message` 这类运行在主线程的消费函数里，后台函数只通过 `queue.put()` 传递数据。

### G.2 点击"开始"按钮，连续点几次，任务被跑了好几遍

- **现象**：预期一次任务只跑一份数据，实际日志里出现了交叠、重复的进度信息。
- **根本原因**：`on_start` 没有检查上一个线程是否还在运行就直接创建了新线程（这和 Layer 6 的"重复调度 after() 链"是同一类问题，只是换成了线程版本）。
- **定位方法**：在 `on_start` 开头打印 `self._worker_thread` 和它的 `is_alive()` 状态，确认是否在任务进行中被再次触发。
- **修复方案**：`on_start` 开头判断 `is_alive()` 直接 return；同时在 UI 层面把"开始"按钮在任务运行期间设为 `disabled`（本例两道防线都有）。

### G.3 点击"取消"后，界面显示"已取消"，但过几秒后台任务其实还在跑（或者相反：点了取消卡住很久没反应）

- **现象**：点取消后状态提示切换了，但 CPU/网络活动迹象显示后台代码还在继续执行；或者点了取消之后界面提示"正在取消…"卡了很久。
- **根本原因**：`threading.Event` 只是一个信号，后台代码必须在合适的检查点主动查询它。如果后台任务里有一大段连续执行、很久才检查一次 `cancel_event.is_set()` 的代码（比如一次很长的同步网络请求没有设置超时），取消请求会被推迟很久才生效；而“界面提前显示已取消”往往是因为代码在没有收到真正的 `cancelled`/`done`/`error` 终态消息之前，就提前把状态改成了"已取消"（没有遵循 B.5 的终态消息原则）。
- **定位方法**：检查后台函数里两次 `cancel_event.is_set()` 检查之间的最长间隔，评估取消响应的粒度是否可接受；检查主线程是不是只在收到 `cancelled`/`done`/`error` 消息时才改变最终状态，而不是在 `on_cancel()` 里直接改。
- **修复方案**：缩短检查点之间的间隔（如果单步操作本身很长，考虑把它拆成更小的子步骤，或者给底层 I/O 操作设置合理的超时）；界面上"正在取消…"这种中间状态只应该作为提示，真正的最终状态必须等待后台线程通过队列汇报的终态消息。

---

## H. Exercises

1. **基础修改**：给 `WorkerMessage` 增加一种新的消息类型 `"warning"`，当某一步耗时超过 0.35 秒（可以用 `time.monotonic()` 测量这一步实际耗时）时发出，主线程收到后用不同颜色（如橙色）在日志区高亮显示这条记录。预期结果：日志区里偶尔出现橙色的"耗时偏高"提示，不影响任务整体正常完成。
2. **工程向练习**：把 `run_simulated_test` 改造成支持"连续执行 N 轮"：每轮都是一次完整的 8 步任务，轮与轮之间自动开始下一轮，直到用户点取消或者完成了预设的轮数；每轮的结果（成功/失败/取消）都要作为独立的消息传回主线程，并且主线程要能统计"总成功次数/总失败次数"。提示：想清楚这个"连续执行"的循环应该写在后台函数内部，还是应该由主线程在收到每一轮的终态消息后自己决定"是否要再启动一个新线程跑下一轮"——两种设计都可行，但对取消响应及时性和线程生命周期管理的要求不同。预期结果：你能用一两句话说清楚自己选择的方案，以及它和另一种方案相比的权衡（这正是 Layer 8 综合项目里"连续执行多个测试"功能需要做的设计决策）。

---

## I. Summary

本层新增的 Mental Model（也是全教程最核心的一组认知）：

- 任何可能阻塞的耗时工作，都必须挪到后台线程；`after()`（Layer 6）解决"何时执行"，线程解决"耗时工作不阻塞界面"，两者配合而不是互相替代。
- 后台线程和主线程之间唯一安全的通信渠道是线程安全的队列（`queue.Queue`）；后台线程只管生产消息，所有 Tkinter 操作必须收敛到主线程里由 `after()` 触发的消费函数中完成。
- 取消是协作式的（`threading.Event` + 后台代码主动检查），不是强制终止；异常必须在后台函数内部捕获并通过队列回传，否则主线程永远不会知道任务已经失败；无论成功、失败还是取消，都要有明确的终态消息，驱动界面恢复到可以重新开始的状态。

与前序 Layer 的关系：这一层把 Layer 2（事件回调）、Layer 3（变量驱动界面更新）、Layer 6（`after()` 调度）全部串联了起来——`queue.Queue` 的消费函数本质上就是一个被 `after()` 周期性调度的回调，内部按消息类型更新 Variable/Widget，和前面几层学到的机制完全一致，只是多了"消息来自另一个线程"这一个新维度。下一层将把 Layer 0~7 的全部知识整合成一个完整的工程项目。

下一步：[Layer 8：综合项目——自动化测试控制台](../08-test-console-project/README.md)。
