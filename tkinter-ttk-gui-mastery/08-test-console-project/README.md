# Layer 8：综合项目——自动化测试控制台

> 对应项目代码：[tkinter-test-console/](tkinter-test-console/)

---

## A. Why

前面七层分别建立了"骨架"（Layer 0/1）、"交互"（Layer 2/3）、"多窗口组织"（Layer 4）、"批量数据展示"（Layer 5）、"定时任务"（Layer 6）、"后台线程+线程安全"（Layer 7）七块独立的 Mental Model。真实工程不会把这些能力分散在七个互不相关的脚本里，而是要把它们组织成一个有清晰分层、职责单一、可维护的应用。这一层的目标，就是把前七层的全部知识，整合成一个具备实际工程价值的小工具——一个可以对设备进行批量模拟测试、展示进度和历史记录的"测试控制台"。

---

## B. Mental Model

### B.1 整体架构（对照各层职责）

```text
Tkinter UI
    |
    v
Application Controller
    |
    v
Test Service
    |
    v
Worker Thread
    |
    v
Queue
    |
    v
Tkinter UI
```

这正是 Layer 7 的生产者-消费者模型，加上一层"控制器"职责（校验表单、决定何时启动、把 Service 层的结果转换成历史记录）。在这个项目里，控制器职责由 `ui/main_window.py` 里的 `MainWindow` 类承担（见 D 节的目录说明，为什么没有单独的 controller 模块）。

### B.2 四个文件各自的职责边界

```text
app.py                     只做"组装"：创建窗口、创建 MainWindow、启动 mainloop()。
models.py                  纯数据：TestConfig / TaskStatus / TestRunResult / HistoryEntry。
                            不 import tkinter，不包含任何"怎么跑测试"的逻辑。
services/test_service.py   纯业务：怎么模拟一次测试、怎么连续跑多次、
                            怎么响应取消、怎么捕获异常。不 import tkinter。
ui/main_window.py           界面 + 协调：构建控件、绑定事件、
                            轮询 Service 的消息队列、把结果显示到界面上。
```

这个边界不是形式主义的拆分，而是直接决定了"哪些代码可以脱离 GUI 环境被测试"：`models.py` 和 `services/test_service.py` 完全不依赖 Tkinter，可以在没有任何图形界面的 CI 环境里跑单元测试（本层开发过程中实际做法：先独立验证这两个模块的逻辑完全正确，再去写界面层，这也是本教程推荐的开发顺序）。

### B.3 任务状态机（在 `models.TaskStatus` 中显式建模）

```text
        start()
IDLE ----------------> RUNNING
                          |   \
                          |    \ cancel()
                          |     v
                 全部完成  |   CANCELLING
                          |     |
                          |     | 后台确认已停止
                          v     v
                        DONE  CANCELLED
                          \     |
             （用户再次点击"开始"，可从 DONE / CANCELLED / FAILED 回到 IDLE）
                          v
                        IDLE

出现未捕获异常：RUNNING -> FAILED -> （用户再次点击"开始"）-> IDLE
```

这张状态机图直接对应根 README 的"任务状态机图"，差别是这里把状态**显式定义成了一个枚举类型**（`TaskStatus`），而不是用散落的字符串或布尔值组合去表示"当前处于什么阶段"——这是从教学示例走向工程代码的一个关键转变：状态转换规则应该集中在一处可查，而不是分散推断。

### B.4 为什么没有单独的 `controller.py`

严格按照 Prompt 给出的目录结构（`app.py` / `models.py` / `services/test_service.py` / `ui/main_window.py`），没有单独的 controller 模块。协调职责（校验表单、决定何时启动任务、把 Service 结果打包成历史记录）就放在 `MainWindow` 类里。这里的边界原则是：**`MainWindow` 只负责"编排"（调用谁、什么时候调用、结果显示在哪个控件），不负责"决策"（校验规则在 `TestConfig.validate()`，模拟测试本身在 `test_service.py`）**。如果未来这个项目继续增长，协调逻辑变得复杂（比如要支持多设备并发测试队列），拆出一个独立的 `controller.py` 是合理的下一步演进方向，但对于这个教学规模的项目，不引入额外模块更清晰。

---

## C. Architecture

