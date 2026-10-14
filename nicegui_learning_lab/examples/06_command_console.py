"""示例 6：命令控制台 —— 在浏览器里安全地触发服务器端命令。

为什么需要这个示例
------------------
很多工程 GUI 小工具最终都需要"点一个按钮，在服务器上跑一条诊断命令"这种
能力（查看磁盘空间、ping 一下某台设备、看看某个目录下有什么文件）。这个
能力一旦做得不好，就是一个远程命令执行漏洞；做得好，就必须把"界面上能点
什么"和"服务器上会跑什么"之间的关系收得非常紧——紧到即使页面被篡改、
即使有人直接拿开发者工具伪造请求，服务器端也只会执行
`services.command_service.COMMAND_WHITELIST` 里登记过的那几个命令，且
参数永远要先通过校验。本示例的全部代码都在演示"界面层该如何正确地使用
一个已经做好安全加固的 Service"，而不是自己去实现任何安全逻辑——所有安全
判断都发生在 `services/command_service.py`、`services/file_service.py`、
`services/path_guard.py` 里，本文件只负责把结果展示出来、把异常翻译成
对用户友好的提示文案。

心智模型
--------
页面分成两个独立的区域（用 `ui.tabs()`/`ui.tab_panels()` 隔开）：

- **命令执行区**：下拉框 -> （根据选择动态生成的）参数输入框 -> “执行”
  按钮。点击执行后，真正的子进程调用发生在线程池里
  （`await run.io_bound(command_service.run_command, ...)`），绝不会在
  NiceGUI 的事件循环线程里直接调用阻塞的 `subprocess.run()`——否则在命令
  还没跑完之前，整个页面（包括其他正在看这个页面的用户）都会被卡住。
- **文件查看区**：下拉框列出 `file_service.list_viewable_files()` 筛选出的
  文件名，选中后调用 `file_service.read_text_file()` 展示内容。

本文件不实现任何新的安全规则，它的职责边界非常窄：收集用户输入 ->
调用 Service -> 把 Service 返回的结果或抛出的具体异常类型，翻译成界面上
的文字。
"""

from __future__ import annotations

import sys
from pathlib import Path

# 集成阶段补丁：原因与做法见 examples/04_config_checker.py 顶部的同名注释
# 和 docs/multitask_plan.md 的"集成阶段修正记录"一节。
_PROJECT_ROOT = Path(__file__).resolve().parent.parent
if str(_PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(_PROJECT_ROOT))

from nicegui import run, ui

from services import command_service, file_service
from services.command_service import CommandSpec
from services.errors import (
    CommandExecutionError,
    CommandTimeoutError,
    NotFoundError,
    PermissionDeniedError,
    ValidationError,
)

#: 文件查看面板的基准目录：项目根目录本身。`Path(__file__).resolve()` 先
#: 得到本文件（`examples/06_command_console.py`）的绝对路径，`.parents[1]`
#: 再向上一级得到项目根目录——这与 `command_service.PROJECT_ROOT` 的算法
#: 完全一致，只是各自独立计算，不互相依赖对方的内部实现细节。
FILE_VIEW_BASE_DIR = Path(__file__).resolve().parents[1]

#: 文件查看面板允许的扩展名白名单。
FILE_VIEW_ALLOWED_SUFFIXES: tuple[str, ...] = (".md", ".txt", ".json", ".log", ".py")

#: 文件查看面板单次读取的最大字节数。
FILE_VIEW_MAX_BYTES = 200_000


def _available_commands() -> dict[str, CommandSpec]:
    """只返回在当前机器上真正可用（找到了可执行文件）的白名单命令。"""
    return {
        name: spec
        for name, spec in command_service.COMMAND_WHITELIST.items()
        if spec.executable is not None
    }


def _unavailable_commands() -> dict[str, CommandSpec]:
    """返回登记在白名单里、但在当前机器上找不到可执行文件的命令。"""
    return {
        name: spec
        for name, spec in command_service.COMMAND_WHITELIST.items()
        if spec.executable is None
    }


