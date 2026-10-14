"""示例 9：多用户 / 多标签页状态隔离 —— 用三种 app.storage 作用域对比。

为什么需要这个示例
------------------
示例 1~8 里出现过的所有"界面状态"，背后其实只有两种存放方式：组件自己持有
的 `.value`（刷新页面就丢，天然按浏览器连接隔离，参见示例 2/3 的讨论），
或者一个模块级的 Python 变量（示例 5 的只读模拟数据源、示例 8 的
`_HISTORY`）。后者有一个从未被挑破的问题：**模块级变量属于这个 Python
进程，不属于任何一个用户**。只要有第二个人（或者同一个人开了第二个浏览器
标签页）访问同一个页面，他们看到的、改动的，都是**同一份**数据——示例 8
的文档已经在"历史记录的简化处理"一节里明确承认了这一点，并把它作为留给
本示例的话题。

本示例要让读者在同一个页面上，用肉眼对比四种"状态存在哪里"的方案，而不是
只讲一遍定义就假设读者记住了：

1. **进程全局计数器**（模块级 `_GLOBAL_PROCESS_COUNTER`）——这是反面教材，
   故意摆在第一位，让读者先看到"最朴素但最危险"的做法长什么样。
2. **`app.storage.tab`**——按"这一个浏览器标签页"隔离。
3. **`app.storage.user`**——按"这一个浏览器/用户会话"隔离（同一个浏览器的
   多个标签页共享，不同浏览器/不同用户互不干扰）。
4. **`app.storage.general`**——所有用户共享，但（与方案 1 不同）是 NiceGUI
   官方提供、会持久化到磁盘的共享存储，不是"不小心用错了模块级变量"。

为什么选了 4 个而不是文档原计划的 3 个
----------------------------------------
最初的设想是只放 `tab` 与 `user` 两种 `app.storage` 作用域去对比"全局变量"
这一个反面教材。但实测（见本文件对应文档 `docs/examples/09_multi_user_state.md`
第 2 节"Mental Model"）发现 `app.storage.general` 和"模块级全局变量"
在表现上非常相似（都是所有人共享同一份），却有一个本质区别：
`app.storage.general` 是 NiceGUI 官方认可的"故意让所有用户共享"的存储
位置，具备持久化（进程重启不丢，见下文"验证来源"），而模块级变量纯粹是
Python 语言特性的副作用，进程一重启就清空。把这两者并排放在一起，才能让
读者看清"故意共享"和"不小心共享"之间的边界到底在哪——这正是本项目
`docs/security_review.md`（第 (B) 部分安全审查）里反复强调的"多用户状态
泄露"问题的核心。因此调整为 4 个并排计数器，并在文档里专门说明这条取舍。

验证来源（请读者注意：以下结论全部来自实际阅读本机已安装的 nicegui 包
源码得出，不是凭记忆写的）
-------------------------------------------------------------------------
以下文件路径是本项目在 `.venv` 虚拟环境里真实安装的 `nicegui==3.18.0`
的源码位置（相对路径基于本项目根目录）：

- `.venv/lib/python3.10/site-packages/nicegui/storage.py`
  - `Storage.tab`：返回 `self._tabs[client.tab_id]`，一个纯内存的
    `ObservableDict`，要求"当前客户端已经建立了 socket 连接"
    （`client.has_socket_connection`），否则直接抛 `RuntimeError`。
  - `Storage.user`：返回 `self._users[request.session['id']]`，是一个
    `PersistentDict`（默认落盘到 `.nicegui/storage-user-<session_id>.json`）；
    如果 `Storage.secret is None`（即 `ui.run()` 没有传
    `storage_secret`），会直接抛
    `RuntimeError('app.storage.user needs a storage_secret passed in
    ui.run()')`。
  - `Storage.general`：直接返回 `self._general`——同样是一个落盘的
    `PersistentDict`（`.nicegui/storage-general.json`），**不检查**
    `Storage.secret`，也不依赖当前请求/连接，这是本示例选它来对比
    "全局变量"的直接依据。
  - `Storage.browser`：返回 `request.session`（一个加密 cookie），同样
    要求 `Storage.secret` 非空，且"响应已经发给浏览器之后就不能再修改"
    （`request.state.responded` 为 `True` 时返回一个 `ReadOnlyDict`）。
    本示例没有用它做并排对比（见下文"为什么不展示 browser"），但结论
    同样来自这段源码。
  - `Storage.client`：返回 `context.client.storage`，一个**不落盘**、
    仅在当前 WebSocket 连接存活期间存在的 `ObservableDict`——页面
    刷新/重新导航就会丢失，比 `tab` 还要短命。
- `.venv/lib/python3.10/site-packages/nicegui/ui_run.py`：`storage_secret`
  参数的官方文档字符串原文是"a value is required to enable
  ui.storage.user and ui.storage.browser"——与上面读到的源码行为完全
  一致。
- `.venv/lib/python3.10/site-packages/nicegui/persistence/file_persistent_dict.py`：
  `app.storage.user`/`app.storage.general` 默认用的 `FilePersistentDict`
  在 `initialize()` 时用 `json.loads()` 从磁盘文件读入数据，写回时用
  `dumps()`（内部是 `orjson`）序列化——这是本文件第二节"把示例 8 的
  `_HISTORY` 改造成按用户隔离"这段代码的直接依据，详见该函数的文档
  字符串。
- `.venv/lib/python3.10/site-packages/nicegui/observables.py`
  `ObservableCollection._observe()`：只会把原生 `dict`/`list`/`set` 包装
  成可观测集合，**不认识**任意的 dataclass 实例——这解释了为什么直接把
  一个 `TaskRecord` 塞进 `app.storage.user` 很危险（见下文"从示例 8 的
  `_HISTORY` 到按用户隔离"一节的详细说明）。

为什么不展示 `app.storage.browser`
-----------------------------------
`app.storage.browser` 的数据是直接放进加密 cookie 里的——这意味着（a）它
的"修改之后立刻可见"这个特性在"响应已经发出去之后"就失效了（见上面读到
的 `request.state.responded` 分支），(b) 它的容量和语义都是为"少量、
在首次请求前就确定好"的数据设计的（例如主题偏好），用来做一个会在按钮
点击后持续递增的计数器体验上会很怪（点击的事件回调发生在响应已经发出之
后，这正是前面提到的"只读"分支会被触发的典型场景）。本示例没有把它做成
第 5 个并排计数器，但在 `docs/examples/09_multi_user_state.md` 第 2 节的
对比表格里仍然完整列出它的语义，确保"5 种 storage"这个列表不缺项。

本文件依赖关系
--------------
本文件 import 了 `models.task`（`TaskRecord`/`TaskStatus`/`new_task`），
用于第二部分"把示例 8 的 `_HISTORY` 改造成按用户隔离"的演示代码；除此之外
只依赖 `nicegui` 本身。本文件**不会修改** `examples/08_test_console.py`，
那是一个独立的、完整的历史示例，这里只是用同样的 `TaskRecord` 词汇单独做
一次"如果要改造，应该怎么改"的演示。
"""

