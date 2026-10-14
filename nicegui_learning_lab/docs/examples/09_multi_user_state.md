# 示例 9：多用户 / 多标签页状态隔离

> 对应代码：`examples/09_multi_user_state.py`
> 对应测试：`tests/test_multi_user_state.py`
> 对应审查：`docs/security_review.md`（第 (B) 部分跨示例安全审查，和本示例
> 讨论的是同一个主题的两个角度）

## 1. Why

示例 5 的 `_DATA_SOURCE`、示例 8 的 `_HISTORY`，背后都是同一种设计：
一个**模块级的 Python 变量**。模块级变量属于**这个 Python 进程**，不属于
任何一个打开页面的人——这件事在示例 8 的文档里已经被明确承认，并被留作
"示例 9 要展开的话题"。

真实的工程 GUI 几乎永远是多用户的（哪怕只是"同一个人开了两个浏览器标签
页"也算），所以"一个值到底存在哪里、谁能看到它、谁能改动它"是每一个需要
记住状态的功能都绕不开的问题。NiceGUI 提供了一整套 `app.storage.*`
API 来回答这个问题，但如果只是"听过这几个名字"而没有亲眼看过它们在同一
个页面上的真实表现差异，很容易在用的时候搞混（这正是本文档"Failure
Modes"小节要专门列出来的几种混淆）。

本示例的目标：在同一个页面上并排放置 4 个计数器，背后分别对应"模块级
全局变量"（反面教材）、`app.storage.tab`、`app.storage.user`、
`app.storage.general`，让读者可以通过**真的去点按钮、真的去开两个标签页/
两个浏览器**，亲眼验证四种方案的隔离边界到底在哪——而不是靠记忆一份
对照表。同时，本示例还会正式回答示例 8 留下的问题："如果要把 `_HISTORY`
改造成按用户隔离，应该怎么做"。

## 2. Mental Model

### 2.1 五种 `app.storage.*` 的真实语义对照表

**重要声明：下表的每一行结论都来自实际阅读本机 `.venv` 虚拟环境里真实
安装的 `nicegui==3.18.0` 源码得出，不是凭记忆写的。** 所有引用的文件
路径都基于本项目根目录，读者可以自己打开这些文件核对：

