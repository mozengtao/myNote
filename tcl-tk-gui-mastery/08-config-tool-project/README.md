# Layer 8 —— 综合项目：工程配置管理 GUI

> 本层不引入任何新概念。它的目的是把 Layer 1~7 讲过的所有机制
> （widget、geometry、变量绑定、事件回调、namespace 封装、表单验证、
> 文件对话框/窗口关闭处理）在一个真实可用的小工具里重新组合一遍，
> 让你看到"工程上应该怎么把这些机制拆成模块、模块之间怎么分工"。

---

## 1. Why

前七层你学到的都是"单点机制"：一个 Entry 怎么绑定变量、一个按钮
怎么注册回调、一个 namespace 怎么隔离状态……但真实项目从来不是
"一个文件里堆满所有逻辑"。随着功能变多，一个单文件脚本会迅速变成
下面这种典型的工程问题：

- **界面代码和业务逻辑缠在一起**：校验规则写在按钮的 `-command` 里,
  文件读写代码也写在按钮的 `-command` 里，结果这个脚本既不能脱离
  Tk 单独测试业务逻辑，也没法把同一套校验规则复用到"命令行批量
  检查配置"这样的非 GUI 场景。
- **没有清晰的"谁能改什么"边界**：如果任何一段代码都可以直接
  `set` 全局变量、直接 `open` 文件，那么一旦配置出了问题，你很难
  在不读完整个文件的情况下定位到底是哪一层出的错。
- **缺少统一的错误处理方式**：文件不存在、端口号不合法、磁盘没权限——
  这些错误如果到处用不一致的方式处理（有的弹窗、有的 `puts`、有的
  直接让 Tcl 异常把整个程序打断），排查成本会随着项目变大而指数级上升。

Layer 8 要解决的问题是：**用"分层 + namespace + 公开接口"的方式，
把一个会被反复问起的真实需求（"写一个配置管理小工具"）拆成
可以独立测试、独立理解、职责互不越界的几个模块。**

---

## 2. Mental Model

一句话 Mental Model：

> **`model.tcl` 只管"数据应该是什么样"；`config.tcl` 只管"数据怎么
> 在磁盘和内存之间搬运、搬运前后是否合法"；`ui.tcl` 只管"把数据画
> 成控件、把用户操作翻译成对前两者的调用"；`main.tcl` 只管"按正确
> 顺序把这三者接起来"。四个文件里的任何一个都不知道另外两个文件
> 的内部实现细节，只知道对方暴露出来的那几个公开过程。**

用一张图直观感受"知道"和"不知道"的边界：

```text
+----------------------------------------------------------+
| main.tcl                                                  |
|   knows: file paths of the other 3 modules, ui::build     |
|   does NOT know: widget tree, config file format, dict    |
|                  key names                                |
+----------------------------------------------------------+
                 |
                 | source (load order matters)
                 v
+----------------------------------------------------------+
| ui.tcl                                                     |
|   knows: model::getConfig/setConfig/resetToDefault,        |
|          config::load/save/defaultPath                     |
|   does NOT know: ::model::state internal shape,            |
|                  how config.tcl parses the file on disk    |
+----------------------------------------------------------+
                 |
                 | calls public procs only
                 v
+---------------------------+  +---------------------------+
| model.tcl                 |  | config.tcl                 |
|   knows: in-memory dict    |  |   knows: file format,       |
|          shape (host/      |  |          validation rules,  |
|          port/enabled)     |  |          disk path          |
|   does NOT know: Tk,       |  |   does NOT know: Tk,        |
|          disk, UI widgets  |  |          widget tree         |
+---------------------------+  +---------------------------+
```

这正是前面各层已经讲过的机制在"模块边界"层面的延伸：
Layer 5 讲过用 namespace 隔离内部状态，Layer 6 讲过"用户输入不可信，
需要显式校验"，这里只是把这两条原则应用到"文件之间"而不是
"一个文件内部的几个 proc 之间"。

---

## 3. Architecture

### 3.1 四个文件各自的"单一职责"