from __future__ import annotations

import sys
from pathlib import Path
from typing import Callable, MutableMapping

# 集成阶段补丁：原因与做法见 examples/04_config_checker.py 顶部的同名注释
# 和 docs/multitask_plan.md 的"集成阶段修正记录"一节（本示例自己的文档
# 在 Execution 一节里也已经提前记录了这个现象，这里把修正落到代码里）。
_PROJECT_ROOT = Path(__file__).resolve().parent.parent
if str(_PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(_PROJECT_ROOT))

from nicegui import app, ui

from models.task import TaskRecord, TaskStatus, new_task

#: ⚠️ 仅供本地教学演示使用的固定密钥，绝不是生产级密钥管理方式。
#:
#: `app.storage.user` / `app.storage.browser` 依赖 Starlette 的
#: `SessionMiddleware` 给每个浏览器会话签发一个加密 cookie，
#: `storage_secret` 就是用来加密/校验这个 cookie 的密钥——泄露这个密钥，
#: 任何人都可以伪造或篡改别人的会话 cookie。生产环境**必须**：
#:   1. 从环境变量（例如 `os.environ["NICEGUI_STORAGE_SECRET"]`）或专门的
#:      密钥管理系统（Vault、AWS/GCP/Azure 的 Secrets Manager 等）读取，
#:      绝不能像这里一样把密钥硬编码进源代码、提交进版本库；
#:   2. 每个部署环境（dev/staging/prod）使用互不相同的随机值，并具备
#:      定期轮换的机制。
#: 本示例作为教学演示，为了"克隆仓库后立刻能跑起来、不需要任何额外配置"，
#: 才选择了这种明显不安全、一眼就能看出"这是演示用的"固定字符串。
STORAGE_SECRET_FOR_LOCAL_TEACHING_DEMO_ONLY = (
    'nicegui-learning-lab-example-09-teaching-only-DO-NOT-USE-IN-PRODUCTION'
)

