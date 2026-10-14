# 架构文档

本文档描述 `nicegui_learning_lab` 的分层结构、依赖方向、已知的一处分层
例外，以及本项目采用的"无头渲染验证"方法及其原理。

## 1. 分层结构

```text
+-------------------------------------------------------------+
|  UI Layer             ui/*.py, examples/*.py, app.py        |
|  NiceGUI 组件、事件回调、页面组装；不包含任何业务规则         |
+------------------------------+--------------------------------+
                               |  调用
                               v
+-------------------------------------------------------------+
|  Service Layer         services/*.py                        |
|  配置/命令/测试相关的业务规则、校验、异常体系                 |
+------------------------------+--------------------------------+
                               |  调用
                               v
+-------------------------------------------------------------+
|  Infrastructure Layer  infrastructure/process_runner.py      |
|  真正执行 subprocess（禁止 shell=True）、超时、输出截断        |
+---------------------------------------------------------------+

                  models/task.py（TaskRecord, TaskStatus）
                  Service 层与 UI 层共用的"任务"词汇表
```

### 1.1 每一层的职责边界

- **UI 层**（`ui/*.py`、`examples/*.py`、`app.py`）：收集用户输入、调用
  Service、把返回值或抛出的异常翻译成界面文案。不做任何校验/业务判断——
  如果一个页面文件里出现了"判断某个字符串是不是合法主机名"这种逻辑，就说明
  这条规则被放错了层。
- **Service 层**（`services/*.py`）：全部业务规则、校验规则、异常体系
  （`services/errors.py`）的唯一来源。**绝不 import nicegui**——这保证了
  `tests/` 目录下大多数测试完全不依赖 nicegui 是否已安装。
- **Infrastructure 层**（`infrastructure/process_runner.py`）：与操作系统
  的边界——真正调用 `subprocess.run()`/`asyncio.create_subprocess_exec()`
  的唯一位置，全项目只有这一个文件允许这样做。
- **Model**（`models/task.py`）：`TaskRecord`/`TaskStatus` 这套"任务"词汇
  表，被 Service 层和 UI 层共同依赖，但自己不依赖任何人（纯标准库）。

### 1.2 依赖方向（人工代码评审保证，本项目未引入 import-linter 等工具强制）

```text
ui/*, examples/*, app.py
        |
        v
services/*  -------->  models/*
        |
        v
infrastructure/*  -->  （仅 services/errors.py，见下方例外说明）
        |
        v
      models/*
```

- `ui/*`、`examples/*`、`app.py` 可以 import `models/*`、`services/*`。
- `services/*` 可以 import `models/*`、`infrastructure/*`、标准库——
  **绝不 import nicegui**。
- `infrastructure/*` 只能使用标准库，**唯一例外**见下一节。

### 1.3 已记录的分层例外：`infrastructure/process_runner.py` → `services/errors.py`

`infrastructure/process_runner.py` 被允许 import 且仅 import
`services/errors.py`（不 import `services/` 下任何其他模块，也不 import
nicegui）。这个例外的产生原因、合理性评估、长期风险与替代方案，已经在
两个独立的地方被记录过，本文档不重复展开，只给出结论与指针：

- 模块自身的解释：见 `infrastructure/process_runner.py` 的模块
  docstring"层次说明"一节。
- 一次独立的事后评审（由负责示例 9 与安全审查的协作者完成）：见
  [`docs/security_review.md`](security_review.md) 第 5 节，其中包含对这个
  例外的独立评估，以及一个更干净的替代方案建议——把
  `CommandTimeoutError`/`CommandExecutionError` 这类异常搬到 `models/`
  包，从根本上消除 `infrastructure -> services` 这条反向依赖箭头。这个
  建议具有后续参考价值，但**本次任务未实施**（超出当时各协作者的文件
  修改权限范围），留给后续迭代。

### 1.4 日志与异常约定

- 日志：标准库 `logging`，每个模块 `logging.getLogger(__name__)`，统一格式
  `%(asctime)s %(levelname)s %(name)s: %(message)s`。`app.py` 在启动时
  调用一次 `logging.basicConfig(...)`；每个独立的 `examples/0N_*.py` 也会
  各自调用一次（因为它们可以独立运行，不经过 `app.py`）。
- 异常：`services/errors.py` 定义了 `ServiceError` 基类及
  `NotFoundError`/`ValidationError`/`PermissionDeniedError`/
  `CommandTimeoutError`/`CommandExecutionError` 五个子类，所有 Service
  函数只抛出这几种类型，UI 层按类型分别捕获、翻译成对用户友好的提示。

## 2. 为什么用 `nicegui.testing.User` 做无头验证，而不是 `curl` + 检查 HTML

最初的验证计划（见历史的 Multitask 任务计划）设想用 `curl` 请求页面、在
返回的原始 HTML 里 grep `.mark()` 元素的文本来确认页面正常渲染。**这个
方法在集成阶段被证明是错误的**：真实启动 `app.py` 并用 `curl` 请求后发现，
NiceGUI 3.18 的首次 HTTP 响应只是一个空的 Vue/Quasar 应用外壳（页面标题
等极少量静态内容之外，几乎不包含任何实际组件），真正的页面内容是浏览器
与服务端建立 WebSocket 连接之后，由客户端渲染出来的——`curl` 拿到的原始
HTML 里**不会**出现 `.mark()` 文本，即使页面功能完全正常。

正确的做法是使用 NiceGUI 官方提供的 `nicegui.testing.User`（配合
`nicegui.testing.user_simulation()`）：它用一个真实的
`httpx.AsyncClient`（`transport=httpx.ASGITransport(core.app)`）走完整的
ASGI 请求栈，并等待页面真正建立"连接"（`await ui.context.client.connected()`
触发的那一步）、客户端渲染出 Vue 组件树之后，再用 `user.should_see(marker=...)`
检查某个 `.mark()` 元素是否存在/可见。`tests/test_app_integration.py` 用
这种方式对 `app.py` 与全部 9 个 `examples/*.py` 做了真实的无头渲染验证
（见该文件的模块 docstring 与 `docs/multitask_plan.md` 的"集成阶段修正
记录"一节）。

`tests/test_multi_user_state.py`（示例 9 的多用户隔离测试）用的是同一套
机制，只是额外用多个独立的 `httpx.AsyncClient`（各自独立的 cookie jar）
模拟多个互不相干的浏览器/用户。

## 3. 已知局限（本次任务明确未覆盖的验证范围）

- 真实浏览器 / Selenium 点击流 UI 测试：`nicegui.testing.User` 是"无头、
  不渲染真实 DOM/CSS"的模拟，不能替代真实浏览器里的视觉回归测试。
- 真实 SSH 目标、真实设备测试执行：`services/test_service.py`、示例 6 的
  `ping_once` 等全部是只读/模拟操作，没有连接任何真实网络设备。
- 多浏览器并发用户下的真实竞态场景：`tests/test_multi_user_state.py`
  验证的是"存储隔离的语义是否正确"，不是"大量并发请求下是否有性能瓶颈
  或锁竞争问题"。

详见 [`docs/learning_guide.md`](learning_guide.md) 与最终验收报告中对这些
局限的完整说明。
