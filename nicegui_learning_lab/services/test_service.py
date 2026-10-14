"""Service 层：一套完全模拟的"vCMTS 自动化测试用例"执行引擎。

为什么需要这个模块
------------------
示例 8（综合自动化测试控制台）需要演示一个在真实工程 GUI 里极其常见、也
极其容易写错的场景："点一个按钮，后台跑一长串步骤，界面上要实时看到进度
和日志，并且中途要能安全地取消"。这个场景本身的复杂度——进度回调、日志
回调、协作式取消、状态机——和"这些步骤具体做什么业务"是两件可以彻底分开
的事情。本模块只负责后者的一个**完全虚构的教学示例**：6 条命名风格借用了
DOCSIS/vCMTS 术语的"测试用例"，但它们的全部"执行逻辑"只是
`await asyncio.sleep(...)` 加几行字符串日志，**不连接任何真实设备，不发出
任何真实网络请求，也不调用 `services/command_service.py` 里任何真正会跑
子进程的命令**。

心智模型
--------
把 `run_test_suite()` 想象成一个"模拟测试跑道"：调用方先从 `TEST_SUITE`
里选出若干个 `TestCase`，`run_test_suite()` 负责按顺序把它们一个个"跑"
一遍，每一步都做三件事——检查有没有被取消、模拟耗时、汇报进度/日志——
全程只通过 `models.task.TaskRecord` 这一套统一的生命周期状态机对外暴露
"现在跑到哪了"，不额外发明第二套状态表示法。

两种"取消"的区别（请读者务必注意，另见示例 7/8 文档的详细讨论）：这里的
取消是**协作式**的——`cancel_event: asyncio.Event` 只是一个标志位，本模块
只在每个测试用例*开始之前*检查一次这个标志位，如果已经被设置就提前退出。
这和"杀掉一个正在运行的外部子进程"（需要操作系统级的 `proc.kill()`，见
`infrastructure/process_runner.py`）是两种完全不同的机制：协作式取消永远
需要被取消的那一方"愿意"在某个检查点主动配合，如果某一步忘记检查，取消
请求会被无限期地晾在一边。

模拟与真实的边界
----------------
本模块创建的所有 `TaskRecord` 都必须是 `is_simulated=True`（这也是
`models.task.new_task()` 的默认值，这里仍然显式传入以便一眼看出意图）。
UI 层（`examples/08_test_console.py`）必须用明显的"[模拟]"文案标注，绝不能
让使用者误以为这是在访问真实的 vCMTS/CM 设备。

关于 `simulate_failure_case` 的设计取舍
----------------------------------------
`TEST_SUITE` 里特意包含一条必定失败的用例，用来演示 `FAILED` 这条状态路径
以及 `TaskRecord.error` 字段该怎么用。一旦跑到这一条，`run_test_suite()`
会立刻把任务迁移到 `FAILED` 并返回，**不会继续执行后面排在它之后的用例**
——这是刻意设计的行为，模拟"一个关键依赖项检查失败后，没有必要继续跑后面
依赖它的测试"这种常见的真实工程逻辑，而不是一个疏漏。

本模块的依赖边界
----------------
本文件只允许 import `models/*`、`services/errors.py` 和标准库（包括
`asyncio`），**绝不 import nicegui**，这样 `tests/test_test_service.py`
即使在没有安装 nicegui 的环境下也能正常运行。本模块也不需要
`command_service`/`process_runner`：这里的"测试"是完全独立的内存模拟，
不会去真的执行示例 6 的任何白名单命令，避免给一套本该稳定可重复的单元
测试引入真实网络/进程依赖。

未来的可能演进
--------------
如果以后想让某条用例真的去跑一个只读诊断命令（例如把 `run_ping_sweep`
换成真正调用示例 6 的 `services.command_service.run_command('ping_once',
...)`），需要新增一个 `is_simulated=False` 的执行路径，并在 UI 上去掉
"[模拟]"标签——这不是本模块当前的职责范围，这里只负责把"后台任务 + 实时
日志 + 协作式取消"这套机制演示清楚。
"""

from __future__ import annotations

import asyncio
from dataclasses import dataclass
from typing import Callable

from models.task import TaskRecord, TaskStatus, new_task
from services.errors import ValidationError


@dataclass(frozen=True)
class TestCase:
    """一条模拟测试用例的静态描述（不含任何执行逻辑，执行逻辑在
    `run_test_suite()` 里统一处理）。
    """

    id: str
    name: str
    description: str


