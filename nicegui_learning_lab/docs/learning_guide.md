# 学习指南

本文档是整个项目的"学习路线图"：建议的阅读顺序、按主题分类的 API 速查、
常见错误清单，以及"如何把本项目里的模拟服务换成真实后端"的具体指引。

## 1. 建议的学习顺序

```text
01 -> 02 -> 03 -> 04 -> 05 -> 06 -> 07 -> 08 -> 09 -> app.py
最小应用  布局与组件 事件与状态 配置检查  任务表格  命令控制台 后台任务  测试控制台 多用户隔离 综合工具
```

每个示例对应 `docs/examples/0N_*.md` 里完整的十个小节（Why / 心智模型 /
架构 / 完整代码 / 运行方式 / 生命周期 / API 参考 / 失败模式 / 练习 / 小结）
——这是比本文档更详细的权威说明，本文档只做跨示例的横向总结。

- **01~03（基础三件套）**：不涉及 Service 层，只讲清楚"事件 -> 回调 -> 修改
  组件"这条链路，以及"页面局部状态"和"模块级共享状态"的区别。读完这三个
  应该能独立判断"我写的这段 NiceGUI 代码里，哪些变量是安全的、哪些可能
  在多用户场景下出问题"。
- **04~06（Service 层与安全边界）**：引入 `services/*.py`，展示"UI 层
  不做任何业务判断"的分层方式，以及命令执行/路径访问这类高风险操作该如何
  收紧到一个固定白名单。读完这三个应该能理解为什么 `services/command_service.py`
  里不存在"传一个命令字符串就能跑"的入口。
- **07~08（异步与生命周期）**：引入 `background_tasks.create()`、
  `asyncio.Event` 协作式取消、`TaskRecord`/`TaskStatus` 状态机。读完应该
  能解释"取消一个协程"和"杀掉一个外部进程"的本质区别。
- **09（多用户）**：把前面所有"模块级全局变量"的隐患正式摊开讲，展示
  `app.storage.*` 的五种真实语义。读完应该能看着一段 NiceGUI 代码里的
  模块级变量，判断它在多用户环境下是否安全。
- **`app.py`**：把所有心智模型和 Service 组装成一个真实能长期跑着用的
  工具，是"从 Demo 到实际工具"的样板。

## 2. API 速查（按用途分类）

| 用途 | API | 首次出现 |
| --- | --- | --- |
| 最基本的组件 | `ui.label`、`ui.button(on_click=...)`、`ui.notify` | 01 |
| 布局 | `ui.row`、`ui.column`、`ui.card`、`ui.tabs`/`ui.tab_panels` | 02、06 |
| 输入组件 | `ui.input`、`ui.select`、`ui.textarea`、`ui.checkbox`、`ui.upload` | 02、03、04、08 |
| 分页表格 | `ui.table(columns, rows, pagination=..., on_pagination_change=...)` | 05 |
| 代码/日志展示 | `ui.code`、`ui.log(max_lines=...)` | 04、06、07 |
| 进度/等待反馈 | `ui.linear_progress`、`ui.spinner` | 07、08 |
| 弹窗 | `ui.dialog` | 05、08 |
| 测试钩子 | `.mark('some-id')` | 全部示例 |
| 后台任务 | `from nicegui import background_tasks; background_tasks.create(coro, name=...)` | 07 |
| 阻塞调用丢进线程池 | `from nicegui import run; await run.io_bound(fn, *args)` | 06 |
| 真正的外部进程（非阻塞流式） | `asyncio.create_subprocess_exec()` | `infrastructure/process_runner.stream_subprocess()` |
| 等待客户端连接建立 | `await ui.context.client.connected()` | 09（`app.storage.tab` 必须在此之后才能访问） |
| 多用户/多标签页存储 | `app.storage.client/tab/user/browser/general` | 09 |
| 无头渲染测试 | `nicegui.testing.User` / `user_simulation()` | `tests/test_multi_user_state.py`、`tests/test_app_integration.py` |

## 3. 常见错误清单（横跨多个示例的失败模式汇总）

详细的失败模式分析分散在每个 `docs/examples/0N_*.md` 的"Failure Modes"
小节里（每个示例至少 3 条），这里只汇总几条跨示例反复出现、最值得记住的：

1. **裸用 `asyncio.create_task()` 而不是 `background_tasks.create()`**
   ——没有强引用时任务可能被垃圾回收器意外回收，协程"跑到一半消失"且没有
   任何报错。
2. **在事件回调里直接调用阻塞函数（如 `subprocess.run()`）而不丢进
   `run.io_bound()`**——会卡住整个进程的事件循环，不只是当前用户的页面。
3. **忘记检查 `cancel_event.is_set()`**——协作式取消完全依赖代码里每一步
   是否记得检查，漏掉一步就意味着这段代码事实上不可取消。
4. **用模块级可变变量（`list`/`dict`）表示"这个页面的状态"**——在单用户
   手动测试时完全看不出问题，一旦有第二个用户/标签页访问就会看到"串台"
   的数据（示例 8 的 `_HISTORY` 是刻意保留的反面教材，示例 9 给出正式
   解法）。