# ---------------------------------------------------------------------------
# 反面教材：进程全局计数器。
#
# 故意用一个模块级可变变量表示"计数器当前的值"——这正是示例 8 文档里反复
# 警告的那种设计：这个值属于**这个 Python 进程**，不属于任何一个用户或
# 浏览器标签页。打开本页面的所有人看到的、改动的，都是这同一个数字。
# ---------------------------------------------------------------------------
_GLOBAL_PROCESS_COUNTER = 0


def _increment_global_counter() -> int:
    """递增模块级全局计数器并返回新值。

    这是本示例唯一一处使用 `global` 语句的地方——故意让它显眼，提醒读者
    "全局计数器"在源码层面长什么样，方便和下面三种 `app.storage` 方案做
    直接对比。
    """
    global _GLOBAL_PROCESS_COUNTER
    _GLOBAL_PROCESS_COUNTER += 1
    return _GLOBAL_PROCESS_COUNTER


# ---------------------------------------------------------------------------
# 把示例 8 的 `_HISTORY` 改造成"按用户隔离"的小演示
# ---------------------------------------------------------------------------
#
# 示例 8 的 `_HISTORY: list[TaskRecord]` 是模块级共享的——这里演示如何把
# 同样的"一次运行产生一个 TaskRecord，需要被记住"的需求，改造成每个用户
# 自己的 `app.storage.user` 条目。
#
# 关键问题："TaskRecord 能不能被直接塞进 app.storage.user？"
# ---------------------------------------------------------------------
# 答案：**不能依赖这样做**。原因已经通过实际读源码 + 实际跑一次读写验证过
# （见本文件模块文档"验证来源"一节、以及交付报告里贴出的真实脚本输出）：
#
# 1. `nicegui/observables.py` 的 `ObservableCollection._observe()` 只认识
#    原生 `dict`/`list`/`set`，不认识任意的 dataclass 实例。把一个
#    `TaskRecord` 直接赋值进 `app.storage.user['x'] = record` 时，
#    `__setitem__` 本身会触发一次变化通知、把当前值序列化落盘（这一步因为
#    `nicegui` 底层用的是 `orjson`，恰好能够处理 dataclass/Enum/datetime，
#    所以**第一次**写入不会报错）。但是：
# 2. 之后如果你调用 `record.append_log(...)` 或
#    `record.transition_to(...)` 这类**直接修改 dataclass 内部属性**的
#    操作，`ObservableDict` 完全不知道这件事发生过（因为它从没把这个
#    dataclass 包装成可观测对象），所以这些修改**不会**被自动持久化。
# 3. 进程重启后重新从磁盘加载（或者在 Redis 模式下从别的 worker 进程读取）
#    时，`json.loads()` 只能还原出普通的 `dict`，不可能还原出
#    `TaskRecord` 实例——`loaded.status` 会直接抛
#    `AttributeError: 'dict'/'ObservableDict' object has no attribute
#    'status'`。
#
# 正确做法：**显式转换成一个只包含 JSON 原生类型的 dict**，自己负责
# "dataclass -> dict"（写入前）和"dict -> 展示用的摘要"（读出后）这两次
# 转换，不依赖 nicegui 替你做任何隐式魔法。


def _task_record_to_storage_dict(record: TaskRecord) -> dict:
    """把一个 `TaskRecord` 转换成可以安全存入 `app.storage.user` 的 dict。

    只挑选历史列表展示需要的字段，并把两类"非 JSON 原生类型"都显式转换：

    - `TaskStatus`（`Enum`）-> `.value`（`str`）
    - `datetime` -> `.isoformat()`（`str`）

    这是本示例对"示例 8 的 `_HISTORY` 问题"给出的正式解法：不是去修改
    `examples/08_test_console.py`，而是在这里演示"如果要改造，应该怎么
    转换"。
    """
    return {
        'id': record.id,
        'name': record.name,
        'status': record.status.value,
        'created_at': record.created_at.isoformat(),
        'finished_at': record.finished_at.isoformat() if record.finished_at else None,
        'result': record.result,
        'error': record.error,
        'is_simulated': record.is_simulated,
    }