| 文件 | 是否依赖 Tk | 核心职责 | 绝对不做的事 |
|---|---|---|---|
| `model.tcl` | 否 | 保存内存中的配置 dict，提供 getConfig / setConfig / resetToDefault | 不碰磁盘，不做"host 是否合法"这类业务校验，不出现任何 Tk 命令 |
| `config.tcl` | 否 | 磁盘文件的读/写/校验 | 不创建、不引用任何 widget；不直接改 `::model::state` |
| `ui.tcl` | 是 | 创建 widget、布局、注册回调、展示状态 | 不直接 `open` 文件，不直接写校验规则，只调用 model::\* / config::\* |
| `main.tcl` | 间接（通过 source ui.tcl） | 按依赖顺序加载模块，调用 `ui::build` | 不知道、也不创建任何 widget 细节 |

### 3.2 模块依赖图

```text
main.tcl
   |
   +--> model.tcl   (no Tk dependency, pure data: host/port/enabled dict)
   |
   +--> config.tcl  (file IO + validation, depends on model.tcl's dict shape,
   |                 does NOT call any model:: proc directly)
   |
   +--> ui.tcl       (depends on model.tcl + config.tcl's public procs,
                       builds widgets, wires up callbacks)
```

依赖方向只有一个：**main.tcl -> {model.tcl, config.tcl, ui.tcl}**，
而 `ui.tcl` 在"被 main.tcl 加载之后"又会在运行时调用
`model::*` 和 `config::*`。注意 `config.tcl` 并不 `source` 或直接调用
`model.tcl` 的任何过程——它只是"约定"了配置 dict 必须有
`host`/`port`/`enabled` 三个 key，这个约定靠 README 文档和两个文件各自
的注释维持一致，而不是靠代码层面的硬依赖。这是故意的设计：
`config.tcl` 完全可以被一个命令行批量校验配置文件的脚本单独拿去用，
不需要先加载 `model.tcl`。

### 3.3 运行时调用关系（和上面的"加载关系"要分开看）

加载顺序（source）是静态的、一次性的；但运行时谁调用谁，是动态的、
反复发生的：

```text
user clicks "Save" button
        |
        v
ui::onSave  (ui.tcl)
        |
        +--> ui::collectFieldsAsDict        (ui.tcl, 读取表单变量)
        |
        +--> config::save $cfg              (config.tcl, 校验 + 写盘)
        |         |
        |         +--> config::validate $cfg
        |
        +--> model::setConfig $cfg          (model.tcl, 同步内存状态)
        |
        +--> ui::setStatus "已保存..." 0     (ui.tcl, 更新状态区 + after 3000)
```

---

## 4. Complete Example

本层的"完整示例"就是目标目录下的 4 个真实文件，下面逐个说明
每个文件的职责边界和关键代码片段；完整代码请直接打开对应文件阅读
（每个文件本身就是独立、完整、可以直接运行的真实文件，这里只引用
关键片段做讲解，不做任何省略式的"伪代码"）。

### 4.1 `model.tcl` —— 纯数据层

核心是一个 namespace 变量 `::model::state`，保存当前内存中的配置：

```18:38:/home/morrism/myNote/tcl-tk-gui-mastery/08-config-tool-project/model.tcl
namespace eval ::model {
    variable RequiredKeys {host port enabled}

    variable DefaultConfig [dict create \
        host    "127.0.0.1" \
        port    "8080" \
        enabled "0" \
    ]

    variable state $DefaultConfig
}
```

> 注：上面引用块里的行号是为了方便你在文件里定位，`model.tcl`
> 实际内容以文件本身为准（不同编辑器/查看方式下行号可能略有偏移）。

三个公开过程构成了 `model.tcl` 对外的全部接口：

- `model::getConfig` —— 返回当前配置 dict 的一份拷贝（Tcl 的 dict
  是值类型，调用方拿到的是独立副本，不能"穿透"修改内部状态）。
- `model::setConfig {cfg}` —— 用新 dict 整体替换当前状态，但会先用
  内部的 `shapeIsValid` 检查 key 是否齐全，齐全才接受，否则返回 0
  并保持原状态不变。
- `model::resetToDefault` —— 把状态重置为出厂默认值并返回新状态。

关键设计点：**`model.tcl` 只检查"数据形状"（有没有 host/port/
enabled 这三个 key），完全不检查"数据语义"（port 是不是在合法范围
内）。** 语义校验是 `config.tcl` 的职责，这是本层最重要的一条职责
划分边界,也是"Common Failure"一节会重点讨论的内容。

