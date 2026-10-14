# Layer 3：Tcl 变量与 GUI 状态绑定

> 核心问题：**当用户修改输入框或勾选复选框时，应用程序如何获得最新值？**
> 这是整个教程的学习重点——Layer 4（事件驱动）建立在本层之上，之后所有的表单、对话框、综合项目都在反复使用本层的机制。请放慢速度，逐节读完。

---

## 1. Why

先把问题问得更具体一点：一个 GUI 程序里，"用户输入的用户名"这份数据，**到底存在哪里**？

如果你来自 C / C++ / Qt / Win32 背景，直觉上的答案可能是："存在 Entry 控件对象内部，要用的时候调用 `entry->text()` 或者 `GetWindowText()` 去问控件要。" 这是一种**命令式查询**模型：数据的"真相"保管在 widget 手里，程序逻辑需要主动、按需地去读取。

Tcl/Tk 选择了一个完全不同的模型：

- 数据的"真相"保管在 **Tcl 解释器的变量表**里，是一个独立于任何 widget 的普通变量（`set username "guest"` 这种最朴素的东西）。
- Widget（Entry、Label、Checkbutton……）只是**订阅**了这个变量——通过 `-textvariable` 或 `-variable` 选项，widget 向 Tcl 核心的 **trace** 机制注册"这个变量一旦被写入，请通知我刷新显示"；同时 widget 自己在响应用户输入时，也会反过来对这个变量执行 `set`。
- 结果就是：**变量永远是最新的**，程序逻辑不需要"去问"任何 widget，只需要读一个普通变量；多个 widget 想展示同一份数据时，只要绑定同一个变量名即可，不需要手写任何同步代码。

这个设计要解决的根本问题是：**当同一份数据需要在多个地方（输入框、预览区、校验逻辑、保存逻辑）保持一致时，"谁是真相源、谁该主动同步谁"这件事不应该由每个开发者手写一遍，而应该是语言/工具包层面提供的基础设施。** Tcl 把这件事下沉到了变量系统本身，这也是为什么本层叫"变量与 GUI 状态绑定"，而不是"Entry 控件用法"——绑定机制比具体哪个 widget 更重要。

但这个模型也有边界：它只解决"**展示同步**"，不解决"**业务逻辑执行**"。把两个 widget 绑定到同一个变量，只会让它们的显示保持一致，不会凭空触发任何校验、保存、网络请求之类的副作用——这一点会在第 6 节反复强调，因为它是最容易被误解的地方。

---

## 2. Mental Model

一句话 Mental Model：

> **Tcl 变量是"共享状态"；Widget 是"订阅者"；trace 是连接两者的"胶水"——而这根胶水对程序逻辑同样开放，不是 Tk 的私有机制。**

```text
+-------------------------------------------------------------+
| Tcl Interpreter Variable Table                              |
|                                                             |
|   username = "guest"   <-- 普通的 名字->值 映射，与 GUI 无关    |
+-------------------------------------------------------------+
         ^                                   |
         | write (set / incr / user typing)  | write-trace fires
         |                                   v
+------------------+                +------------------------+
| Entry Widget     |                | Every other subscriber |
| (-textvariable)  |<---------------| re-reads the variable  |
+------------------+   trace fires  | and refreshes itself   |
                                    +------------------------+
```

关键认知：这张图里没有任何一个箭头是"Entry 专属"的。把 `Entry Widget` 换成 `Label`、`Checkbutton`、甚至你自己写的一个 `proc`，图完全成立——因为 trace 机制本身不知道、也不关心订阅者是 widget 还是普通代码。本层的三个示例分别对应这张图的三种具体形态：

| 示例文件 | 对应的"订阅者" | 绑定选项 |
|---|---|---|
| `entry_binding.tcl` | Entry / Label（文本内容） | `-textvariable` |
| `checkbutton_state.tcl` | Checkbutton（两态值） | `-variable` (+ `-onvalue`/`-offvalue`) |
| `variable_trace.tcl` | 你自己写的 proc（任意逻辑） | `trace add variable ... write ...` |

---

## 3. Architecture

### 3.1 涉及的机制一览

| 机制 | 角色 |
|---|---|
| `set varName value` | 最基础的"写变量"操作，一切绑定机制最终都会落到它（或等价的 C 级写入）上 |
| `-textvariable` | Entry / Label 等 widget 的配置选项，把 widget 的"文本内容"和一个变量双向绑定 |
| `-variable` | Checkbutton / Radiobutton 等 widget 的配置选项，把 widget 的"两态/多态选中状态"和一个变量绑定 |
| `global varName` | 在 `proc` 内部声明"这个名字指向全局变量"，否则 `set` 只会创建/修改一个局部变量 |
| `upvar` | 更通用的"变量引用"机制：把调用栈中**任意层**（包括全局，`#0` 层）的变量关联到当前作用域的一个本地名字，常用于变量名本身是**动态传入**的场景（比如 trace 回调） |
| `trace add variable name write callback` | 手动在任意变量上挂一个"写入时自动调用"的回调；`-textvariable`/`-variable` 在 Tk 内部就是靠同样的机制实现的 |

`-textvariable` 和 `-variable` 不是两套独立的技术，而是"trace 机制 + widget 自己的刷新/写入逻辑"的两种具体包装：前者包装成"显示/编辑任意文本"，后者包装成"显示/切换一个两态值"。理解了这一点，你会发现 `trace add variable` 不是什么高级技巧，而是 Tk 自己天天在用的基础设施——只是平时被 `-textvariable` 这种语法糖藏起来了。

### 3.2 `global` 与 `upvar` 的关系（容易混淆，必须讲清楚）

`proc` 的 body 默认运行在**自己的局部作用域**里，和全局作用域是隔离的。这是 Tcl 的作用域陷阱之一：

```tcl
set counter 0
proc badIncr {} {
    # 没有 global 声明：这里的 set 创建的是一个全新的、
    # 生命周期仅限于本次调用的局部变量 counter，
    # 和外面的全局 counter 完全是两个不同的"格子"。
    set counter [expr {$counter + 1}]
}
```

`global counter` 的作用是："把当前作用域里名字 `counter` 的查找，重定向到全局作用域的 `counter`"。它是下面这行的语法糖：

```tcl
upvar #0 counter counter
```

`upvar #0 varName localName` 的通用形式是："把全局作用域（`#0` 层）里名字为 `varName`（运行时字符串，可以是变量）的变量，关联到当前作用域的本地名字 `localName`"。

