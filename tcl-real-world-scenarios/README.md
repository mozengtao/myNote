# Tcl 真实场景综合示例

> 本目录是 [tcl_mental_model.md](../tcl_mental_model.md) 的配套"综合实战"目录。
>
> `tcl_mental_model.md` 把 `set`/`$`/`[]`/`{}`/`""`、String/List/Dict/Array、
> `if`/`foreach`/`switch`/`expr`、`split`/`join`/`format`/`regexp`，以及
> `proc`/`upvar`/`uplevel`/`eval`/`namespace`/`package` 这些概念，都以**孤立的小片段**
> 形式讲清楚了。这里用 4 个接近真实项目的完整场景，把这些概念**串联起来**，
> 展示它们组合在一起解决实际问题时是什么样子。

## 场景列表（建议按顺序阅读，难度递增）

| # | 场景 | 核心问题 | 综合运用的概念 | 链接 |
|---|---|---|---|---|
| 1 | 日志文件分析与统计报表 | 一段文本日志，怎么变成一份结构化的统计报表？ | String / `split` / `regexp` / Dict / Nested Dict / `foreach` / `switch` / `format` / `proc` | [01-log-analyzer](01-log-analyzer/README.md) |
| 2 | 键值配置文件管理器 | 一份 `key=value` 配置文件，怎么安全地读写、带默认值？ | Nested Dict / Dict vs Array / `regexp` / `switch` / `proc` 默认参数 / `namespace` + `variable` | [02-config-manager](02-config-manager/README.md) |
| 3 | 命令行子命令框架 | 类似 `git <subcommand>` 的工具，内部是怎么组织的？ | `proc` + `args` / `switch` / `upvar` / `uplevel` / `namespace` / `package provide` / `{*}` | [03-cli-framework](03-cli-framework/README.md) |
| 4 | 插件化任务执行引擎 | 怎么做到"新增任务类型不用改主程序"？ | Nested Dict / `foreach` / `eval` / `namespace` / `package` 思想 / `catch` | [04-plugin-task-runner](04-plugin-task-runner/README.md) |

## 为什么是这 4 个场景

它们大致对应 `tcl_mental_model.md` 第 33 节"学习优先级"中 Level 2 - Level 6 的能力，并且每一个场景都不是单一概念的演示，而是**多个概念配合完成一件真实的事**：

```
场景 1: String -> List -> Dict -> Business Logic
            （Level 2-3：数据类型 + 控制流 的综合）
场景 2: Nested Dict + namespace variable
            （Level 2 + Level 6：数据结构选型 + 模块级状态）
场景 3: proc + upvar + uplevel + namespace + package
            （Level 6：进阶机制的综合，侧重"引用" vs "执行"的区别）
场景 4: namespace + eval + catch
            （Level 6：模块化 + 动态执行 + 错误隔离 的综合）
```

## 统一结构

每个场景的 `README.md` 都包含：

1. **真实背景** —— 为什么真实项目里会遇到这个需求
2. **涉及的 Tcl 概念** —— 逐条对照 `tcl_mental_model.md` 的章节号
3. **Mental Model** —— 一句话直觉 + ASCII 数据流图
4. **完整代码** —— 可直接 `tclsh xxx.tcl` 运行
5. **执行讲解** —— 按执行顺序逐步解释
6. **工程实践要点** —— 为什么这样组织代码、常见坑（包括至少一个真实踩坑记录）
7. **练习** —— 2 条留给读者自己扩展的题目

## 运行方式

所有示例都是纯 Tcl（不依赖 Tk），直接用 `tclsh` 运行：

```bash
cd tcl-real-world-scenarios/01-log-analyzer && tclsh log_analyzer.tcl
cd tcl-real-world-scenarios/02-config-manager && tclsh config_manager.tcl
cd tcl-real-world-scenarios/03-cli-framework && tclsh cli_tool.tcl
cd tcl-real-world-scenarios/04-plugin-task-runner && tclsh task_runner.tcl
```

场景 2 运行后会在同目录生成 `app.conf`、`app.saved.conf` 两个演示用文件，属于运行产物，可随时删除。
