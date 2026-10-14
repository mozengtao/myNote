"""模拟任务数据的生成与查询服务。

为什么需要这个模块
------------------
示例 5（任务表格）需要演示"一个会随着筛选/翻页不断变化的大列表该怎么用
`ui.table` 展示"，但本项目此时还没有一个真实的、会产生任务的后端。于是这里
用可复现的随机数据模拟出一批 `TaskRecord`，让 UI 侧可以像对接真实接口一样
去调用"生成数据源" + "按条件查询一页"这两个函数。

模拟与真实的边界
----------------
这里生成的所有 `TaskRecord` 都必须把 `is_simulated` 设为 `True`
（这是 `TaskRecord` 的默认值，这里仍然显式传入以便一眼看出意图）。
UI 侧必须用明显的文案（例如“模拟数据”字样）标注出来，不能让使用者误以为
这是某个真实任务系统的记录——将来如果有其它示例（例如示例 8）接入真实的
任务系统，应该是另外实现一个 `is_simulated=False` 的数据源，而不是改动
这个模拟服务。

两个函数的职责划分
------------------
- `generate_mock_tasks()` 负责"从无到有造出一份完整的数据源"，它只依赖一个
  `seed`，相同的 `seed` 必须产出完全一样的结果（可复现，方便写单元测试、
  方便在文档里讨论一个具体的例子）。
- `list_tasks()` 是一个纯函数：给定一份已经存在的数据源、筛选条件、分页
  参数，返回"这一页应该展示什么"。它不生成数据，也不持有任何状态——这样它
  的分页/筛选逻辑可以脱离 NiceGUI、脱离任何全局变量，被单独、快速地做
  单元测试。真正"持有数据源"的职责交给调用方（本项目中是
  `examples/05_task_table.py` 在模块级别持有一份 `generate_mock_tasks(...)`
  的结果）。
"""

from __future__ import annotations

import random
from datetime import datetime, timedelta, timezone

from models.task import TaskRecord, TaskStatus
from services.errors import ValidationError

#: 生成模拟任务名称时使用的词库，纯粹是为了让示例界面里的数据看起来
#: 不那么单调；没有任何业务含义。
_NAME_FRAGMENTS = (
    'sync-config',
    'collect-metrics',
    'restart-service',
    'rotate-logs',
    'backup-db',
    'deploy-firmware',
    'health-check',
    'cleanup-tmp',
    'run-diagnostics',
    'validate-schema',
)

#: 模拟任务的起始创建时间（固定值，配合固定 seed 实现完全可复现的输出）。
_BASE_CREATED_AT = datetime(2025, 1, 1, tzinfo=timezone.utc)


def _advance_to(record: TaskRecord, target: TaskStatus, rng: random.Random) -> None:
    """把 `record` 从 PENDING 状态推进到 `target` 状态。

    必须全程通过 `TaskRecord.transition_to()` 完成，不能直接赋值
    `.status`——这是项目规格的硬性要求，也保证了即便是模拟数据，状态变化
    的轨迹也始终符合 `models/task.py` 里定义的合法迁移表。
    """
    if target is TaskStatus.PENDING:
        return

    if target is TaskStatus.RUNNING:
        record.transition_to(TaskStatus.RUNNING)
        record.append_log(f'[模拟] 任务 {record.name} 开始运行')
        return

    if target is TaskStatus.CANCELLED:
        # CANCELLED 既可以从 PENDING 直接取消，也可以从 RUNNING 中途取消，
        # 这里随机选一条合法路径，让模拟数据里两种情况都出现。
        if rng.random() < 0.5:
            record.transition_to(TaskStatus.CANCELLED)
            record.append_log(f'[模拟] 任务 {record.name} 在排队阶段被取消')
        else:
            record.transition_to(TaskStatus.RUNNING)
            record.append_log(f'[模拟] 任务 {record.name} 开始运行')
            record.transition_to(TaskStatus.CANCELLED)
            record.append_log(f'[模拟] 任务 {record.name} 运行中被取消')
        return

    # SUCCEEDED / FAILED 在合法迁移表里都必须先经过 RUNNING。
    record.transition_to(TaskStatus.RUNNING)
    record.append_log(f'[模拟] 任务 {record.name} 开始运行')
    record.transition_to(target)
    if target is TaskStatus.SUCCEEDED:
        record.append_log(f'[模拟] 任务 {record.name} 执行完成')
        record.result = '模拟执行成功（退出码 0，非真实命令输出）'
    else:
        record.append_log(f'[模拟] 任务 {record.name} 执行失败')
        record.error = '模拟执行失败（exit code 1，非真实命令输出，仅用于演示错误展示）'