两者的本质区别：**`global` 要求你在写代码的时候就知道变量的确切名字；`upvar #0 $varName ...` 允许变量名在运行时才确定。** 这正是 `onCounterWrite`（见 `variable_trace.tcl`）必须用 `upvar #0 $varName newVal` 而不能写死 `global counter` 的原因——同一个回调函数理论上可以被挂到任意变量上（`trace add variable someOtherVar write onCounterWrite` 完全合法），回调内部只能通过运行时传入的 `varName` 参数去动态解析，`global` 在这种场景下无能为力。反过来，`checkbutton_state.tcl` 里的 `onAgreeChanged` 从设计上就只服务于 `agreeTerms` 这一个变量，直接写 `global agreeTerms` 更直白，没必要上 `upvar`。**选择哪种写法取决于回调是"专用"还是"通用"，不是谁更高级的问题。**

### 3.3 六个核心问题逐一解答

#### 问题 1：Tcl 变量是否属于 Widget？

**不属于。** Tcl 变量存活在 Tcl 解释器的变量表（variable table）里，这张表的生命周期和解释器本身绑定，和任何 widget 对象毫无关系。`set username "guest"` 这行代码执行时，Tk 甚至可能还没有被加载（看 `variable_trace.tcl` 的 Part 1，`counter` 变量和它的 trace 全程没有涉及任何 widget）。

反过来验证这一点最直接的方式是看生命周期的独立性：

- **widget 被销毁，变量依然存在。** `destroy .usernameEntry` 之后，`username` 变量不会被清除，`puts $username` 依然能正常输出——因为变量从来不是 widget 的附属品，widget 只是曾经"订阅"过它。
- **变量被 `unset`，widget 依然存在（但显示会出问题）。** 如果对一个被 `-textvariable` 绑定的变量执行 `unset username`，widget 对象本身不会消失，`.usernameEntry` 这个命令依然可以被调用；但因为 Tk 内部在变量上注册了 unset-trace，它检测到"我依赖的变量没了"，会立刻用当前 widget 的显示内容把这个变量重新 `set` 出来，以保证不出现"引用一个不存在的变量"这种更严重的错误（这一点在第 7 节还会结合具体现象再提一次）。

所以 widget 与变量是两个生命周期独立的对象，关系是单向的"订阅"，不是从属。

#### 问题 2：为什么多个 Widget 可以绑定同一个变量？

因为"绑定"的本质从来不是"变量属于某个 widget"，而是**"在这个变量名上注册一条读/写 trace"**。trace 是可以叠加的——同一个变量上可以同时存在任意多条相互独立的 trace，Tcl 核心按注册顺序依次调用它们，彼此互不知道对方的存在，也不会冲突。

`entry_binding.tcl` 里 `.usernameEntry`、`.previewLabel`、`.mirrorEntry` 三个 widget 全部绑定 `username`，等价于在 `username` 上叠加了三条（Tk 内部实现的）write trace。当 `username` 被 `set` 时，Tcl 核心会依次调用这三条 trace 各自对应的刷新逻辑——每个 widget 只管"我自己怎么重新显示这个值"，互不干扰。变量本身自始至终只是一个普通的"名字 -> 值"映射，它不知道、也不需要知道有多少个订阅者。这就是为什么绑定数量可以任意扩展，不需要修改变量本身的任何代码。

#### 问题 3：修改变量与修改 Widget 显示状态之间是什么关系？

这是一个**双向**关系，但两个方向的触发路径完全不同，不要误以为是同一条路径在"来回走"：

- **方向 A（变量 -> 显示）：** 执行 `set username "newValue"`（不管是程序逻辑调用，还是另一个 widget 内部实现调用）会触发这个变量上所有的 write trace，每个 trace 对应的 widget 都会重新读取 `$username` 并刷新自己的显示。这条路径里，"谁改的变量"完全不重要，trace 只关心"变量变了"这一个事实。
- **方向 B（用户输入 -> 变量）：** 用户在 Entry 里敲键盘，触发的是 Tk 的**事件绑定**（键盘事件 -> Entry 内部的编辑逻辑），这段逻辑的最终效果是对 `username` 执行一次 `set`。这条路径的起点不是变量，而是操作系统事件。

两条路径在"`set` 这一步"汇合，之后统一进入方向 A 描述的 trace 扇出过程。所以完整的因果链是：**用户输入触发 widget 内部逻辑 -> widget 内部逻辑执行 set -> set 触发 write trace -> 所有订阅者（包括触发者自己）重新读取并刷新。** `entry_binding.tcl` 里的 `.resetButton`／`.upperButton` 刻意只演示方向 A（程序主动 `set`），用来证明不需要真的在 Entry 里敲键盘，一样能看到所有 widget 同步刷新。

#### 问题 4：`textvariable` 和 `-command` 是否等价？

**不等价，二者是两种完全不同范式的机制。**

`-textvariable`（以及 `-variable`）是**声明式**的、**持续生效**的状态同步契约：你只在创建 widget 时声明一次"我要和这个变量保持同步"，之后的每一次变量变化都会被自动响应，你不需要、也没有办法在中途"手动触发一次同步"或者"跳过某一次同步"（除非显式 `trace remove`）。

`-command` 是**命令式**的、**一次性**的事件回调：它绑定的是"某个特定 widget 发生了某个特定事件（比如按钮被点击）"这一瞬间，脚本只在那一瞬间执行一次，执行完就结束，不会有任何"持续生效"的语义。即使 `-command` 脚本内部完全不读写任何变量，它照样会在按钮被点击时执行；反过来，不管变量被 `set` 多少次，只要没人点按钮，`-command` 脚本一次都不会被执行。

`variable_trace.tcl` 的 `.printButton` 和 `.inputEntry` 并排放在一起，就是为了让这个区别可以被直接对比观察：敲 Entry 触发的是 trace（因为变量变了），点按钮触发的是 `-command`（因为按钮被点击了）——两者的触发条件完全独立，互不影响。

#### 问题 5：什么情况下应该使用变量绑定，什么情况下应该使用回调？

**变量绑定**适合"**让多个展示/数据源保持同步**"这一类场景：典型例子是表单字段镜像到预览区（`entry_binding.tcl`）、一个布尔状态同时驱动 Checkbutton 外观和其它只读展示（`checkbutton_state.tcl` 里 `.agreeCheck` 的状态可以被任意多个地方只读地共享）。这类场景的特征是：**展示逻辑本身很简单（就是"显示这个值"），不需要额外计算，也没有副作用**，适合交给 Tk 自动维护。

**回调**（`-command`，或者本层的 trace 回调）适合"**响应一个明确的动作/变化，并执行一段有副作用的逻辑**"的场景：点击保存按钮写文件、点击提交按钮发请求、变量变化后需要做格式化/校验/记录日志（`variable_trace.tcl` 里的 `onInputWrite` 会做字符串拼接并打印，这已经超出了"单纯展示同步"的范畴，所以即使它监控的是变量变化，也应该被看作"回调"而不是"绑定"）。这类场景的特征是：**需要执行逻辑，而不仅仅是把一个值重新显示一遍**。