| storage | 真实类名（源码位置） | 存储媒介 | 生命周期 / 作用域 | 是否需要 `storage_secret` |
| --- | --- | --- | --- | --- |
| `app.storage.client` | `ObservableDict`（`nicegui/observables.py`），通过 `context.client.storage` 取得 | 纯内存，**不落盘** | 只在**当前这一次 WebSocket 连接**存活期间存在；刷新页面、重新导航、断线重连都会丢失——是 5 种里最短命的 | **不需要**（`Storage.client` 属性直接返回 `context.client.storage`，不检查 `Storage.secret`） |
| `app.storage.tab` | `ObservableDict`（本地模式）或 `RedisPersistentDict`（配置了 `NICEGUI_REDIS_URL` 时），`nicegui/storage.py` 的 `Storage.tab` | 本地模式下纯内存；Redis 模式下落在 Redis，默认 TTL = `max_tab_storage_age`（30 天） | 属于**这一个浏览器标签页**（由 `client.tab_id` 标识）；同一个标签页刷新/短暂断线重连不丢（`_create_tab_storage()` 里有"继承旧 `tab_id` 数据"的逻辑），但**必须先等 WebSocket 连接建立**（`client.has_socket_connection`）才能访问，否则抛 `RuntimeError`（本示例实际触发过这个异常，见 Failure Modes 第 3 条） | **不需要**（`Storage.tab` 属性不检查 `Storage.secret`，但依赖 `client.tab_id`，这个 id 来自 WebSocket 握手，不是 cookie） |
| `app.storage.user` | `FilePersistentDict`（本地模式）或 `RedisPersistentDict`，`nicegui/storage.py` 的 `Storage.user` | 本地模式默认落盘到 `.nicegui/storage-user-<session_id>.json` | 属于**这一个浏览器会话**（由 `request.session['id']` 标识，这是一个加密 cookie 里的随机 id）；同一浏览器的所有标签页共享，关闭浏览器重开（cookie 还在）也共享，换浏览器/隐身窗口（没有这个 cookie）则是全新的一份 | **需要**：没有传 `storage_secret` 时，`Storage.user` 属性会直接抛 `RuntimeError('app.storage.user needs a storage_secret passed in ui.run()')` |
| `app.storage.browser` | `request.session` 本身（一个经过签名加密的 cookie），`nicegui/storage.py` 的 `Storage.browser` | 直接存在浏览器的 cookie 里，**不在服务器上落盘** | 同一浏览器的所有标签页共享；**只能在"服务器响应还没发给浏览器"之前修改**——`request.state.responded` 为 `True` 之后，这个属性会返回一个只读的 `ReadOnlyDict`（继续赋值不会报错，但不会真的生效，也不会反映到浏览器） | **需要**：原理与 `app.storage.user` 相同（都依赖 `SessionMiddleware`），没有 `storage_secret` 同样会抛 `RuntimeError` |
| `app.storage.general` | `FilePersistentDict`（本地模式）或 `RedisPersistentDict`，`nicegui/storage.py` 的 `Storage.general` | 本地模式默认落盘到 `.nicegui/storage-general.json` | **所有用户、所有浏览器共享**，进程重启不丢（落盘） | **不需要**（`Storage.general` 属性直接返回 `self._general`，不检查 `Storage.secret`，也不依赖当前请求/连接） |

真实引用来源（文件路径相对本项目根目录）：

```startLine:endLine:nicegui_learning_lab/.venv/lib/python3.10/site-packages/nicegui/storage.py
113:    @property
114:    def browser(self) -> ReadOnlyDict | dict:
...
135:    @property
136:    def user(self) -> PersistentDict:
...
157:    @property
158:    def general(self) -> PersistentDict:
...
171:    @property
172:    def tab(self) -> observables.ObservableDict:
173:        """A volatile storage that is only kept during the current tab session."""
174:        client = context.client
175:        if not client.has_socket_connection:
176:            raise RuntimeError('app.storage.tab can only be used with a client connection; '
```

（具体行号以读者自己 `.venv` 里安装的版本为准；本表基于本项目实际安装的
`nicegui==3.18.0`。）

### 2.2 为什么并排放 4 个计数器而不是 3 个

最初的设想只对比"模块级全局变量"（反面教材）、`app.storage.tab`、
`app.storage.user` 三者。但读源码时发现 `app.storage.general` 和"模块级
全局变量"表现上很像（都是所有人共享同一份），却有一个本质区别：
`app.storage.general` 是 NiceGUI **官方认可的、故意共享、会持久化到磁盘**
的存储；模块级变量纯粹是 Python 语言特性的副作用，连文档里都没有"故意
设计成这样"的说法。把这两者并排放在一起，才能让读者看清"故意共享"
和"不小心共享"之间的边界——这正是 `docs/security_review.md` 反复强调的
"多用户状态泄露"问题的核心。因此最终选了 4 个并排计数器。

没有把 `app.storage.browser` 做成第 5 个卡片：它的"响应发出去之后就不能
再修改"这条限制，和"点击按钮之后立刻看到新值"这种计数器体验天然冲突
（按钮点击的事件回调永远发生在"初始响应已经发出去"之后），放进来只会
制造一个"点了没反应"的困惑体验，而不是一次有效的对比。

## 3. Architecture

