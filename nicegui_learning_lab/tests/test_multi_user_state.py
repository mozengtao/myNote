"""示例 9（多用户状态隔离）的真实多用户集成测试。

为什么这个测试长这样
--------------------
这不是一个"假装通过"的测试：它使用 NiceGUI 官方提供的
`nicegui.testing.user_simulation()`（见
`.venv/lib64/python3.10/site-packages/nicegui/testing/user_simulation.py`），
真实地把 `examples/09_multi_user_state.py` 作为一个 NiceGUI 应用启动起来
（在模拟模式下，`ui.run()` 只注册路由、不真正监听端口，详见
`.venv/lib64/python3.10/site-packages/nicegui/ui_run.py` 第 213~215 行
`if helpers.is_user_simulation(): ... return`），然后：

- 用一个独立的 `httpx.AsyncClient`（自带独立的 cookie jar）模拟"用户 A 的
  浏览器"，对同一个 `core.app` 发起真实的 ASGI 请求（走完整的 FastAPI/
  Starlette 中间件栈，包括 `SessionMiddleware`），
- 用另一个独立的 `httpx.AsyncClient` 模拟"用户 B 的浏览器"，
- 用同一个 `httpx.AsyncClient`（复用用户 A 的 cookie jar）打开第二个
  `User` 对象，模拟"用户 A 在同一个浏览器里开的第二个标签页"，

然后分别点击页面上的 "+1" 按钮，检查 `app.storage.user` 与
`app.storage.tab`、`app.storage.general` 的真实隔离/共享行为是否符合
`docs/examples/09_multi_user_state.md` 第 2 节声称的语义。

为什么没有用 `nicegui.testing.plugin` 里的 `user` / `create_user` pytest
fixture
-------------------------------------------------------------------------
那两个 fixture 需要项目把 `nicegui.testing.plugin` 注册成 pytest 插件
（通常是在项目根目录的 `conftest.py` 里写
`pytest_plugins = ['nicegui.testing.plugin']`）。本次任务的交付范围明确
只允许创建/修改 `examples/09_multi_user_state.py`、
`docs/examples/09_multi_user_state.md`、`docs/security_review.md`、
`tests/test_multi_user_state.py` 这几个文件——不包含 `conftest.py`，所以
这里直接复刻 `create_user` fixture 内部的做法（见
`nicegui/testing/user_plugin.py` 的 `create_user`：
`User(httpx.AsyncClient(transport=httpx.ASGITransport(core.app),
base_url='http://test'))`），绕开对 pytest 插件自动发现机制的依赖，效果
完全等价。

为什么用 `asyncio.run()` 包一层，而不是写 `async def test_...`
----------------------------------------------------------------
本机 `.venv` 里只安装了 `pytest`（见 `requirements.txt`），没有安装
`pytest-asyncio`/`pytest-anyio`——裸的 pytest 无法直接收集并运行
`async def` 测试函数（会被当成"协程对象从未被 await"而直接跳过/报警告，
不会真正执行测试体）。为了不额外引入新的测试依赖（也不修改
`requirements.txt`，这是任务的硬性限制），这里统一用同步的
`def test_xxx():` 作为 pytest 看到的测试入口，内部用 `asyncio.run(...)`
驱动真正的异步测试逻辑。
"""

from __future__ import annotations

import asyncio
from pathlib import Path

import httpx

from nicegui import core
from nicegui.testing import User, user_simulation

EXAMPLE_FILE = Path(__file__).resolve().parents[1] / 'examples' / '09_multi_user_state.py'


async def _open_independent_browser(path: str) -> User:
    """在同一个 `core.app` 上，用一个全新、独立的 httpx.AsyncClient 打开页面。

    独立的 `httpx.AsyncClient` 意味着独立的 cookie jar，这正是模拟"换了
    一个完全不同的浏览器/用户"的关键——`app.storage.user` 的身份判定
    （`request.session['id']`，见 `nicegui/storage.py`）完全依赖会话
    cookie，没有这个 cookie 就会被 `SessionMiddleware` 签发一个全新的
    session id，对应一份全新的、空的用户存储。
    """
    client = httpx.AsyncClient(transport=httpx.ASGITransport(core.app), base_url='http://test')
    independent_user = User(client)
    await independent_user.open(path)
    return independent_user