def _storage_dict_to_summary_text(data: dict) -> str:
    """把存储里读出来的 dict 还原成一行人类可读的摘要文本。

    注意：这里**没有**把 dict 还原成一个真正的 `TaskRecord` 对象——本示例
    的历史列表只需要展示摘要，不需要完整的 `TaskRecord`（例如不需要
    `logs`/`transition_to()`）。如果真的需要完整还原（例如要在界面上继续
    调用 `record.append_log(...)`），必须自己写一个
    `TaskRecord(id=data['id'], ..., status=TaskStatus(data['status']),
    created_at=datetime.fromisoformat(data['created_at']), ...)` 这样的
    构造函数——`TaskStatus(字符串)` 和 `datetime.fromisoformat(字符串)`
    都是标准库/项目自带的、能把字符串还原成正确类型的手段。
    """
    finished = f'，结束于 {data["finished_at"]}' if data.get('finished_at') else ''
    return f'[{data["status"]}] {data["name"]}{finished}'


def _simulate_one_history_entry() -> TaskRecord:
    """同步地"模拟跑完一次任务"，返回一个终态的 `TaskRecord`。

    与示例 8 的 `test_service.run_test_suite()` 不同，这里**不**使用
    `asyncio.sleep()`/后台协程——本示例的重点是"存储去哪了"，不是"后台任务
    机制"（那是示例 7/8 已经讲过的话题），所以用最简单的同步代码构造一个
    已经跑完的 `TaskRecord`，瞬间得到结果。
    """
    record = new_task(name='[模拟] 示例 9 历史记录演示任务', is_simulated=True)
    record.transition_to(TaskStatus.RUNNING)
    record.result = '演示任务已完成（纯内存模拟，不代表任何真实设备操作）'
    record.transition_to(TaskStatus.SUCCEEDED)
    return record


