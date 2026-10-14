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
