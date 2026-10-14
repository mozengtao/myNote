"""示例 4：JSON 配置检查器。

这个示例演示两条读取配置的路径：
1. 浏览器端上传一个 JSON 文件（`ui.upload`），内容只在内存里处理，
   从不落盘到 `config/` 目录；
2. 从服务端 `config/` 目录下选择一个已存在的文件名去读取——这条路径必须
   经过 `services/config_service.resolve_config_path()` 的目录越界检查，
   绝不能把用户输入的文件名直接拼成路径。

两条路径最终都会把"原始文本"或"文件路径"交给 `services/config_service`
去解析、校验，UI 代码本身完全不知道 JSON 要怎么解析、一份合法配置应该
长什么样——这些规则的唯一来源是 `services/config_service.py`。
"""

from __future__ import annotations

import json
import logging
import sys
from pathlib import Path

# 集成阶段补丁：直接用 `python3 examples/04_config_checker.py` 运行时，
# 项目根目录默认不在 sys.path 里，会导致 `from services...` 抛出
# `ModuleNotFoundError: No module named 'services'`（这是协调者在集成阶段
# 对全部示例做无头冒烟测试时真实复现、而不是凭空猜测的问题，详见
# docs/multitask_plan.md 的"集成阶段修正记录"一节）。这里显式把项目根目录
# 插入 sys.path，这样无论从哪个工作目录运行本文件都能正确找到
# `models`/`services`/`infrastructure` 这几个包。
_PROJECT_ROOT = Path(__file__).resolve().parent.parent
if str(_PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(_PROJECT_ROOT))

from nicegui import events, ui

from services.config_service import (
    load_json_config,
    parse_json_text,
    resolve_config_path,
    validate_config,
)
from services.errors import NotFoundError, PermissionDeniedError, ValidationError

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s %(levelname)s %(name)s: %(message)s',
)
logger = logging.getLogger(__name__)

#: 本示例唯一允许服务端路径读取访问的基准目录。
CONFIG_DIR = Path(__file__).resolve().parent.parent / 'config'

#: config/example.json 中真实存在的字段，作为本示例的校验规则。
REQUIRED_FIELDS = ['name', 'host', 'port', 'environment']


def _render_outcome(
    container: ui.column,
    *,
    source_label: str,
    data: dict | None = None,
    errors: list[str] | None = None,
    failure_kind: str | None = None,
    failure_message: str | None = None,
) -> None:
    """把一次"读取 + 校验"的结果渲染进 `container`。

    `failure_kind` 用于区分三种失败的展示文案：'not_found' / 'permission' /
    'validation'；`data` 非 None 时代表解析成功，接下来展示 `errors`
    （`validate_config()` 的返回值，空列表代表校验通过）。
    """
    container.clear()
    with container:
        ui.label(f'来源: {source_label}').classes('text-sm text-gray-500')

        if failure_kind is not None:
            kind_label = {
                'not_found': '文件不存在',
                'permission': '权限不足 / 路径越界',
                'validation': 'JSON 语法错误',
            }.get(failure_kind, failure_kind)
            ui.label(f'[{kind_label}] {failure_message}').classes(
                'text-red-600 font-bold'
            ).mark('outcome-error')
            return

        assert data is not None
        formatted = json.dumps(data, indent=2, ensure_ascii=False)
        ui.code(formatted, language='json').classes('w-full').mark('outcome-json')

        if errors:
            ui.label(f'校验未通过，共发现 {len(errors)} 个问题：').classes(
                'text-orange-600 font-bold'
            )
            with ui.column().classes('gap-0').mark('outcome-validation-errors'):
                for message in errors:
                    ui.label(f'· {message}').classes('text-orange-600')
        else:
            ui.label('校验通过：所有必填字段均存在且取值合法').classes(
                'text-green-600 font-bold'
            ).mark('outcome-validation-ok')


