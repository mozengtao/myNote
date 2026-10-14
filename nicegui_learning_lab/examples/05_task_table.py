"""示例 5：任务表格（分页 / 筛选 / 详情，全部基于模拟数据）。

本示例演示一个"数据量较大，不能一次性塞进浏览器"的典型场景该怎么用
`ui.table` 处理：

- "数据源"（`_DATA_SOURCE`）：`generate_mock_tasks()` 生成的完整模拟任务
  列表，一共 200 条，全部标注为 `is_simulated=True`，存放在模块级变量里，
  代表"后端持有的全部数据"。
- "查询结果"：每次翻页/筛选/排序时，调用一次
  `services.task_query_service.list_tasks()`，只拿回"当前这一页"应该
  展示的那一小部分记录，以及过滤后的总条数。
- "UI 当前展示的那一页"：`ui.table` 组件的 `rows`，任何时刻都只包含
  "查询结果"里的那一小部分数据，而不是全部 200 条。

这三者的关系是：数据源不会变（本示例里是只读的模拟数据），查询结果随着
用户操作（翻页、筛选、排序）不断变化，UI 展示的永远是最新一次查询结果。
"""

from __future__ import annotations

import logging
import sys
from pathlib import Path

# 集成阶段补丁：原因与做法见 examples/04_config_checker.py 顶部的同名注释
# 和 docs/multitask_plan.md 的"集成阶段修正记录"一节。
_PROJECT_ROOT = Path(__file__).resolve().parent.parent
if str(_PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(_PROJECT_ROOT))

from nicegui import ui

from models.task import TaskRecord, TaskStatus
from services.errors import ServiceError
from services.task_query_service import generate_mock_tasks, list_tasks

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s %(levelname)s %(name)s: %(message)s',
)
logger = logging.getLogger(__name__)

# ---------------------------------------------------------------------------
# 模拟数据源。只在模块导入时生成一次，代表"后端持有的全部任务记录"。
# 这里的 200 条 TaskRecord 全部是 generate_mock_tasks() 产出的模拟数据
# （is_simulated=True），不是任何真实任务系统的记录。
# ---------------------------------------------------------------------------
_MOCK_TASK_COUNT = 200
_DATA_SOURCE: list[TaskRecord] = generate_mock_tasks(_MOCK_TASK_COUNT, seed=42)
_TASKS_BY_ID: dict[str, TaskRecord] = {task.id: task for task in _DATA_SOURCE}

_DEFAULT_ROWS_PER_PAGE = 10

#: ui.table 的 columns：每个字典里的 'field' 对应 rows 里字典的 key。
_COLUMNS = [
    {'name': 'id', 'label': 'ID', 'field': 'id', 'sortable': True, 'align': 'left'},
    {'name': 'name', 'label': '名称', 'field': 'name', 'sortable': True, 'align': 'left'},
    {'name': 'status', 'label': '状态', 'field': 'status', 'sortable': True, 'align': 'left'},
    {
        'name': 'created_at',
        'label': '创建时间',
        'field': 'created_at',
        'sortable': True,
        'align': 'left',
    },
]

_STATUS_FILTER_OPTIONS = ['全部'] + [status.value for status in TaskStatus]


def _task_to_row(task: TaskRecord) -> dict:
    """把一个 TaskRecord（Python 对象）转换成 ui.table 需要的一行 dict。

    `ui.table` 的 `rows` 是"字典列表"，字典的 key 要和 `columns` 里的
    `field` 对应。这里只挑选表格要展示的几个字段；其余字段（logs、
    result、error 等）留到用户点击某一行、打开详情对话框时，再按 id 去
    `_TASKS_BY_ID` 里查出完整的 TaskRecord 展示。
    """
    return {
        'id': task.id,
        'name': task.name,
        'status': task.status.value,
        'created_at': task.created_at.strftime('%Y-%m-%d %H:%M'),
    }