@ui.page("/command-console")
def index() -> None:
    """命令控制台页面：命令执行区 + 文件查看区。"""
    ui.label("示例 6：命令控制台").classes("text-2xl font-bold").mark("console-title")
    ui.label(
        "本页面演示如何在浏览器里安全地触发服务器端命令：所有可执行的命令都必须"
        "预先在 command_service.COMMAND_WHITELIST 里登记（固定可执行文件 + 严格"
        "校验的参数），不存在“输入任意命令”的入口。"
    ).classes("text-sm text-gray-600").mark("console-description")

    with ui.tabs().classes("w-full") as tabs:
        command_tab = ui.tab("命令执行").mark("tab-command")
        file_tab = ui.tab("文件查看").mark("tab-file")

    with ui.tab_panels(tabs, value=command_tab).classes("w-full"):
        with ui.tab_panel(command_tab):
            _build_command_panel()
        with ui.tab_panel(file_tab):
            _build_file_panel()


def _build_command_panel() -> None:
    """构建“命令执行区”：下拉框 + 动态参数输入 + 执行按钮 + 结果展示。"""
    available = _available_commands()
    unavailable = _unavailable_commands()

    if unavailable:
        hint_text = "、".join(
            f"{name}（{spec.description}）" for name, spec in unavailable.items()
        )
        ui.label(
            f"以下命令在当前机器上不可用（未找到对应的可执行文件），已从下拉框中"
            f"隐藏：{hint_text}"
        ).classes("text-orange-600 text-sm").mark("unavailable-commands-hint")

    if not available:
        ui.label("当前机器上没有任何可用的白名单命令。").classes(
            "text-red-600 font-bold"
        ).mark("no-available-commands")
        return

    command_names = list(available.keys())
    command_select = (
        ui.select(options=command_names, value=command_names[0], label="选择命令")
        .classes("w-72")
        .mark("command-select")
    )
    command_hint = ui.label().classes("text-xs text-gray-500").mark("command-description-hint")

    # `arg_widget_box` 用一个一元素字典作为"可写的闭包状态"：Python 的闭包
    # 不允许在内层函数里直接给外层的简单变量重新赋值（会被当成局部变量），
    # 用字典的一个键来持有"当前参数输入控件"的引用，可以绕开这个限制。
    arg_widget_box: dict[str, ui.element | None] = {"widget": None}
    arg_container = ui.column().classes("w-72")

    def rebuild_arg_input() -> None:
        """根据当前选中的命令，重新生成它需要的参数输入控件（如果需要的话）。"""
        spec = available[command_select.value]
        command_hint.text = spec.description
        arg_container.clear()
        with arg_container:
            if spec.arg_spec is None:
                ui.label("该命令不需要任何参数。").classes("text-xs text-gray-500")
                arg_widget_box["widget"] = None
            elif spec.arg_spec.kind == "choice":
                choices = list(spec.arg_spec.choices)
                widget = ui.select(
                    options=choices,
                    value=choices[0] if choices else None,
                    label=spec.arg_spec.label,
                ).mark("command-arg-input")
                arg_widget_box["widget"] = widget
            else:
                widget = ui.input(
                    label=spec.arg_spec.label,
                    placeholder=spec.arg_spec.error_hint,
                ).mark("command-arg-input")
                arg_widget_box["widget"] = widget

    command_select.on_value_change(lambda _event: rebuild_arg_input())
    rebuild_arg_input()

    with ui.row().classes("items-center gap-2"):
        run_button = ui.button("执行").mark("run-command-button")
        spinner = ui.spinner(size="lg")
        spinner.set_visibility(False)

    status_label = ui.label().classes("text-sm").mark("command-status-label")
    with ui.row().classes("gap-4"):
        returncode_label = ui.label().mark("command-returncode-label")
        duration_label = ui.label().mark("command-duration-label")
    truncated_hint = ui.label().classes("text-orange-600 text-sm").mark("command-truncated-hint")

    ui.label("stdout").classes("text-xs text-gray-500 mt-2")
    stdout_log = ui.log(max_lines=200).classes("w-full h-48").mark("command-stdout-log")
    ui.label("stderr").classes("text-xs text-gray-500 mt-2")
    stderr_log = ui.log(max_lines=200).classes("w-full h-48").mark("command-stderr-log")

    async def handle_run() -> None:
        """“执行”按钮的事件回调：收集参数、在线程池里跑命令、展示结果。"""
        spec = available[command_select.value]
        widget = arg_widget_box["widget"]
        if widget is None:
            user_args: list[str] = []
        else:
            raw_value = widget.value
            user_args = [] if raw_value is None else [str(raw_value)]

        run_button.disable()
        spinner.set_visibility(True)
        status_label.text = "正在执行…"
        returncode_label.text = ""
        duration_label.text = ""
        truncated_hint.text = ""
        stdout_log.clear()
        stderr_log.clear()

        try:
            # command_service.run_command() 内部直接调用标准库的阻塞
            # subprocess.run()，必须丢进线程池执行，否则会卡住 NiceGUI 的
            # 事件循环，导致整个页面（乃至同一进程里其他用户的页面）卡死。
            result = await run.io_bound(
                command_service.run_command, command_select.value, user_args
            )
        except PermissionDeniedError as exc:
            status_label.text = f"权限被拒绝：{exc}"
            ui.notify(f"权限被拒绝：{exc}", type="negative")
        except ValidationError as exc:
            status_label.text = f"参数不合法：{exc}"
            ui.notify(f"参数不合法：{exc}", type="negative")
        except CommandTimeoutError as exc:
            status_label.text = f"命令执行超时：{exc}"
            ui.notify(f"命令执行超时：{exc}", type="negative")
        except CommandExecutionError as exc:
            status_label.text = f"命令无法执行：{exc}"
            ui.notify(f"命令无法执行：{exc}", type="negative")
        else:
            status_label.text = "执行完成"
            process_result = result.process_result
            returncode_label.text = f"退出码：{process_result.returncode}"
            duration_label.text = f"耗时：{process_result.duration_seconds:.3f} 秒"
            for line in process_result.stdout.splitlines():
                stdout_log.push(line)
            for line in process_result.stderr.splitlines():
                stderr_log.push(line)
            hints = []
            if process_result.stdout_truncated:
                hints.append("stdout 输出已截断")
            if process_result.stderr_truncated:
                hints.append("stderr 输出已截断")
            truncated_hint.text = "；".join(hints)
        finally:
            run_button.enable()
            spinner.set_visibility(False)

    run_button.on_click(handle_run)


