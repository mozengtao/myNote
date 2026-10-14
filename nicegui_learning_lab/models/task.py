"""任务（Task）模型 —— 本项目所有 Service 与 UI 层共用的核心数据结构。

为什么需要这个模块
------------------
本项目里每一个"耗时操作"（配置校验、命令执行、后台测试运行）都需要同一套
词汇：一个稳定的 id、一个生命周期状态、时间戳、一个有上限的日志缓冲区，以及
一个明确的"这是模拟结果还是真实结果"标记。把这套词汇集中放在这里，才能让
services/、ui/、tests/ 对"什么是一个任务"达成一致，而不是各自抄一份略有
差异的 dataclass。

心智模型
--------
一个 TaskRecord 就是"当前已知的、某一个工作单元的快照"。TaskStatus 是整个
项目里唯一允许使用的生命周期状态词汇表。`can_transition()` / 
`TaskRecord.transition_to()` 是唯一能决定"状态能否这样变化"的地方——任何
需要修改 TaskRecord 状态的 Service 都必须通过 `transition_to()`，而不是直接
赋值 `.status = ...`，这样非法跳转（例如还没运行过就从 PENDING 直接变成
SUCCEEDED）会立刻抛出异常，而不是悄悄污染 UI 上展示的历史记录。

本模块不依赖 nicegui，也不依赖本项目里的任何其他模块——它是纯标准库代码，
因此即使在没有安装 nicegui 的环境下也可以被导入并做单元测试。
"""

from __future__ import annotations

from dataclasses import dataclass, field
from datetime import datetime, timezone
from enum import Enum
from uuid import uuid4

#: 单个 TaskRecord 在内存中最多保留的日志行数上限。
#: 用于防止一个失控/长时间运行的任务把内存日志撑爆
#: （对应项目规格第 9.3 节："避免无限增长的日志和任务历史"）。
MAX_LOG_LINES = 500


class TaskStatus(str, Enum):
    """本项目所有"类任务"对象共用的生命周期状态。"""

    PENDING = "PENDING"
    RUNNING = "RUNNING"
    SUCCEEDED = "SUCCEEDED"
    FAILED = "FAILED"
    CANCELLED = "CANCELLED"


_TERMINAL_STATES = frozenset({TaskStatus.SUCCEEDED, TaskStatus.FAILED, TaskStatus.CANCELLED})

#: 唯一合法的状态迁移表。凡是没有在这里列出的迁移，一律视为非法。
_ALLOWED_TRANSITIONS: dict[TaskStatus, frozenset[TaskStatus]] = {
    TaskStatus.PENDING: frozenset({TaskStatus.RUNNING, TaskStatus.CANCELLED}),
    TaskStatus.RUNNING: frozenset({TaskStatus.SUCCEEDED, TaskStatus.FAILED, TaskStatus.CANCELLED}),
    TaskStatus.SUCCEEDED: frozenset(),
    TaskStatus.FAILED: frozenset(),
    TaskStatus.CANCELLED: frozenset(),
}


def can_transition(old: TaskStatus, new: TaskStatus) -> bool:
    """判断从 `old` 状态迁移到 `new` 状态是否是一次合法的生命周期跳转。"""
    if old == new:
        return False
    return new in _ALLOWED_TRANSITIONS.get(old, frozenset())


def is_terminal(status: TaskStatus) -> bool:
    """判断 `status` 是否为终止状态（之后不再允许任何状态迁移）。"""
    return status in _TERMINAL_STATES


@dataclass
class TaskRecord:
    """一个在生命周期中被持续追踪的工作单元。

    `is_simulated` 必须由创建该记录的 Service 显式设置：本项目中所有
    模拟/演示类 Service 都应将其设为 True。UI 层必须把这个标记明显地
    展示出来（例如一个"模拟"徽标），确保模拟结果永远不会被误认为是
    真实设备/命令确实执行成功——这是项目规格第 3.4 节的硬性要求。
    """

    id: str
    name: str
    status: TaskStatus
    created_at: datetime
    started_at: datetime | None = None
    finished_at: datetime | None = None
    progress: float = 0.0
    logs: list[str] = field(default_factory=list)
    result: str | None = None
    error: str | None = None
    is_simulated: bool = True

    def append_log(self, line: str) -> None:
        """追加一行日志；超过 MAX_LOG_LINES 时丢弃最旧的若干行。"""
        self.logs.append(line)
        overflow = len(self.logs) - MAX_LOG_LINES
        if overflow > 0:
            del self.logs[:overflow]

    def transition_to(self, new_status: TaskStatus, *, at: datetime | None = None) -> None:
        """将本记录迁移到 `new_status`，并强制执行合法迁移表的约束。

        Raises:
            ValueError: 如果从当前状态无法到达 `new_status`。
        """
        if not can_transition(self.status, new_status):
            raise ValueError(f"illegal task transition: {self.status} -> {new_status}")
        timestamp = at or datetime.now(timezone.utc)
        self.status = new_status
        if new_status is TaskStatus.RUNNING and self.started_at is None:
            self.started_at = timestamp
        if is_terminal(new_status):
            self.finished_at = timestamp


def new_task(name: str, *, task_id: str | None = None, is_simulated: bool = True) -> TaskRecord:
    """创建一个全新的、状态为 PENDING 的 TaskRecord；若未指定 id 则自动生成。"""
    return TaskRecord(
        id=task_id or uuid4().hex,
        name=name,
        status=TaskStatus.PENDING,
        created_at=datetime.now(timezone.utc),
        is_simulated=is_simulated,
    )
