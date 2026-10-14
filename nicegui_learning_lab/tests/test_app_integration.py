"""`app.py`（集成仪表盘）与全部 9 个示例的无头渲染冒烟测试。

为什么不用 `curl` + 在 HTML 里 grep `.mark()`
----------------------------------------------
集成阶段最初尝试了计划里写的"用 curl 请求、在返回的 HTML 里确认
`.mark()` 元素文本"这个方案，结果发现：NiceGUI 3.18 的首次 HTTP 响应只是
一个空的 Vue/Quasar 应用外壳（`<div id="app"></div>` 之类），真正的页面
内容是建立 WebSocket 连接之后才由客户端渲染出来的——`curl` 拿到的原始
HTML 里**不会**出现任何 `.mark()` 文本，即使页面本身完全正常。这不是
猜测，是在集成阶段真实启动 `app.py`、用 curl 请求、人工检查返回内容后
发现的（见 `docs/multitask_plan.md`"集成阶段修正记录"一节）。

正确的做法（本文件采用）：用 NiceGUI 官方提供的 `nicegui.testing.User`
（与 `tests/test_multi_user_state.py` 同一套机制），它会用一个真实的
`httpx.AsyncClient` 走完整的 ASGI/WebSocket 协议栈，等待页面真正建立连接、
真正渲染出 Vue 组件树之后，再检查 `.mark()` 元素是否存在/可见——这才是
对"这个页面到底能不能正常打开"的真实验证，而不是只验证"HTTP 没有返回 500"。

本文件覆盖范围
--------------
按照 `docs/multitask_plan.md` 第 6 节验证计划的要求，对 `app.py` 以及全部
9 个 `examples/*.py` 都做一次真实的无头渲染验证（不是只做 `py_compile`）：

- `app.py` 主页面：四个标签页（配置检查/命令控制台/任务与测试/运行日志）
  是否都能正常渲染出各自的关键元素。
- `examples/04/05/06/08/09`：这几个示例在集成阶段被追加了 sys.path
  启动补丁（见 `docs/multitask_plan.md`"集成阶段修正记录"一节），这里用
  真实渲染验证补丁生效后页面确实能正常打开。
- `examples/01/02/03/07`：本身不依赖 `models`/`services`，不受 sys.path
  问题影响，但仍然在这里做一次真实渲染验证，而不是只满足于
  `py_compile`——"语法能通过"和"页面真的能渲染出关键元素"是两件不同的事。
"""

from __future__ import annotations

import asyncio
from pathlib import Path

from nicegui.testing import User, user_simulation

_PROJECT_ROOT = Path(__file__).resolve().parent.parent


async def _assert_page_renders(main_file: Path, path: str, expected_marks: list[str]) -> None:
    async with user_simulation(main_file=str(main_file)) as user:
        await user.open(path)
        for mark in expected_marks:
            await user.should_see(marker=mark)


def test_example_01_hello_nicegui_renders():
    asyncio.run(
        _assert_page_renders(
            _PROJECT_ROOT / 'examples' / '01_hello_nicegui.py',
            '/',
            ['hello-title', 'hello-description', 'hello-button'],
        )
    )


def test_example_02_layout_and_components_renders():
    asyncio.run(
        _assert_page_renders(
            _PROJECT_ROOT / 'examples' / '02_layout_and_components.py',
            '/',
            ['toolbox-title', 'card-recent-status', 'card-quick-actions', 'host-input'],
        )
    )


def test_example_03_events_and_state_renders():
    asyncio.run(
        _assert_page_renders(
            _PROJECT_ROOT / 'examples' / '03_events_and_state.py',
            '/',
            ['host-input', 'generate-button', 'preview-output'],
        )
    )


def test_example_07_background_task_renders():
    asyncio.run(
        _assert_page_renders(
            _PROJECT_ROOT / 'examples' / '07_background_task.py',
            '/background-task',
            ['page-title', 'start-button', 'cancel-button', 'progress-bar', 'task-log'],
        )
    )


def test_app_dashboard_renders_all_four_panels():
    """`app.py` 的主页面 `/` 应当能渲染出四个标签页和至少一个示例链接。"""
    asyncio.run(
        _assert_page_renders(
            _PROJECT_ROOT / 'app.py',
            '/',
            [
                'dashboard-title',
                'dashboard-tab-config',
                'dashboard-tab-command',
                'dashboard-tab-task',
                'dashboard-tab-log',
                'dashboard-example-link-01',
            ],
        )
    )


def test_example_04_config_checker_renders_after_syspath_fix():
    asyncio.run(
        _assert_page_renders(
            _PROJECT_ROOT / 'examples' / '04_config_checker.py',
            '/',
            ['result-container', 'upload-widget', 'server-file-select', 'server-file-submit'],
        )
    )


def test_example_05_task_table_renders_after_syspath_fix():
    asyncio.run(
        _assert_page_renders(
            _PROJECT_ROOT / 'examples' / '05_task_table.py',
            '/',
            ['task-table'],
        )
    )


def test_example_06_command_console_renders_after_syspath_fix():
    asyncio.run(
        _assert_page_renders(
            _PROJECT_ROOT / 'examples' / '06_command_console.py',
            '/command-console',
            ['console-title', 'tab-command', 'tab-file'],
        )
    )


def test_example_08_test_console_renders_after_syspath_fix():
    # 注意：`history-table` 在历史记录为空时会被 `set_visibility(False)`
    # 隐藏（改成展示 `history-empty-state`），这是本页面刻意设计的正确行为
    # （见 examples/08_test_console.py `_refresh_history_table()`），不是
    # 渲染失败——所以这里改为检查页面刚打开时真正可见的 `history-empty-state`。
    asyncio.run(
        _assert_page_renders(
            _PROJECT_ROOT / 'examples' / '08_test_console.py',
            '/test-console',
            ['page-title', 'start-button', 'history-empty-state'],
        )
    )


def test_example_09_multi_user_state_renders_after_syspath_fix():
    asyncio.run(
        _assert_page_renders(
            _PROJECT_ROOT / 'examples' / '09_multi_user_state.py',
            '/multi-user-state',
            ['global-counter-card', 'user-counter-card', 'general-counter-card'],
        )
    )
