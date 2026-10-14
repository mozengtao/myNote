# 场景 4：插件化任务执行引擎

> 运行：`tclsh task_runner.tcl`

## 1. 真实背景

批处理/跑批系统经常需要支持"可扩展的任务类型"：今天要备份数据库，明天要加一个生成报表的任务类型，后天可能是发邮件——理想情况下，**新增一种任务类型不需要修改主程序**，只需要新增一个插件文件。本例实现这样一个最小的插件化任务执行引擎。

## 2. 涉及的 Tcl 概念

对照 [tcl_mental_model.md](../../tcl_mental_model.md)：

| 概念 | 对应章节 | 在本例中的作用 |
|---|---|---|
| Nested Dict | 第 15 节 | 任务注册表 `taskName -> {plugin, params}` |
| `foreach` + `dict keys` | 第 21-22 节 | 依次加载插件文件、依次执行任务 |
| `eval` | 第 37 节 | 把动态拼出的 `[list ns::run params]` 当作命令执行 |
| `namespace` | 第 38 节 | 每个插件独立的 `::plugins::xxx`，互不冲突 |
| `package` 思想 | 第 39 节 | 插件就是"约定接口 + 独立单元"，本例用 `source` 简化演示动态加载 |
| `catch`（补充点） | 无专门章节 | 单个任务失败不影响整体执行 |

## 3. Mental Model

```
plugins/*.tcl
    |
    | foreach + source（动态加载，主程序不预先知道插件列表）
    v
::plugins::backup::run      ::plugins::report::run      ...
    ^                            ^
    |                            |
tasks (Nested Dict)
    backup-db    -> {plugin ::plugins::backup  params {...}}
    daily-report -> {plugin ::plugins::report  params {...}}
    broken-task  -> {plugin ::plugins::nonexistent params {}}
    |
    | foreach name [dict keys $tasks]
    v
runTask spec
    |
    | set runCmd [list ${plugin}::run $params]
    | catch {eval $runCmd}
    v
results (Dict: taskName -> {ok, message})
    |
    | foreach + format
    v
Execution Report（汇总成功/失败数）
```

## 4. 完整代码

- 主程序：[task_runner.tcl](task_runner.tcl)
- 插件：[plugins/plugin_backup.tcl](plugins/plugin_backup.tcl)、[plugins/plugin_report.tcl](plugins/plugin_report.tcl)

插件约定非常简单：

```tcl
namespace eval ::plugins::<name> {
    namespace export run
}
proc ::plugins::<name>::run {params} {
    # 用 params（一个 Dict）做事，返回结果字符串，失败用 error 抛出
}
```

## 5. 执行讲解

1. `glob -nocomplain -directory $pluginDir *.tcl` 找到 `plugins/` 目录下所有 `.tcl` 文件，`lsort` 保证加载顺序稳定；`foreach` + `source` 依次把它们加载进当前解释器——加载完之后，`::plugins::backup::run`、`::plugins::report::run` 这两个命令就存在了。**主程序的代码里完全没有出现 `plugin_backup.tcl`、`plugin_report.tcl` 这些具体文件名**，新增插件文件不需要改主程序这部分代码。
2. `tasks` 是一个 Nested Dict，每个任务描述自己要用哪个插件 namespace、传什么参数。`broken-task` 故意指向一个不存在的插件 `::plugins::nonexistent`，用来演示失败处理路径。
3. `runTask` 里 `set runCmd [list ${pluginNs}::run $params]` 用 `list` 构造出一个**结构良好**的命令（而不是用字符串拼接），再用 `eval $runCmd` 执行它。因为 `runCmd` 已经是一个合法的 List（`eval` 对一个 List 求值时，List 的每个元素就是一个独立的 word，不会被重新解析/二次替换），这种用法是安全的；mental model 第 37 节提到的注入风险，针对的是对**拼接自用户输入的字符串**做 `eval`，这里不属于那种情况。
4. `catch {eval $runCmd} message` 捕获执行过程中的任何错误（比如 `broken-task` 对应的 `::plugins::nonexistent::run` 根本不存在，会报 "invalid command name"），把错误信息存进 `message`，`runTask` 据此返回 `{ok 0 message ...}`，而不是让整个脚本因为一个任务失败而终止——这是任务执行引擎类系统的基本要求：**一个任务的失败不能拖垮整批任务**。
5. 最后用 `foreach name [dict keys $tasks]` 按任务注册的顺序（而不是 Dict 内部可能的遍历顺序）输出报表，这和场景 1 里"报表场景应显式指定顺序"的实践要点是一致的。

## 6. 工程实践要点

- **主程序与插件之间只通过"命名约定"耦合**（`<namespace>::run {params}`），不是直接的代码依赖——这正是 `namespace` 在工程上最大的价值之一：不同插件可以自由使用自己的辅助 proc、辅助变量，不用担心和其他插件或主程序撞名。
- **`eval` 只用在自己构造的、结构良好的 List 上**，不要对拼接自外部输入的字符串做 `eval`。如果未来需要支持"从配置文件读取参数再调用插件"，应该把参数解析成 Dict/List 之后再用 `list` 构造命令，而不是直接拼字符串。
- **用 `catch` 包住每个独立单元的执行**，是让批处理类系统具备"部分失败不影响整体"韧性的最简单手段。

## 7. 练习

1. 增加一个新插件 `plugin_notify.tcl`（`::plugins::notify::run`），在 `tasks` 里加一条用它的任务，验证不改 `task_runner.tcl` 主体逻辑就能接入新任务类型。
2. 给 `runTask` 增加超时模拟：如果插件执行时间"模拟"超过某个阈值（可以用一个固定 `after` 延迟代替真实耗时），也判定为失败。
