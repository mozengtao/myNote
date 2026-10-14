# Layer 1：Widget 与命令系统

> 核心问题：**为什么 Tk 的控件可以通过 Tcl 命令创建、配置、查询和操作？**

---

## 1. Why

在大多数 GUI 框架里（C++ Qt、Java Swing、Python Tkinter 的类封装层……），一个按钮是一个"对象"：你用 `new Button(...)` 创建它，用 `button->setText(...)` 修改它，用对象指针 / 引用在代码里传来传去。GUI 框架和语言本身是两套语法——"创建对象"和"调用普通函数"长得不一样。

Tcl/Tk 没有走这条路。Tcl 语言里只有一种执行单元：**命令（command）**。所有东西都是 `命令名 参数...` 这种形式——`set`、`puts`、`if`、`proc` 都是命令，用户自己写的 `proc` 也是命令。Tk 的设计者面对"怎么把 GUI 控件集成进 Tcl"这个问题时，选择了一个非常省心也非常彻底的答案：

> **不新增任何语法，而是让每个 widget 本身变成一个 Tcl 命令。**

这带来几个直接好处，也是这一层要讲清楚的原因：

- 不需要学习新的面向对象语法（没有 `->`、没有 `.method()`、没有类继承），`button`、`entry`、`listbox` 的使用方式和你写一个普通 Tcl 脚本完全一致。
- 控件的"创建"和控件的"操作"统一成了同一种动作——调用命令。创建是调用一个叫 `button` 的命令，操作是调用一个叫 `.save` 的命令，形式上没有任何区别。
- Tcl 解释器天生自带的命令表（command table）、作用域规则、`catch` 错误处理，全部可以直接复用在 widget 上，不需要 Tk 另外发明一套"对象查找机制"。

如果不理解这个机制，你会把 `.save configure`、`.save cget`、`.save invoke` 这些写法当成需要死记硬背的"魔法语法"；理解了之后，你会发现它们其实是同一件事的自然推论——**widget 路径就是一个命令名，subcommand 就是这个命令的第一个参数**。

---

## 2. Mental Model

一句话 Mental Model：

> **创建 widget = 在 Tcl 解释器的命令表里注册一个新命令；之后对这个 widget 的所有操作，都是在调用这同一个命令，调用时的第一个参数（subcommand）决定具体做什么。**

用图表达：

```text
Before:
    button .save -text "Save" -command {puts "Saved"}

    Tcl Command Table
    +------------------+-------------------------------+
    | command name     | implementation                |
    +------------------+-------------------------------+
    | puts             | (built-in C function)         |
    | set              | (built-in C function)         |
    | button           | (Tk widget-creation function) |
    +------------------+-------------------------------+
                         |
                         | executes "button .save ..."
                         v
After:
    Tcl Command Table
    +------------------+------------------------------+
    | command name     | implementation               |
    +------------------+------------------------------+
    | puts             | (built-in C function)        |
    | set              | (built-in C function)        |
    | button           | (Tk widget-creation function)|
    | .save            | (Tk button-widget dispatcher)|   <-- new!
    +------------------+------------------------------+

    Now these are just normal command calls:
        .save configure -text "Save Changes"
        .save cget -text
        .save invoke
```

记住这一张图，这一层后面所有内容都是在展开它的细节。

---

## 3. Architecture

### 3.1 参与的对象

| 对象 | 角色 |
|---|---|
| Tcl 解释器的命令表（command table） | 全局唯一的"命令名 -> 实现函数"映射表，`proc` 定义的过程和 Tk 的 widget 命令都注册在这里 |
| widget 创建命令（`button`、`frame`、`entry` ...） | 一类特殊命令，调用后会：①在 Tk 内部分配一个 widget 的 C 结构体；②把这个 widget 的路径注册成命令表里的新命令 |
| widget 命令（例如 `.save`） | widget 创建完成后新增的那个命令，接受 `configure`/`cget`/`invoke`/... 等 subcommand |
| widget 树（widget hierarchy） | 由 widget 路径的父子前缀关系构成的一棵树，根是 `.` |
| 窗口系统（X11 / Win32 / Aqua） | Tk 最终会把 widget 树映射成真实的原生窗口层级，由它负责实际绘制和事件产生 |

### 3.2 核心问题逐一解答

下面逐条回答这一层必须讲清楚的 6 个问题。

**（1）`.save` 为什么既是控件路径，又可以用作命令？**

因为 Tk 在实现 `button`、`frame`、`entry` 等"widget 创建命令"时，内部调用了类似 `Tcl_CreateCommand`（Tk 的 C 源码层面的 API）这样的函数：只要你调用一次 `button .save ...`，Tk 的 C 代码就会：

1. 分配一个代表这个按钮的内部数据结构（存放它的所有选项：`-text`、`-command`、颜色、字体……）；
2. 以 `.save` 这个字符串为名字，向当前 Tcl 解释器注册一条新命令，并把这个命令的"实现函数"指向 Tk 内部一个通用的 widget 分派函数（这个函数知道怎么处理 `button` 类型的 widget）。