```
+------------------------------------------------------------------+
| UI 层：examples/09_multi_user_state.py                           |
|                                                                    |
| multi_user_state_page()（async def，@ui.page('/multi-user-state')）|
|   -> 立即渲染：标题 / 说明 / "怎么做真实对比实验" 横幅               |
|   -> 立即渲染：① 全局计数器卡片、③ user 卡片、④ general 卡片       |
|   -> await ui.context.client.connected()  （见 Failure Modes #3）  |
|   -> 连接建立后补渲染：② tab 计数器卡片                             |
|   -> _build_history_demo_section()：把示例 8 的 _HISTORY 问题       |
|      改造成 app.storage.user['history']: list[dict] 的演示          |
|                                                                    |
| _build_counter_card()：4 个卡片共用的"标题+说明+当前值+按钮"构建函数 |
| _increment_storage_counter()：tab/user/general 共用的 +1 逻辑       |
| _task_record_to_storage_dict() / _storage_dict_to_summary_text()：  |
|   TaskRecord <-> JSON 兼容 dict 的双向转换（见第 6 节 Lifecycle）    |
+------------------------------------------------------------------+
                              |
                              v  读写
+------------------------------------------------------------------+
| NiceGUI 存储层：nicegui/storage.py（本项目 .venv 里的真实实现）     |
| - _GLOBAL_PROCESS_COUNTER：本文件自己的模块级变量，不属于 nicegui   |
| - app.storage.tab   -> Storage.tab   -> ObservableDict（按 tab_id） |
| - app.storage.user  -> Storage.user  -> FilePersistentDict（按会话）|
| - app.storage.general -> Storage.general -> FilePersistentDict      |
|   （全局唯一，落盘到 .nicegui/storage-general.json）                |
+------------------------------------------------------------------+
                              |
                              v  user/general 落盘位置
+------------------------------------------------------------------+
| 磁盘：.nicegui/storage-general.json                                |
|       .nicegui/storage-user-<session_id>.json（每个会话一份）       |
+------------------------------------------------------------------+
```

依赖方向：`examples/09_multi_user_state.py` 依赖 `nicegui`（`app`/`ui`）
和 `models.task`（仅用于"把示例 8 的 `_HISTORY` 改造成按用户隔离"这一段
演示代码），不依赖 `services/`、`infrastructure/`，也**不修改**
`examples/08_test_console.py`。

## 4. Complete Code

完整源码请直接查看仓库文件 `examples/09_multi_user_state.py`（本文档不
重复粘贴全文，避免两处代码后续修改时出现不一致）。以下是四个计数器
卡片的核心定义片段，展示"同一套 UI 构建函数，背后换一种存储实现"这个
设计：

```python
def _increment_storage_counter(storage: MutableMapping) -> int:
    """app.storage.tab / .user / .general 通用的"+1"逻辑。"""
    storage['counter'] = storage.get('counter', 0) + 1
    return storage['counter']


with ui.row().classes('w-full gap-4 mt-4 items-stretch'):
    _build_counter_card(
        card_mark='global-counter-card',
        get_value=lambda: _GLOBAL_PROCESS_COUNTER,
        on_increment=_increment_global_counter,
        ...,
    )
    tab_card_slot = ui.column().classes('contents')  # 占位，见下方
    _build_counter_card(
        card_mark='user-counter-card',
        get_value=lambda: app.storage.user.get('counter', 0),
        on_increment=lambda: _increment_storage_counter(app.storage.user),
        ...,
    )
    _build_counter_card(
        card_mark='general-counter-card',
        get_value=lambda: app.storage.general.get('counter', 0),
        on_increment=lambda: _increment_storage_counter(app.storage.general),
        ...,
    )

# app.storage.tab 必须等 WebSocket 连接建立后才能访问，所以延后到这里：
await ui.context.client.connected()
with tab_card_slot:
    _build_counter_card(
        card_mark='tab-counter-card',
        get_value=lambda: app.storage.tab.get('counter', 0),
        on_increment=lambda: _increment_storage_counter(app.storage.tab),
        ...,
    )
```

"把示例 8 的 `_HISTORY` 改造成按用户隔离"这段的核心转换函数：

