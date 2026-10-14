# 场景 1：日志文件分析与统计报表

> 运行：`tclsh log_analyzer.tcl`

## 1. 真实背景

几乎每个后台服务都会产生文本日志。日常运维/排错中最常见的需求是：

- 统计一段时间内 INFO / WARN / ERROR 各出现多少次，快速判断系统健康度
- 把 WARN / ERROR 的具体内容单独列出来，不用在几千行日志里肉眼翻找

这是一个典型的 **String → List → Dict → Business Logic** 数据处理任务，本例把它做成一个完整的、可以直接运行的小工具。

## 2. 涉及的 Tcl 概念

对照 [tcl_mental_model.md](../../tcl_mental_model.md)：

| 概念 | 对应章节 | 在本例中的作用 |
|---|---|---|
| String | 第 10 节 | 原始日志本身就是一大段文本 |
| `split` | 第 23 节 | 把整段日志按 `\n` 切成行 |
| `regexp` | 第 26 节 | 从每行提取 timestamp / level / message |
| Dict / Nested Dict | 第 13-15 节 | 用 `counts`、`messages` 两个 Dict 做统计，并打包进一个外层 Dict |
| `foreach` | 第 21 节 | 遍历每一行、遍历统计结果 |
| `switch` | 第 27 节 | 按日志级别决定是否记录详细消息 |
| `format` | 第 24 节 | 生成对齐的报表文本 |
| `proc` | 第 34 节 | 把解析、统计、格式化拆成三个独立函数 |

## 3. Mental Model

一句话：**一段文本，经过 split 变成 List，经过 regexp + dict 变成结构化统计数据，最后经过 format 变回给人看的文本。**

```
rawLog (String)
    |
    | split "\n"
    v
lines (List)
    |
    | foreach + regexp
    v
entry (Dict: timestamp/level/message)   -- 每行一个
    |
    | dict incr / dict lappend
    v
stats (Dict)
    +-- counts    (Dict: level -> count)
    +-- messages  (Dict: level -> List of message)
    |
    | formatReport
    v
report (String)
```

## 4. 完整代码

见 [log_analyzer.tcl](log_analyzer.tcl)。核心结构：

```tcl
proc parseLogLine {line} { ... }   ;# String -> Dict（单行解析）
proc analyzeLog {rawText} { ... }  ;# String -> Dict（全量统计）
proc formatReport {stats} { ... }  ;# Dict -> String（报表生成）

set stats [analyzeLog $rawLog]
puts [formatReport $stats]
```

## 5. 执行讲解

1. `rawLog` 用 `{}` 包裹多行日志文本——这里特意用 `{}` 而不是 `""`，因为内容中不需要任何 `$` / `[]` substitution，`{}` 能让这段文本原样保留（对应 mental model 第 6-8 节 `{}` vs `""`）。
2. `analyzeLog` 先用 `split [string trim $rawText] "\n"` 把文本切成行的 List。
3. 对每一行调用 `parseLogLine`：
   - `regexp $pattern $line -> timestamp level message` 这种写法利用了 `regexp` 的"整体匹配丢弃、只取分组"语法（`->` 占位整体匹配结果），直接把三个分组写入三个变量。
   - 如果一行不符合格式（比如空行），`regexp` 返回 0，函数返回空 `dict`，调用方 `continue` 跳过。
4. 对每个成功解析的 entry：
   - `dict incr counts $level` 把该 level 的计数 +1（如果 key 不存在，`dict incr` 会自动从 0 开始）。
   - `switch -- $level { WARN - ERROR { ... } }` 中 `WARN - ERROR` 的写法表示"匹配到 WARN 或 ERROR 时，都执行下面这个分支"（case 穿透），只有这两个级别才会被 `dict lappend` 进 `messages`。
5. `formatReport` 对 `counts` 和 `messages` 分别用固定顺序（`ERROR WARN INFO`）遍历，而不是依赖 `dict for` 的遍历顺序——因为 **Dict 的键顺序对使用者不应有语义依赖**，报表这种需要稳定顺序的场景应该显式指定顺序。
6. `format "%-6s : %3d 条\n"` 生成左对齐 6 字符宽的级别名 + 右对齐 3 位数字的计数，保证多行对齐。

## 6. 工程实践要点

- **用 Dict 返回多个关联结果**：`analyzeLog` 没有用全局变量或者两个返回值，而是把 `counts` 和 `messages` 打包进一个 Dict 返回，调用方用 `dict get $stats counts` 按名字取，可读性和可扩展性（以后加第三个统计维度不破坏接口）都更好。
- **解析和格式化分离**：`parseLogLine`（解析）、`analyzeLog`（统计）、`formatReport`（展示）三个 `proc` 各管一层，任何一层的实现变化（比如日志格式换了、报表格式要改）都不会影响其他两层。
- **正则表达式集中在一处**：只有 `parseLogLine` 里出现 `regexp`，如果日志格式变化，只需要改一个地方。

## 7. 练习

1. 给 `analyzeLog` 增加一个 `DEBUG` 级别的处理，要求 DEBUG 既不计数也不收集（完全忽略）。
2. 把 `rawLog` 换成真正从文件读取（`open` / `read` / `close`），并通过 `$argv` 接收文件路径。