```text
tkinter-test-console/
├── app.py                       入口：组装 root + MainWindow + mainloop()
├── models.py                    TestType / TaskStatus / TestConfig /
│                                 TestRunResult / HistoryEntry
├── services/
│   ├── __init__.py
│   └── test_service.py          ServiceMessage / run_test_batch() / TestService
└── ui/
    ├── __init__.py
    └── main_window.py           MainWindow（控件构建 + 事件绑定 + 协调）
                                  RunDetailDialog（历史记录详情，非模态 Toplevel）
```

| 对象 | 角色 |
|---|---|
| `TestConfig` | 一次批次测试的配置（设备地址/测试类型/次数），带 `validate()` |
| `TaskStatus` | 任务状态机枚举，带 `is_active`/`display_name` |
| `TestService` | Controller 用来控制后台任务生命周期的句柄：`start()`/`cancel()`/`is_running()`/`shutdown()` |
| `run_test_batch()` | 在后台线程中执行的纯函数，连续跑 `repeat_count` 次模拟测试 |
| `HistoryEntry` | 一个完整批次的历史记录（Treeview 的数据模型，参考 Layer 5） |
| `MainWindow` | UI 构建 + 事件绑定 + Controller 协调逻辑 |
| `RunDetailDialog` | 查看历史记录详情的非模态 `Toplevel`（参考 Layer 4） |

---

## D. Complete Code

### D.1 [models.py](tkinter-test-console/models.py)

```python
"""
models.py

Layer 8 综合项目：数据模型层。

这个模块定义了整个应用需要的所有数据结构：
    - TestType / TaskStatus：枚举，描述"测试类型"和"任务状态机"。
    - TestConfig：一次测试批次的配置（设备地址、测试类型、重复次数）。
    - TestRunResult：单次测试运行的结果。
    - HistoryEntry：一个完整测试批次的历史记录（包含它所有的单次结果）。

本模块不 import 任何 Tkinter 相关的东西，也不包含任何业务逻辑（不模拟
测试、不跑线程）——它只是纯数据定义，可以被 UI 层、Service 层共同引用，
也可以被完全独立地做单元测试。
"""

from __future__ import annotations

from dataclasses import dataclass, field
from enum import Enum


class TestType(str, Enum):
    """测试类型。继承 str 是为了方便直接用在 ttk.Combobox 的 values 里，
    并且在打印/日志里直接得到可读的字符串，不需要额外转换。"""

    QUICK = "quick"
    FULL = "full"

    @property
    def display_name(self) -> str:
        return {"quick": "快速测试", "full": "完整测试"}[self.value]

    @property
    def step_count(self) -> int:
        """每一次测试运行包含多少个模拟步骤，决定单次测试的大致耗时。"""
        return {"quick": 3, "full": 6}[self.value]


class TaskStatus(str, Enum):
    """任务状态机：对应根 README 的状态机图。

    状态转换规则（由 services/test_service.py 和 ui/main_window.py 共同
    维护，但转换规则本身只应该在这里被"命名"，而不是分散用裸字符串表示）：

        IDLE -> RUNNING            （用户点击"开始"）
        RUNNING -> CANCELLING      （用户点击"取消"，等待后台确认）
        RUNNING -> DONE            （全部批次正常完成）
        RUNNING -> FAILED          （出现未捕获的异常）
        CANCELLING -> CANCELLED    （后台确认已停止）
        DONE / FAILED / CANCELLED -> IDLE   （用户再次点击"开始"，开启新批次）
    """

    IDLE = "idle"
    RUNNING = "running"
    CANCELLING = "cancelling"
    DONE = "done"
    FAILED = "failed"
    CANCELLED = "cancelled"

    @property
    def is_active(self) -> bool:
        """是否处于"任务仍占用后台线程"的状态（用于决定按钮可用性）。"""
        return self in (TaskStatus.RUNNING, TaskStatus.CANCELLING)

    @property
    def display_name(self) -> str:
        return {
            "idle": "就绪",
            "running": "运行中",
            "cancelling": "正在取消…",
            "done": "完成",
            "failed": "失败",
            "cancelled": "已取消",
        }[self.value]


@dataclass(frozen=True)
class TestConfig:
    """一次测试批次的配置，来自表单输入（UI State -> 业务数据，见 Layer 3）。"""

    device_address: str
    test_type: TestType
    repeat_count: int

    def validate(self) -> None:
        """集中的校验规则。抛出 ValueError，由调用方（UI 层）捕获并展示。"""
        if not self.device_address.strip():
            raise ValueError("设备地址不能为空。")
        if self.repeat_count < 1:
            raise ValueError("测试次数必须至少为 1。")
        if self.repeat_count > 50:
            raise ValueError("测试次数不能超过 50（演示限制）。")


@dataclass(frozen=True)
class TestRunResult:
    """一次具体测试运行（批次中的第 run_index 次）的结果。"""

    run_index: int
    passed: bool
    duration_ms: int
    message: str


@dataclass
class HistoryEntry:
    """一个完整测试批次的历史记录。

    这是 Treeview（Layer 5 的经验）要展示的"业务数据模型"：Treeview 本身
    只是它的视图，这个类才是数据的唯一存储位置。
    """

    entry_id: int
    device_address: str
    test_type: TestType
    started_at: str
    finished_at: str
    final_status: TaskStatus
    runs: list[TestRunResult] = field(default_factory=list)

    @property
    def pass_count(self) -> int:
        return sum(1 for r in self.runs if r.passed)

    @property
    def fail_count(self) -> int:
        return sum(1 for r in self.runs if not r.passed)

    def as_row(self) -> tuple[str, str, str, str, str, str]:
        """Treeview 一行需要的字符串元组（视图投影，见 Layer 5）。"""
        return (
            self.started_at,
            self.device_address,
            self.test_type.display_name,
            self.final_status.display_name,
            str(self.pass_count),
            str(self.fail_count),
        )
```