def _sorted_source(sort_by: str | None, descending: bool) -> list[TaskRecord]:
    """按表头点击选择的字段对数据源排序，返回一份新列表，不修改 `_DATA_SOURCE`。

    `list_tasks()` 的签名里故意没有排序参数——它只负责"过滤 + 分页"这两件
    容易单独做单元测试的事。排序更偏"视图展示"的需求，所以放在示例层：
    先对完整数据源排序，再把排序后的列表交给 `list_tasks()` 做过滤和分页。
    """
    if not sort_by:
        return _DATA_SOURCE

    key_funcs = {
        'id': lambda task: task.id,
        'name': lambda task: task.name,
        'status': lambda task: task.status.value,
        'created_at': lambda task: task.created_at,
    }
    key_func = key_funcs.get(sort_by)
    if key_func is None:
        return _DATA_SOURCE
    return sorted(_DATA_SOURCE, key=key_func, reverse=descending)


class _TableState:
    """保存"这一个浏览器会话当前表格展示状态"的小容器。

    `task_table_page()` 每次被访问都会创建一份新的 `_TableState` 实例，
    所以不同浏览器标签页之间的分页/筛选状态互不影响；真正的"数据源"
    （`_DATA_SOURCE`）才是模块级共享、只读的。
    """

    def __init__(self) -> None:
        self.page = 1
        self.rows_per_page = _DEFAULT_ROWS_PER_PAGE
        self.sort_by: str | None = None
        self.descending = False
        self.status_filter: TaskStatus | None = None
        self.simulate_error_once = False


def _query_current_page(state: _TableState) -> tuple[list[dict], int, str | None]:
    """根据当前状态向 `task_query_service` 查询这一页数据。

    Returns:
        `(表格要展示的 rows, 过滤后的总条数, 错误信息或 None)`。
        `simulate_error_once` 用于演示"加载失败"这种错误状态：触发一次之后
        立刻复位，不会一直卡在错误状态，方便用户紧接着再点"刷新"恢复正常。
    """
    if state.simulate_error_once:
        state.simulate_error_once = False
        return [], 0, '加载失败（这是人为模拟的一次错误，用于演示错误状态展示；再次点击"刷新"即可恢复）'

    try:
        source = _sorted_source(state.sort_by, state.descending)
        page_tasks, total = list_tasks(
            source,
            page=state.page,
            page_size=state.rows_per_page,
            status_filter=state.status_filter,
        )
    except ServiceError as exc:
        logger.warning('查询任务列表失败: %s', exc)
        return [], 0, str(exc)

    rows = [_task_to_row(task) for task in page_tasks]
    return rows, total, None