@ui.page('/multi-user-state')
async def multi_user_state_page() -> None:
    """示例 9 主页面：4 个并排计数器，对比不同的状态存储作用域。

    这个页面函数必须是 `async def`，原因见下方 `app.storage.tab` 卡片
    构建处的注释——这是本示例在实际启动服务并访问页面时真实踩到的一个
    `RuntimeError`，详见 `docs/examples/09_multi_user_state.md` 的
    Failure Modes 第 3 条。
    """
    ui.label('示例 9：多用户 / 多标签页状态隔离').classes('text-2xl font-bold').mark('page-title')
    ui.label(
        '本页面演示同一份"点击 +1"逻辑，背后分别存放在 4 种不同的作用域里会有'
        '什么不同的表现。下面「怎么做真实对比实验」一节会指导你怎么亲眼验证。'
    ).classes('text-sm text-gray-600').mark('page-description')

    with ui.card().classes('w-full bg-blue-50 mt-2').mark('how-to-experiment-banner'):
        ui.label('怎么做真实对比实验（请亲自试一试，不要只读文字）').classes(
            'font-bold text-blue-800'
        )
        ui.markdown(
            '- **验证"进程全局计数器"的危险之处**：打开本页面，点击它的 "+1" 几次，'
            '记下数字；再开一个新的浏览器标签页访问同一个地址——你会发现新标签页'
            '里显示的数字**不是 0**，而是接着刚才的数字继续；在新标签页里点 "+1"，'
            '回到第一个标签页刷新页面，你会看到第一个标签页里的数字也被改变了。\n'
            '- **验证 `app.storage.tab`**：在同一个浏览器里打开两个标签页访问本页面，'
            '分别点击"标签页计数器"的 "+1"——两个标签页显示的数字应该各自独立增长，'
            '互不影响；在任意一个标签页按浏览器的刷新按钮，这个标签页的数字应该'
            '保持不变（不会被重置成 0）。\n'
            '- **验证 `app.storage.user`**：在同一个浏览器里打开两个标签页，点击任意'
            '一个标签页里"用户计数器"的 "+1"，然后切换到另一个标签页刷新页面——'
            '你会看到数字是**共享**的（两个标签页看到同一个值）；接下来换一个'
            '**完全不同的浏览器**（或者当前浏览器的隐私/隐身窗口，这会产生一个全新'
            '的会话 cookie）打开同一个地址，你会看到用户计数器从 0 开始，和刚才那个'
            '浏览器里的值完全无关。\n'
            '- **验证 `app.storage.general`**：和"用户计数器"的实验步骤一样换一个全新'
            '浏览器/隐身窗口，你会看到"全局共享存储计数器"（`app.storage.general`）'
            '显示的数字和原来的浏览器**完全一样**——这和"用户计数器"的表现正好相反，'
            '也和"进程全局计数器"表现相似，但它是官方认可、会持久化到'
            '`.nicegui/storage-general.json` 的存储，而不是一个容易被误用的模块级'
            'Python 变量。'
        ).classes('text-sm')

    with ui.row().classes('w-full gap-4 mt-4 items-stretch'):
        _build_counter_card(
            card_mark='global-counter-card',
            title='① 进程全局计数器（反面教材）',
            scope_note='所有用户、所有标签页共享同一个 Python 进程变量；进程重启即丢失。',
            value_mark='global-counter-value',
            button_mark='global-counter-button',
            get_value=lambda: _GLOBAL_PROCESS_COUNTER,
            on_increment=_increment_global_counter,
            card_classes='bg-red-50',
        )
        # app.storage.tab 的卡片不能在这里直接构建：`Storage.tab`（见
        # nicegui/storage.py）要求"当前客户端已经建立了 socket 连接"
        # （client.has_socket_connection），而本页面函数此刻执行的是
        # "浏览器发出第一次 HTTP 请求、还没有建立 WebSocket 连接"这个阶段
        # ——这不是猜测，是本示例在真实启动服务并用浏览器/curl 访问
        # `/multi-user-state` 时，亲眼看到服务端抛出
        # `RuntimeError: app.storage.tab can only be used with a client
        # connection` 之后，才改成下面这种"先占位、等连接建立后再填充"的
        # 写法（完整报错堆栈见本示例交付报告"验证命令及真实输出"一节）。
        tab_card_slot = ui.column().classes('contents')
        _build_counter_card(
            card_mark='user-counter-card',
            title='③ app.storage.user',
            scope_note='同一浏览器的所有标签页共享；不同浏览器/不同用户互不干扰。',
            value_mark='user-counter-value',
            button_mark='user-counter-button',
            get_value=lambda: app.storage.user.get('counter', 0),
            on_increment=lambda: _increment_storage_counter(app.storage.user),
            card_classes='bg-green-50',
        )
        _build_counter_card(
            card_mark='general-counter-card',
            title='④ app.storage.general',
            scope_note=(
                '所有用户、所有浏览器共享；与①不同之处在于它会持久化到磁盘，'
                '是官方认可的"故意共享"存储，不是误用。'
            ),
            value_mark='general-counter-value',
            button_mark='general-counter-button',
            get_value=lambda: app.storage.general.get('counter', 0),
            on_increment=lambda: _increment_storage_counter(app.storage.general),
            card_classes='bg-purple-50',
        )

    # 必须等 WebSocket 连接真正建立之后，client.tab_id 才会存在，
    # app.storage.tab 才可以被访问——`ui.context.client.connected()` 正是
    # NiceGUI 官方文档（见本文件模块文档引用的
    # https://nicegui.io/documentation/page#wait_for_client_connection）
    # 推荐的等待方式。在这一行 `await` 恢复之前，浏览器已经先收到了上面
    # 三个卡片的初始 HTML；这行之后，第二个卡片的位置会被异步"补"上。
    await ui.context.client.connected()
    with tab_card_slot:
        _build_counter_card(
            card_mark='tab-counter-card',
            title='② app.storage.tab',
            scope_note='只属于这一个浏览器标签页；刷新同一标签页不丢，开新标签页则从 0 开始。',
            value_mark='tab-counter-value',
            button_mark='tab-counter-button',
            get_value=lambda: app.storage.tab.get('counter', 0),
            on_increment=lambda: _increment_storage_counter(app.storage.tab),
            card_classes='bg-yellow-50',
        )

    _build_history_demo_section()