### D.2 [services/test_service.py](tkinter-test-console/services/test_service.py)

```python
"""
services/test_service.py

Layer 8 综合项目：业务服务层（Test Service）。

这个模块实现"模拟设备测试"的全部业务逻辑，并负责把它放进一个后台线程里
运行。严格遵守 Layer 7 建立的规则：

    - 本模块不 import tkinter，不引用任何 Widget/Variable。
    - 唯一的输出通道是 queue.Queue（ServiceMessage）。
    - 取消是协作式的（threading.Event），不是强制终止。
    - 所有异常在后台线程内部被捕获，转换成 ServiceMessage 传回调用方。

对应架构图（见根 README）：

    Tkinter UI -> Application Controller -> Test Service -> Worker Thread
        -> Queue -> Tkinter UI

本模块就是其中的 "Test Service" + "Worker Thread" 两层。
"""

from __future__ import annotations

import queue
import random
import threading
import time
from dataclasses import dataclass
from typing import Literal

from models import TestConfig, TestRunResult

# 每个模拟步骤的耗时（秒）。用较短的时间方便教学时快速看到效果；
# 真实工程中这里会是一次 SSH 命令、一次 HTTP 请求等真实耗时操作。
STEP_DURATION_SECONDS = 0.15

# 单次测试运行失败的模拟概率（仅用于演示异常/失败路径，不代表真实设备行为）。
SIMULATED_FAILURE_PROBABILITY = 0.15

ServiceMessageKind = Literal[
    "log",
    "run_progress",  # 当前批次内，第几步 / 共多少步
    "run_result",  # 单次运行完成，payload 是 TestRunResult
    "batch_done",  # 整个批次（repeat_count 次）全部正常完成
    "batch_cancelled",  # 批次被用户取消
    "batch_error",  # 批次因未预期异常终止
]


@dataclass(frozen=True)
class ServiceMessage:
    """Test Service -> UI 的统一消息信封（与 Layer 7 的 WorkerMessage 同一模式）。"""

    kind: ServiceMessageKind
    payload: object = None


def _run_single_test(
    run_index: int, config: TestConfig, cancel_event: threading.Event, out_queue: "queue.Queue[ServiceMessage]"
) -> TestRunResult | None:
    """执行一次具体的测试运行（批次中的第 run_index 次）。

    返回 None 表示在执行过程中检测到取消信号，调用方需要据此提前结束
    整个批次；否则返回这一次的 TestRunResult。
    """
    started_at = time.monotonic()
    total_steps = config.test_type.step_count

    for step in range(1, total_steps + 1):
        if cancel_event.is_set():
            return None

        time.sleep(STEP_DURATION_SECONDS)

        out_queue.put(
            ServiceMessage(
                "run_progress",
                payload={"run_index": run_index, "step": step, "total_steps": total_steps},
            )
        )

    duration_ms = int((time.monotonic() - started_at) * 1000)
    passed = random.random() >= SIMULATED_FAILURE_PROBABILITY
    message = "设备响应正常" if passed else "设备响应超时或返回异常状态"

    return TestRunResult(
        run_index=run_index, passed=passed, duration_ms=duration_ms, message=message
    )


def run_test_batch(
    config: TestConfig,
    cancel_event: threading.Event,
    out_queue: "queue.Queue[ServiceMessage]",
) -> None:
    """在后台线程中执行的入口函数：连续执行 config.repeat_count 次测试。

    这是 Layer 7 "run_simulated_test" 的工程化版本：
        - 支持"连续执行多个测试"（repeat_count）。
        - 每次运行的结果单独上报（run_result），不等全部跑完才汇总。
        - 任意时刻可以被取消，取消后已经完成的运行结果仍然有效保留。
        - 任何未预期异常都会被捕获并转换成 batch_error 消息。
    """
    try:
        for run_index in range(1, config.repeat_count + 1):
            if cancel_event.is_set():
                out_queue.put(ServiceMessage("batch_cancelled", payload=run_index - 1))
                return

            out_queue.put(
                ServiceMessage(
                    "log",
                    payload=f"开始第 {run_index}/{config.repeat_count} 次测试"
                    f"（{config.test_type.display_name}，设备 {config.device_address}）",
                )
            )

            result = _run_single_test(run_index, config, cancel_event, out_queue)

            if result is None:
                # _run_single_test 内部检测到了取消信号。
                out_queue.put(ServiceMessage("batch_cancelled", payload=run_index - 1))
                return

            out_queue.put(ServiceMessage("run_result", payload=result))
            status_text = "PASS" if result.passed else "FAIL"
            out_queue.put(
                ServiceMessage(
                    "log",
                    payload=f"第 {run_index} 次测试结束：{status_text}"
                    f"（耗时 {result.duration_ms} ms，{result.message}）",
                )
            )

        out_queue.put(ServiceMessage("batch_done", payload=config.repeat_count))
    except Exception as exc:  # noqa: BLE001 - 故意捕获所有异常传回 UI
        out_queue.put(ServiceMessage("batch_error", payload=str(exc)))


class TestService:
    """Application Controller 用来控制后台任务的句柄。

    这个类本身仍然不 import tkinter；它只负责"管理线程的生命周期"
    （创建、启动、取消、等待结束），不管理界面展示（展示逻辑在
    ui/main_window.py 里，通过轮询这个类暴露的队列来完成）。
    """

    def __init__(self) -> None:
        self._thread: threading.Thread | None = None
        self._cancel_event: threading.Event | None = None
        self.messages: "queue.Queue[ServiceMessage]" = queue.Queue()

    def is_running(self) -> bool:
        return self._thread is not None and self._thread.is_alive()

    def start(self, config: TestConfig) -> None:
        if self.is_running():
            raise RuntimeError("已有测试批次在运行，不能重复启动。")

        config.validate()

        self._cancel_event = threading.Event()
        self._thread = threading.Thread(
            target=run_test_batch,
            args=(config, self._cancel_event, self.messages),
            daemon=True,
        )
        self._thread.start()

    def cancel(self) -> None:
        if self._cancel_event is not None:
            self._cancel_event.set()

    def shutdown(self, timeout: float = 2.0) -> None:
        """供应用退出时调用：请求取消并等待线程结束。"""
        self.cancel()
        if self._thread is not None and self._thread.is_alive():
            self._thread.join(timeout=timeout)
```