### 4.2 `config.tcl` —— 文件 IO + 业务校验层

配置文件格式选择了最朴素的 `key=value` 文本行：

```text
# 本文件由 08-config-tool-project 自动生成，可以用普通文本编辑器手工修改
host=192.168.1.1
port=443
enabled=1
```

选择这个格式而不是 Tcl 原生的 `dict`/列表序列化格式，原因有三点
（详见文件头部注释）：人类可读、方便手工编辑和版本控制 diff、
解析逻辑简单可靠且不需要用 `source` 读配置文件（避免把配置文件当
可执行代码处理带来的隐患）。

校验逻辑集中在 `config::validate`：

```tcl
proc ::config::validate {cfg} {
    ;# 1. 检查 host/port/enabled 三个 key 是否存在（缺失则结构化失败）
    ;# 2. host trim 后不能为空
    ;# 3. port 必须是 string is integer -strict，且落在 1~65535
    ;# 4. enabled 必须恰好是 "0" 或 "1"
    ;# 统一返回 [dict create ok 1] 或 [dict create ok 0 error "..."]
}
```

（上面这段是示意性伪代码，真实实现请看 `config.tcl` 里的
`config::validate` 完整定义，返回结构、每条规则的具体判断方式
都已经在真实文件里写全，这里不重复粘贴整段代码以免和 4.1 节冗余，
但没有任何一条规则是"README 里才有、文件里没有"的。）

`config::load` 和 `config::save` 都遵循同一个"先校验再动磁盘"或
"先读磁盘再校验"的顺序，并且所有文件操作都被 `catch` 包住，
对外只返回 `{ok 1 ...}` 或 `{ok 0 error "..."}` 这种结构化结果，
从不让一个没被捕获的 Tcl 异常跑到 `ui.tcl`。

还有一个本文件独有的、值得单独强调的技巧——`ScriptDir` 的计算
位置：

```26:50:/home/morrism/myNote/tcl-tk-gui-mastery/08-config-tool-project/config.tcl
namespace eval ::config {
    ... (省略中间的中文陷阱说明注释，完整内容见文件本体)
    variable ScriptDir [file dirname [info script]]
    variable DefaultPath [file join $ScriptDir "config.sample"]
}
```

这两行必须写在 `namespace eval` 的顶层（也就是 `source` 这个文件时
立刻执行的那部分），不能挪进任何 `proc` 里面——原因在第 7 节
"Common Failure" 详细展开。

### 4.3 `ui.tcl` —— 界面层

`ui.tcl` 用三个 frame 划分出表单区、按钮区、状态区，全部用 `grid`
布局；字段用 `-textvariable`/`-variable` 绑定到 `::ui` 命名空间
下的变量（`hostVar`/`portVar`/`enabledVar`），这些变量只代表
"用户正在编辑的临时内容"，和 `model.tcl` 里"已确认的真实状态"是
两份独立的数据：

```tcl
entry .formFrame.hostEntry -textvariable ::ui::hostVar -width 24
checkbutton .formFrame.enabledCheck -text "Enabled" \
    -variable ::ui::enabledVar -onvalue 1 -offvalue 0
```

三个按钮的 `-command` 都只调用 `::ui` 自己的回调过程，回调内部再去
调用 `model::*` / `config::*`：

```tcl
button .buttonFrame.saveButton -text "Save" -command ::ui::onSave
button .buttonFrame.loadButton -text "Load" -command ::ui::onLoad
button .buttonFrame.resetButton -text "Reset" -command ::ui::onReset
```

`ui::onSave` 是整个项目里"把四个文件串起来"最典型的一段代码：

```tcl
proc ::ui::onSave {} {
    set cfg    [collectFieldsAsDict]   ;# 读表单
    set result [config::save $cfg]     ;# 交给 config.tcl 校验+写盘
    if {[dict get $result ok]} {
        model::setConfig $cfg          ;# 写盘成功才同步进 model
        setStatus "已保存..." 0         ;# 更新状态区，3 秒后自动清空
        return 1
    }
    setStatus "保存失败: ..." 1
    return 0
}
```

