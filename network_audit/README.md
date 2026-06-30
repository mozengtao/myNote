# network_audit

一个可直接运行的 Python 自动化示例项目，用于演示
[../bash/python_automation_guide.md](../bash/python_automation_guide.md) 中的分层心智模型：

> Config -> Workflow -> Service -> Infra -> Parser -> Model -> Analyzer -> Reporter

需求场景：采集多台设备信息 -> 解析 -> 分析 -> 生成 Markdown 报告。
SSH 用"打印命令 + 内置假 CLI 文本"代替，因此**无需任何真实设备即可运行**。

> 配套教学文档（逐层代码讲解 + 数据流图）见
> [../bash/python_automation_demo.md](../bash/python_automation_demo.md)。

## 运行方式

```bash
cd network_audit
python3 main.py
```

- 终端会按主机打印模拟的 SSH 连接与命令日志。
- 运行结束后在当前目录生成 `report.md`。
- 可选安装依赖（不装也能跑，loader 自带 YAML 降级解析）：

```bash
pip install -r requirements.txt
```

## 目录速览

```
network_audit/
+-- main.py                 # 入口：装配依赖并启动 Workflow
+-- config/
|     +-- config.yaml       # 外部配置：hosts / username / timeout
|     +-- loader.py         # YAML -> Config 对象（含无依赖降级解析）
+-- models/
|     +-- device.py         # Device / Interface 数据模型
|     +-- report.py         # AnalysisResult / Report 数据模型
+-- infra/
|     +-- ssh.py            # SSHClient：打印式 mock + 假 CLI 输出
+-- services/
|     +-- device_service.py # 领域能力：collect_version / collect_interfaces
+-- parsers/
|     +-- parser.py         # 纯函数：CLI 文本 -> 模型对象
+-- analysis/
|     +-- analyzer.py       # Device 列表 -> AnalysisResult
+-- workflow/
|     +-- audit.py          # 编排：collect -> parse -> analyze -> report
+-- reporter/
|     +-- markdown.py       # Report 对象 -> report.md
+-- utils/
      +-- logger.py         # 统一日志配置
```

## 逐层职责

| 层 | 目录 | 职责 | 关键类型/函数 |
| --- | --- | --- | --- |
| Configuration | `config/` | 外部配置 -> 对象 | `load_config` -> `Config` |
| Model | `models/` | 描述现实世界 | `Device`、`Interface`、`Report` |
| Infrastructure | `infra/` | 访问外部系统 | `SSHClient.execute` |
| Service | `services/` | 领域能力 | `DeviceService` |
| Parser | `parsers/` | 文本 -> 对象 | `parse_version`、`parse_interfaces` |
| Analyzer | `analysis/` | 对象 -> 汇总对象 | `analyze` -> `AnalysisResult` |
| Workflow | `workflow/` | 编排流程 | `AuditWorkflow.run` |
| Reporter | `reporter/` | 对象 -> 输出格式 | `MarkdownReporter.write` |

## 核心理念

整个程序传递的是**对象（Object Flow）**，而不是到处拼接的字符串：

```
Config -> Device -> AnalysisResult -> Report -> Markdown
```

想换成真实 SSH？只改 `infra/ssh.py`。想输出 HTML/JSON？只加一个 Reporter。
其余各层无需改动——这正是分层解耦的价值。