一个实用的判断标准：如果你发现自己在 `-command` 脚本或者 trace 回调里，除了 `set` 某个展示变量之外什么都没做，那这段逻辑其实可以直接用 `-textvariable`/`-variable` 绑定替代，不需要手写回调；如果回调里有 `set` 之外的逻辑（判断、计算、I/O、调用其它 proc），那就应该保留显式回调，不要试图用变量绑定去"隐藏"这些副作用。

#### 问题 6：变量绑定和显式刷新 UI 各自适合什么场景？

**变量绑定**适合"**简单的、widget 原生支持的双向同步**"：文本内容（`-textvariable`）、两态/多态选中状态（`-variable`）。这些场景下，Tk 内置的刷新逻辑已经完全够用，开发者不需要、也不应该再手写一套刷新代码。

但当**展示逻辑变复杂**时——需要格式化（比如把秒数格式化成 `mm:ss`）、需要组合多个变量（比如"总价 = 单价 × 数量"）、需要校验后才决定显示什么（比如输入不合法时显示错误提示而不是原始输入）——这些都不是任何一个 widget 的 `-textvariable`/`-variable` 能直接表达的，因为**这些 widget 选项只会做"原样显示变量值"这一件事，不会帮你做任何计算或判断**。这种情况下，正确的做法是：用 `trace add variable`（监听"原始数据"变量）或者 `-command`（响应用户动作）去触发一个**显式的刷新过程**——在这个过程里完成计算/校验，再把结果 `set` 到一个**专门用于展示**的变量上，这个展示变量本身仍然可以用 `-textvariable` 绑定到 Label。

`checkbutton_state.tcl` 的 `statusText` 就是这个模式的最小示例：它本身是被 `-textvariable` 绑定到 `.statusLabel` 的（简单同步部分），但它的值不是直接来自某个 Checkbutton，而是由 `showStatus` 这个**显式刷新过程**组合 `agreeTerms` 和 `notifyPref` 计算出来的（复杂展示逻辑部分）。如果试图跳过 `showStatus`，直接让 `.statusLabel` 绑定 `agreeTerms`，会发现它只能展示 `agreeTerms` 一个变量的原始值，没有办法把两个变量组合成一句话——这正是"自动同步"和"显式刷新"的边界所在。**一旦展示逻辑包含计算或判断，就必须用显式刷新，否则逻辑会分散到看不见的地方，变得难以追踪和维护。**

---

## 4. Complete Example

本层共三个完整可运行的 `.tcl` 文件，下面逐一内联展示并讲解。三个文件都已经用 `tclsh` 校验过语法（见第 5 节）。

### 4.1 示例 A：`entry_binding.tcl` —— Entry 双向绑定

```tcl
#!/usr/bin/env wish
#
# entry_binding.tcl —— Layer 3 示例 A：Entry 与 Tcl 变量的双向绑定
#
# 核心问题：用户在输入框里敲字，程序怎么"知道"？
# 答案的雏形：程序根本不需要主动"去问"，因为 Entry 控件在创建时
# 就通过 -textvariable 把自己和一个 Tcl 变量"绑"在了一起——
# 之后不管是用户敲键盘，还是程序自己用 set 改这个变量，
# 两边的状态永远自动保持一致。这就是本层要建立的 Mental Model。
#
# 运行方式：
#   有图形环境：   wish entry_binding.tcl
#   headless 环境： tclsh entry_binding.tcl   （只能校验到 package require Tk 这一步）
package require Tk

# --------------------------------------------------------------------------
# 阶段 0：声明一个普通 Tcl 变量
# --------------------------------------------------------------------------
# 注意：这一行和"GUI"完全没有关系。此刻 username 只是 Tcl 解释器
# 全局变量表里的一个普通条目，值是字符串 "guest"。
# 任何还没创建的 widget 都不知道这个变量的存在。
set username "guest"

wm title    . "Entry Binding Demo"
wm geometry . 360x220

# --------------------------------------------------------------------------
# 阶段 1：创建一堆普通控件做视觉说明
# --------------------------------------------------------------------------
label .promptLabel -text "Username:" -anchor w

# --------------------------------------------------------------------------
# 阶段 2：关键一行——用 -textvariable 把 Entry 和 username 绑定
# --------------------------------------------------------------------------
# entry .usernameEntry -textvariable username
#
# 这一行背后真正发生的事情（这是本层要讲透的机制）：
#   1. Tk 在内部对变量 username 注册了一对"读/写 trace"
#      （trace 是 Tcl 核心提供的机制：当某个变量被 read/write/unset 时，
#       自动调用一个回调；这里的回调是 Tk 的 C 代码内部实现的，
#       对使用者不可见，但语义上等价于我们稍后在 variable_trace.tcl
#       里手写的 trace add variable ... write ... ）。
#   2. 当用户在这个 Entry 里敲键盘时，Tk 的事件绑定层会把按键事件
#      转换成"更新 Entry 显示内容"的动作，而这个动作的实现方式
#      就是对 username 执行一次 set —— 也就是说，用户打字本质上是
#      在间接地反复执行 `set username "新内容"`。
#   3. username 被 set 之后，Tk 之前注册的 write trace 立刻触发，
#      这个 trace 回调会让"所有绑定了 username 的 widget"重新读取
#      变量的最新值并刷新自己的显示——包括 Entry 自己（保持光标和
#      显示同步），也包括下面即将出现的 .previewLabel 和 .mirrorEntry。
entry .usernameEntry -textvariable username

# --------------------------------------------------------------------------
# 阶段 3：第二个、第三个 widget 绑定同一个变量
# --------------------------------------------------------------------------
# 这里特意用两个不同类型的 widget（Label 和另一个 Entry）绑定同一个
# username 变量，用来证明："绑定同一个变量"不是 Entry 独有的特性，
# 而是 Tk 里凡是支持 -textvariable 选项的 widget 都适用的通用机制。
label .previewLabel -textvariable username -fg blue
entry .mirrorEntry  -textvariable username

# --------------------------------------------------------------------------
# 阶段 4：两个按钮——证明"程序侧 set 变量"同样会驱动所有 widget 刷新
# --------------------------------------------------------------------------
# 这两个按钮的 -command 脚本都只做一件事：对 username 做一次 set。
# 点击按钮后，你会看到上面三个 widget（Entry、Label、第二个 Entry）
# 的显示会【同时】变化——这正好反向证明了"绑定"是双向的：
#   用户输入 -> 变量改变 -> 其余 widget 刷新（上面已经讲过）
#   程序 set 变量 -> 变量改变 -> 所有 widget（包括 Entry 本身）刷新（这里演示）
button .resetButton  -text "Reset to guest"   -command { set username "guest" }
button .upperButton  -text "Uppercase"        -command {
    # 注释：这里直接读写同一个全局变量 username。
    # 因为这段脚本是在全局作用域下被 -command 调用执行的，
    # 不需要 global 声明也能访问到最外层的 username。
    set username [string toupper $username]
}

label .hint -text "改任意一个输入框，另外两个会自动同步" -fg gray40

pack .promptLabel -padx 10 -pady {10 0} -anchor w
pack .usernameEntry -padx 10 -pady 5 -fill x
pack .previewLabel -padx 10 -pady 5 -anchor w
pack .mirrorEntry -padx 10 -pady 5 -fill x
pack .resetButton .upperButton -side left -padx 10 -pady 10
pack .hint -padx 10 -pady {0 10} -anchor w

# --------------------------------------------------------------------------
# 阶段 5：进入事件循环（wish 自动完成，这里不需要写 vwait）
# --------------------------------------------------------------------------
# 之后发生的一切（用户打字、点击按钮）都会沿着上面注释描述的路径
# 反复触发："widget 事件 -> set 变量 -> trace -> 其余 widget 刷新"。
```