def _handle_server_path_lookup(filename: str, container: ui.column) -> None:
    """处理"从 config/ 目录选择文件名"这条路径。"""
    if not filename:
        ui.notify('请先选择一个文件名', type='warning')
        return

    try:
        path = resolve_config_path(filename, CONFIG_DIR)
        data = load_json_config(path)
    except NotFoundError as exc:
        logger.info('配置文件未找到: %s', exc)
        _render_outcome(
            container,
            source_label=f'服务端 config/{filename}',
            failure_kind='not_found',
            failure_message=str(exc),
        )
        return
    except PermissionDeniedError as exc:
        logger.warning('配置文件访问被拒绝: %s', exc)
        _render_outcome(
            container,
            source_label=f'服务端 config/{filename}',
            failure_kind='permission',
            failure_message=str(exc),
        )
        return
    except ValidationError as exc:
        logger.warning('配置文件 JSON 语法错误: %s', exc)
        _render_outcome(
            container,
            source_label=f'服务端 config/{filename}',
            failure_kind='validation',
            failure_message=str(exc),
        )
        return

    errors = validate_config(data, REQUIRED_FIELDS)
    _render_outcome(
        container,
        source_label=f'服务端 config/{filename}',
        data=data,
        errors=errors,
    )
    ui.notify('读取成功' if not errors else '读取成功，但校验未通过', type='positive' if not errors else 'warning')


def _handle_upload(event: events.UploadEventArguments, container: ui.column) -> None:
    """处理浏览器端上传文件：内容只在内存里处理，从不写入 `config/` 目录。"""
    raw_bytes = event.content.read()
    try:
        text = raw_bytes.decode('utf-8')
    except UnicodeDecodeError as exc:
        logger.warning('上传文件不是合法的 UTF-8 文本: %s', exc)
        _render_outcome(
            container,
            source_label=f'浏览器上传: {event.name}',
            failure_kind='validation',
            failure_message=f'文件不是合法的 UTF-8 文本: {exc}',
        )
        return

    try:
        data = parse_json_text(text)
    except ValidationError as exc:
        logger.warning('上传文件 JSON 语法错误: %s', exc)
        _render_outcome(
            container,
            source_label=f'浏览器上传: {event.name}',
            failure_kind='validation',
            failure_message=str(exc),
        )
        return

    errors = validate_config(data, REQUIRED_FIELDS)
    _render_outcome(
        container,
        source_label=f'浏览器上传: {event.name}',
        data=data,
        errors=errors,
    )
    ui.notify('读取成功' if not errors else '读取成功，但校验未通过', type='positive' if not errors else 'warning')


def _list_config_filenames() -> list[str]:
    """列出 `config/` 目录下可供选择的文件名（仅文件名，不含路径）。"""
    if not CONFIG_DIR.is_dir():
        return []
    return sorted(p.name for p in CONFIG_DIR.iterdir() if p.is_file())


@ui.page('/')
def config_checker_page() -> None:
    """配置检查器页面：一个上传入口 + 一个服务端文件选择入口，共用同一块结果展示区域。"""
    ui.label('JSON 配置检查器').classes('text-2xl font-bold')
    ui.label(
        '左侧：从浏览器上传任意 JSON 文件（内容只在内存中处理，不会写入服务器的 config/ 目录）。'
        '右侧：从服务器 config/ 目录中选择一个已存在的文件读取（文件名会被限制在 config/ 目录内，'
        '无法通过 ".." 或绝对路径访问目录之外的文件）。'
    ).classes('text-sm text-gray-600 max-w-2xl')

    result_container = ui.column().classes('w-full mt-4').mark('result-container')

    with ui.row().classes('w-full gap-8 items-start'):
        with ui.card().classes('w-80'):
            ui.label('方式一：浏览器上传').classes('font-bold')
            ui.upload(
                on_upload=lambda e: _handle_upload(e, result_container),
                auto_upload=True,
            ).props('accept=.json').classes('w-full').mark('upload-widget')

        with ui.card().classes('w-80'):
            ui.label('方式二：服务端 config/ 目录').classes('font-bold')
            filenames = _list_config_filenames()
            selected = ui.select(
                options=filenames,
                value=filenames[0] if filenames else None,
                label='选择文件名',
            ).classes('w-full').mark('server-file-select')
            ui.button(
                '读取并校验',
                on_click=lambda: _handle_server_path_lookup(selected.value, result_container),
            ).classes('mt-2').mark('server-file-submit')
            ui.label(
                '提示：可以选择 example_invalid.json 来演示 JSON 语法错误的展示效果。'
            ).classes('text-xs text-gray-500 mt-1')


if __name__ in {'__main__', '__mp_main__'}:
    ui.run(title='示例 4：JSON 配置检查器')