从这一刻起，`.save` 在 Tcl 解释器眼里，和 `puts`、`set`，或者你自己写的 `proc myProc` 没有本质区别——它们都只是"命令表里的一个条目"。路径字符串和命令名字完全是同一个东西，这不是巧合，是 Tk 故意设计成这样的。

**（2）`configure` 与 `cget` 的职责区别是什么？**

- `cget -选项名`：**只查询一个选项的当前值**，返回值就是那个值本身（一个字符串），适合用在"我只想知道当前文字是什么"这种场景。
- `configure`：
  - 不带任何 `-选项` 参数：返回**所有**选项的完整信息列表，每个选项是一个 5 元素子列表 `{选项名 数据库名 数据库类 默认值 当前值}`，适合用在"我想看看这个控件支持哪些选项、默认值是什么"这种内省（introspection）场景；
  - 带一个 `-选项` 参数：返回**这一个**选项的完整 5 元素信息（不只是当前值）；
  - 带 `-选项 值` 的一对或多对参数：**设置**一个或多个选项，可以一次改好几个，没有返回值。

简单记忆：**`cget` 只读、只认一个选项、只给值；`configure` 既能读（给全信息）也能写（可以一次写多个）。**

**（3）`invoke` 如何触发按钮对应的行为？**

创建按钮时写的 `-command {脚本}`，这段脚本在创建那一刻只是被当作一个字符串原样存进这个按钮的内部选项表里，**不会被执行**。真正执行它的时机有两个：

- 用户真的用鼠标点击了这个按钮：Tk 的事件循环捕获到"鼠标按下 + 释放都发生在这个按钮区域内"的事件序列后，由按钮的内部逻辑取出 `-command` 对应的脚本，在全局作用域下执行一次；
- 代码里主动调用 `.save invoke`：效果和上面完全一样（同样是取出 `-command` 脚本、在全局作用域下执行一次），**唯一的区别是触发方式**——`invoke` 是程序直接调用，不经过真实的鼠标事件，也不需要经过 Tk 的事件循环。这意味着 `invoke` 在没有鼠标、没有显示设备的环境下（比如自动化测试、本教程的 headless 校验）同样可以正常工作。

**（4）Widget 命令与普通 Tcl 过程 `proc` 有什么区别？**

两者都是"命令表里的一个条目"，可以被同样的方式调用，但实现方式和调用习惯不一样：

| | Widget 命令（如 `.save`） | 普通 `proc` |
|---|---|---|
| 实现语言 | C（Tk 的内部实现） | 纯 Tcl 脚本 |
| 子命令分派 | 内置：第一个参数自动被当作 subcommand，由 Tk 内部的 C 代码做 `switch`-like 分派 | 没有内置机制，如果你想要 `myWidget subcmd ...` 这种风格，需要自己在 `proc` 体内手写 `switch -- $subcmd {...}` |
| 使用风格 | "对象风格"——一个命令名对应一个具体实例，状态（选项值）存在这个实例内部 | "函数风格"——一个命令名对应一段逻辑，状态通常要靠参数、全局变量或 `namespace` 变量显式管理 |
| 创建方式 | 由另一条命令（`button`/`frame`/...）间接创建 | 由 `proc` 直接定义 |

一句话：**widget 命令是 Tk 用 C 实现的、自带子命令分派的"对象风格"命令；`proc` 是你自己用纯 Tcl 写的、默认没有子命令概念的命令。** 如果你想让自己写的 `proc` 也有同样的"对象感"，完全可以模仿这个模式——这也是 Layer 5（过程、作用域与组件封装）要讲的内容之一。

**（5）为什么控件通常在创建时获得唯一的路径名？**

路径名要同时承担两个职责，所以必须唯一：

- **结构职责**：路径本身编码了这个控件在 widget 树里的位置（父子层级关系），比如 `.content.input` 一看就知道它的父容器是 `.content`；
- **标识职责**：路径同时是这个控件在 Tcl **全局命令表**里的名字。Tcl 的命令表是一个简单的"名字 -> 实现"映射，如果两个不同的控件注册了相同的名字，后创建的会覆盖/冲突前一个——这就是为什么 Tk 强制要求同一父窗口下的子控件不能重名，而不同父窗口下即使子控件取名相同（例如 `.toolbar.save` 和 `.content.save` 都可以存在）也不会冲突，因为它们的完整路径字符串是不同的。

**（6）父子 Widget 之间的命名关系是什么？**

规则很简单，而且是强制的：**子控件路径必须以父控件路径为前缀，中间用一个 `.` 分隔。** 例如：

- `.toolbar.save` 的父窗口必须是 `.toolbar`；
- `.toolbar` 的父窗口是根窗口 `.`（根窗口比较特殊，它自己就是一个点，所以它的直接子控件路径是 `.` 后面直接拼子控件名，不会出现 `..`）。