**讲解：** 这个文件把"一个变量驱动三个 widget"的场景做到极致：`.usernameEntry` 和 `.mirrorEntry` 是两个独立的 Entry，`.previewLabel` 是一个只读的 Label，三者全部绑定 `username`。你在任何一个 Entry 里敲字，另外两个 widget 都会实时跟着变化——这不是因为它们之间有任何直接联系，而是因为它们都在独立地订阅同一个变量。`.resetButton`/`.upperButton` 证明了这条链路反过来也成立：程序代码一次 `set`，所有 widget 一起刷新。

### 4.2 示例 B：`checkbutton_state.tcl` —— 布尔状态与自定义两态值

```tcl
#!/usr/bin/env wish
#
# checkbutton_state.tcl —— Layer 3 示例 B：Checkbutton 与布尔 / 自定义状态绑定
#
# 核心问题：用户勾选/取消一个复选框，程序怎么拿到"当前是勾选还是没勾选"？
# 答案和 Entry 一样：Checkbutton 通过 -variable（不是 -textvariable）
# 把自己的"勾选状态"绑定到一个 Tcl 变量上；区别在于 Entry 绑定的是
# "文本内容"，Checkbutton 绑定的是一个"两态值"（默认是 0/1，
# 但可以用 -onvalue / -offvalue 自定义成任意两个值）。
#
# 运行方式：
#   有图形环境：   wish checkbutton_state.tcl
#   headless 环境： tclsh checkbutton_state.tcl   （只能校验到 package require Tk 这一步）
package require Tk

# --------------------------------------------------------------------------
# 阶段 0：两个"状态变量"，初始值必须和 Checkbutton 的默认外观对应
# --------------------------------------------------------------------------
# agreeTerms 使用 Checkbutton 的默认约定：0 表示未勾选，1 表示已勾选。
set agreeTerms 0

# notifyPref 演示 -onvalue / -offvalue：变量里存的不是 0/1，
# 而是业务上更有意义的字符串 "enabled" / "disabled"。
# 这说明 Checkbutton 绑定的"两态值"可以是任意两个你指定的值，
# 不是只能用布尔 0/1——Tk 并不关心这两个值具体是什么，
# 它只负责"勾选时把变量设成 onvalue，取消勾选时设成 offvalue"。
set notifyPref "disabled"

# statusText 是一个派生状态：它不直接绑定到任何 Checkbutton，
# 而是由下面的 proc 在"被显式调用时"重新计算出来，
# 用来对照"自动同步的变量绑定"和"需要显式触发的派生显示"两种模式。
set statusText "Status: not checked yet"

wm title    . "Checkbutton State Demo"
wm geometry . 380x260

# --------------------------------------------------------------------------
# 阶段 1：第一个 Checkbutton —— 标准布尔用法
# --------------------------------------------------------------------------
# checkbutton .agreeCheck -text "I agree to the terms" -variable agreeTerms
#
# 不写 -onvalue / -offvalue 时，Tk 使用内置默认值：
#   勾选  -> 变量被 set 为 1
#   取消  -> 变量被 set 为 0
# 和 Entry 的 -textvariable 一样，这个绑定也是通过在 agreeTerms 上
# 注册读/写 trace 实现的，只是 Checkbutton 写入变量的值只有两种可能，
# 而不是像 Entry 那样是任意文本。
checkbutton .agreeCheck -text "I agree to the terms" -variable agreeTerms

# --------------------------------------------------------------------------
# 阶段 2：第二个 Checkbutton —— 自定义 onvalue / offvalue
# --------------------------------------------------------------------------
checkbutton .notifyCheck -text "Enable email notifications" \
    -variable notifyPref -onvalue "enabled" -offvalue "disabled"

# --------------------------------------------------------------------------
# 阶段 3：用一个 trace 实时打印 agreeTerms 的变化（方式一：trace）
# --------------------------------------------------------------------------
# 这里提前用到了 Layer 3 示例 C（variable_trace.tcl）要详细讲的机制：
# trace add variable <name> write <callback>
# 效果：只要 agreeTerms 被 set（不管是用户点击触发，还是程序自己
# set），onAgreeChanged 就会被自动调用一次，不需要我们在 Checkbutton
# 上额外挂 -command。
proc onAgreeChanged {varName index op} {
    # varName/index/op 是 trace 回调的标准三个参数：
    #   varName —— 被追踪的变量名（这里恒为 "agreeTerms"）
    #   index   —— 如果追踪的是数组元素，这里是元素名；普通变量为空字符串
    #   op      —— 触发原因，这里恒为 "write"
    global agreeTerms
    puts "\[trace\] agreeTerms changed -> $agreeTerms"
}
trace add variable agreeTerms write onAgreeChanged

# --------------------------------------------------------------------------
# 阶段 4：用一个按钮显式查询两个状态变量（方式二：主动查询 + 回调）
# --------------------------------------------------------------------------
# 这个按钮演示的是和"变量绑定自动同步"完全不同的另一种获取状态的方式：
# 不依赖任何自动刷新，而是在用户点击这个特定按钮的那一刻，
# 主动读一次 agreeTerms 和 notifyPref 的当前值，拼成一段文字。
# 对比：
#   .agreeCheck / .notifyCheck 的状态同步是"声明式"的，
#       只要绑定关系建立好，后续完全不需要额外代码。
#   .queryButton 的状态查询是"命令式"的，
#       必须显式写代码在某个时间点去读取变量当前值。
proc showStatus {} {
    global agreeTerms notifyPref statusText
    set statusText "agreeTerms=$agreeTerms, notifyPref=$notifyPref"
    puts "\[query\] $statusText"
}
button .queryButton -text "Show Status" -command showStatus

label .statusLabel -textvariable statusText -fg blue -anchor w

label .hint1 -text "勾选 'I agree' 会在终端打印 trace 日志" -fg gray40
label .hint2 -text "点击 'Show Status' 才会刷新下面这一行" -fg gray40

pack .agreeCheck .notifyCheck -anchor w -padx 10 -pady 5
pack .queryButton -pady 10
pack .statusLabel -padx 10 -pady 5 -anchor w -fill x
pack .hint1 .hint2 -padx 10 -anchor w

# --------------------------------------------------------------------------
# 阶段 5：进入事件循环
# --------------------------------------------------------------------------
# 之后：
#   - 勾选/取消任意 Checkbutton -> 对应变量被 set -> 如果该变量上有
#     trace（agreeTerms 有），trace 立刻打印；.statusLabel 不会变，
#     因为它绑定的是 statusText，而 statusText 只会在 showStatus 被
#     调用时才重新计算。
#   - 点击 "Show Status" -> 显式调用 showStatus -> 重新计算 statusText
#     -> statusText 被 set -> .statusLabel（textvariable 绑定）自动刷新。
```