```python
def _task_record_to_storage_dict(record: TaskRecord) -> dict:
    """把一个 TaskRecord 转换成可以安全存入 app.storage.user 的 dict。"""
    return {
        'id': record.id,
        'name': record.name,
        'status': record.status.value,                 # Enum -> str
        'created_at': record.created_at.isoformat(),     # datetime -> str
        'finished_at': record.finished_at.isoformat() if record.finished_at else None,
        'result': record.result,
        'error': record.error,
        'is_simulated': record.is_simulated,
    }


def handle_add_history_entry() -> None:
    record = _simulate_one_history_entry()
    entry_dict = _task_record_to_storage_dict(record)
    history: list[dict] = list(app.storage.user.get('history', []))
    history.append(entry_dict)
    app.storage.user['history'] = history   # 整体重新赋值，确保触发持久化
```

## 5. Execution

本机已在项目根目录创建并安装好 `.venv`（见本次任务的交付报告）。真实
可执行的命令如下：

```bash
cd /home/morrism/myNote/nicegui_learning_lab
. .venv/bin/activate

# 1) 语法检查
python3 -m py_compile examples/09_multi_user_state.py

# 2) 真实的多用户集成测试（见第 8 节、交付报告的真实输出）
python3 -m pytest -v tests/test_multi_user_state.py

# 3) 启动服务（注意：项目里 examples/*.py 直接用 `python3 examples/xxx.py`
#    运行时，`models`/`services` 包不在 sys.path 里，这不是本示例引入的
#    新问题——examples/08_test_console.py 等既有示例直接这样跑同样会
#    ModuleNotFoundError: No module named 'models'，必须显式把项目根目录
#    加进 PYTHONPATH；这是本次审查顺带发现、但按任务范围不允许去改
#    examples/01-08 的一个既有现象，这里只在本示例自己的文档里提醒读者）
PYTHONPATH=. python3 examples/09_multi_user_state.py
# 浏览器访问 http://localhost:8080/multi-user-state
```

打开页面后：

1. 点击"① 进程全局计数器"的 "+1" 几次，记下数字；开一个新标签页访问
   同一地址，会发现新标签页里显示的不是 0，而是接着刚才的数字继续。
2. 在同一个浏览器开两个标签页，分别点击"② app.storage.tab"的 "+1"：
   两个标签页的数字应该各自独立增长；刷新任意一个标签页，它的数字应该
   保持不变。
3. 在同一个浏览器开两个标签页，点击任意一个的"③ app.storage.user"的
   "+1"，切到另一个标签页刷新，应该看到共享的数字；换一个隐身窗口打开，
   应该从 0 开始。
4. 换隐身窗口访问"④ app.storage.general"：应该看到和原浏览器完全一样
   的数字（与③不同、与①相似，但它是官方持久化存储）。
5. 滚动到页面底部"附加演示"区域，点击"模拟新增一条历史记录"：当前
   浏览器会多一条历史；换隐身窗口打开，应该是空列表。

## 6. Lifecycle

以"两个独立浏览器各自打开页面、各自点击计数器"这个真实场景为例（与
`tests/test_multi_user_state.py` 里实际跑过的断言完全对应）：

1. **用户 A 的浏览器第一次打开 `/multi-user-state`**：这是一次普通的
   HTTP GET 请求，`SessionMiddleware`（由 `set_storage_secret()` 在
   `ui.run(storage_secret=...)` 时注册）发现这个请求没有会话 cookie，
   签发一个全新的、加密的 `session['id']`。`multi_user_state_page()`
   开始同步执行：渲染标题、说明、横幅，然后依次构建①全局、③用户、
   ④全局共享三个卡片——这三者此刻都能正常访问（①是普通 Python 变量，
   ③④只需要 `request` 上下文，这次 HTTP 请求本身就提供了），对应的
   `get_value()` 分别返回 `0`（假设是本进程第一次访问）、
   `app.storage.user.get('counter', 0)`（这个会话第一次访问，返回
   `0`）、`app.storage.general.get('counter', 0)`（可能是非 0，如果之前
   有其他用户点过）。