### D.3 [ui/main_window.py](tkinter-test-console/ui/main_window.py)

```python
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
```

### D.4 [app.py](tkinter-test-console/app.py)

```python
"""
app.py

Layer 8 综合项目：应用入口。

运行方式（必须在 tkinter-test-console/ 目录下运行，这样 Python 才能
正确找到同级的 models.py / services / ui 模块）：

    cd tkinter-ttk-gui-mastery/08-test-console-project/tkinter-test-console
    python3 app.py

这个文件只做"组装"工作：创建根窗口，创建 MainWindow，启动事件循环。
不包含任何业务逻辑或界面细节——那些分别在 models.py / services/
test_service.py / ui/main_window.py 里。
"""

from __future__ import annotations

import tkinter as tk

from ui.main_window import MainWindow

APP_TITLE = "自动化测试控制台 (Tkinter Test Console)"


def build_root_window() -> tk.Tk:
    root = tk.Tk()
    root.title(APP_TITLE)
    root.geometry("560x560")
    root.minsize(480, 480)
    return root


def main() -> None:
    root = build_root_window()
    MainWindow(root)
    root.mainloop()


if __name__ == "__main__":
    main()
```

---

## E. Lifecycle

1. `python3 app.py` → `build_root_window()` 创建根窗口 → `MainWindow(root)` 构建两页 Notebook（"测试控制"/"历史记录"）→ `root.mainloop()`。
2. 用户填写设备地址/测试类型/测试次数，点"开始测试"：`on_start` 组装 `TestConfig` → `self.service.start(config)`（内部先 `config.validate()`，失败则抛 `ValueError` 被 UI 层捕获展示；成功则启动后台线程）→ UI 进入 `RUNNING` 状态，开始轮询。
3. 后台线程里 `run_test_batch` 连续执行 `repeat_count` 次 `_run_single_test`，每次运行的每一步都 `put` 一条 `run_progress` 消息，运行完成 `put` 一条 `run_result`。
4. 主线程 `_poll_queue` 每 50ms 把队列里的消息排空分发给 `_handle_message`：进度条、状态文字、成功/失败计数全部在这里更新。
5. 用户点"取消"：`on_cancel` 只是设置取消信号并把状态改为 `CANCELLING`——真正的"已取消"状态要等后台线程确认（`batch_cancelled` 消息）才会被设置。
6. 批次结束（`batch_done`/`batch_cancelled`/`batch_error` 三者之一）：`_finalize_batch` 把这次批次打包成 `HistoryEntry`，加入 `self.history` 并刷新历史记录 Treeview，同时把按钮状态恢复到可以重新开始。
7. 用户切换到"历史记录"页，选中一条记录点"查看详情"：弹出非模态的 `RunDetailDialog`，展示这次批次每一次运行的具体结果。
8. 用户关闭窗口：`on_close` 取消轮询、`service.shutdown()`（设置取消信号 + `join`），确认后台线程已退出后才 `root.destroy()`。