def _build_file_panel() -> None:
    """构建“文件查看区”：下拉框列出可查看文件，选中后展示（受限的）内容。"""
    files = file_service.list_viewable_files(FILE_VIEW_BASE_DIR, FILE_VIEW_ALLOWED_SUFFIXES)

    ui.label(
        f"只能查看项目根目录（{FILE_VIEW_BASE_DIR}）下、扩展名在 "
        f"{FILE_VIEW_ALLOWED_SUFFIXES} 内的文件，且单个文件最多读取 "
        f"{FILE_VIEW_MAX_BYTES} 字节。"
    ).classes("text-xs text-gray-500").mark("file-panel-hint")

    if not files:
        ui.label("当前没有可查看的文件。").classes("text-red-600").mark("no-viewable-files")
        return

    file_select = (
        ui.select(options=files, value=files[0], label="选择文件")
        .classes("w-72")
        .mark("file-select")
    )
    file_truncated_hint = ui.label().classes("text-orange-600 text-sm").mark("file-truncated-hint")
    file_content_view = ui.code("").classes("w-full").mark("file-content-view")

    async def handle_file_select_change() -> None:
        """文件下拉框变化时的回调：读取选中文件内容并展示。"""
        filename = file_select.value
        if not filename:
            return

        file_truncated_hint.text = ""
        try:
            text, truncated = await run.io_bound(
                file_service.read_text_file,
                filename,
                FILE_VIEW_BASE_DIR,
                max_bytes=FILE_VIEW_MAX_BYTES,
                allowed_suffixes=FILE_VIEW_ALLOWED_SUFFIXES,
            )
        except PermissionDeniedError as exc:
            file_content_view.content = ""
            ui.notify(f"权限被拒绝：{exc}", type="negative")
            return
        except NotFoundError as exc:
            file_content_view.content = ""
            ui.notify(f"文件不存在：{exc}", type="negative")
            return

        file_content_view.content = text
        if truncated:
            file_truncated_hint.text = f"内容已截断，仅显示前 {FILE_VIEW_MAX_BYTES} 字节。"

    file_select.on_value_change(lambda _event: handle_file_select_change())
    # 页面首次渲染后立刻加载一次默认选中的文件内容，不需要用户手动再点一次
    # 下拉框才能看到内容。`once=True` 让这个定时器只触发一次，相当于"页面
    # 加载完成后的一次性初始化回调"。
    ui.timer(0.01, handle_file_select_change, once=True)


if __name__ in {"__main__", "__mp_main__"}:
    ui.run(title="示例 6：命令控制台")