def _click(user: User, marker: str) -> None:
    """点击 `user` 当前页面上带有指定 `.mark(marker)` 的按钮。"""
    user.find(marker=marker).click()


def _read_label_text(user: User, marker: str) -> str:
    """读取 `user` 当前页面上带有指定 `.mark(marker)` 的 `ui.label` 的文本。"""
    (label,) = user.find(marker=marker).elements
    return label.text


async def _async_test_tab_and_user_isolation() -> None:
    async with user_simulation(main_file=str(EXAMPLE_FILE)) as user_a_tab1:
        # ------------------------------------------------------------
        # 用户 A，标签页 1：打开页面，等待 WebSocket 连接建立
        #（app.storage.tab 卡片是在 `await ui.context.client.connected()`
        # 之后才会被渲染出来的，见 examples/09_multi_user_state.py 对应
        # 注释），再分别点击三次"用户计数器"、两次"标签页计数器"。
        # ------------------------------------------------------------
        await user_a_tab1.open('/multi-user-state')
        await user_a_tab1.should_see(marker='tab-counter-card')

        for _ in range(3):
            _click(user_a_tab1, 'user-counter-button')
        for _ in range(2):
            _click(user_a_tab1, 'tab-counter-button')

        assert _read_label_text(user_a_tab1, 'user-counter-value') == '3'
        assert _read_label_text(user_a_tab1, 'tab-counter-value') == '2'

        # ------------------------------------------------------------
        # 用户 A，标签页 2：复用**同一个** httpx.AsyncClient（也就是复用
        # 同一份会话 cookie），模拟同一个浏览器里打开的第二个标签页。
        # ------------------------------------------------------------
        user_a_tab2 = User(user_a_tab1.http_client)
        await user_a_tab2.open('/multi-user-state')
        await user_a_tab2.should_see(marker='tab-counter-card')

        # 用户计数器应当是"共享"的：标签页 2 一打开就应该已经看到标签页 1
        # 积累的值 3，不是从 0 开始。
        assert _read_label_text(user_a_tab2, 'user-counter-value') == '3'
        # 标签页计数器应当是"独立"的：标签页 2 是一个新的 tab_id，必须从
        # 0 开始，不能继承标签页 1 的值 2。
        assert _read_label_text(user_a_tab2, 'tab-counter-value') == '0'

        # 在标签页 2 里再点一次用户计数器、三次标签页计数器。
        _click(user_a_tab2, 'user-counter-button')
        for _ in range(3):
            _click(user_a_tab2, 'tab-counter-button')
        assert _read_label_text(user_a_tab2, 'user-counter-value') == '4'
        assert _read_label_text(user_a_tab2, 'tab-counter-value') == '3'

        # 回到标签页 1：刷新（重新 open 同一个 path，模拟浏览器刷新按钮）
        # 后，用户计数器应该看到标签页 2 刚才写入的最新值 4（共享），
        # 而标签页计数器应该还是刷新前的值 2（这是重新建立连接后分配到的
        # 新 tab_id，但本断言的重点是：它绝不会是 3——3 是标签页 2 的值，
        # 如果两个标签页共享了同一份 tab 存储，这里就会错误地看到 3）。
        user_a_tab1_refreshed = User(user_a_tab1.http_client)
        await user_a_tab1_refreshed.open('/multi-user-state')
        await user_a_tab1_refreshed.should_see(marker='tab-counter-card')
        assert _read_label_text(user_a_tab1_refreshed, 'user-counter-value') == '4'
        assert _read_label_text(user_a_tab1_refreshed, 'tab-counter-value') != '3'

        # ------------------------------------------------------------
        # 用户 B：一个完全独立的浏览器（独立 cookie jar），验证
        # app.storage.user 对用户 B 是全新的（从 0 开始），
        # 不会看到用户 A 积累的值 4。
        # ------------------------------------------------------------
        user_b = await _open_independent_browser('/multi-user-state')
        await user_b.should_see(marker='tab-counter-card')
        assert _read_label_text(user_b, 'user-counter-value') == '0'
        assert _read_label_text(user_b, 'tab-counter-value') == '0'

        _click(user_b, 'user-counter-button')
        assert _read_label_text(user_b, 'user-counter-value') == '1'
        # 用户 A 的值不应该被用户 B 的点击影响。
        assert _read_label_text(user_a_tab1_refreshed, 'user-counter-value') == '4'


