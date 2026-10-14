"""“命令控制台”面板：`app.py` 里对示例 6（`examples/06_command_console.py`）
命令执行区的精简复用（不含示例 6 的"文件查看区"，那一部分留给独立示例）。

本文件不实现任何新的安全规则：能跑什么、参数怎么校验，完全由
`services/command_service.py` 的 `COMMAND_WHITELIST` 决定，面板只负责
"收集输入 -> 调用 Service -> 展示结果/翻译异常"，与示例 6 的职责划分完全
一致（见 `docs/examples/06_command_console.md`）。
"""

from __future__ import annotations

import logging

from nicegui import run, ui

from services import command_service
from services.errors import (
    CommandExecutionError,
    CommandTimeoutError,
    PermissionDeniedError,
    ValidationError,
)

logger = logging.getLogger(__name__)


def _available_commands() -> dict:
    """只返回在当前机器上真正可用（找到了可执行文件）的白名单命令。"""
    return {
        name: spec
        for name, spec in command_service.COMMAND_WHITELIST.items()
        if spec.executable is not None
    }


def build_command_panel() -> None:
    """构建命令控制台面板：下拉框 + 动态参数输入 + 执行按钮 + 结果展示。"""
    ui.label(
        '只能执行 services/command_service.py 里登记过的白名单命令；'
        '参数要么限定为固定选项，要么必须匹配专门的正则表达式。'
    ).classes('text-sm text-gray-600').mark('command-panel-description')

    available = _available_commands()
    if not available:
        ui.label('当前机器上没有任何可用的白名单命令。').classes(
            'text-red-600 font-bold'
        ).mark('command-panel-no-commands')
        return

    command_names = list(available.keys())
    command_select = (
        ui.select(options=command_names, value=command_names[0], label='选择命令')
        .classes('w-72')
        .mark('command-panel-select')
    )
    command_hint = ui.label().classes('text-xs text-gray-500').mark('command-panel-hint')

    # 与 examples/06_command_console.py 相同的写法：用一个单元素字典持有
    # "当前参数输入控件"的引用，绕开闭包不能给外层简单变量重新赋值的限制。
    arg_widget_box: dict[str, ui.element | None] = {'widget': None}
    arg_container = ui.column().classes('w-72')

    def rebuild_arg_input() -> None:
        spec = available[command_select.value]
        command_hint.text = spec.description
        arg_container.clear()
        with arg_container:
            if spec.arg_spec is None:
                ui.label('该命令不需要任何参数。').classes('text-xs text-gray-500')
                arg_widget_box['widget'] = None
            elif spec.arg_spec.kind == 'choice':
                choices = list(spec.arg_spec.choices)
                widget = ui.select(
                    options=choices,
                    value=choices[0] if choices else None,
                    label=spec.arg_spec.label,
                ).mark('command-panel-arg-input')
                arg_widget_box['widget'] = widget
            else:
                widget = ui.input(
                    label=spec.arg_spec.label,
                    placeholder=spec.arg_spec.error_hint,
                ).mark('command-panel-arg-input')
                arg_widget_box['widget'] = widget

    command_select.on_value_change(lambda _event: rebuild_arg_input())
    rebuild_arg_input()

    with ui.row().classes('items-center gap-2 mt-2'):
        run_button = ui.button('执行').mark('command-panel-run-button')
        spinner = ui.spinner(size='lg')
        spinner.set_visibility(False)

    status_label = ui.label().classes('text-sm mt-2').mark('command-panel-status-label')
    stdout_log = ui.log(max_lines=100).classes('w-full h-32 mt-2').mark('command-panel-stdout-log')

    async def handle_run() -> None:
        spec = available[command_select.value]
        widget = arg_widget_box['widget']
        user_args: list[str] = [] if widget is None or widget.value is None else [str(widget.value)]

        run_button.disable()
        spinner.set_visibility(True)
        status_label.text = '正在执行…'
        stdout_log.clear()

        try:
            # 阻塞调用，必须丢进线程池执行，原因见 examples/06 对应文档。
            result = await run.io_bound(
                command_service.run_command, command_select.value, user_args
            )
        except (PermissionDeniedError, ValidationError, CommandTimeoutError, CommandExecutionError) as exc:
            status_label.text = f'执行失败：{exc}'
            status_label.classes(replace='text-sm mt-2 text-red-700')
            logger.warning('command_panel: 执行失败 command=%r error=%s', command_select.value, exc)
            ui.notify(f'执行失败：{exc}', type='negative')
        else:
            process_result = result.process_result
            status_label.text = f'执行完成，退出码 {process_result.returncode}，耗时 {process_result.duration_seconds:.3f} 秒'
            status_label.classes(replace='text-sm mt-2 text-green-700')
            for line in process_result.stdout.splitlines():
                stdout_log.push(line)
            for line in process_result.stderr.splitlines():
                stdout_log.push(f'[stderr] {line}')
            logger.info(
                'command_panel: 执行完成 command=%r returncode=%d',
                command_select.value,
                process_result.returncode,
            )
        finally:
            run_button.enable()
            spinner.set_visibility(False)

    run_button.on_click(handle_run)
