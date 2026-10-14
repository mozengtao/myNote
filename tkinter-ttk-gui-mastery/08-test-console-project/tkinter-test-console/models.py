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
