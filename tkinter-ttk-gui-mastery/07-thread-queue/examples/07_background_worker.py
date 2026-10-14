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