2. **`await ui.context.client.connected()`**：页面函数在这里暂停。
   浏览器收到的第一份响应里已经包含了①③④三个卡片的初始状态，但还
   没有②标签页计数器卡片。
3. **浏览器建立 WebSocket 连接**：NiceGUI 的 JS 运行时自动发起 socket.io
   握手，带上一个 `tab_id`（如果是这个浏览器第一次打开，是一个新生成的
   id；如果是刷新同一个标签页，JS 侧会尽量复用之前的 `tab_id`）。
   服务端 `Client.handle_handshake()` 调用
   `core.app.storage._create_tab_storage(tab_id, old_tab_id)`，为这个
   `tab_id` 建立一份新的 `ObservableDict`（或继承 `old_tab_id` 的数据）。
4. **`client.connected()` 的 `await` 恢复**：`multi_user_state_page()`
   继续往下执行，构建②标签页计数器卡片，此时 `app.storage.tab` 已经
   可以安全访问，返回 `0`（这个 tab_id 第一次出现）。
5. **用户 A 点击"③ app.storage.user"的 "+1" 三次**：每次点击触发
   `handle_click()` -> `_increment_storage_counter(app.storage.user)` ->
   `app.storage.user['counter'] = app.storage.user.get('counter', 0) +
   1`。这是对 `FilePersistentDict`（一个 `ObservableDict` 子类）的
   `__setitem__`，会触发一次"变化通知"，NiceGUI 在后台安排一次把整份
   用户存储序列化写入
   `.nicegui/storage-user-<session_id>.json` 的操作。界面上
   `value_label.text` 被同步更新为 `'3'`。
6. **用户 A 在同一个浏览器开第二个标签页，访问同一地址**：这次 HTTP
   请求**带着**和第一个标签页相同的会话 cookie（同一个浏览器），所以
   `request.session['id']` 相同，`app.storage.user` 取到的是**同一份**
   `FilePersistentDict`——③卡片初始渲染时 `get_value()` 直接返回 `3`，
   和第一个标签页点出来的值一致。但 WebSocket 握手会生成一个**不同**的
   `tab_id`，所以②卡片在 `connected()` 之后渲染出来的值是全新的 `0`。
7. **换一个完全独立的浏览器（或隐身窗口）访问同一地址**：这次请求没有
   第 1 步签发的那个会话 cookie，`SessionMiddleware` 签发一个全新的
   `session['id']`，`app.storage.user` 对应一份全新的、空的
   `FilePersistentDict`——③卡片从 `0` 开始，与用户 A 的会话完全无关。
   但④卡片（`app.storage.general`）此刻会看到用户 A 之前点出来的值，
   因为它不区分任何会话，是进程里唯一的一份。
8. **点击"模拟新增一条历史记录"**：`_simulate_one_history_entry()`
   同步创建一个 `TaskRecord`，走完 `PENDING -> RUNNING -> SUCCEEDED`
   的合法迁移（全程通过 `transition_to()`，不直接赋值 `.status`，与
   项目规格一致），`_task_record_to_storage_dict()` 把它转换成一个只含
   `str`/`bool`/`None` 的 dict，整体追加进
   `app.storage.user['history']` 并重新整体赋值回去，触发持久化。
   这份历史记录的作用域和③用户计数器完全一样——同一会话的所有标签页
   共享，不同会话互不干扰。

## 7. API Reference

### `examples/09_multi_user_state.py`