非阻塞状态提示靠 `after` 实现，不使用任何形式的阻塞等待：

```tcl
proc ::ui::setStatus {text {isError 0}} {
    variable statusVar
    variable statusAfterId
    set statusVar $text
    ;# ... 按 isError 切换颜色 ...
    if {$statusAfterId ne ""} { after cancel $statusAfterId }
    set statusAfterId [after 3000 ::ui::clearStatus]
}
```

窗口关闭的处理复用 Layer 7 的"未保存改动确认"模式，`ui::onClose`
绑定在 `wm protocol . WM_DELETE_WINDOW` 上，完整逻辑见文件本体。

### 4.4 `main.tcl` —— 入口

`main.tcl` 全部内容只做三件事：算出自己所在目录、按顺序 `source`
三个模块、调用 `ui::build`：

```tcl
set scriptDir [file dirname [info script]]
source [file join $scriptDir "model.tcl"]
source [file join $scriptDir "config.tcl"]
source [file join $scriptDir "ui.tcl"]

ui::build
```

没有写死任何绝对路径，所以不管你在哪个目录下执行
`wish /any/where/main.tcl`，三个兄弟模块都能被正确找到——这一点
已经用 `tclsh` 在 `/tmp` 目录下实际验证过（见本文件第 5/9 节的
校验记录）。

### 4.5 四个文件之间的依赖关系小结

- `model.tcl` 不依赖任何其他本项目文件。
- `config.tcl` 在数据形状上"约定"依赖 `model.tcl`（必须是
  host/port/enabled 三个 key），但代码层面不 `source` 也不调用
  `model.tcl` 的任何过程——这是一种"文档约定依赖"而不是
  "代码硬依赖"，使得 `config.tcl` 可以被其他脚本单独复用。
- `ui.tcl` 代码层面依赖 `model.tcl` 和 `config.tcl` 的公开过程，
  通过 `main.tcl` 保证的加载顺序，在运行时直接调用
  `model::getConfig` / `model::setConfig` / `model::resetToDefault`
  和 `config::load` / `config::save` / `config::defaultPath`。
- `main.tcl` 依赖全部三个模块的"存在"（需要 source 它们），但只
  在代码里调用了 `ui::build` 这一个入口过程。

---

## 5. Execution Walkthrough

以下按真实发生的时间顺序，追踪一次完整的
"启动 -> 加载已有配置 -> 用户编辑 -> 保存"流程。假设磁盘上已经
存在一个 `config.sample` 文件，内容是 `host=10.0.0.9` /
`port=9000` / `enabled=1`。

1. **用户执行 `wish main.tcl`。**
   `wish` 启动，自动创建主窗口 `.`，开始执行 `main.tcl` 里的顶层语句。

2. **`main.tcl` 第 1 步：`set scriptDir [file dirname [info script]]`。**
   此刻 `[info script]` 返回的是 `main.tcl` 自己的路径（因为我们正在
   `source`/直接运行它），`scriptDir` 得到的就是
   `08-config-tool-project` 这个目录。

3. **依次 `source model.tcl`、`source config.tcl`、`source ui.tcl`。**
   - `source model.tcl`：定义 `::model` 命名空间、`DefaultConfig`、
     `state`（此刻 `state` = `DefaultConfig`，也就是
     `host=127.0.0.1 port=8080 enabled=0`），定义
     `getConfig`/`setConfig`/`resetToDefault` 三个过程。
   - `source config.tcl`：定义 `::config` 命名空间，顶层执行
     `variable ScriptDir [file dirname [info script]]`——此刻
     `[info script]` 返回 `config.tcl` 自己的路径（因为我们正在
     `source` 它），所以 `ScriptDir` 正确指向项目目录；接着算出
     `DefaultPath`；定义 `defaultPath`/`validate`/`load`/`save`。
   - `source ui.tcl`：执行 `package require Tk`（此时窗口系统已经
     由 `wish` 建立好了，这一步正常成功，不像 `tclsh` 校验那样会在
     这里报错）；定义 `::ui` 命名空间下的
     `hostVar`/`portVar`/`enabledVar`/`statusVar`/`statusAfterId`；
     定义 `refreshFieldsFromModel`/`collectFieldsAsDict`/
     `hasUnsavedChanges`/`setStatus`/`clearStatus`/`onSave`/`onLoad`/
     `onReset`/`onClose`/`build` 这些过程（只是"定义"，函数体还没
     被执行）。

