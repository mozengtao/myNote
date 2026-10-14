# Layer 6：`after()` 与定时任务

> 对应示例代码：[examples/06_after_timer.py](examples/06_after_timer.py)

---

## A. Why

很多界面需要"过一段时间自动做点什么"：刷新一个时钟显示、周期性轮询某个状态、延迟执行一个操作。初学者的第一反应往往是 `time.sleep()`，但这是 GUI 编程里最经典的反模式之一——它会直接冻结整个事件循环。这一层要建立的核心认知是：`after()` 提供了一种"在事件循环内部排队未来要执行的回调"的机制，它不是线程，也不是定时器中断，而是事件循环自身调度能力的延伸。

---

## B. Mental Model

### B.1 `after()` 在事件循环里的位置

```text
mainloop()
   |
   +-- process event          等待/处理一个事件（鼠标、键盘、系统消息……）
   |
   +-- execute callback        执行这个事件对应的回调（可能是 command=，
   |                           也可能是到期的 after() 回调）
   |
   +-- schedule future callback via after()
   |                           回调内部可以调用 after() 排一个"未来"回调
   |
   +-- continue processing events   继续处理下一个事件/到期任务
```

一句话 Mental Model：

> **`after(delay_ms, callback)` 并不是"启动了一个独立的计时器线程"，而是告诉事件循环："再过至少 `delay_ms` 毫秒，当你下一次检查到期任务时，请把 `callback` 当作一个普通事件来处理。"`callback` 仍然和所有其他回调一样，在主线程里顺序执行，不会和其他事件处理交叠。**

这解释了两个常见疑惑：
1. **为什么 `after()` 的延迟不精确？** 因为它只保证"不早于 `delay_ms`"，如果主线程此刻正在处理别的耗时回调，`after()` 回调会被推迟到那之后才执行。
2. **为什么 `after()` 调度的回调不会和用户点击"同时"发生？** 因为单线程事件循环任何时刻只做一件事，`after()` 回调和按钮点击回调一样，都要排队等待被处理。

### B.2 为什么本例用 `time.monotonic()` 而不是直接数 tick 次数

如果简单地用"每次 tick 加 100ms"来估算流逝时间，会积累系统调度误差（真实的 tick 间隔几乎总是比 100ms 略长）。更稳健的做法是记录"这一段计时开始的真实时间点"（`time.monotonic()`，不受系统时间被用户/NTP 修改影响），每次 tick 用"当前时间 - 开始时间"算出精确的流逝时长。`after()` 本身只负责"大致每隔多久触发一次检查"，真正的时间计算交给 `time.monotonic()`。

### B.3 为什么 `after()` 不会把函数自动放进后台线程

这是本层最重要的一条边界：**`after()` 调度的回调，运行时机仍然是"事件循环轮到它的时候"，运行线程仍然是主线程**。它只是延迟了"什么时候执行"，完全没有改变"在哪个线程执行"。如果你在 `after()` 回调里写了一段耗时的同步计算（比如真的网络请求），这段计算照样会阻塞主线程、冻住窗口——和直接在 `command=` 回调里写一样危险。`after()` 解决的是"定时/周期执行"的问题，不解决"耗时任务不阻塞界面"的问题，后者需要 Layer 7 的线程 + 队列方案。

### B.4 为什么不能用 `time.sleep()` 模拟 GUI 定时器

```text
错误写法：
    def on_start_clicked():
        for i in range(10):
            time.sleep(1)              # 阻塞了调用方——也就是主线程！
            label.config(text=str(i))  # 这行代码甚至不会被看到执行，
                                        # 因为窗口在整个 for 循环期间完全冻结，
                                        # 不会重绘，用户的任何点击/拖动都被
                                        # 操作系统认为"程序未响应"。
```

`time.sleep()` 会让当前线程（这里是运行 `mainloop()` 的主线程）彻底停下来，`mainloop()` 没有机会处理任何新的事件——包括重绘窗口和响应用户输入。正确的"等待一段时间后做某事"永远应该用 `after()`，让"等待"这件事交还给事件循环去调度，而不是用 `sleep()` 把执行流程卡死在当前这一帧。

---

## C. Architecture

| 对象 | 角色 |
|---|---|
| `self._after_id` | 当前唯一一条 tick 链的任务句柄；`None` 表示没有任务在排队 |
| `self._running` | 计时器是否处于"运行中"状态 |
| `self._tick_started_at` | 本次运行段开始的真实时间戳（`time.monotonic()`） |
| `self._elapsed_ms` | 之前所有已完成运行段累计的流逝时长 |
| `schedule_tick()` / `cancel_tick()` | 成对的调度/取消方法，是整个计时器稳定运行的关键 |