#: 模块级固定清单：全部是教学用途的虚构测试用例，命名借用了 DOCSIS/vCMTS
#: 领域术语，但**不连接任何真实设备，不发出任何真实网络请求**。顺序即
#: `run_test_suite()` 默认的执行顺序（见该函数文档）。
TEST_SUITE: list[TestCase] = [
    TestCase(
        id='check_cm_registration',
        name='检查调制解调器注册状态',
        description='（模拟）检查 CM 是否已在 CMTS 完成注册流程；纯内存模拟，不连接任何真实设备。',
    ),
    TestCase(
        id='check_downstream_snr',
        name='检查下行信噪比',
        description='（模拟）检查下行信道信噪比（SNR）是否处于正常范围；纯内存模拟，不连接任何真实设备。',
    ),
    TestCase(
        id='check_upstream_power',
        name='检查上行发射功率',
        description='（模拟）检查上行发射功率是否处于合理范围；纯内存模拟，不连接任何真实设备。',
    ),
    TestCase(
        id='verify_firmware_version',
        name='校验固件版本',
        description='（模拟）校验设备固件版本是否符合预期版本号；纯内存模拟，不连接任何真实设备。',
    ),
    TestCase(
        id='run_ping_sweep',
        name='执行一次 ping 扫描',
        description=(
            '（模拟）对一组模拟地址执行一次 ping 扫描；这是纯内存的教学模拟，'
            '不调用示例 6 命令控制台里真正会发出 ICMP 报文的 ping_once，不产生任何真实网络流量。'
        ),
    ),
    TestCase(
        id='simulate_failure_case',
        name='模拟失败用例（教学用，必定失败）',
        description=(
            '（模拟，教学用途）这条用例被刻意设计为必定失败，专门用来演示 '
            'FAILED 状态与 record.error 字段该如何展示；它不代表任何真实故障，'
            '也不说明被测设备有任何问题。'
        ),
    ),
]

#: id -> TestCase 的查找表，供校验 `selected_ids` 时使用。
_TEST_CASES_BY_ID: dict[str, TestCase] = {case.id: case for case in TEST_SUITE}

#: `run_test_suite()` 刻意必定失败的用例 id，集中定义成常量，避免在函数体
#: 内出现一处"魔法字符串"。
_GUARANTEED_FAILURE_ID = 'simulate_failure_case'


def _resolve_selected_cases(selected_ids: list[str]) -> list[TestCase]:
    """校验 `selected_ids` 并翻译成按**传入顺序**排列的用例列表。

    故意保留调用方传入的顺序（而不是改用 `TEST_SUITE` 的展示顺序）：这样
    "跑到 `simulate_failure_case` 就不再执行后面排队的用例"这条行为才有
    意义——调用方（通常是 UI 层按用户勾选顺序，或者测试代码按场景需要）
    决定执行顺序，本函数只负责校验，不重新排序。

    Raises:
        ValidationError: `selected_ids` 为空列表，或包含不存在的 id。
    """
    if not selected_ids:
        raise ValidationError('必须至少选择一个测试用例，selected_ids 不能为空')

    unknown_ids = [case_id for case_id in selected_ids if case_id not in _TEST_CASES_BY_ID]
    if unknown_ids:
        raise ValidationError(f'selected_ids 中包含未知的测试用例 id：{unknown_ids!r}')

    return [_TEST_CASES_BY_ID[case_id] for case_id in selected_ids]