| 名称 | 说明 |
| --- | --- |
| `STORAGE_SECRET_FOR_LOCAL_TEACHING_DEMO_ONLY` | 仅供本地教学演示的固定密钥；生产环境必须改为从环境变量/密钥管理系统读取（见模块文档）。 |
| `_GLOBAL_PROCESS_COUNTER` / `_increment_global_counter()` | 反面教材：模块级可变变量，所有用户/标签页共享，进程重启丢失。 |
| `_increment_storage_counter(storage)` | `app.storage.tab`/`.user`/`.general` 通用的 `+1` 逻辑，三者运行时都兼容 `MutableMapping` 接口。 |
| `_build_counter_card(...)` | 构建单个"标题+说明+当前值+按钮"卡片的通用函数，四个卡片共用。 |
| `multi_user_state_page()`（`async def`，路由 `/multi-user-state`） | 主页面：先渲染①③④卡片，`await ui.context.client.connected()` 后再渲染②卡片，最后渲染历史演示区块。 |
| `_task_record_to_storage_dict(record)` | 把 `TaskRecord` 转成可安全存入 `app.storage.user` 的 JSON 兼容 `dict`（`Enum.value`、`datetime.isoformat()`）。 |
| `_storage_dict_to_summary_text(data)` | 把存储里的 `dict` 还原成一行可读摘要（不是完整还原成 `TaskRecord`）。 |
| `_simulate_one_history_entry()` | 同步构造一个终态 `TaskRecord`，用于历史演示区块（不使用 `asyncio.sleep`/后台协程）。 |

### 复用自 `models/task.py`（见示例 1~8 文档，这里不重复展开）

| 名称 | 说明 |
| --- | --- |
| `TaskRecord` / `TaskStatus` / `new_task()` | 与示例 5/8 完全相同的任务生命周期词汇表，`transition_to()` 是唯一允许修改 `.status` 的方式。 |

### NiceGUI 存储 API（见第 2 节表格，这里只列出本示例实际用到的方法/属性）

| 名称 | 说明 |
| --- | --- |
| `app.storage.tab` / `.user` / `.general` | `MutableMapping` 接口（`get()`/`__setitem__`），具体类型见第 2 节表格。 |
| `ui.context.client.connected(timeout=None)` | 协程；阻塞直到当前客户端的 WebSocket 连接建立，`app.storage.tab` 必须在它之后才能访问。 |

## 8. Failure Modes

1. **忘记传 `storage_secret`，导致 `app.storage.user` 抛 `RuntimeError`**
   - 现象：如果 `ui.run(...)` 没有传 `storage_secret`，第一次访问
     `app.storage.user` 时会直接抛出
     `RuntimeError('app.storage.user needs a storage_secret passed in
     ui.run()')`（源码见 `nicegui/storage.py` 的 `Storage.user` 属性，
     本文档第 2.1 节已引用）。`app.storage.browser` 会抛出同样性质的
     错误。
   - 排查方法：检查 `ui.run()` 调用是否传了非 `None` 的 `storage_secret`
     参数。本示例的 `STORAGE_SECRET_FOR_LOCAL_TEACHING_DEMO_ONLY` 就是
     为了避免这个坑而特意定义、特意传入的。
   - 修复方法：传入一个密钥字符串；生产环境应从环境变量/密钥管理系统
     读取，绝不能像本示例一样硬编码一个明显的占位字符串。