---

## D. Complete Code

```python
"""
06_after_timer.py

Layer 6：after() 与定时任务。

运行方式：
    python3 examples/06_after_timer.py

本例实现一个简单的秒表：
    - 开始：每 100ms 刷新一次已用时间显示。
    - 暂停：取消尚未执行的 after() 回调，停止计时但保留当前已用时间。
    - 重置：停止计时并把已用时间清零。

关键设计点：
    1. 只保存"当前唯一一条"after() 任务的 id（self._after_id），
       开始计时前先检查是否已经在运行，避免重复调度出多条并行的
       计时链（这是本层最容易踩的坑）。
    2. 关闭窗口时，用 WM_DELETE_WINDOW 协议回调主动 cancel 掉尚未
       执行的 after() 任务，避免"窗口已经不存在了，但还有一个
       after() 回调将在未来某个时刻尝试更新一个不存在的控件"。
    3. 绝不使用 time.sleep() 来实现这个定时效果（会阻塞事件循环，
       导致窗口在睡眠期间完全无响应）。
"""

from __future__ import annotations

import time
import tkinter as tk
from tkinter import ttk

TICK_INTERVAL_MS = 100  # 每 100 毫秒刷新一次显示


class StopwatchApp:
    def __init__(self, root: tk.Tk) -> None:
        self.root = root

        # 计时状态：全部是普通 Python 属性，不涉及后台线程，
        # 所以在这一层完全不需要考虑"线程安全"问题——
        # after() 调度的回调始终在主线程里执行（见本层 README B.3）。
        self._running = False
        self._elapsed_ms = 0
        self._tick_started_at: float | None = None  # time.monotonic() 时间戳

        # 保存 after() 返回的任务 id，这是实现"不重复调度"和
        # "可取消"的关键——没有这个 id，你无法取消一个已经排队的回调。
        self._after_id: str | None = None

        self._build_widgets()

        # 捕获窗口关闭请求，确保退出前取消任何挂起的 after() 任务。
        self.root.protocol("WM_DELETE_WINDOW", self.on_close)

    def _build_widgets(self) -> None:
        container = ttk.Frame(self.root, padding=20)
        container.pack(fill="both", expand=True)

        self.time_var = tk.StringVar(value="00.0 s")
        ttk.Label(
            container, textvariable=self.time_var, font=("TkDefaultFont", 28)
        ).pack(pady=(0, 16))

        btn_row = ttk.Frame(container)
        btn_row.pack()
        self.start_btn = ttk.Button(btn_row, text="开始", command=self.on_start)
        self.start_btn.pack(side="left")
        self.pause_btn = ttk.Button(
            btn_row, text="暂停", command=self.on_pause, state="disabled"
        )
        self.pause_btn.pack(side="left", padx=(8, 0))
        ttk.Button(btn_row, text="重置", command=self.on_reset).pack(
            side="left", padx=(8, 0)
        )

    # ------------------------------------------------------------------
    # 计时核心：schedule_tick / cancel_tick 成对出现
    # ------------------------------------------------------------------
    def schedule_tick(self) -> None:
        """调度下一次 tick。

        重要：每次只允许存在一条"链"。调用前不检查、不取消旧的
        after() 任务的话，重复点击"开始"会在后台堆积出多条并行的
        tick 链，表现为计时器"越点越快"（因为每一条链都在各自更新
        同一个 elapsed_ms，相当于时间被算了好几遍）。
        """
        self._after_id = self.root.after(TICK_INTERVAL_MS, self.on_tick)

    def cancel_tick(self) -> None:
        """取消尚未执行的 tick（如果存在）。

        after_cancel() 只能取消"还没被执行"的回调；已经在执行中的
        回调无法从中途打断（这符合单线程事件循环的本质——它本来就是
        顺序执行的，不存在"正在执行的函数被另一个事件打断"的情况）。
        """
        if self._after_id is not None:
            self.root.after_cancel(self._after_id)
            self._after_id = None

    def on_tick(self) -> None:
        """每 TICK_INTERVAL_MS 毫秒执行一次。

        这是本层 README 里" mainloop() 不断处理事件、执行回调、调度
        未来回调"这条主线的具体体现：on_tick 本身执行得很快（只是算一下
        时间差、更新一下 StringVar），然后立刻调度下一次 tick，
        整个过程不会阻塞事件循环处理其他事件（比如用户点击"暂停"）。
        """
        if not self._running or self._tick_started_at is None:
            return

        now = time.monotonic()
        delta_ms = int((now - self._tick_started_at) * 1000)
        self._update_display(self._elapsed_ms + delta_ms)

        self.schedule_tick()

    def _update_display(self, total_ms: int) -> None:
        seconds = total_ms / 1000
        self.time_var.set(f"{seconds:05.1f} s")

    # ------------------------------------------------------------------
    # 回调
    # ------------------------------------------------------------------
    def on_start(self) -> None:
        if self._running:
            # 防止重复点击"开始"导致重复调度（见 schedule_tick 的说明）。
            return

        self._running = True
        self._tick_started_at = time.monotonic()
        self.start_btn.config(state="disabled")
        self.pause_btn.config(state="normal")
        self.schedule_tick()

    def on_pause(self) -> None:
        if not self._running:
            return

        # 暂停前，先把"已经流逝的时间"累加进 elapsed_ms，再取消未来的 tick。
        now = time.monotonic()
        if self._tick_started_at is not None:
            self._elapsed_ms += int((now - self._tick_started_at) * 1000)
        self._tick_started_at = None
        self._running = False

        self.cancel_tick()
        self.start_btn.config(state="normal")
        self.pause_btn.config(state="disabled")

    def on_reset(self) -> None:
        self.cancel_tick()
        self._running = False
        self._tick_started_at = None
        self._elapsed_ms = 0
        self._update_display(0)
        self.start_btn.config(state="normal")
        self.pause_btn.config(state="disabled")

    def on_close(self) -> None:
        """窗口关闭前，主动取消尚未执行的 after() 任务。

        如果不这么做：窗口 destroy() 之后，之前排队的 on_tick 仍然
        会在未来某个时刻尝试访问 self.time_var / self.root 等已经被
        销毁的对象，引发异常（在某些 Tkinter 版本下只是打印一段
        TclError 到控制台，但这属于"未清理的后台工作"，应当避免）。
        """
        self.cancel_tick()
        self.root.destroy()


def build_root_window() -> tk.Tk:
    root = tk.Tk()
    root.title("after() Timer Demo")
    root.geometry("280x160")
    root.resizable(False, False)
    return root


def main() -> None:
    root = build_root_window()
    StopwatchApp(root)
    root.mainloop()


if __name__ == "__main__":
    main()
```