async def run_test_suite(
    selected_ids: list[str],
    *,
    on_progress: Callable[[float], None],
    on_log: Callable[[str], None],
    cancel_event: asyncio.Event,
    step_delay: float = 0.3,
) -> TaskRecord:
    """按顺序模拟执行一组测试用例，返回全程记录状态/日志/进度的 `TaskRecord`。

    这是一个纯粹的协程：它本身不关心自己是被 `background_tasks.create()`
    即发即忘地启动，还是被 `asyncio.run()` 在测试里直接 `await`——两种
    调用方式下行为完全一致，这也是 `tests/test_test_service.py` 可以直接
    `asyncio.run(run_test_suite(...))` 测试的原因。

    执行流程（对 `selected_ids` 对应、按 `TEST_SUITE` 顺序排列后的每一条
    用例依次执行）：

    1. 检查 `cancel_event.is_set()`：如果已经被设置，把任务迁移到
       `CANCELLED` 并立即返回，**不会再执行这一条、也不会执行后面排队的
       用例**。
    2. 通过 `on_log(...)` 和 `record.append_log(...)` 同时记录一条"开始
       执行某用例"的日志——两者都要写，因为 `on_log` 是用来实时推给 UI
       （例如 `ui.log.push()`）的，而 `record.logs` 是留给历史详情页回放
       用的；两份日志内容完全一致。
    3. `await asyncio.sleep(step_delay)` 模拟这一步的执行耗时。
    4. 如果这一条用例的 id 是 `'simulate_failure_case'`：记一条失败日志、
       设置 `record.error`、把任务迁移到 `FAILED`，然后**立即返回**，不再
       继续跑后面的用例（这是刻意设计的行为，见模块顶部文档说明）。
    5. 否则记一条"执行完成"日志，并通过 `on_progress(当前完成比例)` 和
       `record.progress = 当前完成比例` 汇报进度（比例 = 已完成用例数 /
       选中的用例总数）。
    6. 全部用例都跑完且没有触发失败路径时，通过 `on_log(...)` 和
       `record.append_log(...)` 记一条"全部执行成功"的日志，设置
       `record.result`，把任务迁移到 `SUCCEEDED`，并返回这个
       `TaskRecord`。

    Args:
        selected_ids: 要执行的 `TestCase.id` 列表，**按传入顺序执行**（不会
            被重新排序成 `TEST_SUITE` 的展示顺序），这样调用方可以精确控制
            "先跑哪个、后跑哪个"，也让"失败后不再执行后面排队的用例"这条
            行为可以被直接观察到。
        on_progress: 每完成一条非失败用例时调用一次，参数是 0.0~1.0 之间
            的完成比例，典型用法是更新 `ui.linear_progress.value`。
        on_log: 每产生一条日志时调用一次（开始/完成/失败都会触发），典型
            用法是 `ui.log.push(line)`。
        cancel_event: 调用方新建的 `asyncio.Event`；在任意时刻调用
            `cancel_event.set()`，本协程会在下一个用例开始前的检查点感知
            到并停下来——这是"协作式取消"，不是立即中断。
        step_delay: 每个模拟步骤的模拟耗时（秒）。生产教学场景下用默认值
            `0.3` 让用户能在界面上看清进度变化；单元测试里应传入很小的值
            （例如 `0.01`）让测试快速跑完。

    Returns:
        执行结束时的 `TaskRecord`（终态为 `SUCCEEDED` / `FAILED` /
        `CANCELLED` 三者之一），`is_simulated` 恒为 `True`。

    Raises:
        ValidationError: `selected_ids` 为空，或包含 `TEST_SUITE` 里不存在
            的 id。这个校验在创建 `TaskRecord` 之前就完成，所以校验失败时
            不会产生任何"半途而废"的任务记录。
    """
    selected_cases = _resolve_selected_cases(selected_ids)
    total = len(selected_cases)

    record = new_task(
        name=f'vCMTS 模拟自动化测试（{total} 项用例）',
        is_simulated=True,
    )
    record.transition_to(TaskStatus.RUNNING)

    for index, case in enumerate(selected_cases):
        if cancel_event.is_set():
            cancel_message = f'[模拟] 检测到取消请求，任务在执行用例 {case.name!r} 之前被取消'
            on_log(cancel_message)
            record.append_log(cancel_message)
            record.transition_to(TaskStatus.CANCELLED)
            return record

        start_message = f'[模拟] 开始执行用例「{case.name}」（{case.id}）'
        on_log(start_message)
        record.append_log(start_message)

        await asyncio.sleep(step_delay)

        if case.id == _GUARANTEED_FAILURE_ID:
            error_message = (
                f'[模拟] 用例「{case.name}」执行失败（这是教学用途的、刻意设计的'
                '必然失败分支，不代表任何真实设备故障）'
            )
            on_log(error_message)
            record.append_log(error_message)
            record.error = error_message
            record.transition_to(TaskStatus.FAILED)
            return record

        finish_message = f'[模拟] 用例「{case.name}」执行完成'
        on_log(finish_message)
        record.append_log(finish_message)

        progress = (index + 1) / total
        on_progress(progress)
        record.progress = progress

    success_message = f'[模拟] 全部 {total} 项用例执行成功'
    on_log(success_message)
    record.append_log(success_message)
    record.result = success_message
    record.transition_to(TaskStatus.SUCCEEDED)
    return record
