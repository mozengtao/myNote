"""“任务与测试”面板：精简复用示例 5（任务表格）与示例 8（自动化测试控制台）。

两个子标签页各自只复用对应的 Service，不重新实现任何分页/筛选/状态机
逻辑：
- “任务列表”标签页复用 `services/task_query_service.py`（与
  `examples/05_task_table.py` 共享同一套模拟数据生成/分页/筛选函数）。
- “运行测试”标签页复用 `services/test_service.py`（与
  `examples/08_test_console.py` 共享同一套 `TEST_SUITE`/`run_test_suite`），
  但省略了示例 8 完整的历史记录/详情对话框——那部分留给
  `examples/08_test_console.py` 和 `examples/09_multi_user_state.py`
  （按用户隔离的版本）去完整演示。
"""

from __future__ import annotations

import asyncio
import logging

from nicegui import background_tasks, ui

from models.task import TaskStatus
from services import test_service
from services.errors import ValidationError
from services.task_query_service import generate_mock_tasks, list_tasks

logger = logging.getLogger(__name__)

#: 与 examples/05_task_table.py 使用相同的 seed，保证两处展示的是同一份
#: 可复现的模拟数据——这不是巧合，而是"同一个 Service、同一份数据源"的
#: 直接体现。
_MOCK_TASK_COUNT = 200
_DATA_SOURCE = generate_mock_tasks(_MOCK_TASK_COUNT, seed=42)

_PAGE_SIZE = 10
_STATUS_FILTER_OPTIONS = ['全部'] + [status.value for status in TaskStatus]

_TASK_TABLE_COLUMNS = [
    {'name': 'id', 'label': 'ID', 'field': 'id', 'align': 'left'},
    {'name': 'name', 'label': '名称', 'field': 'name', 'align': 'left'},
    {'name': 'status', 'label': '状态', 'field': 'status', 'align': 'left'},
]


def _row_for(record) -> dict:
    return {'id': record.id, 'name': record.name, 'status': record.status.value}


def _build_task_list_tab() -> None:
    """“任务列表”子标签页：下拉筛选 + 分页表格，均经由 `list_tasks()` 查询。"""
    ui.label(
        f'{_MOCK_TASK_COUNT} 条固定模拟任务（is_simulated=True），'
        '与示例 5 共享同一个随机种子生成的数据源。'
    ).classes('text-xs text-gray-500').mark('task-list-description')

    status_select = ui.select(
        options=_STATUS_FILTER_OPTIONS, value='全部', label='按状态筛选'
    ).classes('w-48').mark('task-list-status-select')

    state = {'page': 1}

    def _query_and_refresh() -> None:
        status_filter = None if status_select.value == '全部' else TaskStatus(status_select.value)
        try:
            page_items, total = list_tasks(
                _DATA_SOURCE, page=state['page'], page_size=_PAGE_SIZE, status_filter=status_filter
            )
        except ValidationError as exc:
            logger.warning('task_panel: 分页参数不合法 (%s)', exc)
            return
        table.rows = [_row_for(record) for record in page_items]
        table.pagination = {**table.pagination, 'page': state['page'], 'rowsNumber': total}

    def _handle_pagination_change(event) -> None:
        # Quasar 表格翻页/排序时，把新的分页参数放进 event.args 回传过来
        # （与 examples/05_task_table.py 的 `_on_pagination_change` 做法一致）。
        state['page'] = event.args.get('page', state['page'])
        _query_and_refresh()

    def _handle_status_change(_event) -> None:
        state['page'] = 1
        _query_and_refresh()

    table = ui.table(
        columns=_TASK_TABLE_COLUMNS,
        rows=[],
        row_key='id',
        pagination={'rowsPerPage': _PAGE_SIZE, 'page': 1},
        on_pagination_change=_handle_pagination_change,
    ).classes('w-full mt-2').mark('task-list-table')

    status_select.on_value_change(_handle_status_change)
    _query_and_refresh()


def _build_test_runner_tab() -> None:
    """“运行测试”子标签页：勾选用例 -> 后台运行 -> 实时进度/日志。"""
    ui.label(
        '以下测试用例全部来自 services/test_service.py 的 TEST_SUITE，'
        '纯内存模拟，不连接任何真实设备（详见示例 7/8 的文档）。'
    ).classes('text-xs text-gray-500').mark('test-runner-description')

    checkboxes: dict[str, ui.checkbox] = {}
    with ui.column().classes('gap-0 mt-2'):
        for case in test_service.TEST_SUITE:
            checkboxes[case.id] = ui.checkbox(case.name).mark(f'test-runner-checkbox-{case.id}')

    cancel_event_box = {'event': asyncio.Event(), 'running': False}

    with ui.row().classes('items-center gap-2 mt-2'):
        start_button = ui.button('开始运行').mark('test-runner-start-button')
        cancel_button = ui.button('取消').mark('test-runner-cancel-button')
        cancel_button.disable()

    status_label = ui.label('尚未开始').classes('text-sm font-bold mt-2').mark('test-runner-status-label')
    progress_bar = ui.linear_progress(value=0.0).classes('mt-2').mark('test-runner-progress-bar')
    log = ui.log(max_lines=100).classes('w-full h-40 mt-2').mark('test-runner-log')

    async def _drive_run(selected_ids: list[str], cancel_event: asyncio.Event) -> None:
        status_label.text = '[模拟] 运行中…'
        try:
            record = await test_service.run_test_suite(
                selected_ids,
                on_progress=lambda value: setattr(progress_bar, 'value', value),
                on_log=log.push,
                cancel_event=cancel_event,
            )
        except ValidationError as exc:
            status_label.text = f'启动失败：{exc}'
            status_label.classes(replace='text-sm font-bold mt-2 text-red-700')
            logger.warning('task_panel: 测试启动失败 (%s)', exc)
        else:
            status_label.text = f'[模拟] 已结束：{record.status.value}'
            status_label.classes(replace='text-sm font-bold mt-2')
            logger.info('task_panel: 测试运行结束 status=%s', record.status.value)
        finally:
            cancel_event_box['running'] = False
            start_button.enable()
            cancel_button.disable()

    def handle_start() -> None:
        selected_ids = [case_id for case_id, checkbox in checkboxes.items() if checkbox.value]
        if not selected_ids:
            ui.notify('请至少勾选一个测试用例', type='warning')
            return
        if cancel_event_box['running']:
            return

        cancel_event_box['running'] = True
        cancel_event_box['event'] = asyncio.Event()
        progress_bar.value = 0.0
        log.clear()
        start_button.disable()
        cancel_button.enable()

        background_tasks.create(
            _drive_run(selected_ids, cancel_event_box['event']),
            name='app-task-panel-test-run',
        )

    def handle_cancel() -> None:
        cancel_event_box['event'].set()

    start_button.on_click(handle_start)
    cancel_button.on_click(handle_cancel)


def build_task_panel() -> None:
    """构建"任务与测试"面板：两个子标签页，分别对接两个独立的 Service。"""
    with ui.tabs().classes('w-full') as tabs:
        list_tab = ui.tab('任务列表').mark('task-panel-tab-list')
        test_tab = ui.tab('运行测试').mark('task-panel-tab-test')

    with ui.tab_panels(tabs, value=list_tab).classes('w-full'):
        with ui.tab_panel(list_tab):
            _build_task_list_tab()
        with ui.tab_panel(test_tab):
            _build_test_runner_tab()