如果你试图创建 `.toolbar.save` 但 `.toolbar` 这个 frame 还不存在，Tk 会直接报错——这正是 [第 7 节 Common Failure](#7-common-failure) 要讲的典型错误之一。

### 3.3 Widget 树示意图

```text
.
|-- .toolbar
|   |-- .toolbar.open
|   `-- .toolbar.save
`-- .content
    |-- .content.input
    `-- .content.output
```

### 3.4 路径层级与窗口系统（X11）层级的关系

Tk 的 widget 路径层级不是一个只存在于 Tcl 脚本里的"抽象概念"——它直接对应底层窗口系统（在 Linux 上通常是 X11）里真实的父子窗口关系：

```text
Tk widget path hierarchy          X11 window hierarchy (conceptual)
-----------------------           ----------------------------------
.                        <--->    root toplevel window
|-- .toolbar             <--->      child window (clipped inside root)
|   |-- .toolbar.open    <--->        grandchild window (clipped inside .toolbar)
|   `-- .toolbar.save    <--->        grandchild window (clipped inside .toolbar)
`-- .content             <--->      child window (clipped inside root)
    |-- .content.input   <--->        grandchild window (clipped inside .content)
    `-- .content.output  <--->        grandchild window (clipped inside .content)
```

这种对应关系带来的实际后果（也是初学者容易忽略、但很重要的直觉）：

- 子窗口的可见区域被**裁剪**在父窗口的区域内——如果父窗口比子窗口小，子窗口超出的部分不会显示；
- 父窗口被隐藏（`wm withdraw`）、移动或销毁时，所有子窗口会**跟着一起**隐藏、移动或销毁，不需要你手动遍历子控件逐个处理；
- `destroy .toolbar` 会级联销毁 `.toolbar.open`、`.toolbar.save`，以及它们各自对应的 Tcl 命令——这就是为什么 [Part E 错误 3](#7-common-failure) 里，销毁一个 widget 之后，连它的命令本身都不能再调用了。

可以用 `winfo parent <path>` 命令在运行时查询某个 widget 真实的父窗口，Complete Example 的 Part B 里会演示这一点。

### 3.5 常见控件速览

下面这些控件在后续章节会反复出现，这里只做"是什么、典型用途"的快速介绍，不展开列举所有选项：

| 控件 | 是什么 | 典型用途 |
|---|---|---|
| `label` | 只显示文字或图片，不可编辑、不接受点击交互 | 字段说明、状态提示、标题 |
| `button` | 可点击的按钮，点击时执行 `-command` | 触发一次性动作（保存、提交、打开文件） |
| `entry` | 单行文本输入框 | 用户名、搜索关键字等单行输入 |
| `text` | 多行、可滚动的文本编辑/展示区域，功能比 `entry` 强大得多（支持多种格式标记） | 日志展示、长文本编辑 |
| `frame` | 不显示任何内容的容器控件 | 把一组相关控件分组、辅助布局 |
| `checkbutton` | 开/关两态的方框勾选控件 | 布尔型选项开关（是否启用某功能） |
| `radiobutton` | 一组里"只能选一个"的圆形选项控件 | 多选一（选择日志级别、选择模式） |
| `listbox` | 展示一组可选中条目的列表 | 候选项列表、多项选择 |
| `scale` | 可拖动的滑块，在一个数值范围内取值 | 音量、进度、阈值调节 |
| `scrollbar` | 滚动条，通常和 `listbox`/`text`/`canvas` 配合，不单独使用 | 让内容超出可视区域的控件可以被滚动查看 |

---

## 4. Complete Example

文件：[`widget_commands_demo.tcl`](./widget_commands_demo.tcl)

运行方式：

```bash
# 有图形环境（Linux + X11 / Windows / macOS）：
wish widget_commands_demo.tcl

# headless 环境（没有 $DISPLAY，没有 Xvfb）：
tclsh widget_commands_demo.tcl
# 只能校验到 "package require Tk" 这一行；之后的 widget 创建 /
# configure / cget / invoke 这套语义，在任何真正支持 Tk 的环境下完全相同。
```

> **关于这份示例的重要说明**：本文件从 `package require Tk` 开始就需要真正连接到一个显示环境。在 headless 环境下用 `tclsh` 运行，程序会在这一行准确报错（"连接不上显示"），而不是在更早的位置出现语法错误——这证明这一行之前的所有 Tcl 语法合法。但 `.save configure` / `.save cget` / `.save invoke` 这套"创建 -> 设置 -> 查询 -> 触发"的使用模式，和你在有图形界面的机器上用 `wish` 真正运行时，代码逐字相同、语义完全一致——变化的只是"有没有真实窗口把它画出来"，不是命令本身的行为。

```tcl
#!/usr/bin/env wish
#
# widget_commands_demo.tcl —— Layer 1 Complete Example
#
# 核心问题：为什么 Tk 的控件可以通过 Tcl 命令创建、配置、查询和操作？
#
# 本文件要演示的核心机制只有一句话：
#   每创建一个 widget（例如 button .save ...），Tk 内部就会调用类似
#   Tcl_CreateCommand 的 C API，把这个 widget 的路径（例如 ".save"）
#   注册成当前 Tcl 解释器里的一个新命令。
#   之后 ".save configure ..." / ".save cget ..." / ".save invoke" 这些写法，
#   本质上都是在调用同一个命令 ".save"，第一个参数（subcommand）决定具体行为，
#   这是一种"对象风格"的命令分派，和普通 proc 完全不同（见 README 第 4 点）。
#
# 运行方式：
#   有图形环境：   wish widget_commands_demo.tcl
#   headless 环境： tclsh widget_commands_demo.tcl
#       本文件从第一行可执行语句开始就需要真正的 Tk（因为要创建 widget），
#       所以在没有 $DISPLAY 的机器上，tclsh 只能校验到 "package require Tk"
#       这一行——如果报错点恰好停在这一行（提示连接不上显示），而不是更早
#       出现 "missing close-brace" / "invalid command name" 之类的语法错误，
#       就说明这一行之前（含这一行本身）的所有 Tcl 语法都是合法的。
#
# 重要说明：下面演示的 "创建 -> configure -> cget -> invoke" 这套使用模式，
# 在任何真正支持 Tk 的环境下（Linux + X11、Windows、macOS）语义完全相同。
# 本文件在编写、校验时所在的终端看不到真实窗口，但代码本身与在有图形界面
# 的机器上用 wish 运行时逐字相同——这正是"控件即命令"思想的好处之一：
# 控件的命令接口是跨平台一致的，变化的只是底层窗口系统如何渲染它。
package require Tk

wm title    . "Layer 1 - Widget and Commands Demo"
wm geometry . 480x420

# ===========================================================================
# Part A：最小复现 —— widget 路径即命令
# ===========================================================================
# 这是 Layer 1 要讲的核心模式，先用最少的代码把它完整过一遍。
puts "===== Part A: .save pattern ====="

# 创建：button .save ... 同时完成两件事——
#   1) 在 widget 树里挂一个新的按钮控件，路径是 ".save"（父窗口是根窗口 "."）；
#   2) 在 Tcl 解释器的命令表里注册一个新命令，名字就是这个路径 ".save"。
# -command 的值 {puts "Saved"} 只是被原样存成一段 Tcl 脚本，此刻不会执行。
button .save -text "Save" -command {puts "Saved"}

# cget 的职责：查询"单个"选项的当前值，返回值就是那个值本身（不是列表）。
puts "A1 create -> \[.save cget -text\] = [.save cget -text]"

# configure 不带任何 "-选项" 参数时，返回该选项的完整信息列表：
#   {选项名 数据库名 数据库类 默认值 当前值}
# 这是 configure 与 cget 最大的职责区别：
#   configure 面向"设置 + 查全部元信息"，cget 面向"只要当前值"。
puts "A2 configure -text (query full info) -> [.save configure -text]"

# configure 带 "-选项 新值" 时，用于"设置"一个或多个选项，没有返回值。
# 这里改掉按钮上显示的文字。
.save configure -text "Save Changes"

# 再次用 cget 确认刚才的 configure 生效了。
puts "A3 cget -text (after configure) -> [.save cget -text]"

# invoke 的职责：直接调用 -command 关联的脚本，效果等价于"用户点击了这个按钮"，
# 但它是程序主动触发的，不经过真实的鼠标事件、也不经过 Tk 的事件循环——
# 这意味着 invoke 在 headless 环境下（没有鼠标、没有事件循环）同样可用，
# 常被用来做自动化测试或者给按钮"编程触发"一次点击。
puts "A4 calling .save invoke, expect \"Saved\" on the next line ->"
.save invoke

# ===========================================================================
# Part B：Widget 树与路径命名 —— 父子关系如何体现在路径里
# ===========================================================================
# 对应 README 里的这棵树：
#   .
#   |-- .toolbar
#   |   |-- .toolbar.open
#   |   `-- .toolbar.save
#   `-- .content
#       |-- .content.input
#       `-- .content.output
#
# 命名规则很简单：子控件路径 = 父控件路径 + "." + 子控件名。
# 根窗口 "." 是特例——它自己就是一个点，所以它的直接子控件路径
# 是 "." 直接拼上子控件名（不会出现两个连续的点）。
puts "\n===== Part B: widget tree and path naming ====="

# 两个 frame 作为"分组容器"，父窗口都是根窗口 "."。
# frame 本身不显示任何内容，只是一个用来组织布局、承载子控件的容器。
frame .toolbar -borderwidth 1 -relief raised
frame .content -borderwidth 1 -relief flat

# .toolbar 下的两个按钮：路径前缀必须是 ".toolbar"，否则 Tk 找不到父窗口。
# 注意：这里的 ".toolbar.save" 和 Part A 的 ".save" 是两个完全不同的命令——
# Tcl 命令名是整条路径字符串，哪怕末尾的名字一样，路径不同就是不同的命令。
button .toolbar.open -text "Open" -command {puts "Open clicked"}
button .toolbar.save -text "Save" -command {puts "Save (toolbar) clicked"}

# .content 下的 label / entry / text：同样必须以 ".content" 为前缀。
label .content.label  -text "Name:"
entry .content.input
text  .content.output -width 32 -height 4

# 布局（几何管理器是 Layer 2 的主角，这里只做最少量使用，让控件能显示出来）。
pack .toolbar -side top -fill x
pack .content -side top -fill both -expand 1
pack .toolbar.open .toolbar.save -side left -padx 4 -pady 4
pack .content.label  -side top -anchor w
pack .content.input  -side top -fill x -padx 4
pack .content.output -side top -fill both -expand 1 -padx 4 -pady 4

# winfo parent 是 Tk 提供的"查询真实父窗口"的命令，用来验证：
# 路径里编码的父子关系，和 Tk 内部窗口层级（最终对应 X11 窗口层级）是一致的。
puts "B1 winfo parent .toolbar.save -> [winfo parent .toolbar.save]"
puts "B2 winfo parent .content.output -> [winfo parent .content.output]"
puts "B3 .toolbar.save cget -text -> [.toolbar.save cget -text]"

# ===========================================================================
# Part C：entry / text —— 没有 invoke 时，用"等价操作"验证同一套命令模式
# ===========================================================================
# invoke 是 button / checkbutton / radiobutton 特有的子命令。
# entry、text 这类"内容容器"控件没有 invoke，但同样遵循"控件即命令、
# 子命令决定行为"的规则：用 insert / get / delete 读写内容，原理完全一样。
puts "\n===== Part C: entry / text content operations ====="

# entry：configure 修改外观选项，cget 查询外观选项。
.content.input configure -width 20 -foreground blue
puts "C1 entry cget -width (after configure) -> [.content.input cget -width]"

# insert / get 是 entry 的"内容操作"，可以类比成它的"等价于 invoke"的动作。
.content.input insert 0 "Alice"
puts "C2 entry get (after insert) -> [.content.input get]"

# text 同理：configure/cget 管外观，insert/get 管内容。
.content.output configure -wrap word
puts "C3 text cget -wrap -> [.content.output cget -wrap]"
.content.output insert end "Hello from Layer 1.\n"
puts "C4 text get 1.0 end (after insert) -> [string trim [.content.output get 1.0 end]]"

# ===========================================================================
# Part D：listbox / checkbutton / radiobutton / scale / scrollbar
# ===========================================================================
# 这几种控件各自有专属的子命令，但都遵循同一套"路径即命令、子命令分派行为"
# 的模式——这正是本 Layer 想让你建立的直觉：记住这个模式，而不是死记每个
# 控件的选项表。
puts "\n===== Part D: listbox / checkbutton / radiobutton / scale / scrollbar ====="

# listbox：列表选择控件，典型用途是展示一组可滚动、可选中的候选项。
listbox .content.list -height 4
.content.list insert end "Item A" "Item B" "Item C"
pack .content.list -side top -fill x -padx 4 -pady 4
puts "D1 listbox size -> [.content.list size], item 0 -> [.content.list get 0]"

# checkbutton：开/关两态的控件，典型用途是布尔型选项开关；
# -variable 把它和一个 Tcl 变量双向绑定（这是 Layer 3 的核心话题，这里先感受一下）。
set enableLogging 0
checkbutton .content.logging -text "Enable logging" \
    -variable enableLogging -command {puts "logging flag is now $enableLogging"}
pack .content.logging -side top -anchor w
puts "D2 checkbutton cget -text -> [.content.logging cget -text]"
# invoke 对 checkbutton 的效果：切换一次 -variable 绑定的值，并执行 -command。
.content.logging invoke
puts "D3 after invoke, enableLogging = $enableLogging"

# radiobutton：互斥单选控件，典型用途是"一组选项里选且只能选一个"。
# 同一组 radiobutton 共享同一个 -variable，谁被 invoke/点击，变量就变成谁的 -value。
set logLevel info
radiobutton .content.levelInfo  -text "Info"  -variable logLevel -value info
radiobutton .content.levelDebug -text "Debug" -variable logLevel -value debug
pack .content.levelInfo .content.levelDebug -side left -anchor w
.content.levelDebug invoke
puts "D4 after invoke levelDebug, logLevel = $logLevel"

# scale：滑块控件，典型用途是在一个数值范围内选值（音量、进度、阈值等）。
# scale 没有 invoke，但有 set / get 这对"等价操作"：set 改值，get 读当前值。
scale .content.volume -from 0 -to 100 -orient horizontal -length 150
pack .content.volume -side top -pady 4
.content.volume set 42
puts "D5 scale get (after set 42) -> [.content.volume get]"

# scrollbar：滚动条控件，典型用途是配合 listbox/text/canvas 等"内容可能超出
# 可视区域"的控件一起使用；它自己不存内容，只是和目标控件互相调用对方的
# yview/set 子命令来同步"看到哪一段内容"。
scrollbar .content.outputScroll -orient vertical \
    -command {.content.output yview}
.content.output configure -yscrollcommand {.content.outputScroll set}
pack .content.outputScroll -side right -fill y
puts "D6 scrollbar cget -orient -> [.content.outputScroll cget -orient]"

# ===========================================================================
# Part E：三种常见错误（用 catch 捕获，让演示脚本能继续往下跑）
# ===========================================================================
puts "\n===== Part E: common failures ====="

# 错误 1：路径名拼写错误 —— ".svae" 从来没被创建过，根本不是一个命令。
# Tcl 会报 "invalid command name" 这一类错误，和调用任何不存在的命令一样，
# 这恰好说明：widget 路径在 Tcl 眼里和普通命令名没有任何区别。
if {[catch {.svae invoke} errMsg]} {
    puts "E1 typo (.svae invoke) -> $errMsg"
}

# 错误 2：忘记给控件加父路径前缀 —— 这里引用的父路径 ".content.toolbar"
# 从未被创建过（我们只创建过 .content 和 .toolbar，没有 .content.toolbar），
# 所以创建子控件时 Tk 找不到父窗口，创建失败。
if {[catch {button .content.toolbar.hidden -text "Hidden"} errMsg]} {
    puts "E2 missing parent (.content.toolbar.hidden) -> $errMsg"
}

# 错误 3：对已销毁的 widget 继续调用命令 —— destroy 不仅删除了界面上的控件，
# 也把它对应的那个 Tcl 命令从命令表里一起删掉了，之后再调用就和错误 1 一样，
# 变成"调用一个不存在的命令"。
button .temp -text "Temporary"
destroy .temp
if {[catch {.temp cget -text} errMsg]} {
    puts "E3 destroyed widget (.temp cget) -> $errMsg"
}

# ===========================================================================
# 收尾：这是一个以终端输出为主的演示脚本，不是要你长期停留观察的 GUI 界面，
# 所以这里显式调用 exit，不像 Layer 0 的 hello.tcl 那样停在事件循环里等待
# 鼠标点击（事件循环本身是 Layer 4 才深入讲解的话题，这里先不展开）。
# ===========================================================================
puts "\nDemo complete."
exit 0
```

### 4.1 实际运行效果（在有 Tk / 图形环境的机器上用 `wish` 运行）

```text
===== Part A: .save pattern =====
A1 create -> [.save cget -text] = Save
A2 configure -text (query full info) -> -text text Text {} Save
A3 cget -text (after configure) -> Save Changes
A4 calling .save invoke, expect "Saved" on the next line ->
Saved

===== Part B: widget tree and path naming =====
B1 winfo parent .toolbar.save -> .toolbar
B2 winfo parent .content.output -> .content
B3 .toolbar.save cget -text -> Save

===== Part C: entry / text content operations =====
C1 entry cget -width (after configure) -> 20
C2 entry get (after insert) -> Alice
C3 text cget -wrap -> word
C4 text get 1.0 end (after insert) -> Hello from Layer 1.

===== Part D: listbox / checkbutton / radiobutton / scale / scrollbar =====
D1 listbox size -> 3, item 0 -> Item A
D2 checkbutton cget -text -> Enable logging
logging flag is now 1
D3 after invoke, enableLogging = 1
D4 after invoke levelDebug, logLevel = debug
D5 scale get (after set 42) -> 42
D6 scrollbar cget -orient -> vertical

===== Part E: common failures =====
E1 typo (.svae invoke) -> invalid command name ".svae"
E2 missing parent (.content.toolbar.hidden) -> bad window path name ".content.toolbar"
E3 destroyed widget (.temp cget) -> invalid command name ".temp"

Demo complete.
```

> 上面这份输出是按真实 Tk 的行为整理出来的参考结果（本文件在无显示环境下编写，`A2`/`E2` 这两行的具体文本格式以你实际运行 `wish` 时的 Tcl/Tk 版本输出为准，核心结论——`configure` 返回 5 元素信息列表、父路径缺失会报 "bad window path name" 类错误——是一致的）。

---

## 5. Execution Walkthrough

按真正用 `wish` 执行时的顺序，逐步解释这个文件做了什么：

1. **`package require Tk`**：加载 Tk 扩展，注册 `button`/`frame`/`label`/... 等 widget 创建命令，连接窗口系统，创建根窗口 `.`。
2. **`wm title` / `wm geometry`**：设置根窗口的标题和初始大小，这是在操作窗口管理属性，不创建新 widget。
3. **Part A**：`button .save ...` 创建按钮的同时注册命令 `.save`；随后 `cget` 读一次、`configure` 写一次、再 `cget` 确认变化、最后 `invoke` 主动触发一次 `-command`，完整走完"创建 -> 查 -> 改 -> 再查 -> 触发"的闭环。
4. **Part B**：创建 `.toolbar`、`.content` 两个 frame，以及它们各自的子控件，拼出和 README 里一致的 widget 树；用 `pack` 做最简单的布局；用 `winfo parent` 验证路径里的父子关系和 Tk 实际记录的父窗口是一致的。
5. **Part C**：针对没有 `invoke` 的 `entry`/`text`，演示它们各自的"内容操作"子命令（`insert`/`get`），说明"控件即命令、子命令决定行为"这套规律同样适用于它们。
6. **Part D**：依次创建 `listbox`、`checkbutton`、`radiobutton`、`scale`、`scrollbar`，每个都做一次 `configure`/`cget`，并用各自最贴近"触发动作"的子命令（`invoke`、`set`）演示效果，强化"子命令风格因控件而异，但命令分派机制相同"的认识。
7. **Part E**：用 `catch` 依次触发三类常见错误（拼写错误、父路径缺失、操作已销毁的 widget），把错误信息打印出来而不中断脚本，为 [第 7 节](#7-common-failure) 提供真实可复现的例子。
8. **`exit 0`**：显式退出，不进入 Tk 的事件循环——因为这份脚本的教学重点是"命令调用会产生什么结果"，不是一个需要长期停留的交互式界面。

---

## 6. Data Flow / Control Flow

以 `.save configure -text "Save Changes"` 和 `.save invoke` 这两条调用为例，画出从"用户敲下这行代码/脚本执行到这一行"到"真正发生了什么"的完整链路：

```text
Script line:
    .save configure -text "Save Changes"
          |
          v
+-------------------------------------------+
| Tcl parser                                 |
|   recognizes ".save" as the command name,  |
|   "configure", "-text", "Save Changes"     |
|   as its arguments                         |
+---------------------+-----------------------+
                      |
                      v
+-------------------------------------------+
| Command table lookup                       |
|   ".save" -> Tk button-widget dispatcher   |
+---------------------+-----------------------+
                      |
                      v
+-------------------------------------------+
| Widget dispatcher (C code inside Tk)       |
|   sees subcommand "configure"              |
|   -> updates the internal option table     |
|      of this one button instance           |
|   -> marks the widget as "needs redraw"    |
|      (only matters if it is on screen)     |
+---------------------+-----------------------+
                      |
                      v
            option table updated;
            no screen change yet if the
            event loop has not processed
            a redraw request


Script line:
    .save invoke
          |
          v
   (same parser + command table lookup as above)
          |
          v
+-------------------------------------------+
| Widget dispatcher sees subcommand "invoke" |
|   -> reads the stored -command script      |
|      from this button's option table       |
|   -> evaluates that script at global scope |
+---------------------+-----------------------+
                      |
                      v
            {puts "Saved"} executes right now,
            printing "Saved" to stdout --
            exactly as if the user had
            clicked the button, but triggered
            directly by this one command call
```

关键认识：

- `configure` 修改的是 widget **内部的选项表**，是否"看起来变了"要等几何管理器和事件循环把重绘请求处理掉（这是 Layer 2、Layer 4 的话题），但选项表本身是立刻更新的，`cget` 能立刻读到新值。
- `invoke` 完全不经过"事件队列 -> 事件循环 -> 事件分发"这条路径，它是一条捷径，直接从"命令调用"跳到"执行 `-command` 脚本"，这也是为什么 headless 环境下没有事件循环也能正常触发按钮逻辑。

---

## 7. Common Failure

### 7.1 路径名拼写错误

```tcl
button .save -text "Save" -command {puts "Saved"}
.svae invoke
;# invalid command name ".svae"
```

**现象**：报错 `invalid command name ".svae"`。
**原因**：`.svae` 从来没有被创建过，Tcl 命令表里根本没有这个条目——这和你打错一个普通命令名（比如把 `puts` 打成 `pust`）报的错误是同一类错误，因为 widget 路径对 Tcl 来说就是普通命令名。
**修复**：检查拼写，建议用变量保存控件路径（见 [第 8 节 Engineering Practice](#8-engineering-practice)），避免在多处手敲同一个路径字符串。

### 7.2 忘记给控件加父路径前缀

```tcl
;# 从未创建过 .content.toolbar 这个 frame
button .content.toolbar.hidden -text "Hidden"
;# bad window path name ".content.toolbar"（或类似信息，提示找不到父窗口）
```

**现象**：创建失败，报错提示父窗口不存在。
**原因**：Tk 创建子控件时会校验路径中除最后一段之外的部分（即父路径）必须已经是一个存在的 widget；这里 `.content` 存在，但 `.content.toolbar` 这一级从未创建过，所以整条路径的父窗口查找失败。
**修复**：先创建完整的父级链路（例如先 `frame .content.toolbar`），再创建子控件；或者检查是否手滑多写/少写了一级路径。

### 7.3 对已销毁的 widget 调用命令

```tcl
button .temp -text "Temporary"
destroy .temp
.temp cget -text
;# invalid command name ".temp"
```

**现象**：报错 `invalid command name ".temp"`，即使这行代码在逻辑上"看起来"是在操作一个你刚刚创建过的控件。
**原因**：`destroy` 不只是把控件从界面上拿掉，还会把它对应的 Tcl 命令从命令表里一起删除（连同它所有子控件和子控件的命令，参见 3.4 节）。销毁之后，`.temp` 这个名字在命令表里已经不存在了。
**修复**：在销毁控件之后，清理所有仍然持有这个路径的变量/数据结构；如果不确定某个控件是否已经被销毁，调用前用 `winfo exists .temp` 先判断，或者用 `catch` 包裹调用，捕获可能的 `invalid command name` 错误。

---

## 8. Engineering Practice

真实项目里，围绕"widget 即命令"这个机制，有几条值得养成的习惯：

1. **不要手敲重复的路径字符串**：把常用的控件路径存进变量（例如 `set saveBtn .toolbar.save`），后续用 `$saveBtn configure ...` 调用，既能减少拼写错误，也方便以后调整 widget 树结构时只改一处。
2. **用 `frame` 做命名空间式的分组**：把功能相关的控件放进同一个 `frame` 下（如本例的 `.toolbar`、`.content`），路径天然带上了"属于哪个功能区"的信息，比把所有控件都挂在根窗口下、靠命名约定区分要清晰得多。
3. **创建顺序遵循"先父后子"**：先创建 frame 容器，再创建它下面的子控件，这是 Tk 的硬性要求（父路径必须已存在），顺手把这个顺序变成写代码时的肌肉记忆，能避免大部分"父窗口不存在"的报错。
4. **用 `winfo exists` 防御性检查，而不是裸调用**：在控件可能已经被销毁（比如用户关闭了某个子窗口）的代码路径上，调用前加一层 `if {[winfo exists $path]} {...}` 判断，比等着捕获 `invalid command name` 异常更清晰。
5. **`cget`/`configure` 的返回值不要硬编码格式假设**：`configure` 不带参数返回的 5 元素列表格式在做"动态生成选项面板"这类工具时很有用，但不同控件、不同选项的"默认值"字段格式可能不完全一致，写代码时用 `lindex` 按位置取值，不要假设成固定的字符串拼接格式。
6. **把"创建 widget"和"布局 widget"分成两个清晰的阶段**：创建阶段只管 `-command`、初始文字等逻辑相关的选项，布局（`pack`/`grid`/`place`，Layer 2 的主题）放在之后统一处理，代码读起来更容易对应"这一段是在定义有什么控件，那一段是在定义它们怎么摆放"。

---

## 9. Exercises

1. **基础：补全一个 `.cancel` 按钮**
   在 `widget_commands_demo.tcl` 的 Part A 之后，仿照 `.save` 的写法创建一个 `.cancel` 按钮，`-text` 为 `"Cancel"`，`-command` 打印 `"Cancelled"`；然后依次用 `cget`、`configure`、`invoke` 各操作一次，并用 `puts` 打印每一步的结果。

2. **进阶：写一个"安全调用"小工具**
   写一个 `proc safeInvoke {path}`，功能是：如果 `$path` 这个 widget 存在（用 `winfo exists` 判断）并且支持 `invoke`（用 `catch` 包一层），就调用它的 `invoke`；否则打印一条友好的提示信息（例如 `"widget $path does not exist, skip invoke"`），而不是让整个脚本因为 `invalid command name` 报错而中断。用 Part E 里的三种错误场景分别测试这个 `proc`。

3. **挑战：动态打印整棵 widget 树**
   写一个 `proc dumpWidgetTree {root}`，用 `winfo children $root` 递归遍历从 `$root` 开始的所有子控件，按缩进打印出类似本文档"Widget 树示意图"那样的结构（不要求和示意图格式完全一致，但要能体现父子层级）。用本文件 Part B 创建出的 `.`、`.toolbar`、`.content` 结构测试它。

---

## 10. Mental Model Summary

1. 创建 widget（`button`/`frame`/...）会同时完成两件事：分配内部数据结构、把 widget 路径注册成一个新的 Tcl 命令。
2. widget 路径字符串，就是它在 Tcl 命令表里的名字，二者是同一个东西，不是两个互相映射的概念。
3. `widget路径 subcommand 参数...` 这种调用形式，和调用任何普通 Tcl 命令完全一样，subcommand 决定具体行为。
4. `cget -选项` 只读一个选项的当前值；`configure` 既能一次设置多个选项，也能在不带 `-选项` 时查询全部选项的元信息。
5. `invoke` 直接执行 `-command` 关联的脚本，效果等价于一次点击，但不经过鼠标事件和事件循环。
6. widget 命令是 Tk 用 C 实现、内置子命令分派的"对象风格"命令；`proc` 是纯 Tcl 定义的命令，默认没有子命令分派能力。
7. 子控件路径必须以父控件路径为前缀，根窗口 `.` 是唯一的例外起点。
8. widget 路径层级直接对应底层窗口系统（如 X11）的父子窗口关系：裁剪、显示/隐藏、销毁都是级联的。
9. `destroy` 一个 widget 会级联销毁它的所有子 widget，并把它们对应的 Tcl 命令一并从命令表中删除。
10. 常见错误大多源于"把 widget 路径当成普通字符串随便拼"：拼写错误、父路径不存在、对已销毁 widget 继续调用，三者本质上都是"命令表里查不到这个命令"。

---

## 11. Knowledge Check

1. 如果你先执行 `destroy .toolbar`，再执行 `.toolbar.save cget -text`，会发生什么？为什么会这样，而不是提示"`.toolbar.save` 这个控件本身没被销毁"？
2. `.save configure -text` （只带一个选项名，不带新值）和 `.save configure -text "Hi"` （带选项名和新值）这两条调用，返回值和副作用分别有什么不同？
3. 假设你要给一个自己写的 `proc counter` 增加类似 widget 那样的"`$counterName subcommand ...`"调用风格（例如 `counter1 increment`、`counter1 get`），大致需要在 `proc` 内部做什么，才能模拟出和 Tk widget 命令类似的子命令分派效果？