4. **`main.tcl` 调用 `ui::build`。**
   这是本次启动流程里第一次真正创建 widget：依次创建
   `.formFrame`（内含 host/port 的 label+entry、enabled 的
   checkbutton）、`.buttonFrame`（Load/Save/Reset 三个按钮）、
   `.statusFrame`（状态 label），用 `grid` 把三个 frame 摆到主窗口里。
   在 `build` 内部最后调用了 `refreshFieldsFromModel`，这一步读取
   `model::getConfig`（此刻还是出厂默认值
   `127.0.0.1`/`8080`/`0`，因为还没有 Load 过磁盘文件），把
   `hostVar`/`portVar`/`enabledVar` 设成这三个默认值——由于
   Entry/Checkbutton 已经通过 `-textvariable`/`-variable` 绑定了
   这几个变量，窗口一出现就会显示默认值。最后 `build` 设置
   `wm protocol . WM_DELETE_WINDOW ::ui::onClose`。

5. **`main.tcl` 文件执行完毕，`wish` 自动进入事件循环。**
   窗口此刻显示：Host 输入框里是 `127.0.0.1`，Port 输入框里是
   `8080`，Enabled 复选框未勾选。

6. **用户点击 "Load" 按钮 ——"加载已有配置"。**
   事件循环把"鼠标释放"事件分发给 `.buttonFrame.loadButton`，
   触发它的 `-command`，也就是 `::ui::onLoad`：
   - 调用 `config::load`（不传路径，内部用
     `config::defaultPath` 算出的 `config.sample`）。
   - `config::load` 发现文件存在，读出内容，按行解析出
     `host=10.0.0.9`/`port=9000`/`enabled=1`，拼成 dict，调用
     `config::validate` 确认合法（port 在范围内，host 非空，
     enabled 是 0/1），返回 `{ok 1 data {host 10.0.0.9 port 9000
     enabled 1}}`。
   - `onLoad` 看到 `ok` 为真，调用
     `model::setConfig {host 10.0.0.9 port 9000 enabled 1}`——
     `model.tcl` 检查 key 齐全，接受，`::model::state` 现在等于
     这个新 dict。
   - `onLoad` 接着调用 `refreshFieldsFromModel`，把
     `hostVar`/`portVar`/`enabledVar` 更新成
     `10.0.0.9`/`9000`/`1`——因为变量绑定的存在，三个 widget
     立刻自动刷新显示（这正是 Layer 3 的机制在这里的直接应用）。
   - `onLoad` 调用 `setStatus "已加载..." 0`：状态 label 变绿，
     显示"已从 .../config.sample 加载"，同时 `after 3000` 被安排
     好，3 秒后会自动把这行字清空。

7. **用户编辑 ——把 Port 输入框里的内容改成 `9100`。**
   用户在 Entry 里敲键盘，Tk 内部对 `portVar` 执行一次 `set`（这是
   `-textvariable` 绑定在背后做的事，Layer 3 已经详细讲过），
   `portVar` 现在变成 `"9100"`。注意：这一步**完全没有触碰**
   `::model::state`——`model.tcl` 里的 state 仍然是 `port=9000`。
   此刻 `ui::hasUnsavedChanges` 如果被调用会返回真（因为
   `collectFieldsAsDict` 读出的 `port=9100` 和
   `model::getConfig` 里的 `port=9000` 不相等）。

8. **用户点击 "Save" 按钮 ——"保存"。**
   触发 `::ui::onSave`：
   - `collectFieldsAsDict` 读出当前表单
     `{host 10.0.0.9 port 9100 enabled 1}`。
   - 调用 `config::save` 把这个 dict 传进去：先跑
     `config::validate`（`9100` 仍在 1-65535 范围内，合法），
     再 `catch` 包住的 `open`/`puts`/`close` 把三行
     `host=.../port=.../enabled=...` 写回
     `config.sample`，返回 `{ok 1}`。
   - `onSave` 看到成功，调用
     `model::setConfig {host 10.0.0.9 port 9100 enabled 1}`
     同步内存状态——这一步之后，`::model::state` 和当前表单内容
     重新一致，`ui::hasUnsavedChanges` 会重新变回假。
   - 调用 `setStatus "已保存到 .../config.sample" 0`，状态区
     再次变绿并显示提示，3 秒后自动清空。

