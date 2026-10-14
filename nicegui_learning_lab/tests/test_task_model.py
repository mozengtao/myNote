"""`models/task.py` 的单元测试。

为什么这个测试文件是在集成阶段补的
------------------------------------
`models/task.py` 是整个项目最底层的冻结契约（由协调者本人在并行任务开始
之前创建），当时只用一次性 shell 脚本手动验证过它的行为（创建任务、
PENDING -> RUNNING -> FAILED 的合法迁移、非法迁移报错、日志裁剪），没有
把这些验证固化成一个真正会被 `pytest` 收集、每次运行都会重新检查的测试
文件——这是协调者在集成阶段核对 `tests/` 目录内容时自己发现的疏漏（本项目
最初的目录规划里就列出了 `tests/test_task_model.py`，但一直没有被创建），
现在补上，覆盖 `can_transition()`/`is_terminal()`/`TaskRecord.transition_to()`/
`TaskRecord.append_log()`/`new_task()` 这几个最基础、被全项目所有 Service
依赖的函数。

本文件完全不依赖 nicegui（`models/task.py` 本身也不依赖），可以在没有
安装 nicegui 的环境下运行。
"""

from __future__ import annotations

from datetime import datetime, timezone

import pytest

from models.task import (
    MAX_LOG_LINES,
    TaskRecord,
    TaskStatus,
    can_transition,
    is_terminal,
    new_task,
)


# ---------------------------------------------------------------------------
# can_transition() / is_terminal()
# ---------------------------------------------------------------------------


@pytest.mark.parametrize(
    'old,new',
    [
        (TaskStatus.PENDING, TaskStatus.RUNNING),
        (TaskStatus.PENDING, TaskStatus.CANCELLED),
        (TaskStatus.RUNNING, TaskStatus.SUCCEEDED),
        (TaskStatus.RUNNING, TaskStatus.FAILED),
        (TaskStatus.RUNNING, TaskStatus.CANCELLED),
    ],
)
def test_can_transition_allows_legal_edges(old, new):
    assert can_transition(old, new) is True


@pytest.mark.parametrize(
    'old,new',
    [
        (TaskStatus.PENDING, TaskStatus.SUCCEEDED),  # 必须先经过 RUNNING
        (TaskStatus.PENDING, TaskStatus.FAILED),
        (TaskStatus.SUCCEEDED, TaskStatus.RUNNING),  # 终止状态不能再迁移
        (TaskStatus.FAILED, TaskStatus.PENDING),
        (TaskStatus.CANCELLED, TaskStatus.RUNNING),
        (TaskStatus.PENDING, TaskStatus.PENDING),  # 原地不动也不是一次合法迁移
    ],
)
def test_can_transition_rejects_illegal_edges(old, new):
    assert can_transition(old, new) is False


@pytest.mark.parametrize(
    'status,expected',
    [
        (TaskStatus.PENDING, False),
        (TaskStatus.RUNNING, False),
        (TaskStatus.SUCCEEDED, True),
        (TaskStatus.FAILED, True),
        (TaskStatus.CANCELLED, True),
    ],
)
def test_is_terminal(status, expected):
    assert is_terminal(status) is expected


# ---------------------------------------------------------------------------
# new_task()
# ---------------------------------------------------------------------------


def test_new_task_defaults():
    record = new_task('demo')
    assert record.name == 'demo'
    assert record.status is TaskStatus.PENDING
    assert record.is_simulated is True
    assert record.started_at is None
    assert record.finished_at is None
    assert record.progress == 0.0
    assert record.logs == []
    assert record.id  # 自动生成的 uuid4 hex，非空


def test_new_task_accepts_explicit_id_and_is_simulated_false():
    record = new_task('demo', task_id='fixed-id-001', is_simulated=False)
    assert record.id == 'fixed-id-001'
    assert record.is_simulated is False


def test_new_task_generates_distinct_ids():
    first = new_task('a')
    second = new_task('b')
    assert first.id != second.id


# ---------------------------------------------------------------------------
# TaskRecord.transition_to()
# ---------------------------------------------------------------------------


def test_transition_to_running_sets_started_at_once():
    record = new_task('demo')
    assert record.started_at is None
    record.transition_to(TaskStatus.RUNNING)
    assert record.status is TaskStatus.RUNNING
    assert record.started_at is not None
    first_started_at = record.started_at

    # 再次"迁移"到 RUNNING 是非法的（RUNNING -> RUNNING 不在合法迁移表里），
    # 这里单独验证 started_at 不会被 transition_to 以外的途径意外改写。
    with pytest.raises(ValueError):
        record.transition_to(TaskStatus.RUNNING)
    assert record.started_at == first_started_at


def test_transition_to_terminal_state_sets_finished_at():
    record = new_task('demo')
    record.transition_to(TaskStatus.RUNNING)
    assert record.finished_at is None
    record.transition_to(TaskStatus.FAILED)
    assert record.status is TaskStatus.FAILED
    assert record.finished_at is not None


def test_transition_to_illegal_edge_raises_value_error_and_does_not_mutate_status():
    record = new_task('demo')
    with pytest.raises(ValueError):
        record.transition_to(TaskStatus.SUCCEEDED)  # PENDING 不能直接到 SUCCEEDED
    # 失败的迁移尝试不应该改变当前状态。
    assert record.status is TaskStatus.PENDING
    assert record.finished_at is None


def test_transition_to_accepts_explicit_timestamp():
    record = new_task('demo')
    fixed_time = datetime(2025, 1, 1, 12, 0, 0, tzinfo=timezone.utc)
    record.transition_to(TaskStatus.RUNNING, at=fixed_time)
    assert record.started_at == fixed_time


def test_full_lifecycle_pending_running_succeeded():
    record = new_task('demo')
    record.transition_to(TaskStatus.RUNNING)
    record.transition_to(TaskStatus.SUCCEEDED)
    assert record.status is TaskStatus.SUCCEEDED
    assert is_terminal(record.status) is True
    assert record.started_at is not None
    assert record.finished_at is not None
    assert record.started_at <= record.finished_at


def test_cancel_directly_from_pending_does_not_set_started_at():
    record = new_task('demo')
    record.transition_to(TaskStatus.CANCELLED)
    assert record.status is TaskStatus.CANCELLED
    # 从 PENDING 直接取消，从未经过 RUNNING，所以不应该有开始时间。
    assert record.started_at is None
    assert record.finished_at is not None


# ---------------------------------------------------------------------------
# TaskRecord.append_log()
# ---------------------------------------------------------------------------


def test_append_log_accumulates_lines_in_order():
    record = new_task('demo')
    record.append_log('line 1')
    record.append_log('line 2')
    assert record.logs == ['line 1', 'line 2']


def test_append_log_trims_to_max_log_lines():
    record = new_task('demo')
    total_lines = MAX_LOG_LINES + 100
    for index in range(total_lines):
        record.append_log(f'line {index}')

    assert len(record.logs) == MAX_LOG_LINES
    # 裁剪必须丢弃最旧的行，保留最新的 MAX_LOG_LINES 行。
    assert record.logs[0] == f'line {total_lines - MAX_LOG_LINES}'
    assert record.logs[-1] == f'line {total_lines - 1}'
