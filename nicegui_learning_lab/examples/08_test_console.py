"""示例 8：综合自动化测试控制台 —— 把示例 7 的"裸协程"升级成正式 Service。

为什么需要这个示例
------------------
示例 7 演示了"后台任务机制"本身该怎么写：启动、取消、异常处理。但那个
示例里的任务是一个完全临时的本地协程，没有名字、没有历史、没有统一的生命
周期表示——一旦页面刷新，那次运行发生过什么就彻底无从查起了。真实的工程
GUI 几乎总是需要回答"刚才跑的是哪一次？跑了哪些用例？结果是什么？"这些
问题，这就需要把"一次运行"本身变成一个有唯一 id、有统一状态机的对象——
这正是 `models.task.TaskRecord` 存在的意义，也是本示例要在示例 7 的基础上
演进出来的东西。

本示例的全部业务逻辑（6 条模拟测试用例、按顺序执行、进度/日志回调、取消
检查点、失败后提前结束）都定义在 `services/test_service.py` 里，并且已经
由 `tests/test_test_service.py` 覆盖测试过（不依赖 nicegui 即可运行）。
本文件只负责界面层的事：收集用户选择 -> 调用 `test_service.run_test_suite()`
-> 把返回的/过程中的信息展示出来，不重新实现任何一遍选用例/判断取消/
判断失败的逻辑。

从示例 7 到示例 8：裸协程 -> 正式 Service 的演进关系
----------------------------------------------------
| | 示例 7 | 示例 8 |
| --- | --- | --- |
| 任务逻辑在哪 | 写死在 `examples/07_background_task.py` 的一个函数里 | 独立成 `services/test_service.py`，不依赖 nicegui，可单独做单元测试 |
| 任务身份 | 没有：只有"当前有没有在跑"这一个布尔状态 | 每次运行都是一个独立的 `TaskRecord`，有唯一 `id` |
| 状态表示 | 临时的 `status_label.text` 字符串 | `TaskStatus` 这一套项目统一的生命周期状态机，`transition_to()` 强制校验合法迁移 |
| 历史 | 没有：刷新页面/再点一次开始，之前跑过什么就没了 | 模块级 `_HISTORY: list[TaskRecord]`，每次运行结束都追加一条，可在历史表格里回看 |
| 启动方式 | `background_tasks.create(协程函数())` | 同样是 `background_tasks.create(test_service.run_test_suite(...))`——启动机制完全一致，变的只是"协程里到底在跑什么" |

为什么不能只用一个全局 `running: bool` 表示"这个页面的运行状态"
-------------------------------------------------------------------
一个朴素的做法是维护一个全局（或页面级）布尔变量 `running`，`True` 就说明
"现在有任务在跑"。这个方案在只有"一次运行、不需要历史、不需要区分是哪一次
运行"的最简单场景下能工作，但很快就会出问题：

- **无法回答"正在运行的是哪一次？"**——布尔值只能回答"有没有"，回答不了
  "是哪个"。一旦需要展示"当前运行的任务 id / 名称 / 已经跑到第几步"，
  布尔值就无能为力，还得另外发明变量去记这些信息——而这些信息本来就是
  `TaskRecord` 已经提供好的。
- **无法区分"合法状态"和"非法状态"**——`running = True` 之后紧接着又被设成
  `True`（重复启动）这种明显违反业务规则的情况，布尔值完全不会报错；而
  `TaskRecord.transition_to()` 会在任何非法迁移（例如对一个已经处于终止
  状态的任务再次调用 `transition_to(RUNNING)`）时立刻抛出异常。
- **无法支持"多个并发运行"或"按用户隔离"**——这是示例 9 要展开的话题：当
  应用变成多用户、甚至同一用户允许开多个并发任务时，"运行状态"必须是一个
  可以被索引、可以被持久化（`app.storage`）的集合，而不是进程里的一个
  全局变量。`TaskRecord` 天生就是"集合里的一个元素"，`running: bool` 从
  设计上就只能表示"全局唯一的一件事"，扩展不到那个场景。

模拟与真实的边界
----------------
本页面展示的全部结果都来自 `services.test_service.run_test_suite()`，其
`TaskRecord.is_simulated` 恒为 `True`。界面上反复出现"[模拟]"字样，就是为
了确保使用者在任何时候都清楚：这里跑的不是真实的 vCMTS/CM 设备诊断，纯粹
是教学用的内存模拟。

历史记录的简化处理（请读者注意这是刻意的简化，不是推荐的生产做法）
---------------------------------------------------------------------
`_HISTORY` 是一个模块级的、进程内存里的 `list[TaskRecord]`：所有打开这个
页面的浏览器标签页共享同一份历史，进程一重启历史就清空，也没有按用户做
任何隔离。真实应用通常会用 `app.storage.user`（按会话隔离）或某种真正的
持久化存储（数据库/文件）来保存这类历史，这是示例 9 要展开的话题——这里
选择最简单的全局列表，是为了不让"怎么存储历史"这件事分散读者对本示例
核心主题（后台任务 + Service 化 + TaskRecord 生命周期）的注意力。
"""

