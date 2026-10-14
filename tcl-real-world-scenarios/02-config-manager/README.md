# 场景 2：键值配置文件管理器（INI-like Config）

> 运行：`tclsh config_manager.tcl`
> 运行后会在同目录生成 `app.conf`（示例原始配置）和 `app.saved.conf`（修改后另存的配置），这两个文件只是运行产物，可以随时删除。

## 1. 真实背景

几乎所有服务/工具都需要一份可读写的配置文件，常见形式是带 section 的 `key=value`：

```ini
[server]
host=127.0.0.1
port=8080
```

本例实现一个最小但完整的配置管理模块：**加载文件 → 按 section/key 读写 → 修改后写回文件**，并且用 `namespace` 把它包装成一个可以被其他脚本复用的模块。

## 2. 涉及的 Tcl 概念

对照 [tcl_mental_model.md](../../tcl_mental_model.md)：

| 概念 | 对应章节 | 在本例中的作用 |
|---|---|---|
| Nested Dict | 第 15 节 | `section -> key -> value` 两层结构保存整份配置 |
| Array vs Dict | 第 16-17 节 | 本例选 Dict 而非 Array（见下方"Dict vs Array"小节） |
| `split` / `regexp` | 第 23、26 节 | 解析 `[section]` 和 `key=value` 两种行格式 |
| `switch` | 第 27 节 | 按行的类型（section / kv / blank）分流处理 |
| `proc` 默认参数 | 第 34.2 节 | `config::get section key {default ""}` |
| `namespace` + `variable` | 第 38 节 | 用模块级 `variable data` 保存整份配置状态 |

## 3. Mental Model

```
app.conf (file, String content)
    |
    | gets（逐行读取）
    v
line (String)
    |
    | parseLine: regexp 判断类型
    v
    +-- blank    -> 忽略
    +-- section  -> currentSection = name
    +-- kv       -> dict set data currentSection key value
                          |
                          v
data (Nested Dict)
    +-- server    -> dict(host, port)
    +-- database  -> dict(host, port, user)
    |
    | config::get / config::put
    v
业务代码读写配置
    |
    | config::dump + save
    v
app.saved.conf (file)
```

## 4. 完整代码

见 [config_manager.tcl](config_manager.tcl)。对外接口（`namespace export`）：

```tcl
config::load path              ;# 从文件加载，覆盖当前配置
config::get section key ?default?
config::put section key value
config::sectionExists section
config::dump                   ;# 当前配置 -> 文本
config::save path              ;# dump 结果写入文件
```

## 5. 执行讲解

1. `config::parseLine` 用两个 `regexp` 分别识别 `[section]` 和 `key=value` 两种行；都不匹配、或者是空行/`#` 开头的注释，归为 `blank`，直接忽略。
2. `config::load` 用 `open path r` + `while {[gets $fp line] >= 0}` 逐行读取（这是 mental model 之外的最小必要补充：文件 I/O）。遇到 `[section]` 就把 `currentSection` 切换过去，并用 `dict exists`/`dict set` 保证该 section 在 `data` 中存在；遇到 `key=value` 就 `dict set data $currentSection $key $value` 写入**当前** section 下。
3. `config::get section key {default ""}` 利用 `proc` 的默认参数：调用时不传第三个参数，`default` 就是 `""`；示例里特意传了 `300` 来演示"配置项不存在时返回调用方指定的默认值"这个真实场景里非常常见的模式。
4. `config::put` 没有叫 `config::set`——详见下面"工程实践要点"里的踩坑记录，这是本例特意演示的一个真实陷阱。
5. `config::dump` 用 `dict for {section kv} $data` 遍历外层，再用 `dict for {key value} $kv` 遍历内层，把 Nested Dict 重新拼回文本；`config::save` 只是把 `dump` 的结果写入文件。

## 6. 工程实践要点

### Dict vs Array：为什么选 Dict

mental model 第 16-17 节提到：

> Dict → 一个结构化 Value；Array → 一组相关 Variables

本例的配置数据需要：**作为一个整体被 `config::load` 整体替换**（`set data [dict create]` 清空重来）、**作为参数在 `dump`/`save` 之间传递**、**支持任意深度嵌套（section -> key）**。这些都是 Dict 的强项。如果用 Array，`array set`/`array get` 虽然也能表达扁平的 key-value，但天然不支持嵌套（`user(server,host)` 这种只是约定俗成的 key 命名技巧，本质还是一层），也不能整体作为一个值传给函数或存进另一个数据结构——这正是本例选 Dict 而非 Array 的原因。

### 真实踩坑：namespace 内部 proc 不要和内置命令同名

本例最初把"写入配置项"的过程命名为 `config::set`，结果 **`config::load` 内部所有 `set data ...}`、`set currentSection ...` 全部报错**。原因：

```
proc 的"当前命名空间"由它被定义的位置决定，而不是调用者的位置。
config::load 是在 ::config 命名空间下定义的，所以它内部所有不带
:: 前缀的命令名，都会优先在 ::config 命名空间里查找。
一旦 ::config 下面也有一个名叫 set 的 proc（即 config::set），
它就会在命名空间内部"屏蔽"掉内置的 set 命令。
```

这是 `namespace` 实践中一个很容易踩、但官方文档不会特别提醒你的坑，因此本例把这个 proc 改名为 `config::put`，并保留这段注释作为记录。**通用经验：避免在自定义命名空间里定义和 Tcl 内置命令（`set`/`list`/`string`/...）同名的 proc。**

## 7. 练习

1. 给 `config::load` 增加重复 key 检测：同一个 section 下出现两次相同 key 时打印警告。
2. 实现 `config::remove section ?key?`：不传 `key` 时删除整个 section，传 `key` 时只删除该项（提示：`dict unset`）。