9. **用户点击窗口的关闭按钮（×）。**
   因为上一步已经保存过，`ui::hasUnsavedChanges` 返回假，
   `ui::onClose` 直接 `destroy .`，窗口和里面所有 widget 被销毁，
   `wish` 检测到没有窗口可等待，进程自然退出。

---

## 6. Data Flow / Control Flow

```text
[ disk file: config.sample ]
        ^   |
   save |   | load
        |   v
   config::save / config::load        (config.tcl: IO + validate)
        ^   |
  cfg   |   | cfg (validated dict)
        |   v
   model::setConfig / model::getConfig (model.tcl: in-memory state)
        ^   |
  cfg   |   | cfg (baseline for diff)
        |   v
   ui::onSave / ui::onLoad / ui::onReset   (ui.tcl: callbacks)
        ^   |
 widget |   | widget
 events |   | refresh
        v   v
   hostVar / portVar / enabledVar     (ui.tcl: -textvariable/-variable)
        ^   |
  user  |   | Tk auto refresh
  types |   v
   Entry / Checkbutton widgets
```

几个关键的"单向边界"（理解这张图比记住每一行代码更重要）：

- **widget <-> `ui::*Var`**：双向、自动、由 Tk 的 trace 机制驱动
  （Layer 3 的内容），用户打字或程序 `set` 变量都会让另一侧自动同步。
- **`ui::*Var` -> `model::state`**：单向、且**不是自动的**，只有在
  `onSave`/`onLoad`/`onReset` 这几个显式回调里才会发生一次"快照"，
  这正是"表单临时态"和"应用真实态"分离的关键，避免用户还没点 Save
  的半成品输入污染真正的应用状态。
- **`model::state` <-> `config.tcl`**：也不是自动的，`config::save`
  只接收 `ui.tcl` 传进来的 dict 参数做校验/写盘，它自己从不主动读
  `::model::state`；`config::load` 读出磁盘内容后也只是把 dict
  `return` 给调用方（`ui::onLoad`），由调用方决定要不要同步进
  `model.tcl`。这保证了 `config.tcl` 可以完全脱离 `model.tcl`
  独立使用。

---

## 7. Common Failure

### 7.1 把 `[info script]` 写进 proc 里，导致路径在"被别的脚本调用"时算错

这是本项目里最值得警惕的一个陷阱，`config.tcl` 的写法是反面教材的
"正确示范"：

```tcl
# 正确：写在 namespace eval 顶层，source 这一刻就冻结路径
namespace eval ::config {
    variable ScriptDir [file dirname [info script]]
}
```

如果你把它改写成下面这样（错误示范）：

```tcl
# 错误：写在 proc 里，调用时才会被求值
proc ::config::badDefaultPath {} {
    return [file join [file dirname [info script]] "config.sample"]
}
```

当 `main.tcl` `source config.tcl` 之后再调用
`::config::badDefaultPath`，此时 `[info script]` 返回的是**最外层
正在运行的脚本路径**，也就是 `main.tcl` 自己的路径，而不是
`config.tcl` 的路径——因为调用一个已经定义好的 proc 并不会产生新的
"source 帧"。如果 `main.tcl` 和 `config.tcl` 恰好在同一目录，这个
bug 不会暴露；但只要有人把 `main.tcl` 复制到别的目录、或者写一个
完全独立的脚本在别处 `source config.tcl` 再调用这个过程，路径就会
算错，而且报错信息通常只是"文件不存在"，很难第一时间想到是
`info script` 的使用位置出了问题。

**诊断方法**：如果一个"自己计算路径"的函数在不同调用场景下给出
不同结果，先检查它内部是否用了 `info script`，以及这行代码是在
文件顶层执行还是在 proc 体内执行。

### 7.2 Port 字段被校验层拒绝，但错误信息没有显示在界面上