@ui.page('/')
def task_table_page() -> None:
    """任务表格页面：分页表格 + 状态筛选 + 行详情对话框，全部基于模拟数据。"""
    state = _TableState()

    ui.label('任务表格（模拟数据）').classes('text-2xl font-bold')
    ui.label(
        f'下方 {_MOCK_TASK_COUNT} 条任务记录全部由 generate_mock_tasks() 生成，'
        '是用于演示分页/筛选的【模拟数据】，不代表任何真实任务执行结果。'
    ).classes('text-sm text-orange-600 font-bold').mark('simulated-data-banner')

    status_message = ui.label('').classes('text-sm text-gray-600 mt-1').mark('status-message')

    empty_state_label = ui.label('没有符合筛选条件的任务。').classes('text-gray-500 italic mt-4')
    empty_state_label.set_visibility(False)
    empty_state_label.mark('empty-state')

    error_state_label = ui.label('').classes('text-red-600 font-bold mt-4')
    error_state_label.set_visibility(False)
    error_state_label.mark('error-state')

    def _apply_query_result() -> None:
        """重新查询当前状态对应的那一页数据，并刷新表格 / 空状态 / 错误状态展示。"""
        rows, total, error_message = _query_current_page(state)

        if error_message is not None:
            table.set_visibility(False)
            empty_state_label.set_visibility(False)
            error_state_label.text = error_message
            error_state_label.set_visibility(True)
            status_message.text = ''
            return

        error_state_label.set_visibility(False)

        if total == 0:
            table.set_visibility(False)
            empty_state_label.set_visibility(True)
        else:
            table.set_visibility(True)
            empty_state_label.set_visibility(False)

        table.rows = rows
        table.pagination = {
            'rowsPerPage': state.rows_per_page,
            'page': state.page,
            'sortBy': state.sort_by,
            'descending': state.descending,
            'rowsNumber': total,
        }
        table.update()

        filter_label = status_select.value
        status_message.text = f'共 {total} 条（筛选条件: {filter_label}），第 {state.page} 页'

    def _on_pagination_change(event) -> None:
        """翻页/排序时，Quasar 表格会把新的分页参数放进 `event.args` 回传过来。"""
        new_pagination = event.args
        state.page = new_pagination.get('page', state.page)
        state.rows_per_page = new_pagination.get('rowsPerPage', state.rows_per_page)
        state.sort_by = new_pagination.get('sortBy')
        state.descending = bool(new_pagination.get('descending', False))
        _apply_query_result()

    table = (
        ui.table(
            columns=_COLUMNS,
            rows=[],
            row_key='id',
            pagination={
                'rowsPerPage': state.rows_per_page,
                'page': state.page,
                'sortBy': None,
                'descending': False,
            },
            on_pagination_change=_on_pagination_change,
        )
        .classes('w-full mt-2')
        .mark('task-table')
    )

    detail_dialog = ui.dialog().mark('task-detail-dialog')
    with detail_dialog, ui.card().classes('w-96'):
        detail_content = ui.column().classes('w-full gap-1')
        ui.button('关闭', on_click=detail_dialog.close)

    def _render_detail(task_id: str) -> None:
        """在详情对话框里展示某条任务记录的完整信息，包括 logs/result/error。"""
        task = _TASKS_BY_ID.get(task_id)
        detail_content.clear()
        with detail_content:
            if task is None:
                ui.label(f'未找到任务 {task_id}（可能数据源已刷新）').classes('text-red-600')
                detail_dialog.open()
                return

            ui.label(f'任务：{task.name}').classes('text-lg font-bold')
            if task.is_simulated:
                ui.label('【模拟数据】').classes('text-xs text-orange-600 font-bold')
            ui.label(f'ID: {task.id}')
            ui.label(f'状态: {task.status.value}')
            ui.label(f'创建时间: {task.created_at.strftime("%Y-%m-%d %H:%M:%S")}')
            if task.started_at is not None:
                ui.label(f'开始时间: {task.started_at.strftime("%Y-%m-%d %H:%M:%S")}')
            if task.finished_at is not None:
                ui.label(f'结束时间: {task.finished_at.strftime("%Y-%m-%d %H:%M:%S")}')
            if task.result:
                ui.label(f'结果: {task.result}').classes('text-green-700')
            if task.error:
                ui.label(f'错误: {task.error}').classes('text-red-700')
            ui.label('日志:').classes('font-bold mt-2')
            if task.logs:
                with ui.column().classes('gap-0'):
                    for line in task.logs:
                        ui.label(line).classes('text-xs font-mono')
            else:
                ui.label('(无日志)').classes('text-xs text-gray-500')
        detail_dialog.open()

    def _on_row_click(event) -> None:
        # event.args 形如 [原生点击事件, 被点击的行 dict, 行索引]。
        row = event.args[1]
        _render_detail(row['id'])

    table.on('rowClick', _on_row_click)

    def _refresh() -> None:
        _apply_query_result()

    def _simulate_error() -> None:
        state.simulate_error_once = True
        _refresh()
        ui.notify('已触发一次模拟加载失败', type='warning')

    def _on_status_filter_change() -> None:
        value = status_select.value
        state.status_filter = None if value == '全部' else TaskStatus(value)
        state.page = 1
        _refresh()

    with ui.row().classes('items-center gap-4 mt-2'):
        status_select = (
            ui.select(
                options=_STATUS_FILTER_OPTIONS,
                value='全部',
                label='按状态筛选',
                on_change=_on_status_filter_change,
            )
            .classes('w-48')
            .mark('status-filter-select')
        )
        ui.button('刷新', on_click=_refresh).mark('refresh-button')
        ui.button('模拟一次加载失败', on_click=_simulate_error).props(
            'outline color=negative'
        ).mark('simulate-error-button')

    _apply_query_result()


if __name__ in {'__main__', '__mp_main__'}:
    ui.run(title='示例 5：任务表格')
