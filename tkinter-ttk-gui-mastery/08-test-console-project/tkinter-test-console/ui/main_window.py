"""
ui/main_window.py

Layer 8 综合项目：界面层（含 Application Controller 职责）。

架构位置（见根 README 的整体架构图）：

    Tkinter UI -> Application Controller -> Test Service -> Worker Thread
        -> Queue -> Tkinter UI

MainWindow 类同时承担"界面构建/事件绑定"和"协调 Service 层"两件事——
按照本项目的目录约定（app.py / models.py / services/test_service.py /
ui/main_window.py，没有单独的 controller 模块），协调逻辑就放在这里，
但严格遵守一个边界：所有"决定做什么"的业务规则（校验、状态转换、模拟
测试本身）都已经下沉到 models.py / services/test_service.py，这个类里
只有"把结果显示在哪个控件上"这类纯粹的界面编排代码。
"""

from __future__ import annotations

import queue
import tkinter as tk
from datetime import datetime
from tkinter import messagebox, ttk

from models import HistoryEntry, TaskStatus, TestConfig, TestRunResult, TestType
from services.test_service import ServiceMessage, TestService

POLL_INTERVAL_MS = 50

TEST_TYPE_DISPLAY_TO_ENUM = {t.display_name: t for t in TestType}

HISTORY_COLUMNS = ("started_at", "device", "test_type", "status", "pass_count", "fail_count")
HISTORY_HEADINGS = {
    "started_at": "开始时间",
    "device": "设备地址",
    "test_type": "测试类型",
    "status": "最终状态",
    "pass_count": "成功数",
    "fail_count": "失败数",
}


class RunDetailDialog(tk.Toplevel):
    """历史记录详情窗口：纯信息展示，不阻塞主窗口。

    对照 Layer 4 的 Mental Model：这是"仅用于展示信息、不需要阻塞主流程"
    的场景，所以这里**不**调用 grab_set()，用户仍然可以继续操作主窗口。
    """

    def __init__(self, master: tk.Tk, entry: HistoryEntry) -> None:
        super().__init__(master)
        self.title(f"测试详情 - {entry.device_address}")
        self.geometry("420x300")
        self.transient(master)

        container = ttk.Frame(self, padding=12)
        container.pack(fill="both", expand=True)

        summary = (
            f"设备地址：{entry.device_address}\n"
            f"测试类型：{entry.test_type.display_name}\n"
            f"开始时间：{entry.started_at}\n"
            f"结束时间：{entry.finished_at}\n"
            f"最终状态：{entry.final_status.display_name}\n"
            f"成功 / 失败：{entry.pass_count} / {entry.fail_count}"
        )
        ttk.Label(container, text=summary, justify="left").pack(anchor="w", pady=(0, 8))

        tree = ttk.Treeview(
            container, columns=("run", "result", "duration", "message"), show="headings", height=8
        )
        tree.heading("run", text="第几次")
        tree.heading("result", text="结果")
        tree.heading("duration", text="耗时")
        tree.heading("message", text="说明")
        tree.column("run", width=50, anchor="center")
        tree.column("result", width=60, anchor="center")
        tree.column("duration", width=70, anchor="center")
        tree.column("message", width=180, anchor="w")
        tree.pack(fill="both", expand=True)

        for run in entry.runs:
            tree.insert(
                "",
                "end",
                values=(
                    run.run_index,
                    "PASS" if run.passed else "FAIL",
                    f"{run.duration_ms} ms",
                    run.message,
                ),
            )

        ttk.Button(container, text="关闭", command=self.destroy).pack(pady=(8, 0))