**讲解：** 这个文件刻意把"自动同步"（Checkbutton 的勾选状态）和"显式刷新"（`statusText` 的计算）放在同一个窗口里对比：`.agreeCheck`/`.notifyCheck` 一勾选立刻生效，不需要任何额外代码；`.statusLabel` 却只有点了 `.queryButton` 才会更新——即使它背后依赖的两个变量早就变了。这正是第 6 问的具体体现：组合多个变量、生成一句描述性文字，属于"复杂展示逻辑"，必须走显式刷新路径，不能指望某个 `-textvariable` 自动帮你拼字符串。

### 4.3 示例 C：`variable_trace.tcl` —— trace 的机制与时机

```tcl
#!/usr/bin/env wish
#
# variable_trace.tcl —— Layer 3 示例 C：trace add variable 的机制与时机
#
# 核心问题：除了 widget 自带的 -textvariable / -variable 绑定之外，
# 我们能不能自己监控一个普通 Tcl 变量的变化？能——这就是 trace。
# 事实上，-textvariable / -variable 在 Tk 内部就是靠 trace 实现的，
# 本文件要把这个"黑盒"打开，手写一遍同样的机制。
#
# 运行方式：
#   有图形环境：   wish variable_trace.tcl
#   headless 环境： tclsh variable_trace.tcl
#
# 注意：本文件分成两部分——
#   第一部分是纯 Tcl 逻辑，完全不依赖 Tk，在 tclsh 下会【完整执行完】
#     （不会在这部分报错），用来证明 trace 是 Tcl 核心能力，
#     和有没有图形界面毫无关系。
#   第二部分才 package require Tk，构建一个 Entry 把同一套 trace
#     机制接到真实的 GUI 输入上；在 headless 环境下，脚本会执行到
#     这一行才报错（"无法连接到显示"），这是预期行为。

# ============================================================================
# 第一部分：纯 Tcl 环境下的 trace（不需要 Tk，也不需要 $DISPLAY）
# ============================================================================

puts "=== Part 1: pure Tcl trace demo (no Tk needed) ==="

# counter 是一个普通变量，这里先验证最朴素的 Mental Model：
# "变量就是一个名字到值的映射"，和任何 widget 都没有关系。
set counter 0

# --------------------------------------------------------------------------
# 关键认知点：trace 回调拿不到"旧值"，只能拿到触发时变量的"新值"
# --------------------------------------------------------------------------
# trace add variable 的 write 回调被调用时，变量其实已经被写入完成了。
# Tcl 不会主动帮你保留"修改前的值"。如果业务上需要同时知道旧值和新值
# （这是本示例要求做到的），就必须自己在回调外维护一份"上一次见到的值"，
# 在回调里先打印，再更新这份影子记录。
set ::lastCounterValue $counter

proc onCounterWrite {varName index op} {
    # 使用 upvar #0 把全局变量 $varName 关联到本地名字 newVal，
    # 这样写法不依赖 varName 恒为某个固定名字，更通用。
    upvar #0 $varName newVal
    global lastCounterValue

    puts "\[trace\] var=$varName op=$op old=$lastCounterValue new=$newVal"

    # 打印完之后，把这次的新值记录成"下一次触发时的旧值"。
    set lastCounterValue $newVal
}

# trace add variable <name> write <callback>
#   <name>     —— 要监控的变量名（必须已经存在，或者之后会被创建）
#   write      —— 监控的操作类型，这里只关心"写入"（还有 read / unset）
#   <callback> —— 变量被写入时，Tcl 会以
#                   callback varName index op
#                 的形式自动调用它（index 对数组元素追踪才有意义，
#                 普通标量变量这里恒为空字符串）
trace add variable counter write onCounterWrite

puts "-- 连续修改 counter，观察 trace 同步触发 --"
set counter 1
set counter 5
incr counter          ;# incr 本质上也是"写入"操作，同样会触发 write trace
incr counter -3

# --------------------------------------------------------------------------
# 关键认知点：trace 的触发时机 —— 同步、在修改变量的那次调用内部完成
# --------------------------------------------------------------------------
# 上面四行 set/incr 执行完，终端应该已经依次打印出 4 行 trace 日志。
# 这证明了 trace 回调不是"异步排队、等事件循环空闲了再执行"，
# 而是【同步】发生的：`set counter 5` 这一行代码本身，在返回之前，
# 就已经把 onCounterWrite 调用完了。调用栈大致是：
#   main script
#     -> set counter 5          (Tcl 核心命令)
#       -> onCounterWrite ...   (trace 回调，在 set 返回前执行)
#   (set 命令真正返回，main script 继续往下走)
# 这一点和事件循环无关，因此这部分代码不需要 wish、不需要 vwait、
# 甚至不需要 package require Tk 就能完整跑通——这是本文件特意把
# "纯 Tcl 部分"放在 package require Tk 之前的原因。
puts "最终 counter = $counter"
puts ""

# trace 和按钮 -command 的本质区别（用一句话先立住，Execution Walkthrough
# 里会结合 GUI 部分再展开）：
#   trace    回调触发的理由是"这个变量被写入了"，它不关心是谁写的、
#            为什么写——可能是用户敲键盘，也可能是另一段业务逻辑调用
#            set，甚至可能是另一个 widget 的内部实现在写。
#   -command 回调触发的理由是"用户对这个特定 widget 做了这个特定动作"
#            （比如点击了这个按钮），它和任何变量是否改变没有直接关系；
#            哪怕 -command 脚本里一次 set 都不做，它照样会被执行。
puts "=== Part 1 done. 以上输出不依赖任何 GUI 环境 ==="
puts ""

# 良好习惯：不再需要的 trace 应该显式移除，避免变量在后续代码里
# 被意外修改时仍然触发已经"过期"的回调逻辑（典型场景：窗口被关闭、
# 对应的业务上下文已经不存在了，但变量本身因为是全局的，依然存活）。
trace remove variable counter write onCounterWrite
puts "已移除 counter 上的 trace（trace remove variable counter write onCounterWrite）"
puts "-- 验证：下面这次 set 不会再打印 trace 日志 --"
set counter 999
puts "counter 现在是 $counter，但没有 \[trace\] 行输出，说明回调确实被移除了"
puts ""

# ============================================================================
# 第二部分：把同一套 trace 机制接到真实的 Tk Entry 上
# ============================================================================
# 在 headless（无 $DISPLAY）环境下，脚本会执行到下面这一行才报错，
# 这正是本教程校验方式要求确认的"唯一报错点"。
puts "=== Part 2: 接入 Tk GUI，正在尝试连接显示... ==="
package require Tk

wm title    . "Variable Trace Demo"
wm geometry . 420x240

# inputValue 是绑定给 Entry 的变量；用户在 Entry 里敲字，
# 本质上就是在反复对 inputValue 执行 set（和示例 A 的 username 一样）。
set inputValue ""
set ::lastInputValue $inputValue

label .promptLabel -text "输入内容，终端会打印 trace 日志：" -anchor w
entry .inputEntry -textvariable inputValue

# logText 用来把 trace 日志同时镜像一份到窗口里，方便没有盯着终端的人
# 也能看到效果；注意 logText 本身不是被 trace 追踪的对象，
# 它只是 onInputWrite 回调里"顺手"更新的另一个变量。
set logText "(尚无输入)"
label .logLabel -textvariable logText -fg blue -anchor w -wraplength 380 -justify left

proc onInputWrite {varName index op} {
    upvar #0 $varName newVal
    global lastInputValue logText

    set line "old=\"$lastInputValue\" new=\"$newVal\""
    puts "\[trace\] $line"
    set logText "Last change: $line"

    set lastInputValue $newVal
}
trace add variable inputValue write onInputWrite

label .hint -text "对比：下面的按钮用 -command，点击才会打印；上面的 Entry 靠 trace，一改就打印" -fg gray40 -wraplength 380 -justify left

button .printButton -text "Print current value (via -command)" -command {
    # 这是 -command 风格：只有点击这个按钮的瞬间才会执行一次，
    # 和 inputValue 是否被修改过没有任何直接关系。
    puts "\[-command\] button clicked, inputValue is currently \"$inputValue\""
}

pack .promptLabel -padx 10 -pady {10 5} -anchor w -fill x
pack .inputEntry -padx 10 -pady 5 -fill x
pack .logLabel -padx 10 -pady 5 -anchor w -fill x
pack .hint -padx 10 -pady 5 -anchor w -fill x
pack .printButton -pady 10

# --------------------------------------------------------------------------
# 进入事件循环之后：
#   - 每敲一个字符 -> inputValue 被 set -> onInputWrite 同步触发
#     -> 终端打印一行，.logLabel 同步刷新。
#   - 点击 .printButton -> 执行一次 -command 脚本 -> 终端打印一行，
#     但 .logLabel 不会变化（因为这条路径完全没有碰 inputValue 之外
#     的任何会被 trace 的变量，也没有调用 onInputWrite）。
# --------------------------------------------------------------------------
```