5. **在 `app.storage.tab` 还没能访问时就使用它**——必须先
   `await ui.context.client.connected()`，否则会在真实运行时抛出
   `RuntimeError: app.storage.tab can only be used with a client connection`
   （这是集成阶段真实复现过的错误，见 `examples/09_multi_user_state.py`
   的代码注释）。
6. **把一个 dataclass 实例直接存进 `app.storage.user`**——`nicegui` 的
   `ObservableDict` 只认识原生 `dict`/`list`/`set`，之后对 dataclass 字段
   的直接修改不会被自动持久化；正确做法是显式转换成只包含 JSON 原生类型
   的 `dict`（见 `examples/09_multi_user_state.py` 的
   `_task_record_to_storage_dict()`）。
7. **用 `curl` + 检查原始 HTML 验证 NiceGUI 页面是否正常**——NiceGUI 的
   首次 HTTP 响应只是一个空的客户端渲染外壳，真正内容在 WebSocket 连接
   建立后才会出现；正确的无头验证方式见 `docs/architecture.md` 第 2 节。
8. **直接给 `TaskRecord.status` 赋值而不是调用 `transition_to()`**——会
   绕过合法状态迁移表的校验，产生理论上不该存在的状态跳转（例如还没
   `RUNNING` 过就变成 `SUCCEEDED`）。

## 4. 从 Demo 到真实工具：如何把模拟服务换成真实后端

本项目里所有"看起来像是在做真实操作"的地方，实际上全部是内存模拟
（`TaskRecord.is_simulated=True`），这是刻意的教学设计。如果要把它们接入
真实系统，建议按以下方式演进，而不是直接在现有文件上大改：

### 4.1 `services/test_service.py`（模拟测试用例）→ 真实设备巡检

- 保持 `run_test_suite()` 的函数签名不变（`selected_ids`、`on_progress`、
  `on_log`、`cancel_event` 这套协议已经被 UI 层依赖）。
- 新增一个 `is_simulated=False` 的执行路径：把 `TEST_SUITE` 里
  `run_ping_sweep` 这类用例换成真正调用
  `services.command_service.run_command('ping_once', [host])`（示例 6 已有
  的白名单命令），而不是 `asyncio.sleep()`。
- 协作式取消的检查点、`TaskRecord` 状态机完全可以保持不变——这套机制
  本来就是为了同时支撑模拟和真实场景设计的。
- UI 上去掉"[模拟]"文案前，必须先确认 `is_simulated` 字段确实被设置为
  `False`，并且异常处理覆盖了真实网络调用可能出现的所有失败模式（超时、
  目标不可达、权限问题），而不只是模拟场景下的"必定失败用例"。

### 4.2 `services/command_service.py`（白名单命令）→ 更多真实诊断命令

- 新增命令必须走 `_build_whitelist()` 里相同的模式：固定 `executable`
  （`shutil.which()` 解析）、明确的 `ArgSpec`（`choice` 或 `regex`）、
  固定 `cwd`/`timeout`/`max_output_bytes`。
- **绝不**为了"方便"引入一个"自由输入完整命令行"的入口——这正是本项目
  `docs/security_review.md` 反复强调要避免的远程命令执行面。
- 如果新命令需要访问真实网络设备（而不是本机诊断命令），应该在
  `infrastructure/` 下新增一个独立模块（例如未来的 `ssh_runner.py`），
  复用 `services/errors.py` 的异常体系，而不是把 SSH 连接逻辑直接写进
  `command_service.py`。

### 4.3 `services/task_query_service.py`（模拟任务列表）→ 真实任务系统

- `generate_mock_tasks()` 是"数据从哪来"，`list_tasks()` 是"怎么分页/
  筛选"——这两个职责本来就是分开的。接入真实系统时，只需要替换前者（比如
  改成从数据库/真实队列系统读取），`list_tasks()` 这个纯函数可以完全不变，
  因为它只依赖"一份 `TaskRecord` 列表"这个抽象，不关心数据来自哪里。

### 4.4 示例 8/9 的"历史记录" → 真实持久化

- 示例 8 的模块级 `_HISTORY` 和示例 9"按用户隔离"的 `app.storage.user`
  方案都是教学简化（前者明确是反面教材，后者是正式解法但仍然是单进程
  内存+文件存储）。真实的多实例部署通常需要：
  - 用 Redis 等外部存储后端配置 NiceGUI 的 storage（`app.storage` 支持
    这种模式，但本项目未实际验证，留作后续练习），使得历史记录可以在
    多个应用进程/多台机器之间共享；
  - 或者彻底绕开 `app.storage`，直接把 `TaskRecord` 写入真实数据库，
    `app.storage.user` 只保存"这个用户最近查看的任务 id 列表"这类轻量
    索引。

## 5. 本次任务明确未覆盖的部分（如实列出，而非回避）

见 [`docs/architecture.md`](architecture.md) 第 3 节"已知局限"：真实浏览器
UI 测试、真实 SSH/设备连接、多浏览器并发下的竞态/性能场景均未覆盖。