---

## E. Lifecycle

1. `StopwatchApp.__init__` 初始化计时状态（全部"空闲"），注册窗口关闭协议。
2. 用户点"开始"：`on_start` 置位 `_running=True`，记录起始时间点，调用 `schedule_tick()` 排出第一条 tick。
3. 大约 100ms 后，事件循环发现这个 `after()` 任务到期，调用 `on_tick()`：计算并显示流逝时间，然后**在 `on_tick()` 内部**再调用一次 `schedule_tick()` 排出下一条——这就是"链"的由来：每次 tick 都负责排出下一次 tick，而不是一开始就排好 N 次。
4. 用户点"暂停"：`on_pause` 把当前运行段的时长累加进 `_elapsed_ms`，`cancel_tick()` 取消掉下一条还没触发的 tick，链在这里断开。
5. 用户点"重置"：取消任何挂起任务，清零所有状态。
6. 用户关闭窗口：`on_close` 先 `cancel_tick()` 清理掉可能还排着的任务，再 `root.destroy()`。

---

## F. Key APIs

| API | 作用 | 输入 | 输出 / 副作用 | 常见错误 |
|---|---|---|---|---|
| `widget.after(delay_ms, callback, *args)` | 在事件循环中排队一个未来将被调用的回调 | 延迟毫秒数、无参或带参的函数 | 返回一个任务 id（字符串），用于之后取消 | 丢弃返回值导致之后无法取消；以为 `delay_ms` 是精确延迟 |
| `widget.after_cancel(after_id)` | 取消一个尚未执行的 `after()` 任务 | `after()` 返回的 id | 该任务不会再被执行 | 对一个已经执行过的 id 调用（通常是无害的空操作，但说明没有正确追踪状态）；或者传了 `None` 导致异常 |
| `widget.after_idle(callback)` | 等事件队列暂时空闲时执行一次（不等固定延迟） | 回调函数 | 常用于"把一次性收尾工作延后到当前事件处理完之后" | 和 `after(0, ...)` 混淆：两者效果相近但语义不同，`after_idle` 更强调"等当前这批事件处理完" |
| `widget.protocol("WM_DELETE_WINDOW", callback)` | 捕获窗口管理器的关闭请求（点击标题栏 X） | 回调函数（无参） | 用户点击关闭按钮时调用 `callback`，而不是直接销毁窗口 | 不设置的话，点击 X 会立即销毁窗口，来不及做任何清理（取消 after()、关闭文件、通知后台线程等） |
| `time.monotonic()` | 获取一个不受系统时间调整影响的单调递增时间戳 | 无 | 浮点秒数，只能用于计算差值 | 用 `time.time()` 代替它来做时长计算，可能因为系统时间被 NTP 调整而出现负数或跳变 |

