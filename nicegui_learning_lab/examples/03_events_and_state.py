"""示例 3：事件、输入与状态 —— 命令参数生成器。

为什么需要这个示例
------------------
前两个示例的事件回调都很简单（打个招呼、更新一个标签）。真实的工程 GUI
小工具通常需要：先校验用户输入、校验失败时给出具体原因并中止、校验通过
后才执行"业务操作"（这里是拼出一段命令预览文本）、最后把结果同步回界面。
本示例用一个"命令参数生成器"演示这整条链路，并且刻意把"校验逻辑"写成一个
不依赖 nicegui 的纯函数，用来说明"校验/业务规则可以、也应该独立于 UI 框架
存在"——这样它才能被单独测试，也才能在未来被挪到 services/ 层而不用改动
签名。

安全说明
--------
本示例只做字符串拼接和展示，**绝不会**把拼出来的命令真的交给 shell 去
执行。这是"绝不执行未经校验的用户输入"这条项目原则在最简单形式下的体现：
即使输入已经通过了格式校验，它依然只是"看起来安全的字符串"，而不是"可以
安全执行的命令"——这两者不能划等号，真正需要执行命令的示例会在后面引入
专门的白名单机制（见 services/ 层未来的 command_service）。

心智模型
--------
一次"点击生成预览"大致会经过这几步：

    用户点击按钮
        -> 事件回调读取 host_input.value / env_select.value / extra_input.value
        -> 调用纯函数 validate_hostname() 做格式校验
        -> 校验失败：ui.notify(type='negative') 展示原因，不生成预览
        -> 校验通过：调用纯函数 build_preview_command() 拼出预览文本
        -> 把预览文本写入 preview_label，并启用"复制"按钮背后的数据

这几个 `ui.input` / `ui.select` 对象自己持有的 `.value`，只是"这个页面
当前展示给这一个浏览器标签页的状态"，页面一刷新就会重置——这叫"页面级
状态"。如果将来要让多个页面共享"当前选中的环境是什么"这类信息，就不能
再依赖某个组件的 `.value`，而要放到一个更上层的"应用状态"里（NiceGUI
提供了 `app.storage` 系列工具来做这件事）。这个话题这里先点到为止，
示例 9 会专门展开。

本文件不依赖 models/、services/ 中的任何模块；`validate_hostname()` 和
`build_preview_command()` 这两个函数也完全不依赖 nicegui，可以被单独
import 后用普通的 Python/pytest 测试。
"""

from __future__ import annotations

import re

from nicegui import ui

#: 目标环境下拉框的可选项。
ENVIRONMENTS = ['dev', 'staging', 'prod']

#: 合法主机名只允许的字符集合：字母、数字、点号、连字符。
_HOSTNAME_PATTERN = re.compile(r'[A-Za-z0-9.\-]+')


def validate_hostname(hostname: str) -> str | None:
    """校验主机名是否合法。

    这是一个不依赖 nicegui 的纯函数：输入输出都只是普通的 Python `str` /
    `None`，不触碰任何 UI 组件，因此既可以在本文件的事件回调里调用，也可以
    被单独 import 后直接做单元测试，完全不需要启动 NiceGUI 应用。

    Args:
        hostname: 用户在 `ui.input` 里填写的主机名，调用方应先 `.strip()`。

    Returns:
        校验通过时返回 `None`；校验失败时返回一句人类可读的中文错误原因。
    """
    if not hostname:
        return '主机名不能为空'
    if not _HOSTNAME_PATTERN.fullmatch(hostname):
        return '主机名只能包含字母、数字、点号（.）和连字符（-）'
    return None


def build_preview_command(host: str, env: str, extra_args: str) -> str:
    """拼出一段仅用于展示的命令预览文本，调用方必须保证 `host` 已通过校验。

    同样是一个不依赖 nicegui 的纯函数，方便与 `validate_hostname()` 一起
    单独测试"输入 -> 预览文本"这条业务规则，而不用管它最终是怎么显示在
    页面上的。

    Args:
        host: 已通过 `validate_hostname()` 校验的主机名。
        env: 目标环境，取值应来自 `ENVIRONMENTS`。
        extra_args: 用户额外填写的参数，允许为空字符串。

    Returns:
        形如 `ssh {host} -- deploy --env {env} {extra_args}` 的预览文本。
    """
    extra = extra_args.strip()
    preview = f'ssh {host} -- deploy --env {env}'
    if extra:
        preview = f'{preview} {extra}'
    return preview


@ui.page('/')
def index() -> None:
    """构建"命令参数生成器"页面上的全部 UI 元素。"""
    with ui.column().classes('w-full gap-4 p-4'):
        ui.label('命令参数生成器').classes('text-2xl font-bold').mark(
            'generator-title'
        )
        ui.label(
            '填写主机名、目标环境和额外参数后点击"生成预览"。本页面只拼接'
            '并展示字符串，不会真的执行任何命令。'
        ).mark('generator-description')

        host_input = ui.input(label='主机名').mark('host-input')
        env_select = ui.select(
            ENVIRONMENTS, value=ENVIRONMENTS[0], label='目标环境'
        ).mark('env-select')
        extra_textarea = ui.textarea(label='额外参数（可选）').mark('extra-textarea')

        preview_label = ui.label('').mark('preview-output')

        def handle_generate() -> None:
            """读取当前输入值，先做校验，校验通过后才生成预览文本。"""
            host = host_input.value.strip()
            env = env_select.value
            extra_args = extra_textarea.value or ''

            error = validate_hostname(host)
            if error is not None:
                preview_label.text = ''
                ui.notify(error, type='negative')
                return

            preview_text = build_preview_command(host, env, extra_args)
            preview_label.text = preview_text
            ui.notify('预览已生成', type='positive')

        def handle_copy() -> None:
            """把当前预览文本写入剪贴板；如果还没生成预览则提示用户。"""
            text = preview_label.text
            if not text:
                ui.notify('请先生成预览，再复制', type='warning')
                return
            ui.clipboard.write(text)
            ui.notify('已复制到剪贴板', type='positive')

        with ui.row().classes('gap-2'):
            ui.button('生成预览', on_click=handle_generate).mark('generate-button')
            ui.button('复制', on_click=handle_copy).mark('copy-button')


if __name__ in {'__main__', '__mp_main__'}:
    ui.run(title='示例 3：事件、输入与状态')