**讲解：** 这是本层最重要的文件。Part 1 完全不涉及 Tk，用纯 Tcl 证明了 trace 的两个关键性质：① 回调默认拿不到旧值，必须自己维护影子变量；② trace 是同步触发的，发生在 `set`/`incr` 这条语句自身返回之前，和事件循环毫无关系（因此在 `tclsh` 下能完整跑完，不需要进入任何事件等待）。Part 2 把同一个 `onInputWrite` 风格的回调接到一个真实的 Entry 上，并且并排放了一个纯 `-command` 按钮，方便直接对比"因为变量变了"和"因为按钮被点了"这两种完全不同的触发语义。

---

## 5. Execution Walkthrough

### 5.1 `entry_binding.tcl`

1. `package require Tk`：加载 Tk，创建主窗口 `.`。
2. `set username "guest"`：此刻变量存在，没有任何 widget，也没有任何 trace。
3. `label .promptLabel ...`：创建一个静态文本 Label，和 `username` 无关。
4. `entry .usernameEntry -textvariable username`：Tk 在 `username` 上注册一对内部读/写 trace，并立刻把 Entry 的初始显示内容设为 `username` 的当前值 `"guest"`。
5. `label .previewLabel -textvariable username` / `entry .mirrorEntry -textvariable username`：在 `username` 上再叠加两条同类型的 trace，同样立刻同步显示为 `"guest"`。此刻 `username` 上一共有三条（Tk 内部的）write trace。
6. `button ...`：创建两个按钮，`-command` 脚本只是被原样存成字符串，不会立刻执行。
7. `pack ...`：把所有 widget 纳入布局，开始真正出现在窗口里。
8. 进入事件循环（`wish` 隐式完成）：
   - 若用户在 `.usernameEntry` 里敲字 -> Entry 内部逻辑 `set username <新内容>` -> 三条 trace 依次触发 -> `.usernameEntry` 自己、`.previewLabel`、`.mirrorEntry` 都刷新成新内容。
   - 若用户点击 `.resetButton` -> 执行 `-command` 脚本 `set username "guest"` -> 同样触发三条 trace -> 三个 widget 一起变回 `"guest"`。

### 5.2 `checkbutton_state.tcl`

1. `package require Tk` 之后依次 `set agreeTerms 0`、`set notifyPref "disabled"`、`set statusText "..."`——三个变量此刻都只是普通变量。
2. `checkbutton .agreeCheck -variable agreeTerms`：Tk 在 `agreeTerms` 上注册读/写 trace，并把 Checkbutton 的初始勾选状态设为"未勾选"（因为 `agreeTerms` 当前是 `0`，匹配默认 `offvalue`）。
3. `checkbutton .notifyCheck -variable notifyPref -onvalue enabled -offvalue disabled`：同理，初始值 `"disabled"` 匹配 `offvalue`，初始显示为未勾选。
4. `trace add variable agreeTerms write onAgreeChanged`：在 `agreeTerms` 上叠加**第二条**（这次是我们自己写的）write trace——此刻 `agreeTerms` 上一共有两条 trace：Tk 内部那条（负责刷新 Checkbutton 外观）和 `onAgreeChanged`（负责打印日志），**两条互不干扰，按注册顺序依次执行**。
5. `button .queryButton -command showStatus`：创建按钮，`showStatus` 这个命令名被存成回调，暂不执行。
6. 用户勾选 `.agreeCheck` -> Checkbutton 内部逻辑执行 `set agreeTerms 1` -> 依次触发两条 trace：Checkbutton 自己刷新为"已勾选"外观，`onAgreeChanged` 打印 `agreeTerms changed -> 1`。**`.statusLabel` 这时不会有任何变化**，因为它绑定的是 `statusText`，没有人碰过这个变量。
7. 用户点击 `.queryButton` -> 执行 `showStatus` -> 读取 `agreeTerms`/`notifyPref` 当前值 -> `set statusText "..."` -> `.statusLabel` 的 `-textvariable` 绑定被触发，刷新显示。

