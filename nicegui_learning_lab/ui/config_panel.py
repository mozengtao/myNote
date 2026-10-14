"""“配置检查”面板：`app.py` 里对示例 4（`examples/04_config_checker.py`）
服务端路径分支的精简复用。

本面板只复用服务端 `config/` 目录这一条路径（不包含示例 4 里 `ui.upload`
浏览器端上传的另一条路径）——`app.py` 的定位是"把已经讲清楚的心智模型
组装成一个真实可用的小工具"，不是重新演示一遍教学内容；需要完整理解两条
路径区别的读者应该去看 `examples/04_config_checker.py` 和
`docs/examples/04_config_checker.md`。
"""

from __future__ import annotations

import json
import logging
from pathlib import Path

from nicegui import ui

from services.config_service import load_json_config, resolve_config_path, validate_config
from services.errors import NotFoundError, PermissionDeniedError, ValidationError

logger = logging.getLogger(__name__)

#: 与 `examples/04_config_checker.py` 完全一致的基准目录与必填字段约定。
CONFIG_DIR = Path(__file__).resolve().parents[1] / 'config'
REQUIRED_FIELDS = ['name', 'host', 'port']


def _list_config_files() -> list[str]:
    """列出 `config/` 目录下的 `.json` 文件名（只读遍历自己的目录，不涉及
    任何外部输入，因此不需要经过 `confine_path()`）。
    """
    if not CONFIG_DIR.is_dir():
        return []
    return sorted(path.name for path in CONFIG_DIR.glob('*.json'))


def build_config_panel() -> None:
    """构建配置检查面板：选择 `config/` 下的文件 -> 读取 -> 校验 -> 展示结果。"""
    ui.label(
        '从 config/ 目录选择一个 JSON 文件，读取并校验是否包含'
        f' {REQUIRED_FIELDS} 这几个必填字段。'
    ).classes('text-sm text-gray-600').mark('config-panel-description')

    files = _list_config_files()
    if not files:
        ui.label('config/ 目录下没有找到任何 .json 文件。').classes(
            'text-red-600'
        ).mark('config-panel-no-files')
        return

    file_select = (
        ui.select(options=files, value=files[0], label='选择配置文件')
        .classes('w-72')
        .mark('config-panel-select')
    )
    check_button = ui.button('读取并校验').mark('config-panel-check-button')

    result_label = ui.label().classes('text-sm font-bold mt-2').mark('config-panel-result-label')
    content_view = ui.code('').classes('w-full mt-2').mark('config-panel-content-view')
    error_list_container = ui.column().classes('gap-1 mt-2').mark('config-panel-error-list')

    def handle_check() -> None:
        filename = file_select.value
        error_list_container.clear()
        content_view.content = ''

        try:
            path = resolve_config_path(filename, CONFIG_DIR)
            data = load_json_config(path)
        except NotFoundError as exc:
            result_label.text = f'文件不存在：{exc}'
            result_label.classes(replace='text-sm font-bold mt-2 text-red-700')
            logger.warning('config_panel: 文件不存在 filename=%r (%s)', filename, exc)
            return
        except PermissionDeniedError as exc:
            result_label.text = f'路径不合法：{exc}'
            result_label.classes(replace='text-sm font-bold mt-2 text-red-700')
            logger.warning('config_panel: 路径越界 filename=%r (%s)', filename, exc)
            return
        except ValidationError as exc:
            result_label.text = f'JSON 语法错误：{exc}'
            result_label.classes(replace='text-sm font-bold mt-2 text-red-700')
            logger.warning('config_panel: JSON 语法错误 filename=%r (%s)', filename, exc)
            return

        content_view.content = json.dumps(data, ensure_ascii=False, indent=2)
        errors = validate_config(data, REQUIRED_FIELDS)
        if errors:
            result_label.text = f'校验未通过，发现 {len(errors)} 个问题：'
            result_label.classes(replace='text-sm font-bold mt-2 text-orange-700')
            with error_list_container:
                for message in errors:
                    ui.label(f'- {message}').classes('text-sm text-orange-700')
            logger.info('config_panel: 校验未通过 filename=%r errors=%d', filename, len(errors))
        else:
            result_label.text = '校验通过：所有必填字段均存在且非空。'
            result_label.classes(replace='text-sm font-bold mt-2 text-green-700')
            logger.info('config_panel: 校验通过 filename=%r', filename)

    check_button.on_click(handle_check)