def generate_mock_tasks(count: int, *, seed: int = 42) -> list[TaskRecord]:
    """生成一份固定数量、可复现的模拟任务列表。

    所有返回的 `TaskRecord` 都满足 `is_simulated=True`；`created_at` 依次
    递增；`status` 在 `TaskStatus` 的几种取值间随机但可复现地分布
    （相同的 `count` + `seed` 永远产出完全一样的列表，内容逐字段相等）。

    Args:
        count: 要生成的任务数量。
        seed: 随机数种子，用于保证可复现性；默认值 42 只是一个没有特殊
            含义的约定值。

    Returns:
        长度为 `count` 的 `TaskRecord` 列表，`created_at` 按生成顺序递增。
    """
    rng = random.Random(seed)
    statuses = list(TaskStatus)
    tasks: list[TaskRecord] = []

    for index in range(count):
        fragment = rng.choice(_NAME_FRAGMENTS)
        name = f'mock-{fragment}-{index:04d}'
        created_at = _BASE_CREATED_AT + timedelta(minutes=index)
        record = TaskRecord(
            id=f'mock-task-{index:06d}',
            name=name,
            status=TaskStatus.PENDING,
            created_at=created_at,
            is_simulated=True,
        )
        record.append_log(f'[模拟] 任务 {record.name} 已创建（种子={seed}）')
        target_status = rng.choice(statuses)
        _advance_to(record, target_status, rng)
        tasks.append(record)

    return tasks


def list_tasks(
    tasks: list[TaskRecord],
    *,
    page: int,
    page_size: int,
    status_filter: TaskStatus | None = None,
) -> tuple[list[TaskRecord], int]:
    """对一份已有的任务列表做"先筛选、再分页"的纯函数查询。

    这个函数故意接收完整的 `tasks` 列表，而不是自己生成或持有任何状态：
    这样它才是一个输入决定输出的纯函数，分页边界、筛选逻辑都可以脱离
    NiceGUI、脱离全局变量单独做单元测试。真正持有"数据源"的职责在调用方
    （参见 `examples/05_task_table.py`）。

    Args:
        tasks: 完整的任务数据源（通常来自 `generate_mock_tasks()`）。
        page: 页码，从 1 开始。
        page_size: 每页行数。
        status_filter: 为 `None` 时不过滤；否则只保留状态等于该值的任务。

    Returns:
        一个二元组 `(当前页的 TaskRecord 列表, 过滤后的总条数)`。
        当 `page` 超出范围（例如筛选后总共只有 2 页，却请求第 5 页）时，
        不会抛异常，而是返回一个空列表 —— 这是分页场景下的正常情况，
        不是错误。

    Raises:
        ValidationError: `page` 或 `page_size` 不是正整数。
    """
    if page < 1:
        raise ValidationError(f'page 必须从 1 开始，实际为 {page}')
    if page_size < 1:
        raise ValidationError(f'page_size 必须为正整数，实际为 {page_size}')

    filtered = tasks if status_filter is None else [
        task for task in tasks if task.status == status_filter
    ]
    total = len(filtered)

    start = (page - 1) * page_size
    end = start + page_size
    return filtered[start:end], total