class MainWindow:
    def __init__(self, root: tk.Tk) -> None:
        self.root = root
        self.service = TestService()

        self.status = TaskStatus.IDLE

        # 当前批次的中间状态：批次运行期间逐步积累，批次结束时打包成
        # 一条 HistoryEntry。这些字段本身不是 Tkinter Variable，因为它们
        # 不需要被任何控件直接绑定展示，只是控制器内部的簿记状态。
        self._current_config: TestConfig | None = None
        self._current_runs: list[TestRunResult] = []
        self._current_started_at: str | None = None

        self.history: list[HistoryEntry] = []
        self._next_history_id = 1
        self._history_iid_to_id: dict[str, int] = {}

        self._poll_after_id: str | None = None

        self._build_widgets()
        self.root.protocol("WM_DELETE_WINDOW", self.on_close)

    # ------------------------------------------------------------------
    # UI 构建
    # ------------------------------------------------------------------
    def _build_widgets(self) -> None:
        notebook = ttk.Notebook(self.root)
        notebook.pack(fill="both", expand=True, padx=8, pady=8)

        control_page = ttk.Frame(notebook, padding=12)
        notebook.add(control_page, text="测试控制")
        self._build_control_page(control_page)

        history_page = ttk.Frame(notebook, padding=12)
        notebook.add(history_page, text="历史记录")
        self._build_history_page(history_page)

    def _build_control_page(self, parent: ttk.Frame) -> None:
        parent.columnconfigure(1, weight=1)

        # --- 表单：设备地址 / 测试类型 / 测试次数 ---
        self.device_var = tk.StringVar(value="")
        self.test_type_display_var = tk.StringVar(value=TestType.QUICK.display_name)
        self.repeat_var = tk.IntVar(value=3)

        ttk.Label(parent, text="设备地址：").grid(row=0, column=0, sticky="w", pady=4)
        ttk.Entry(parent, textvariable=self.device_var).grid(
            row=0, column=1, sticky="ew", pady=4
        )

        ttk.Label(parent, text="测试类型：").grid(row=1, column=0, sticky="w", pady=4)
        ttk.Combobox(
            parent,
            textvariable=self.test_type_display_var,
            values=[t.display_name for t in TestType],
            state="readonly",
        ).grid(row=1, column=1, sticky="ew", pady=4)

        ttk.Label(parent, text="测试次数：").grid(row=2, column=0, sticky="w", pady=4)
        ttk.Spinbox(
            parent, from_=1, to=50, textvariable=self.repeat_var, width=8
        ).grid(row=2, column=1, sticky="w", pady=4)

        # --- 按钮 ---
        btn_row = ttk.Frame(parent)
        btn_row.grid(row=3, column=0, columnspan=2, sticky="w", pady=(8, 8))
        self.start_btn = ttk.Button(btn_row, text="开始测试", command=self.on_start)
        self.start_btn.pack(side="left")
        self.cancel_btn = ttk.Button(
            btn_row, text="取消", command=self.on_cancel, state="disabled"
        )
        self.cancel_btn.pack(side="left", padx=(8, 0))

        # --- 状态 / 进度 / 计数 ---
        status_row = ttk.Frame(parent)
        status_row.grid(row=4, column=0, columnspan=2, sticky="ew", pady=(0, 4))
        self.status_var = tk.StringVar(value=f"状态：{TaskStatus.IDLE.display_name}")
        ttk.Label(status_row, textvariable=self.status_var).pack(side="left")

        self.count_var = tk.StringVar(value="成功 0 / 失败 0")
        ttk.Label(status_row, textvariable=self.count_var).pack(side="right")

        self.progress = ttk.Progressbar(parent, mode="determinate", maximum=1)
        self.progress.grid(row=5, column=0, columnspan=2, sticky="ew", pady=(0, 8))

        # --- 日志区 ---
        log_frame = ttk.LabelFrame(parent, text="运行日志", padding=4)
        log_frame.grid(row=6, column=0, columnspan=2, sticky="nsew")
        parent.rowconfigure(6, weight=1)
        log_frame.columnconfigure(0, weight=1)
        log_frame.rowconfigure(0, weight=1)

        self.log_text = tk.Text(log_frame, height=10, state="disabled", wrap="word")
        log_scroll = ttk.Scrollbar(log_frame, orient="vertical", command=self.log_text.yview)
        self.log_text.configure(yscrollcommand=log_scroll.set)
        self.log_text.grid(row=0, column=0, sticky="nsew")
        log_scroll.grid(row=0, column=1, sticky="ns")

    def _build_history_page(self, parent: ttk.Frame) -> None:
        parent.columnconfigure(0, weight=1)
        parent.rowconfigure(0, weight=1)

        self.history_tree = ttk.Treeview(
            parent, columns=HISTORY_COLUMNS, show="headings", selectmode="browse", height=10
        )
        for col in HISTORY_COLUMNS:
            self.history_tree.heading(col, text=HISTORY_HEADINGS[col])
            self.history_tree.column(col, width=100, anchor="center")
        self.history_tree.column("device", width=140, anchor="w")

        history_scroll = ttk.Scrollbar(
            parent, orient="vertical", command=self.history_tree.yview
        )
        self.history_tree.configure(yscrollcommand=history_scroll.set)
        self.history_tree.grid(row=0, column=0, sticky="nsew")
        history_scroll.grid(row=0, column=1, sticky="ns")

        ttk.Button(parent, text="查看详情", command=self.on_view_history_detail).grid(
            row=1, column=0, sticky="w", pady=(8, 0)
        )

    # ------------------------------------------------------------------
    # 启动 / 取消
    # ------------------------------------------------------------------
    def on_start(self) -> None:
        if self.service.is_running():
            # 双重保险：按钮状态本身已经会禁用，这里再做一次防御性检查。
            return

        test_type = TEST_TYPE_DISPLAY_TO_ENUM[self.test_type_display_var.get()]
        config = TestConfig(
            device_address=self.device_var.get(),
            test_type=test_type,
            repeat_count=self.repeat_var.get(),
        )

        try:
            self.service.start(config)
        except ValueError as exc:
            # TestConfig.validate() 抛出的校验错误，直接展示给用户，
            # 不启动任何后台线程，也不改变任务状态。
            messagebox.showerror("参数校验失败", str(exc))
            return
        except RuntimeError as exc:
            messagebox.showerror("无法启动", str(exc))
            return

        self._current_config = config
        self._current_runs = []
        self._current_started_at = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

        self._set_status(TaskStatus.RUNNING)
        self.progress.config(value=0, maximum=max(config.repeat_count, 1))
        self.count_var.set("成功 0 / 失败 0")
        self._clear_log()
        self._log(f"批次已启动：{config.device_address}，共 {config.repeat_count} 次。")

        self._start_polling()

    def on_cancel(self) -> None:
        if not self.service.is_running():
            return
        self.service.cancel()
        self._set_status(TaskStatus.CANCELLING)

    # ------------------------------------------------------------------
    # 队列轮询：唯一允许更新 Widget 的地方（见 Layer 7）
    # ------------------------------------------------------------------
    def _start_polling(self) -> None:
        self._poll_after_id = self.root.after(POLL_INTERVAL_MS, self._poll_queue)

    def _poll_queue(self) -> None:
        try:
            while True:
                message = self.service.messages.get_nowait()
                self._handle_message(message)
        except queue.Empty:
            pass

        if self.service.is_running():
            self._poll_after_id = self.root.after(POLL_INTERVAL_MS, self._poll_queue)
        else:
            self._poll_after_id = None

    def _handle_message(self, message: ServiceMessage) -> None:
        if message.kind == "log":
            self._log(str(message.payload))

        elif message.kind == "run_progress":
            info = message.payload
            self.status_var.set(
                f"状态：{self.status.display_name}"
                f"（第 {info['run_index']}/{self._current_config.repeat_count} 次，"
                f"步骤 {info['step']}/{info['total_steps']}）"
            )

        elif message.kind == "run_result":
            result: TestRunResult = message.payload  # type: ignore[assignment]
            self._current_runs.append(result)
            self.progress.config(value=len(self._current_runs))
            passed = sum(1 for r in self._current_runs if r.passed)
            failed = len(self._current_runs) - passed
            self.count_var.set(f"成功 {passed} / 失败 {failed}")

        elif message.kind == "batch_done":
            self._finalize_batch(TaskStatus.DONE)
            self._log("批次全部完成。")

        elif message.kind == "batch_cancelled":
            self._finalize_batch(TaskStatus.CANCELLED)
            self._log(f"批次已取消（已完成 {message.payload} 次）。")

        elif message.kind == "batch_error":
            self._finalize_batch(TaskStatus.FAILED)
            self._log(f"批次异常终止：{message.payload}")
            messagebox.showerror("测试批次异常", str(message.payload))

    def _finalize_batch(self, final_status: TaskStatus) -> None:
        """批次结束（无论成功/取消/失败）的统一收尾：

        1. 把这次批次打包成一条 HistoryEntry，加入历史记录并刷新表格。
        2. 恢复状态为 final_status，并把控件状态恢复到可以重新开始。

        这是 Layer 7 "无论成功、失败还是取消都要有终态消息驱动界面恢复"
        这条规则，在综合项目里的具体实现。
        """
        if self._current_config is not None:
            entry = HistoryEntry(
                entry_id=self._next_history_id,
                device_address=self._current_config.device_address,
                test_type=self._current_config.test_type,
                started_at=self._current_started_at or "",
                finished_at=datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
                final_status=final_status,
                runs=list(self._current_runs),
            )
            self._next_history_id += 1
            self.history.append(entry)
            self._refresh_history_table()

        self._set_status(final_status)
        self.start_btn.config(state="normal")
        self.cancel_btn.config(state="disabled")

    # ------------------------------------------------------------------
    # 历史记录（Treeview，参考 Layer 5）
    # ------------------------------------------------------------------
    def _refresh_history_table(self) -> None:
        self.history_tree.delete(*self.history_tree.get_children())
        self._history_iid_to_id.clear()
        for entry in self.history:
            iid = self.history_tree.insert("", "end", values=entry.as_row())
            self._history_iid_to_id[iid] = entry.entry_id

    def on_view_history_detail(self) -> None:
        selected = self.history_tree.selection()
        if not selected:
            messagebox.showinfo("查看详情", "请先在列表中选中一条历史记录。")
            return

        entry_id = self._history_iid_to_id.get(selected[0])
        entry = next((e for e in self.history if e.entry_id == entry_id), None)
        if entry is None:
            return

        RunDetailDialog(self.root, entry)

    # ------------------------------------------------------------------
    # 状态 / 日志辅助方法
    # ------------------------------------------------------------------
    def _set_status(self, status: TaskStatus) -> None:
        self.status = status
        self.status_var.set(f"状态：{status.display_name}")
        if status.is_active:
            self.start_btn.config(state="disabled")
            self.cancel_btn.config(state="normal" if status is TaskStatus.RUNNING else "disabled")
        else:
            self.start_btn.config(state="normal")
            self.cancel_btn.config(state="disabled")

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
        if self._poll_after_id is not None:
            self.root.after_cancel(self._poll_after_id)
            self._poll_after_id = None

        # 请求取消并等待后台线程真正结束，避免窗口销毁后线程仍在跑
        # （或者线程结束后试图访问已经不存在的队列消费者）。
        self.service.shutdown(timeout=2.0)

        self.root.destroy()