def test_app_storage_user_isolates_but_app_storage_tab_is_finer_grained() -> None:
    """核心验收：app.storage.user 按"浏览器会话"隔离，app.storage.tab 按"标签页"隔离。

    完整断言列表见 `_async_test_tab_and_user_isolation()` 的实现与注释；
    这里只是提供一个同步的 pytest 入口（原因见模块文档"为什么用
    asyncio.run()"一节）。
    """
    asyncio.run(_async_test_tab_and_user_isolation())


async def _async_test_general_storage_is_shared_across_independent_browsers() -> None:
    async with user_simulation(main_file=str(EXAMPLE_FILE)) as user_a:
        await user_a.open('/multi-user-state')
        await user_a.should_see(marker='tab-counter-card')

        for _ in range(5):
            _click(user_a, 'general-counter-button')
        assert _read_label_text(user_a, 'general-counter-value') == '5'
        # 用户计数器此时应当仍是初始值 0——用来确认"全局共享计数器"与
        # "用户计数器"这两张卡片用的是两个完全独立的存储键/作用域，点
        # 一个不会影响另一个。
        assert _read_label_text(user_a, 'user-counter-value') == '0'

        # 换一个完全独立的浏览器（新的 httpx.AsyncClient，没有 user_a 的
        # cookie）打开同一个页面：app.storage.general 是进程级共享存储，
        # 应该立刻看到用户 A 刚才积累的值 5；而 app.storage.user 必须是
        # 全新的会话、从 0 开始。
        user_b = await _open_independent_browser('/multi-user-state')
        await user_b.should_see(marker='tab-counter-card')
        assert _read_label_text(user_b, 'general-counter-value') == '5'
        assert _read_label_text(user_b, 'user-counter-value') == '0'


def test_app_storage_general_is_shared_across_independent_browsers() -> None:
    """核心验收：app.storage.general 在完全不同的浏览器/会话之间也是共享的。"""
    asyncio.run(_async_test_general_storage_is_shared_across_independent_browsers())


async def _async_test_history_demo_is_isolated_per_user() -> None:
    async with user_simulation(main_file=str(EXAMPLE_FILE)) as user_a:
        await user_a.open('/multi-user-state')
        await user_a.should_see(marker='tab-counter-card')
        await user_a.should_see(marker='history-demo-empty')

        _click(user_a, 'history-demo-add-button')
        await user_a.should_see(marker='history-demo-entry')
        (entry_label,) = user_a.find(marker='history-demo-entry').elements
        assert 'SUCCEEDED' in entry_label.text

        # 换一个独立浏览器，应当看到空历史——这是本示例"把示例 8 的
        # _HISTORY 改造成按用户隔离"这段代码的核心验收点：历史记录现在
        # 存在 app.storage.user 里，天然按用户分开，不会像示例 8 的模块级
        # `_HISTORY` 那样让所有人共享同一份列表。
        user_b = await _open_independent_browser('/multi-user-state')
        await user_b.should_see(marker='tab-counter-card')
        await user_b.should_see(marker='history-demo-empty')
        await user_b.should_not_see(marker='history-demo-entry')


def test_history_demo_entries_do_not_leak_between_independent_users() -> None:
    """核心验收：把 TaskRecord 转成 dict 存入 app.storage.user 后，历史记录按用户隔离。"""
    asyncio.run(_async_test_history_demo_is_isolated_per_user())