def _increment_storage_counter(storage: MutableMapping) -> int:
    """`app.storage.tab` / `.user` / `.general` 通用的"+1"逻辑。

    三者在 NiceGUI 里的运行时类型都兼容 `MutableMapping`/`dict` 的接口
    （`tab`/`general` 是 `ObservableDict`，`user` 通常是 `FilePersistentDict`
    这个 `PersistentDict` 子类），所以可以共用同一个函数，不需要分别写三份
    几乎一样的逻辑。
    """
    storage['counter'] = storage.get('counter', 0) + 1
    return storage['counter']


def _build_counter_card(
    *,
    card_mark: str,
    title: str,
    scope_note: str,
    value_mark: str,
    button_mark: str,
    get_value: Callable[[], int],
    on_increment: Callable[[], int],
    card_classes: str,
) -> None:
    """构建一个"标题 + 作用域说明 + 当前值 + +1 按钮"的计数器卡片。

    四个计数器卡片的结构完全一样，唯一的区别是"值存在哪、怎么+1"，所以把
    卡片的 UI 结构抽成一个函数，用参数区分行为，避免四段几乎相同的代码。
    """
    with ui.card().classes(f'w-64 {card_classes}').mark(card_mark):
        ui.label(title).classes('font-bold')
        ui.label(scope_note).classes('text-xs text-gray-600 mt-1')
        value_label = ui.label(str(get_value())).classes('text-3xl font-mono mt-2').mark(value_mark)

        def handle_click() -> None:
            on_increment()
            value_label.text = str(get_value())

        ui.button('+1', on_click=handle_click).classes('mt-2').mark(button_mark)


def _build_history_demo_section() -> None:
    """构建"把示例 8 的 _HISTORY 改造成按用户隔离"的演示区块。"""
    ui.separator().classes('mt-6')
    ui.label('附加演示：把示例 8 的 _HISTORY 改造成按用户隔离').classes(
        'text-lg font-bold mt-4'
    ).mark('history-demo-title')
    ui.label(
        '点击下方按钮会"模拟跑完一次任务"（纯内存模拟，不是真实后台任务——那是'
        '示例 7/8 的话题），并把结果转换成 dict 后存进 app.storage.user["history"]。'
        '刷新本页面、或者在同一浏览器开新标签页，历史记录都应该还在；换一个浏览器/'
        '隐身窗口打开，应该看到一个空列表。'
    ).classes('text-sm text-gray-600').mark('history-demo-description')

    history_list_container = ui.column().classes('gap-1 mt-2').mark('history-demo-list')

    def _refresh_history_list() -> None:
        history_list_container.clear()
        history: list[dict] = app.storage.user.get('history', [])
        with history_list_container:
            if not history:
                ui.label('（暂无历史记录）').classes('text-gray-500 italic').mark(
                    'history-demo-empty'
                )
            for entry in history:
                ui.label(_storage_dict_to_summary_text(entry)).classes(
                    'text-sm font-mono'
                ).mark('history-demo-entry')

    def handle_add_history_entry() -> None:
        record = _simulate_one_history_entry()
        entry_dict = _task_record_to_storage_dict(record)

        # 关键的一步：读出当前列表、追加、整体赋值回去——而不是对一个已经
        # 取出来的列表变量做 .append() 之后假装它会自动生效。
        # app.storage.user 的 __setitem__（见 nicegui/observables.py 的
        # ObservableDict）只有在真正发生一次"赋值"动作时才会触发变化通知
        # 并安排持久化；如果只是先 `history = app.storage.user['history']`
        # 再 `history.append(entry_dict)`，根据你拿到的是不是同一个
        # ObservableList 实例，有可能"看起来"也生效（因为 ObservableList
        # 自己的 append 也会触发变化通知），但显式地整体重新赋值是最不依赖
        # 内部实现细节、最不容易出错的写法。
        history: list[dict] = list(app.storage.user.get('history', []))
        history.append(entry_dict)
        app.storage.user['history'] = history

        _refresh_history_list()
        ui.notify(f'已新增一条历史记录：{entry_dict["id"][:8]}', type='positive')

    ui.button('模拟新增一条历史记录', on_click=handle_add_history_entry).mark(
        'history-demo-add-button'
    )
    _refresh_history_list()


if __name__ in {'__main__', '__mp_main__'}:
    ui.run(
        title='示例 9：多用户 / 多标签页状态隔离',
        storage_secret=STORAGE_SECRET_FOR_LOCAL_TEACHING_DEMO_ONLY,
    )