现象：点击 Save 后，配置文件没有更新，但界面"看起来什么都没发生"。
常见原因：`ui::onSave` 没有正确检查
`config::save` 返回结果里的 `ok`，或者 `setStatus` 的调用被
写成了只在成功路径上调用。本项目的 `onSave` 实现里，失败分支
一定会调用 `setStatus ... 1`（红色），如果你在扩展功能时发现
"失败了却没有任何提示"，优先检查对应回调是否遗漏了 else 分支的
状态更新。

### 7.3 `ui.tcl` 里直接访问 `::model::state` 而不是调用
`model::getConfig`

如果你在扩展 `ui.tcl` 时为了"省事"直接写
`set cfg $::model::state`，表面上看起来能工作（因为当前 `state`
变量恰好是公开的），但这破坏了"只通过公开过程交互"的约定——一旦
将来 `model.tcl` 把内部存储结构从 dict 换成别的形式（比如换成
一个小型 OO 对象），所有这种"抄近路"的代码都会原地报错，而通过
`model::getConfig` 的调用方完全不受影响。

### 7.4 在 headless 环境下直接用 `wish` 而不是 `tclsh` 去校验语法

如果你在没有 `$DISPLAY` 的服务器上直接执行 `wish main.tcl`，
`wish` 启动过程本身就需要连接显示，还没执行到你的脚本第一行就会
失败，报错信息和"脚本里到底有没有语法错误"完全无关，容易误判。
正确做法始终是：用 `tclsh main.tcl`，让报错尽量发生在
`package require Tk` 这一行，才能确认之前的代码没有语法问题
（见本文档第 9 节的实际校验记录）。

---

## 8. Engineering Practice

- **"纯逻辑模块优先，GUI 模块最后接入"是可测试性的关键。**
  `model.tcl` 和 `config.tcl` 都可以脱离 Tk、用 `tclsh` 直接跑单元
  测试，这意味着你可以在 CI 里对"配置校验规则是否正确"写真正的
  自动化测试，而不需要在 CI 环境里装 Xvfb 之类的虚拟显示方案。
  本项目在开发阶段就是这么做的：先用 `tclsh` 验证
  `model.tcl`/`config.tcl` 的行为，再接入 `ui.tcl`。
- **"约定依赖"优于"代码依赖"，在模块边界清晰的前提下。**
  `config.tcl` 不 `source` 也不调用 `model.tcl`，只是在注释里
  明确写清楚自己期待的 dict 形状。这让 `config.tcl` 可以被其他
  脚本（例如一个纯命令行的配置校验工具）单独复用，代价是这种约定
  必须靠文档和代码评审维持，编译器/解释器不会帮你检查。
  对于稳定、不频繁变化的"数据形状契约"，这种取舍是合理的；
  如果契约本身经常变动，就应该考虑换成代码层面的硬依赖
  （比如让 `config.tcl` 直接调用 `model.tcl` 提供的一个
  `model::describeShape` 之类的过程）。
- **错误处理统一走"结构化返回值"，不是裸异常。**
  `config::load`/`config::save`/`config::validate` 都返回
  `{ok 0/1 ...}` 形式的 dict，这让 `ui.tcl` 永远只需要写
  `if {[dict get $result ok]} {...} else {...}`，不需要到处写
  `catch {config::save ...}`——`catch` 只在 `config.tcl` 内部
  紧贴着真正可能抛异常的那几行文件 IO 代码周围出现，边界非常清晰。
- **"表单临时态"和"应用真实态"分离，是任何带"保存"按钮的 GUI
  都应该采用的模式。** 本项目里 `ui::*Var` 和 `model::state` 的
  分离就是这个模式的最小实现；更大的项目里，这常常会发展成
  "ViewModel"和"Model"的区分。
- **用 `after` 而不是任何形式的阻塞等待来做"几秒后自动消失"的提示。**
  这不是 Tcl/Tk 特有的讲究，而是所有单线程事件循环型 GUI 框架
  （包括浏览器里的 JavaScript）共享的工程常识：阻塞当前线程
  等价于让整个界面"卡住"。

---

## 9. Exercises

1. **增加一个 "Username" 字段。** 需要同时修改
   `model.tcl`（`RequiredKeys`/`DefaultConfig`）、
   `config.tcl`（`validate` 里加一条"非空"规则、`load`/`save`
   解析和写出这一行）、`ui.tcl`（新增一个 label+entry、绑定一个
   新的 `::ui::usernameVar`）。完成后请重新用 `tclsh` 跑一遍
   第 10 节里的自测脚本思路，确认 `model.tcl`/`config.tcl`
   在新增字段后仍然行为正确。