from __future__ import annotations

import asyncio
import sys
from pathlib import Path

# 集成阶段补丁：原因与做法见 examples/04_config_checker.py 顶部的同名注释
# 和 docs/multitask_plan.md 的"集成阶段修正记录"一节。
_PROJECT_ROOT = Path(__file__).resolve().parent.parent
if str(_PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(_PROJECT_ROOT))

from nicegui import background_tasks, ui

from models.task import TaskRecord, TaskStatus
from services import test_service
from services.errors import ValidationError

#: 模块级历史记录：所有浏览器标签页共享的、进程内存里的运行历史。
#: 见模块顶部文档"历史记录的简化处理"一节的说明。
_HISTORY: list[TaskRecord] = []

#: id -> TaskRecord 的查找表，供历史表格的"点击查看详情"使用，
#: 与 `_HISTORY` 保持同步（每次 append 都会同步更新）。
_HISTORY_BY_ID: dict[str, TaskRecord] = {}

#: TaskStatus -> 展示给用户的中文文案。
_STATUS_LABELS: dict[TaskStatus, str] = {
    TaskStatus.PENDING: '等待中',
    TaskStatus.RUNNING: '运行中',
    TaskStatus.SUCCEEDED: '成功',
    TaskStatus.FAILED: '失败',
    TaskStatus.CANCELLED: '已取消',
}

#: TaskStatus -> 状态标签使用的文字颜色 class（Tailwind/Quasar 风格的原子类）。
_STATUS_COLOR_CLASSES: dict[TaskStatus, str] = {
    TaskStatus.PENDING: 'text-gray-600',
    TaskStatus.RUNNING: 'text-blue-600',
    TaskStatus.SUCCEEDED: 'text-green-700',
    TaskStatus.FAILED: 'text-red-700',
    TaskStatus.CANCELLED: 'text-orange-600',
}

#: 历史记录表格的列定义；`id_short`/`created_at_text` 是专门为展示准备的
#: 衍生字段（见 `_task_to_history_row()`），完整的 `id` 仍然保留在行数据
#: 里，供点击详情时查找完整的 `TaskRecord`。
_HISTORY_COLUMNS = [
    {'name': 'id_short', 'label': 'ID', 'field': 'id_short', 'align': 'left'},
    {'name': 'name', 'label': '名称', 'field': 'name', 'align': 'left'},
    {'name': 'status', 'label': '状态', 'field': 'status', 'align': 'left'},
    {'name': 'created_at_text', 'label': '创建时间', 'field': 'created_at_text', 'align': 'left'},
]