---

## F. Key APIs

本层不再重复 Layer 0~7 已经讲过的 Tkinter API，只汇总这个项目里新出现的组织模式：

| 模式 | 作用 | 常见错误 |
|---|---|---|
| `TestConfig.validate()` 集中校验 | 把"什么样的输入是合法的"这条规则放在数据模型里，而不是散落在 UI 回调里判断 | 在 UI 层重复写一遍校验逻辑，导致两处规则不一致 |
| `ServiceMessage` / `TaskStatus` 用枚举/dataclass 显式建模 | 让"消息类型""状态"可以被 IDE 补全、被类型检查工具检查，而不是裸字符串容易打错 | 到处手写字符串比较（如 `if status == "running"`），一旦改了字符串某处忘记同步 |
| `TestService.is_running()` + 按钮 disabled 双重防线 | 防止同一任务重复启动（Layer 7 的教训在工程项目里要落实两层防御） | 只在一处做检查，另一处留了漏洞 |
| `_finalize_batch()` 统一收尾 | 无论成功/取消/失败，都走同一个函数恢复界面状态、写历史记录 | 为三种终态各写一套重复的收尾代码，容易遗漏某个分支的清理 |

---

## G. Common Failure

### G.1 运行 `python3 app.py` 报 `ModuleNotFoundError: No module named 'models'`

- **现象**：从别的目录（比如仓库根目录）运行 `python3 08-test-console-project/tkinter-test-console/app.py`，报找不到 `models` 模块。
- **根本原因**：Python 会把"被直接执行的脚本所在目录"加入 `sys.path`，但如果你是从别的工作目录调用这个脚本的相对/绝对路径，脚本所在目录仍然会被正确加入——真正常见的触发方式其实是用 `python3 -m` 的方式从错误的包路径启动，或者 IDE 的"运行"配置里设置了错误的工作目录，导致 `services/test_service.py` 内部 `from models import ...` 找不到。
- **定位方法**：确认当前运行方式——推荐始终 `cd` 到 `tkinter-test-console/` 目录后再 `python3 app.py`；如果用 IDE 运行，检查"工作目录"配置是否指向这个目录。
- **修复方案**：按本层 README 顶部给出的运行方式操作；如果需要支持从任意目录运行，可以在 `app.py` 开头显式把自己所在目录加入 `sys.path`（`sys.path.insert(0, os.path.dirname(__file__))`），但对教学项目而言，统一约定运行目录更简单清晰。

### G.2 连续点击"开始测试"，日志里出现两段交叠的批次