2. **把不可 JSON 序列化的对象直接存进 `app.storage`，第一次写入"看起来
   成功"，但重启/跨进程读取后得到的不是你期望的类型**
   - 现象：这是本示例实际跑脚本验证出来的一个微妙陷阱，**不是理论
     推测**。直接执行：
     ```python
     from nicegui.persistence.file_persistent_dict import FilePersistentDict
     d1 = FilePersistentDict(path)
     await d1.initialize()
     d1['task'] = some_task_record   # TaskRecord 实例，不是 dict
     ```
     第一次写入**不会报错**——因为 NiceGUI 底层用 `orjson` 做序列化，
     恰好原生支持 dataclass/`Enum`/`datetime`，`__setitem__` 触发的那一次
     持久化能成功把 `TaskRecord` 序列化成 JSON。但接下来：
     - 如果调用 `some_task_record.append_log(...)` 之类**直接修改
       dataclass 内部属性**的操作，`ObservableDict`（见
       `nicegui/observables.py` 的 `ObservableCollection._observe()`，
       只认识原生 `dict`/`list`/`set`，不认识任意 dataclass 实例）完全
       感知不到这次修改，**不会**触发新的持久化。
     - 如果模拟"进程重启"（用一个全新的 `FilePersistentDict` 实例重新
       `initialize()` 同一个文件），读回来的 `d2['task']` 类型是
       `ObservableDict`（一个普通字典），**不是** `TaskRecord`。本示例
       实际执行过这个验证，真实输出是：
       ```
       进程1 内存里的类型: <class 'models.task.TaskRecord'>
       mutate 之后磁盘内容是否包含新日志: False
       进程2（模拟重启）读回的类型: <class 'nicegui.observables.ObservableDict'>
       AttributeError: 'ObservableDict' object has no attribute 'status'
       ```
   - 排查方法：如果发现"重启后 `app.storage.user` 里的某个值访问
     `.属性`报 `AttributeError`"，先检查当初写入的是不是一个 dataclass
     实例，而不是一个已经转换好的 `dict`。
   - 修复方法：本示例给出的正式解法——写入前用
     `_task_record_to_storage_dict()` 把 `TaskRecord` 转成只含 JSON 原生
     类型的 `dict`，读出后如果需要完整对象，自己写转换函数还原
     （`TaskStatus(字符串)`、`datetime.fromisoformat(字符串)`），不要
     依赖 nicegui 替你做任何隐式的"看起来能存就直接存"。

3. **误以为 `app.storage.tab` 可以在页面函数一开始就访问，结果在真实
   启动服务后遇到 `RuntimeError`**
   - 现象：这同样是本示例在真实启动服务、用 `curl` 访问
     `/multi-user-state` 时**真实触发过**的报错（不是理论推测），完整
     堆栈的关键几行：
     ```
     File ".../examples/09_multi_user_state.py", line 285, in multi_user_state_page
       _build_counter_card(...)
     File ".../examples/09_multi_user_state.py", line 291, in <lambda>
       get_value=lambda: app.storage.tab.get('counter', 0),
     File ".../nicegui/storage.py", line 176, in tab
       raise RuntimeError('app.storage.tab can only be used with a client connection; '
     RuntimeError: app.storage.tab can only be used with a client connection; ...
     ```
   - 原因：`@ui.page` 装饰的函数在"浏览器发出第一次 HTTP 请求"时就会
     开始执行，但这个阶段还没有建立 WebSocket 连接，而 `client.tab_id`
     （`app.storage.tab` 的身份标识）是在 WebSocket 握手时才分配的。
     `app.storage.user`/`.general` 不受影响，因为它们只需要
     `request`（HTTP 请求本身就有），不需要 socket 连接。
   - 修复方法：把页面函数声明为 `async def`，在访问 `app.storage.tab`
     之前先 `await ui.context.client.connected()`（本示例的实际做法，
     见第 4 节代码片段）；或者把依赖 `app.storage.tab` 的 UI 构建逻辑
     整体放进一个 `async` 的事件回调里，而不是放在页面函数的顶层。

## 9. Exercises

1. **练习：把"全局计数器"也改造成"显式共享"，并观察行为是否不变。**
   要求：新增一个 `app.storage.general` 的第二个 key（例如
   `'counter_v2'`），不要动原来的 `_GLOBAL_PROCESS_COUNTER`，而是新加一
   个按钮，点击后同时递增 `_GLOBAL_PROCESS_COUNTER` 和
   `app.storage.general['counter_v2']`，并把两个值都展示出来。
   验收标准：重启一次 Python 进程（`Ctrl+C` 再重新 `python3
   examples/09_multi_user_state.py`）后刷新页面，`_GLOBAL_PROCESS_
   COUNTER` 展示的值归零，而 `app.storage.general['counter_v2']` 展示的
   值保持重启前的数字不变——用这个对比亲手验证"进程变量"和
   "`app.storage.general`"唯一的本质区别就是"是否持久化"。