### 5.3 `variable_trace.tcl`

1. **Part 1（无 Tk）**：`set counter 0` -> `trace add variable counter write onCounterWrite` -> 连续 `set counter 1` / `set counter 5` / `incr counter` / `incr counter -3`，每一条语句执行时都会**在该语句返回之前**同步调用一次 `onCounterWrite`，总共打印 4 行 `[trace]` 日志，最终 `puts "最终 counter = $counter"` 输出 `3`。随后 `trace remove variable counter write onCounterWrite` 解除绑定，再执行 `set counter 999` 时不再有任何打印——证明 trace 确实是"按需注册、按需解除"的，不是变量自带的永久属性。
2. **Part 2（接入 Tk）**：`package require Tk` 在 headless 环境下会在这一步报错并中止脚本——这是预期行为，之前 Part 1 的全部逻辑已经完整跑完并打印了正确结果。如果在有图形界面的机器上用 `wish` 运行，脚本会继续往下创建 `.inputEntry`，并在 `inputValue` 上注册第二个独立的 trace 回调 `onInputWrite`（和 `.inputEntry` 自己的内部 trace 并存）；之后每次在 Entry 里敲字，`onInputWrite` 都会同步打印一行并更新 `.logLabel`；点击 `.printButton` 则只会打印一次 `[-command]` 日志，不会影响 `.logLabel`。

---

## 6. Data Flow / Control Flow

### 6.1 基础状态流转图

```text
User Input
    |
    v
Entry Widget
    |
    v
Tcl Variable
    |
    +------> Preview Label   (automatic, via -textvariable trace)
    |
    +------> Application Logic   (NOT automatic -- see note below)
```

**关键说明（必须明确指出，否则是本层最大的误区）：**

`Preview Label` 这个分支之所以"自动"发生，是因为 Label 本身用 `-textvariable` 显式订阅了这个变量——这是 widget 自己选择接入了 trace 机制。但 `Application Logic` 这个分支**不会凭空自动执行**：它是否执行、什么时候执行，完全取决于你有没有为这个变量显式配置 `trace add variable ... write <callback>`，或者有没有在某个事件处理（`-command`、`bind`）里显式读取这个变量并调用业务逻辑。

换句话说，图中这两个分支看起来对称，实际语义完全不同：**上面那个箭头的"自动"，来自 widget 主动订阅；下面那个箭头要"自动"，必须先由你写代码主动订阅（trace）或主动触发（事件回调）。把一个变量绑定给另一个展示 widget，绝不会顺带触发任何业务逻辑分支**——`checkbutton_state.tcl` 里 `agreeTerms` 被勾选后 `.statusLabel` 不会自动更新，就是这条规则最直接的证据。

### 6.2 多订阅者场景（对应示例 A）

```text
+-----------------------+
| Entry A (.usernameEntry)|
+-----------+-----------+
            |  set (user types)
            v
+-----------------------+
| Tcl Variable: username |
+-----------+-----------+
            |  write-trace fan-out
            |
   +--------+----------------------+
   |                               |
   v                               v
+-----------------------+   +-----------------------+
| Entry B (.mirrorEntry)  |   | Label (.previewLabel)  |
+-----------------------+   +-----------------------+
```

三个 widget 对变量而言是完全对等的订阅者，图里把 Entry A 画成"起点"只是因为这次恰好是它触发了 `set`；如果换成在 Entry B 里敲字，箭头方向完全一样，只是起点换成 Entry B。**没有任何一个 widget 是变量的"所有者"，它们都只是平等的订阅者。**

### 6.3 trace 的同步触发时序（对应示例 C）

```text
Time -->

1. main script executes: set counter 5
2.   Tcl core writes 5 into variable table
3.   Tcl core invokes onCounterWrite  (still inside step 1's call)
4.     onCounterWrite reads/prints old & new value
5.     onCounterWrite updates lastCounterValue
6.   onCounterWrite returns
7. set counter 5 returns control to main script
8. main script continues to next line
```

这张图要强调的是：第 3~6 步完全发生在第 1 步"这一条语句"的执行过程内部，不是排到某个队列里等事件循环方法去处理。这和 `-command` 回调的触发时机形成对照——`-command` 回调是在**未来某次事件循环迭代中，由事件分发机制调用**的，和当前这行代码的执行完全不同步、不在同一个调用栈上。

---

## 7. Common Failure

1. **忘记 `global`，以为改了全局变量，其实改的是一个同名局部变量。**
   ```tcl
   set username "guest"
   proc resetUsername {} {
       # 错误：没有 global 声明，这里创建的是一个新的局部变量 username，
       # proc 返回后它就消失了，外层（widget 绑定的那个）username 完全没变。
       set username "guest"
   }
   ```
   诊断方法：在 proc 里 `puts [info exists ::username]` 和 `puts [info level 0]` 分别检查全局和局部是否真的是你以为的那个变量；或者干脆养成习惯——任何要读写"外部状态"的 proc，第一行先写 `global ...`。

2. **以为 trace 回调能自动拿到旧值。** 很多人第一次写 `trace add variable ... write cb` 时，会假设 `cb` 的参数里包含旧值，结果发现签名只有 `varName index op`。必须像本层示例那样，自己在回调外维护一份"上一次的值"快照。

3. **以为 `unset` 一个被 widget 绑定的变量等于"清空 widget 显示"。** 实际上 Tk 自己在这个变量上也挂了 unset-trace，一旦检测到变量消失，它会立刻用 widget 当前的显示内容把变量重新创建出来，防止出现"控件引用一个不存在的变量"这种更危险的状态。所以 `unset username`（当它被某个 Entry 绑定时）往往"看起来什么都没发生"，这不是 bug，而是 Tk 的保护机制。

4. **在 trace 回调里又去 `set` 被监控的同一个变量，造成意外递归。**
   ```tcl
   trace add variable total write {apply {{n i op} {
       # 危险：这里又对 total 做了一次 set，会重新触发自己这条 trace，
       # 如果没有退出条件，会无限递归直到栈溢出。
       global total
       set total [expr {$total + 0}]
   }}}
   ```
   安全写法：如果确实需要在回调里修正变量值，必须先判断"新值是否已经等于要设置的值"，避免无意义的重复写入；或者改用一个单独的派生变量承接计算结果，不要回写被监控的原变量。