def _status_text(status: TaskStatus) -> str:
    """把 `TaskStatus` 翻译成状态标签要展示的完整文案，恒带"[模拟]"前缀。"""
    return f'[模拟] 当前状态：{_STATUS_LABELS[status]}（{status.value}）'


def _task_to_history_row(record: TaskRecord) -> dict:
    """把一个 `TaskRecord` 转换成历史表格需要的一行 dict（含完整 id）。"""
    return {
        'id': record.id,
        'id_short': record.id[:8],
        'name': record.name,
        'status': _STATUS_LABELS[record.status],
        'created_at_text': record.created_at.strftime('%Y-%m-%d %H:%M:%S'),
    }


class _PageState:
    """保存"这一个浏览器会话当前运行状态"的小容器。

    `is_task_running` 只是一个**防止重复点击"开始运行"按钮**的轻量级
    UI 门闩（因为按钮在任务运行期间本来就会被禁用，这里是一层防御性
    兜底），**不是**本示例真正用来表示"任务生命周期"的机制——那是
    `TaskRecord.status` 的职责，见模块顶部文档"为什么不能只用一个全局
    `running: bool`"一节的详细讨论。
    """

    def __init__(self) -> None:
        self.cancel_event = asyncio.Event()
        self.is_task_running = False


@ui.page('/test-console')
def test_console_page() -> None:
    """示例 8 页面：用例选择表单 + 启动/取消 + 实时进度日志 + 历史记录。"""
    state = _PageState()

    ui.label('示例 8：综合自动化测试控制台').classes('text-2xl font-bold').mark('page-title')
    ui.label(
        '本页面展示的全部测试结果均为【模拟】数据，不连接任何真实 vCMTS/CM 设备；'
        '测试用例定义与执行逻辑见 services/test_service.py 的 TEST_SUITE / run_test_suite()。'
    ).classes('text-sm text-orange-600 font-bold').mark('simulated-banner')

    # ------------------------------------------------------------------
    # 测试用例选择区：把"测试用例列表"（展示 name + description）和
    # "测试配置表单"（勾选要跑哪些）合并成同一组控件——每条用例一个
    # ui.checkbox（标题即 name），下方附一行灰色小字展示 description，
    # 这样用户不需要在两处分别查看"有哪些用例"和"我正在勾选什么"。
    # ------------------------------------------------------------------
    ui.label('选择要运行的测试用例').classes('text-lg font-bold mt-4')
    checkboxes: dict[str, ui.checkbox] = {}
    with ui.column().classes('gap-1 w-full'):
        for case in test_service.TEST_SUITE:
            checkbox = ui.checkbox(case.name).mark(f'case-checkbox-{case.id}')
            checkboxes[case.id] = checkbox
            ui.label(case.description).classes('text-xs text-gray-500 ml-8 -mt-1')

    with ui.row().classes('items-center gap-2 mt-4'):
        start_button = ui.button('开始运行').mark('start-button')
        cancel_button = ui.button('取消').mark('cancel-button')
        cancel_button.disable()
        spinner = ui.spinner(size='lg')
        spinner.set_visibility(False)

    status_label = ui.label('尚未开始').classes('text-sm font-bold mt-2').mark('status-label')
    progress_bar = ui.linear_progress(value=0.0).classes('mt-2').mark('progress-bar')
    ui.label('实时日志').classes('text-xs text-gray-500 mt-2')
    log = ui.log(max_lines=200).classes('w-full h-64').mark('run-log')

    # ------------------------------------------------------------------
    # 历史记录区
    # ------------------------------------------------------------------
    ui.label('历史记录').classes('text-lg font-bold mt-6')
    ui.label(
        '以下历史记录保存在进程内存里的模块级列表中，所有浏览器标签页共享、'
        '进程重启后即丢失；真实应用通常会用 app.storage 做持久化/按用户隔离'
        '（这是示例 9 的话题，这里先用最简单的方式讲清示例 8 自身的重点）。'
    ).classes('text-xs text-gray-500').mark('history-storage-hint')

    history_empty_label = ui.label('暂无历史记录。').classes('text-gray-500 italic mt-2')
    history_empty_label.mark('history-empty-state')
    history_empty_label.set_visibility(len(_HISTORY) == 0)

    history_table = (
        ui.table(columns=_HISTORY_COLUMNS, rows=[], row_key='id')
        .classes('w-full mt-2')
        .mark('history-table')
    )
    history_table.set_visibility(len(_HISTORY) > 0)

    detail_dialog = ui.dialog().mark('history-detail-dialog')
    with detail_dialog, ui.card().classes('w-[32rem]'):
        detail_content = ui.column().classes('w-full gap-1')
        ui.button('关闭', on_click=detail_dialog.close)

    def _refresh_history_table() -> None:
        """用 `_HISTORY` 的当前内容重新渲染历史表格（最新一次运行排在最前面）。"""
        rows = [_task_to_history_row(record) for record in reversed(_HISTORY)]
        history_table.rows = rows
        history_table.update()
        history_empty_label.set_visibility(len(rows) == 0)
        history_table.set_visibility(len(rows) > 0)

    def _render_detail(task_id: str) -> None:
        """在详情对话框里展示某条历史记录的完整信息：日志 + result/error。"""
        record = _HISTORY_BY_ID.get(task_id)
        detail_content.clear()
        with detail_content:
            if record is None:
                ui.label(f'未找到任务 {task_id}（历史记录可能已被清空）').classes(
                    'text-red-600'
                )
                detail_dialog.open()
                return

            ui.label(f'任务：{record.name}').classes('text-lg font-bold')
            ui.label('[模拟数据]').classes('text-xs text-orange-600 font-bold')
            ui.label(f'ID: {record.id}')
            ui.label(f'状态: {_STATUS_LABELS[record.status]}（{record.status.value}）')
            ui.label(f'创建时间: {record.created_at.strftime("%Y-%m-%d %H:%M:%S")}')
            if record.started_at is not None:
                ui.label(f'开始时间: {record.started_at.strftime("%Y-%m-%d %H:%M:%S")}')
            if record.finished_at is not None:
                ui.label(f'结束时间: {record.finished_at.strftime("%Y-%m-%d %H:%M:%S")}')
            ui.label(f'进度: {record.progress:.0%}')
            if record.result:
                ui.label(f'结果: {record.result}').classes('text-green-700')
            if record.error:
                ui.label(f'错误: {record.error}').classes('text-red-700')
            ui.label('完整日志:').classes('font-bold mt-2')
            if record.logs:
                with ui.column().classes('gap-0'):
                    for line in record.logs:
                        ui.label(line).classes('text-xs font-mono')
            else:
                ui.label('(无日志)').classes('text-xs text-gray-500')
        detail_dialog.open()

    def _on_history_row_click(event) -> None:
        # event.args 形如 [原生点击事件, 被点击的行 dict, 行索引]（与示例 5 一致）。
        row = event.args[1]
        _render_detail(row['id'])

    history_table.on('rowClick', _on_history_row_click)
    _refresh_history_table()  # 页面刚打开时，先展示已有的（模块级共享）历史。

    # ------------------------------------------------------------------
    # 启动 / 取消 / 驱动运行
    # ------------------------------------------------------------------
    def _set_status(status: TaskStatus) -> None:
        """同步更新状态标签的文案与颜色（见模块级 _STATUS_* 映射表）。"""
        status_label.text = _status_text(status)
        status_label.classes(
            replace=f'text-sm font-bold mt-2 {_STATUS_COLOR_CLASSES[status]}'
        )

    def _notify_result(record: TaskRecord) -> None:
        """任务结束后弹出一次对应状态的 `ui.notify`。"""
        if record.status is TaskStatus.SUCCEEDED:
            ui.notify(f'测试完成：{record.result}', type='positive')
        elif record.status is TaskStatus.FAILED:
            ui.notify(f'测试失败：{record.error}', type='negative')
        elif record.status is TaskStatus.CANCELLED:
            ui.notify('测试已被取消', type='warning')

    async def _drive_run(selected_ids: list[str], cancel_event: asyncio.Event) -> None:
        """驱动一次 `test_service.run_test_suite()`，并把结果同步到界面与历史记录。

        这是真正被 `background_tasks.create()` 启动的协程：`run_test_suite()`
        内部已经完整实现了状态机迁移、进度/日志回调、取消检查点、失败后
        提前结束这几件事，本函数不重新判断任何一条业务规则，只负责把它的
        返回值（或抛出的 `ValidationError`）翻译成界面更新。
        """
        _set_status(TaskStatus.RUNNING)
        try:
            record = await test_service.run_test_suite(
                selected_ids,
                on_progress=lambda value: setattr(progress_bar, 'value', value),
                on_log=log.push,
                cancel_event=cancel_event,
            )
        except ValidationError as exc:
            # 正常情况下 handle_start() 已经在调用前校验过"至少选择一个
            # 用例"，这里不会被触发；保留这一层是为了演示 Service 层抛出
            # 的 ValidationError 永远必须被调用方妥善处理，而不是放任它从
            # background_tasks.create() 启动的协程里一路冒出去。
            status_label.text = f'启动失败：{exc}'
            status_label.classes(replace='text-sm font-bold mt-2 text-red-700')
            ui.notify(f'启动失败：{exc}', type='negative')
        else:
            _set_status(record.status)
            _HISTORY.append(record)
            _HISTORY_BY_ID[record.id] = record
            _refresh_history_table()
            _notify_result(record)
        finally:
            state.is_task_running = False
            start_button.enable()
            cancel_button.disable()
            spinner.set_visibility(False)

    def handle_start() -> None:
        """"开始运行"按钮的点击回调：收集勾选结果，交给后台协程执行。"""
        selected_ids = [case_id for case_id, checkbox in checkboxes.items() if checkbox.value]
        if not selected_ids:
            ui.notify('请至少勾选一个测试用例', type='warning')
            return
        if state.is_task_running:
            # 理论上走不到这里，因为按钮在任务运行期间已被禁用；保留这一层
            # 纯粹是防御性兜底，不依赖它作为"是否允许启动"的唯一判断依据。
            ui.notify('已有一个任务正在运行，请先等待其结束或点击取消', type='warning')
            return

        state.is_task_running = True
        # 每次开始都用一枚全新的 Event，避免复用上一次运行用过、可能已经
        # 被 set() 过的旧对象（与示例 7 的 _PageState 是同一个教训）。
        state.cancel_event = asyncio.Event()
        progress_bar.value = 0.0
        log.clear()
        _set_status(TaskStatus.PENDING)

        # 立即禁用"开始运行"，防止用户在任务还没跑完时重复点击、意外启动
        # 第二个并发任务——这正是模块顶部文档里讨论的"为什么不能只用一个
        # 全局 running: bool"问题的另一面：即使有了 TaskRecord，界面上也
        # 仍然需要一层简单的按钮门闩来避免真的产生两个并发任务。
        start_button.disable()
        cancel_button.enable()
        spinner.set_visibility(True)

        # 唯一正确的"即发即忘"启动方式：background_tasks.create()。
        # 绝不要裸用 asyncio.create_task(...)——原因见示例 7 文档。
        background_tasks.create(
            _drive_run(selected_ids, state.cancel_event),
            name='example-08-test-run',
        )

    def handle_cancel() -> None:
        """"取消"按钮的点击回调：只设置标志位，其余收尾交给 `_drive_run()`。"""
        state.cancel_event.set()

    start_button.on_click(handle_start)
    cancel_button.on_click(handle_cancel)


if __name__ in {'__main__', '__mp_main__'}:
    ui.run(title='示例 8：综合自动化测试控制台')