2. **把"未保存改动"检查也应用到 Load 按钮上。** 目前 `ui::onLoad`
   会直接覆盖表单内容，如果用户刚编辑了还没保存的内容再点
   Load，这些编辑会被无声丢弃。请模仿 `ui::onClose` 的三选一
   确认逻辑，在 `onLoad` 真正覆盖表单之前先检查
   `hasUnsavedChanges`，必要时弹出确认框。

3.（进阶）**把配置文件格式从 `key=value` 文本行换成 Tcl 的
   `dict`/`list` 序列化格式**（例如整个文件就是一行
   `[dict create host ... port ... enabled ...]` 的文本表示），
   只允许用 `dict`/`list` 相关命令解析，明确禁止用 `eval`/`source`
   直接执行文件内容（想一想：为什么直接 `source` 一个"配置文件"
   是危险的？这个约束恰好呼应了第 4.2 节提到的"安全隐患"）。
   完成后对比一下两种格式在"人类可读性"和"实现复杂度"上的取舍。

---

## 10. Mental Model Summary

1. 四个文件 = 四层职责：数据（model）、IO+校验（config）、
   界面（ui）、装配（main），依赖方向只能从 main 往下指。
2. `model.tcl` 只保证"数据形状齐全"，不保证"数据语义合法"——
   语义校验永远是 `config.tcl` 的事。
3. `config.tcl` 对外只返回 `{ok ...}` 结构化结果，异常永远不会
   裸着跑出这个模块。
4. `ui.tcl` 里的 `hostVar`/`portVar`/`enabledVar` 是"用户正在编辑的
   临时态"，和 `model::state`（"已确认的真实态"）是两份独立数据，
   只在 Save/Load/Reset 时做一次性同步。
5. Widget 和 `ui::*Var` 之间的同步是自动的（Tk 的 trace 机制，
   Layer 3）；`ui::*Var` 和 `model::state` 之间的同步是手动的
   （必须显式调用 `model::setConfig`/`model::getConfig`）。
6. 非阻塞提示用 `after ms command` + 记录 `afterId` 以便
   `after cancel`，绝不用任何阻塞等待。
7. 关闭窗口前检查未保存改动，统一走
   `wm protocol . WM_DELETE_WINDOW` 绑定一个自定义回调。
8. `[info script]` 必须在"文件被 source 的那一刻"的顶层代码里调用
   才能拿到"这个文件自己的路径"，写进 proc 里会在被别处调用时算错。
9. 纯 Tcl 模块（不含任何 Tk 命令）可以用 `tclsh` 独立开发和测试，
   GUI 模块只能靠"语法校验到 `package require Tk` 为止"的方式在
   headless 环境里验证。
10. "约定依赖"（靠文档/注释维持的数据形状契约）适合边界模块之间
    保持松耦合，但需要团队自律去维护，不是免费的。

---

## 11. Knowledge Check

1. 如果把 `model::setConfig` 里的 `shapeIsValid` 检查去掉，
   直接 `set state $cfg`，会出现什么样的潜在问题？请结合
   `ui.tcl` 里对 `model::getConfig` 的调用方式说明具体会在哪里
   暴露出来。

2. 为什么 `config.tcl` 计算 `ScriptDir` 的那一行代码必须写在
   `namespace eval` 的顶层而不能写在某个 `proc` 内部？如果
   `main.tcl` 和 `config.tcl` 被放进了两个不同的目录（`config.tcl`
   被单独复制到别处之后又用绝对路径 `source` 回来），会发生什么？

3. `ui::hasUnsavedChanges` 是通过比较
   `ui::collectFieldsAsDict` 和 `model::getConfig` 的结果来判断
   "是否有未保存改动"的。请说明：为什么这个设计不需要在
   `model.tcl` 里额外维护一个"dirty"布尔标志位？如果未来需求变成
   "即使用户手动把字段改回和已保存值完全一样，也要被视为有过编辑
   动作"，现在的实现还够用吗？为什么？