- **现象**：点击一次"开始"还没结束就再点一次，日志混乱。
- **根本原因**：与 Layer 7 的 G.2 完全相同的问题，这里有两道防线：`TestService.start()` 内部检查 `is_running()` 并抛 `RuntimeError`；UI 层 `on_start` 也检查了一次。如果这两道防线都被绕过（例如极端情况下按钮状态更新有延迟导致快速连续点击），还是可能出现竞争。
- **定位方法**：检查 `self.start_btn.config(state="disabled")` 是否确实在启动成功后立刻执行（本例中紧跟在 `self.service.start(config)` 成功之后）。
- **修复方案**：确保"禁用开始按钮"这一步，和"启动后台线程"这一步在同一个回调函数的紧邻语句中完成，减少两者之间可能被用户再次点击的时间窗口。

### G.3 历史记录详情窗口打开后，主窗口的"查看详情"按钮点了没反应

- **现象**：打开过一次详情窗口后，再选中别的历史记录点"查看详情"，没有新窗口弹出。
- **根本原因**：`RunDetailDialog` 没有像 Layer 4 的 `SettingsDialog` 那样被"持有引用+判断是否存在"地管理——这里是故意简化的（因为查看详情是信息性窗口，允许用户同时打开多个也是合理的交互），但如果你在练习中改成"只允许一个详情窗口"，就需要重新引入 Layer 4 的 `winfo_exists()` 判断模式，否则行为会不符合预期。
- **定位方法**：确认当前实现的预期行为是"允许打开多个详情窗口"还是"只保留一个"，对照代码实际逻辑。
- **修复方案**：如果要约束成"只有一个"，参考 Layer 4 `on_open_settings()` 的实现模式。

---

## H. Exercises

1. **基础修改**：在"测试控制"页新增一个只读的"累计运行批次数"标签，每次 `_finalize_batch()` 被调用时加 1（不区分最终状态）。预期结果：无论批次是正常完成、被取消还是失败，这个计数都会增加。
2. **工程向练习**：给 `models.TestConfig` 增加一个"每步超时时间"字段（`step_timeout_ms: int`），并在 `services/test_service.py` 的 `_run_single_test` 里，让单个步骤的模拟耗时超过这个超时值时，判定该次运行为失败（而不是像现在一样用纯随机概率决定成功/失败）。提示：这需要你同时修改 `TestConfig.validate()`（校验超时值是否合理）和 UI 表单（新增一个输入控件绑定这个新字段）。预期结果：你能独立完成"模型加字段 → 校验规则同步更新 → 业务逻辑使用新字段 → 界面暴露新字段"这一条完整的工程改动链路，这正是这个项目分层架构带来的好处——每一步修改都知道该去哪个文件。

---

## I. Summary

本层把 Layer 0~7 的全部 Mental Model 整合进了一个四文件、职责清晰的真实项目：

- Widget Tree + 布局（Layer 0/1）在 `ui/main_window.py` 的 `_build_control_page`/`_build_history_page` 里随处可见。
- 事件回调 + 变量绑定（Layer 2/3）体现在表单的 `StringVar`/`IntVar` 和按钮的 `command=`。
- 多页面组织 + 非模态详情窗口（Layer 4）体现在 `ttk.Notebook` 和 `RunDetailDialog`。
- Treeview 数据/视图分离（Layer 5）体现在历史记录表格和 `HistoryEntry`。
- `after()` 轮询（Layer 6）和线程 + 队列（Layer 7）共同构成了整个任务执行与界面更新的核心机制。
- 新增的工程认知：把"纯数据"（`models.py`）、"纯业务逻辑"（`services/test_service.py`）和"界面+协调"（`ui/main_window.py`）分成三个独立的、依赖方向单一的模块（UI 依赖 Service 和 Model，Service 依赖 Model，但 Model 和 Service 都不依赖 UI），是让一个 GUI 项目能够被独立测试、独立维护、长期演进的关键结构性决定。

到这里，Layer 0~8 的学习路径已经完整闭环：从一个只有一行 Label 的窗口，到一个具备表单、多页面、批量数据展示、定时任务、后台线程和清晰分层架构的工程项目。接下来可以查阅 [09-debugging-checklist](../09-debugging-checklist/README.md) 作为日常排错清单，以及 [10-concept-mapping](../10-concept-mapping/README.md) 把这套 Mental Model 和你已有的 C/C++/Go/Rust 经验连接起来。