---

## G. Common Failure

### G.1 计时器"越点越快"

- **现象**：点了几次"开始"（哪怕中间没点"暂停"），显示的时间增长速度变成了原来的 2 倍、3 倍。
- **根本原因**：`on_start` 没有检查 `self._running` 就直接调用了 `schedule_tick()`，导致同时存在多条并行的 tick 链，每条链都在各自调用 `_update_display()`，相当于同一段时间被重复计算了多次。
- **定位方法**：在 `schedule_tick()` 里打印 `self._after_id`，观察是否同一时刻有多个不同的 id 在"活跃"。
- **修复方案**：`on_start` 开头判断 `if self._running: return`，确保同一时间只有一条链在跑（本例已经这么写）。

### G.2 关闭窗口后，终端里打印了一堆 Tcl 相关的报错

- **现象**：程序本身看起来运行正常，但关闭窗口的瞬间或之后，控制台出现 `invalid command name ".!label"` 之类的报错。
- **根本原因**：窗口已经被 `destroy()`，但之前排队的 `after()` 回调还没来得及取消，到期后尝试访问已经被销毁的 Widget（比如 `self.time_var.set(...)` 背后其实还是在操作一个 Tcl 对象，这个对象已经不存在了）。
- **定位方法**：确认 `root.protocol("WM_DELETE_WINDOW", ...)` 是否正确设置，并且这个回调里确实调用了 `cancel_tick()`（或等价的清理逻辑）。
- **修复方案**：任何注册了周期性 `after()` 的组件，都要在窗口/组件销毁前主动取消，这是一个必须养成的习惯性配对动作（"调度就要有对应的取消路径"）。

### G.3 窗口在某个操作期间完全无法拖动/点击

- **现象**：点击某个按钮后，窗口整体"冻住"几秒钟，鼠标点哪都没反应，过几秒又恢复正常。
- **根本原因**：那个按钮的回调里混入了 `time.sleep()` 或其他长时间同步阻塞的调用（哪怕只是想"模拟延迟"），这会直接冻结 `mainloop()`。
- **定位方法**：在怀疑的回调函数开头和结尾打印时间戳，确认是不是这段代码本身执行时间过长；检查代码里是否有 `time.sleep()`、同步网络请求、大文件同步读写等。
- **修复方案**：如果确实需要"过一会儿做点什么"，改用 `after()`；如果是真正耗时的操作（网络、子进程、大量计算），需要用 Layer 7 的线程 + 队列方案，而不是在主线程里硬等。

---

## H. Exercises

1. **基础修改**：给秒表加一个"圈数"（lap）按钮，点击时把当前显示的时间记录到一个列表里（并打印或展示出来），但不影响计时器本身的运行状态。预期结果：计时器持续运行的同时，可以多次记录"圈数时间点"。
2. **工程向练习**：把 `TICK_INTERVAL_MS` 改成可以在运行时通过一个 `ttk.Combobox`（候选值 50/100/250/500ms）动态调整，调整后新的间隔要在**下一次** tick 生效，而不需要用户先暂停再开始。提示：这需要你思考 `schedule_tick()` 读取间隔值的时机——它应该每次调用时读取"当前"的间隔配置，而不是在计时开始时固定下来。预期结果：计时运行中切换刷新间隔，显示刷新的频率立刻改变，已经累计的时间不受影响。

---

## I. Summary

本层新增的 Mental Model：

- `after()` 是事件循环自身的调度能力，不是线程，不会改变回调的执行线程；它解决的是"定时/延迟/周期执行"的问题，不解决"耗时任务阻塞界面"的问题。
- 任何通过"回调里再次调用 `after()`"形成的调度链，都必须保存任务 id、提供显式的取消路径，并在不再需要时（暂停、重置、窗口关闭）主动取消——这是避免重复调度和"僵尸回调"的唯一可靠手段。
- `time.sleep()` 在 GUI 回调里永远是错误答案，任何"等一等再做事"的需求都应该用 `after()` 实现。

与前序 Layer 的关系：本层第一次引入了"时间"和"状态随时间演化"的概念，为 Layer 7 的后台任务铺垫了关键认知——`after()` 本身跑在主线程里，而真正的"耗时工作"必须挪到别的地方去做，这正是下一层要解决的问题。

下一步：[Layer 7：后台线程与线程安全](../07-thread-queue/README.md)。