2. **练习：给 `app.storage.user` 计数器加一个"重置"按钮，并验证它不会
   影响其他用户。**
   要求：新增一个"重置用户计数器"按钮，点击后把
   `app.storage.user['counter']` 设回 `0`。
   验收标准：用两个不同的浏览器（或一个普通窗口 + 一个隐身窗口）各自把
   用户计数器点到不同的值，在其中一个浏览器点击"重置"，另一个浏览器的
   用户计数器刷新后应该**保持不受影响**；可以直接运行
   `tests/test_multi_user_state.py` 里已经写好的多用户隔离测试作为
   参照，补充一条新的断言覆盖"重置"按钮。

3. **练习：把"历史演示区块"的 `app.storage.user['history']` 换成真正可以
   还原成 `TaskRecord` 的版本。**
   要求：新增一个函数 `_storage_dict_to_task_record(data: dict) ->
   TaskRecord`，把 `_task_record_to_storage_dict()` 的输出完整还原成一个
   `TaskRecord` 实例（包括用 `TaskStatus(data['status'])` 还原枚举、用
   `datetime.fromisoformat(data['created_at'])` 还原时间）；在"模拟新增
   一条历史记录"旁边加一个"查看详情"按钮，点击后用还原出来的
   `TaskRecord` 展示完整字段。
   验收标准：还原出来的 `TaskRecord` 的 `status`/`created_at` 字段类型
   分别必须是 `TaskStatus`/`datetime`（不是字符串），且
   `record.status.value` 能正常访问而不报错；补充一个不依赖 nicegui 的
   纯单元测试，断言"转成 dict 再还原"之后，除了 `logs`（本示例的
   `_task_record_to_storage_dict()` 没有保留这个字段）之外的字段都和
   原始 `TaskRecord` 相等。

## 10. Summary

示例 9 用"同一个页面、四个并排计数器"的方式，把 `app.storage.*` 几种
作用域之间最容易被混淆的边界变成了读者可以亲手验证的真实实验，而不是
一份需要背下来的对照表：

- **进程全局变量**（反面教材）与 **`app.storage.general`** 表现相似
  （都共享），但前者是语言特性的副作用、后者是官方认可且持久化的共享
  存储——这个对比直接呼应了 `docs/security_review.md` 里反复强调的
  "故意共享"与"不小心共享"的边界问题。
- **`app.storage.tab`** 与 **`app.storage.user`** 的隔离粒度不同
  （标签页 vs. 会话），且 `app.storage.tab` 有一个本示例真实踩到过的
  前置条件——必须等 WebSocket 连接建立之后才能访问。
- 示例 8 留下的"`_HISTORY` 该怎么改造成按用户隔离"的问题，在这里给出了
  正式解法：**显式转换成 JSON 兼容的 `dict`**，而不是假设 nicegui 会
  替你处理任意 Python 对象——这个结论是通过实际运行脚本验证出来的
  （见第 8 节 Failure Modes 第 2 条），不是凭空的最佳实践建议。
- `tests/test_multi_user_state.py` 用 NiceGUI 官方的
  `nicegui.testing.user_simulation()` 真实模拟了两个独立浏览器 +
  同一浏览器的两个标签页，实际跑通并断言了"`app.storage.user` 按会话
  隔离、`app.storage.tab` 按标签页隔离、`app.storage.general` 全局共享"
  这三条核心结论——这些不是理论上"应该"成立的结论，而是真实执行
  `pytest` 得到的通过结果（完整输出见本次任务的交付报告）。
