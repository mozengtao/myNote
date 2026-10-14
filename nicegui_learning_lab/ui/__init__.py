"""UI 层：`app.py` 的仪表盘外壳与各个功能面板。

本包与 `examples/01-09` 的关系
------------------------------
`examples/*.py` 是**教学用**的独立单文件示例，每一个都可以单独 `python3
examples/0N_xxx.py` 启动，专注讲清一个心智模型（事件、分页、命令执行、
后台任务、多用户状态……）。`ui/*.py` 则是把其中会被复用的 Service 调用，
组装成**一个**多页面"真实工具"（`app.py`）里的若干面板——这是从"Demo"走向
"实际工具"的桥梁（见 `docs/architecture.md`）。

两者刻意共用同一套 `models/`/`services/`/`infrastructure/`：面板里不会
重新实现任何一条业务规则或安全校验，只负责"收集用户输入 -> 调用 Service
-> 把结果或异常翻译成界面"，与 `examples/*.py` 的职责边界完全一致。

依赖方向
--------
本包里的模块可以 `import nicegui`、`import models.*`、`import services.*`，
但**不允许**被 `models/`、`services/`、`infrastructure/` 反向导入——依赖
方向只能是单向的 UI -> Service -> Infrastructure（见 `docs/architecture.md`
的分层图）。
"""
