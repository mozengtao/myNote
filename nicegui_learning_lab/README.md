# NiceGUI 学习实验室

一个面向有 C/C++/Go/Rust 背景、熟悉基础 Python、但不熟悉 Web 前端的读者的
NiceGUI 完整学习项目：9 个由浅入深的独立可运行示例、一个分层架构
（Model / Service / Infrastructure / UI）、一套完整的心智模型文档，以及
一个把所有示例的能力组装成真实小工具的 `app.py`。

## 快速开始

```bash
cd nicegui_learning_lab
python3 -m venv .venv
. .venv/bin/activate
pip install -r requirements.txt

# 运行纯 Python 测试（不依赖 nicegui 是否安装）
python3 -m pytest -q

# 启动综合工具台
python3 app.py
# 浏览器访问 http://localhost:8080

# 或者单独运行某一个教学示例，例如示例 6
python3 examples/06_command_console.py
# 浏览器访问 http://localhost:8080
```

## 目录地图

```text
nicegui_learning_lab/
├── app.py                 # 综合工具台入口：组装 ui/*.py 四个面板
├── requirements.txt        # nicegui==3.18.0、pytest>=7.4
├── README.md                本文件
├── .gitignore
├── config/                 # 示例 4 使用的 JSON 配置样例（含一份故意语法错误的夹具）
├── docs/
│   ├── mental_models.md    # 应用运行模型 / UI-状态-事件-Service 关系 / 异步执行模型
│   ├── architecture.md     # 分层结构、依赖方向、无头验证方法原理
│   ├── learning_guide.md   # 学习路线图、API 速查、常见错误、模拟服务换真实后端指引
│   ├── multitask_plan.md   # 本项目并行开发的真实执行记录
│   ├── security_review.md  # 跨示例的多用户/安全审查报告
│   └── examples/0N_*.md    # 每个示例完整的十节式文档（Why/心智模型/架构/.../小结）
├── examples/0N_*.py         # 9 个独立可运行的教学示例
├── ui/                      # app.py 的仪表盘外壳与四个功能面板
├── models/task.py           # TaskRecord / TaskStatus，全项目共用的"任务"词汇表
├── services/                # 业务规则、校验、异常体系、命令白名单（不依赖 nicegui）
├── infrastructure/          # 唯一允许调用 subprocess 的模块
└── tests/                   # pytest 测试套件（大多数不依赖 nicegui 是否安装）
```

## 9 个教学示例一览

| 编号 | 主题 | 讲清楚什么 |
| --- | --- | --- |
| 01 | 最小应用 | `ui.label` + `ui.button` + `ui.notify`，完整的请求/响应边界 |
| 02 | 布局与组件 | `ui.row`/`ui.column`/`ui.card`/输入组件 |
| 03 | 事件与状态 | 页面局部状态 vs 共享状态，先校验再展示 |
| 04 | JSON 配置检查器 | `ui.upload` 与服务端路径两条读取路径、目录穿越防护 |
| 05 | 任务表格 | `ui.table` 服务端分页/筛选，不一次性加载全部数据 |
| 06 | 命令控制台 | 固定白名单命令执行、路径确认、安全边界 |
| 07 | 后台任务 | `background_tasks.create()`、协作式取消、异常处理 |
| 08 | 综合测试控制台 | `TaskRecord` 生命周期、复用 Service 而非重新实现 |
| 09 | 多用户状态隔离 | 五种 `app.storage.*` 的真实语义、反面教材对比 |

每个示例都有一份对应的 `docs/examples/0N_*.md`，包含完整的 Why / 心智
模型 / 架构 / 完整代码 / 运行方式 / 生命周期 / API 参考 / 失败模式（≥3）/
练习（≥3，附验收标准）/ 小结十个小节。

## `app.py` 与 `examples/*.py` 的区别

`examples/*.py` 是教学用的独立单文件示例，各自讲清一个心智模型，互相之间
不复用对方的代码。`app.py`（配合 `ui/*.py`）则是把其中会被复用的 Service
调用（`config_service`、`command_service`、`task_query_service`、
`test_service`）组装成**一个**多页面应用：四个标签页（配置检查 / 命令
控制台 / 任务与测试 / 运行日志），复用与各示例完全相同的
`models/`/`services/`/`infrastructure/` 代码。这是"从教学 Demo 走向一个
可以长期跑着用的小工具"的样板，详见
[`docs/architecture.md`](docs/architecture.md)。

## 测试

```bash
# 完整测试套件（需要先 pip install -r requirements.txt）
python3 -m pytest -q

# 只跑不依赖 nicegui 的部分（models/、services/ 的纯逻辑测试）
python3 -m pytest -q tests/test_task_model.py tests/test_config_service.py \
    tests/test_command_service.py tests/test_task_query_service.py tests/test_test_service.py
```

真实的测试执行记录（命令与输出）见项目最终验收报告与
[`docs/multitask_plan.md`](docs/multitask_plan.md)。

## 学习路线

建议按 01 → 09 → `app.py` 的顺序阅读，详见
[`docs/learning_guide.md`](docs/learning_guide.md)。