5. **把 `-command` 脚本里的赋值误当成"绑定"。** 例如 `button .b -command { set status "clicked" }`，这只是"点击时写一次变量"，和 `-textvariable`/`-variable` 那种持续生效的绑定是两回事——如果后面某处代码又单独 `set status "..."`，`.b` 的 `-command` 不会被重新触发，这是正常的，因为它压根不是变量的订阅者。

6. **多个独立对话框复用同一个全局变量名，互相覆盖。** 如果多次弹出"同一种"对话框（比如多个编辑窗口），却都绑定了同名的全局变量（比如 `username`），后面打开的实例会覆盖前面实例正在编辑的数据。应对方式见第 8 节的命名空间 / 数组建议。

---

## 8. Engineering Practice

1. **用数组统一管理一组相关的表单字段，而不是散落的全局变量。**
   ```tcl
   array set form {username "" email "" agree 0}
   entry .e1 -textvariable form(username)
   entry .e2 -textvariable form(email)
   checkbutton .c1 -variable form(agree)
   ```
   好处：一眼能看出"这个表单一共有哪些字段"；`trace add variable form write onFormChange` 可以用**一条** trace 监控整个数组的任意元素写入（回调的 `index` 参数会告诉你具体是哪个元素变了），不需要给每个字段单独挂一条。

2. **把状态变量放进专属命名空间，避免全局命名冲突。** 例如 `namespace eval ::EditDialog { variable username "" }`，widget 绑定写成 `-textvariable ::EditDialog::username`。这样即使项目里同时存在多个"编辑对话框"概念，也不会和其它模块的全局变量撞名。

3. **区分"展示变量"和"业务变量"，复杂展示逻辑永远走显式刷新。** 不要试图在 `-textvariable` 绑定的变量上叠加过多语义（既要承载原始输入、又要承载格式化结果），而应该像 `checkbutton_state.tcl` 的 `statusText` 一样，单独开一个"只用于展示"的变量，由一个清晰命名的 `proc`（比如 `refreshXxxDisplay`）负责在合适的时机重新计算并写入它。

4. **trace 回调保持轻量、幂等，重逻辑交给事件循环调度。** 因为 trace 是同步执行的，如果回调里做了耗时操作（网络请求、大量计算），会直接阻塞当前这次 `set` 调用，进而阻塞整个 UI（Tcl/Tk 是单线程事件循环）。正确做法：trace 回调里只做"标记脏（dirty）"或者 `after idle <真正的处理 proc>`，把重逻辑调度到下一次事件循环空闲时再执行。

5. **widget/对话框销毁时，主动 `trace remove`。** 尤其是当 trace 回调引用了闭包状态或者和某个即将销毁的 widget 强相关时，不清理会导致变量在生命周期后段被意外写入时，仍然触发一个逻辑上已经"过期"的回调。

6. **优先使用 `trace add variable ... write` 而不是轮询（polling）去检测变量变化。** 不要写 `after 100 { if {$x ne $lastX} {...} }` 这种定时检查，既浪费 CPU，又有最多 100ms 的延迟；trace 是事件驱动、零延迟、零轮询开销的正确方案。

---

## 9. Exercises

1. **字符计数（对应第 6 问）。** 在 `entry_binding.tcl` 的基础上增加一个 `.charCountLabel`，用 `trace add variable username write onUsernameChanged` 在回调里计算 `[string length $username]` 并把结果 `set` 到一个新的展示变量 `charCountText` 上，再用 `-textvariable charCountText` 绑定到 `.charCountLabel`。想一想：为什么不能直接给 `.charCountLabel` 配 `-textvariable username`？

2. **把 `checkbutton_state.tcl` 的两个状态变量改造成数组字段。** 把 `agreeTerms`/`notifyPref` 合并成 `form(agree)`/`form(notify)`，只用**一条** `trace add variable form write onFormChange` 替换掉原来的 `onAgreeChanged`，在回调里用 `index`（trace 回调的第二个参数）判断具体是哪个字段变了，并分别打印不同的日志。

3.（进阶）**摄氏度/华氏度互相联动转换器。** 用两个 Entry 分别绑定 `celsius` 和 `fahrenheit` 两个变量，各自挂一条 `trace add variable ... write ...`：在 `celsius` 的回调里按公式算出华氏度并 `set fahrenheit`，在 `fahrenheit` 的回调里反过来算出摄氏度并 `set celsius`。实现后思考并解决一个问题：这样写会不会导致两个 trace 互相触发、无限递归？如果会，应该怎么加一个"容差判断"或者"重入标志"来阻止它？

---

## 10. Mental Model Summary

1. Tcl 变量独立于任何 widget，存活在解释器的变量表里，生命周期与 widget 无关。
2. `-textvariable`/`-variable` 不是魔法，本质是 widget 在变量上注册了读/写 trace。
3. 多个 widget 可以绑定同一个变量，因为 trace 可以在同一个变量上叠加多条，互不冲突。
4. 变量变化会触发所有订阅者刷新，但触发路径分两种：用户输入 -> widget 内部 `set`；程序逻辑 -> 直接 `set`。
5. `textvariable`/`-variable` 是声明式、持续生效的状态同步；`-command` 是命令式、一次性的事件回调，二者不等价。
6. `trace` 的触发条件是"变量被写了"，不关心是谁写的；`-command` 的触发条件是"这个 widget 发生了这个事件"，不关心任何变量。
7. trace 回调默认拿不到旧值，必须自己维护一份影子变量才能对比新旧。
8. trace 是同步触发的，发生在修改变量那条语句返回之前，和事件循环的调度时机无关。
9. 简单的原样展示同步交给变量绑定；需要格式化/组合/校验的复杂展示逻辑，必须走显式的 trace 或 `-command` 刷新过程。
10. 把变量绑定到另一个展示 widget 上，不会自动触发任何业务逻辑——业务逻辑永远需要显式的 trace 回调或事件回调才会执行。

---

## 11. Knowledge Check

1. 如果一个 Tcl proc 内部直接 `set username "new"`，但外层 GUI 完全没有反应，最可能的原因是什么？你会用什么方法确认自己的猜测是否正确？
2. 一个变量上可以同时存在"Tk 内部注册的 trace"和"你自己 `trace add` 的 trace"吗？如果可以，它们的执行顺序是怎样的，彼此之间会不会互相影响？
3. 为什么说"把 Checkbutton 的状态变量绑定到一个 Label 上"不能代替"在业务逻辑里判断这个 Checkbutton 是否被勾选"？请结合"自动同步"和"业务逻辑触发"的区别说明。
